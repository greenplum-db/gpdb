#!/bin/bash -l

set -eo pipefail

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

# Since we are cloning and building on remote machine,
# files won't be deleted as concourse container destroys.
# We have to clean everything for success build.
function cleanup() {
    local SESSION_ID=$1
    ssh -T -p $REMOTE_PORT $REMOTE_USER@$REMOTE_HOST <<- EOF
    rm -rf $GPDB_DIR
    rm -rf gpdb-compile/$SESSION_ID
EOF
}

function _main() {

    if [ -z "$REMOTE_PORT" ]; then
        REMOTE_PORT=22
    fi

    # Get session id from previous test task
    SESSION_ID=$(cat session_id/session_id)

    time setup_gpadmin_user
    time import_remote_key
    time cleanup $SESSION_ID
}

_main "$@"
