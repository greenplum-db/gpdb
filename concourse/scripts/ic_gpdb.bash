#!/bin/bash -l

set -eox pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

function gen_env(){
  cat > /opt/run_test.sh <<-EOF
		trap look4diffs ERR

		function look4diffs() {

		    diff_files="../src/test/regress/regression.diffs
				../src/test/regress/bugbuster/regression.diffs"

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
		source /opt/gcc_env.sh
		cd "\${1}/gpdb_src/gpAux"
		source gpdemo/gpdemo-env.sh
		make ${MAKE_TEST_COMMAND}
	EOF

	chmod a+x /opt/run_test.sh
}

function setup_gpadmin_user() {
    ./gpdb_src/concourse/scripts/setup_gpadmin_user.bash "$TEST_OS"
}

function _main() {
    if [ -z "${MAKE_TEST_COMMAND}" ]; then
        echo "FATAL: MAKE_TEST_COMMAND is not set"
        exit 1
    fi

    if [ -z "$TEST_OS" ]; then
        echo "FATAL: TEST_OS is not set"
        exit 1
    fi

    if [ "$TEST_OS" != "centos" -a "$TEST_OS" != "sles" ]; then
        echo "FATAL: TEST_OS is set to an invalid value: $TEST_OS"
	echo "Configure TEST_OS to be centos or sles"
        exit 1
    fi

    time install_sync_tools
    time configure
    time install_gpdb
    time setup_gpadmin_user
    time make_cluster
    time gen_env
    time run_test
}

_main "$@"
