#!/bin/bash -l

set -eox pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
GREENPLUM_INSTALL_DIR=/usr/local/gpdb

function load_transfered_bits_into_install_dir() {
  mkdir -p $GREENPLUM_INSTALL_DIR
  tar xzf $TRANSFER_DIR/$COMPILED_BITS_FILENAME -C $GREENPLUM_INSTALL_DIR
}

function configure() {
  pushd gpdb_src
    ./configure --prefix=${GREENPLUM_INSTALL_DIR} --with-gssapi --with-perl --with-python --with-libxml --enable-mapreduce --disable-orca --enable-pxf ${CONFIGURE_FLAGS}
  popd
}

function setup_gpadmin_user() {
    ./gpdb_src/concourse/scripts/setup_gpadmin_user.bash ubuntu
}

function make_cluster() {
  export BLDWRAP_POSTGRES_CONF_ADDONS="fsync=off"
  source "${GREENPLUM_INSTALL_DIR}/greenplum_path.sh"
  export DEFAULT_QD_MAX_CONNECT=150
  pushd gpdb_src/gpAux/gpdemo
    su gpadmin -c "source ${GREENPLUM_INSTALL_DIR}/greenplum_path.sh && make create-demo-cluster"
  popd
}

function gen_icw_test_script(){
  cat > /opt/run_test.sh <<-EOF
  SRC_DIR="\${1}/gpdb_src"
  trap look4diffs ERR
  function look4diffs() {
    diff_files=\`find .. -name regression.diffs\`
    for diff_file in \${diff_files}; do
      if [ -f "\${diff_file}" ]; then
        cat <<-FEOF
          ======================================================================
          DIFF FILE: \${diff_file}
          ----------------------------------------------------------------------
          \$(cat "\${diff_file}")
FEOF
      fi
    done
  exit 1
  }
  source ${GREENPLUM_INSTALL_DIR}/greenplum_path.sh
  source \${SRC_DIR}/gpAux/gpdemo/gpdemo-env.sh
  cd \${SRC_DIR}
  make ${MAKE_TEST_COMMAND}

EOF

	chmod a+x /opt/run_test.sh
}

function gen_unit_test_script(){
  cat > /opt/run_unit_test.sh <<-EOF
    SRC_DIR="\${1}/gpdb_src"
    RESULT_FILE="\${SRC_DIR}/gpMgmt/gpMgmt_testunit_results.log"
    trap look4results ERR
    function look4results() {
      cat "\${RESULT_FILE}"
      exit 1
    }
    source ${GREENPLUM_INSTALL_DIR}/greenplum_path.sh
    source \${SRC_DIR}/gpAux/gpdemo/gpdemo-env.sh
    cd \${SRC_DIR}/gpMgmt/bin
    make check
    # show results into concourse
    cat \${RESULT_FILE}
EOF

	chmod a+x /opt/run_unit_test.sh
}

function run_icw_test() {
  su - gpadmin -c "bash /opt/run_test.sh $(pwd)"
}

function run_unit_test() {
  su - gpadmin -c "bash /opt/run_unit_test.sh $(pwd)"
}

function _main() {
    if [ -z "${MAKE_TEST_COMMAND}" ]; then
        echo "FATAL: MAKE_TEST_COMMAND is not set"
        exit 1
    fi

    time load_transfered_bits_into_install_dir
    time configure
    time setup_gpadmin_user
    time make_cluster
    time gen_unit_test_script
    time gen_icw_test_script
    time run_unit_test
    time run_icw_test
}

_main "$@"
