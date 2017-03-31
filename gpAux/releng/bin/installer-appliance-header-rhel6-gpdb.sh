#!/bin/sh

installPath=/usr/local/GP-%%GP_VERSION%%
install_log=$( pwd )/install-$( date +%d%m%y-%H%M%S ).log

cat > ${install_log} <<-EOF
======================================================================
                             Greenplum DB
                    Appliance Automated Installer
----------------------------------------------------------------------
Timestamp ......... : $( date )
Product Installer.. : $( basename $0 )
Product Version ... : $( echo $installPath | cut -d "-" -f 2 )
Build Number ...... : 
Install Dir ....... : ${installPath}
Install Log file .. : ${install_log}
======================================================================

EOF

cat ${install_log}

# Make sure only root can run our script
if [ "`id -u `" != "0" ]; then
cat <<-EOF
======================================================================
ERROR: This script must be run as root.
======================================================================
EOF
   exit 1
fi

# Make sure hostfile exists in current working directory
if [ ! -f `pwd`/hostfile ]; then
cat <<-EOF
======================================================================
ERROR: hostfile does not exist (`pwd`/hostfile)
======================================================================
EOF
   exit 1
fi

#Check for needed tools
UTILS="sed tar awk cat mkdir tail mv more"
for util in ${UTILS}; do
    which ${util} > /dev/null 2>&1
    if [ $? != 0 ] ; then
cat <<-EOF
======================================================================
ERROR: ${util} was not found in your path.
       Please add ${util} to your path before running
       the installer again.
       Exiting installer.
======================================================================
EOF
       exit 1
    fi
done

#Verify that tar in path is GNU tar. If not, try using gtar.
#If gtar is not found, exit.
TAR=
tar --version > /dev/null 2>&1
if [ $? = 0 ] ; then
    TAR=tar
else
    which gtar > /dev/null 2>&1
    if [ $? = 0 ] ; then
        gtar --version > /dev/null 2>&1
        if [ $? = 0 ] ; then
            TAR=gtar
        fi
    fi
fi

if [ -z ${TAR} ] ; then
cat <<-EOF
======================================================================
ERROR: GNU tar is needed to extract this installer.
       Please add it to your path before running the installer again.
       Exiting installer.
======================================================================
EOF
    exit 1
fi
platform="RedHat/CentOS"
arch=x86_64
if [ -f /etc/redhat-release ]; then
    if [ `uname -m` != "${arch}" ] ; then
        echo "Installer will only install on ${platform} ${arch}"
        exit 1
    fi
else
    echo "Installer will only install on ${platform} ${arch}"
    exit 1
fi
SKIP=`awk '/^__END_HEADER__/ {print NR + 1; exit 0; }' "$0"`

if [ ! -d ${installPath} ] ; then
    echo "Creating ${installPath}"
    mkdir -p ${installPath}
    if [ $? -ne "0" ] ; then
    cat <<-EOF
======================================================================
ERROR: Error creating ${installPath}
======================================================================
EOF
        exit 1
    fi
fi 

tail -n +${SKIP} "$0" | ${TAR} zxf - -C ${installPath}
if [ $? -ne 0 ] ; then
    cat <<-EOF
======================================================================
ERROR: Extraction failed
======================================================================
EOF
    exit 1
fi

cat <<-EOF
======================================================================
Executing Post Appliance Installation Steps
======================================================================

EOF

##
## Setup symlink
##

symlinkPath="`dirname ${installPath}`/greenplum-db"
rm -f ${symlinkPath}
        ln -s "${installPath}" "${symlinkPath}"

##
## Update greenplum_path.sh
##

sed "s,^GPHOME.*,GPHOME=${installPath}," ${installPath}/greenplum_path.sh > ${installPath}/greenplum_path.sh.tmp
mv ${installPath}/greenplum_path.sh.tmp ${installPath}/greenplum_path.sh

##
## Setup segments
##

echo "Executing: source ${installPath}/greenplum_path.sh" | tee -a ${install_log}
source ${installPath}/greenplum_path.sh

echo "" | tee -a ${install_log}

echo "Executing: gpseginstall --file hostfile -c csv 2>&1 | tee -a ${install_log}" | tee -a ${install_log}
gpseginstall --file hostfile -c csv 2>&1 | tee -a ${install_log}

if [ $? -ne 0 ] ; then
    cat <<-EOF
======================================================================
ERROR: gpseginstall failed
======================================================================
EOF
    exit 1
fi

cat <<-EOF
======================================================================
Installation complete
======================================================================
EOF

exit 0

__END_HEADER__
