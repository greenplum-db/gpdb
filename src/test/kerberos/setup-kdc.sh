#!/bin/bash
#
# Helper script, to set up a test kerberos (KDC) for realm GPDB.EXAMPLE.
#
# The KDC database is created in ./test-kdc-db. It is
# initialized with a service principal for "postgres/localhost",
# and a user principal for "krbtestuser@GPDB.EXAMPLE".

set -e  # exit on error

KDC_PATH=$PWD

rm -rf ${KDC_PATH}/test-kdc-db
mkdir ${KDC_PATH}/test-kdc-db

# Generate kdc.conf (krb5.conf is static, and doesn't need to be generated
# this way)
cat >> ${KDC_PATH}/test-kdc-db/kdc.conf <<EOF
[kdcdefaults]
 kdc_ports = 7500
 kdc_tcp_ports = 7500

[realms]
  GPDB.EXAMPLE= {
    profile = ./krb5.conf
    database_name = ${KDC_PATH}/test-kdc-db/principal
    admin_keytab = ${KDC_PATH}/test-kdc-db/kadm5.keytab
    acl_file = ${KDC_PATH}/test-kdc-db/kadm5.acl
    key_stash_file = ${KDC_PATH}/test-kdc-db/stash.k5.GPDB.EXAMPLE
    max_life = 1h 0m 0s
    max_renewable_life = 2h 0m 0s
  }

[logging]
    default = FILE:${KDC_PATH}/test-kdc-db/kdc.log
    kdc = FILE:${KDC_PATH}/test-kdc-db/kdc.log
    admin_server = FILE:${KDC_PATH}/test-kdc-db/kadmin.log
EOF

export KRB5_CONFIG=${KDC_PATH}/krb5.conf
export KRB5_KDC_PROFILE=${KDC_PATH}/test-kdc-db/kdc.conf

# Create KDC database, with no password
LD_LIBRARY_PATH= kdb5_util create -P"" -r GPDB.EXAMPLE -s

# Create a service principal for the PostgreSQL server, running at "localhost",
# and a keytab file for it
LD_LIBRARY_PATH= kadmin.local -q "addprinc -randkey postgres/localhost"
LD_LIBRARY_PATH= kadmin.local -q "ktadd -k ./server.keytab postgres/localhost"

# Create a user principal, and a keytab file for it.
LD_LIBRARY_PATH= kadmin.local -q "addprinc -randkey krbtestuser@GPDB.EXAMPLE"
LD_LIBRARY_PATH= kadmin.local -q "ktadd -k ./client.keytab krbtestuser@GPDB.EXAMPLE"
