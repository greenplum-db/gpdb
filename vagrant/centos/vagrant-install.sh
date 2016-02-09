#!/bin/bash

# GPDB Kernel Settings
sudo rm -f /etc/sysctl.d/gpdb.conf
printf "kernel.shmmax = 500000000\n"                     >> /etc/sysctl.d/gpdb.conf
printf "kernel.shmmni = 4096\n"                          >> /etc/sysctl.d/gpdb.conf
printf "kernel.shmall = 4000000000\n"                    >> /etc/sysctl.d/gpdb.conf
printf "kernel.sem = 250 512000 100 2048\n"              >> /etc/sysctl.d/gpdb.conf
printf "kernel.sysrq = 1\n"                              >> /etc/sysctl.d/gpdb.conf
printf "kernel.core_uses_pid = 1\n"                      >> /etc/sysctl.d/gpdb.conf
printf "kernel.msgmnb = 65536\n"                         >> /etc/sysctl.d/gpdb.conf
printf "kernel.msgmax = 65536\n"                         >> /etc/sysctl.d/gpdb.conf
printf "kernel.msgmni = 2048\n"                          >> /etc/sysctl.d/gpdb.conf
printf "net.ipv4.tcp_syncookies = 1\n"                   >> /etc/sysctl.d/gpdb.conf
printf "net.ipv4.ip_forward = 0\n"                       >> /etc/sysctl.d/gpdb.conf
printf "net.ipv4.conf.default.accept_source_route = 0\n" >> /etc/sysctl.d/gpdb.conf
printf "net.ipv4.tcp_tw_recycle = 1\n"                   >> /etc/sysctl.d/gpdb.conf
printf "net.ipv4.tcp_max_syn_backlog = 4096\n"           >> /etc/sysctl.d/gpdb.conf
printf "net.ipv4.conf.all.arp_filter = 1\n"              >> /etc/sysctl.d/gpdb.conf
printf "net.ipv4.ip_local_port_range = 1025 65535\n"     >> /etc/sysctl.d/gpdb.conf
printf "net.core.netdev_max_backlog = 10000\n"           >> /etc/sysctl.d/gpdb.conf
printf "net.core.rmem_max = 2097152\n"                   >> /etc/sysctl.d/gpdb.conf
printf "net.core.wmem_max = 2097152\n"                   >> /etc/sysctl.d/gpdb.conf
printf "vm.overcommit_memory = 2\n"                      >> /etc/sysctl.d/gpdb.conf
sudo sysctl -p /etc/sysctl.d/gpdb.conf

# GPDB Kernel Limits
sudo rm -f /etc/security/limits.d/gpdb.conf
printf "* soft nofile 65536"                     >> /etc/security/limits.d/gpdb.conf
printf "* hard nofile 65536"                     >> /etc/security/limits.d/gpdb.conf
printf "* soft nproc 131072"                     >> /etc/security/limits.d/gpdb.conf
printf "* hard nproc 131072"                     >> /etc/security/limits.d/gpdb.conf

# Do not destroy user context on logout
sudo bash -c 'echo "RemoveIPC=no" >> /etc/systemd/logind.conf'
sudo service systemd-logind restart

# Installation
sudo -H -u gpadmin /bin/bash /vagrant/vagrant-install-gpdb.sh
