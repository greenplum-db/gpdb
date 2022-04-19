#!/bin/bash

set -eo pipefail

function set_ssl_off {
    sed -ri 's/ssl=on/ssl=off/g' $1/postgresql.conf
    sed -ri "s/ssl_cert_file='server.crt'//g" $1/postgresql.conf
    sed -ri "s/ssl_key_file='server.key'//g" $1/postgresql.conf
    sed -ri "s/ssl_ca_file='root.crt'//g" $1/postgresql.conf
    sed -ri "s/hostssl     all         ssltestuser 0.0.0.0\/0    cert//g" $1/pg_hba.conf
}

for dir in $(find $MASTER_DATA_DIRECTORY/../../.. -name pg_hba.conf)
    do
        set_ssl_off $(dirname $dir)/
done

gpstop -ar
