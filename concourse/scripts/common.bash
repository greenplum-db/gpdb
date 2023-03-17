#!/bin/bash -l

## ----------------------------------------------------------------------
## General purpose functions
## ----------------------------------------------------------------------

function set_env() {
    export TERM=xterm-256color
    export TIMEFORMAT=$'\e[4;33mIt took %R seconds to complete this step\e[0m'
}

function build_arch() {
    if [[ ! -f /etc/os-release ]]; then
        echo "unable to determine platform" && exit 1
    fi
    local id="$(. /etc/os-release && echo "${ID}")"
    local version=$(. /etc/os-release && echo "${VERSION_ID}")
    # BLD_ARCH expects rhel{6,7,8}_x86_64 || rocky8_x86_64 || photon3_x86_64 || sles12_x86_64 || ubuntu18.04_x86_64 || ol8_x86_64
    case ${id} in
    photon | sles | rhel | rocky | ol) version=$(echo "${version}" | cut -d. -f1) ;;
    centos) id="rhel" ;;
    *) ;;
    esac
    echo "${id}${version}_x86_64"
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
    pushd gpdb_src
    # The full set of configure options which were used for building the
    # tree must be used here as well since the toplevel Makefile depends
    # on these options for deciding what to test. Since we don't ship
    ./configure --prefix=/usr/local/greenplum-db-devel --disable-orca --enable-gpcloud --enable-mapreduce --enable-orafce --enable-tap-tests --with-gssapi --with-libxml --with-openssl --with-perl --with-python --with-uuid=e2fs --with-llvm --with-zstd PYTHON=python3 PKG_CONFIG_PATH="${GPHOME}/lib/pkgconfig" ${CONFIGURE_FLAGS}

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

    su gpadmin -c "source /usr/local/greenplum-db-devel/greenplum_path.sh; LANG=en_US.utf8 make create-demo-cluster WITH_MIRRORS=${WITH_MIRRORS:-true}"

    if [[ "$MAKE_TEST_COMMAND" =~ gp_interconnect_type=proxy ]]; then
        # generate the addresses for proxy mode
        su gpadmin -c bash -- -e <<EOF
      source /usr/local/greenplum-db-devel/greenplum_path.sh
      source $PWD/gpdemo-env.sh

      delta=-3000

      psql -tqA -d postgres -P pager=off -F: -R, \
          -c "select dbid, content, address, port+\$delta as port
                from gp_segment_configuration
               order by 1" \
      | xargs -rI'{}' \
        gpconfig --skipvalidation -c gp_interconnect_proxy_addresses -v "'{}'"

      # also have to enlarge gp_interconnect_tcp_listener_backlog
      gpconfig -c gp_interconnect_tcp_listener_backlog -v 1024

      gpstop -u
EOF
    fi

    popd
}

function run_test() {
    su gpadmin -c "bash /opt/run_test.sh $(pwd)"
}

function install_python_requirements_on_single_host() {
    # installing python requirements on single host only happens for demo cluster tests,
    # and is run by root user. Therefore, pip install as root user to make items globally
    # available
    local requirements_txt="$1"

    export PIP_CACHE_DIR=${PWD}/pip-cache-dir
    pip3 --retries 10 install -r ${requirements_txt}
}

function install_python_requirements_on_multi_host() {
    # installing python requirements on multi host happens exclusively as gpadmin user.
    # Therefore, add the --user flag and add the user path to the path in run_behave_test.sh
    # the user flag is required for centos 7
    local requirements_txt="$1"

    # Set PIP Download cache directory
    export PIP_CACHE_DIR=/home/gpadmin/pip-cache-dir

    pip3 --retries 10 install --user -r ${requirements_txt}
    while read -r host; do
        scp ${requirements_txt} "$host":/tmp/requirements.txt
        ssh $host PIP_CACHE_DIR=${PIP_CACHE_DIR} pip3 --retries 10 install --user -r /tmp/requirements.txt
    done </tmp/hostfile_all
}

function setup_coverage() {
    # Enables coverage.py on all hosts in the cluster. Note that this function
    # modifies greenplum_path.sh, so callers need to source that file AFTER this
    # is done.
    local gpdb_src_dir="$1"
    local commit_sha=$(head -1 "$gpdb_src_dir/.git/HEAD")
    local coverage_path="/tmp/coverage/$commit_sha"

    # This file will be copied into GPDB's PYTHONPATH; it sets up the coverage
    # hook for all Python source files that are executed.
    cat >/tmp/sitecustomize.py <<SITEEOF
import coverage
coverage.process_startup()
SITEEOF

    # Set up coverage.py to handle analysis from multiple parallel processes.
    cat >/tmp/coveragerc <<COVEOF
[run]
branch = True
data_file = $coverage_path/coverage
parallel = True
COVEOF

    # Now copy everything over to the hosts.
    while read -r host; do
        scp /tmp/sitecustomize.py "$host":/usr/local/greenplum-db-devel/lib/python
        scp /tmp/coveragerc "$host":/usr/local/greenplum-db-devel
        ssh "$host" "mkdir -p $coverage_path" </dev/null

        # Enable coverage instrumentation after sourcing greenplum_path.
        ssh "$host" "echo 'export COVERAGE_PROCESS_START=/usr/local/greenplum-db-devel/coveragerc' >> /usr/local/greenplum-db-devel/greenplum_path.sh" </dev/null
    done </tmp/hostfile_all
}

function tar_coverage() {
    # Call this function after running tests under the setup_coverage
    # environment. It performs any final needed manipulation of the coverage
    # data before it is published.
    local prefix="$1"

    # Uniquify the coverage files a little bit with the supplied prefix.
    pushd ./coverage/*
    for f in *; do
        mv "$f" "$prefix.$f"
    done

    # Compress coverage files and remove the originals
    tar --remove-files -cf "$prefix.tar" *
    popd
}

function add_ccache_support() {

    _TARGET_OS=$1

    ## Add CCache support
    if [[ "${USE_CCACHE}" = "true" ]]; then
        if [[ "${_TARGET_OS}" = "centos" ]]; then
            # Enable CCache use
            export PATH=/usr/lib64/ccache:$PATH
        fi

        export CCACHE_DIR=$(pwd)/ccache_dir
        export CCACHE_BASEDIR=$(pwd)

        ## Display current Ccache Stats
        display_ccache_stats

        ## Zero the cache statistics
        ccache -z
    fi
}

function display_ccache_stats() {
    if [[ "${USE_CCACHE}" = "true" ]]; then
        cat <<EOF

======================================================================
                            CCACHE STATS
----------------------------------------------------------------------

$(ccache --show-stats)

======================================================================

EOF
    fi
}
