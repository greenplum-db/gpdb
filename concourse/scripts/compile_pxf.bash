#!/bin/bash -l
set -exo pipefail

export TERM=xterm
export JAVA_TOOL_OPTIONS=-Dfile.encoding=UTF8

_main() {
  pushd pxf_src/pxf
  export BUILD_NUMBER="${TARGET_OS}"
  make bundle
  mv distributions/*.tar.gz ../../pxf_tarball
  popd
}

_main $@
