#!/bin/bash

# Remove existing certificates directory and create a new one
rm -rf ./certificates
mkdir ./certificates

# Generate private key and self-signed certificate for the Certificate Authority (CA)
openssl req -x509 -sha256 -newkey rsa:4096 -days 365 -nodes \
    -keyout ./certificates/ca-key.pem \
    -out ./certificates/ca-cert.pem \
    -subj "/C=US/ST=California/L=Palo Alto/O=Greenplum/OU=GPDB/CN=$1" \

# Generate private key and certificate signing request for the server
openssl req -newkey rsa:4096 -nodes \
    -keyout ./certificates/server-key.pem \
    -out ./certificates/server-request.pem \
    -subj "/C=US/ST=California/L=Palo Alto/O=Greenplum/OU=GPDB/CN=$1" -sha256

# Create a configuration file for certificate extensions
echo "subjectAltName=DNS:$1,DNS:localhost,IP:0.0.0.0" > ./certificates/extensions.conf

# Sign the server certificate using the CA
openssl x509 -req -in ./certificates/server-request.pem -days 365 \
    -CA ./certificates/ca-cert.pem \
    -CAkey ./certificates/ca-key.pem \
    -CAcreateserial \
    -out ./certificates/server-cert.pem \
    -extfile ./certificates/extensions.conf \
    -sha256
