#!/bin/bash -l

set -eo pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

function setup_gpadmin_user() {
    ./gpdb_src/concourse/scripts/setup_gpadmin_user.bash "$TEST_OS"
}

# Get ssh private key from REMOTE_KEY, which is assumed to
# be encode in base64. We can't pass the key content directly
# since newline doesn't work well for env variable.
function import_remote_key() {
    echo -n $REMOTE_KEY | base64 -d > ~/remote.key
    chmod 400 ~/remote.key

    eval `ssh-agent -s`
    ssh-add ~/remote.key

    ssh-keyscan -p $REMOTE_PORT $REMOTE_HOST > pubkey
    awk '{printf "[%s]:", $1 }' pubkey > tmp
    echo -n $REMOTE_PORT >> tmp
    awk '{$1 = ""; print $0; }' pubkey >> tmp

    cat tmp >> ~/.ssh/known_hosts
}

# Simulate actual clients package installation, and try to
# connect with psql.
# SSH tunnel will forward the port of gpdemo cluster on concourse
# worker to remote machine.
function run_clients_test() {
    unzip installer_aix7_gpdb_clients/*.zip
    scp -P $REMOTE_PORT *clients*.bin $REMOTE_USER@$REMOTE_HOST:$GPDB_DIR/
    ssh -T -p $REMOTE_PORT $REMOTE_USER@$REMOTE_HOST "$GPDB_DIR/*clients*.bin" <<-EOF
    yes
    $GPDB_DIR/install
    yes
    yes
EOF

    ssh -T -R$PGPORT:127.0.0.1:$PGPORT -p $REMOTE_PORT $REMOTE_USER@$REMOTE_HOST <<- EOF
    export PGPORT=$PGPORT
    . $GPDB_DIR/install/greenplum_clients_path.sh
    psql -h 127.0.0.1 -c 'select version();' postgres
EOF
}

# Simulate actual loaders package installation. Loader
# tests need psql as well, so install to same location
# as clients.
# SSH tunnel will forward gpdb cluster port to remote
# machine and gpfdist port to concourse worker.
function run_loaders_test() {
    unzip installer_aix7_gpdb_loaders/*.zip
    scp -P $REMOTE_PORT *loaders*.bin $REMOTE_USER@$REMOTE_HOST:$GPDB_DIR/
    ssh -T -p $REMOTE_PORT $REMOTE_USER@$REMOTE_HOST "$GPDB_DIR/*loaders*.bin" <<-EOF
    yes
    $GPDB_DIR/install
    yes
    yes
EOF

    # copy test suites and diff utils to remote
    scp -P $REMOTE_PORT -r ./gpdb_src/gpMgmt/bin/gpload_test/gpload2 $REMOTE_USER@$REMOTE_HOST:$GPDB_DIR/
    scp -P $REMOTE_PORT ./gpdb_src/src/test/regress/*.pl $REMOTE_USER@$REMOTE_HOST:$GPDB_DIR/install/bin/
    scp -P $REMOTE_PORT ./gpdb_src/src/test/regress/*.pm $REMOTE_USER@$REMOTE_HOST:$GPDB_DIR/install/bin/

    ssh -T -R$PGPORT:127.0.0.1:$PGPORT -L8081:127.0.0.1:8081 -L8082:127.0.0.1:8082 -p $REMOTE_PORT $REMOTE_USER@$REMOTE_HOST <<- EOF
    export PGPORT=$PGPORT
    export PGUSER=gpadmin
    export PGHOST=127.0.0.1
    . $GPDB_DIR/install/greenplum_loaders_path.sh
    cd $GPDB_DIR/gpload2
    python_64 TEST_REMOTE.py
EOF
}

function run_remote_test() {
    source ./gpdb_src/gpAux/gpdemo/gpdemo-env.sh
    # Get a session id for different commit builds.
    SESSION_ID=`ssh -T -p $REMOTE_PORT $REMOTE_USER@$REMOTE_HOST 'echo \$\$'`

    # Save the session ID for next task in the job
    #  this dir/file is a volume to be exported
    #  from the task
    echo $SESSION_ID > session_id/session_id

    # Create working directory on remote machine.
    ssh -T -p $REMOTE_PORT $REMOTE_USER@$REMOTE_HOST > dir <<-EOF
    mkdir -p gpdb-compile/$SESSION_ID
    cd gpdb-compile/$SESSION_ID
    echo \$PWD
EOF

    GPDB_DIR=`cat dir`
    run_clients_test
    run_loaders_test
}

function _main() {

    if [ -z "$REMOTE_PORT" ]; then
        REMOTE_PORT=22
    fi

    time configure
    time install_gpdb
    time setup_gpadmin_user
    time make_cluster
    time import_remote_key
    time run_remote_test
}

_main "$@"
