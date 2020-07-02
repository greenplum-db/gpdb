#!/usr/bin/env bash

if [ -z "$1" ]; then
  printf "Must specify a value for GPHOME"
  exit 1
fi

SET_PYTHONHOME="${2:-no}"

GPHOME_PATH="$1"
cat <<EOF
GPHOME="${GPHOME_PATH}"

EOF

if [ "${SET_PYTHONHOME}" = "yes" ]; then
	cat <<-"EOF"
	PYTHONHOME="${GPHOME}/ext/python"
	export PYTHONHOME

	PATH="${PYTHONHOME}/bin:${PATH}"
	LD_LIBRARY_PATH="${PYTHONHOME}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
	EOF
fi

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
