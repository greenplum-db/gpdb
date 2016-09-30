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

function install_sync_tools() {
    tar -xzf sync_tools_gpdb_centos/sync_tools_gpdb.tar.gz -C gpdb_src/gpAux
}

function configure() {
  source /opt/gcc_env.sh
  pushd gpdb_src
      ./configure --prefix=/usr/local/greenplum-db-devel
  popd
}

function make_cluster() {
  source /usr/local/greenplum-db-devel/greenplum_path.sh
  workaround_before_concourse_stops_stripping_suid_bits
  pushd gpdb_src/gpAux/gpdemo
      su gpadmin -c make cluster
  popd
}

workaround_before_concourse_stops_stripping_suid_bits() {
  chmod u+s /bin/ping
}

function run_test() {
  ln -s "$(pwd)/gpdb_src/gpAux/ext/rhel6_x86_64/python-2.6.2" /opt
  su - gpadmin -c "bash /opt/run_test.sh $(pwd)"
}
