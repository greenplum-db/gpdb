#!/bin/bash -l

set -eox pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

function gen_env(){
  cat > /opt/run_test.sh <<-EOF

		source /usr/local/greenplum-db-devel/greenplum_path.sh
		cd "\${1}/gpdb_src/gpAux"
		source gpdemo/gpdemo-env.sh
		cd "\${1}/gpdb_src/gpMgmt/"
		# Does this report failures?
		make -f Makefile.behave behave tags=${BEHAVE_TAGS}
	EOF

	chmod a+x /opt/run_test.sh
}

function _main() {

    if [ -z "${BEHAVE_TAGS}" ]; then
        echo "FATAL: BEHAVE_TAGS is not set"
        exit 1
    fi

    time install_gpdb
    time ./gpdb_src/concourse/scripts/setup_gpadmin_user.bash

    time make_cluster

    time gen_env
    time run_test
}

_main "$@"
