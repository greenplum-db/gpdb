#!/usr/bin/env bash

main() {
	adduser --disabled-password --gecos "" gpadmin
	locale-gen en_US.UTF-8
	service ssh start

	chown -R gpadmin:gpadmin ./gpdb

	su -c ./gpdb/contrib/pg_upgrade/ci/run-test-as-gpadmin.sh gpadmin
}

main "$@"
