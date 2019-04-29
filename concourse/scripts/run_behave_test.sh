#!/bin/bash
set -ex

BEHAVE_FLAGS=$@

# virtualenv 16.0 and greater does not support python2.6, which is
# used on centos6
pip install --user virtualenv~=15.0
export PATH=$PATH:~/.local/bin

# create virtualenv before sourcing greenplum_path since greenplum_path
# modifies PYTHONHOME and PYTHONPATH
#
# XXX Patch up the vendored Python's RPATH so we can successfully run
# virtualenv. If we instead set LD_LIBRARY_PATH (as greenplum_path.sh does), the
# system Python and the vendored Python will collide and virtualenv will fail.
# This step requires patchelf.
patchelf \
    --set-rpath /usr/local/greenplum-db-devel/ext/python/lib \
    /usr/local/greenplum-db-devel/ext/python/bin/python

virtualenv \
    --python /usr/local/greenplum-db-devel/ext/python/bin/python /tmp/venv


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

# activate virtualenv after sourcing greenplum_path, so that virtualenv takes
# precedence
source /tmp/venv/bin/activate

cd /home/gpadmin/gpdb_src/gpMgmt
pip install -r requirements-dev.txt
make -f Makefile.behave behave flags="$BEHAVE_FLAGS"
