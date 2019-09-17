#!/usr/bin/env bash

set -e

remove_gphdfs_permissions() {
	psql postgres -c "alter role $USER NOCREATEEXTTABLE(protocol='gphdfs',type='readable');"
	psql postgres -c "alter role $USER NOCREATEEXTTABLE(protocol='gphdfs',type='writable');"
}

after_cluster_created() {
	remove_gphdfs_permissions
}

main() {
	source "scripts/shared/ui.bash"
	source "scripts/shared/init_cluster.bash"

	# extract arguments
	local gpdb_installation_path=$1
	local gpdb_source_path=$2
	local gpdb_version="gpdb5"

	echo "Initializing a $gpdb_version cluster using: installation=$gpdb_installation_path source=$gpdb_source_path"
	echo ""

	# validate arguments
	validate_gpdb_installation_path "$gpdb_installation_path" "$gpdb_version"
	validate_gpdb_source_path "$gpdb_source_path" "$gpdb_version"

	# initialize
	init_cluster "$gpdb_installation_path" "$gpdb_source_path" "$gpdb_version"
}

main "$@"
