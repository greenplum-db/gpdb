#!/bin/bash

copy_backup_files() {
    source /usr/local/greenplum-db-devel/greenplum_path.sh;
    gpssh -f hostfile_all "bash -c \"\
        cd /data/gpdata
        find . -type d -name db_dumps > /tmp/db_dumps.files
        tar cf /tmp/db_dumps.tar -T /tmp/db_dumps.files
    \""
}

restore_backup_files() {
    source /usr/local/greenplum-db-devel/greenplum_path.sh;
    gpssh -f hostfile_all "bash -c \"\
        tar xf /tmp/db_dumps.tar -C /data/gpdata
    \""
}

destroy_gpdb() {
    # Setup environment
    source /usr/local/greenplum-db-devel/greenplum_path.sh;
    export PGPORT=5432;
    export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1;

    yes|gpdeletesystem -f -d $MASTER_DATA_DIRECTORY
    rm -rf $GPHOME/*
}
