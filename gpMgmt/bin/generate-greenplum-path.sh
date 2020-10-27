#!/usr/bin/env bash

cat <<"EOF"
WORKING_DIR="$(pwd)"

# Follow symlinks to find the real script
cd "$(dirname "$0")" || exit 1
script_file=$(pwd)/$(basename "$0")
while [[ -L "$script_file" ]]; do
  script_file=$(readlink "$script_file")
  cd "$(dirname "$script_file")" || exit 1
  script_file=$(pwd)/$(basename "$script_file")
done

SCRIPT_DIR="$( (cd "$( dirname "${script_file}" )" && pwd -P) )"
cd "$WORKING_DIR" || exit 1

GPDB_DIR=$(basename "${SCRIPT_DIR}")
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
