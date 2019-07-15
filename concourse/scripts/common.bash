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
      ./configure --prefix=/usr/local/greenplum-db-devel --with-perl --with-python --with-libxml --enable-mapreduce --enable-orafce --enable-tap-tests --disable-orca --with-openssl ${CONFIGURE_FLAGS}

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


function install_python_hacks() {
    # Our Python installation doesn't run standalone; it requires
    # LD_LIBRARY_PATH which causes virtualenv to fail (because the system and
    # vendored libpythons collide). We'll try our best to install patchelf to
    # fix this later, but it's not available on all platforms.
    if which zypper > /dev/null; then
        zypper addrepo https://download.opensuse.org/repositories/devel:tools:building/SLE_12_SP4/devel:tools:building.repo
        # Note that this will fail on SLES11.
        if ! zypper --non-interactive --gpg-auto-import-keys install patchelf; then
            set +x
            echo 'WARNING: could not install patchelf; virtualenv may fail later'
            echo 'WARNING: This is a known issue on SLES11.'
            set -x
        fi
    elif which yum > /dev/null; then
        yum install -y patchelf
    elif which apt > /dev/null; then
        apt update
        apt install patchelf
    else
        set +x
        echo "ERROR: install_python_hacks() doesn't support the current platform and should be modified"
        exit 1
    fi
}

function _install_python_requirements() {
    # virtualenv 16.0 and greater does not support python2.6, which is
    # used on centos6
    pip install --user virtualenv~=15.0
    export PATH=$PATH:~/.local/bin

    # create virtualenv before sourcing greenplum_path since greenplum_path
    # modifies PYTHONHOME and PYTHONPATH
    #
    # XXX Patch up the vendored Python's RPATH so we can successfully run
    # virtualenv. If we instead set LD_LIBRARY_PATH (as greenplum_path.sh
    # does), the system Python and the vendored Python will collide and
    # virtualenv will fail. This step requires patchelf.
    if which patchelf > /dev/null; then
        patchelf \
            --set-rpath /usr/local/greenplum-db-devel/ext/python/lib \
            /usr/local/greenplum-db-devel/ext/python/bin/python

        virtualenv \
            --python /usr/local/greenplum-db-devel/ext/python/bin/python /tmp/venv
    else
        # We don't have patchelf on this environment. The only workaround we
        # currently have is to set both PYTHONHOME and LD_LIBRARY_PATH and
        # pray that the resulting libpython collision doesn't break
        # something too badly.
        echo 'WARNING: about to execute a cross-linked virtualenv; here there be dragons'
        LD_LIBRARY_PATH=/usr/local/greenplum-db-devel/ext/python/lib \
        PYTHONHOME=/usr/local/greenplum-db-devel/ext/python \
        virtualenv \
            --python /usr/local/greenplum-db-devel/ext/python/bin/python /tmp/venv
    fi
}

function install_python_requirements_on_single_host() {
    local requirements_txt="$1"
    _install_python_requirements

    # Install requirements into the vendored Python stack
    mkdir -p /tmp/py-requirements
    source /tmp/venv/bin/activate
        pip install --prefix /tmp/py-requirements -r ${requirements_txt}
        cp -r /tmp/py-requirements/* /usr/local/greenplum-db-devel/ext/python/
    deactivate
}

function install_python_requirements_on_multi_host() {
    local requirements_txt="$1"
    _install_python_requirements

    # Install requirements into the vendored Python stack on all hosts.
    mkdir -p /tmp/py-requirements
    source /tmp/venv/bin/activate
        pip install --prefix /tmp/py-requirements -r ${requirements_txt}
        while read -r host; do
            rsync -rz /tmp/py-requirements/ "$host":/usr/local/greenplum-db-devel/ext/python/
        done < /tmp/hostfile_all
    deactivate
}
