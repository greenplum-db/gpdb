#!/bin/bash
#
# Test Kerberos authentication
#
# This test script sets up a KDC for testing purposes, re-configures
# the GPDB server to use it for authentication, and opens a test
# connection with psql and Kerberos authentication.
#
# Requires a GPDB server to be running, and GPDB environment variables to be
# set up correctly. MASTER_DATA_DIRECTORY must point to the GPDB master
# server's data directory. (These tests need to be run on the master node,
# because this script needs to modify the server configuration files.
#
# Requires MIT Kerberos KDC utilities, krb5kdc, kadmin etc. to be installed.
#

set -e  # exit on error

if [ -z "$MASTER_DATA_DIRECTORY" ]; then
  echo "FAILED: MASTER_DATA_DIRECTORY not set!"
  exit 1
fi

export KRB5_CONFIG=$(pwd)/krb5.conf
export KRB5_KDC_PROFILE=$(pwd)/test-kdc-db/kdc.conf
export KRB5CCNAME=FILE:$(pwd)/krb5cc

###
# Clean up any leftovers from previous run
make clean

###
# Save old server config
cp ${MASTER_DATA_DIRECTORY}/pg_hba.conf ./pg_hba.conf.orig
cp ${MASTER_DATA_DIRECTORY}/postgresql.conf ./postgresql.conf.orig

###
# Set up KDC database, with a service principal for the server, and a user
# principal for "krbtestuser"
echo "Setting up test KDC..."
bash setup-kdc.sh

###
# Launch KDC daemon. (Note that -P requires an absolute path)
#
# We use the -n option to prevent it from detaching, but we put  it to
# background with &. This way, it gets killed if the terminal session is
# closed.
/usr/sbin/krb5kdc -p 7500 -r GPDB.EXAMPLE -P "$(pwd)/krb5kdc.pid" -n &
KDC_PID=$!
echo "KDC daemon launched, pid ${KDC_PID}"

###
# Create test user in the database
psql "dbname=template1" -c "DROP USER IF EXISTS krbtestuser"
psql "dbname=template1" -c "CREATE USER krbtestuser LOGIN"

###
# Configure the server, to use the generated Kerberos configuration.
#
# We overwrite all 'host' lines in the pg_hba.conf, with lines that only
# allow Kerberos-authenticated TCP connections. But we leave any 'local'
# lines, for Unix domain socket connections, unmodified. That's not strictly
# necessary, but if the test failed and we don't clean up, at least you can
# still login.
cp server.keytab ${MASTER_DATA_DIRECTORY}/server.keytab

grep ^local ./pg_hba.conf.orig > ./pg_hba.conf.kerberized

cat >> ./pg_hba.conf.kerberized <<EOF
# Kerberos test config

host    all         all         127.0.0.1/24          gss include_realm=0 krb_realm=GPDB.EXAMPLE
host    all         all         ::1/128               gss include_realm=0 krb_realm=GPDB.EXAMPLE

EOF
cp ./pg_hba.conf.kerberized ${MASTER_DATA_DIRECTORY}/pg_hba.conf

# add krb_server_keyfile='server.keytab' to postgresql.conf
cp ./postgresql.conf.orig ./postgresql.conf.kerberized
echo "krb_server_keyfile='server.keytab'" >> ./postgresql.conf.kerberized
cp ./postgresql.conf.kerberized  ${MASTER_DATA_DIRECTORY}/postgresql.conf

###
# Restart the server, to reload the config.
#
# Changing krb_server_keyfile requires a restart to take effect, SIGHUP
# is not enough (GPDB_84_MERGE_FIXME: this changes in PostgreSQL 8.4, commit
# a27addbc87)
pg_ctl -D ${MASTER_DATA_DIRECTORY} restart -w

echo "Server configured for Kerberos authentication"

###
# Test that we can *not* connect yet, because we haven't run kinit.
#
echo "Testing connection, should fail"
! psql "dbname=postgres hostaddr=127.0.0.1 krbsrvname=postgres  host=gpdb-server.example user=krbtestuser" -c "SELECT version()"

# Run kinit, to obtain a Kerberos TGT, so that we can log in.
echo "Obtaining Kerberos ticket-granting-ticket from KDC..."
kinit -k -t ./client.keytab krbtestuser@GPDB.EXAMPLE

###
# Test that we can connect, now that we have run kinit.
#
echo "Testing connection, should succeed and print version"
psql "dbname=postgres hostaddr=127.0.0.1 krbsrvname=postgres  host=gpdb-server.example user=krbtestuser" -c "SELECT version()"

###
# Also test expiration
psql "dbname=template1" -c "ALTER USER krbtestuser valid until '2014-04-10 11:46:00-07'"

# should not be able to connect anymore
echo "Testing connection, with expired user account. Should not succeed"
! psql "dbname=postgres hostaddr=127.0.0.1 krbsrvname=postgres  host=gpdb-server.example user=krbtestuser" -c "SELECT version()"

psql "dbname=template1" -c "ALTER USER krbtestuser valid until '2054-04-10 11:46:00-07'"
# now it should succeed again.
echo "Testing connection, with user account with expiration in future. Should succeed"
psql "dbname=postgres hostaddr=127.0.0.1 krbsrvname=postgres  host=gpdb-server.example user=krbtestuser" -c "SELECT version()"

###
# All done! Restore previous pg_hba.conf and postgresql.conf
echo "All tests executed successfully! Cleaning up..."

cp ./pg_hba.conf.orig ${MASTER_DATA_DIRECTORY}/pg_hba.conf
cp ./postgresql.conf.orig ${MASTER_DATA_DIRECTORY}/postgresql.conf
rm ${MASTER_DATA_DIRECTORY}/server.keytab
# Reload the config, to put the old pg_hba.conf back into effect. The
# krb_server_keyfile change won't take effect until restart, but that
# doesn't matter.
pg_ctl -D ${MASTER_DATA_DIRECTORY} reload

kill ${KDC_PID}
wait %1
