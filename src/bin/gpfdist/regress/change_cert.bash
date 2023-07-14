#!/bin/bash

if [ "$1" = "client" ]; then
	for dir in $(find $COORDINATOR_DATA_DIRECTORY/../../.. -name pg_hba.conf)
		do
		if [ -d $(dirname $dir)/gpfdists ]; then
			cp $(dirname $dir)/gpfdists/client2.crt $(dirname $dir)/gpfdists/client.crt
			cp $(dirname $dir)/gpfdists/client2.key $(dirname $dir)/gpfdists/client.key
		fi
	done
fi

# Create a new CA direrctory for gpfdist with a random root.crt
if [ "$1" = "gpfdist" ]; then
	mkdir data/gpfdist_ssl/certs_server_no_verify
	openssl req -new -newkey rsa:4096 -days 365 -nodes -x509 -subj "/C=US/ST=California/L=Palo Alto/O=Pivotal/CN=root" -out data/gpfdist_ssl/certs_server_no_verify/root.crt
	cp data/gpfdist_ssl/certs_matching/server.* data/gpfdist_ssl/certs_server_no_verify
fi

# Clear gpdb client and root CA certification.
if [ "$1" = "clear" ]; then
	for dir in $(find $COORDINATOR_DATA_DIRECTORY/../../.. -name pg_hba.conf)
		do
		if [ -d $(dirname $dir)/gpfdists ]; then
			mv $(dirname $dir)/gpfdists/client.crt $(dirname $dir)/gpfdists/client.crt.bak
			mv $(dirname $dir)/gpfdists/client.key $(dirname $dir)/gpfdists/client.key.bak
			mv $(dirname $dir)/gpfdists/root.crt $(dirname $dir)/gpfdists/root.crt.bak
		fi
	done
fi

# Reset gpdb client and root CA certification.
if [ "$1" = "recover" ]; then
	for dir in $(find $COORDINATOR_DATA_DIRECTORY/../../.. -name pg_hba.conf)
		do
		if [ -d $(dirname $dir)/gpfdists ]; then
			mv $(dirname $dir)/gpfdists/client.crt.bak $(dirname $dir)/gpfdists/client.crt
			mv $(dirname $dir)/gpfdists/client.key.bak $(dirname $dir)/gpfdists/client.key
			mv $(dirname $dir)/gpfdists/root.crt.bak $(dirname $dir)/gpfdists/root.crt
		fi
	done
fi