#!/bin/bash -l
set -exo pipefail

GREENPLUM_INSTALL_DIR=/usr/local/greenplum-db-devel
PXF_ARTIFACTS_DIR=$(pwd)/${OUTPUT_ARTIFACT_DIR}

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

function _main() {
  if [ "${TARGET_OS}" == "centos" ]; then
      # Build pxf(server) only for centos
      export TERM=xterm
      export JAVA_HOME=/usr/lib/jvm/java-1.7.0-openjdk.x86_64
      export PATH=${JAVA_HOME}/bin:${PATH}
      export BUILD_NUMBER="${TARGET_OS}"
      export PXF_HOME="${GREENPLUM_INSTALL_DIR}/pxf"
      export JAVA_TOOL_OPTIONS=-Dfile.encoding=UTF8
      if [ -d pxf_gradle_dependencies ]; then
          mkdir -p ~/.gradle
          tar -xzf pxf_gradle_dependencies/pxf_gradle_dependencies.tar.gz -C ~/.gradle
      fi
      pushd pxf_src/pxf
          make install -s DATABASE=gpdb
      popd
      pushd ${GREENPLUM_INSTALL_DIR}
          chmod -R 755 pxf
          tar -czf ${PXF_ARTIFACTS_DIR}/pxf.tar.gz pxf
      popd
  fi
}

_main "$@"
