#!/bin/bash -l

set -eox pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

function setup_gpadmin_user() {
	./gpdb_src/concourse/scripts/setup_gpadmin_user.bash "$TEST_OS"
}

function install_gpdb_clients() {
	mkdir -p /usr/local/greenplum-clients-devel
	tar -xzf bin_gpdb_clients/bin_gpdb_clients.tar.gz -C /usr/local/greenplum-clients-devel
	pushd /usr/local/greenplum-clients-devel
	source /usr/local/greenplum-clients-devel/greenplum_clients_path.sh
	psql --version
	chown -R gpadmin:gpadmin /usr/local/greenplum-clients-devel
	popd
}

function copy_test_cases() {
	# always use the test cases without SCRAM-SHA-256 for ABI testing
	pushd gpdb_src
	rm -rf src/test/authentication/t/*
	cp ../abi_test_src/src/test/authentication/t/* src/test/authentication/t/
	rm -rf src/test/ssl/t/*
	cp ../abi_test_src/src/test/ssl/t/* src/test/ssl/t/
	popd
}

function gen_env() {
	cat > /opt/run_test.sh <<-EOF
		trap look4diffs ERR

		function look4diffs() {

		    diff_files=\`find .. -name regression.diffs\`

		    for diff_file in \${diff_files}; do
			if [ -f "\${diff_file}" ]; then
			    cat <<-FEOF

						======================================================================
						DIFF FILE: \${diff_file}
						----------------------------------------------------------------------

						\$(cat "\${diff_file}")

					FEOF
			fi
		    done
		    exit 1
		}

		source /usr/local/greenplum-db-devel/greenplum_path.sh
		# greenplum_clients_path.sh gets it path via `pwd`, cd to workaround this issue.
		cd /usr/local/greenplum-clients-devel && source /usr/local/greenplum-clients-devel/greenplum_clients_path.sh

		source "\${1}/gpdb_src/gpAux/gpdemo/gpdemo-env.sh"

		cd "\${1}/gpdb_src/src/test/regress"
		make
		cd "\${1}/gpdb_src/src/test/authentication"
		make check
		cd "\${1}/gpdb_src/src/test/ssl"
		make check
	EOF

	chmod a+x /opt/run_test.sh
}

function _main() {
	if [ -z "$TEST_OS" ]; then
		echo "FATAL: TEST_OS is not set"
		exit 1
	fi

	case "${TEST_OS}" in
		centos|ubuntu|sles) ;; #Valid
		*)
			echo "FATAL: TEST_OS is set to an invalid value: $TEST_OS"
			echo "Configure TEST_OS to be centos, or ubuntu"
			exit 1
			;;
	esac

	time install_and_configure_gpdb
	time setup_gpadmin_user
	time make_cluster
	time install_gpdb_clients
	time copy_test_cases
	time gen_env
	time run_test
}

_main "$@"
