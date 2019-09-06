#!/bin/bash -l
set -ex

nohup ssh -i "${REMOTE_KEY}" -T  -L7070:127.0.0.1:7070  -p "${REMOTE_PORT}" "${REMOTE_USER}@${REMOTE_HOST}" \
"start_gpfdist_ssl_not_matching.bat" >/tmp/start_gpfdist_ssl_not_matching.log 2>&1 &
