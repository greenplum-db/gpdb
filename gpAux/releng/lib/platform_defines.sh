#!/bin/bash

# BuildBot has to run through launchd on OSX 10.5 (Leopard); we have to pull in ~build/.bashrc ourselves
if [ "`uname -rs`" = "Darwin 9.6.0" ]; then
    pushd ~ >/dev/null && . .profile && popd >/dev/null
fi

PLAT=`uname -s`
if [ $? -ne 0 ] ; then
    echo "Error executing uname -s"
    exit 1
fi

#Set some default defines for commands
TAR="tar"
PS="ps"
MAIL="mail"
WHOAMI="whoami"
KERNEL_REV=`uname -rv`
HOST=`uname -n`
HOST_IP=`host ${HOST} | sed 's,.*address ,,'`
MPP_ARCH=""
BIZGRES_ARCH=""
CDBUNIT_ARCH=""
OS_RELEASE=""
TIME_CMD="time -p"
TAIL_CMD="tail"
FIND="find"
#This is used by the buildDB
HOST_ID=""

#Current Table of host_id's
#host_id | hostname
#----+------------
#1 | stinger4
#2 | kite12
#3 | minidev
#4 | g5dev 
#5 | kite14  
#6 | build1 
#7 | build0 
#19 | bit1
#20 | ice	// temporary CentOS 5 build server

#Solaris
if [ ${PLAT} = "SunOS" ]; then
    CPU=`isainfo | awk '{print $1}'`
    if [ "${CPU}" = "amd64" ] ; then
        MPP_ARCH="SOL-x86_64"
        BIZGRES_ARCH="Solaris"
        CDBUNIT_ARCH="Solaris64"
        TEST_ARCH="SOL-x86_64"
        PLATFORM_ID="4"
        PDW_HW="x86_64"
    fi
    if [ "${CPU}" = "sparcv9" ] ; then
        OS_VERSION_MAJOR=`uname -r | awk -F. ' { print $2}'`
        MPP_ARCH="SOL${OS_VERSION_MAJOR}-sparc"
        BIZGRES_ARCH="Solaris"
        CDBUNIT_ARCH="Solaris64sparc"
        TEST_ARCH="SOL${OS_VERSION_MAJOR}-sparc"
        PLATFORM_ID="19"
        PDW_HW="sparc"
    fi

    # remap values for 32-/64-bit variants if BLD_ARCH is set
    if [ ! -z "${BLD_ARCH}" ]; then
        case ${BLD_ARCH} in
            sol10_x86_32)
                MPP_ARCH=SOL10-x86_32
                TEST_ARCH=SOL10-x86_32
                PLATFORM_ID=32
            ;;
            sol10_sparc_64)
                MPP_ARCH=SOL10-sparc_64
                TEST_ARCH=SOL10-sparc_64
                PLATFORM_ID=21
            ;;
            sol10_sparc_32)
                MPP_ARCH=SOL10-sparc_32
                TEST_ARCH=SOL10-sparc_32
                PLATFORM_ID=22
            ;;
            sol9_sparc_64)
                MPP_ARCH=SOL9-sparc_64
                TEST_ARCH=SOL9-sparc_64
                PLATFORM_ID=24
            ;;
            sol9_sparc_32)
                MPP_ARCH=SOL9-sparc_32
                TEST_ARCH=SOL9-sparc_32
                PLATFORM_ID=25
            ;;
            sol8_sparc_64)
                MPP_ARCH=SOL8-sparc_64
                TEST_ARCH=SOL8-sparc_64
                PLATFORM_ID=27
            ;;
            sol8_sparc_32)
                MPP_ARCH=SOL8-sparc_32
                TEST_ARCH=SOL8-sparc_32
                PLATFORM_ID=28
        esac
    fi
    TAIL_CMD="/usr/xpg4/bin/tail"
    TAR="gtar"
    WHOAMI="/usr/ucb/whoami"
    PS="/usr/ucb/ps"
    MAIL="mailx"
    OS_RELEASE=`grep Solaris /etc/release`
    FIND="gfind"
    PDW_OS="Solaris"
fi
#Linux i386 or x86_64
if [ ${PLAT} = "Linux" ]; then
    OS_RELEASE="Undefined linux OS"
    OS_VERSION_MAJOR=4
    if [ -r /etc/redhat-release ] ; then
        OS_RELEASE=`cat /etc/redhat-release` 
        MPP_ARCH_BASE="RHEL"
        #Check for CentOS or Red Hat
        if grep 'Red Hat' /etc/redhat-release > /dev/null 2>&1 ; then
            OS_VERSION_MAJOR=`echo $OS_RELEASE | awk ' { print $7}' | awk -F "." '{print $1}'`
        else
            OS_VERSION_MAJOR=`echo $OS_RELEASE | awk ' { print $3}' | awk -F "." ' { print $1}'`
        fi
    fi
    if [ -r /etc/SuSE-release ]; then
        OS_RELEASE=`head -1 /etc/SuSE-release`
        MPP_ARCH_BASE="SLES"
        OS_VERSION_MAJOR=`head -2 /etc/SuSE-release | tail -1 | awk '{print $3}'`
    fi
    CPU=`uname -p`
    if [ ${CPU} = "i686" ]; then
        MPP_ARCH="${MPP_ARCH_BASE}${OS_VERSION_MAJOR}-i386"
        BIZGRES_ARCH="Redhat32"
        CDBUNIT_ARCH="Linux32"
        TEST_ARCH="Linux-i386"
        PLATFORM_ID="5"
        PDW_HW="i386"
    fi
    if [ ${CPU} = "x86_64" ]; then
        MPP_ARCH="${MPP_ARCH_BASE}${OS_VERSION_MAJOR}-x86_64"
        BIZGRES_ARCH="Redhat64"
        CDBUNIT_ARCH="Linux64"
        TEST_ARCH="${MPP_ARCH}"
        if [ "${OS_VERSION_MAJOR}" = "4" ]; then
            PLATFORM_ID="6"
        else
            PLATFORM_ID="20"
        fi
        PDW_HW="x86_64"
    fi
    PDW_OS="Linux"

    # remap kite14 to win32 as it is a mingw32 cross-compiler host
    if [ "`hostname`" = "kite14" ]; then
        MPP_ARCH="WinXP-x86_32"
        CDBUNIT_ARCH="WinXP-x86_32"
        TEST_ARCH="WinXP-x86_32"
        PLATFORM_ID="7"
        PDW_HW="x86_32"
        PDW_OS="Windows"
    fi
fi
#OSX PPC or Intel
if [ ${PLAT} = "Darwin" ]; then
    CPU=`uname -p`
    if [ ${CPU} = "powerpc" ]; then
        MPP_ARCH="OSX-ppc"
        BIZGRES_ARCH="OSX"
        CDBUNIT_ARCH="OSX-ppc"
        TEST_ARCH="OSX-PPC"
        PLATFORM_ID="1"
        PDW_HW="ppc"
    fi
    if [ ${CPU} = "i386" ]; then
        MPP_ARCH="OSX-i386"
        BIZGRES_ARCH="OSX-i386"
        CDBUNIT_ARCH="OSX-i386"
        TEST_ARCH="OSX-i386"
        PLATFORM_ID="2"
        PDW_HW="i386"
    fi
    MAIL="mailx"
    OS_RELEASE=`/usr/sbin/system_profiler SPSoftwareDataType | awk -F: '/System Version/ {print $2}'`
    PDW_OS="OSX"
fi
#Catch all for undefined arches
if [ x"${MPP_ARCH}" = "x" ]; then
    echo "Error calculating MPP_ARCH"
    exit 1
fi
if [ x"${CDBUNIT_ARCH}" = "x" ]; then
    echo "Error calculating CDBUNIT_ARCH"
    exit 1
fi

export PLAT
export TAR
export MPP_ARCH
export BIZGRES_ARCH
export CDBUNIT_ARCH
export PS
export MAIL
export KERNEL_REV
export OS_RELEASE
export TIME_CMD
export PDW_OS
export PDW_HW
