#!/bin/bash -l
set -exo pipefail

GREENPLUM_INSTALL_DIR=/usr/local/gpdb
TRANSFER_DIR_ABSOLUTE_PATH=$(pwd)/${TRANSFER_DIR}
COMPILED_BITS_FILENAME=${COMPILED_BITS_FILENAME:="compiled_bits_ubuntu16.tar.gz"}
ORCA_VERSION=3.116.0

function build_external_depends() {
    # fix the /root/.ccache missing issue during build
    mkdir -p /root/.ccache && touch  /root/.ccache/ccache.conf

    mkdir gpdb_src/temp_build_orca
    pushd gpdb_src/temp_build_orca
        wget https://github.com/greenplum-db/gporca/archive/refs/tags/v${ORCA_VERSION}.tar.gz
        tar -xzf v${ORCA_VERSION}.tar.gz
        cd gporca-${ORCA_VERSION}
        cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=../. -H. -Brel
        ninja install -C rel
    popd
}

function install_external_depends() {
    pushd gpdb_src/temp_build_orca
        cp -R lib/lib* ${GREENPLUM_INSTALL_DIR}/lib/
    popd
}

function build_gpdb() {
    build_external_depends
    pushd gpdb_src
        CWD=$(pwd)
        LD_LIBRARY_PATH=${CWD}/temp_build_orca/lib CC=$(which gcc) CXX=$(which g++) ./configure --enable-mapreduce --with-gssapi --with-perl --with-libxml \
          --with-python \
          --with-libraries=${CWD}/temp_build_orca/lib \
          --with-includes=${CWD}/temp_build_orca/include \
          --prefix=${GREENPLUM_INSTALL_DIR} \
          ${CONFIGURE_FLAGS}
        make -j4
        LD_LIBRARY_PATH=${CWD}/depends/build/lib make install
    popd
    install_external_depends
}

function unittest_check_gpdb() {
  make -C gpdb_src/src/backend -s unittest-check
}

function export_gpdb() {
  TARBALL="$TRANSFER_DIR_ABSOLUTE_PATH"/$COMPILED_BITS_FILENAME
  pushd $GREENPLUM_INSTALL_DIR
    source greenplum_path.sh
    python -m compileall -x test .
    chmod -R 755 .
    tar -czf ${TARBALL} ./*
  popd
}

function _main() {
    build_gpdb
    unittest_check_gpdb
    export_gpdb
}

_main "$@"
