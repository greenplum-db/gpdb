#!/bin/bash
set -ex

BEHAVE_FLAGS=$@

cat > ~/gpdb-env.sh << EOF
  source /usr/local/greenplum-db-devel/greenplum_path.sh
  export PGPORT=5432
  export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1
  export PGDATABASE=gptest

  alias mdd='cd \$MASTER_DATA_DIRECTORY'
EOF
source ~/gpdb-env.sh

if gpstate > /dev/null 2>&1 ; then
  createdb gptest
  gpconfig --skipvalidation -c fsync -v off
  gpstop -u
fi

cd /home/gpadmin/gpdb_src/gpMgmt
make -f Makefile.behave behave flags="$BEHAVE_FLAGS"
