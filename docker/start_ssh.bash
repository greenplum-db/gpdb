#!/bin/bash

set -ex

ssh_keyscan_for_user() {
  local user="${1}"
  local home_dir
  home_dir=$(eval echo "~${user}")

  {
    ssh-keyscan localhost
    ssh-keyscan 0.0.0.0
    ssh-keyscan github.com
  } >> "${home_dir}/.ssh/known_hosts"
}

/usr/sbin/sshd -D &

ssh_keyscan_for_user gpadmin
ssh_keyscan_for_user root

