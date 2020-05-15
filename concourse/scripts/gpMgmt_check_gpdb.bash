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

    # Run some of the following commands in a subshell so that they do not
    # pollute the environment after sourcing greenplum_path.
    (install_and_configure_gpdb)
    setup_gpadmin_user
    (make_cluster)

    install_python_requirements_on_single_host ./gpdb_src/gpMgmt/requirements-dev.txt

    gen_env

    # need to run setup_coverage as gpadmin due to scp and ssh commands
    su gpadmin -c "
        source ./gpdb_src/concourse/scripts/common.bash
        # setup hostfile_all for demo_cluster tests
        echo localhost > /tmp/hostfile_all
        setup_coverage ./gpdb_src
    "

    run_test

    # collect coverage
    cp -r /tmp/coverage/* ./coverage/
    tar_coverage "$TEST_NAME"
}

_main "$@"
