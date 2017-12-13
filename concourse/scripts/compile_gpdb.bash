#!/bin/bash -l
set -exo pipefail

GREENPLUM_INSTALL_DIR=/usr/local/greenplum-db-devel
export GPDB_ARTIFACTS_DIR
GPDB_ARTIFACTS_DIR=$(pwd)/$OUTPUT_ARTIFACT_DIR

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

GPDB_SRC_PATH=${GPDB_SRC_PATH:=gpdb_src}
GPDB_BIN_FILENAME=${GPDB_BIN_FILENAME:="bin_gpdb.tar.gz"}

function expand_glob_ensure_exists() {
  local -a glob=($*)
  [ -e "${glob[0]}" ]
  echo "${glob[0]}"
}

function prep_env_for_centos() {
  case "$TARGET_OS_VERSION" in
    5)
      BLDARCH=rhel5_x86_64
      export JAVA_HOME=/usr/lib/jvm/java-1.6.0-openjdk-1.6.0.39.x86_64
      source /opt/gcc_env.sh
      ;;

    6)
      BLDARCH=rhel6_x86_64
      export JAVA_HOME=/usr/lib/jvm/java-1.7.0-openjdk.x86_64
      source /opt/gcc_env.sh
      ;;

    7)
      BLDARCH=rhel7_x86_64
      echo "Detecting java7 path ..."
      java7_packages=$(rpm -qa | grep -F java-1.7)
      java7_bin="$(rpm -ql $java7_packages | grep /jre/bin/java$)"
      alternatives --set java "$java7_bin"
      export JAVA_HOME="${java7_bin/jre\/bin\/java/}"
      ln -sf /usr/bin/xsubpp /usr/share/perl5/ExtUtils/xsubpp
      source /opt/gcc_env.sh
      ;;

    *)
    echo "TARGET_OS_VERSION not set or recognized for Centos/RHEL"
    exit 1
    ;;
  esac

  ln -sf $(pwd)/$GPDB_SRC_PATH/gpAux/ext/${BLDARCH}/python-2.7.12 /opt/python-2.7.12
  export PATH=${JAVA_HOME}/bin:${PATH}
}

function prep_env_for_sles() {
  export JAVA_HOME=$(expand_glob_ensure_exists /usr/java/jdk1.7*)
  export PATH=${JAVA_HOME}/bin:${PATH}
  source /opt/gcc_env.sh
}

function generate_build_number() {
  pushd $GPDB_SRC_PATH
    #Only if its git repro, add commit SHA as build number
    # BUILD_NUMBER file is used by getversion file in GPDB to append to version
    if [ -d .git ] ; then
      echo "commit:`git rev-parse HEAD`" > BUILD_NUMBER
    fi
  popd
}

function make_sync_tools() {
  pushd $GPDB_SRC_PATH/gpAux
    # Requires these variables in the env:
    # IVYREPO_HOST IVYREPO_REALM IVYREPO_USER IVYREPO_PASSWD
    make sync_tools
  popd
}

function link_tools_for_sles() {
  ln -sf "$(expand_glob_ensure_exists "$(pwd)"/${GPDB_SRC_PATH}/gpAux/ext/*/python-2.7.12 )" /opt
}

function build_gpdb() {
  pushd $GPDB_SRC_PATH/gpAux
    # Use -j4 to speed up the build. (Doesn't seem worth trying to guess a better
    # value based on number of CPUs or anything like that. Going above -j4 wouldn't
    # make it much faster, and -j4 is small enough to not hurt too badly even on
    # a single-CPU system
    if [ -n "$1" ]; then
      make "$1" GPROOT=/usr/local PARALLEL_MAKE_OPTS=-j4 -s dist
    else
      make GPROOT=/usr/local PARALLEL_MAKE_OPTS=-j4 -s dist
    fi
  popd
}

function build_gppkg() {
  pushd $GPDB_SRC_PATH/gpAux
    make gppkg BLD_TARGETS="gppkg" INSTLOC="$GREENPLUM_INSTALL_DIR" GPPKGINSTLOC="$GPDB_ARTIFACTS_DIR" RELENGTOOLS=/opt/releng/tools
  popd
}

function unittest_check_gpdb() {
  pushd $GPDB_SRC_PATH
    source $GREENPLUM_INSTALL_DIR/greenplum_path.sh
    make GPROOT=/usr/local -s unittest-check
  popd
}

function build_pxf() {
  pushd pxf_src/pxf
  export TERM=xterm
  export BUILD_NUMBER="${TARGET_OS}"
  export PXF_HOME="${GREENPLUM_INSTALL_DIR}/pxf"
  export JAVA_TOOL_OPTIONS=-Dfile.encoding=UTF8
  make install -s DATABASE=gpdb
  popd
}

function export_gpdb() {
  TARBALL="$GPDB_ARTIFACTS_DIR"/$GPDB_BIN_FILENAME
  pushd $GREENPLUM_INSTALL_DIR
    source greenplum_path.sh
    python -m compileall -x test .
    chmod -R 755 .
    tar -czf "${TARBALL}" ./*
  popd
}

function export_gpdb_extensions() {
  pushd $GPDB_SRC_PATH/gpAux
    if ls greenplum-*zip* 1>/dev/null 2>&1; then
      chmod 755 greenplum-*zip*
      cp greenplum-*zip* "$GPDB_ARTIFACTS_DIR"/
    fi
    if ls "$GPDB_ARTIFACTS_DIR"/*.gppkg 1>/dev/null 2>&1; then
      chmod 755 "$GPDB_ARTIFACTS_DIR"/*.gppkg
    fi
  popd
}

function export_gpdb_win32_ccl() {
    pushd $GPDB_SRC_PATH/gpAux
    if [ -f "$(find . -maxdepth 1 -name 'greenplum-*.msi' -print -quit)" ] ; then
        cp greenplum-*.msi "$GPDB_ARTIFACTS_DIR"/
    fi
    popd
}

function _main() {
  case "$TARGET_OS" in
   centos)
      prep_env_for_centos
      ;;
    sles)
      prep_env_for_sles
      ;;
    win32)
        export BLD_ARCH=win32
        CONFIGURE_FLAGS="${CONFIGURE_FLAGS} --disable-pxf"
        ;;
    *)
        echo "only centos, sles and win32 are supported TARGET_OS'es"
        false
        ;;
  esac

  generate_build_number
  make_sync_tools
  case "$TARGET_OS" in
    sles) link_tools_for_sles ;;
  esac

  # By default, only GPDB Server binary is build.
  # Use BLD_TARGETS flag with appropriate value string to generate client, loaders
  # connectors binaries
  if [ -n "$BLD_TARGETS" ]; then
    BLD_TARGET_OPTION=("BLD_TARGETS=\"$BLD_TARGETS\"")
  else
    BLD_TARGET_OPTION=("")
  fi

  build_gpdb "${BLD_TARGET_OPTION[@]}"
  build_gppkg
  if [ "$TARGET_OS" != "win32" ] ; then
      # Don't unit test when cross compiling. Tests don't build because they
      # require `./configure --with-zlib`.
      unittest_check_gpdb
  fi
  if [ "$TARGET_OS" == "centos" ]; then
      # Build pxf(server) only for centos
      build_pxf
  fi
  export_gpdb
  export_gpdb_extensions
  export_gpdb_win32_ccl
}

_main "$@"
