#!/bin/bash

# Install needed packages. Please add to this list if you discover additional prerequisites
sudo yum group install -y "Development Tools"
INSTALL_PKGS="apr-devel bison bzip2-devel cmake3 epel-release flex gcc gcc-c++ krb5-devel libcurl-devel libevent-devel libkadm5 libxml2-devel libzstd-devel openssl-devel python39 python39-devel python39-psutil python3-pip perl-ExtUtils-MakeMaker.noarch perl-ExtUtils-Embed.noarch readline-devel rsync xerces-c-devel zlib-devel python3-psutil python3-pyyaml python3-psycopg2"

sudo yum install -y $INSTALL_PKGS

sudo yum --enablerepo=powertools install -y libyaml-devel

sudo yum install -y postgresql 
sudo yum install -y postgresql-devel

pip3.9 install -r python-dependencies.txt

#Remove the python3 softlink because on Rocky8 its pointing to python3.6 by default. We need python3.9 for gpdb7.
sudo rm -rf /usr/bin/python3
sudo ln -s /usr/bin/python3.9 /usr/bin/python3

#For all Greenplum Database host systems running RHEL, CentOs or Rocky8, SELinux must either be Disabled or configured to allow unconfined access to Greenplum processes, directories, and the gpadmin user.
setenforce 0
sudo tee -a /etc/selinux/config << EOF
SELINUX=disabled
EOF

#To prevent SELinux-related SSH authentication denials that could occur even with SELinux deactivated
sudo tee -a /etc/sssd/sssd.conf << EOF
selinux_provider=none
EOF

sudo systemctl stop firewalld.service

#Configure kernel settings so the system is optimized for Greenplum Database.
sudo tee -a /etc/sysctl.d/10-gpdb.conf << EOF
kernel.msgmax = 65536
kernel.msgmnb = 65536
kernel.msgmni = 2048
kernel.sem = 500 2048000 200 8192
kernel.shmmni = 1024
kernel.core_uses_pid = 1
kernel.core_pattern=/var/core/core.%h.%t
kernel.sysrq = 1
net.core.netdev_max_backlog = 2000
net.core.rmem_max = 4194304
net.core.wmem_max = 4194304
net.core.rmem_default = 4194304
net.core.wmem_default = 4194304
net.ipv4.tcp_rmem = 4096 4224000 16777216
net.ipv4.tcp_wmem = 4096 4224000 16777216
net.core.optmem_max = 4194304
net.core.somaxconn = 10000
net.ipv4.ip_forward = 0
net.ipv4.tcp_congestion_control = cubic
net.ipv4.tcp_tw_recycle = 0
net.core.default_qdisc = fq_codel
net.ipv4.tcp_mtu_probing = 0
net.ipv4.conf.all.arp_filter = 1
net.ipv4.conf.default.accept_source_route = 0
net.ipv4.ip_local_port_range = 10000 65535
net.ipv4.tcp_max_syn_backlog = 4096
net.ipv4.tcp_syncookies = 1
net.ipv4.ipfrag_high_thresh = 41943040
net.ipv4.ipfrag_low_thresh = 31457280
net.ipv4.ipfrag_time = 60
net.ipv4.ip_local_reserved_ports=65330
vm.overcommit_memory = 2
vm.overcommit_ratio = 95
vm.swappiness = 10
vm.dirty_expire_centisecs = 500
vm.dirty_writeback_centisecs = 100
vm.zone_reclaim_mode = 0
EOF

RAM_IN_KB=`cat /proc/meminfo | grep MemTotal | awk '{print $2}'`
RAM_IN_BYTES=$(($RAM_IN_KB*1024))
echo "vm.min_free_kbytes = $(($RAM_IN_BYTES*3/100/1024))" | sudo tee -a /etc/sysctl.d/10-gpdb.conf > /dev/null
echo "kernel.shmall = $(($RAM_IN_BYTES/2/4096))" | sudo tee -a /etc/sysctl.d/10-gpdb.conf > /dev/null
echo "kernel.shmmax = $(($RAM_IN_BYTES/2))" | sudo tee -a /etc/sysctl.d/10-gpdb.conf > /dev/null
if [ $RAM_IN_BYTES -le $((64*1024*1024*1024)) ]; then
    echo "vm.dirty_background_ratio = 3" | sudo tee -a /etc/sysctl.d/10-gpdb.conf > /dev/null
    echo "vm.dirty_ratio = 10" | sudo tee -a /etc/sysctl.d/10-gpdb.conf > /dev/null
else
    echo "vm.dirty_background_ratio = 0" | sudo tee -a /etc/sysctl.d/10-gpdb.conf > /dev/null
    echo "vm.dirty_ratio = 0" | sudo tee -a /etc/sysctl.d/10-gpdb.conf > /dev/null
    echo "vm.dirty_background_bytes = 1610612736 # 1.5GB" | sudo tee -a /etc/sysctl.d/10-gpdb.conf > /dev/null
    echo "vm.dirty_bytes = 4294967296 # 4GB" | sudo tee -a /etc/sysctl.d/10-gpdb.conf > /dev/null
fi

sudo sysctl -p

sudo tee -a /etc/security/limits.d/10-nproc.conf << EOF
* soft nofile 524288
* hard nofile 524288
* soft nproc 131072
* hard nproc 131072
* soft core unlimited
EOF


ulimit -n 65536 65536

