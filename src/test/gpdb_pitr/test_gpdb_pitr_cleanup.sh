#!/usr/bin/env bash

## ==================================================================
## Run this script to clean up after the GPDB PITR test.
##
## Note: Assumes PITR cluster is up and running, and that the gpdemo
## cluster settings are sourced.
## ==================================================================

# Store gpdemo master and primary segment data directories.
# This assumes default settings for the data directories.
DATADIR="${COORDINATOR_DATA_DIRECTORY%*/*/*}"
MASTER=${DATADIR}/qddir/demoDataDir-1
PRIMARY1=${DATADIR}/dbfast1/demoDataDir0
PRIMARY2=${DATADIR}/dbfast2/demoDataDir1
PRIMARY3=${DATADIR}/dbfast3/demoDataDir2

TEMP_DIR=$PWD/temp_test

# Stop the PITR cluster.
echo "Stopping the PITR cluster..."
REPLICA_MASTER=$TEMP_DIR/replica_m
COORDINATOR_DATA_DIRECTORY=$REPLICA_MASTER gpstop -ai -q

# Remove the temp_test directory.
echo "Removing the temporary test directory..."
rm -rf $TEMP_DIR

# Reset the wal_level on the gpdemo master and primary segments.
echo "Undoing WAL Archive settings on gpdemo cluster..."
for datadir in $MASTER $PRIMARY1 $PRIMARY2 $PRIMARY3; do
  sed -i'' -e "/wal_level = replica/d" $datadir/postgresql.conf
  sed -i'' -e "/archive_mode = on/d" $datadir/postgresql.conf
  sed -i'' -e "/archive_command = 'cp %p/d" $datadir/postgresql.conf
done

# Start back up the gpdemo cluster.
echo "Starting back up the gpdemo cluster..."
gpstart -a
dropdb gpdb_pitr_database
