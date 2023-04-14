#!/bin/bash

BASENAME=$(basename "$0")

if [ "$#" != "1" ]; then
	echo "Usage: $BASENAME /path/to/config_file"
	exit 1
fi

if [ -a "$1" ]; then
	echo "Warning: file $1 already exists."
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

	[sse_s3]
	accessid = "$gpcloud_access_key_id"
	secret = "$gpcloud_secret_access_key"

	threadnum = 3
	chunksize = 16777217
	server_side_encryption = sse-s3

	[proxy]
	accessid = "$gpcloud_access_key_id"
	secret = "$gpcloud_secret_access_key"

	threadnum = 3
	chunksize = 16777217
	proxy = socks5://127.0.0.1:1080

	[wrong_proxy]
	accessid = "$gpcloud_access_key_id"
	secret = "$gpcloud_secret_access_key"

	threadnum = 3
	chunksize = 16777217
	proxy = socks5://127.0.0.1:1090

	[no_accessid_secret]
	accessid = ""
	secret = ""

	threadnum = 3
	chunksize = 16777217
	EOF
else
	echo "Error: environment varibles \$gpcloud_access_key_id and \$gpcloud_secret_access_key are not set."
	exit 1
fi

exit 0
