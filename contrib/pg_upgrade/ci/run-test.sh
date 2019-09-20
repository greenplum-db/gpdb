#!/usr/bin/env bash

create_gpadmin_user() {
	adduser --disabled-password --gecos "" gpadmin
}

ensure_ssh_daemon_is_running() {
	service ssh start
}

setup_utf8_locale() {
	locale-gen en_US.UTF-8
}

change_ownership_of_resources_to_gpadmin() {
	chown -R gpadmin:gpadmin ./gpdb6
	chown -R gpadmin:gpadmin ./gpdb5
}

run_test() {
	su -c ./gpdb6/contrib/pg_upgrade/ci/run-test-as-gpadmin.sh gpadmin
}

main() {
	create_gpadmin_user
	ensure_ssh_daemon_is_running
	setup_utf8_locale
	change_ownership_of_resources_to_gpadmin
	run_test
}

main "$@"
