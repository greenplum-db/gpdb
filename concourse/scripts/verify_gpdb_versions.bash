#!/bin/bash
set -xeo pipefail

install_greenplum() {
  local bin_gpdb=$1
  local install_dir="${2}"

  rm -rf "$install_dir"
  mkdir -p "$install_dir"
  tar -xf "${bin_gpdb}/bin_gpdb.tar.gz" -C "$install_dir"
}

assert_postgres_version_matches() {
  local gpdb_src_sha="${1}"
  local install_dir="${2}"

  if ! readelf --string-dump=.rodata "${install_dir}/bin/postgres" | grep -c "commit:$gpdb_src_sha" > /dev/null ; then
    echo "bin_gpdb version: gpdb_src commit '${gpdb_src_sha}' not found in binary '$(command -v postgres)'. Exiting..."
    exit 1
  fi
}

yum -d0 -y install git

GREENPLUM_INSTALL_DIR=/usr/local/greenplum-db-devel
GPDB_SRC_SHA=$(cd gpdb_src && git rev-parse HEAD)

for bin_gpdb in bin_gpdb_{centos{6,7},ubuntu18.04}; do
  install_greenplum "$bin_gpdb" "${GREENPLUM_INSTALL_DIR}"
  assert_postgres_version_matches "$GPDB_SRC_SHA" "${GREENPLUM_INSTALL_DIR}"
done

echo "Release Candidate SHA: ${GPDB_SRC_SHA}"
