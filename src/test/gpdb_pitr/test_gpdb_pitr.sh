#!/usr/bin/env bash

## ==================================================================
## Required: A fresh gpdemo cluster with mirrors sourced.
##
## This script tests and showcases a very simple Point-In-Time
## Recovery scenario by utilizing WAL Archiving and restore
## points. This test also demonstrates the commit blocking during
## distributed restore point creation during concurrent transactions
## to guarantee cluster consistency.
##
## Note: After successfully running this test, the PITR cluster will
## still be up and running from the temp_test directory. Run the
## `clean` Makefile target to go back to the gpdemo cluster.
## ==================================================================

# Store gpdemo master and primary segment data directories.
# This assumes default settings for the ports and data directories.
DATADIR="${MASTER_DATA_DIRECTORY%*/*/*}"
MASTER=${DATADIR}/qddir/demoDataDir-1
PRIMARY1=${DATADIR}/dbfast1/demoDataDir0
PRIMARY2=${DATADIR}/dbfast2/demoDataDir1
PRIMARY3=${DATADIR}/dbfast3/demoDataDir2
MASTER_PORT=7000
PRIMARY1_PORT=7002
PRIMARY2_PORT=7003
PRIMARY3_PORT=7004

# Set up temporary directories to store the basebackups and the WAL
# archives that will be used for Point-In-Time Recovery later.
TEMP_DIR=$PWD/temp_test
REPLICA_MASTER=$TEMP_DIR/replica_m
REPLICA_PRIMARY1=$TEMP_DIR/replica_p1
REPLICA_PRIMARY2=$TEMP_DIR/replica_p2
REPLICA_PRIMARY3=$TEMP_DIR/replica_p3

ARCHIVE_MASTER=$TEMP_DIR/archive_m
ARCHIVE_PRIMARY1=$TEMP_DIR/archive_p1
ARCHIVE_PRIMARY2=$TEMP_DIR/archive_p2
ARCHIVE_PRIMARY3=$TEMP_DIR/archive_p3

REPLICA_MASTER_DBID=10
REPLICA_PRIMARY1_DBID=11
REPLICA_PRIMARY2_DBID=12
REPLICA_PRIMARY3_DBID=13

# The options for pg_regress and pg_isolation2_regress.
REGRESS_OPTS="--dbname=gpdb_pitr_database --use-existing --init-file=../regress/init_file --load-extension=gp_inject_fault"
ISOLATION2_REGRESS_OPTS="${REGRESS_OPTS} --init-file=../isolation2/init_file_isolation2"

# Run test via pg_regress with given test name.
run_test()
{
    ../regress/pg_regress $REGRESS_OPTS $1
    if [ $? != 0 ]; then
        exit 1
    fi
}

# Run test via pg_isolation2_regress with given test name. The
# isolation2 framework is mainly used to demonstrate the commit
# blocking scenario.
run_test_isolation2()
{
    ../isolation2/pg_isolation2_regress $ISOLATION2_REGRESS_OPTS $1
    if [ $? != 0 ]; then
        exit 1
    fi
}

# Remove temporary test directory if it already exists.
[ -d $TEMP_DIR ] && rm -rf $TEMP_DIR

# Set up WAL Archiving by updating the postgresql.conf files of the
# master and primary segments. Afterwards, restart the cluster to load
# the new settings.
echo "Setting up WAL Archiving configurations..."
for segment_role in MASTER PRIMARY1 PRIMARY2 PRIMARY3; do
  ARCHIVE_VAR=ARCHIVE_$segment_role
  DATADIR_VAR=$segment_role
  echo "wal_level = replica
archive_mode = on
archive_command = 'cp %p ${!ARCHIVE_VAR}/%f'" >> ${!DATADIR_VAR}/postgresql.conf
  mkdir -p ${!ARCHIVE_VAR}
done
gpstop -ar -q

# Create the basebackups which will be our replicas for Point-In-Time
# Recovery later.
echo "Creating basebackups..."
for segment_role in MASTER PRIMARY1 PRIMARY2 PRIMARY3; do
  PORT_VAR=${segment_role}_PORT
  REPLICA_VAR=REPLICA_$segment_role
  REPLICA_DBID_VAR=REPLICA_${segment_role}_DBID
  pg_basebackup -h localhost -p ${!PORT_VAR} -D ${!REPLICA_VAR} --target-gp-dbid ${!REPLICA_DBID_VAR}
done

# Create our test database.
createdb gpdb_pitr_database

# Run setup test. This will create the tables, create the restore
# points, and demonstrate the commit blocking.
run_test_isolation2 gpdb_pitr_setup

# Stop the gpdemo cluster. We'll be focusing on the PITR cluster from
# now onwards.
echo "Stopping gpdemo cluster to now focus on PITR cluster..."
gpstop -a -q

# Create recovery.conf files in all the replicas to set up for
# Point-In-Time Recovery. Specifically, we need to have the
# restore_command and recovery_target_name set up properly. We'll also
# need to empty out the postgresql.auto.conf file to disable
# synchronous replication on the PITR cluster since it won't have
# mirrors to replicate to.
echo "Creating recovery.conf files in the replicas and starting them up..."
for segment_role in MASTER PRIMARY1 PRIMARY2 PRIMARY3; do
  ARCHIVE_VAR=ARCHIVE_$segment_role
  REPLICA_VAR=REPLICA_$segment_role
  echo "standby_mode = 'on'
restore_command = 'cp ${!ARCHIVE_VAR}/%f %p'
recovery_target_name = 'test_restore_point'
primary_conninfo = ''" > ${!REPLICA_VAR}/recovery.conf
  echo "" > ${!REPLICA_VAR}/postgresql.auto.conf
  pg_ctl start -D ${!REPLICA_VAR} -l /dev/null
done

# Wait up to 30 seconds for new master to accept connections.
RETRY=60
while true; do
  pg_isready > /dev/null
  if [ $? == 0 ]; then
    break
  fi

  sleep 0.5s
  RETRY=$[$RETRY - 1]
  if [ $RETRY -le 0 ]; then
    echo "FAIL: Timed out waiting for new master to accept connections."
    exit 1
  fi
done

# Reconfigure the segment configuration on the replica master so that
# the other replicas are recognized as primary segments.
echo "Configuring replica master's gp_segment_configuration..."
PGOPTIONS="-c gp_role=utility" psql postgres -c "
SET allow_system_table_mods=true;
DELETE FROM gp_segment_configuration WHERE preferred_role='m';
UPDATE gp_segment_configuration SET dbid=${REPLICA_MASTER_DBID}, datadir='${REPLICA_MASTER}' WHERE content = -1;
UPDATE gp_segment_configuration SET dbid=${REPLICA_PRIMARY1_DBID}, datadir='${REPLICA_PRIMARY1}' WHERE content = 0;
UPDATE gp_segment_configuration SET dbid=${REPLICA_PRIMARY2_DBID}, datadir='${REPLICA_PRIMARY2}' WHERE content = 1;
UPDATE gp_segment_configuration SET dbid=${REPLICA_PRIMARY3_DBID}, datadir='${REPLICA_PRIMARY3}' WHERE content = 2;
"

# Restart the cluster to get the MPP parts working.
echo "Restarting cluster now that the new cluster is properly configured..."
export MASTER_DATA_DIRECTORY=$REPLICA_MASTER
gpstop -ar -q

# Run validation test to confirm we have gone back in time.
run_test gpdb_pitr_validate

# Print unnecessary success output.
echo "============================================="
echo "SUCCESS! GPDB Point-In-Time Recovery worked."
echo "============================================="
