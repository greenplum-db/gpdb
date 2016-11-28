#!/bin/bash -l

set -exo pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

function gen_env(){
	cat > /home/gpadmin/run_regression_test.sh <<-EOF
	set -exo pipefail

	source /opt/gcc_env.sh
	source /usr/local/greenplum-db-devel/greenplum_path.sh

	cd "\${1}/gpdb_src/gpAux"
	source gpdemo/gpdemo-env.sh

	cd "\${1}/gpdb_src/gpAux/extensions/gpcloud"
	make -B install

	cd "\${1}/gpdb_src/gpAux/extensions/gpcloud/regress"
	make installcheck pgxs_dir=/usr/local/greenplum-db-devel/lib/postgresql/pgxs

	[ -s regression.diffs ] && cat regression.diffs && exit 1
	exit 0
	EOF

	chown -R gpadmin:gpadmin $(pwd)
	chown gpadmin:gpadmin /home/gpadmin/run_regression_test.sh
	chmod a+x /home/gpadmin/run_regression_test.sh
}

function run_regression_test() {
	su - gpadmin -c "bash /home/gpadmin/run_regression_test.sh $(pwd)"
}

function setup_gpadmin_user() {
	./gpdb_src/concourse/scripts/setup_gpadmin_user.bash "$TEST_OS"
}

function _main() {
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
	ln -s "$(pwd)/gpdb_src/gpAux/ext/rhel5_x86_64/python-2.6.2" /opt

	time configure
	sed -i s/1024/unlimited/ /etc/security/limits.d/90-nproc.conf
	time install_gpdb
	time setup_gpadmin_user
	time make_cluster
	time gen_env

	echo -n "$s3conf" | base64 -d > /home/gpadmin/s3.conf
	chown gpadmin:gpadmin /home/gpadmin/s3.conf

	time run_regression_test
}

_main "$@"
