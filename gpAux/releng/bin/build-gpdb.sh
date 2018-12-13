#!/bin/bash
## ======================================================================
## ======================================================================

BLD_ROOT=$( pwd )

set -x

#This script performs an MPP build

#Pick up some platform defines
PLATFORM_DEFS=${BLD_ROOT}/releng/lib/platform_defines.sh
if [ -r ${PLATFORM_DEFS} ] ; then
    . ${PLATFORM_DEFS}
else
    echo "${PLATFORM_DEFS} does not appear to exist."
fi

if [ "$1" = "debug" ] || [ "$1" = "dbg" ] || [ "$1" = "dbg32" ]; then
    BUILD_TYPE="debug"
    DEBUG="true"
    ISGCOV="false"
fi
if [ "$1" = "optimized" ] || [ "$1" = "opt" ] || [ "$1" = "opt32" ]; then
    BUILD_TYPE="optimized"
    DEBUG="false"
    ISGCOV="false"
fi
. /etc/relengrc
if [ -z "${BLD_ARCH}" ]; then
  if [ "$1" = "opt32" ] || [ "$1" = "dbg32" ]; then
    BLD_ARCH=`echo ${BLD_ARCH_DEFAULT} | sed -e s,_64$,_32,`
  else
    BLD_ARCH=${BLD_ARCH_DEFAULT}
  fi
fi
if [ "$1" = "gcov" ] ; then
    BUILD_TYPE="gcov"
    DEBUG="true"
    ISGCOV="true"
fi

if [ x"$1" = "x" ] ; then
    echo "Error: Argument must be supplied when running $0"
    echo "Valid arguments are optimized, debug."
    exit 1
fi

WORKDIR=`pwd`
PRODUCT="GPDB"
ARCHIVE_HOST="build-prod"
GPDB="greenplum-db"
DEVPATH=
BUILD_INFO="buildInfo.txt"
BUILD_ID_INFO="buildIdInfo.txt"
BUILD_ID="buildId.txt"
GCC_VERSION=`gcc --version | grep GCC`
BUILD_START_DATE=`date`

#Calc the branch 

if [ -z "${BRANCH}" ]; then
    BRANCH=`awk '{print $4}' .buildbot-sourcedata  | sed "s,[',\,],,g"`
    if [ $? -ne 0 ] ; then
        echo "Error getting branch from .buildbot-sourcedata"
        exit 1
    fi
fi

if [ -f /releng/bin/gcc ] && [ "${BRANCH}" = "main" ]; then
    echo `date` "Setting path to use /releng/bin/gcc"
    echo "export PATH=/releng/bin:$PATH"
    export PATH=/releng/bin:$PATH
    echo ""
fi

if [ "${BRANCH}" = "main" ]; then
    echo "`date` -- INFO: Setting default R_HOME value"
    #Default
    RHOME="/releng/lib/R"

    if [ "${MPP_ARCH}" = "RHEL4-i386" ] ; then
        RHOME="/releng/lib/R"
    fi
    if [ "${MPP_ARCH}" = "RHEL4-x86_64" ] || [ "${MPP_ARCH}" = "RHEL5-x86_64" ] ; then
        RHOME="/releng/lib64/R"
    fi
    if [ "${MPP_ARCH}" = "OSX-i386" ] ; then
        if [ -d /releng/R-2.6.1 ]; then
            RHOME="/releng/R-2.6.1/lib/R"
        else
            RHOME="/releng/lib/R"
        fi
        echo "export DYLD_LIBRARY_PATH=${RHOME}/lib:$DYLD_LIBRARY_PATH"
        export DYLD_LIBRARY_PATH=${RHOME}/lib:$DYLD_LIBRARY_PATH
    fi
    if [ "${MPP_ARCH}" = "SOL-x86_64" ] ; then
        RHOME="/releng/lib/R"
    fi
    if [ "${MPP_ARCH}" = "SuSE10-x86_64" ] ; then
        RHOME="/releng/R-2.6.1/lib64/R/"
    fi

    echo "`date` -- INFO: export R_HOME=${RHOME}"    
    export R_HOME=${RHOME}
    echo ""

fi

if [ -d /opt/gcc-4.1.1/ ] && [ "${BRANCH}" != "main" ] && [ "${BRANCH}" = "Release-3_4-branch" ] && [ "${BRANCH}" = "Release-3_4_filerep-branch" ] && [ "${BRANCH}" != "Release-3_3-branch" ] && [ "${BRANCH}" != "Release-3_2-branch" ] && [ "${BRANCH}" != "Release-3_2_clients-branch" ] && [ "${BRANCH}" != "Release-3_2_0_6_clients-branch" ] ; then
    echo ""
    echo "Adding GCC 4.1.1 to PATH"
    echo "export PATH=/opt/gcc-4.1.1/bin:$PATH"
    export PATH=/opt/gcc-4.1.1/bin:$PATH
    echo "export LD_LIBRARY_PATH=/opt/gcc-4.1.1/lib:$LD_LIBRARY_PATH"
    export LD_LIBRARY_PATH=/opt/gcc-4.1.1/lib:$LD_LIBRARY_PATH
    echo ""
fi

generateBuildInfo() {
    #Generate buildInfo.txt

    echo "Generating ${BUILD_INFO}"
    rm -f ${BUILD_INFO}

    #Get CVS_DATESTAMP from .buildbot-sourcedata 
    BUILDBOT_SRC_DATA="${WORKDIR}/.buildbot-sourcedata"
    if [ -f "${BUILDBOT_SRC_DATA}" ] ; then
        awk '{print $6, $7}' ${BUILDBOT_SRC_DATA} | sed -e "s,',,g" -e 's/,//g'
        SRC_DATESTAMP=`awk '{print $6, $7}' ${BUILDBOT_SRC_DATA} | sed -e "s,',,g" -e 's/,//g'`
        if [ $? -ne 0 ] ; then
            echo "Error getting datestamp from ${BUILDBOT_SRC_DATA}"
            exit 1
        fi
    else
        echo "############################################"
        echo "Error: ${BUILDBOT_SRC_DATA} is not present."
        echo "Not using datestamp in build number"
        echo "############################################"
    fi

    touch ${BUILD_INFO}
    echo "module=cdb2" >> ${BUILD_INFO}
    echo "branch=${BRANCH}" >> ${BUILD_INFO}
    echo "checkout_time=${SRC_DATESTAMP}" >> ${BUILD_INFO}
    echo "host=${HOST}" >> ${BUILD_INFO}
    echo "platform=${TEST_ARCH}" >> ${BUILD_INFO} 
    echo "build_time=${BUILD_START_DATE}" >> ${BUILD_INFO}
    echo "build_duration=${BUILD_DURATION}" >> ${BUILD_INFO}
    echo "build_cmd=make" >> ${BUILD_INFO}
    echo "compiler=${GCC_VERSION}" >> ${BUILD_INFO}
    echo "isDebug=${DEBUG}" >> ${BUILD_INFO}
    echo "isGcov=${ISGCOV}" >> ${BUILD_INFO}

    echo "Contents of buildInfo.txt" 
    echo "=================================="
    cat buildInfo.txt
    echo "=================================="
    echo ""

    echo "Copy build.out to build-prod..."
    BUILD_NUMBER=`cat ${WORKDIR}/BUILD_NUMBER`
    ARCHIVE_PATH="/var/www/html/reports/${BUILD_NUMBER}/compile/${BRANCH}_${BUILD_TYPE}/${MPP_ARCH}/${HOST}/"

    echo 'ssh build@${ARCHIVE_HOST} "mkdir -p ${ARCHIVE_PATH}"'
    ssh build@${ARCHIVE_HOST} "mkdir -p ${ARCHIVE_PATH}"
    if [ $? -ne 0 ] ; then
        echo "Error creating directory for build.out on build-prod"
    fi
    echo "scp -pq ${WORKDIR}/build.out ${ARCHIVE_HOST}:${ARCHIVE_PATH}"
    scp -pq ${WORKDIR}/build.out ${ARCHIVE_HOST}:${ARCHIVE_PATH}
    if [ $? -ne 0 ] ; then
        echo "Error copying build.out to build-prod"
    fi
}


generateBuildIDInfo() {
    echo "Generating ${BUILD_ID_INFO}"
    rm -f ${BUILD_ID_INFO}

    #Get CVS_DATESTAMP from .buildbot-sourcedata
    BUILDBOT_SRC_DATA="${WORKDIR}/.buildbot-sourcedata"
    if [ -f "${BUILDBOT_SRC_DATA}" ] ; then
        SRC_DATESTAMP=`awk '{print $6, $7}' ${BUILDBOT_SRC_DATA} | sed -e "s,',,g" -e 's/,//g'`
        if [ $? -ne 0 ] ; then
            echo "Error getting datestamp from ${BUILDBOT_SRC_DATA}"
            exit 1
        fi
    else
        echo "##################################"
        echo "Error: ${BUILDBOT_SRC_DATA} is not present."
        echo "Not using datestamp in build number"
        echo "##################################"
    fi

    touch ${BUILD_ID_INFO}
    echo "module=cdb2" >> ${BUILD_ID_INFO}
    echo "branch=${BRANCH}" >> ${BUILD_ID_INFO}
    echo "checkout_time=${SRC_DATESTAMP}" >> ${BUILD_ID_INFO}

    echo "Contents of ${BUILD_ID_INFO}"
    echo "======================"
    cat ${BUILD_ID_INFO}
    echo "======================"
}

setBuildDbPath() {
    echo `date` "Setting up path to run buildDB.py"
    TMP_PATH=${PATH}
    export PATH=${HOME}/build_tools/python-2.5/bin:${PATH}
    TMP_LD_LIBRARY_PATH=${LD_LIBRARY_PATH}
    export LD_LIBRARY_PATH=${HOME}/build_tools/pg-8.2.4/lib:${LD_LIBRARY_PATH}
    TMP_DYLD_LIBRARY_PATH=${DYLD_LIBRARY_PATH} 
    export DYLD_LIBRARY_PATH=${HOME}/build_tools/pg-8.2.4/lib:${DYLD_LIBRARY_PATH}
    echo ""
}

getBuildId() {
    echo "Checking out buildDB"
    cd ${WORKDIR}

    echo "mkdir buildTest"
    mkdir buildTest

    echo "cd buildTest"
    cd buildTest

    echo "checkout_source_set buildTest main buildTest-sync.log `date +%Y%m%d%H%M`"
    checkout_source_set buildTest main buildTest-sync.log `date +%Y%m%d%H%M`
    if [ $? -ne 0 ] ; then
        echo "Error checking out buildTest"
        exit 1
    fi

    echo "cd buildDB"
    cd buildDB

    #Create the buildIdInfo file
    generateBuildIDInfo

    #Get the buildID
    ./buildDB.py -p 5432 -h build-prod -d cdbunit -u postgres addBuild buildIdInfo.txt > ${BUILD_ID} 2>&1
    if [ $? -ne 0 ] ; then
        echo "Error running buildDB"
        exit 1
    fi

    echo "Contents of ${BUILD_ID}"
    echo "======================"
    cat ${BUILD_ID}
    echo "======================"

    #If buildId is not present in the txt file, error out
    grep buildId ${BUILD_ID} > /dev/null 2>&1
    if [ $? -ne 0 ] ; then
        echo "Error: buildId is not present in ${BUILD_ID}"
        exit 1
    fi 

    #Create BUILD_NUMBER
    echo "Creating ${WORKDIR}/BUILD_NUMBER"
    awk -F= '/^buildId/ {print $2}' ${BUILD_ID} > ${WORKDIR}/BUILD_NUMBER
    
    cd ${WORKDIR}
    echo "Moving buildTest -> buildTest-buildID"
    mv buildTest buildTest-buildID
    echo ""
}

unsetBuildDbPath() {
    #Unset the paths
    echo `date` "Unsetting path for buildDB.py"
    export PATH=${TMP_PATH}
    export LD_LIBRARY_PATH=${TMP_LD_LIBRARY_PATH}
    export DYLD_LIBRARY_PATH=${TMP_DYLD_LIBRARY_PATH}
    echo ""
}

compileSource() {
    if [ "${TEST_ARCH}" = "SOL-x86_64" ] || [ "${TEST_ARCH}" = "Linux-x86_64" ] ; then
        echo `date` "Setting up path to build plperl and plpython"
        PYTHON_VER="2.5.1"
        PERL_VER="5.8.8"
        echo "PATH=/opt/perl-${PERL_VER}/bin:/opt/python-${PYTHON_VER}/bin:${PATH}"
        PATH=/opt/perl-${PERL_VER}/bin:/opt/python-${PYTHON_VER}/bin:${PATH}
        echo "LD_LIBRARY_PATH=/opt/perl-${PERL_VER}/lib:/opt/python-${PYTHON_VER}/lib:${LD_LIBRARY_PATH}"
        LD_LIBRARY_PATH=/opt/perl-${PERL_VER}/lib:/opt/python-${PYTHON_VER}/lib:${LD_LIBRARY_PATH}
        export PATH
        export LD_LIBRARY_PATH
        echo ""
    fi

    #kite12/linux 32bit needs this...
    export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

    echo `date` "Setting GPROOT"
    echo "export GPROOT=${WORKDIR}"
    export GPROOT=${WORKDIR}
    GPDB_INSTALL="${GPROOT}/greenplum-db-devel"
    echo ""

    rm -f build.out
    touch build.out
    
    if [ "${BUILD_TYPE}" = "gcov" ] ; then
        echo `date` "Creating .anno files for gcov usage"
        set -x
        P4CLIENT=buildbot_${HOST}_gpdb-gcov-${BRANCH}-${HOST}
        echo `date` "Using P4CLIENT: ${P4CLIENT}"
        export P4CLIENT=${P4CLIENT}
        cd /home/build/buildbot_slave_gpdb/gpdb-gcov-dbg-${BRANCH}-${HOST}
        time -p find . -name '*.c' | awk '{print "p4anno.pl "$1" >"$1".anno"}' | sh
        set +x
        echo `date` "Unsetting P4CLIENT"
        unset P4CLIENT
        cd ${WORKDIR}
        echo `date` "Done with .anno file creation."
        echo ""
    fi

    #AUTH libs
    AUTH_CMD=""
    if [ "${BLD_ARCH}" != "win32" ]; then
      if [ "${BRANCH}" = "main" ] || [ "${BRANCH}" = "Release-3_4-branch" ] || [ "${BRANCH}" = "Release-3_4_filerep-branch" ] || [ "${BRANCH}" = "Release-3_3-branch" ] || [ "${BRANCH}" = "Release-3_1_0-branch" ] || [ "${BRANCH}" = "Release-3_2_0_0_EAP1-branch" ] || [ "${BRANCH}" = "Release-3_2_0_0_EAP2-branch" ] || [ "${BRANCH}" = "Release-3_2-branch" ] || [ "${BRANCH}" = "Release-3_2_clients-branch" ] || [ "${BRANCH}" = "Release-3_2_0_6_clients-branch" ] ; then
        echo `date` "Making authlibs"
        echo "make BLD_ARCH=${BLD_ARCH} authlibs >> ${WORKDIR}/build.out 2>&1"
        make BLD_ARCH=${BLD_ARCH} authlibs >> ${WORKDIR}/build.out 2>&1
        if [ $? -ne 0 ] ; then
            echo "Error building authlibs"
            echo "------------------------------------------------"
            cat ${WORKDIR}/build.out
            echo "------------------------------------------------"
            buildTestUpdate
            exit 1
        fi
        echo "`date` -- INFO: Sourcing authlib_libpath_setup.sh"
        . authlib_libpath_setup.sh
        if [ $? -ne 0 ] ; then
            echo "Error sourcing path setup for auth libs"
            exit 1
        fi
        AUTH_CMD="AUTH=true"
        echo `date` "Done making authlibs"
        echo ""
      fi
    fi

    if [ "${BUILD_TYPE}" = "debug" ] ; then
        MAKE_OPTS="DEVPATH=${GPDB_INSTALL} ${AUTH_CMD}"
    fi
    if [ "${BUILD_TYPE}" = "optimized" ] ; then
        MAKE_OPTS="dist ${AUTH_CMD}"
    fi
    if [ "${BRANCH}" = "Release-3_1_0-branch" ] || [ "${BRANCH}" = "Release-3_2_0_0_EAP1-branch" ] || [ "${BRANCH}" = "Release-3_2_0_0_EAP2-branch" ] || [ "${BRANCH}" = "Release-3_2-branch" ] || [ "${BRANCH}" = "Release-3_2_clients-branch" ] || [ "${BRANCH}" = "Release-3_2_0_6_clients-branch" ] ; then
        MAKE_OPTS+= PG_LANG=true
    fi

    echo ""
    echo `date` "PATH setup: $PATH"
    echo `date` "LD_LIBRARY_PATH setup: $LD_LIBRARY_PATH"
    echo `date` "Using gcc: `which gcc`"
    echo ""
 
    export GPROOT=${WORKDIR}
    if [ "${BUILD_TYPE}" = "gcov" ] ; then
        echo `date` "Running: ${TIME_CMD} make BLD_ARCH=${BLD_ARCH} DEVPATH=${GPDB_INSTALL} PG_LANG=true ${AUTH_CMD} CUSTOM_COPT='-fprofile-arcs -ftest-coverage' > build.out 2>&1"
        ${TIME_CMD} make BLD_ARCH=${BLD_ARCH} DEVPATH=${GPDB_INSTALL} PG_LANG=true ${AUTH_CMD} CUSTOM_COPT='-fprofile-arcs -ftest-coverage' > build.out 2>&1
    else
        echo `date` "Running: ${TIME_CMD} make BLD_ARCH=${BLD_ARCH} BLD_TARGETS="clients connectivity loaders pwx" ${MAKE_OPTS} >> ${WORKDIR}/build.out 2>&1"
        ${TIME_CMD} make BLD_ARCH=${BLD_ARCH} BLD_TARGETS="clients connectivity loaders pwx" ${MAKE_OPTS} >> ${WORKDIR}/build.out 2>&1
    fi 
    result_of_make=$?

# TODO: This is an incorrect representation of build_duration as it only covers the primary make step
# and not the authlibs, pygres, nor regress targets nor the overhead of this script.  Switch to the
# time for the entire build step, instead.
    #The last 3 lines of build.out are from ${TIME_CMD}
    BUILD_DURATION=`${TAIL_CMD} -n 3 ${WORKDIR}/build.out | awk '/real/ {print $2}' | awk -F. '{print $1}'`
    if [ $? -ne 0 ] ; then
        echo "Error in getting build time from build.out"
        exit 1
    fi

    if [ $result_of_make != 0 ]; then
        cat ${WORKDIR}/build.out
        echo ""
        echo `date` "Error during compilation"
        buildTestUpdate
        echo "Exiting"
        exit 1
    else
        echo `date` "make finished."
    fi 
    echo ""

    # backing out build of pygress on "clients" branches as it breaks on Solaris and is not required
    #if [ ${BRANCH} = "main" ] || [ "${BRANCH}" = "Release-3_4-branch" ] || [ "${BRANCH}" = "Release-3_4_filerep-branch" ] || [ "${BRANCH}" = "Release-3_3-branch" ] || [ ${BRANCH} = "Release-3_1_0-branch" ] || [ "${BRANCH}" = "Release-3_2_0_0_EAP1-branch" ] || [ "${BRANCH}" = "Release-3_2_0_0_EAP2-branch" ] || [ "${BRANCH}" = "Release-3_2-branch" ] || [ "${BRANCH}" = "Release-3_2_clients-branch" ] || [ "${BRANCH}" = "Release-3_2_0_6_clients-branch" ] ; then
    if [ ${BRANCH} = "main" ] || [ "${BRANCH}" = "Release-3_4-branch" ] || [ "${BRANCH}" = "Release-3_4_filerep-branch" ] || [ "${BRANCH}" = "Release-3_3-branch" ] || [ ${BRANCH} = "Release-3_1_0-branch" ] || [ "${BRANCH}" = "Release-3_2_0_0_EAP1-branch" ] || [ "${BRANCH}" = "Release-3_2_0_0_EAP2-branch" ] || [ "${BRANCH}" = "Release-3_2-branch" ] ; then
        echo `date` "Building pygres"
        echo "PATH=${GPROOT}/greenplum-db-devel/bin:${PATH} && make BLD_ARCH=${BLD_ARCH} pygres >> ${WORKDIR}/build.out 2>&1"
        PATH=${GPROOT}/greenplum-db-devel/bin:${PATH} && make BLD_ARCH=${BLD_ARCH} pygres >> ${WORKDIR}/build.out 2>&1

        if [ $? != 0 ]; then
            cat ${WORKDIR}/build.out
            echo ""
            echo `date` "Error during pygres compilation"
            buildTestUpdate
            echo "Exiting"
            exit 1
        fi
        echo ""
    fi

    echo "Compile finished at" `date`
    echo ""
}

compilePGTest() {
    if [ "${BLD_ARCH}" = "win32" ]; then return; fi
    echo "Starting compile of libregress at" `date`
    echo "if [ -f env.sh ]; then . ./env.sh ]; fi"
    if [ -f env.sh ]; then . ./env.sh ]; fi
    echo "Entering cdb-pg/src/test/regress"
    pushd cdb-pg/src/test/regress > /dev/null 2>&1
    echo "make BLD_ARCH=${BLD_ARCH} >> ${WORKDIR}/build.out 2>&1"
    make BLD_ARCH=${BLD_ARCH} >> ${WORKDIR}/build.out 2>&1
    if [ $? -ne 0 ] ; then
        echo "Make error."
        echo "------------------------------------------------"
        cat ${WORKDIR}/build.out
        echo "------------------------------------------------"
        buildTestUpdate
        echo "Exiting"
        exit 1
    fi

    mkdir -p ${GPDB_INSTALL}/test
    if  [ ${PLAT} = "SunOS" ] || [ ${PLAT} = "Linux" ] ; then
        LIBREGRES="libregress.so.0.0"
    else
        LIBREGRES="libregress.0.0.so"
    fi
    if [ ! -f ${LIBREGRES} ]; then
        LIBREGRES="regress.so"
    fi
    cp -p ${LIBREGRES} ${GPDB_INSTALL}/test/regress.so
    if [ $? -ne 0 ] ; then 
        echo "Error copying ${LIBREGRES} -> ${GPDB_INSTALL}/test/regress.so"
        buildTestUpdate
        echo "Exiting"
        exit 1
    fi
    popd > /dev/null 2>&1
    echo "${LIBREGRES} -> ${GPDB_INSTALL}/test/regress.so"
    echo "Finished compile of libregress at" `date`
    echo ""
}

buildTestUpdate() {
    #Update the build/test db with buildDB.py
    #This will only need to be done if there was a compile/build issue,
    #otherwise it is done in post-build-gpdb.sh... duplicated code for now.

    generateBuildInfo

    WORKDIR=`pwd`
    BUILD_INFO="${WORKDIR}/buildInfo.txt"

    #Populate buildInfo.txt with the location of this build
    if [ -f "${BUILD_INFO}" ] ; then
        echo "location=" >> ${BUILD_INFO}
        echo "failure_msg=make error" >> ${BUILD_INFO}
    else
        echo "${BUILD_INFO} not found, skipping update."
    fi

    setBuildTestPath

    echo `date` "Starting buildTest update process"
    cd ${WORKDIR}

    echo "mkdir buildTest"
    mkdir buildTest

    echo "cd buildTest"
    cd buildTest

    echo "checkout_source_set buildTest main buildTest-sync.log `date +%Y%m%d%H%M` ${BUILD_TYPE}"
    checkout_source_set buildTest main buildTest-sync.log `date +%Y%m%d%H%M` ${BUILD_TYPE}
    if [ $? -ne 0 ] ; then
        echo "Error checking out buildTest"
        exit 1
    fi

    echo "cd buildDB"
    cd buildDB

    ## testing
    exit 1

    BUILD_DB_OPTS="-p 5432 -h build-prod -d cdbunit -u postgres"

    echo "./buildDB.py ${BUILD_DB_OPTS} build ${BUILD_INFO}"
    ./buildDB.py ${BUILD_DB_OPTS} build ${BUILD_INFO}
    if [ $? -ne 0 ] ; then
        echo "Error updating build DB with failed build info"
        exit 1
    fi

    unsetBuildTestPath

    echo `date` "Finished buildTest update process"
    echo ""
}

setBuildTestPath() {
    #The python in ~/build_tools/python-2.5 has pygresql
    BUILD_TOOLS=${HOME}/build_tools
    TMP_PATH=${PATH}
    export PATH=${BUILD_TOOLS}/python-2.5/bin:${PATH}
    TMP_LD_LIBRARY_PATH=${LD_LIBRARY_PATH}
    export LD_LIBRARY_PATH=${BUILD_TOOLS}/pg-8.2.4/lib:${LD_LIBRARY_PATH}
    TMP_DYLD_LIBRARY_PATH=${DYLD_LIBRARY_PATH}
    export DYLD_LIBRARY_PATH=${BUILD_TOOLS}/pg-8.2.4/lib:${DYLD_LIBRARY_PATH}
}
unsetBuildTestPath() {
    #Unset what is set by setBuildTestPath
    export PATH=${TMP_PATH}
    export LD_LIBRARY_PATH=${TMP_LD_LIBRARY_PATH}
    export DYLD_LIBRARY_PATH=${TMP_DYLD_LIBRARY_PATH}
}



#Main
echo `date` "Starting a ${PRODUCT} build on ${MPP_ARCH}"
echo ""
setBuildDbPath
getBuildId
unsetBuildDbPath
compileSource
compilePGTest
generateBuildInfo
echo ""
echo `date` "Done with build"
exit 0
