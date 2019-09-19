#!/usr/bin/env bash

set -o nounset

#
# Test assumes that the 6X installation has already been created
#
# ./scripts/test.bash
#
main() {
	local gpdb5_installation_path=$1
	local gpdb5_source_path=$2

	local gpdb6_installation_path=$3
	local gpdb6_source_path=$4

	./scripts/init-gpdb5-cluster.bash "$gpdb5_installation_path" "$gpdb5_source_path"
	./scripts/init-gpdb6-cluster.bash "$gpdb6_installation_path" "$gpdb6_source_path"

	make check
}

main "$@"
