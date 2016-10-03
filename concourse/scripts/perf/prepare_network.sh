#!/bin/bash

set -e

HOSTFILE_HEAD="# BEGIN GENERATED CONTENT"
HOSTFILE_TAIL="# END GENERATED CONTENT"

main() {
  if [[ -z ${VERIFY:-} ]]; then
    apply
  else
    verify
  fi
}

apply() {
  echo "Configuring Network"

  sed -i -e "/$HOSTFILE_HEAD/,/$HOSTFILE_TAIL/d" /etc/hosts
  echo "$HOSTFILE_HEAD" >> /etc/hosts
  echo "${PRIVATE_IP} ${HOST}" >> /etc/hosts
  echo "$HOSTFILE_TAIL" >> /etc/hosts

  update_known_hosts

  echo "Configuration Done"
}

verify() {
  echo "Verifying network configuration"
  cat /etc/hosts
  cat ~/.ssh/known_hosts
}

update_known_hosts() {

    for U in root gpadmin; do
      su $U -c "ssh-keygen -R ${HOST}" || true

      su $U -c "ssh-keyscan ${HOST} >> ~/.ssh/known_hosts"

      su $U -c "rm -f ~/.ssh/known_hosts.old"
    done
}

main "$@"
