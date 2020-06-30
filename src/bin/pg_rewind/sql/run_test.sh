#!/bin/bash
#
# pg_rewind.sh
#
# Test driver for pg_rewind. This test script initdb's and configures a
# cluster and creates a table with some data in it. Then, it makes a
# standby of it with pg_basebackup, and promotes the standby.
#
# The result is two clusters, so that the old "master" cluster can be
# resynchronized with pg_rewind to catch up with the new "standby" cluster.
# This test can be run with either a local data folder or a remote
# connection as source.
#
# Before running this script, the calling script should've included
# config_test.sh, and defined four functions to define the test case:
#
#  before_master   - runs after initializing the master, before starting it
#  before_standby  - runs after starting the master, before creating the
#                    standby
#  standby_following_master - runs after standby has been created and started
#  after_promotion - runs after standby has been promoted, but old master is
#                    still running
#  before_master_restart_after_rewind - runs after pg_rewind executes and the
#                    master has not been restarted
#  after_rewind    - runs after pg_rewind and after restarting the rewound
#                    old master
#
# In those functions, the test script can use $MASTER_PSQL and $STANDBY_PSQL
# to run psql against the master and standby servers, to cause the servers
# to diverge.

# Initialize master, data checksums are mandatory
rm -rf $TEST_MASTER
initdb --data-checksums -N -A trust -D $TEST_MASTER >>$log_path 2>&1

# Custom parameters for master's postgresql.conf
cat >> $TEST_MASTER/postgresql.conf <<EOF
shared_buffers = 1MB
max_connections = 50
listen_addresses = '$LISTEN_ADDRESSES'
port = $PORT_MASTER
wal_keep_segments=5
EOF

# Accept replication connections on master
cat >> $TEST_MASTER/pg_hba.conf <<EOF
local replication all trust
host replication all 127.0.0.1/32 trust
host replication all ::1/128 trust
EOF

# We have to specify the master's dbid explicitly because initdb only creates an
# empty file. gpconfigurenewseg is tasked with populating the master's dbid.
cat >> $TEST_MASTER/internal.auto.conf <<EOF
gp_dbid=${MASTER_DBID}
EOF

#### Now run the test-specific parts to initialize the master before setting
echo "Master initialized."
declare -F before_master > /dev/null && before_master

pg_ctl -w -D $TEST_MASTER start -o "$MASTER_PG_CTL_OPTIONS" >>$log_path 2>&1

# up standby
echo "Master running."
declare -F before_standby > /dev/null && before_standby

# Set up standby with necessary parameter
rm -rf $TEST_STANDBY

# Base backup is taken with xlog files included
pg_basebackup -D $TEST_STANDBY -p $PORT_MASTER -x --target-gp-dbid $STANDBY_DBID --verbose >>$log_path 2>&1

echo "port = $PORT_STANDBY" >> $TEST_STANDBY/postgresql.conf
echo "wal_keep_segments = 5" >> $TEST_STANDBY/postgresql.conf

cat > $TEST_STANDBY/recovery.conf <<EOF
primary_conninfo='port=$PORT_MASTER'
standby_mode=on
recovery_target_timeline='latest'
EOF

# Start standby
pg_ctl -w -D $TEST_STANDBY start -o "$STANDBY_PG_CTL_OPTIONS" >>$log_path 2>&1

#### Now run the test-specific parts to run after standby has been started
# up standby
echo "Standby initialized and running."
declare -F standby_following_master > /dev/null && standby_following_master

# sleep a bit to make sure the standby has caught up.
sleep 1

# For some tests, we want to stop the master before standby promotion
if [ "$STOP_MASTER_BEFORE_PROMOTE" == "true" ]; then
    pg_ctl -w -D $TEST_MASTER stop -m $MASTER_PG_CTL_STOP_MODE >>$log_path 2>&1
fi

# Now promote slave and insert some new data on master, this will put
# the master out-of-sync with the standby.

pg_ctl -w -D $TEST_STANDBY promote >>$log_path 2>&1

wait_until_standby_is_promoted >>$log_path 2>&1

#### Now run the test-specific parts to run after promotion
echo "Standby promoted."
declare -F after_promotion > /dev/null && after_promotion

# For some tests, we want to stop the master after standby promotion
if [ "$STOP_MASTER_BEFORE_PROMOTE" != "true" ]; then
    pg_ctl -w -D $TEST_MASTER stop -m $MASTER_PG_CTL_STOP_MODE >>$log_path 2>&1
fi

# For a local test, source node need to be stopped as well.
if [ $TEST_SUITE == "local" ]; then
	pg_ctl -w -D $TEST_STANDBY stop -m fast >>$log_path 2>&1
else
	# Force a checkpoint to update the source timline id in pg_control data,
	# else pg_rewind will quit without recovery if the timline is is not
	# updated because it is same as the one on the target.
	standby_checkpoint >>$log_path 2>&1
fi

# At this point, the rewind processing is ready to run.
# We now have a very simple scenario with a few diverged WAL record.
# The real testing begins really now with a bifurcation of the possible
# scenarios that pg_rewind supports.

# Keep a temporary postgresql.conf for master node or it would be
# overwritten during the rewind.
cp $TEST_MASTER/postgresql.conf $TESTROOT/master-postgresql.conf.tmp

# Now run pg_rewind
echo "Running pg_rewind..."
echo "Running pg_rewind..." >> $log_path
if [ $TEST_SUITE == "local" ]; then
	# Do rewind using a local pgdata as source
	pg_rewind \
	    --progress \
	    --debug \
	    --source-pgdata=$TEST_STANDBY \
	    --target-pgdata=$TEST_MASTER >>$log_path 2>&1
elif [ $TEST_SUITE == "remote" ]; then
	# Do rewind using a remote connection as source
	PGOPTIONS=${PGOPTIONS_UTILITY} pg_rewind \
		 --progress \
		 --debug \
		 --source-server="port=$PORT_STANDBY dbname=postgres" \
		 --target-pgdata=$TEST_MASTER >>$log_path 2>&1
else
	# Cannot come here normally
	echo "Incorrect test suite specified"
	exit 1
fi

# After rewind is done, restart the source node in local mode.
if [ $TEST_SUITE == "local" ]; then
	pg_ctl -w -D $TEST_STANDBY start -o "$STANDBY_PG_CTL_OPTIONS" >>$log_path 2>&1
fi

# Now move back postgresql.conf with old settings
mv $TESTROOT/master-postgresql.conf.tmp $TEST_MASTER/postgresql.conf

# Plug-in rewound node to the now-promoted standby node
cat > $TEST_MASTER/recovery.conf <<EOF
primary_conninfo='port=$PORT_STANDBY'
standby_mode=on
recovery_target_timeline='latest'
EOF

declare -F before_master_restart_after_rewind > /dev/null && before_master_restart_after_rewind

# Restart the master to check that rewind went correctly
pg_ctl -w -D $TEST_MASTER start -o "$MASTER_PG_CTL_OPTIONS" >>$log_path 2>&1

#### Now run the test-specific parts to check the result
echo "Old master restarted after rewind."
# Make sure master is able to connect to standby and reach streaming state.
wait_until_standby_streaming_state
PGOPTIONS=${PGOPTIONS_UTILITY} $STANDBY_PSQL -c "SELECT state FROM pg_stat_replication;"

# Now promote master and run validation queries
pg_ctl -w -D $TEST_MASTER promote >>$log_path 2>&1
wait_until_master_is_promoted >>$log_path 2>&1
echo "Master promoted."

declare -F after_rewind > /dev/null && after_rewind

# Stop remaining servers
pg_ctl stop -D $TEST_MASTER -m fast -w >>$log_path 2>&1
pg_ctl stop -D $TEST_STANDBY -m fast -w >>$log_path 2>&1
