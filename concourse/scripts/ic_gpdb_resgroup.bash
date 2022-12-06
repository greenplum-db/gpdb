#!/bin/bash -l

set -eox pipefail

./ccp_src/scripts/setup_ssh_to_cluster.sh

CLUSTER_NAME=$(cat ./cluster_env_files/terraform/name)

if [ "$TEST_OS" = centos6 ]; then
    CGROUP_BASEDIR=/cgroup
else
    CGROUP_BASEDIR=/sys/fs/cgroup
fi

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

    if [ "$TEST_OS" = centos6 ]; then
        ssh -t $gpdb_host_alias sudo bash -ex <<EOF
        mkdir -p $basedir
        mount -t tmpfs tmpfs $basedir
        for group in $groups; do
                mkdir -p $basedir/\$group
                mount -t cgroup -o $options,\$group cgroup $basedir/\$group
        done
EOF
    fi

     if [ "$TEST_OS" = rhel8 ]; then
         ssh -t $gpdb_host_alias sudo bash -ex <<EOF
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

    ssh -t $gpdb_host_alias sudo bash -ex <<EOF
        for comp in cpuset cpu cpuacct memory; do
            chmod -R 777 $basedir/\$comp
            mkdir -p $basedir/\$comp/gpdb
            chown -R gpadmin:gpadmin $basedir/\$comp/gpdb
            chmod -R 777 $basedir/\$comp/gpdb
        done
EOF
}

run_resgroup_test() {
    local gpdb_coordinator_alias=$1

    ssh $gpdb_coordinator_alias bash -ex <<EOF
        source /usr/local/greenplum-db-devel/greenplum_path.sh
        export PGPORT=5432
        export MASTER_DATA_DIRECTORY=/data/gpdata/coordinator/gpseg-1
        export LDFLAGS="-L\${GPHOME}/lib"
        export CPPFLAGS="-I\${GPHOME}/include"

        cd /home/gpadmin/gpdb_src
        ./configure --prefix=/usr/local/greenplum-db-devel \
            --without-zlib --without-rt --without-libcurl \
            --without-libedit-preferred --without-docdir --without-readline \
            --disable-gpcloud --disable-gpfdist --disable-orca \
            ${CONFIGURE_FLAGS}

        make -C /home/gpadmin/gpdb_src/src/test/regress
        ssh sdw1 mkdir -p /home/gpadmin/gpdb_src/src/test/regress </dev/null
        ssh sdw1 mkdir -p /home/gpadmin/gpdb_src/src/test/isolation2 </dev/null
        scp /home/gpadmin/gpdb_src/src/test/regress/regress.so \
            gpadmin@sdw1:/home/gpadmin/gpdb_src/src/test/regress/

        make installcheck-resgroup || (
            errcode=\$?
            find src/test/isolation2 -name regression.diffs \
            | while read diff; do
                cat <<EOF1

======================================================================
DIFF FILE: \$diff
----------------------------------------------------------------------

EOF1
                cat \$diff
              done
            exit \$errcode
        )
EOF
}

keep_minimal_cgroup_dirs() {
    local gpdb_coordinator_alias=$1
    local basedir=$CGROUP_BASEDIR

    ssh -t $gpdb_coordinator_alias sudo bash -ex <<EOF
        rmdir $basedir/memory/gpdb/*/ || :
        rmdir $basedir/memory/gpdb
        rmdir $basedir/cpuset/gpdb/*/ || :
        rmdir $basedir/cpuset/gpdb
EOF
}

setup_binary_swap_test() {
    local gpdb_coordinator_alias=$1

    if [ "${TEST_BINARY_SWAP}" != "true" ]; then
        return 0
    fi

    ssh $gpdb_coordinator_alias mkdir -p /tmp/local/greenplum-db-devel
    ssh $gpdb_coordinator_alias tar -zxf - -C /tmp/local/greenplum-db-devel \
        < binary_swap_gpdb/bin_gpdb.tar.gz
    ssh $gpdb_coordinator_alias sed -i -e "s@/usr/local@/tmp/local@" \
        /tmp/local/greenplum-db-devel/greenplum_path.sh
}

run_binary_swap_test() {
    local gpdb_coordinator_alias=$1

    if [ "${TEST_BINARY_SWAP}" != "true" ]; then
        return 0
    fi

    scp -r /tmp/build/*/binary_swap_gpdb/ $gpdb_coordinator_alias:/home/gpadmin/
    ssh $gpdb_coordinator_alias bash -ex <<EOF
        source /usr/local/greenplum-db-devel/greenplum_path.sh
        export PGPORT=5432
        export MASTER_DATA_DIRECTORY=/data/gpdata/coordinator/gpseg-1
        export BINARY_SWAP_VARIANT=_resgroup

        cd /home/gpadmin
        time ./gpdb_src/concourse/scripts/test_binary_swap_gpdb.bash
EOF
}

mount_cgroups ccp-${CLUSTER_NAME}-0
mount_cgroups ccp-${CLUSTER_NAME}-1
make_cgroups_dir ccp-${CLUSTER_NAME}-0
make_cgroups_dir ccp-${CLUSTER_NAME}-1
run_resgroup_test cdw

#
# below is for binary swap test
#

# remove cgroup dirs for new features such as the memory auditor
# the purpose is to ensure the backward compatibilities
keep_minimal_cgroup_dirs ccp-${CLUSTER_NAME}-0
keep_minimal_cgroup_dirs ccp-${CLUSTER_NAME}-1
# deploy the binaries for binary swap test
setup_binary_swap_test sdw1
# run it
run_binary_swap_test cdw
