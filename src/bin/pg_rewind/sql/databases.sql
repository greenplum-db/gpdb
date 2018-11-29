#!/bin/bash

# This file has the .sql extension, but it is actually launched as a shell
# script. This contortion is necessary because pg_regress normally uses
# psql to run the input scripts, and requires them to have the .sql
# extension, but we use a custom launcher script that runs the scripts using
# a shell instead.

TESTNAME=databases

. sql/config_test.sh

# Create a database in master.
function before_standby
{
PGOPTIONS=${PGOPTIONS_UTILITY} $MASTER_PSQL <<EOF
CREATE DATABASE inmaster;
EOF
}

function standby_following_master
{
# Create another database after promotion
PGOPTIONS=${PGOPTIONS_UTILITY} $MASTER_PSQL -c "CREATE DATABASE beforepromotion"
}

# This script runs after the standby has been promoted. Old Master is still
# running.
function after_promotion
{
PGOPTIONS=${PGOPTIONS_UTILITY} $MASTER_PSQL -c "CREATE DATABASE master_afterpromotion"

PGOPTIONS=${PGOPTIONS_UTILITY} $STANDBY_PSQL -c "CREATE DATABASE standby_afterpromotion"
}

# Compare results generated by querying original master after rewind
function after_rewind
{
PGOPTIONS=${PGOPTIONS_UTILITY} $MASTER_PSQL -c "SELECT datname from pg_database"
}
# Run the test
. sql/run_test.sh
