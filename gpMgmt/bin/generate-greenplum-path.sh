#!/usr/bin/env bash

cat <<"EOF"
#!/usr/bin/env bash
GPHOME="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

EOF

if [ -x "${PYTHONHOME}/bin/python" ]; then
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
