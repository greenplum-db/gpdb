#!/bin/bash -l

set -eox pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

function gen_env(){
    cat > /opt/run_test.sh <<-EOF
		set -ex

		source /usr/local/greenplum-db-devel/greenplum_path.sh

		cd "\${1}/gpdb_src/gpAux"
		source gpdemo/gpdemo-env.sh

		cd "\${1}/gpdb_src/gpMgmt/"
		BEHAVE_TAGS="${BEHAVE_TAGS}"
		BEHAVE_FLAGS="${BEHAVE_FLAGS}"
		if [ ! -z "\${BEHAVE_TAGS}" ]; then
		    make -f Makefile.behave behave tags=\${BEHAVE_TAGS}
		else
		    flags="\${BEHAVE_FLAGS}" make -f Makefile.behave behave
		fi
	EOF

    chmod a+x /opt/run_test.sh
}

function _main() {

    if [ -z "${BEHAVE_TAGS}" ] && [ -z "${BEHAVE_FLAGS}" ]; then
        echo "FATAL: BEHAVE_TAGS or BEHAVE_FLAGS not set"
        exit 1
    fi

    time install_gpdb
    time ./gpdb_src/concourse/scripts/setup_gpadmin_user.bash

    # Run inside a subshell so it does not pollute the environment after
    # sourcing greenplum_path
    time (make_cluster)

    time install_python_hacks
    time install_python_requirements_on_single_host ./gpdb_src/gpMgmt/requirements-dev.txt

    time gen_env

    time run_test
}

_main "$@"
