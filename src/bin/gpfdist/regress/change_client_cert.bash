#!/bin/bash

for dir in $(find $MASTER_DATA_DIRECTORY/../../.. -name pg_hba.conf)
    do
	if [ -d $(dirname $dir)/gpfdists ]; then
	    cp $(dirname $dir)/gpfdists/client2.crt $(dirname $dir)/gpfdists/client.crt
	    cp $(dirname $dir)/gpfdists/client2.key $(dirname $dir)/gpfdists/client.key
	fi
done
