#!/bin/bash -l

## ----------------------------------------------------------------------
## General purpose functions
## ----------------------------------------------------------------------

function set_env() {
    export TERM=xterm-256color
    export TIMEFORMAT=$'\e[4;33mIt took %R seconds to complete this step\e[0m';
}

## ----------------------------------------------------------------------
## Test functions
## ----------------------------------------------------------------------

function install_gpdb() {
    [ ! -d /usr/local/greenplum-db-devel ] && mkdir -p /usr/local/greenplum-db-devel
    tar -xzf bin_gpdb/bin_gpdb.tar.gz -C /usr/local/greenplum-db-devel
}

function configure() {
  source /opt/gcc_env.sh
  pushd gpdb_src
      # The full set of configure options which were used for building the
      # tree must be used here as well since the toplevel Makefile depends
      # on these options for deciding what to test. Since we don't ship
      # Perl on SLES we must also skip GPMapreduce as it uses pl/perl.
      if [ "$TEST_OS" == "sles" ]; then
        ./configure --prefix=/usr/local/greenplum-db-devel --with-python --with-libxml --enable-orafce --disable-orca ${CONFIGURE_FLAGS}
      else
        ./configure --prefix=/usr/local/greenplum-db-devel --with-perl --with-python --with-libxml --enable-mapreduce --enable-orafce --disable-orca ${CONFIGURE_FLAGS}
      fi
  popd
}

function gen_gpexpand_input() {
  HOSTNAME=`hostname`
  PWD=`pwd`
  cat > input <<-EOF
$HOSTNAME:$HOSTNAME:25436:$PWD/datadirs/expand/primary:7:2:p
$HOSTNAME:$HOSTNAME:25437:$PWD/datadirs/expand/mirror:8:2:m
EOF

  chmod a+r input
}

function make_cluster() {
  source /usr/local/greenplum-db-devel/greenplum_path.sh
  export BLDWRAP_POSTGRES_CONF_ADDONS=${BLDWRAP_POSTGRES_CONF_ADDONS}
  # Currently, the max_concurrency tests in src/test/isolation2
  # require max_connections of at least 129.
  export DEFAULT_QD_MAX_CONNECT=150
  export STATEMENT_MEM=250MB
  export PGPORT=15432
  pushd gpdb_src/gpAux/gpdemo
  export MASTER_DATA_DIRECTORY=`pwd`"/datadirs/qddir/demoDataDir-1"
  # If $ONLINE_EXPAND is set, the job is to run ICW after expansion to see
  # whether all the cases have been passed without restarting cluster.
  # So it will create a cluster with two segments and execute gpexpand to
  # expand the cluster to three segments
  if [ ! -z "$ONLINE_EXPAND" ]
  then
    su gpadmin -c "make create-demo-cluster NUM_PRIMARY_MIRROR_PAIRS=2"
    # Backup master pid, to check it after the whole test, be sure there is
    # no test case restarting the cluster
    su gpadmin -c "head -n 1 $MASTER_DATA_DIRECTORY/postmaster.pid > master.pid.bk"
    gen_gpexpand_input
    su gpadmin -c "createdb gpstatus"
    su gpadmin -c "gpexpand -i input -D gpstatus"
  else
    su gpadmin -c "make create-demo-cluster"
  fi
  popd
}

function run_test() {
  # is this particular python version giving us trouble?
  ln -s "$(pwd)/gpdb_src/gpAux/ext/rhel6_x86_64/python-2.7.12" /opt
  su gpadmin -c "bash /opt/run_test.sh $(pwd)"
}
