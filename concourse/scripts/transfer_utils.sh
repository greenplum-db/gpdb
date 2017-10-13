#!/bin/bash

allow_connections_from_all_hosts() {
    echo host    all     all     0.0.0.0/0       trust >> /data/gpdata/master/gpseg-1/pg_hba.conf
    source /usr/local/greenplum-db-devel/greenplum_path.sh
    export PGPORT=5432
    export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1
    gpstop -u
}

create_map_file() {
    echo sdw1-2,`getent hosts sdw1-2 | cut -d ' ' -f1` > /tmp/source_map_file
}
