#!/bin/bash

if [ x$1 != x ] ; then
    GPHOME_PATH=$1
else
    GPHOME_PATH="\`pwd\`"
fi

if [ "$2" = "ISO" ] ; then
	cat <<-EOF
		if [ "\${BASH_SOURCE:0:1}" == "/" ]
		then
		    GPHOME=\`dirname "\$BASH_SOURCE"\`
		else
		    GPHOME=\`pwd\`/\`dirname "\$BASH_SOURCE"\`
		fi
	EOF
else
	cat <<-EOF
		GPHOME=${GPHOME_PATH}
	EOF
fi


PLAT=`uname -s`
if [ $? -ne 0 ] ; then
    echo "Error executing uname -s"
    exit 1
fi

cat << EOF

# Replace with symlink path if it is present and correct
if [ -h \${GPHOME}/../greenplum-db ]; then
    GPHOME_BY_SYMLINK=\`(cd \${GPHOME}/../greenplum-db/ && pwd -P)\`
    if [ x"\${GPHOME_BY_SYMLINK}" = x"\${GPHOME}" ]; then
        GPHOME=\`(cd \${GPHOME}/../greenplum-db/ && pwd -L)\`/.
    fi
    unset GPHOME_BY_SYMLINK
fi
EOF

cat <<EOF
#setup PYTHONHOME
if [ -x \$GPHOME/ext/python/bin/python ]; then
    PYTHONHOME="\$GPHOME/ext/python"
fi
EOF

#setup PYTHONPATH
if [ "x${PYTHONPATH}" == "x" ]; then
    PYTHONPATH="\$GPHOME/lib/python"
else
    PYTHONPATH="\$GPHOME/lib/python:${PYTHONPATH}"
fi
cat <<EOF
PYTHONPATH=${PYTHONPATH}
EOF

cat <<EOF
PATH=\$GPHOME/bin:\$PYTHONHOME/bin:\$PATH
EOF

# OSX does not need JAVA_HOME 
if [ "${PLAT}" = "Darwin" ] ; then
cat << EOF
DYLD_LIBRARY_PATH=\$GPHOME/lib:\$PYTHONHOME/lib:\$DYLD_LIBRARY_PATH
EOF
fi

# OSX does not have LD_LIBRARY_PATH
if [ "${PLAT}" != "Darwin" ] ; then
    cat <<EOF
LD_LIBRARY_PATH=\$GPHOME/lib:\$PYTHONHOME/lib:\$LD_LIBRARY_PATH
EOF
fi

# AIX uses yet another library path variable
# Also, Python on AIX requires special copies of some libraries.  Hence, lib/pware.
if [ "${PLAT}" = "AIX" ]; then
cat <<EOF
PYTHONPATH=\${GPHOME}/ext/python/lib/python2.7:\${PYTHONPATH}
LIBPATH=\${GPHOME}/lib/pware:\${GPHOME}/lib:\${GPHOME}/ext/python/lib:/usr/lib/threads:\${LIBPATH}
export LIBPATH
GP_LIBPATH_FOR_PYTHON=\${GPHOME}/lib/pware
export GP_LIBPATH_FOR_PYTHON
EOF
fi

# openssl configuration file path
cat <<EOF
OPENSSL_CONF=\$GPHOME/etc/openssl.cnf
EOF

cat <<EOF
export GPHOME
export PATH
EOF

if [ "${PLAT}" != "Darwin" ] ; then
cat <<EOF
export LD_LIBRARY_PATH
EOF
else
cat <<EOF
export DYLD_LIBRARY_PATH
EOF
fi

cat <<EOF
export PYTHONPATH
export PYTHONHOME
EOF

cat <<EOF
export OPENSSL_CONF
EOF
