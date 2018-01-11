#!/bin/bash
set -eox pipefail

TINCTARGET="$@"
TINCDIR=/home/gpadmin/gpdb_src/src/test/tinc

cat > ~/gpdb-env.sh << EOF
  source /usr/local/greenplum-db-devel/greenplum_path.sh
  export PGPORT=5432
  export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1
  export PGDATABASE=gptest

  alias mdd='cd \$MASTER_DATA_DIRECTORY'
  alias tinc='cd ${TINCDIR}'
EOF
source ~/gpdb-env.sh

createdb gptest
createdb gpadmin

# make fsync by default off to improve test stability
gpconfig --skipvalidation -c fsync -v off
gpstop -u
gpconfig -s fsync

trap look4diffs ERR
function look4diffs() {
    find ${TINCDIR} -name *.diff -exec cat {} \; >> ${TINCDIR}/regression.diffs
    echo "=================================================================="
    echo "The differences that may have caused some tests to fail can be viewed in the file ${TINCDIR}/regression.diffs."
    echo "=================================================================="
    cat ${TINCDIR}/regression.diffs
    exit 1
}

cd ${TINCDIR}
source tinc_env.sh
make ${TINCTARGET}
