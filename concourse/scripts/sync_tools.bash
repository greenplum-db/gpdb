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
         5)
           BLD_ARCH=rhel5_x86_64
           ;;

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

  case "${TASK_OS}" in
    centos)
        case "${TASK_OS_VERSION}" in
         5)
           export JAVA_HOME=/usr/lib/jvm/java-1.6.0-openjdk-1.6.0.39.x86_64
           ;;

         6)
           export JAVA_HOME=/usr/lib/jvm/java-1.7.0-openjdk.x86_64
           ;;

         7)
           echo "Detecting java7 path ..."
           java7_packages=$(rpm -qa | grep -F java-1.7)
           java7_bin="$(rpm -ql ${java7_packages} | grep /jre/bin/java$)"
           alternatives --set java "$java7_bin"
           export JAVA_HOME="${java7_bin/jre\/bin\/java/}"
           ln -sf /usr/bin/xsubpp /usr/share/perl5/ExtUtils/xsubpp
           ;;

        esac
        ;;
    *)
        echo "only centos 6 and 7 are supported TASK_OS'es"
        false
        ;;
  esac

  make_sync_tools

  # Move ext directory to output dir
  mv ${GPDB_SRC_PATH}/gpAux/ext gpAux_ext/

}

_main "$@"
