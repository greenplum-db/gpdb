#!/bin/bash -l

set -eox pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

./ccp_src/scripts/setup_ssh_to_cluster.sh

CLUSTER_NAME=$(cat ./cluster_env_files/terraform/name)

CGROUP_BASEDIR=/sys/fs/cgroup

if [ "$TEST_OS" = centos7 ]; then
    CGROUP_AUTO_MOUNTED=1
fi

mount_cgroups() {
    local gpdb_host_alias=$1
    local basedir=$CGROUP_BASEDIR
    local options=rw,nosuid,nodev,noexec,relatime
    local groups="cpuset blkio cpuacct cpu memory"
    local rhel8_groups="cpuset blkio cpu,cpuacct memory"

    if [ "$CGROUP_AUTO_MOUNTED" ]; then
        # nothing to do as cgroup is already automatically mounted
        return
    fi

    if [ "$TEST_OS" = rhel8 ]; then
      ssh $gpdb_host_alias sudo -n bash -ex <<EOF
        mkdir -p $basedir
        mount -t tmpfs tmpfs $basedir
        for group in $rhel8_groups; do
                mkdir -p $basedir/\$group
                mount -t cgroup -o $options,\$group cgroup $basedir/\$group
        done
        ln -s $basedir/cpu,cpuacct $basedir/cpu
        ln -s $basedir/cpu,cpuacct $basedir/cpuacct
EOF
    fi
}

make_cgroups_dir() {
    local gpdb_host_alias=$1
    local basedir=$CGROUP_BASEDIR

    ssh $gpdb_host_alias sudo -n bash -ex <<EOF
        for comp in cpuset cpu cpuacct memory; do
            chmod -R 777 $basedir/\$comp
            mkdir -p $basedir/\$comp/gpdb
            chown -R gpadmin:gpadmin $basedir/\$comp/gpdb
            chmod -R 777 $basedir/\$comp/gpdb
        done
EOF
}

# remove resource queue test from icw task,else the resoure manager mode may change to 'none'
run_resgroup_icw_test() {
    local gpdb_master_alias=$1

    ssh $gpdb_master_alias bash -ex <<EOF
        trap look4diffs ERR
        
	function look4diffs() {
		
		diff_files=\`find -name regression.diffs\`
		
		for diff_file in \${diff_files}; do
		if [ -f "\${diff_file}" ]; then
		    cat <<-FEOF
				======================================================================
				DIFF FILE: \${diff_file}
				----------------------------------------------------------------------

				\$(cat "\${diff_file}")

			FEOF
		fi
		done
		exit 1
	}
        source /usr/local/greenplum-db-devel/greenplum_path.sh
        export LDFLAGS="-L\${GPHOME}/lib"
        export CPPFLAGS="-I\${GPHOME}/include"
        export BLDWRAP_POSTGRES_CONF_ADDONS=${BLDWRAP_POSTGRES_CONF_ADDONS}

        cd /home/gpadmin/gpdb_src
        sed -i 's/explain_format//g' src/test/regress/greenplum_schedule
        sed -i '/resource_manager_switch/,/resource_manager_restore/d' src/test/regress/greenplum_schedule
        echo>src/test/isolation2/isolation2_resqueue_schedule
        ./configure --prefix=/usr/local/greenplum-db-devel --disable-orca --enable-gpfdist \
         --enable-gpcloud --enable-orafce --enable-tap-tests \
         --with-gssapi --with-libxml --with-openssl --with-perl --with-python \
         --with-uuid=e2fs --with-llvm --with-zstd PYTHON=python3.9 PKG_CONFIG_PATH="${GPHOME}/lib/pkgconfig" ${CONFIGURE_FLAGS}

        LANG=en_US.utf8 make create-demo-cluster WITH_MIRRORS=${WITH_MIRRORS:-true}
        source /home/gpadmin/gpdb_src/gpAux/gpdemo/gpdemo-env.sh

        PG_TEST_EXTRA="kerberos ssl" make -s ${MAKE_TEST_COMMAND}
EOF
}

keep_minimal_cgroup_dirs() {
    local gpdb_master_alias=$1
    local basedir=$CGROUP_BASEDIR

    ssh $gpdb_master_alias sudo -n bash -ex <<EOF
        rmdir $basedir/memory/gpdb/*/ || :
        rmdir $basedir/memory/gpdb
        rmdir $basedir/cpuset/gpdb/*/ || :
        rmdir $basedir/cpuset/gpdb
EOF
}


mount_cgroups ccp-${CLUSTER_NAME}-0
make_cgroups_dir ccp-${CLUSTER_NAME}-0
run_resgroup_icw_test cdw

keep_minimal_cgroup_dirs ccp-${CLUSTER_NAME}-0
