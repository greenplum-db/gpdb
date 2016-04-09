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

# Solaris needs amd64 in PATH for java to work
if [ "${PLAT}" = "SunOS" ] ; then
cat <<EOF
PATH=\$GPHOME/bin:\$PYTHONHOME/bin:\$PATH
EOF
else
cat <<EOF
PATH=\$GPHOME/bin:\$PYTHONHOME/bin:\$PATH
EOF
fi

# OSX does not need JAVA_HOME 
if [ "${PLAT}" = "Darwin" ] ; then
cat << EOF
DYLD_LIBRARY_PATH=\$GPHOME/lib:\$PYTHONHOME/lib:\$DYLD_LIBRARY_PATH
EOF
fi

# OSX does not have LD_LIBRARY_PATH
if [ "${PLAT}" != "Darwin" ] ; then
    #Solaris needs /usr/sfw/lib in order for groupsession to work and /usr/local/lib for readline for Python 
    if [ "${PLAT}" = "SunOS" ] ; then
    cat <<EOF
LD_LIBRARY_PATH=\$GPHOME/lib:\$PYTHONHOME/lib:/usr/sfw/lib:/usr/local/python/lib:\$LD_LIBRARY_PATH
EOF
    else
    cat <<EOF
LD_LIBRARY_PATH=\$GPHOME/lib:\$PYTHONHOME/lib:\$LD_LIBRARY_PATH
EOF
    fi
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
EOF

cat <<EOF
export OPENSSL_CONF
EOF
