#!/bin/bash

if [[ -z "${MASTER_DATA_DIRECTORY}" ]]; then
    echo "Error: Environment Variable MASTER_DATA_DIRECTORY not set!!"
    exit 1
fi

CERTS_PATH=$MASTER_DATA_DIRECTORY
CLIENT_CERTS_PATH=$HOME/.postgresql
HOST_NAME=`hostname`
GP_USER=ssltestuser

mkdir -p ${CLIENT_CERTS_PATH}

function gencert() {
openssl req -new -newkey rsa:4096 -days 365 -nodes -x509 \
    -subj "/C=US/ST=California/L=Palo Alto/O=Pivotal/CN=$1root" \
    -keyout $1root.key  -out $1root.crt

openssl req -new -newkey rsa:4096 -nodes \
    -subj "/C=US/ST=California/L=Palo Alto/O=Pivotal/CN=$1ca" \
    -keyout $1ca.key -out $1ca.csr

openssl x509 -req -in $1ca.csr -CA $1root.crt -CAkey $1root.key \
    -extfile <(printf "basicConstraints=CA:TRUE\nsubjectAltName=DNS:$HOST_NAME") \
    -days 365 -out $1ca.crt -sha256 -CAcreateserial

openssl req -new -newkey rsa:2048 -nodes \
    -subj "/C=US/ST=California/L=Palo Alto/O=Pivotal/CN=$2" \
    -keyout $1.key  -out $1.csr

openssl x509 -req -in $1.csr -CA $1ca.crt -CAkey $1ca.key \
    -extfile <(printf "subjectAltName=DNS:$HOST_NAME") \
    -days 365 -out $1.crt -sha256 -CAcreateserial
}

function generate_gpdb_certs() {
    pushd ${CERTS_PATH}
    #rm -f *.crt *.key *.csr *.srl
    gencert server $HOST_NAME
    cat serverca.crt >> serverroot.crt

    cp serverroot.crt ${CLIENT_CERTS_PATH}/root.crt
    chmod 600 *.key
    popd
}
function generate_client_certs() {
    pushd ${CLIENT_CERTS_PATH}
    #rm -f *.crt *.key *.csr *.srl
    gencert postgresql $GP_USER
    cat postgresqlca.crt >> postgresqlroot.crt

    cp postgresqlroot.crt ${CERTS_PATH}/root.crt
    chmod 600 *.key
    popd
}
function update_gpdb_certs() {
    for dir in $(find $MASTER_DATA_DIRECTORY/../.. -name pg_hba.conf)
        do
            if [[ ${CERTS_PATH} -ef $(dirname $dir) ]]; then
                echo "$(dirname $dir) already has certs"
            else
                cp -rf ${CERTS_PATH}/server.* $(dirname $dir)/
                cp -rf ${CERTS_PATH}/root.crt $(dirname $dir)/
            fi
        done
}

generate_gpdb_certs
generate_client_certs
update_gpdb_certs
