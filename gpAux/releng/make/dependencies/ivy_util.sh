#!/bin/bash
## ======================================================================
## Post download untar trigger
## ======================================================================

FILE=$1
REVISION=$2

ARTIFACT_DIR=$( dirname ${FILE} )

##
## Expand tarball
##

if [ ! -f "${FILE}" ]; then
    echo "WARNING: tarball does not exist (${FILE})"
    exit 2
else
    pushd ${ARTIFACT_DIR}/.. > /dev/null
    gunzip -qc ${FILE} | tar xf -
    popd > /dev/null

    if [ $? != 0 ]; then
        echo "FATAL: Problem exapanding tarball (${FILE})"
        exit 1
    fi
fi

## ----------------------------------------------------------------------
## Devel package processing
##   Restore object files into loation used for build
## ----------------------------------------------------------------------

echo "${FILE}" | grep devel
if [ $? = 0 ]; then
    BASE_PULSE_PROJECT=`tar tvf ${FILE} | head -1 | sed -e 's|\(.* \./\)\(.*\)|\2|' | cut -d '/' -f1`
    BASE_BUILD_STAGE=`tar tvf ${FILE}  | head -1 | sed -e 's|\(.* \./\)\(.*\)|\2|' | cut -d '/' -f2`
    rm -rf /opt/releng/build/${BASE_PULSE_PROJECT}/${BASE_BUILD_STAGE}
    cd ..
    [ ! -d /opt/releng/build/${BASE_PULSE_PROJECT} ] && mkdir -p /opt/releng/build/${BASE_PULSE_PROJECT}
    ln -s ${ARTIFACT_DIR}/../${BASE_PULSE_PROJECT}/${BASE_BUILD_STAGE} /opt/releng/build/${BASE_PULSE_PROJECT}/${BASE_BUILD_STAGE}
fi

exit 0
