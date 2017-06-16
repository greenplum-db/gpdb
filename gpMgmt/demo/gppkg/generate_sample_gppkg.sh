#!/bin/bash
set -ex

DIR="$1"
if [ -z ${DIR} ]; then
    echo "usage: $0 <path to source directory>"
    exit 1
fi

DESTINATION_GPPKG_PATH=/tmp/sample-gppkg
rm -rf ${DESTINATION_GPPKG_PATH}
mkdir -p ${DESTINATION_GPPKG_PATH}/deps

BUILDROOT=/tmp/rpmbuild
rm -rf ${BUILDROOT}
mkdir -p ${BUILDROOT}/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

# We assume that these rpms have already been installed
#yum -y install rpmdevtools rpmlint

rpmdev-setuptree
cp ${DIR}/sample-sources.tar.gz ${BUILDROOT}/SOURCES/
cp ${DIR}/sample.spec ${BUILDROOT}/SPECS/

# --define "%_topdir ${BUILDROOT}" tells where to find SOURCES
rpmbuild -bb ${BUILDROOT}/SPECS/sample.spec --define "%_topdir ${BUILDROOT}" --define "debug_package %{nil}"
cp ${BUILDROOT}/RPMS/x86_64/*.rpm ${DESTINATION_GPPKG_PATH}
cp ${DIR}/gppkg_spec.yml ${DESTINATION_GPPKG_PATH}
cd ${DESTINATION_GPPKG_PATH}
tar czf sample.gppkg *
