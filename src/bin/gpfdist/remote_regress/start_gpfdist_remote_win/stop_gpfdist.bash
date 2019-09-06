#!/bin/bash -l
set -ex

ps -A -o pid,command  | grep ssh |grep "start_gpfdist" |grep -v "gpdb" | awk '{print $1;}'|xargs kill
ssh -i "${REMOTE_KEY}" -T -p "${REMOTE_PORT}" "${REMOTE_USER}@${REMOTE_HOST}" \
"stop_gpfdist.bat" >/tmp/stop_gpfdist.log 2>&1
