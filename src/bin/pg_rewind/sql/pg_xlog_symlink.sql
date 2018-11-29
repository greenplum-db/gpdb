#!/bin/bash

# This file has the .sql extension, but it is actually launched as a shell
# script. This contortion is necessary because pg_regress normally uses
# psql to run the input scripts, and requires them to have the .sql
# extension, but we use a custom launcher script that runs the scripts using
# a shell instead.

TESTNAME=pg_xlog_symlink

. sql/config_test.sh

# Change location of pg_xlog and symlink to new location
function before_master
{
	TEST_XLOG=$TESTROOT/pg_xlog
	rm -rf $TEST_XLOG
	mkdir $TEST_XLOG
	cp -r $TEST_MASTER/pg_xlog/* $TEST_XLOG/
	rm -rf $TEST_MASTER/pg_xlog
	ln -s $TEST_XLOG $TEST_MASTER/pg_xlog
}

# Do an insert in master.
function before_standby
{
PGOPTIONS=${PGOPTIONS_UTILITY} $MASTER_PSQL <<EOF
CREATE TABLE tbl1 (d text);
INSERT INTO tbl1 VALUES ('in master');
CHECKPOINT;
EOF
}

function standby_following_master
{
# Insert additional data on master that will be replicated to standby
PGOPTIONS=${PGOPTIONS_UTILITY} $MASTER_PSQL -c "INSERT INTO tbl1 values ('in master, before promotion');"

# Launch checkpoint after standby has been started
PGOPTIONS=${PGOPTIONS_UTILITY} $MASTER_PSQL -c "CHECKPOINT;"
}

# This script runs after the standby has been promoted. Old Master is still
# running.
function after_promotion
{
# Insert a row in the old master. This causes the master and standby to have
# "diverged", it's no longer possible to just apply the standy's logs over
# master directory - you need to rewind.
PGOPTIONS=${PGOPTIONS_UTILITY} $MASTER_PSQL -c "INSERT INTO tbl1 VALUES ('in master, after promotion');"

# Also insert a new row in the standby, which won't be present in the old
# master.
PGOPTIONS=${PGOPTIONS_UTILITY} $STANDBY_PSQL -c "INSERT INTO tbl1 VALUES ('in standby, after promotion');"
}

function after_rewind
{
PGOPTIONS=${PGOPTIONS_UTILITY} $MASTER_PSQL -c "SELECT * from tbl1"
}

# Run the test
. sql/run_test.sh
