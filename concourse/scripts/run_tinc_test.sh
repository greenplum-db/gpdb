#!/bin/bash

TINC_TARGET=$@

source /usr/local/greenplum-db-devel/greenplum_path.sh
export PGPORT=5432
export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1
export PGDATABASE=gptest
createdb gptest
createdb gpadmin
cd /home/gpadmin/gpdb_src/src/test/tinc
source tinc_env.sh
make $@
