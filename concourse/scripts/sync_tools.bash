#!/bin/bash -l
set -exo pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
GPDB_SRC_PATH=${GPDB_SRC_PATH:=gpdb_src}


function make_sync_tools() {
  pushd ${GPDB_SRC_PATH}/gpAux
    # Requires these variables in the env:
    # IVYREPO_HOST IVYREPO_REALM IVYREPO_USER IVYREPO_PASSWD
    make sync_tools
  popd

  wget -q -O - https://github.com/greenplum-db/gporca/archive/v3.48.0.tar.gz | tar zxf - -C ${GPDB_SRC_PATH}/gpAux/ext/${BLD_ARCH}
  mkdir -p orca_src
  mv ${GPDB_SRC_PATH}/gpAux/ext/${BLD_ARCH}/gporca*/* orca_src/

}

function _main() {
  case "${TARGET_OS}" in
    centos)
        case "${TARGET_OS_VERSION}" in
         6)
           BLD_ARCH=rhel6_x86_64
           ;;

         7)
           BLD_ARCH=rhel7_x86_64 
           ;;
        esac
        ;;
    ubuntu)
        export BLD_ARCH=ubuntu18.04_x86_64
        ;;
    *)
        echo "only centos, ubuntu are supported TARGET_OS'es"
        false
        ;;
  esac

  # We have moved out of ivy for Centos{6,7} and Ubuntu never had ivy
  # so make sync_tools will not create the necessary directory for centos or ubuntu.
  case "${TARGET_OS}" in
    centos|ubuntu)
        mkdir -p ${GPDB_SRC_PATH}/gpAux/ext/${BLD_ARCH}
        ;;
  esac
  make_sync_tools

  # Move ext directory to output dir.
  mv ${GPDB_SRC_PATH}/gpAux/ext gpAux_ext/

}

_main "$@"
