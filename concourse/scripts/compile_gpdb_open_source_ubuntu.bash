#!/bin/bash -l
set -exo pipefail

GREENPLUM_INSTALL_DIR=/usr/local/gpdb

# This is a canonical way to build GPDB. The intent is to validate that GPDB compiles
# with a fairly basic build. It is not meant to be exhaustive or include all features
# and components available in GPDB.

function build_gpdb() {
  pushd gpdb_src
    CC=$(which gcc) CXX=$(which g++) ./configure --enable-mapreduce --with-perl --with-libxml \
	--disable-orca --with-python --disable-gpfdist --prefix=${GREENPLUM_INSTALL_DIR}
    # Use -j4 to speed up the build. (Doesn't seem worth trying to guess a better
    # value based on number of CPUs or anything like that. Going above -j4 wouldn't
    # make it much faster, and -j4 is small enough to not hurt too badly even on
    # a single-CPU system
    make -j4
    make install
  popd
}

function unittest_check_gpdb() {
  make -C gpdb_src/src/backend -s unittest-check
}

function _main() {
  build_gpdb
  unittest_check_gpdb
}

_main "$@"
