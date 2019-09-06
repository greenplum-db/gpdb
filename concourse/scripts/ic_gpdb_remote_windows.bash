#!/bin/bash -l
#shellcheck disable=1090,1091,2129,2035

set -eo pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

export DEFAULT_REALM=GPDB.KRB

function setup_gpadmin_user() {
    ./gpdb_src/concourse/scripts/setup_gpadmin_user.bash "${TEST_OS}"
}

function configure_gpdb_ssl_kerberos() {
    cp ./gpdb_src/src/test/ssl/ssl/server.crt "${MASTER_DATA_DIRECTORY}"
    cp ./gpdb_src/src/test/ssl/ssl/server.key "${MASTER_DATA_DIRECTORY}"
    cp ./gpdb_src/src/test/ssl/ssl/root+server_ca.crt "${MASTER_DATA_DIRECTORY}"
    chmod 600 "${MASTER_DATA_DIRECTORY}/server.key"

    PG_CONF="${MASTER_DATA_DIRECTORY}/postgresql.conf"
    echo "ssl=on" >> "${PG_CONF}"
    echo "ssl_ca_file='root+server_ca.crt'">> "${PG_CONF}"
    echo "ssl_cert_file='server.crt'">> "${PG_CONF}"
    echo "ssl_key_file='server.key'">> "${PG_CONF}"

    gpconfig -c krb_server_keyfile -v  '/home/gpadmin/gpdb-server-krb5.keytab'

    PG_HBA="${MASTER_DATA_DIRECTORY}/pg_hba.conf"
    echo "hostssl   all gpadmin 0.0.0.0/0   trust">> "${PG_HBA}"
    echo "host all all 0.0.0.0/0 gss include_realm=0 krb_realm=${DEFAULT_REALM}" >> "${PG_HBA}"
    gpstop -ar

    psql postgres -c 'create role "user1/127.0.0.1" superuser login'
}

function setup_kerberos() {

# Setup krb5.conf
cat > /etc/krb5.conf <<EOF
[logging]
 default = FILE:/var/log/krb5libs.log
 kdc = FILE:/var/log/krb5kdc.log
 admin_server = FILE:/var/log/kadmind.log
[libdefaults]
 default_realm = ${DEFAULT_REALM}
 dns_lookup_realm = false
 dns_lookup_kdc = false
 ticket_lifetime = 24h
 renew_lifetime = 7d
 forwardable = true
 rdns = false
[realms]
 ${DEFAULT_REALM} = {
  kdc = 127.0.0.1
  admin_server = 127.0.0.1
 }
[domain_realm]
 .gpdb.krb = ${DEFAULT_REALM}
 gpdb.krb = ${DEFAULT_REALM}
EOF

# Start KDC on master node
kdb5_util create -s -r ${DEFAULT_REALM} -P changeme

kadmin.local -q "addprinc -pw changeme root/admin"

krb5kdc

kadmin.local -q "addprinc -randkey user1/127.0.0.1@${DEFAULT_REALM}"
kadmin.local -q "addprinc -randkey postgres/127.0.0.1@${DEFAULT_REALM}"

# Gernerate keys
rm -rf /home/gpadmin/gpdb-krb5.keytab
kadmin.local -q "xst -norandkey -k /home/gpadmin/gpdb-client-krb5.keytab user1/127.0.0.1@${DEFAULT_REALM}"
kadmin.local -q "xst -norandkey -k /home/gpadmin/gpdb-server-krb5.keytab postgres/127.0.0.1@${DEFAULT_REALM}"
chown gpadmin:gpadmin /home/gpadmin/gpdb-*-krb5.keytab
chmod 400 /home/gpadmin/gpdb-*-krb5.keytab
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
    set -eo pipefail
    if [ -f /opt/gcc_env.sh ]; then
        source /opt/gcc_env.sh
    fi

    scp -P "${REMOTE_PORT}" ./gpdb_src/concourse/scripts/windows_remote_test.ps1 "${REMOTE_USER}@${REMOTE_HOST}:"
    scp -P "${REMOTE_PORT}" ./bin_gpdb_clients_windows/*.msi "${REMOTE_USER}@${REMOTE_HOST}:"
    scp -P "${REMOTE_PORT}" ./bin_gpdb_clients_windows/*.exe "${REMOTE_USER}@${REMOTE_HOST}:"

    ssh -T -R"${PGPORT}:127.0.0.1:${PGPORT}" -p "${REMOTE_PORT}" "${REMOTE_USER}@${REMOTE_HOST}" 'powershell < windows_remote_test.ps1'

    scp -P "${REMOTE_PORT}" -r ./gpdb_src/gpMgmt/bin/gpload_test/gpload2 "${REMOTE_USER}@${REMOTE_HOST}:."
    scp -P "${REMOTE_PORT}" ./gpdb_src/src/test/regress/*.pl "${REMOTE_USER}@${REMOTE_HOST}:./gpload2"
    scp -P "${REMOTE_PORT}" ./gpdb_src/src/test/regress/*.pm "${REMOTE_USER}@${REMOTE_HOST}:./gpload2"
    scp -P "${REMOTE_PORT}" ./gpdb_src/concourse/scripts/ic_gpdb_remote_windows.bat "${REMOTE_USER}@${REMOTE_HOST}:"

    scp -P "${REMOTE_PORT}" /home/gpadmin/gpdb-client-krb5.keytab "${REMOTE_USER}@${REMOTE_HOST}:./gpdb-krb5.keytab"
    scp -P "${REMOTE_PORT}" /etc/krb5.conf "${REMOTE_USER}@${REMOTE_HOST}:./krb5.ini"

    ssh -T -R"${PGPORT}:127.0.0.1:${PGPORT}" -R88:127.0.0.1:88 -L8081:127.0.0.1:8081 -L8082:127.0.0.1:8082 -p "${REMOTE_PORT}" "${REMOTE_USER}@${REMOTE_HOST}" "ic_gpdb_remote_windows.bat ${PGPORT}"
    # run gpfdist test
    pushd gpdb_src/src/test/regress
    make pg_regress
    popd
    pushd gpdb_src/src/bin/gpfdist/remote_regress
    echo "export REMOTE_HOST=${REMOTE_HOST}" > ssh_remote_env.sh
    echo "export REMOTE_PORT=${REMOTE_PORT}" >> ssh_remote_env.sh
    echo "export REMOTE_USER=${REMOTE_USER}" >> ssh_remote_env.sh
    echo "export REMOTE_KEY=~/remote.key" >> ssh_remote_env.sh
    make installcheck_win
    popd
}

function install_packages {
    yum install -y jq openssl-devel krb5-server krb5-libs krb5-auth-dialog krb5-workstation
}

function create_cluster() {
    export CONFIGURE_FLAGS="--enable-gpfdist --with-openssl"
    time install_and_configure_gpdb
    export WITH_MIRRORS=false
    time make_cluster
}

function gpadmin_run_tests(){
    pushd "${1}"
    REMOTE_PORT=${2}
    REMOTE_USER=${3}
    if [ -z "$REMOTE_PORT" ]; then
        REMOTE_PORT=22
    fi
    REMOTE_HOST=$(jq -r '."gpdb-clients-ip"' terraform_windows/metadata)
    export REMOTE_HOST
    export REMOTE_PORT
    export REMOTE_USER
    source ./gpdb_src/gpAux/gpdemo/gpdemo-env.sh
    source /usr/local/greenplum-db-devel/greenplum_path.sh
    configure_gpdb_ssl_kerberos
    time import_remote_key
    time run_remote_test
}

function _main() {
    export -f configure_gpdb_ssl_kerberos 
    export -f import_remote_key
    export -f run_remote_test
    export -f gpadmin_run_tests
    pushd bin_gpdb
        mv *.tar.gz bin_gpdb.tar.gz
    popd

    pushd bin_gpdb_clients_windows
        tar xzvf *.tar.gz
    popd

    time install_packages
    time setup_gpadmin_user
    time setup_kerberos
    time create_cluster
    su gpadmin -c 'gpadmin_run_tests $(pwd) "${REMOTE_PORT}" "${REMOTE_USER}"'
    cp bin_gpdb_clients_windows/*.msi bin_gpdb_clients_windows_rc/
    pushd bin_gpdb_clients_windows_rc
        VERSION=$(cat ../bin_gpdb_clients_windows/version)
        mv greenplum-clients-x86_64.msi "greenplum-clients-${VERSION}-x86_64.msi"
    popd
}

_main "$@"
