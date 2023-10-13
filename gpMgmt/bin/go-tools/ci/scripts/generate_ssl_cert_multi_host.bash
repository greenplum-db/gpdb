#!/bin/bash

rm -rf ./certificates
mkdir ./certificates
source /usr/local/greenplum-db-devel/greenplum_path.sh

# Private key and self-signed certificate only on cdw
echo "Generating ca cert on cdw"
openssl req -x509 -newkey rsa:4096 -days 365 -nodes -keyout ./certificates/ca-key.pem -out ./certificates/ca-cert.pem -subj "/C=US/ST=California/L=Palo Alto/O=Greenplum/OU=GPDB/CN=cdw"

# Generate server cert for each host and copy them to the respective hosts
while read host; do
    echo "Generating server cert for $host"
    # Private key and certificate signing request
    openssl req -newkey rsa:4096 -nodes -keyout ./certificates/server-key.pem -out ./certificates/server-request.pem -subj "/C=US/ST=California/L=Palo Alto/O=Greenplum/OU=GPDB/CN=$host"

    # Signed certificate
    echo "subjectAltName=DNS:$host,DNS:localhost,IP:0.0.0.0" > ./certificates/extensions.conf
    openssl x509 -req -in ./certificates/server-request.pem -days 365 -CA ./certificates/ca-cert.pem -CAkey ./certificates/ca-key.pem -CAcreateserial -out ./certificates/server-cert.pem -extfile ./certificates/extensions.conf

    echo "Copying server cert to /tmp/certificates on $host"
    gpsync -a -h $host certificates =:/tmp/
done < /tmp/hostfile_all
