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

  ln -sf /$(pwd)/gpdb_src/gpAux/ext/${BLDARCH}/python-2.7.12 /opt/python-2.7.12
  export PATH=${JAVA_HOME}/bin:${PATH}
}

function prep_env_for_sles() {
  ln -sf "$(pwd)/gpdb_src/gpAux/ext/sles11_x86_64/python-2.7.12" /opt
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
    # Requires these variables in the env:
    # IVYREPO_HOST IVYREPO_REALM IVYREPO_USER IVYREPO_PASSWD
    make sync_tools
    # We have compiled LLVM with native zlib on CentOS6 and not from
    # the zlib downloaded from artifacts.  Therefore, remove the zlib
    # downloaded from artifacts in order to use the native zlib.
    find ext -name 'libz.*' -exec rm -f {} \;
  popd
}

function build_gpdb() {
  pushd gpdb_src/gpAux
    # Use -j4 to speed up the build. (Doesn't seem worth trying to guess a better
    # value based on number of CPUs or anything like that. Going above -j4 wouldn't
    # make it much faster, and -j4 is small enough to not hurt too badly even on
    # a single-CPU system
    if [ -n "$1" ]; then
      make "$1" GPROOT=/usr/local PARALLEL_MAKE_OPTS=-j4 dist
    else
      make GPROOT=/usr/local PARALLEL_MAKE_OPTS=-j4 dist
    fi
  popd
}

function build_gppkg() {
  pushd gpdb_src/gpAux
    make gppkg BLD_TARGETS="gppkg" INSTLOC="$GREENPLUM_INSTALL_DIR" GPPKGINSTLOC="$GPDB_ARTIFACTS_DIR" RELENGTOOLS=/opt/releng/tools
  popd
}

function unittest_check_gpdb() {
  pushd gpdb_src
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
    if ls "$GPDB_ARTIFACTS_DIR"/*.gppkg 1>/dev/null 2>&1; then
      chmod 755 "$GPDB_ARTIFACTS_DIR"/*.gppkg
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

  # Copy gpaddon_src into gpAux/addon directory and set the ADDON_DIR
  # environment variable, so that quicklz support is available in enterprise
  # builds.
  export ADDON_DIR=addon
  # We cannot symlink the addon directory here because `make -C` resolves the
  # symlink and `cd`s to the actual directory. Currently the Makefile in the
  # addon directory assumes that it is located in a particular location under
  # the source tree and hence needs to be copied over.
  cp -R gpaddon_src gpdb_src/gpAux/$ADDON_DIR
  build_gpdb "${BLD_TARGET_OPTION[@]}"
  build_gppkg
  unittest_check_gpdb
  export_gpdb
  export_gpdb_extensions
}

_main "$@"
