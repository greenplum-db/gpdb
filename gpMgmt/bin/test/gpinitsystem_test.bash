#!/usr/bin/env bash

. lib/gp_bash_functions.sh

__cleanupTestUsers() {
  dropuser 123456
  dropuser abc123456
}

mimic_gpinitsystem_setup() {
  # ensure COORDINATOR_PORT is set, it is needed by SET_GP_USER_PW
  GET_COORDINATOR_PORT "$COORDINATOR_DATA_DIRECTORY"

  # the return value set when performing ERROR_CHK
  # on the status code returned from $PSQL
  #
  # set it to a default value
  RETVAL=0
}

it_should_quote_the_username_during_alter_user_in_SET_GP_USER_PW() {
  mimic_gpinitsystem_setup

  # given a user that is only a number
  USER_NAME=123456
  createuser $USER_NAME
  trap __cleanupTestUsers EXIT

  # when we run set user password
  SET_GP_USER_PW

  # then it should succeed
  if [ "$RETVAL" != "0" ]; then
    local error_message="$(tail -n 10 "$LOG_FILE")"
    echo "got an exit status of $RETVAL while running SET_GP_USER_PW for $USER_NAME, wanted success: $error_message"
    exit 1
  fi
}

it_should_quote_the_password_during_alter_user_in_SET_GP_USER_PW() {
  mimic_gpinitsystem_setup

  # given a user
  USER_NAME=abc123456
  createuser $USER_NAME
  trap __cleanupTestUsers EXIT

  # when we run set user password with a password containing single quotes
  GP_PASSWD="abc'"
  SET_GP_USER_PW

  # then it should succeed
  if [ "$RETVAL" != "0" ]; then
    local error_message="$(tail -n 10 "$LOG_FILE")"
    echo "got an exit status of $RETVAL while running SET_GP_USER_PW for $USER_NAME with password $GP_PASSWD, wanted success: $error_message"
    exit 1
  fi
}

main() {
  it_should_quote_the_username_during_alter_user_in_SET_GP_USER_PW
  it_should_quote_the_password_during_alter_user_in_SET_GP_USER_PW
}

main
