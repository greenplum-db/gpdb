#!/bin/bash

source /usr/local/greenplum-db-devel/greenplum_path.sh
source /opt/gcc_env.sh
source ./gpdb_src/gpAux/gpdemo/gpdemo-env.sh
gpstart -a
pg_dumpall -f ./sqldump/dump.sql
xz -z ./sqldump/dump.sql
