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

setup_ddboost() {
    ssh centos@mdw "sudo rpm -ivh https://discuss.pivotal.io/hc/en-us/article_attachments/201458518/compat-libstdc__-33-3.2.3-69.el6.x86_64.rpm"
    ssh centos@mdw "echo \"$DD_SOURCE_HOST source-dd\" | sudo tee -a /etc/hosts"
    ssh centos@mdw "echo \"$DD_DEST_HOST dest-dd\" | sudo tee -a /etc/hosts"

    ssh centos@sdw1 "sudo rpm -ivh https://discuss.pivotal.io/hc/en-us/article_attachments/201458518/compat-libstdc__-33-3.2.3-69.el6.x86_64.rpm"
    ssh centos@sdw1 "echo \"$DD_SOURCE_HOST source-dd\" | sudo tee -a /etc/hosts"
    ssh centos@sdw1 "echo \"$DD_DEST_HOST dest-dd\" | sudo tee -a /etc/hosts"
}
