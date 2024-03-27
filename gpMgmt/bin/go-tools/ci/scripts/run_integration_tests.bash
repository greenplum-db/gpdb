#!/bin/bash

set -eux -o pipefail

ccp_src/scripts/setup_ssh_to_cluster.sh

scp cluster_env_files/hostfile_all cdw:/tmp
tar -xzf gp_binary/gp.tgz
scp gp cdw:/home/gpadmin/

ssh -n cdw "
    set -eux -o pipefail

    export PATH=/usr/local/go/bin:\$PATH
    source /usr/local/greenplum-db-devel/greenplum_path.sh

    chmod +x gp
    gpsync -f /tmp/hostfile_all gp =:/usr/local/greenplum-db-devel/bin/gp
    cd /home/gpadmin/gpdb_src/gpMgmt/bin/go-tools
    ./ci/scripts/generate_ssl_cert_multi_host.bash

    make integration FILE=/tmp/hostfile_all
"
