#!/bin/bash -l

set -eox pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

function gen_env(){
  cat > /opt/run_test.sh <<-EOF
		source /usr/local/greenplum-db-devel/greenplum_path.sh
		cd "\${1}/gpdb_src/gpAux"
		source gpdemo/gpdemo-env.sh
		PGDATABASE=${PGDATABASE:=gptest}
		createdb ${PGDATABASE}
		export PGDATABASE="${PGDATABASE}"
		cd ../src/test/tinc
		source tinc_env.sh
		make ${MAKE_TEST_COMMAND}
	EOF

	chmod a+x /opt/run_test.sh
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

    time install_gpdb
    time ./gpdb_src/mgmt/concourse/scripts/setup_gpadmin_user.bash "$TEST_OS"

    #set the connection limit to 250 and segment pairs to 2 for TINC tests
    export DEFAULT_QD_MAX_CONNECT=250
    export NUM_PRIMARY_MIRROR_PAIRS=2
    time make_cluster

    time gen_env
    time run_test
}

_main "$@"
