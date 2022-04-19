#!/bin/bash

set -eo pipefail

function set_ssl_on {
    echo "ssl=on" >> $1/postgresql.conf
    cat >> $1/postgresql.conf <<-EOF
ssl_cert_file='server.crt'
ssl_key_file='server.key'
ssl_ca_file='root.crt'
EOF

    echo "hostssl     all         ssltestuser 0.0.0.0/0    cert" >> $1/pg_hba.conf
}

./gen_gpdb_certs.sh
for dir in $(find $MASTER_DATA_DIRECTORY/../../.. -name pg_hba.conf)
    do
        set_ssl_on $(dirname $dir)/
done

gpstop -ar
createuser ssltestuser -s || true
