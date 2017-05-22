#!/bin/bash -l

set -eox pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

function gen_env(){
  cat > /opt/run_test.sh <<-EOF

		base_path=\${1}
		RESULT_FILE="\${base_path}/gpdb_src/gpMgmt/gpMgmt_testunit_results.log"
		trap look4results ERR

		function look4results() {

		cat "\${RESULT_FILE}"
		exit 1
		}

		source /usr/local/greenplum-db-devel/greenplum_path.sh
		source /opt/gcc_env.sh
		source \${base_path}/gpdb_src/gpAux/gpdemo/gpdemo-env.sh
		cd \${base_path}/gpdb_src/gpMgmt/bin
		make check
		# show results into concourse
		cat \${RESULT_FILE}
	EOF

	chmod a+x /opt/run_test.sh
}

function setup_gpadmin_user() {
    ./gpdb_src/concourse/scripts/setup_gpadmin_user.bash "$TEST_OS"
}

function _main() {

    configure
    install_gpdb
    setup_gpadmin_user
    make_cluster
    gen_env
    run_test
}

_main "$@"
