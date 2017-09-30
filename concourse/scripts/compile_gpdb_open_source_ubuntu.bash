#!/bin/bash -l
set -exo pipefail

GREENPLUM_INSTALL_DIR=/usr/local/gpdb
TRANSFER_DIR_ABSOLUTE_PATH="$(pwd)/${TRANSFER_DIR}"
COMPILED_BITS_FILENAME=${COMPILED_BITS_FILENAME:="compiled_bits_ubuntu16.tar.gz"}

function build_gpdb() {
  pushd gpdb_src
    CC=$(which gcc) CXX=$(which g++) ./configure --enable-mapreduce --with-gssapi --with-perl --with-libxml \
	--disable-orca --with-python --prefix=${GREENPLUM_INSTALL_DIR}
    make -j4
    make install
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
    tar -czf "${TARBALL}" ./*
  popd
}

function _main() {
  build_gpdb
  unittest_check_gpdb
  export_gpdb
}

_main "$@"
