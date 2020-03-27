#!/usr/bin/env bash

. lib/gp_bash_functions.sh

__cleanupTestUsers() {
  dropuser 123456
  dropuser abc123456
}

mimic_gpinitsystem_setup() {
  # ensure MASTER_PORT is set, it is needed by SET_GP_USER_PW
  GET_MASTER_PORT "$MASTER_DATA_DIRECTORY"

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

it_should_work_with_a_hostname() {
  ARRAY_LINE="sdw1~sdw1~50433~/data/primary/gpseg0~1~0"
  SET_VAR $ARRAY_LINE
  if [ "$GP_HOSTNAME" != "sdw1" ]; then
    echo "got hostname $GP_HOSTNAME, wanted sdw1"
    exit 1
  fi
  if [ "$GP_HOSTADDRESS" != "sdw1" ]; then
    echo "got address $GP_HOSTADDRESS, wanted sdw1"
    exit 1
  fi
  if [ "$GP_PORT" != "50433" ]; then
    echo "got port $GP_PORT, wanted 50433"
    exit 1
  fi
  if [ "$GP_DIR" != "/data/primary/gpseg0" ]; then
    echo "got data directory $GP_DIR, wanted /data/primary/gpseg0"
    exit 1
  fi
  if [ "$GP_DBID" != "1" ]; then
    echo "got dbid $GP_DBID, wanted 1"
    exit 1
  fi
  if [ "$GP_CONTENT" != "0" ]; then
    echo "got content $GP_CONTENT, wanted 0"
    exit 1
  fi
}

it_should_work_without_a_hostname() {
  ARRAY_LINE="sdw1~50433~/data/primary/gpseg0~1~0"
  SET_VAR $ARRAY_LINE
  if [ "$GP_HOSTNAME" != "sdw1" ]; then
    echo "got hostname $GP_HOSTNAME, wanted sdw1"
    exit 1
  fi
  if [ "$GP_HOSTADDRESS" != "sdw1" ]; then
    echo "got address $GP_HOSTADDRESS, wanted sdw1"
    exit 1
  fi
  if [ "$GP_PORT" != "50433" ]; then
    echo "got port $GP_PORT, wanted 50433"
    exit 1
  fi
  if [ "$GP_DIR" != "/data/primary/gpseg0" ]; then
    echo "got data directory $GP_DIR, wanted /data/primary/gpseg0"
    exit 1
  fi
  if [ "$GP_DBID" != "1" ]; then
    echo "got dbid $GP_DBID, wanted 1"
    exit 1
  fi
  if [ "$GP_CONTENT" != "0" ]; then
    echo "got content $GP_CONTENT, wanted 0"
    exit 1
  fi
}

set_var_should_error_out_if_given_the_wrong_number_of_fields() {
  IGNORE_WARNINGS=0

  LONG_ARRAY_LINE="one~two~3~/four~5~6~7"
  ret=$(SET_VAR $LONG_ARRAY_LINE)
  if [[ "$ret" != *"wrong number of fields"* ]]; then
      echo "did not error out on too many fields"
      exit 1
  fi

  SHORT_ARRAY_LINE="one~two~3~/four"
  ret=$(SET_VAR $SHORT_ARRAY_LINE)
  if [[ "$ret" != *"wrong number of fields"* ]]; then
      echo "did not error out on too few fields"
      exit 1
  fi
}

set_var_should_error_out_if_given_bad_field_values() {
  IGNORE_WARNINGS=0

  BAD_PORT_ARRAY_LINE="host~address~port~/dir~0~1"
  ret=$(SET_VAR $BAD_PORT_ARRAY_LINE)
  if [[ "$ret" != *"non-numeric value"* ]]; then
      echo "did not error out on non-numeric port value"
      exit 1
  fi

  BAD_DBID_ARRAY_LINE="host~address~5432~/dir~dbid~1"
  ret=$(SET_VAR $BAD_DBID_ARRAY_LINE)
  if [[ "$ret" != *"non-numeric value"* ]]; then
      echo "did not error out on non-numeric dbid value"
      exit 1
  fi

  BAD_CONTENT_ARRAY_LINE="host~address~5432~/dir~0~content"
  ret=$(SET_VAR $BAD_CONTENT_ARRAY_LINE)
  if [[ "$ret" != *"non-numeric value"* ]]; then
      echo "did not error out on non-numeric content value"
      exit 1
  fi

  BAD_DIR_ARRAY_LINE="host~address~5432~dir~0~1"
  ret=$(SET_VAR $BAD_DIR_ARRAY_LINE)
  if [[ "$ret" != *"not a valid path"* ]]; then
      echo "did not error out on invalid directory value"
      exit 1
  fi
}

main() {
  it_should_quote_the_username_during_alter_user_in_SET_GP_USER_PW
  it_should_quote_the_password_during_alter_user_in_SET_GP_USER_PW
  it_should_work_with_a_hostname
  it_should_work_without_a_hostname
  set_var_should_error_out_if_given_the_wrong_number_of_fields
  set_var_should_error_out_if_given_bad_field_values
}

main
