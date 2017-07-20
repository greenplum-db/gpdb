#!/bin/bash

BEHAVE_FLAGS=$@

run_behave_tests() {
    local gpdb_master_host=$1

    yum install -y openssh-clients
    remote_script=/tmp/run_behave_test.sh
    script=`mktemp`
    cat <<HERE > $script
source /usr/local/greenplum-db-devel/greenplum_path.sh
export PGPORT=5432
export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1
cd /home/gpadmin/gpdb_src/gpMgmt
make -f Makefile.behave behave flags='${BEHAVE_FLAGS}'

HERE

scp $script $gpdb_master_host:$remote_script

ssh -t $gpdb_master_host "bash $remote_script"

}

# Look at ~/.ssh/config for available hosts
run_behave_tests mdw
