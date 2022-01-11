#!/usr/bin/env bash
set -exo pipefail

BASE_DIR=$(pwd)

GPDB_CLIENTS_HOME="${BASE_DIR}/gpdb_clients_home"

prepare() {
	mkdir "${GPDB_CLIENTS_HOME}"
	echo "Extracting gpdb clients package ..."
	tar -xzvf "${BASE_DIR}"/bin_gpdb_clients/*.tar.gz -C "${GPDB_CLIENTS_HOME}"
}

run_test() {
	cd "${GPDB_CLIENTS_HOME}" && source greenplum_clients_path.sh

	psql --version
	gpfdist --version
	gpload --version

	createdb --version
	createuser --version

	dropdb --version
	dropuser --version

	pg_dumpall --version
	pg_dump --version

	ldd "${GPDB_CLIENTS_HOME}/bin/gpfdist"
}

output_rc() {
	pushd "${BASE_DIR}" || return 1
	cp -a bin_gpdb_clients/*.tar.gz bin_gpdb_clients_rc/
	popd || return 1
}

main(){
	prepare
	run_test
	output_rc
}

main "$@"
