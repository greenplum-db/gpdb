#!/bin/bash

set -e -u

source $(dirname "$0")/common.sh

main() {
  INSTANCE_IDS=($(cat instance_ids/instance_ids.txt))

  check_config

  log "Deleting instaces : ${INSTANCE_IDS}"
  ec2-terminate-instances ${INSTANCE_IDS}

  wait_until_status "terminated"
  log "Instances terminated successfully"
}

main "$@"
