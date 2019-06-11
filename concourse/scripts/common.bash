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
    mkdir -p /usr/local/greenplum-db-devel
    tar -xzf bin_gpdb/bin_gpdb.tar.gz -C /usr/local/greenplum-db-devel
}

function setup_configure_vars() {
    # We need to add GPHOME paths for configure to check for packaged
    # libraries (e.g. ZStandard).
    source /usr/local/greenplum-db-devel/greenplum_path.sh
    export LDFLAGS="-L${GPHOME}/lib"
    export CPPFLAGS="-I${GPHOME}/include"
}

function configure() {
  if [ -f /opt/gcc_env.sh ]; then
    # ubuntu uses the system compiler
    source /opt/gcc_env.sh
  fi
  pushd gpdb_src
      # The full set of configure options which were used for building the
      # tree must be used here as well since the toplevel Makefile depends
      # on these options for deciding what to test. Since we don't ship
      ./configure --prefix=/usr/local/greenplum-db-devel --with-perl --with-python --with-libxml --enable-mapreduce --enable-orafce --enable-tap-tests --disable-orca ${CONFIGURE_FLAGS}

  popd
}

function install_and_configure_gpdb() {
  install_gpdb
  setup_configure_vars
  configure
}

function make_cluster() {
  source /usr/local/greenplum-db-devel/greenplum_path.sh
  export BLDWRAP_POSTGRES_CONF_ADDONS=${BLDWRAP_POSTGRES_CONF_ADDONS}
  export STATEMENT_MEM=250MB
  pushd gpdb_src/gpAux/gpdemo
  su gpadmin -c "source /usr/local/greenplum-db-devel/greenplum_path.sh; make create-demo-cluster"
  popd
}

function run_test() {
  # is this particular python version giving us trouble?
  ln -s "$(pwd)/gpdb_src/gpAux/ext/rhel6_x86_64/python-2.7.12" /opt
  su gpadmin -c "bash /opt/run_test.sh $(pwd)"
}
