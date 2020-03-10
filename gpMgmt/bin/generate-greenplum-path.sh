#!/usr/bin/env bash

cat <<"EOF"
#!/usr/bin/env bash
GPHOME="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

PYTHONHOME="${GPHOME}/ext/python"
PYTHONPATH="${GPHOME}/lib/python"
PATH="${GPHOME}/bin:${PYTHONHOME}/bin:${PATH}"
LD_LIBRARY_PATH="${GPHOME}/lib:${PYTHONHOME}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
EOF

PLAT=`uname -s`
if [ $? -ne 0 ] ; then
    echo "Error executing uname -s"
    exit 1
fi

# AIX uses yet another library path variable
# Also, Python on AIX requires special copies of some libraries.  Hence, lib/pware.
if [ "${PLAT}" = "AIX" ]; then
cat <<"EOF"
PYTHONPATH="${GPHOME}/ext/python/lib/python2.7:${PYTHONPATH}"
LIBPATH="${GPHOME}/lib/pware:${GPHOME}/lib:${GPHOME}/ext/python/lib:/usr/lib/threads:${LIBPATH}"
export LIBPATH
GP_LIBPATH_FOR_PYTHON="${GPHOME}/lib/pware"
export GP_LIBPATH_FOR_PYTHON
EOF
fi

cat <<"EOF"

if [ -e "$GPHOME/etc/openssl.cnf" ]; then
	OPENSSL_CONF="$GPHOME/etc/openssl.cnf"
fi

export GPHOME
export PATH
export PYTHONHOME
export PYTHONPATH
export LD_LIBRARY_PATH
export OPENSSL_CONF
EOF
