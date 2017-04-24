#!/bin/bash

if [ -f "$1" ]; then
	echo Warning: file $1 already exists
	exit 0
fi

if [ -n "$gpcloud_access_key_id" ] && [ -n "$gpcloud_secret_access_key" ]; then
	cat > $1 <<-EOF
	[default]
	accessid = "$gpcloud_access_key_id"
	secret = "$gpcloud_secret_access_key"

	threadnum = 3
	chunksize = 16777217

	[no_autocompress]
	accessid = "$gpcloud_access_key_id"
	secret = "$gpcloud_secret_access_key"

	autocompress = false

	threadnum = 3
	chunksize = 16777217
	EOF
else
	echo "Error: environment varibles \$gpcloud_access_key_id and \$gpcloud_secret_access_key are not set."
	exit 1
fi

exit 0
