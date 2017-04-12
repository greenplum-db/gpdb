#!/bin/bash -l

set -exo pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

function gen_env(){
	cat > /home/gpadmin/build_gpcloud_components.sh <<-EOF
	set -exo pipefail

	source /opt/gcc_env.sh
	source /usr/local/greenplum-db-devel/greenplum_path.sh

	cd "\${1}/gpdb_src/gpAux/extensions/gpcloud"
	make
	make gpcheckcloud
	EOF

	chown -R gpadmin:gpadmin $(pwd)
	chown gpadmin:gpadmin /home/gpadmin/build_gpcloud_components.sh
	chmod a+x /home/gpadmin/build_gpcloud_components.sh
}

function setup_gpadmin_user() {
	./gpdb_src/concourse/scripts/setup_gpadmin_user.bash "centos"
}

function build_gpcloud_components() {
	su - gpadmin -c "bash /home/gpadmin/build_gpcloud_components.sh $(pwd)"
}

function push_to_staging_server() {
	set +x && echo -n "$EC2_PRIVATE_KEY" | base64 -d > /home/gpadmin/ec2_private_key && set -x
	chmod 600 /home/gpadmin/ec2_private_key

	ssh -i /home/gpadmin/ec2_private_key -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null gpadmin@$EC2_INSTANCE_IP "source /home/gpadmin/greenplum-db-data/env/env.sh; source /home/gpadmin/greenplum-db/greenplum_path.sh; if gpstate &> /dev/null; then gpstop -a; fi"
	scp -i /home/gpadmin/ec2_private_key -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null bin_gpdb/bin_gpdb.tar.gz gpadmin@$EC2_INSTANCE_IP:/home/gpadmin/bin_gpdb.tar.gz
	ssh -i /home/gpadmin/ec2_private_key -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null gpadmin@$EC2_INSTANCE_IP "rm -rf /home/gpadmin/greenplum-db-devel/;mkdir /home/gpadmin/greenplum-db-devel;tar -xzf /home/gpadmin/bin_gpdb.tar.gz -C /home/gpadmin/greenplum-db-devel;source /home/gpadmin/greenplum-db-data/env/env.sh;sed -ri 's/\/usr\/local/\/home\/gpadmin/g' /home/gpadmin/greenplum-db-devel/greenplum_path.sh;source /home/gpadmin/greenplum-db/greenplum_path.sh"

	scp -i /home/gpadmin/ec2_private_key -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null gpdb_src/gpAux/extensions/gpcloud/bin/gpcheckcloud/gpcheckcloud gpadmin@$EC2_INSTANCE_IP:/home/gpadmin/greenplum-db/bin/
	scp -i /home/gpadmin/ec2_private_key -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null gpdb_src/gpAux/extensions/gpcloud/gpcloud.so gpadmin@$EC2_INSTANCE_IP:/home/gpadmin/greenplum-db/lib/postgresql/
	ssh -i /home/gpadmin/ec2_private_key -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null gpadmin@$EC2_INSTANCE_IP "source /home/gpadmin/greenplum-db-data/env/env.sh; source /home/gpadmin/greenplum-db/greenplum_path.sh; gpstart -a"
}

function _main() {
	time configure
	time install_gpdb
	time setup_gpadmin_user
	time gen_env

	time build_gpcloud_components
	time push_to_staging_server
}

_main "$@"
