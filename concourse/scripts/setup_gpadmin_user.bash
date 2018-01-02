#!/bin/bash

# Based on install_hawq_toolchain.bash in Pivotal-DataFabric/ci-infrastructure repo

set -euxo pipefail
setup_ssh_for_user() {
  local user="${1}"
  local home_dir
  home_dir=$(eval echo "~${user}")

  mkdir -p "${home_dir}"/.ssh
  touch "${home_dir}/.ssh/authorized_keys" "${home_dir}/.ssh/known_hosts" "${home_dir}/.ssh/config"
  ssh-keygen -t rsa -N "" -f "${home_dir}/.ssh/id_rsa"
  cat "${home_dir}/.ssh/id_rsa.pub" >> "${home_dir}/.ssh/authorized_keys"
  chmod 0600 "${home_dir}/.ssh/authorized_keys"
  cat << 'NOROAMING' >> "${home_dir}/.ssh/config"
Host *
  UseRoaming no
NOROAMING
  chown -R "${user}" "${home_dir}/.ssh"
}

ssh_keyscan_for_user() {
  local user="${1}"
  local home_dir
  home_dir=$(eval echo "~${user}")

  {
    ssh-keyscan localhost
    ssh-keyscan 0.0.0.0
    ssh-keyscan `hostname`
  } >> "${home_dir}/.ssh/known_hosts"
}

transfer_ownership() {
    chmod a+w gpdb_src
    find gpdb_src -type d -exec chmod a+w {} \;
    # Needed for the gpload test
    [ -f gpdb_src/gpMgmt/bin/gpload_test/gpload2/data_file.csv ] && chown gpadmin:gpadmin gpdb_src/gpMgmt/bin/gpload_test/gpload2/data_file.csv
    [ -d /usr/local/gpdb ] && chown -R gpadmin:gpadmin /usr/local/gpdb
    [ -d /usr/local/greenplum-db-devel ] && chown -R gpadmin:gpadmin /usr/local/greenplum-db-devel
    chown -R gpadmin:gpadmin /home/gpadmin
}

set_limits() {
  # Currently same as what's recommended in install guide
  if [ -d /etc/security/limits.d ]; then
    cat > /etc/security/limits.d/gpadmin-limits.conf <<-EOF
		gpadmin soft core unlimited
		gpadmin soft nproc 131072
		gpadmin soft nofile 65536
	EOF
  fi
  # Print now effective limits for gpadmin
  su gpadmin -c 'ulimit -a'
}

setup_gpadmin_user() {
  groupadd supergroup
  case "$TEST_OS" in
    sles)
      groupadd gpadmin
      /usr/sbin/useradd -G gpadmin,supergroup,tty gpadmin
      ;;
    centos)
      /usr/sbin/useradd -G supergroup,tty gpadmin
      ;;
    ubuntu)
      /usr/sbin/useradd -G supergroup,tty gpadmin -s /bin/bash
      ;;
    *) echo "Unknown OS: $TEST_OS"; exit 1 ;;
  esac
  echo -e "password\npassword" | passwd gpadmin
  setup_ssh_for_user gpadmin
  transfer_ownership
  set_limits
}

setup_sshd() {
  if [ ! "$TEST_OS" = 'ubuntu' ]; then
    test -e /etc/ssh/ssh_host_key || ssh-keygen -f /etc/ssh/ssh_host_key -N '' -t rsa1
  fi
  test -e /etc/ssh/ssh_host_rsa_key || ssh-keygen -f /etc/ssh/ssh_host_rsa_key -N '' -t rsa
  test -e /etc/ssh/ssh_host_dsa_key || ssh-keygen -f /etc/ssh/ssh_host_dsa_key -N '' -t dsa

  # For Centos 7, disable looking for host key types that older Centos versions don't support.
  sed -ri 's@^HostKey /etc/ssh/ssh_host_ecdsa_key$@#&@' /etc/ssh/sshd_config
  sed -ri 's@^HostKey /etc/ssh/ssh_host_ed25519_key$@#&@' /etc/ssh/sshd_config

  # See https://gist.github.com/gasi/5691565
  sed -ri 's/UsePAM yes/UsePAM no/g' /etc/ssh/sshd_config
  # Disable password authentication so builds never hang given bad keys
  sed -ri 's/PasswordAuthentication yes/PasswordAuthentication no/g' /etc/ssh/sshd_config

  setup_ssh_for_user root

  if [ "$TEST_OS" = 'ubuntu' ]; then
    mkdir -p /var/run/sshd
    chmod 0755 /var/run/sshd
  fi

  /usr/sbin/sshd

  ssh_keyscan_for_user root
  ssh_keyscan_for_user gpadmin
}

determine_os() {
  if [ -f /etc/redhat-release ] ; then
    echo "centos"
    return
  fi
  if [ -f /etc/os-release ] && grep -q '^NAME=.*SLES' /etc/os-release ; then
    echo "sles"
    return
  fi
  if lsb_release -a | grep -q 'Ubuntu' ; then
    echo "ubuntu"
    return
  fi
  echo "Could not determine operating system type" >/dev/stderr
  exit 1
}

# This might no longer be necessary, as the centos7 base image has been updated
# with ping's setcap set properly, although it would need to be verified to work
# for other OSs used by Concourse.
# https://github.com/Pivotal-DataFabric/toolsmiths-images/pull/27
workaround_before_concourse_stops_stripping_suid_bits() {
  chmod u+s /bin/ping
}

_main() {
  TEST_OS=$(determine_os)
  setup_gpadmin_user
  setup_sshd
  workaround_before_concourse_stops_stripping_suid_bits
}

_main "$@"
