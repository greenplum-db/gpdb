#!/bin/bash -l

set -eox pipefail

./ccp_src/aws/setup_ssh_to_cluster.sh

CLUSTER_NAME=$(cat ./cluster_env_files/terraform/name)

if [ "$TEST_OS" = centos6 ]; then
    CGROUP_BASEDIR=/cgroup
else
    CGROUP_BASEDIR=/sys/fs/cgroup
fi

if [ "$TEST_OS" = centos7 -o "$TEST_OS" = sles12 ]; then
    CGROUP_AUTO_MOUNTED=1
fi

mount_cgroups() {
    local gpdb_host_alias=$1
    local basedir=$CGROUP_BASEDIR
    local options=rw,nosuid,nodev,noexec,relatime
    local groups="cpuset blkio cpuacct cpu memory"

    if [ "$CGROUP_AUTO_MOUNTED" ]; then
        # nothing to do as cgroup is already automatically mounted
        return
    fi

    ssh -t $gpdb_host_alias sudo bash -ex <<EOF
        mkdir -p $basedir
        mount -t tmpfs tmpfs $basedir
        for group in $groups; do
                mkdir -p $basedir/\$group
                mount -t cgroup -o $options,\$group cgroup $basedir/\$group
        done
EOF
}

make_cgroups_dir() {
    local gpdb_host_alias=$1
    local basedir=$CGROUP_BASEDIR

    ssh -t $gpdb_host_alias sudo bash -ex <<EOF
        for comp in cpu cpuacct; do
            chmod -R 777 $basedir/\$comp
            mkdir -p $basedir/\$comp/gpdb
            chown -R gpadmin:gpadmin $basedir/\$comp/gpdb
            chmod -R 777 $basedir/\$comp/gpdb
        done
EOF
}

run_resgroup_test() {
    local gpdb_master_alias=$1

    ssh $gpdb_master_alias bash -ex <<EOF
        source /usr/local/greenplum-db-devel/greenplum_path.sh
        export PGPORT=5432
        export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1

        cd /home/gpadmin/gpdb_src
        ./configure --prefix=/usr/local/greenplum-db-devel \
            --without-zlib --without-rt --without-libcurl \
            --without-libedit-preferred --without-docdir --without-readline \
            --disable-gpcloud --disable-gpfdist --disable-orca \
            --disable-pxf ${CONFIGURE_FLAGS}

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

mount_cgroups ccp-${CLUSTER_NAME}-0
mount_cgroups ccp-${CLUSTER_NAME}-1
make_cgroups_dir ccp-${CLUSTER_NAME}-0
make_cgroups_dir ccp-${CLUSTER_NAME}-1
run_resgroup_test mdw
