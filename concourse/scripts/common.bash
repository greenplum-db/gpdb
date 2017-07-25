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
        ./configure --prefix=/usr/local/greenplum-db-devel --with-python --with-libxml --disable-orca ${CONFIGURE_FLAGS}
	  else
        ./configure --prefix=/usr/local/greenplum-db-devel --with-perl --with-python --with-libxml --enable-mapreduce --disable-orca ${CONFIGURE_FLAGS}
      fi
  popd
}

function make_cluster() {
  source /usr/local/greenplum-db-devel/greenplum_path.sh
  export BLDWRAP_POSTGRES_CONF_ADDONS=${BLDWRAP_POSTGRES_CONF_ADDONS}
  # Currently, the max_concurrency tests in src/test/isolation2
  # require max_connections of at least 129.
  export DEFAULT_QD_MAX_CONNECT=150
  workaround_before_concourse_stops_stripping_suid_bits
  pushd gpdb_src/gpAux/gpdemo
    if [[ $CONFIGURE_FLAGS == *"--enable-segwalrep"* ]]; then
      su gpadmin -c "make create-segwalrep-cluster"
    else
      su gpadmin -c "make create-demo-cluster"
    fi
  popd
}

workaround_before_concourse_stops_stripping_suid_bits() {
  chmod u+s /bin/ping
}

function run_test() {
	# is this particular python version giving us trouble?
  ln -s "$(pwd)/gpdb_src/gpAux/ext/rhel6_x86_64/python-2.7.12" /opt
  su - gpadmin -c "bash /opt/run_test.sh $(pwd)"
}
