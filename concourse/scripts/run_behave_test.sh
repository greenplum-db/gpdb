#!/bin/bash
set -ex

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

BEHAVE_FLAGS=$@

cat > ~/gpdb-env.sh <<'EOF'
  source /usr/local/greenplum-db-devel/greenplum_path.sh
  export PGPORT=5432
  export COORDINATOR_DATA_DIRECTORY=/data/gpdata/coordinator/gpseg-1
  export PGDATABASE=gptest

  alias cdd='cd \$COORDINATOR_DATA_DIRECTORY'
  # pip installs are done using --user, so are in ~/.local/bin which
  # is not in the default path over ssh
  export PATH=~/.local/bin:${PATH}
EOF
source ~/gpdb-env.sh

if gpstate > /dev/null 2>&1 ; then
  createdb gptest
  gpconfig --skipvalidation -c fsync -v off
  gpstop -u
fi

# TODO remove this pip install. only for debugging
if [ -f ~/segment_host_list ]; then
    gpssh -f ~/segment_host_list -e 'pip3 install coverage --user'
fi

ssh sdw5 'pip3 install coverage --user' > /dev/null 2>&1 || true

cd /home/gpadmin/gpdb_src/gpMgmt
make -f Makefile.behave behave flags="$BEHAVE_FLAGS"
