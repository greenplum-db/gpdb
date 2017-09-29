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
# Check that krb5kdc is in $PATH.
if ! command -v krb5kdc > /dev/null; then
    echo "Kerberos utility 'krb5kdc' not found in \$PATH:"
    echo "$PATH"
    exit 1
fi

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
LD_LIBRARY_PATH= krb5kdc -p 7500 -r GPDB.EXAMPLE -P "$(pwd)/krb5kdc.pid" -n &
KDC_PID=$!
echo "KDC daemon launched, pid ${KDC_PID}"

kill_kdc_pid(){
kill ${KDC_PID}
wait %1
}

trap kill_kdc_pid EXIT

###
# test KDC without exiting make, to ensure that server is stopped
./test-gpdb-auth-with-kdc.sh
SUCCESS=$?

exit $SUCCESS
