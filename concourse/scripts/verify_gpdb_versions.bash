#!/bin/bash
set -xeo pipefail

install_greenplum() {
  local bin_gpdb=$1
  local install_dir=/usr/local/greenplum-db-devel

  rm -rf "$install_dir"
  mkdir -p "$install_dir"
  tar -xf "${bin_gpdb}/bin_gpdb.tar.gz" -C "$install_dir"
  source "${install_dir}/greenplum_path.sh"
}

assert_postgres_version_matches() {
  local gpdb_src_sha=$1

  if [[ ! "$(postgres --gp-version)" =~ "commit:$gpdb_src_sha" ]]; then
    echo "bin_gpdb version: '$(postgres --gp-version)' does not match gpdb_src commit: '${gpdb_src_sha}'. Exiting..."
    exit 1
  fi
}

yum -d1 -y install git

GPDB_SRC_SHA=$(cd gpdb_src && git rev-parse HEAD)
for bin_gpdb in bin_gpdb_centos{6,7}; do
  install_greenplum "$bin_gpdb"
  assert_postgres_version_matches "$GPDB_SRC_SHA"
done

echo "Release Candidate SHA: ${GPDB_SRC_SHA}"
