#!/bin/bash
set -eo pipefail
#set -x

GREENPLUM_INSTALL_DIR=/usr/local/greenplum-db-devel

compare_bin_gpdb_version_with_gpdb_src() {
  local bin_gpdb=$1

  # Install GPDB from bin_gpdb
  rm -rf ${GREENPLUM_INSTALL_DIR}
  mkdir -p ${GREENPLUM_INSTALL_DIR}
  tar -xf "${bin_gpdb}/bin_gpdb.tar.gz" -C "${GREENPLUM_INSTALL_DIR}/"

  # Get gpdb commit SHA
  source "${GREENPLUM_INSTALL_DIR}/greenplum_path.sh"
  bin_gpdb_version=$(postgres --gp-version | sed 's/.*build commit:\(.*\)-oss/\1/')

  if [ "${bin_gpdb_version}" != "${GPDB_SRC_VERSION}" ]; then
    echo "bin_gpdb version: ${bin_gpdb_version} does not match gpdb_src version: ${GPDB_SRC_VERSION}, please wait for the next build. Exiting..."
    exit 1
  fi

}

# Install dependencies
yum -y install git

# Get commit SHA from gpdb_src
pushd gpdb_src
  GPDB_SRC_VERSION=$(git rev-parse HEAD)
popd

for bin_gpdb in bin_gpdb_* ; do
  compare_bin_gpdb_version_with_gpdb_src "${bin_gpdb}"
done

echo "Release Candidate: ${GPDB_SRC_VERSION}"
