#!/bin/bash -l
set -exo pipefail

GREENPLUM_INSTALL_DIR=/usr/local/gpdb
TRANSFER_DIR_ABSOLUTE_PATH=$(pwd)/${TRANSFER_DIR}
COMPILED_BITS_FILENAME=${COMPILED_BITS_FILENAME:="compiled_bits_ubuntu16.tar.gz"}

function build_external_depends() {
    pushd gpdb_src/depends
        ./configure
        make
    popd
}

function install_external_depends() {
    pushd gpdb_src/depends
        make install
    popd
}

function build_gpdb() {
    build_external_depends
    pushd gpdb_src
        CWD=$(pwd)
        LD_LIBRARY_PATH=${CWD}/depends/build/lib CC=$(which gcc) CXX=$(which g++) ./configure \
            --enable-mapreduce --with-gssapi --with-perl --with-libxml \
            --enable-pxf \
            --with-python \
            --with-libraries=${CWD}/depends/build/lib \
            --with-includes=${CWD}/depends/build/include \
            --prefix=${GREENPLUM_INSTALL_DIR}
        make -j4
        LD_LIBRARY_PATH=${CWD}/depends/build/lib make install
    popd
    install_external_depends
}

function build_pxf() {
  pushd pxf_src/pxf
      export TERM=xterm
      export BUILD_NUMBER="${TARGET_OS}"
      export PXF_HOME="${GREENPLUM_INSTALL_DIR}/pxf"
      export JAVA_TOOL_OPTIONS=-Dfile.encoding=UTF8
      export JAVA_HOME=$(readlink -f /usr/bin/java | sed "s:/jre/bin/java::")
      export SHELL=/bin/bash
      make install DATABASE=gpdb
  popd
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
    build_pxf
    unittest_check_gpdb
    export_gpdb
}

_main "$@"
