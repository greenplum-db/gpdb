#!/bin/bash
set -exo pipefail

GPDB_SRC_PATH=${GPDB_SRC_PATH:=gpdb_src}

function build_xerces
{
    OUTPUT_DIR="gpdb_src/gpAux/ext/${BLD_ARCH}"
    mkdir -p xerces_patch/concourse
    cp -r gpdb_src/src/backend/gporca/concourse/xerces-c xerces_patch/concourse
    /usr/bin/python3 xerces_patch/concourse/xerces-c/build_xerces.py --output_dir=${OUTPUT_DIR}
    rm -rf build
}

function test_orca
{
    if [ -n "${SKIP_UNITTESTS}" ]; then
        return
    fi
    OUTPUT_DIR="../../../../gpAux/ext/${BLD_ARCH}"
    pushd ${GPDB_SRC_PATH}/src/backend/gporca
    concourse/build_and_test.py --build_type=RelWithDebInfo --output_dir=${OUTPUT_DIR}
    concourse/build_and_test.py --build_type=Debug --output_dir=${OUTPUT_DIR}
    popd
}

function _main
{
  mkdir gpdb_src/gpAux/ext
  build_xerces
  test_orca
}

_main "$@"
