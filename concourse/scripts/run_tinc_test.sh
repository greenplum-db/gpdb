#!/bin/bash

TINC_TARGET="$@"
TINC_DIR=/home/gpadmin/gpdb_src/src/test/tinc

trap look4diffs ERR
function look4diffs() {
	find "\${TINC_DIR}" -name *.diff -exec cat {} \; >> "\${TINC_DIR}/regression.diffs"
	echo "=================================================================="
	echo "The differences that caused some tests to fail can also be viewed in the file saved at \${TINC_DIR}/regression.diffs."
	echo "=================================================================="
	cat "\${TINC_DIR}/regression.diffs"
	exit 1
}

cat > ~/gpdb-env.sh << EOF
  source /usr/local/greenplum-db-devel/greenplum_path.sh
  export PGPORT=5432
  export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1
  export PGDATABASE=gptest

  alias mdd='cd \$MASTER_DATA_DIRECTORY'
  alias tinc='cd ${TINC_DIR}'
EOF
source ~/gpdb-env.sh

# make fsync by default off to improve test stability
gpconfig --skipvalidation -c fsync -v off
gpstop -u

createdb gptest
createdb gpadmin
cd ${TINC_DIR}
source tinc_env.sh
make ${TINC_TARGET}
