#!/bin/bash
set -eox pipefail

./ccp_src/aws/setup_ssh_to_cluster.sh

CLUSTER_NAME=$(cat ./cluster_env_files/terraform/name)

if [ "$TEST_OS" != sles12 ]; then
    echo "FATAL: TEST_OS is set to an invalid value: $TEST_OS"
    exit 1
fi

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

SRCDIR_OLD=/home/gpadmin/gpdb_src
SRCDIR_NEW=/data/gpdata/gpdb_src

prepare_env() {
    local gpdb_host_alias=$1

    ssh -t $gpdb_host_alias sudo bash -ex <<"EOF"
        systemctl status guestregister.service || true
        systemctl stop guestregister.service || true

        zypper -n refresh
        zypper -n install gcc-c++ make bison flex \
            krb5 krb5-client krb5-server \
            || ( ps -ef; exit 1 )

        cp /usr/lib/mit/sbin/krb5kdc /usr/sbin/
        ln -sf "$(rpm -ql perl | grep libperl.so)" /lib64/libperl.so
EOF
}

setup_demo_cluster() {
    local gpdb_master_alias=$1

    ssh -t $gpdb_master_alias bash -ex <<EOF
        source /usr/local/greenplum-db-devel/greenplum_path.sh
        export PGPORT=5432
        export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1

        gpstop -a
        unset PGPORT
        unset MASTER_DATA_DIRECTORY

        cp -a ${SRCDIR_OLD} ${SRCDIR_NEW}

        cd ${SRCDIR_NEW}
        ./configure --prefix=/usr/local/greenplum-db-devel \
            --with-python --disable-orca --without-readline \
            --without-zlib --disable-gpfdist --without-libcurl \
            --disable-pxf --without-libbz2

        export DEFAULT_QD_MAX_CONNECT=150
        export STATEMENT_MEM=250MB
        make create-demo-cluster

        source gpAux/gpdemo/gpdemo-env.sh

        make -C ${SRCDIR_NEW}/src/test/regress
EOF
}

run_resgroup_test() {
    local gpdb_master_alias=$1

    #scp gpdb_binary/bin_gpdb.tar.gz $gpdb_master_alias:/tmp

    ssh -t $gpdb_master_alias bash -ex <<EOF
        function look4diffs() {
            find . -name regression.diffs \
            | while read diff_file; do
                [ -f "\${diff_file}" ] || continue

                cat <<FEOF

======================================================================
DIFF FILE: \${diff_file}
----------------------------------------------------------------------

FEOF
                cat "\${diff_file}"
              done
            exit 1
        }

        cd ${SRCDIR_NEW}
        trap look4diffs ERR

        source /usr/local/greenplum-db-devel/greenplum_path.sh
        source gpAux/gpdemo/gpdemo-env.sh

        make $MAKE_TEST_COMMAND CC=gcc CXX=g++
EOF
}

prepare_env ccp-${CLUSTER_NAME}-0
setup_demo_cluster mdw
run_resgroup_test mdw
