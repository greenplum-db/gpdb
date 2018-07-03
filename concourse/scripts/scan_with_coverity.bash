#!/bin/bash -l
set -exo pipefail

BASE_DIR=$(pwd)
export GPDB_ARTIFACTS_DIR
GPDB_ARTIFACTS_DIR=$BASE_DIR/$OUTPUT_ARTIFACT_DIR

function prep_env_for_centos() {
  BLD_ARCH=rhel7_x86_64
  echo "Detecting java7 path ..."
  java7_packages=($(rpm -qa | grep -F java-1.7))
  java7_bin="$(rpm -ql "${java7_packages[@]}" | grep /jre/bin/java$)"
  alternatives --set java "$java7_bin"
  export JAVA_HOME="${java7_bin/jre\/bin\/java/}"
  ln -sf /usr/bin/xsubpp /usr/share/perl5/ExtUtils/xsubpp
  source /opt/gcc_env.sh

  ln -sf "$BASE_DIR"/gpdb_src/gpAux/ext/${BLD_ARCH}/python-2.7.12 /opt/python-2.7.12
  export PATH=${JAVA_HOME}/bin:${PATH}
}

function generate_build_number() {
  pushd gpdb_src
    #Only if its git repro, add commit SHA as build number
    # BUILD_NUMBER file is used by getversion file in GPDB to append to version
    if [ -d .git ] ; then
      echo "commit:$(git rev-parse HEAD)" > BUILD_NUMBER
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

function build_gpdb_and_scan_with_coverity() {
  local cov_int_dir="$1"

  pushd gpdb_src/gpAux
    cov-build --dir "$cov_int_dir" make BLD_TARGETS="gpdb" GPROOT=/usr/local
  popd
}

function upload_to_coverity() {
  (
    set +x
    local cov_int_base="$1"
    local sha="$2"
    local cov_int_tar="$cov_int_base"/cov-int.tgz

    tar czfp "$cov_int_tar" -C "$cov_int_base" cov-int

    response=$(curl --verbose \
    --progress-bar \
    --form token="$COVERITY_TOKEN" \
    --form email="$COVERITY_EMAIL" \
    --form file=@"$cov_int_tar" \
    --form version="$sha" \
    --form description="Generated by Concourse on https://gpdb.data.pivotal.ci/" \
    https://scan.coverity.com/builds?project=greenplum-db%2Fgpdb)

    ERROR_STRINGS=(
    "quota for this project has been reached"
    )

    for ERR in "${ERROR_STRINGS[@]}"; do
      if echo "$response" | grep -q "$ERR"; then
	echo "Coverty returned: \"$response\""
	echo "Response matches following know error: \"$ERR\""
        exit 1
      fi
    done
  )
}

function _main() {
  prep_env_for_centos
  generate_build_number
  make_sync_tools

  /opt/prepare-coverity.bash

  mkdir -p "$GPDB_ARTIFACTS_DIR"/cov-int
  build_gpdb_and_scan_with_coverity "$GPDB_ARTIFACTS_DIR"/cov-int

  sha=$(cd gpdb_src && git rev-parse HEAD)
  upload_to_coverity "$GPDB_ARTIFACTS_DIR" "$sha"
}

_main "$@"
