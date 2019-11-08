#!/bin/bash
set -e

main() {
	local old_port=50000
	local new_port=60000

	# need python for sql isolation test modules.
	source gpdb6/greenplum_path.sh

	if [ $# -eq 0 ]
	then
		# run scheduled tests
		gpdb5_input="--schedule=gpdb5_schedule"
		gpdb6_input="--schedule=gpdb6_schedule"
	else
		# allow focused test
		gpdb5_input=$1
		gpdb6_input="upgraded_$1"
	fi

	./scripts/reset-cluster
	./scripts/gpdb5-cluster start
	./pg_upgrade_regress --init-file=./init_file \
						 --psqldir=gpdb5/bin \
						 --port=$old_port \
						 --old-port=$old_port \
						 $gpdb5_input
	./scripts/gpdb5-cluster stop
	./scripts/upgrade-cluster
	./scripts/gpdb6-cluster start
	./pg_upgrade_regress --use-existing \
						 --init-file=./init_file \
						 --psqldir=$new_bindir \
						 --port=$new_port \
						 --old-port=$old_port \
						 $gpdb6_input
	./scripts/gpdb6-cluster stop
}

main "$@"
