#!/bin/bash

allow_connections_from_all_hosts() {
    echo host    all     all     0.0.0.0/0       trust >> /data/gpdata/master/gpseg-1/pg_hba.conf
    source /usr/local/greenplum-db-devel/greenplum_path.sh
    export PGPORT=5432
    export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1
    gpstop -u
}

create_map_file() {
    echo sdw1-2,`getent hosts sdw1-2 | cut -d ' ' -f1` > /tmp/source_map_file
}

setup_ssh_to_both_clusters() {
    cp -R cluster_env_files/.ssh /root/.ssh
    cat cluster_env_files2/.ssh/config >> /root/.ssh/config
    cat cluster_env_files2/.ssh/known_hosts >> /root/.ssh/known_hosts
    cp cluster_env_files2/.ssh/*private_key.pem /root/.ssh/.
}

exchange_keys() {
    scp -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -i cluster_env_files/private_key.pem cluster_env_files/private_key.pem gpadmin@$(cat cluster_env_files/etc_hostfile | awk 'NR==1{print $1}'):~/.ssh/
    scp -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -i cluster_env_files/private_key.pem cluster_env_files/private_key.pem gpadmin@$(cat cluster_env_files/etc_hostfile | awk 'NR==2{print $1}'):~/.ssh/
    scp -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -i cluster_env_files2/private_key.pem cluster_env_files2/private_key.pem gpadmin@$(cat cluster_env_files2/etc_hostfile | awk 'NR==1{print $1}'):~/.ssh/
    scp -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -i cluster_env_files2/private_key.pem cluster_env_files2/private_key.pem gpadmin@$(cat cluster_env_files2/etc_hostfile | awk 'NR==2{print $1}'):~/.ssh/
    cat cluster_env_files/public_key.openssh | ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -i cluster_env_files2/private_key.pem gpadmin@$(cat cluster_env_files2/etc_hostfile | awk 'NR==1{print $1}') "cat - >> .ssh/authorized_keys"
    cat cluster_env_files/public_key.openssh | ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -i cluster_env_files2/private_key.pem gpadmin@$(cat cluster_env_files2/etc_hostfile | awk 'NR==2{print $1}') "cat - >> .ssh/authorized_keys"
    cat cluster_env_files2/public_key.openssh | ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -i cluster_env_files/private_key.pem gpadmin@$(cat cluster_env_files/etc_hostfile | awk 'NR==1{print $1}') "cat - >> .ssh/authorized_keys"
    cat cluster_env_files2/public_key.openssh | ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -i cluster_env_files/private_key.pem gpadmin@$(cat cluster_env_files/etc_hostfile | awk 'NR==2{print $1}') "cat - >> .ssh/authorized_keys"
    cat cluster_env_files2/etc_hostfile | ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -i cluster_env_files/private_key.pem centos@$(cat cluster_env_files/etc_hostfile | awk 'NR==1{print $1}') "cat - | sudo tee -a /etc/hosts"
    cat cluster_env_files/etc_hostfile | ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -i cluster_env_files2/private_key.pem centos@$(cat cluster_env_files2/etc_hostfile | awk 'NR==2{print $1}') "cat - | sudo tee -a /etc/hosts"
}

setup_gptransfer() {
    yum install -y -d1 openssh openssh-clients epel-release
    setup_ssh_to_both_clusters
    exchange_keys
    ssh -t mdw "source /home/gpadmin/gpdb_src/concourse/scripts/transfer_utils.sh; create_map_file"
    ssh -t mdw-2 "source /home/gpadmin/gpdb_src/concourse/scripts/transfer_utils.sh; allow_connections_from_all_hosts"
}
