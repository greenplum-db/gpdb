#!/bin/bash

# Remove existing certificates directory and create a new one
rm -rf ./certificates
mkdir ./certificates

source /usr/local/greenplum-db-devel/greenplum_path.sh

# Generate private key and self-signed certificate for the Certificate Authority (CA) on cdw
echo "Generating CA certificate on cdw"
openssl req -x509 -newkey rsa:4096 -days 365 -nodes \
    -keyout ./certificates/ca-key.pem \
    -out ./certificates/ca-cert.pem \
    -subj "/C=US/ST=California/L=Palo Alto/O=Greenplum/OU=GPDB/CN=cdw" \

# Generate server certificates for each host and copy them to respective hosts
while read host; do
    echo "Generating server certificate for $host"

    # Generate private key and certificate signing request for the server
    openssl req -newkey rsa:4096 -nodes \
        -keyout ./certificates/server-key.pem \
        -out ./certificates/server-request.pem \
        -subj "/C=US/ST=California/L=Palo Alto/O=Greenplum/OU=GPDB/CN=$host"

    # Sign the server certificate using the CA
    echo "subjectAltName=DNS:$host,DNS:localhost,IP:0.0.0.0" > ./certificates/extensions.conf
    openssl x509 -req -in ./certificates/server-request.pem -days 365 \
        -CA ./certificates/ca-cert.pem \
        -CAkey ./certificates/ca-key.pem \
        -CAcreateserial \
        -out ./certificates/server-cert.pem \
        -extfile ./certificates/extensions.conf

    echo "Copying server certificate to /tmp/certificates on $host"
    gpsync -a -h $host certificates =:/tmp/

done < /tmp/hostfile_all
