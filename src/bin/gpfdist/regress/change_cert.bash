#!/bin/bash

if [ "$1" = "client" ]; then
	for dir in $(find $COORDINATOR_DATA_DIRECTORY/../.. -name pg_hba.conf)
		do
		if [ -d $(dirname $dir)/gpfdists ]; then
			cp $(dirname $dir)/gpfdists/client2.crt $(dirname $dir)/gpfdists/client.crt
			cp $(dirname $dir)/gpfdists/client2.key $(dirname $dir)/gpfdists/client.key
		fi
	done
fi

# Create a new CA direrctory for gpfdist with a random root.crt
if [ "$1" = "gpfdist_create" ]; then
	cp data/gpfdist_ssl/certs_matching/server.* data/gpfdist_ssl/certs_server_no_verify
	cp data/gpfdist_ssl/certs_matching/root.crt data/gpfdist_ssl/certs_server_no_verify
	echo $(ls data/gpfdist_ssl/certs_server_no_verify)
fi

if [ "$1" = "gpfdist_replace_root" ]; then
	mv data/gpfdist_ssl/certs_server_no_verify/root.crt data/gpfdist_ssl/certs_server_no_verify/root.crt.bak
	openssl req -new -newkey rsa:4096 -days 365 -nodes -x509 -subj "/C=US/ST=California/L=Palo Alto/O=Pivotal/CN=root" -out data/gpfdist_ssl/certs_server_no_verify/root.crt
	echo $(ls data/gpfdist_ssl/certs_server_no_verify)
fi

if [ "$1" = "gpfdist_reset_root" ]; then
	rm -rf data/gpfdist_ssl/certs_server_no_verify/root.crt
	mv data/gpfdist_ssl/certs_server_no_verify/root.crt.bak data/gpfdist_ssl/certs_server_no_verify/root.crt
	echo $(ls data/gpfdist_ssl/certs_server_no_verify)
fi

# Clear gpdb root CA certification.
if [ "$1" = "clear_gpdb_root" ]; then
	for dir in $(find $COORDINATOR_DATA_DIRECTORY/../.. -name pg_hba.conf)
		do
		if [ -d $(dirname $dir)/gpfdists ]; then
			mv $(dirname $dir)/gpfdists/root.crt $(dirname $dir)/gpfdists/root.crt.bak
			echo $(ls $(dirname $dir)/gpfdists)
		fi
	done
fi

# Clear gpdb client CA certification.
if [ "$1" = "clear_gpdb_client" ]; then
	for dir in $(find $COORDINATOR_DATA_DIRECTORY/../.. -name pg_hba.conf)
		do
		if [ -d $(dirname $dir)/gpfdists ]; then
			mv $(dirname $dir)/gpfdists/client.crt $(dirname $dir)/gpfdists/client.crt.bak
			mv $(dirname $dir)/gpfdists/client.key $(dirname $dir)/gpfdists/client.key.bak
			echo $(ls $(dirname $dir)/gpfdists)
		fi
	done
fi

# Reset gpdb root CA certification.
if [ "$1" = "recover_gpdb_root" ]; then
	for dir in $(find $COORDINATOR_DATA_DIRECTORY/../.. -name pg_hba.conf)
		do
		if [ -d $(dirname $dir)/gpfdists ]; then
			mv $(dirname $dir)/gpfdists/root.crt.bak $(dirname $dir)/gpfdists/root.crt
			echo $(ls $(dirname $dir)/gpfdists)
		fi
	done
fi

# Reset gpdb root CA certification.
if [ "$1" = "recover_gpdb_client" ]; then
	for dir in $(find $COORDINATOR_DATA_DIRECTORY/../.. -name pg_hba.conf)
		do
		if [ -d $(dirname $dir)/gpfdists ]; then
			mv $(dirname $dir)/gpfdists/client.crt.bak $(dirname $dir)/gpfdists/client.crt
			mv $(dirname $dir)/gpfdists/client.key.bak $(dirname $dir)/gpfdists/client.key
			echo $(ls $(dirname $dir)/gpfdists)
		fi
	done
fi