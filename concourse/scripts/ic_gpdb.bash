#!/bin/bash -l

set -eox pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

function gen_env(){
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
		cd "\${1}/gpdb_src"
		source gpAux/gpdemo/gpdemo-env.sh
		export TEST_PGFDW=1
		make -s ${MAKE_TEST_COMMAND}
	EOF

	chmod a+x /opt/run_test.sh
}

function setup_gpadmin_user() {
    ./gpdb_src/concourse/scripts/setup_gpadmin_user.bash "$TEST_OS"
}

function move_postgres_for_fdw() {
    if [ -d postgres_for_fdw ]; then
        pgfdw_test_dir=gpdb_src/contrib/postgres_fdw/testdata
        if [ ! -d "${pgfdw_test_dir}" ]; then
            mkdir -p ${pgfdw_test_dir}
        fi
        mv postgres_for_fdw/postgresql-* ${pgfdw_test_dir}/
        chown -R gpadmin:gpadmin ${pgfdw_test_dir}
    fi
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
    time gen_env
    time move_postgres_for_fdw
    time run_test

    if [ "${TEST_BINARY_SWAP}" == "true" ]; then
        time ./gpdb_src/concourse/scripts/test_binary_swap_gpdb.bash
    fi

    if [ "${DUMP_DB}" == "true" ]; then
        chmod 777 sqldump
        su gpadmin -c ./gpdb_src/concourse/scripts/dumpdb.bash
    fi
}

_main "$@"
