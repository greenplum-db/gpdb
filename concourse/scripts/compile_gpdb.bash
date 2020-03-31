#!/bin/bash -l
set -exo pipefail

GREENPLUM_INSTALL_DIR=/usr/local/greenplum-db-devel
export GPDB_ARTIFACTS_DIR=$(pwd)/${OUTPUT_ARTIFACT_DIR}

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

GPDB_SRC_PATH=${GPDB_SRC_PATH:=gpdb_src}
GPDB_BIN_FILENAME=${GPDB_BIN_FILENAME:="bin_gpdb.tar.gz"}

function expand_glob_ensure_exists() {
  local -a glob=($*)
  [ -e "${glob[0]}" ]
  echo "${glob[0]}"
}

function prep_env_for_centos() {
  case "${TARGET_OS_VERSION}" in
    5)
      BLD_ARCH=rhel5_x86_64
      export JAVA_HOME=/usr/lib/jvm/java-1.6.0-openjdk-1.6.0.39.x86_64
      ;;

    6)
      BLD_ARCH=rhel6_x86_64
      export JAVA_HOME=/usr/lib/jvm/java-1.7.0-openjdk.x86_64
      ;;

    7)
      BLD_ARCH=rhel7_x86_64
      echo "Detecting java7 path ..."
      java7_packages=$(rpm -qa | grep -F java-1.7)
      java7_bin="$(rpm -ql ${java7_packages} | grep /jre/bin/java$)"
      alternatives --set java "$java7_bin"
      export JAVA_HOME="${java7_bin/jre\/bin\/java/}"
      ln -sf /usr/bin/xsubpp /usr/share/perl5/ExtUtils/xsubpp
      ;;

    *)
    echo "TARGET_OS_VERSION not set or recognized for Centos/RHEL"
    exit 1
    ;;
  esac

  source /opt/gcc_env.sh
  ln -sf $(pwd)/${GPDB_SRC_PATH}/gpAux/ext/${BLD_ARCH}/python-2.7.12 /opt/python-2.7.12
  export PATH=${JAVA_HOME}/bin:${PATH}
}

function prep_env_for_sles() {
  export JAVA_HOME=$(expand_glob_ensure_exists /usr/java/jdk1.7*)
  export PATH=${JAVA_HOME}/bin:${PATH}
  source /opt/gcc_env.sh
}

function generate_build_number() {
  pushd ${GPDB_SRC_PATH}
    #Only if its git repro, add commit SHA as build number
    # BUILD_NUMBER file is used by getversion file in GPDB to append to version
    if [ -d .git ] ; then
      echo "commit:`git rev-parse HEAD`" > BUILD_NUMBER
    fi
  popd
}

function link_tools_for_sles() {
  ln -sf "$(expand_glob_ensure_exists $(pwd)/${GPDB_SRC_PATH}/gpAux/ext/*/python-2.7.12 )" /opt
}

function build_gpdb() {
  pushd ${GPDB_SRC_PATH}/gpAux
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
  pushd ${GPDB_SRC_PATH}/gpAux
    make gppkg BLD_TARGETS="gppkg" INSTLOC="${GREENPLUM_INSTALL_DIR}" GPPKGINSTLOC="${GPDB_ARTIFACTS_DIR}" RELENGTOOLS=/opt/releng/tools
  popd
}

function git_info() {
  pushd ${GPDB_SRC_PATH}

  "${CWDIR}/git_info.bash" | tee ${GREENPLUM_INSTALL_DIR}/etc/git-info.json

  PREV_TAG=$(git describe --tags --abbrev=0 @^)

  cat > ${GREENPLUM_INSTALL_DIR}/etc/git-current-changelog.txt <<-EOF
	======================================================================
	Git log since previous release tag (${PREV_TAG})
	----------------------------------------------------------------------

	EOF

  git log --abbrev-commit --date=relative "${PREV_TAG}..@" | tee -a ${GREENPLUM_INSTALL_DIR}/etc/git-current-changelog.txt

  popd
}

function unittest_check_gpdb() {
  pushd ${GPDB_SRC_PATH}
    source ${GREENPLUM_INSTALL_DIR}/greenplum_path.sh
    make GPROOT=/usr/local -s unittest-check
  popd
}

function export_gpdb() {
  TARBALL="${GPDB_ARTIFACTS_DIR}/${GPDB_BIN_FILENAME}"
  pushd ${GREENPLUM_INSTALL_DIR}
    source greenplum_path.sh
    python -m compileall -q -x test .
    chmod -R 755 .
    tar -czf "${TARBALL}" ./*
  popd
}

function export_gpdb_extensions() {
  pushd ${GPDB_SRC_PATH}/gpAux
    if ls greenplum-*zip* 1>/dev/null 2>&1; then
      chmod 755 greenplum-*zip*
      cp greenplum-*zip* "${GPDB_ARTIFACTS_DIR}/"
    fi
    if ls "$GPDB_ARTIFACTS_DIR"/*.gppkg 1>/dev/null 2>&1; then
      chmod 755 "$GPDB_ARTIFACTS_DIR"/*.gppkg
    fi
  popd
}

function export_gpdb_win32_ccl() {
    pushd ${GPDB_SRC_PATH}/gpAux
    if [ -f "$(find . -maxdepth 1 -name 'greenplum-*.msi' -print -quit)" ] ; then
        cp greenplum-*.msi "${GPDB_ARTIFACTS_DIR}/"
    fi
    popd
}

function _main() {
  case "${TARGET_OS}" in
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
  
  # Copy input ext dir; assuming ext doesnt exist
  mv gpAux_ext/ext ${GPDB_SRC_PATH}/gpAux

  # Copy .ant dir
  mv gphdfs_dist/.ant /root/
  mkdir -p ${GPDB_SRC_PATH}/gpAux/extensions/gphdfs/dist
  mv gphdfs_dist/dist/*.jar ${GPDB_SRC_PATH}/gpAux/extensions/gphdfs/dist/
  mv gphdfs_dist/dist/*.tar ${GPDB_SRC_PATH}/gpAux/extensions/gphdfs/


  case "${TARGET_OS}" in
    sles) link_tools_for_sles ;;
  esac

  # By default, only GPDB Server binary is build.
  # Use BLD_TARGETS flag with appropriate value string to generate client, loaders
  # connectors binaries
  if [ -n "${BLD_TARGETS}" ]; then
    BLD_TARGET_OPTION=("BLD_TARGETS=\"$BLD_TARGETS\"")
  else
    BLD_TARGET_OPTION=("")
  fi

  # Copy gpaddon_src into gpAux/addon directory and set the ADDON_DIR
  # environment variable, so that quicklz support is available in enterprise
  # builds.
  export ADDON_DIR=addon
  export CONFIGURE_FLAGS=${CONFIGURE_FLAGS}
  # We cannot symlink the addon directory here because `make -C` resolves the
  # symlink and `cd`s to the actual directory. Currently the Makefile in the
  # addon directory assumes that it is located in a particular location under
  # the source tree and hence needs to be copied over.
  rsync -au gpaddon_src/ ${GPDB_SRC_PATH}/gpAux/${ADDON_DIR}
  build_gpdb "${BLD_TARGET_OPTION[@]}"
  git_info
  build_gppkg
  if [ "${TARGET_OS}" != "win32" ] ; then
      # Don't unit test when cross compiling. Tests don't build because they
      # require `./configure --with-zlib`.
      unittest_check_gpdb
  fi
  if [ "${EXTRACT_PXF}" == "true" ] ; then
      # Bundle PXF server
      tar -xzf pxf_tarball/pxf.tar.gz -C ${GREENPLUM_INSTALL_DIR}
  fi
  export_gpdb
  export_gpdb_extensions
  export_gpdb_win32_ccl
}

_main "$@"
