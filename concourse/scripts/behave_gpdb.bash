#!/bin/bash -l

set -eox pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

function install_python_hacks() {
    # Our Python installation doesn't run standalone; it requires
    # LD_LIBRARY_PATH which causes virtualenv to fail (because the system and
    # vendored libpythons collide). We'll try our best to install patchelf to
    # fix this later, but it's not available on all platforms.
    if which zypper > /dev/null; then
        zypper addrepo https://download.opensuse.org/repositories/devel:tools:building/SLE_12_SP4/devel:tools:building.repo
        # Note that this will fail on SLES11.
        if ! zypper --non-interactive --gpg-auto-import-keys install patchelf; then
            set +x
            echo 'WARNING: could not install patchelf; virtualenv may fail later'
            echo 'WARNING: This is a known issue on SLES11.'
            set -x
        fi
    elif which yum > /dev/null; then
        yum install -y patchelf
    else
        set +x
        echo "ERROR: install_python_hacks() doesn't support the current platform and should be modified"
        exit 1
    fi
}

function gen_env(){
    cat > /opt/run_test.sh <<-EOF
		set -ex

		# virtualenv 16.0 and greater does not support python2.6, which is
		# used on centos6
		pip install --user virtualenv~=15.0
		export PATH=\$PATH:~/.local/bin

		# create virtualenv before sourcing greenplum_path since greenplum_path
		# modifies PYTHONHOME and PYTHONPATH
		#
		# XXX Patch up the vendored Python's RPATH so we can successfully run
		# virtualenv. If we instead set LD_LIBRARY_PATH (as greenplum_path.sh
		# does), the system Python and the vendored Python will collide and
		# virtualenv will fail. This step requires patchelf.
		if which patchelf > /dev/null; then
			patchelf \
				--set-rpath /usr/local/greenplum-db-devel/ext/python/lib \
				/usr/local/greenplum-db-devel/ext/python/bin/python

			virtualenv \
				--python /usr/local/greenplum-db-devel/ext/python/bin/python /tmp/venv
		else
			# We don't have patchelf on this environment. The only workaround we
			# currently have is to set both PYTHONHOME and LD_LIBRARY_PATH and
			# pray that the resulting libpython collision doesn't break
			# something too badly.
			echo 'WARNING: about to execute a cross-linked virtualenv; here there be dragons'
			LD_LIBRARY_PATH=/usr/local/greenplum-db-devel/ext/python/lib \
			PYTHONHOME=/usr/local/greenplum-db-devel/ext/python \
			virtualenv \
				--python /usr/local/greenplum-db-devel/ext/python/bin/python /tmp/venv
		fi

		# activate virtualenv after sourcing greenplum_path, so that virtualenv
		# takes precedence
		source /usr/local/greenplum-db-devel/greenplum_path.sh
		source /tmp/venv/bin/activate

		cd "\${1}/gpdb_src/gpAux"
		source gpdemo/gpdemo-env.sh

		cd "\${1}/gpdb_src/gpMgmt/"
		pip install -r requirements-dev.txt

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
        kernel.sem = 500 1024000 200 4096
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
    time gen_env

    if echo "$BEHAVE_TAGS" "$BEHAVE_FLAGS" | grep -q "gpcheck\b"; then
        time gpcheck_setup
    fi

    time run_test
}

_main "$@"
