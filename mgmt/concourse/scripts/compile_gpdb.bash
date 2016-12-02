#!/bin/bash -l
set -exo pipefail

GREENPLUM_INSTALL_DIR=/usr/local/greenplum-db-devel
export GPDB_ARTIFACTS_DIR
GPDB_ARTIFACTS_DIR=$(pwd)/$OUTPUT_ARTIFACT_DIR

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

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
      # This is necessesary to build gphdfs.
      # It should be removed once the build image has this included.
      yum install -y ant-junit
      ;;

    7)
      BLDARCH=rhel7_x86_64
      alternatives --set java /usr/lib/jvm/java-1.7.0-openjdk-1.7.0.111-2.6.7.2.el7_2.x86_64/jre/bin/java
      export JAVA_HOME=/usr/lib/jvm/java-1.7.0-openjdk-1.7.0.111-2.6.7.2.el7_2.x86_64
      ln -sf /usr/bin/xsubpp /usr/share/perl5/ExtUtils/xsubpp
      source /opt/gcc_env.sh
      yum install -y ant-junit
      ;;

    *)
    echo "TARGET_OS_VERSION not set or recognized for Centos/RHEL"
    exit 1
    ;;
  esac

  ln -sf /$(pwd)/gpdb_src/gpAux/ext/${BLDARCH}/python-2.6.2 /opt/python-2.6.2
  export PATH=${JAVA_HOME}/bin:${PATH}
}

function prep_env_for_sles() {
  ln -sf "$(pwd)/gpdb_src/gpAux/ext/sles11_x86_64/python-2.6.2" /opt
  export JAVA_HOME=/usr/lib64/jvm/java-1.6.0-openjdk-1.6.0
  export PATH=${JAVA_HOME}/bin:${PATH}
  source /opt/gcc_env.sh
}

function generate_build_number() {
  pushd gpdb_src
    #Only if its git repro, add commit SHA as build number
    # BUILD_NUMBER file is used by getversion file in GPDB to append to version
    if [ -d .git ] ; then
      echo "commit:`git rev-parse HEAD`" > BUILD_NUMBER
    fi
  popd
}

function make_sync_tools() {
  pushd gpdb_src/gpAux
    make IVYREPO_HOST=${IVYREPO_HOST} IVYREPO_REALM="${IVYREPO_REALM}" IVYREPO_USER=${IVYREPO_USER} IVYREPO_PASSWD=${IVYREPO_PASSWD} sync_tools
    # We have compiled LLVM with native zlib on CentOS6 and not from
    # the zlib downloaded from artifacts.  Therefore, remove the zlib
    # downloaded from artifacts in order to use the native zlib.
    find ext -name 'libz.*' -exec rm -f {} \;
    tar -czf ../../sync_tools_gpdb/sync_tools_gpdb.tar.gz ext

  popd
}

function build_gpdb() {
  pushd gpdb_src/gpAux
    if [ -n "$1" ]; then
      make "$1" GPROOT=/usr/local dist
    else
      make GPROOT=/usr/local dist
    fi
  popd
}

function build_gppkg() {
  pushd gpdb_src/gpAux
    make gppkg BLD_TARGETS="gppkg" INSTLOC="$GREENPLUM_INSTALL_DIR" GPPKGINSTLOC="$GPDB_ARTIFACTS_DIR" RELENGTOOLS=/opt/releng/tools
  popd
}

function unittest_check_gpdb() {
  pushd gpdb_src/gpAux
    source $GREENPLUM_INSTALL_DIR/greenplum_path.sh
    make GPROOT=/usr/local unittest-check
  popd
}

function export_gpdb() {
  TARBALL="$GPDB_ARTIFACTS_DIR"/bin_gpdb.tar.gz
  pushd $GREENPLUM_INSTALL_DIR
    source greenplum_path.sh
    python -m compileall -x test .
    chmod -R 755 .
    tar -czf "${TARBALL}" ./*
  popd
}

function export_gpdb_extensions() {
  pushd gpdb_src/gpAux
    if ls greenplum-*zip* 1>/dev/null 2>&1; then
      chmod 755 greenplum-*zip*
      cp greenplum-*zip* "$GPDB_ARTIFACTS_DIR"/
    fi
    chmod 755 "$GPDB_ARTIFACTS_DIR"/*.gppkg
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
    *)
      echo "only centos and sles are supported TARGET_OS'es"
      false
      ;;
  esac

  generate_build_number
  make_sync_tools
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
  unittest_check_gpdb
  export_gpdb
  export_gpdb_extensions
}

_main "$@"
