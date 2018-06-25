#!/bin/bash

set -xeuo pipefail

MAPR_SSH_OPTS="-i cluster_env_files/private_key.pem"
node_hostname="ccp-$(cat ./terraform*/name)-0"

get_device_name() {
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
    echo $(ssh -tt "${node_hostname}" "sudo bash -c \"readlink -f ${device_name}\"") | sed 's/\\r//g'
}

# modify gpadmin userid and group to match
# that one of concourse test container
modify_groupid_userid() {
    ssh -ttn "${node_hostname}" "sudo bash -c \"\
        usermod -u 500 gpadmin; \
        groupmod -g 501 gpadmin; \
        # find / -group 501 -exec chgrp -h foo {} \;; \
        # find / -user  500 -exec chown -h foo {} \;; \
    \""
}

install_java() {
    ssh -ttn "${node_hostname}" "sudo bash -c \"\
        yum install -y java-1.7.0-openjdk; \
    \""
}

enable_root_ssh_login() {
    ssh -ttn "${node_hostname}" "sudo bash -c \"\
        mkdir -p /root/.ssh/
        cp /home/centos/.ssh/authorized_keys /root/.ssh/; \
        sed -ri 's/PermitRootLogin no/PermitRootLogin yes/g' /etc/ssh/sshd_config; \
        service sshd restart; \
    \""
}

download_and_run_mapr_setup() {
    ssh -ttn "${node_hostname}" "sudo bash -c \"\
        cd /root; \
        wget http://package.mapr.com/releases/v5.2.0/redhat/mapr-setup; \
        chmod 755 mapr-setup; \
        ./mapr-setup; \
    \""
}

# create cluster configuration file
create_config_file() {
    local device_name=$1

    cat > /tmp/singlenode_config <<-EOF
[Control_Nodes]
$node_hostname: $device_name
[Data_Nodes]
[Client_Nodes]
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
EOF

    scp ${MAPR_SSH_OPTS} cluster_env_files/private_key.pem centos@"${node_hostname}":/tmp
    scp ${MAPR_SSH_OPTS} /tmp/singlenode_config centos@"${node_hostname}":/tmp
    ssh -ttn "${node_hostname}" "sudo bash -c \"\
        mv /tmp/singlenode_config /opt/mapr-installer/bin/singlenode_config; \
        chown root:root /opt/mapr-installer/bin/singlenode_config; \
    \""
}

run_quick_installer() {
    ssh -ttn "${node_hostname}" "sudo bash -c \"\
        /opt/mapr-installer/bin/install --user root --private-key /tmp/private_key.pem --quiet --cfg /opt/mapr-installer/bin/singlenode_config new; \
    \""
}

grant_top_level_write_permission() {
    ssh -ttn "${node_hostname}" "sudo bash -c \"\
        hadoop fs -chmod 777 /;\
    \""

}
setup_node() {
    local devicename
    devicename=$(get_device_name)

    echo "Device name: $devicename"

    modify_groupid_userid
    enable_root_ssh_login
    download_and_run_mapr_setup
    create_config_file "${devicename}"
    run_quick_installer
    grant_top_level_write_permission
}

setup_node
