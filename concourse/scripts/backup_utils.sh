#!/bin/bash

destroy_gpdb() {
    # Setup environment
    source /usr/local/greenplum-db-devel/greenplum_path.sh;
    export PGPORT=5432;
    export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1;

    yes|gpdeletesystem -f -d $MASTER_DATA_DIRECTORY
    rm -rf $GPHOME/*
    rm -rf /home/gpadmin/gpdb_src/gpMgmt/bin/pythonSrc/ext/install
}
