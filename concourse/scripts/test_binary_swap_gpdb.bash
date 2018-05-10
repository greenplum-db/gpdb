#!/bin/bash -l

set -eox pipefail

if [ "$USER" = gpadmin ]; then
    tmpdir=/tmp
else
    tmpdir=/opt
fi

function gen_env() {
    cat > $tmpdir/run_test.sh <<-EOF
        source /usr/local/greenplum-db-devel/greenplum_path.sh
        [ -e \${1}/gpdb_src/gpAux/gpdemo/gpdemo-env.sh ] && \
        source \${1}/gpdb_src/gpAux/gpdemo/gpdemo-env.sh
        cd "\${1}/gpdb_src/src/test/binary_swap"
        ./test_binary_swap.sh -b /tmp/local/greenplum-db-devel \
            -v "${BINARY_SWAP_VARIANT}" || (
            errcode=\$?
            find . -name regression.diffs \
            | while read diff; do
                cat <<EOF1

======================================================================
DIFF FILE: \$diff
----------------------------------------------------------------------

EOF1
                cat \$diff
              done
            exit \$errcode
        )
EOF

    chmod a+x $tmpdir/run_test.sh
}

function install_gpdb_binary_swap() {
    [ ! -d /tmp/local/greenplum-db-devel ] && mkdir -p /tmp/local/greenplum-db-devel
    tar -xzf binary_swap_gpdb/bin_gpdb.tar.gz -C /tmp/local/greenplum-db-devel
    sed -i -e "s/\/usr\/local/\/tmp\/local/" /tmp/local/greenplum-db-devel/greenplum_path.sh
    chown -R gpadmin:gpadmin /tmp/local
}

function run_test() {
    if [ "$USER" = gpadmin ]; then
        bash $tmpdir/run_test.sh $(pwd)
    else
        su - gpadmin -c "bash $tmpdir/run_test.sh $(pwd)"
    fi
}

function _main() {
    time install_gpdb_binary_swap
    time gen_env
    time run_test
}

_main "$@"
