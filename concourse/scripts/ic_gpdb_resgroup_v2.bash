#!/bin/bash -l

set -eox pipefail

./ccp_src/scripts/setup_ssh_to_cluster.sh

CLUSTER_NAME=$(cat ./cluster_env_files/terraform/name)

CGROUP_BASEDIR=/sys/fs/cgroup

if [ "$TEST_OS" = centos7 ]; then
    CGROUP_AUTO_MOUNTED=1
fi

enable_cgroup_subtree_control() {
    local gpdb_host_alias=$1
    local basedir=$CGROUP_BASEDIR

    ssh -t $gpdb_host_alias sudo bash -ex <<EOF
        chmod -R 777 $basedir/
        echo "+cpu" >> $basedir/cgroup.subtree_control
        echo "+cpuset" >> $basedir/cgroup.subtree_control
        echo "+memory" >> $basedir/cgroup.subtree_control
        mkdir $basedir/gpdb
        chmod -R 777 $basedir/gpdb
EOF
}

run_resgroup_test() {
    local gpdb_master_alias=$1

    ssh $gpdb_master_alias bash -ex <<EOF
        source /usr/local/greenplum-db-devel/greenplum_path.sh
        export PGPORT=5432
        export COORDINATOR_DATA_DIRECTORY=/data/gpdata/coordinator/gpseg-1
        export LDFLAGS="-L\${GPHOME}/lib"
        export CPPFLAGS="-I\${GPHOME}/include"

        cd /home/gpadmin/gpdb_src
        PYTHON=python3 ./configure --prefix=/usr/local/greenplum-db-devel \
            --without-zlib --without-rt --without-libcurl \
            --without-libedit-preferred --without-docdir --without-readline \
            --disable-gpcloud --disable-gpfdist --disable-orca \
            --without-python PKG_CONFIG_PATH="\${GPHOME}/lib/pkgconfig" ${CONFIGURE_FLAGS}

        make -C /home/gpadmin/gpdb_src/src/test/regress
        ssh sdw1 mkdir -p /home/gpadmin/gpdb_src/src/test/regress </dev/null
        ssh sdw1 mkdir -p /home/gpadmin/gpdb_src/src/test/isolation2 </dev/null
        scp /home/gpadmin/gpdb_src/src/test/regress/regress.so \
            gpadmin@sdw1:/home/gpadmin/gpdb_src/src/test/regress/

        make PGOPTIONS="-c optimizer=off" installcheck-resgroup-v2 || (
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

setup_binary_swap_test() {
    local gpdb_master_alias=$1

    if [ "${TEST_BINARY_SWAP}" != "true" ]; then
        return 0
    fi

    ssh $gpdb_master_alias mkdir -p /tmp/local/greenplum-db-devel
    ssh $gpdb_master_alias tar -zxf - -C /tmp/local/greenplum-db-devel \
        < binary_swap_gpdb/bin_gpdb.tar.gz
    ssh $gpdb_master_alias sed -i -e "s@/usr/local@/tmp/local@" \
        /tmp/local/greenplum-db-devel/greenplum_path.sh
}

run_binary_swap_test() {
    local gpdb_master_alias=$1

    if [ "${TEST_BINARY_SWAP}" != "true" ]; then
        return 0
    fi

    scp -r /tmp/build/*/binary_swap_gpdb/ $gpdb_master_alias:/home/gpadmin/
    ssh $gpdb_master_alias bash -ex <<EOF
        source /usr/local/greenplum-db-devel/greenplum_path.sh
        export PGPORT=5432
        export COORDINATOR_DATA_DIRECTORY=/data/gpdata/coordinator/gpseg-1
        export BINARY_SWAP_VARIANT=_resgroup

        cd /home/gpadmin
        time ./gpdb_src/concourse/scripts/test_binary_swap_gpdb.bash
EOF
}

enable_cgroup_subtree_control ccp-${CLUSTER_NAME}-0
enable_cgroup_subtree_control ccp-${CLUSTER_NAME}-1
run_resgroup_test cdw

#
# below is for binary swap test
#

# deploy the binaries for binary swap test
setup_binary_swap_test sdw1
# run it
run_binary_swap_test cdw
