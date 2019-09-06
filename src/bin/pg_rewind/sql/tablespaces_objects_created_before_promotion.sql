#!/bin/bash

# This file has the .sql extension, but it is actually launched as a shell
# script. This contortion is necessary because pg_regress normally uses
# psql to run the input scripts, and requires them to have the .sql
# extension, but we use a custom launcher script that runs the scripts using
# a shell instead.

TESTNAME=tablespaces_objects_created_before_promotion

. sql/config_test.sh

MASTER_PG_CTL_STOP_MODE="fast"

TABLESPACE_LOCATION=$TESTROOT/$TESTNAME/ts

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
rm -rf $TABLESPACE_LOCATION
mkdir $TABLESPACE_LOCATION
}

# This script runs after the standby has been started.
function standby_following_master
{
# Create a new tablespace that will be both on the master and the standby.
PGOPTIONS=${PGOPTIONS_UTILITY} $MASTER_PSQL -c "CREATE TABLESPACE ts LOCATION '$TABLESPACE_LOCATION';"
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
# Create a new table in the standby that will not be on the old master.
PGOPTIONS=${PGOPTIONS_UTILITY} $STANDBY_PSQL -c "CREATE TABLE t_heap_after_promotion(i int) TABLESPACE ts;"
PGOPTIONS=${PGOPTIONS_UTILITY} $STANDBY_PSQL -c "INSERT INTO t_heap_after_promotion VALUES(generate_series(1, 100));"
# Modify objects created before the standby was promoted
PGOPTIONS=${PGOPTIONS_UTILITY} $STANDBY_PSQL -c "INSERT INTO t_heap VALUES(generate_series(1, 100));"
PGOPTIONS=${PGOPTIONS_UTILITY} $STANDBY_PSQL -c "INSERT INTO t_ao VALUES(generate_series(1, 100));"
PGOPTIONS=${PGOPTIONS_UTILITY} $STANDBY_PSQL -c "CHECKPOINT;"
}

function before_master_restart_after_rewind
{
# Confirm that after rewind the master has the correct symlink set up.
local ts_oid=$(PGOPTIONS=${PGOPTIONS_UTILITY} psql --no-psqlrc -p $PORT_STANDBY -A -q --tuples-only -c "SELECT oid from pg_tablespace where spcname='ts';")
local db_oid=$(PGOPTIONS=${PGOPTIONS_UTILITY} psql --no-psqlrc -p $PORT_STANDBY -A -q --tuples-only -c "SELECT oid from pg_database where datname='$PGDATABASE';")
local ts_target_dir=$(readlink $TEST_MASTER/pg_tblspc/${ts_oid})
echo $ts_target_dir
# Confirm that after rewind the tablespace symlink target directory has been created.
[ -d $ts_target_dir ] && echo "symlink target directory for tablespace ts exists."

# Test that relfilenodes appear after rewind completes in master.
local num_relfiles=$(ls $ts_target_dir/GPDB_*/${db_oid}/ | wc -l)
# Expect 7 relfiles (for t_heap, t_heap_idx, t_ao (with its 3 metadata tables) and t_heap_after_promotion)
echo $num_relfiles
}

function after_rewind
{
# Confirm that the heap and ao table data is intact
local count_rows_t_heap=$(PGOPTIONS=${PGOPTIONS_UTILITY} psql --no-psqlrc -p $PORT_MASTER -A -q --tuples-only -c "SELECT count(*) from t_heap;")
# Expect 200 rows in t_heap
echo ${count_rows_t_heap}
local count_rows_t_ao=$(PGOPTIONS=${PGOPTIONS_UTILITY} psql --no-psqlrc -p $PORT_MASTER -A -q --tuples-only -c "SELECT count(*) from t_ao;")
# Expect 200 rows in t_ao
echo ${count_rows_t_ao}
# Confirm that the heap index is useable in order to query the heap table. Expect two tuples as we did two inserts.
local some_specific_value=5
PGOPTIONS="${PGOPTIONS_UTILITY} -c enable_seqscan=off -c optimizer_enable_tablescan=off" psql --no-psqlrc -p $PORT_MASTER -A -q --tuples-only -c "SELECT i FROM t_heap WHERE i=${some_specific_value};"
local count_rows_t_heap_after_promotion=$(PGOPTIONS=${PGOPTIONS_UTILITY} psql --no-psqlrc -p $PORT_MASTER -A -q --tuples-only -c "SELECT count(*) from t_heap_after_promotion;")
# Expect 100 rows in t_heap_after_promotion
echo ${count_rows_t_heap_after_promotion}
}

# Run the test
. sql/run_test.sh
