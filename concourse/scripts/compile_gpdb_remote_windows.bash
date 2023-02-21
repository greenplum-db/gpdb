#!/bin/bash
#shellcheck disable=2153,2087,2035,2140

set -eox pipefail

ROOT_DIR=$(pwd)

# Get ssh private key from REMOTE_KEY, which is assumed to
# be encode in base64. We can't pass the key content directly
# since newline doesn't work well for env variable.
function setup_ssh_keys() {
    # Setup ssh keys
    set +x
    echo -n "${REMOTE_KEY}" | base64 -d > ~/remote.key
    set -x
    chmod 400 ~/remote.key

    eval "$(ssh-agent -s)"
    ssh-add ~/remote.key

    # Scan for target server's public key, append port number
    mkdir -p ~/.ssh
    ssh-keyscan -p "${REMOTE_PORT}" "${REMOTE_HOST}" > ~/.ssh/known_hosts
}

# All the variables in this funciton are GLOBAL environment variables.
# We set up the runtime vars here.
function remote_setup() {
    # Get a session id for different commit builds.
    SESSION_ID=$(date +%Y%m%d%H%M%S.%N)

    WORK_DIR="C:\\Users\\buildbot\\${SESSION_ID}"
    
    # Get git information from local repo(concourse gpdb_src input)
    pushd gpdb_src
        GIT_URI=$(git config --get remote.origin.url)
        GIT_COMMIT=$(git rev-parse HEAD)
        GPDB_VERSION=$(./getversion --short)
        GIT_TAG=$(echo $GPDB_VERSION | grep -E -o '[0-9]\.[0-9]+\.[0-9]+')
    popd
}

# Since we're cloning in a different machine, maybe there's 
# new commit pushed to the same repo. We need to reset to the
# same commit to current concourse build.
function remote_clone() {
    # Connect to remote windows server powershell environment and execute
    # the specified commands
    ssh -A -T -p "${REMOTE_PORT}" "${REMOTE_USER}"@"${REMOTE_HOST}" <<- EOF
    mkdir "${WORK_DIR}"
    cd "${WORK_DIR}"
    git clone "${GIT_URI}" gpdb_src
    cd gpdb_src
    git reset --hard "${GIT_COMMIT}"
    git submodule update --init --recursive
EOF
}

function remote_compile() {
    # Connect to remote windows server powershell environment and execute
    # the specified commands
    ssh -T -p "${REMOTE_PORT}" "${REMOTE_USER}"@"${REMOTE_HOST}" <<- EOF
    cd "${WORK_DIR}\gpdb_src"
    set WORK_DIR=${WORK_DIR}
    set GPDB_VERSION=${GIT_TAG}
    concourse\scripts\compile_gpdb_remote_windows.bat
EOF
}

function download() {
    pushd "$ROOT_DIR/gpdb_artifacts/"
        scp -P "${REMOTE_PORT}" -q "${REMOTE_USER}"@"${REMOTE_HOST}":"${WORK_DIR}/*.msi" ./
        scp -P "${REMOTE_PORT}" -q "${REMOTE_USER}"@"${REMOTE_HOST}":"${WORK_DIR}/*.exe" ./
        echo "${GPDB_VERSION}" > version
        tar cvzf greenplum-clients-x86_64.tar.gz *
    popd
}

# Since we are cloning and building on remote machine,
# files won't be deleted as concourse container destroys.
# We have to clean everything for success build.
function cleanup() {
    # Connect to remote windows server powershell environment and execute
    # the specified commands
    ssh -T -p "${REMOTE_PORT}" "${REMOTE_USER}"@"${REMOTE_HOST}" <<- EOF
    rmdir /S /Q "${WORK_DIR}"
EOF
}

function _main() {

    if [[ -z "${REMOTE_PORT}" ]]; then
        REMOTE_PORT=22
    fi

    time setup_ssh_keys
    time remote_setup
    trap cleanup EXIT
    time remote_clone
    time remote_compile
    time download
}

_main "$@"
