#!/bin/bash
#
# Initialize some variables, before running pg_rewind.sh.

set -e

mkdir -p "regress_log"
log_path="regress_log/pg_rewind_log_${TESTNAME}_${TEST_SUITE}"
: ${MAKE=make}

rm -f $log_path

# Guard against parallel make issues (see comments in pg_regress.c)
unset MAKEFLAGS
unset MAKELEVEL

# Check at least that the option given is suited
if [ "$TEST_SUITE" = 'remote' ]; then
	echo "Running tests with libpq connection as source" >>$log_path 2>&1
	TEST_SUITE="remote"
elif [ "$TEST_SUITE" = 'local' ]; then
	echo "Running tests with local data folder as source" >>$log_path 2>&1
	TEST_SUITE="local"
else
	echo "TEST_SUITE environment variable must be set to either \"local\" or \"remote\""
	exit 1
fi

# Set listen_addresses desirably
testhost=`uname -s`
case $testhost in
	MINGW*) LISTEN_ADDRESSES="localhost" ;;
	*)      LISTEN_ADDRESSES="" ;;
esac

# Indicate of binaries
PATH=$bindir:$PATH
export PATH

# Adjust these paths for your environment
TESTROOT=$PWD/tmp_check_$TEST_SUITE
TEST_MASTER=$TESTROOT/data_master
TEST_STANDBY=$TESTROOT/data_standby

# Create the root folder for test data
mkdir -p $TESTROOT

# Clear out any environment vars that might cause libpq to connect to
# the wrong postmaster (cf pg_regress.c)
#
# Some shells, such as NetBSD's, return non-zero from unset if the variable
# is already unset. Since we are operating under 'set -e', this causes the
# script to fail. To guard against this, set them all to an empty string first.
PGDATABASE="";        unset PGDATABASE
PGUSER="";            unset PGUSER
PGSERVICE="";         unset PGSERVICE
PGSSLMODE="";         unset PGSSLMODE
PGREQUIRESSL="";      unset PGREQUIRESSL
PGCONNECT_TIMEOUT=""; unset PGCONNECT_TIMEOUT
PGHOST="";            unset PGHOST
PGHOSTADDR="";        unset PGHOSTADDR

export PGDATABASE="postgres"

# Define non conflicting ports for both nodes, this could be a bit
# smarter with for example dynamic port recognition using psql but
# this will make it for now.
PG_VERSION_NUM=90401
PORT_MASTER=`expr $PG_VERSION_NUM % 16384 + 49152`
PORT_STANDBY=`expr $PORT_MASTER + 1`

PGOPTIONS_UTILITY='-c gp_session_role=utility'
MASTER_PSQL="psql -a --no-psqlrc -p $PORT_MASTER"
STANDBY_PSQL="psql -a --no-psqlrc -p $PORT_STANDBY"
STANDBY_PSQL_TUPLES_ONLY="psql -t --no-psqlrc -p $PORT_STANDBY"
PG_CTL_COMMON_OPTIONS="--gp_dbid=2 --gp_contentid=0"
MASTER_PG_CTL_OPTIONS="-p ${PORT_MASTER} $PG_CTL_COMMON_OPTIONS"
STANDBY_PG_CTL_OPTIONS="-p ${PORT_STANDBY} $PG_CTL_COMMON_OPTIONS"
MASTER_PG_CTL_STOP_MODE="fast"

function wait_for_promotion {
   retry=50
   until [ $retry -le 0 ]
   do
      PGOPTIONS=${PGOPTIONS_UTILITY} ${1} -c "select 1;" && break
      retry=$[$retry-1]
      sleep 0.2
   done
}

function wait_until_standby_is_promoted {
    wait_for_promotion "${STANDBY_PSQL}"
}

function wait_until_master_is_promoted {
    wait_for_promotion "${MASTER_PSQL}"
}

function wait_until_standby_streaming_state {
   retry=150
   until [ $retry -le 0 ]
   do
      PGOPTIONS=${PGOPTIONS_UTILITY} $STANDBY_PSQL -c "SELECT state FROM pg_stat_replication;" | grep 'streaming' > /dev/null && break
      retry=$[$retry-1]
      sleep 0.2
   done
}
