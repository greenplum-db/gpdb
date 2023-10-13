#!/bin/bash

rm -rf ./certificates
mkdir ./certificates

# Private key and self-signed certificate
openssl req -x509 -sha256 -newkey rsa:4096 -days 365 -nodes -keyout ./certificates/ca-key.pem -out ./certificates/ca-cert.pem -subj "/C=US/ST=California/L=Palo Alto/O=Greenplum/OU=GPDB/CN=$1"

# Private key and certificate signing request
openssl req -newkey rsa:4096 -nodes -keyout ./certificates/server-key.pem -out ./certificates/server-request.pem -subj "/C=US/ST=California/L=Palo Alto/O=Greenplum/OU=GPDB/CN=$1" -sha256

# Signed certificate
echo "subjectAltName=DNS:$1,DNS:localhost,IP:0.0.0.0" > ./certificates/extensions.conf
openssl x509 -req -in ./certificates/server-request.pem -days 365 -CA ./certificates/ca-cert.pem -CAkey ./certificates/ca-key.pem -CAcreateserial -out ./certificates/server-cert.pem -extfile ./certificates/extensions.conf -sha256
