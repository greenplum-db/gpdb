#!/bin/bash

# This file has the .sql extension, but it is actually launched as a shell
# script. This contortion is necessary because pg_regress normally uses
# psql to run the input scripts, and requires them to have the .sql
# extension, but we use a custom launcher script that runs the scripts using
# a shell instead.

TESTNAME=tablespaces_objects_removed_after_promotion

. sql/config_test.sh

MASTER_PG_CTL_STOP_MODE="fast"

TABLESPACE_LOCATION=$TESTROOT/$TESTNAME/ts
DROP_TABLESPACE_LOCATION=$TESTROOT/$TESTNAME/drop_ts

cat <<EOF
-- start_matchsubs
-- m/tmp_check_local/
-- s/tmp_check_local/tmp_check/
-- m/tmp_check_remote/
-- s/tmp_check_remote/tmp_check/
-- m/\/.*data_master/
-- s/\/.*data_master/some_data_master_parent_dir\/data_master/
-- m/\/.*\/tmp_check/
-- s/\/.*\/tmp_check/some_location\/tmp_check/
-- end_matchsubs
EOF

# Create tablespace location directory
function before_master
{
rm -rf $TABLESPACE_LOCATION $DROP_TABLESPACE_LOCATION
mkdir $TABLESPACE_LOCATION $DROP_TABLESPACE_LOCATION
}

# This script runs after the standby has been started.
function standby_following_master
{
# Create two new tablespace that will be both on the master and the standby.
PGOPTIONS=${PGOPTIONS_UTILITY} $MASTER_PSQL -c "CREATE TABLESPACE ts LOCATION '$TABLESPACE_LOCATION';"
PGOPTIONS=${PGOPTIONS_UTILITY} $MASTER_PSQL -c "CREATE TABLESPACE drop_ts LOCATION '$DROP_TABLESPACE_LOCATION';"
DROP_TS_OID=$(PGOPTIONS=${PGOPTIONS_UTILITY} psql --no-psqlrc -p $PORT_MASTER -A -q --tuples-only -c "SELECT oid from pg_tablespace where spcname='drop_ts';")
# Now create objects in that tablespace
PGOPTIONS=${PGOPTIONS_UTILITY} $MASTER_PSQL -c "CREATE TABLE t_heap(i int) TABLESPACE ts;"
PGOPTIONS=${PGOPTIONS_UTILITY} $MASTER_PSQL -c "CREATE INDEX t_heap_idx ON t_heap(i) TABLESPACE ts;"
PGOPTIONS=${PGOPTIONS_UTILITY} $MASTER_PSQL -c "INSERT INTO t_heap VALUES(generate_series(1, 100));"
PGOPTIONS=${PGOPTIONS_UTILITY} $MASTER_PSQL -c "CREATE TABLE t_ao(i int) WITH (appendonly=true) TABLESPACE ts;"
PGOPTIONS=${PGOPTIONS_UTILITY} $MASTER_PSQL -c "INSERT INTO t_ao VALUES(generate_series(1, 100));"
PGOPTIONS=${PGOPTIONS_UTILITY} $MASTER_PSQL -c "CHECKPOINT;"
}

# This script runs after the standby has been promoted.
function after_promotion
{
# Drop all objects in ts and drop the tablespace drop_ts
PGOPTIONS=${PGOPTIONS_UTILITY} $STANDBY_PSQL -c "DROP TABLE t_heap;"
PGOPTIONS=${PGOPTIONS_UTILITY} $STANDBY_PSQL -c "DROP TABLE t_ao;"
PGOPTIONS=${PGOPTIONS_UTILITY} $STANDBY_PSQL -c "DROP TABLESPACE drop_ts;"
PGOPTIONS=${PGOPTIONS_UTILITY} $STANDBY_PSQL -c "CHECKPOINT;"
}

function before_master_restart_after_rewind
{
# Confirm that after rewind the master still has the correct symlink set up for tablespace ts.
local ts_oid=$(PGOPTIONS=${PGOPTIONS_UTILITY} psql --no-psqlrc -p $PORT_STANDBY -A -q --tuples-only -c "SELECT oid from pg_tablespace where spcname='ts';")
local db_oid=$(PGOPTIONS=${PGOPTIONS_UTILITY} psql --no-psqlrc -p $PORT_STANDBY -A -q --tuples-only -c "SELECT oid from pg_database where datname='$PGDATABASE';")
local ts_target_dir=$(readlink $TEST_MASTER/pg_tblspc/${ts_oid})
echo $ts_target_dir
# Confirm that after rewind the tablespace symlink target directory still exists.
[ -d $ts_target_dir ] && echo "symlink target directory for tablespace ts exists."

# Test that there are 0 relfilenodes as all objects have been deleted for tablespace ts.
local num_relfiles=$(ls $ts_target_dir/GPDB_*/${db_oid}/ | wc -l)
echo $num_relfiles

# Confirm that drop_ts_oid directory has been removed from the pg_tblspc directory
readlink $TEST_MASTER/pg_tblspc/$DROP_TS_OID || echo "drop_ts tablespace link removed."
}

# Run the test
. sql/run_test.sh
