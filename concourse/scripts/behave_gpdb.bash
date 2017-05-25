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

function gpcheck_setup() {
    # gpcheck looks for specific system settings as a requirement.
    # normally, containers do not have root access when running as gpadmin.
    # So, we need to setup system requirements prior to running the test.  Note
    # that, for this test, we simply change the conf file gpcheck only checks
    # the conf file, not the runtime system settings
    echo "
        xfs_mount_options = rw,noatime,inode64,allocsize=16m
        kernel.shmmax = 500000000
        kernel.shmmni = 4096
        kernel.shmall = 4000000000
        kernel.sem = 250 512000 100 2048
        kernel.sysrq = 1
        kernel.core_uses_pid = 1
        kernel.msgmnb = 65536
        kernel.msgmax = 65536
        kernel.msgmni = 2048
        net.ipv4.tcp_syncookies = 1
        net.ipv4.ip_forward = 0
        net.ipv4.conf.default.accept_source_route = 0
        net.ipv4.tcp_tw_recycle = 1
        net.ipv4.tcp_max_syn_backlog = 4096
        net.ipv4.conf.all.arp_filter = 1
        net.ipv4.ip_local_port_range = 1025 65535
        net.core.netdev_max_backlog = 10000
        vm.overcommit_memory = 2" >> /etc/sysctl.conf

    echo "
        * soft  nofile  65536
        * hard  nofile  65536
        * soft  nproc   131072
        * hard  nproc   131072" >> /etc/security/limits.conf
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

    if [ "$GPCHECK_SETUP" = "true" ]; then
        time gpcheck_setup
    fi

    time run_test
}

_main "$@"
