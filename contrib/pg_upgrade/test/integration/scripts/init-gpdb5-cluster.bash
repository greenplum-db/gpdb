#!/usr/bin/env bash

set -e

main() {
  source "scripts/shared/ui.bash"
  source "scripts/shared/init_cluster.bash"
  
  # extract arguments
  local gpdb_installation_path=$1
  local gpdb_source_path=$2
  local gpdb_version="gpdb5"

  # validate arguments
  validate_gpdb_installation_path "$gpdb_installation_path" "$gpdb_version"
  validate_gpdb_source_path "$gpdb_source_path" "$gpdb_version"
  
  # initialize
  init_cluster "$gpdb_installation_path" "$gpdb_source_path" "$gpdb_version"
}

main "$@"