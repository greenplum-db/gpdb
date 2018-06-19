#!/bin/bash

MAPR_SSH_OPTS="-i cluster_env_files/private_key.pem"

get_device_name() {
    local node_hostname=$1
    local device_name="/dev/xvdb"

    # We check fdisk to see if there is an nvme disk because we cannot
    # rename the device name to /dev/xvdb like EBS or other ephemeral
    # disks. If we find an nvme disk, it will be at /dev/nvme0n1.
    local nvme
    nvme=$(ssh -tt "${node_hostname}" "sudo bash -c \"fdisk -l | grep /dev/nvme\"")
    ssh -tt "${node_hostname}" "[ -L /dev/disk/by-id/google-disk-for-gpdata ]"
    local google_disk_exit_code=$?

    if [[ "$nvme" == *"/dev/nvme"* ]]; then
        device_name="/dev/nvme0n1"
    elif [ "$google_disk_exit_code" = "0" ]; then
        device_name="/dev/disk/by-id/google-disk-for-gpdata"
    fi
    echo $device_name
}

# modify gpadmin userid and group to match
# that one of concourse test container
modify_groupid_userid() {
    local node_hostname=$1
    ssh -ttn "${node_hostname}" "sudo bash -c \"\
        usermod -u 500 gpadmin; \
        groupmod -g 501 gpadmin; \
        # find / -group 501 -exec chgrp -h foo {} \;; \
        # find / -user  500 -exec chown -h foo {} \;; \
    \""
}

install_java() {
    local node_hostname=$1
    ssh -ttn "${node_hostname}" "sudo bash -c \"\
        yum install -y java-1.7.0-openjdk; \
    \""
}

enable_root_ssh_login() {
    local node_hostname=$1
    ssh -ttn "${node_hostname}" "sudo bash -c \"\
        sed -ri 's/PermitRootLogin no/PermitRootLogin yes/g' /etc/ssh/sshd_config; \
        service sshd restart; \
    \""
}

download_and_run_mapr_setup() {
    local node_hostname=$1
    ssh -ttn "${node_hostname}" "sudo bash -c \"\
        cd /root; \
        wget http://package.mapr.com/releases/v5.2.0/redhat/mapr-setup; \
        chmod 755 mapr-setup; \
        ./mapr-setup; \
    \""
}

# create cluster configuration file
create_config_file() {
    local node_hostname=$1
    local device_name=$2

    cat > /tmp/singlenode_config <<-EOF
# Each Node section can specify nodes in the following format
# Hostname: disk1, disk2, disk3
# Specifying disks is optional. If not provided, the installer will use the values of 'disks' from the Defaults section
[Control_Nodes]
$node_hostname: $device_name
[Data_Nodes]
#data-node1.mydomain
#data-node2.mydomain: /dev/sdb, /dev/sdc, /dev/sdd
#data-node3.mydomain: /dev/sdd
#data-node4.mydomain: /dev/sdb, /dev/sdd
[Client_Nodes]
#client1.mydomain
#client2.mydomain
#client3.mydomain
[Options]
MapReduce1 = false
YARN = true
HBase = false
MapR-DB = true
ControlNodesAsDataNodes = true
WirelevelSecurity = false
LocalRepo = false
[Defaults]
ClusterName = mapr
User = mapr
Group = mapr
Password = mapr
UID = 2000
GID = 2000
Disks = $device_name
StripeWidth = 3
ForceFormat = false
CoreRepoURL = http://package.mapr.com/releases
EcoRepoURL = http://package.mapr.com/releases/ecosystem-5.x
Version = 5.2.0
MetricsDBHost =
MetricsDBUser =
MetricsDBPassword =
MetricsDBSchema =

#[Spark]
#SparkVersion = 0.9.1
#SparkMasters = control-node1.mydomain, control-node2.mydomain
#SparkSlaves = data-node1.mydomain, data-node2.mydomain, data-node3.mydomain
#SparkMem = 2
#SparkWorkerMem = 1
#SparkDaemonMem = 16

#[Hive]
#HiveVersion = 0.12
#HiveServers = control-node1.mydomain
#HiveMetaStore = control-node2.mydomain
#HiveClients = client-node1.mydomain, data-node3.mydomain
EOF

    scp ${MAPR_SSH_OPTS} cluster_env_files/private_key.pem centos@"${node_hostname}":/tmp
    scp ${MAPR_SSH_OPTS} /tmp/singlenode_config centos@"${node_hostname}":/tmp
    ssh -ttn "${node_hostname}" "sudo bash -c \"\
        mv /tmp/singlenode_config /opt/mapr-installer/bin/singlenode_config; \
        chown root:root /opt/mapr-installer/bin/singlenode_config; \
    \""
}

# run quick installer
run_quick_installer() {
    local node_hostname=$1
    ssh -ttn "${node_hostname}" "sudo bash -c \"\
        /opt/mapr-installer/bin/install --user root --private-key /tmp/private_key.pem --quiet --cfg /opt/mapr-installer/bin/singlenode_config new; \
        echo -e "\n" \
    \""
}

setup_node() {
    set -x
    local nodename
    local devicename

    CCP_CLUSTER_NAME=$(cat ./terraform*/name)

    nodename="ccp-${CCP_CLUSTER_NAME}-0"
    devicename=$(get_device_name "${nodename}")

    echo "Device name: $devicename"

    modify_groupid_userid "${nodename}"
    # Java is installed by mapr automatically
#    install_java "${nodename}"
    enable_root_ssh_login "${nodename}"
    download_and_run_mapr_setup "${nodename}"
    create_config_file "${nodename}" "${devicename}"
    run_quick_installer "${nodename}"
}

#NUMBER_OF_NODES=$1

#if [ -z "${NUMBER_OF_NODES}" ]; then
#    echo "Number of nodes must be supplied to this script"
#    exit 1
#fi


setup_node

#for ((i=0; i < NUMBER_OF_NODES; ++i)); do
#    _setup_node ccp-"${CLUSTER_NAME}-${i}" &
#done

#wait
