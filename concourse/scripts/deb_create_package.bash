#!/bin/bash
set -euxo pipefail

SRC_DIR=gpdb_src

# the image should already have all the necessary apt-get packages for building .deb's

# cannot use 'mv' because of concourse inputs
# mv: inter-device move failed: 'debian_release/debian' to 'gpdb_src/debian'; unable to remove target: Directory not empty
cp -R debian_release/debian ${SRC_DIR}/

# Regex to capture required gporca version and download gporca source
ORCA_TAG=$(grep -Po 'v\d+.\d+.\d+' ${SRC_DIR}/depends/conanfile_orca.txt)
git clone --branch ${ORCA_TAG} https://github.com/greenplum-db/gporca.git ${SRC_DIR}/gporca

pushd ${SRC_DIR}
    VERSION=`./getversion | tr " " "."`-oss
    SHA=`git rev-parse --short HEAD`
    MESSAGE="Bumping to Greenplum version $VERSION, git SHA $SHA"
    PACKAGE=`cat debian/control | egrep "^Package: " | cut -d " " -f 2`

    dch --create -M --package $PACKAGE -v $VERSION "$MESSAGE"
popd

# the image should already have all the debian packages
# yes | mk-build-deps -i ${SRC_DIR}/debian/control

pushd ${SRC_DIR}
    debuild -us -uc -b
popd
cp greenplum-db*.deb deb_package_ubuntu16/greenplum-db.deb
