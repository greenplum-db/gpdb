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
TEST_MASTER=$TESTROOT/$TESTNAME/data_master
TEST_STANDBY=$TESTROOT/$TESTNAME/data_standby

# Remove data dirs from previous passing tests. Only retain the ones that
# failed. We parse regression.out to determine which previous tests directories
# can be deleted. Unfortunately this has the effect of leaving the last test
# untouched. We could check the last test inside the Makefile if we wish to.
if [ -f regression.out ]; then
	PASSED_TEST=`tail -2 regression.out | grep -w "ok" | cut -d " " -f2`
	if [ ! -z $PASSED_TEST ]; then
		rm -rf $TESTROOT/$PASSED_TEST
	fi
fi

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

MASTER_DBID=2
STANDBY_DBID=5

PGOPTIONS_UTILITY='-c gp_role=utility'
MASTER_PSQL="psql -a --no-psqlrc -p $PORT_MASTER"
STANDBY_PSQL="psql -a --no-psqlrc -p $PORT_STANDBY"
STANDBY_PSQL_TUPLES_ONLY="psql -t --no-psqlrc -p $PORT_STANDBY"
# -m option is used for pg_rewind tests to convey starting in single
# postgres instance mode the QE segment. So, ignore the distributed
# log checking and hence enable vacuuming the tables in pg_rewind
# tests. If pg_rewind tests use full GPDB cluster, -m option will not
# be needed.
PG_CTL_COMMON_OPTIONS="--gp_contentid=0 -m"
MASTER_PG_CTL_OPTIONS="--gp_dbid=${MASTER_DBID} -p ${PORT_MASTER} $PG_CTL_COMMON_OPTIONS"
STANDBY_PG_CTL_OPTIONS="--gp_dbid=${STANDBY_DBID} -p ${PORT_STANDBY} $PG_CTL_COMMON_OPTIONS"
MASTER_PG_CTL_STOP_MODE="fast"

function wait_for_promotion {
   retry=600
   until [ $retry -le 0 ]
   do
      PGOPTIONS=${PGOPTIONS_UTILITY} ${1} -c "select 'promotion is done';" && return 0
      retry=$[$retry-1]
      sleep 0.5
   done
   echo "error: timeout, promotion is not done."
   exit 1
}

function standby_checkpoint {
   PGOPTIONS=${PGOPTIONS_UTILITY} ${STANDBY_PSQL} -c "checkpoint;"
}

function wait_until_standby_is_promoted {
    wait_for_promotion "${STANDBY_PSQL}"
}

function wait_until_master_is_promoted {
    wait_for_promotion "${MASTER_PSQL}"
}

function wait_until_standby_streaming_state {
   retry=600
   until [ $retry -le 0 ]
   do
      PGOPTIONS=${PGOPTIONS_UTILITY} $STANDBY_PSQL -c "SELECT state FROM pg_stat_replication;" | grep 'streaming' > /dev/null && return 0
      retry=$[$retry-1]
      sleep 0.5
   done
   echo "error: timeout, standby streaming is not done."
   exit 1
}
