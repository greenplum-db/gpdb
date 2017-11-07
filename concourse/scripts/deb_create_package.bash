#!/bin/bash
set -euxo pipefail

SRC_DIR=gpdb_src

apt-get update
apt-get install -y vim \
                   debmake \
                   equivs \
                   git

pushd ${SRC_DIR}
    VERSION=`./getversion | tr " " "."`-oss
    SHA=`git rev-parse --short HEAD`
    MESSAGE="Bumping to Greenplum version $VERSION, git SHA $SHA"
    PACKAGE=`cat debian/control | egrep "^Package: " | cut -d " " -f 2`

    dch --create -M --package $PACKAGE -v $VERSION "$MESSAGE"
popd

set +e
    # processing triggers can result in a non-zero result code
    yes | mk-build-deps -i ${SRC_DIR}/debian/control
set -e

pushd ${SRC_DIR}
    dpkg-buildpackage -us -uc -b
    # print contents just for human-readable feedback; not necessary
    dpkg --contents ../greenplum-db*.deb
popd
cp greenplum-db*.deb deb_package_ubuntu16/greenplum-db.deb
