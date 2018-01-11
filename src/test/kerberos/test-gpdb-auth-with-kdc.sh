#!/bin/bash
#
# Test kerberos.
#

set -e  # exit on error

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

# Reload the config.
pg_ctl -D ${MASTER_DATA_DIRECTORY} reload -w

echo "Server configured for Kerberos authentication"

###
# Test that we can *not* connect yet, because we haven't run kinit.
#
echo "Testing connection, should fail"
psql "dbname=postgres hostaddr=127.0.0.1 krbsrvname=postgres  host=localhost user=krbtestuser" -c "SELECT version()" &&
if [ $? -eq 0 ]; then
    echo "FAIL: Connection to server succeeded, even though we had not run kinit yet!"
    exit 1
fi

# Run kinit, to obtain a Kerberos TGT, so that we can log in.
echo "Obtaining Kerberos ticket-granting-ticket from KDC..."
LD_LIBRARY_PATH= kinit -k -t ./client.keytab krbtestuser@GPDB.EXAMPLE

###
# Test that we can connect, now that we have run kinit.
#
echo "Testing connection, should succeed and print version"
psql -P pager=off "dbname=postgres hostaddr=127.0.0.1 krbsrvname=postgres  host=localhost user=krbtestuser" -c "SELECT version()"

###
# Also test expiration
psql -P pager=off "dbname=template1" -c "ALTER USER krbtestuser valid until '2014-04-10 11:46:00-07'"

# should not be able to connect anymore
echo "Testing connection, with expired user account. Should not succeed"
psql "dbname=postgres hostaddr=127.0.0.1 krbsrvname=postgres  host=localhost user=krbtestuser" -c "SELECT version()" &&
if [ $? -eq 0 ]; then
    echo "FAIL: Connection with expired user account succeeded!"
    exit 1
fi

psql -P pager=off "dbname=template1" -c "ALTER USER krbtestuser valid until '2054-04-10 11:46:00-07'"
# now it should succeed again.
echo "Testing connection, with user account with expiration in future. Should succeed"
psql -P pager=off "dbname=postgres hostaddr=127.0.0.1 krbsrvname=postgres  host=localhost user=krbtestuser" -c "SELECT version()" 

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
