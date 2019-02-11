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
    sles)
        export BLD_ARCH=sles11_x86_64
        ;;
    win32)
        export BLD_ARCH=win32
        ;;
    *)
        echo "only centos, sles and win32 are supported TARGET_OS'es"
        false
        ;;
  esac

  # We have moved out of ivy for Centos{6,7}, so make sync_tools
  # will not create the necessary directory for centos.
  if [ "${TARGET_OS}" = centos ]; then
    mkdir -p ${GPDB_SRC_PATH}/gpAux/ext/${BLD_ARCH}
  fi
  make_sync_tools

  # Move ext directory to output dir.
  mv ${GPDB_SRC_PATH}/gpAux/ext gpAux_ext/

}

_main "$@"
