#!/bin/bash -l
#shellcheck disable=1090,1091,2129,2035

set -eo pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

function setup_gpadmin_user() {
    ./gpdb_src/concourse/scripts/setup_gpadmin_user.bash "${TEST_OS}"
}

function configure_gpdb_ssl() {
    cp ./gpdb_src/src/test/ssl/ssl/server.crt "${MASTER_DATA_DIRECTORY}"
    cp ./gpdb_src/src/test/ssl/ssl/server.key "${MASTER_DATA_DIRECTORY}"
    cp ./gpdb_src/src/test/ssl/ssl/root+server_ca.crt "${MASTER_DATA_DIRECTORY}"
    chmod 600 "${MASTER_DATA_DIRECTORY}/server.key"

    PG_CONF="${MASTER_DATA_DIRECTORY}/postgresql.conf"
    echo "ssl=on" >> "${PG_CONF}"
    echo "ssl_ca_file='root+server_ca.crt'">> "${PG_CONF}"
    echo "ssl_cert_file='server.crt'">> "${PG_CONF}"
    echo "ssl_key_file='server.key'">> "${PG_CONF}"

    PG_HBA="${MASTER_DATA_DIRECTORY}/pg_hba.conf"
    echo "hostssl   all gpadmin 0.0.0.0/0   trust">> "${PG_HBA}"
    gpstop -ar
}

# Get ssh private key from REMOTE_KEY, which is assumed to
# be encode in base64. We can't pass the key content directly
# since newline doesn't work well for env variable.
function import_remote_key() {
    echo -n "${REMOTE_KEY}" | base64 -d > ~/remote.key
    chmod 400 ~/remote.key

    eval "$(ssh-agent -s)"
    ssh-add ~/remote.key

    # Scan for target server's public key, append port number
    # maybe run with ssh -o StrictHostKeyChecking=no?
    mkdir -p ~/.ssh
    ssh-keyscan -p "${REMOTE_PORT}" "${REMOTE_HOST}" > ~/.ssh/known_hosts
}

function run_remote_test() {
    source ./gpdb_src/gpAux/gpdemo/gpdemo-env.sh
    #restart gpdb with ssl
    export -f configure_gpdb_ssl
    /bin/sh -c "su gpadmin -c configure_gpdb_ssl"

    scp -P "${REMOTE_PORT}" ./gpdb_src/concourse/scripts/windows_remote_test.ps1 "${REMOTE_USER}@${REMOTE_HOST}:"
    scp -P "${REMOTE_PORT}" ./bin_gpdb_clients_windows/*.msi "${REMOTE_USER}@${REMOTE_HOST}:"
    scp -P "${REMOTE_PORT}" ./bin_gpdb_clients_windows/*.exe "${REMOTE_USER}@${REMOTE_HOST}:"

    ssh -T -R"${PGPORT}:127.0.0.1:${PGPORT}" -p "${REMOTE_PORT}" "${REMOTE_USER}@${REMOTE_HOST}" 'powershell < windows_remote_test.ps1'

    scp -P "${REMOTE_PORT}" -r ./gpdb_src/gpMgmt/bin/gpload_test/gpload2 "${REMOTE_USER}@${REMOTE_HOST}:."
    scp -P "${REMOTE_PORT}" ./gpdb_src/src/test/regress/*.pl "${REMOTE_USER}@${REMOTE_HOST}:./gpload2"
    scp -P "${REMOTE_PORT}" ./gpdb_src/src/test/regress/*.pm "${REMOTE_USER}@${REMOTE_HOST}:./gpload2"
    scp -P "${REMOTE_PORT}" ./gpdb_src/concourse/scripts/ic_gpdb_remote_windows.bat "${REMOTE_USER}@${REMOTE_HOST}:"

    ssh -T -R"${PGPORT}:127.0.0.1:${PGPORT}" -L8081:127.0.0.1:8081 -L8082:127.0.0.1:8082 -p "${REMOTE_PORT}" "${REMOTE_USER}@${REMOTE_HOST}" "ic_gpdb_remote_windows.bat ${PGPORT}"
}

function create_cluster() {
    export CONFIGURE_FLAGS="--enable-gpfdist --with-openssl"
    yum install -y openssl-devel
    time install_and_configure_gpdb
    time setup_gpadmin_user
    export WITH_MIRRORS=false
    time make_cluster
}

function _main() {
    if [[ -z "${REMOTE_PORT}" ]]; then
        REMOTE_PORT=22
    fi
    yum install -y jq
    REMOTE_HOST=$(jq -r '."gpdb-clients-ip"' terraform_windows/metadata)
    export REMOTE_HOST

    pushd bin_gpdb
        mv *.tar.gz bin_gpdb.tar.gz
    popd

    pushd bin_gpdb_clients_windows
        tar xzvf *.tar.gz
    popd

    time create_cluster
    time import_remote_key
    time run_remote_test

    cp bin_gpdb_clients_windows/*.msi bin_gpdb_clients_windows_rc/
    pushd bin_gpdb_clients_windows_rc
        VERSION=$(cat ../bin_gpdb_clients_windows/version)
        mv greenplum-clients-x86_64.msi "greenplum-clients-${VERSION}-x86_64.msi"
    popd
}

_main "$@"
