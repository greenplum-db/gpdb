#!/bin/bash

BEHAVE_FLAGS=$@

run_behave_tests() {
    local gpdb_host=$1

    yum install -y openssh-clients
    ssh -t $gpdb_host "bash -c \"\
        # Setup environment
        source /usr/local/greenplum-db-devel/greenplum_path.sh; \
        export PGDATABASE=gptest; \
        export PGPORT=5432; \
        export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1; \
        createdb ${PGDATABASE}; \
        # Run behave tests
        cd /home/gpadmin/gpdb_src/gpMgmt/; \
        make -f Makefile.behave behave flags='${BEHAVE_FLAGS}'; \
    \""
}

# Look at ~/.ssh/config for available hosts
run_behave_tests mdw
