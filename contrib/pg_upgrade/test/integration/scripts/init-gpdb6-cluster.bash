#!/usr/bin/env bash

set -e

after_cluster_created() {
	echo "after cluster created"
}

main() {
	source "scripts/shared/ui.bash"
	source "scripts/shared/init_cluster.bash"

	# extract arguments
	local gpdb_installation_path=$1
	local gpdb_source_path=$2
	local gpdb_version="gpdb6"

	echo "Initializing a $gpdb_version cluster using: installation=$gpdb_installation_path source=$gpdb_source_path"
	echo ""

	# validate arguments
	validate_gpdb_installation_path "$gpdb_installation_path" "$gpdb_version"
	validate_gpdb_source_path "$gpdb_source_path" "$gpdb_version"

	# initialize
	init_cluster "$gpdb_installation_path" "$gpdb_source_path" "$gpdb_version"
}

main "$@"
