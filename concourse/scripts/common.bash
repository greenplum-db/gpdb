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

# usage: gen_gpexpand_input <old_size> <new_size>
function gen_gpexpand_input() {
  local old="$1"
  local new="$2"
  local inputfile="/tmp/inputfile.${old}-${new}"
  local i

  for ((i=old; i<new; i++)); do
    cat <<EOF
$HOSTNAME:$HOSTNAME:$((25432+i*2)):$PWD/datadirs/expand/primary${i}:$((3+i*2)):${i}:p
$HOSTNAME:$HOSTNAME:$((25433+i*2)):$PWD/datadirs/expand/mirror${i}:$((4+i*2)):${i}:m
EOF
  done | tee $inputfile

  chmod a+r $inputfile
}

# extract PGOPTIONS from MAKE_TEST_COMMAND
function get_pgoptions() {
  local pgoptions

  cat >/tmp/get_pgoptions.mk <<"EOF"
$(info $(PGOPTIONS))
EOF

  pgoptions="$(eval make $MAKE_TEST_COMMAND -f /tmp/get_pgoptions.mk -nq 2>/dev/null)"
  echo "$pgoptions"
}

# usage: expand_cluster <old_size> <new_size>
function expand_cluster() {
  local old="$1"
  local new="$2"
  local inputfile="/tmp/inputfile.${old}-${new}"
  local pidfile="/tmp/postmaster.pid.${old}-${new}"
  local dbname="gpstatus"
  local pgoptions="$(get_pgoptions)"
  local uncompleted
  local partial

  pushd gpdb_src/gpAux/gpdemo

  gen_gpexpand_input "$old" "$new"

  # Backup master pid, by checking it later we can know whether the cluster is
  # restarted during the tests.
  su gpadmin -c "head -n 1 $MASTER_DATA_DIRECTORY/postmaster.pid >$pidfile"
  su gpadmin -c "createdb $dbname" 2>/dev/null || : # ignore failure
  # begin expansion
  su gpadmin -c "yes | PGOPTIONS='$pgoptions' gpexpand -D $dbname -s -i $inputfile"
  # redistribute tables
  su gpadmin -c "yes | PGOPTIONS='$pgoptions' gpexpand -D $dbname -s"
  # check the result
  uncompleted=$(su gpadmin -c "psql -Aqtd $dbname -c \"select count(*) from gpexpand.status_detail where status <> 'COMPLETED'\"")
  # cleanup
  su gpadmin -c "yes | PGOPTIONS='$pgoptions' gpexpand -D $dbname -s -c"

  popd

  if [ "$uncompleted" -ne 0 ]; then
	  echo "error: some tables are not successfully expanded"
	  return 1
  fi

  # double check gp_distribution_policy.numsegments
  partial=$(su gpadmin -c "psql -Aqtd $dbname -c \"select count(*) from gp_distribution_policy where numsegments <> $new\"")
  if [ "$partial" -ne 0 ]; then
	  echo "error: not all the tables are expanded by gpexpand"
	  return 1
  fi

  echo "all the tables are successfully expanded"
  return 0
}

# usage: make_cluster [<demo_cluster_options>]
#
# demo_cluster_options are passed to `make create-demo-cluster` literally,
# any options accepted by that command are acceptable here.
#
# e.g. make_cluster WITH_MIRRORS=false
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
  su gpadmin -c "make create-demo-cluster $@"
  popd
}

function run_test() {
  # is this particular python version giving us trouble?
  ln -s "$(pwd)/gpdb_src/gpAux/ext/rhel6_x86_64/python-2.7.12" /opt
  su gpadmin -c "bash /opt/run_test.sh $(pwd)"
}
