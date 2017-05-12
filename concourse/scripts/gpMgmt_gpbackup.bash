#!/bin/bash -l

set -eox pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

function gen_env(){
  cat > /opt/run_test.sh <<-EOF
		set -x
		trap look4results ERR

		function look4results() {

		    results_files="\${base_path}/gpdb_src/gpMgmt/go-utils/src/backup_restore/gpbackup_test_results.log"

		    for results_file in \${results_files}; do
			if [ -f "\${results_file}" ]; then
			    cat <<-FEOF

						======================================================================
						RESULTS FILE: \${results_file}
						----------------------------------------------------------------------

						\$(cat "\${results_file}")

					FEOF
			fi
		    done
		    exit 1
		}
		base_path=\${1}
		export GOPATH=\${base_path}/gpdb_src/gpMgmt/go-utils
		export PATH=\$PATH:/usr/local/go/bin:\$GOPATH/bin
		cd \${base_path}/gpdb_src/gpMgmt/go-utils/src/backup_restore
		make test > gpbackup_test_results.log
		# show results into concourse
		cat \${base_path}/gpdb_src/gpMgmt/go-utils/src/backup_restore/gpbackup_test_results.log
	EOF

	chmod a+x /opt/run_test.sh
}

function setup_gpadmin_user() {
    ./gpdb_src/concourse/scripts/setup_gpadmin_user.bash "$TEST_OS"
}

function _main() {
    configure '--enable-gpbackup'
    setup_gpadmin_user
    gen_env
    run_test
}

_main "$@"
