#!/bin/bash -l

set -exo pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

function gen_env(){
	cat > /home/gpadmin/run_regression_gpcheckcloud.sh <<-EOF
	set -exo pipefail

	source /opt/gcc_env.sh
	source /usr/local/greenplum-db-devel/greenplum_path.sh

	if [ "$overwrite_gpcloud" = "true" ]
	then
		cd "\${1}/gpdb_src/gpAux/extensions/gpcloud"
		make install -C bin/gpcheckcloud
	fi

	cd "\${1}/gpdb_src/gpAux/extensions/gpcloud/regress"
	bash gpcheckcloud_regress.sh
	EOF

	chown -R gpadmin:gpadmin $(pwd)
	chown gpadmin:gpadmin /home/gpadmin/run_regression_gpcheckcloud.sh
	chmod a+x /home/gpadmin/run_regression_gpcheckcloud.sh
}

function run_regression_gpcheckcloud() {
	su - gpadmin -c "bash /home/gpadmin/run_regression_gpcheckcloud.sh $(pwd)"
}

function setup_gpadmin_user() {
	./gpdb_src/concourse/scripts/setup_gpadmin_user.bash "centos"
}

function _main() {
	time configure
        sed -i s/1024/unlimited/ /etc/security/limits.d/90-nproc.conf
	time install_gpdb
	time setup_gpadmin_user
	time gen_env

	set +x && echo -n "$s3conf" | base64 -d > /home/gpadmin/s3.conf && set -x
	chown gpadmin:gpadmin /home/gpadmin/s3.conf

	time run_regression_gpcheckcloud
}

_main "$@"
