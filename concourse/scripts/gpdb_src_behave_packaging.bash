#!/bin/bash -l

set -euxo pipefail

function echo_expected_env_variables() {
  echo "Source code tarball file to be created: ${GPDB_SRC_TAR_GZ}"
}

function _main() {
  echo_expected_env_variables
  cd gpdb_src

  tar -czf "../${GPDB_SRC_TAR_GZ}" gpMgmt/test gpMgmt/bin/pythonSrc gpMgmt/bin/Makefile gpMgmt/Makefile.behave
}

_main
