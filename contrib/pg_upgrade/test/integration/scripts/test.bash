#!/usr/bin/env bash

set -o nounset

download_gpdb5() {
	./scripts/download-gpdb5-source.bash
}

initialize_gpdb5_cluster() {
	./scripts/init-gpdb5-cluster.bash \
		./gpdb5-source/gpdb5-installation \
		./gpdb5-source
}

initialize_gpdb6_cluster() {
	local root_directory=$(git rev-parse --show-toplevel)

	./scripts/init-gpdb6-cluster.bash \
		"$GPHOME" \
		"$root_directory"
}

#
# Test assumes that the 6X installation has already been created
#
# ./scripts/test.bash
#
main() {
	download_gpdb5
	initialize_gpdb5_cluster
	initialize_gpdb6_cluster

	make check
}

main "$@"
