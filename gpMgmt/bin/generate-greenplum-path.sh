#!/usr/bin/env bash

cat <<"EOF"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
if [ ! -L "${SCRIPT_DIR}" ]; then
	GPDB_DIR=$(basename "${SCRIPT_DIR}")
else
	GPDB_DIR=$(basename "$(readlink "${SCRIPT_DIR}")")
fi
GPHOME=$(dirname "${SCRIPT_DIR}")/"${GPDB_DIR}"
EOF

cat <<"EOF"
PYTHONPATH="${GPHOME}/lib/python"
PATH="${GPHOME}/bin:${PATH}"
LD_LIBRARY_PATH="${GPHOME}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

if [ -e "${GPHOME}/etc/openssl.cnf" ]; then
	OPENSSL_CONF="${GPHOME}/etc/openssl.cnf"
fi

export GPHOME
export PATH
export PYTHONPATH
export LD_LIBRARY_PATH
export OPENSSL_CONF
EOF
