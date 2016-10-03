#!/bin/bash

set -e -u

SYSCTL_HEAD="# BEGIN GENERATED CONTENT"
SYSCTL_TAIL="# END GENERATED CONTENT"
SYSCTL="$SYSCTL_HEAD
kernel.hostname = ${HOST}
kernel.shmmax = 500000000
kernel.shmmni = 4096
kernel.shmall = 4000000000
kernel.sem = 250 512000 100 2048
kernel.sysrq = 1
kernel.core_uses_pid = 1
kernel.msgmnb = 65536
kernel.msgmax = 65536
kernel.msgmni = 2048
net.ipv4.tcp_syncookies = 1
net.ipv4.ip_forward = 0
net.ipv4.conf.default.accept_source_route = 0
net.ipv4.tcp_tw_recycle = 1
net.ipv4.tcp_max_syn_backlog = 4096
net.ipv4.conf.all.arp_filter = 1
net.ipv4.ip_local_port_range = 1025 65535
net.ipv6.conf.all.disable_ipv6 = 1
net.ipv6.conf.default.disable_ipv6 = 1
net.core.netdev_max_backlog = 10000
net.core.rmem_max = 2097152
net.core.wmem_max = 2097152
vm.overcommit_memory = 2
vm.overcommit_ratio = 100
$SYSCTL_TAIL"

KERNEL="elevator=deadline transparent_hugepage=never"

main() {
  if [[ -z ${VERIFY:-} ]]; then
    apply
  else
    verify
  fi
}

apply() {
  echo "Configuring Kernel"

  sed -i -e "/$SYSCTL_HEAD/,/$SYSCTL_TAIL/d" /etc/sysctl.conf
  echo "$SYSCTL" >> /etc/sysctl.conf  # Settings for next reboot
  echo "$SYSCTL" | /sbin/sysctl -p -  # Current settings

  sed -i -r "s/kernel(.+)/kernel\1 $KERNEL/" /boot/grub/grub.conf
  echo never > /sys/kernel/mm/transparent_hugepage/enabled
  for BLOCKDEV in /sys/block/*/queue/scheduler; do
    echo deadline > "$BLOCKDEV"
  done

  echo "Kernel configuration done."
}

verify() {
  echo "Verifying changed sysctl params"
  sysctl $(echo "$SYSCTL" | sed -e '/^#/d' | cut -f1 -d ' ')
}

main "$@"
