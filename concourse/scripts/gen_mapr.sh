#!/bin/bash

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
# Specifying disks is optional. If not provided, the installer will use the values of 'disks' from the Defau
lts section
[Control_Nodes]
#control-node1.mydomain: /dev/disk1, /dev/disk2, /dev/disk3
#control-node2.mydomain: /dev/disk3, /dev/disk9
#control-node3.mydomain: /dev/sdb, /dev/sdc, /dev/sdd
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
ClusterName = my.cluster.com
User = mapr
Group = mapr
Password = default_password
UID = 2000
GID = 2000
Disks = $device_name
StripeWidth = 3
ForceFormat = false
CoreRepoURL = http://package.mapr.com/releases
EcoRepoURL = http://package.mapr.com/releases/ecosystem-4.x
Version = 4.0.2
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

    scp /tmp/singlenode_config "${node_hostname}":/opt/mapr-installer/bin/singlenode_config
}

# run quick installer
run_quick_installer() {
    local node_hostname=$1
    ssh -ttn "${node_hostname}" "sudo bash -c \"\
        /opt/mapr-installer/bin/install --cfg /opt/mapr-installer/bin/singlenode_config new; \
    \""
}

_setup_node() {
    set -x
    local nodename
    local devicename

    nodename=$1
    devicename=$(get_device_name "${nodename}")

    modify_groupid_userid "${nodename}"
    install_java "${nodename}"
    download_and_run_mapr_setup "${nodename}"
    create_config_file "${nodename}" "${devicename}"
    run_quick_installer "${nodename}"
}

NUMBER_OF_NODES=$1

if [ -z "${NUMBER_OF_NODES}" ]; then
    echo "Number of nodes must be supplied to this script"
    exit 1
fi

CLUSTER_NAME=$(cat ./terraform*/name)

for ((i=0; i < NUMBER_OF_NODES; ++i)); do
    _setup_node ccp-"${CLUSTER_NAME}-${i}" &
done
