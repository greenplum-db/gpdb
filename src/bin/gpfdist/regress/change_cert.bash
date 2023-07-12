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

if [ "$1" = "gpfdist" ]; then
	mkdir data/gpfdist_ssl/certs_server_no_verify
	openssl req -new -newkey rsa:4096 -days 365 -nodes -x509 -subj "/C=US/ST=California/L=Palo Alto/O=Pivotal/CN=root" -out data/gpfdist_ssl/certs_server_no_verify/root.crt
	cp data/gpfdist_ssl/certs_matching/server.* data/gpfdist_ssl/certs_server_no_verify
fi