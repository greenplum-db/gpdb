log() {
  echo -e "$@"
}

remote_run() {
  local IP="$1"
  local KEYFILE="${AWS_KEYPAIR}.pem"

  ssh -i "${KEYFILE}" -t -t ${SSH_USER}@${IP} "${@:2}"
}

remote_run_script() {
  local IP="$1"
  local USER="$2"
  local EXPORTS="$3"
  local KEYFILE="${AWS_KEYPAIR}.pem"
  local SCRIPT_BODY=$(printf "%q" "${EXPORTS}; $(cat $4)")

  ssh -i "${KEYFILE}" -t -t ${SSH_USER}@${IP} "sudo -u ${USER} bash -c ${SCRIPT_BODY}"
}


error() {
  echo >&2 "$@"
  exit 1
}

check_config() {
  log "Configuration"
  log "=============================="

  log "AWS_REGION=${AWS_REGION}"
  export EC2_URL="https://ec2.${AWS_REGION}.amazonaws.com"

  log "EC2_URL=${EC2_URL}"
  log "AMI=${AMI}"
  log "INSTANCE_TYPE=${INSTANCE_TYPE}"
  log "INSTANCE_NAME=${INSTANCE_NAME}"
  log "SUBNET_ID=${SUBNET_ID}"
  log "SECURITY_GROUP_ID=${SECURITY_GROUP_ID}"
  log "SSH_USER=${SSH_USER}"
  log "TENANCY=${TENANCY}"

  log "WAIT_SECS=${WAIT_SECS}"
  export WAIT=${WAIT_SECS}

  log "RETRIES=${RETRIES}"

  log "=============================="
}

wait_until_sshable() {
  IP=$1

  local i
  for i in $(seq "${RETRIES}"); do
    if nc -z -v "${IP}" 22; then
      return
    fi
    sleep $WAIT
  done

  error "Timed out waiting for ${IP} to come online."
}

wait_until_status() {
  local STATUS=$1

  log "Waiting for status: ${STATUS}"

  local N=0
  until [[ $N -ge $RETRIES ]]; do
    local COUNT=$(
      ec2-describe-instances --show-empty-fields ${INSTANCE_IDS[*]} |
      grep INSTANCE |
      cut -f6 |
      grep -c ${STATUS}
    ) || true

    log "${COUNT} of ${#INSTANCE_IDS[@]} instances ${STATUS}"

    [[ "$COUNT" -eq "${#INSTANCE_IDS[@]}" ]] && break

    N=$(($N+1))
    sleep $WAIT
  done

  if [[ $N -ge $RETRIES ]]; then
    error "Timed out waiting for instances to reach status: ${STATUS}"
  fi
}

wait_until_check_ok() {
  log "Waiting for instances to pass status checks"

  local N=0
  until [[ $N -ge $RETRIES ]]; do
    local COUNT

    COUNT=$(
      ec2-describe-instance-status --show-empty-fields ${INSTANCE_IDS[*]} |
      grep -e "\bINSTANCE\b" |
      cut -f6,7 |
      xargs -n 2 |
      grep -c "ok ok"
    ) || true

    log "${COUNT} of ${#INSTANCE_IDS[@]} instances pass status checks"

    [[ "$COUNT" -eq "${#INSTANCE_IDS[@]}" ]] && break

    COUNT=$(
      ec2-describe-instance-status --show-empty-fields ${INSTANCE_IDS[*]} |
      grep -e "\bINSTANCE\b" |
      cut -f6,7 |
      xargs -n 2 |
      grep -c "impaired"
    ) || true

    if [[ "$COUNT" -gt 0 ]]; then
      error "${COUNT} of ${#INSTANCE_IDS[@]} failed to pass status checks"
    fi

    N=$(($N+1))
    sleep $WAIT
  done

  if [[ $N -ge $RETRIES ]]; then
    error "Timed out waiting for instances to pass status checks"
  fi
}
