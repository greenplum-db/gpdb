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

# In a pipeline for tests with asserts, standard LLVM is replaced to custom one
# built with asserts on, so JIT-ed code can benefit from the assertion checks
# just like other parts.
#
# Note: the assertion mode of LLVM should keep synced between build and runtime.
# A mismatch would fail the execution of program with libLLVM dependency
# (due to different ABI and LLVM_ABI_BREAKING_CHECKS)
#
# Note2: clang also has libLLVM as its runtime dependencies, the standard libLLVM
# must be preserved and applied to clang in build stage.
#
function setup_llvm() {

    # only setup custom llvm in pipeline with asserts on
    if [[ "${CONFIGURE_FLAGS}" =~ enable-cassert ]];then

        # only when custom llvm is provided (the build stage and tests with JIT on) 
        [ -d llvm-with-asserts-packages ] || return 0

        echo "installing llvm with asserts"
        pushd llvm-with-asserts-packages

        # backup original llvm libs
        local backup_libdir=$PWD/lib.orig
        local assertion_mode=$(llvm-config --assertion-mode)
        if [ "$assertion_mode" = "OFF" ];then
            mkdir -p "$backup_libdir"
            local libfiles=$(llvm-config --libfiles)
            cp -t $backup_libdir $libfiles
        fi

        # fix dependency of standard llvm for clang
        local clang_path=$(which clang)
        if [ ! -e ${clang_path}.orig ];then
            mv ${clang_path} ${clang_path}.orig
        fi
        cat >$clang_path <<EOF
#!/bin/bash
export LD_LIBRARY_PATH=$PWD/lib.orig/\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}
exec clang.orig \$@
EOF
        chmod +x $clang_path

        # version check: replace llvm with mismatched version is not expected
        local installed_version=$(rpm -q --queryformat='%{version}' llvm)
        local installed_release=$(rpm -q --queryformat='%{release}' llvm |grep -Po '^\d+')
        if [ -z "$installed_version" -o -z "$installed_release" ];then
            echo "failed to get installed llvm version info"
            exit 1
        fi

        echo "extracting llvm packages:"
        tar -xzvf llvm-with-asserts-*.tar.gz

        local custom_llvm_pkg=$(find ./ -name 'llvm-[0-9]*.rpm')
        local custom_version=$(rpm -q --queryformat='%{version}' $custom_llvm_pkg)
        local custom_release=$(rpm -q --queryformat='%{release}' $custom_llvm_pkg |grep -Po '^\d+')
        if [ "$custom_version" != "$installed_version" -o "$custom_release" != "$installed_release" ];then
            echo "llvm version not synced: installed $installed_version-$installed_release, custom $custom_version-$custom_release"
            exit 1
        fi

        # replace llvm:

        local llvm_package_list
        local pkg pkg_custom
        local eflag
        for pkg in $(rpm -qa --queryformat='%{name}-%{version}\n' |grep llvm);do
            [[ "$pkg" =~ ^llvm- ]] || continue
            pkg_custom=$(echo ./$pkg-*.rpm)
            if [ -z "$pkg_custom" -o ! -f "$pkg_custom" ];then
                echo "$pkg not found in custom llvm packages"
                eflag=1
            else
                llvm_package_list="$llvm_package_list $pkg_custom"
            fi
        done
        if [ -n "$eflag" ];then
            echo "failed to find all llvm packages wanted"
            exit 1
        fi

        yum install -y $llvm_package_list

        # check if llvm assertion is ON now
        assertion_mode=$(llvm-config --assertion-mode)
        if [ "$assertion_mode" = "OFF" ];then
            echo "llvm assertion is still not enabled"
            exit 1
        fi

        popd
    fi
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
    ./configure --prefix=/usr/local/greenplum-db-devel --disable-orca --enable-gpcloud --enable-orafce --enable-tap-tests --with-gssapi --with-libxml --with-openssl --with-perl --with-python --with-uuid=e2fs --with-llvm --with-zstd PYTHON=python3.9 PKG_CONFIG_PATH="${GPHOME}/lib/pkgconfig" ${CONFIGURE_FLAGS}

    popd
}

function install_and_configure_gpdb() {
    install_gpdb
    setup_llvm
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
