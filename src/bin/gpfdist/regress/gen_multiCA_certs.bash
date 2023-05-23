#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Usage: gen_multiCA_certs.bash CERTS_PATH"
    exit 1
fi

CERTS_PATH=$1

function gencert() {
openssl req -new -newkey rsa:4096 -days 365 -nodes -x509 \
    -subj "/C=US/ST=California/L=Palo Alto/O=Pivotal/CN=$1root" \
    -keyout $1root.key  -out $1root.crt

openssl req -new -newkey rsa:4096 -nodes \
    -subj "/C=US/ST=California/L=Palo Alto/O=Pivotal/CN=$1ca" \
    -keyout $1ca.key -out $1ca.csr

openssl x509 -req -in $1ca.csr -CA $1root.crt -CAkey $1root.key \
    -extfile <(printf "basicConstraints=CA:TRUE") \
    -days 365 -out $1ca.crt -sha256 -CAcreateserial

openssl req -new -newkey rsa:2048 -nodes \
    -subj "/C=US/ST=California/L=Palo Alto/O=Pivotal/CN=localhost" \
    -keyout $1.key  -out $1.csr

openssl x509 -req -in $1.csr -CA $1ca.crt -CAkey $1ca.key \
    -days 365 -out $1.crt -sha256 -CAcreateserial
}

function generate_gpfdist_certs() {
	mkdir -p ${CERTS_PATH}
	pushd ${CERTS_PATH}
	rm -f *.crt *.key *.csr *.srl
	gencert server
	gencert client
	gencert client2
	cp clientroot.crt root.crt
	cat clientca.crt >> root.crt
	cat client2root.crt >> root.crt
	cat client2ca.crt >> root.crt
	cat serverca.crt >> server.crt
	rm -f *.csr *.srl
	popd
}

function update_gpdb_certs() {
    for dir in $(find $COORDINATOR_DATA_DIRECTORY/../../.. -name pg_hba.conf)
        do
        if [ -d $(dirname $dir)/gpfdists ]; then
            rm -rf $(dirname $dir)/gpfdists/*
            cp -rf ${CERTS_PATH}/* $(dirname $dir)/gpfdists
            cp $(dirname $dir)/gpfdists/serverroot.crt $(dirname $dir)/gpfdists/root.crt
        fi
    done
}

generate_gpfdist_certs
update_gpdb_certs

