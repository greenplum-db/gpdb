#!/bin/bash

set -eo pipefail
# Set env var ${NL} because "\n" can not be converted to new line for unknown escaping reason
export NL="
"

# Retrieve cell content from $RAW_STR according to the row and column numbers,
# and set it to the given variable name.
# Arg 1 (output): Variable name.
# Arg 2 (input): Row number.
# Arg 3 (input): Column number.
# Example:
# 1: @post_run 'get_cell USER_NAME 1 2': SELECT id,name,state FROM users;
# Assume 'SELECT id,name,state FROM users;' returns:
# | id | name  | state |
# |----+-------+--------|
# | 1  | Jonh  | Alive  |
# | 2  | Alice | Dead   |
# by calling `get_cell USER_NAME 1 2' will set $USER_NAME to 'Jonh'.
get_tuple_cell() {
    var_name=$1
    row=$2
    col=$3
    cmd="echo \"\$RAW_STR\" | awk -F '|' 'NR==(($row+2)) {print \$$col}' | awk '{\$1=\$1;print}'"
    output=`eval $cmd`
    eval $var_name="$output"
}

# Generate $MATCHSUBS and echo the $RAWSTR based on the given original string and replacement pairs.
# Arg 1n (input): The original string to be replaced.
# Arg 2n (input): The replacement string.
# Example:
# 1: @post_run 'genereate_match_sub $USER_NAME user1 $USER_ID id1': SELECT id,name,state FROM users;
# here we assume $USER_NAME and $USER_ID has been set to 'Jonh' and '1' already. Then the above
# statement will generate $MATCHSUBS section:
# m/\bJonh\b/
# s/\bJonh\b/user1/
# \b here is for matching the whole word. (word boundaries)
create_match_sub() {
    to_replace=""
    for var in "$@"
        do
        if [ -z "$to_replace" ]
        then
            to_replace=$var
        else
            # \b is trying to match the whole word to make it more stable.
            export MATCHSUBS="${MATCHSUBS}${NL}m/\\b${to_replace}\\b/${NL}s/\\b${to_replace}\\b/${var}/${NL}"
            to_replace=""
        fi
    done
    echo "${RAW_STR}"
}

# Generate $MATCHSUBS and Trim the Tailing spaces.
# This is similar to create_match_sub() but dealing with the tailing spaces.
# Sometimes we have variable length cells, like userid:
# | username | userid | gender |
# |----------+--------+--------|
# | john     | 12     | male   |
# we need to match the 12 with a var $USERID which has been set by get_call().
# The output source will be something like:
# | username | userid | gender |
# |----------+--------+--------|
# | john     | userid1     | male   |
# to match it: match_sub userid1 $USERID
# but the problem here is the userid may change for different test executions. If we
# get a 3 digits userid like '123', the diff will fail since we have one more space than
# the actual sql output.
# To deal with it, use match_sub_tt userid $USERID
# And make the output source like (note: append one space to the replaced string):
# | username | userid  | gender |
# |----------+---------+--------|
# | john     | userid1 | male   |
# Notice here that there is no space following userid1 since we replace the whole userid with
# its tailing spaces with 'userid1'. Like '123   ' -> 'userid'.
create_match_sub_with_spaces() {
    to_replace=""
    for var in "$@"
        do
        if [ -z "$to_replace" ]
        then
            to_replace=$var
        else
            # \b is trying to match the whole word to make it more stable.
            export MATCHSUBS="${MATCHSUBS}${NL}m/\\b${to_replace}\\b/${NL}s/\\b${to_replace} */${var} /${NL}"
            to_replace=""
        fi
    done
    echo "${RAW_STR}"
}

# Substitute in the $RAW_STR and echo the result.
# Multi substitution pairs can be passed as arguments, like:
# sub "to_replace_1" "replacement_1" "to_replace_2" "replacement_2"
# This could be useful for both @pre_run and @post_run. e.g.:
# @pre_run 'sub @TOKEN1 ${TOKEN1}': SELECT state FROM GP_ENDPOINTS_STATUS_INFO() WHERE token='@TOKEN1';
# Assume the $TOKEN has value '01234', The SQL will become:
# SELECT state FROM GP_ENDPOINTS_STATUS_INFO() WHERE token='01234';
create_sub() {
    to_replace=""
    for var in "$@"
        do
        if [ -z "$to_replace" ]
        then
            to_replace=$var
        else
            RAW_STR=$(echo "$RAW_STR" | sed -E "s/${to_replace}/${var}/g")
            to_replace=""
        fi
    done
    echo "${RAW_STR}"
}

# Parse the endpoint state info output and save them into environment variables for @post_run.
# Usage: parse_endpoint_info <postfix> <endpoint_col> <token_col> <host_col> <port_col>
# Output(environment variables):
#   "TOKEN$postfix"
#   "ENDPOINT_NAME$postfix[]"
#   "ENDPOINT_TOKEN$postfix[]"
#   "ENDPOINT_HOST$postfix[]"
#   "ENDPOINT_PORT$postfix[]"
# e.g.:
# For the given SQL result:
#      endpointname     |             token                |  hostname | port  | state
# ----------------------+----------------------------------+-------------+-------+--------
#  c1_00001507_00000000 | 071500004015dc6da471b20417afed65 | host_1111 | 25432 | READY
#  c1_00001507_00000001 | 071500004015dc6da471b20417afed65 | host_1112 | 25433 | READY
#  c1_00001507_00000002 | 071500004015dc6da471b20417afed65 | host_1113 | 25434 | READY
# (3 rows)
# parse_endpoint_info 1 1 2 3 4 will setup below variables:
# TOEKN1='071500004015dc6da471b20417afed65'
# ENDPOINT_NAME1[0]='c1_00001507_00000000'
# ENDPOINT_TOKEN1[0]='071500004015dc6da471b20417afed65'
# ENDPOINT_HOST1[0]='host_1111'
# ENDPOINT_PORT1[0]='25432'
# ENDPOINT_NAME1[1]='c1_00001507_00000001'
# ENDPOINT_TOKEN1[1]='071500004015dc6da471b20417afed65'
# ENDPOINT_HOST1[1]='host_1112'
# ENDPOINT_PORT1[1]='25433'
# ENDPOINT_NAME1[2]='c1_00001507_00000002'
# ENDPOINT_TOKEN1[2]='071500004015dc6da471b20417afed65'
# ENDPOINT_HOST1[2]='host_1113'
# ENDPOINT_PORT1[2]='25434'
parse_endpoint_info() {
    local postfix=$1
    local endpoint_name_col=$2
    local token_col=$3
    local host_col=$4
    local port_col=$5
    export CURRENT_ENDPOINT_POSTFIX="${postfix}"

    eval "ENDPOINT_NAME${postfix}=()"
    eval "ENDPOINT_TOKEN${postfix}=()"
    eval "ENDPOINT_HOST${postfix}=()"
    eval "ENDPOINT_PORT${postfix}=()"
    while IFS= read -r line ; do
        local name=""
        name="$(echo "${line}" | awk -F '|' "{print \$${endpoint_name_col}}" | awk '{$1=$1;print}')"
        local token=""
        token="$(echo "${line}" | awk -F '|' "{print \$${token_col}}" | awk '{$1=$1;print}')"
        local host=""
        host="$(echo "${line}" | awk -F '|' "{print \$${host_col}}" | awk '{$1=$1;print}' )"
        local port=""
        port="$(echo "${line}" | awk -F '|' "{print \$${port_col}}" | awk '{$1=$1;print}' )"
        eval "ENDPOINT_NAME${postfix}+=(${name})"
        eval "ENDPOINT_TOKEN${postfix}+=(${token})"
        eval "ENDPOINT_HOST${postfix}+=(${host})"
        eval "ENDPOINT_PORT${postfix}+=(${port})"

        eval "TOKEN${postfix}=${token}"
        export RETRIEVE_TOKEN=${token}

        create_match_sub_with_spaces  "${name}" "endpoint_id${postfix}" \
            "${port}" port_id \
            "${token}" token_id \
            "${host}" host_id > /dev/null

    # Filter out the first two lines and the last line.
    done <<<"$(echo "$RAW_STR" | sed '1,2d;$d')"
    # Ignore first 2 lines(table header) since hostname length may affect the diff result.
    echo "${RAW_STR}" | sed '1,2d'
}

# Find the corresponding endpoint in the environment variables saved by previous
# parse_endpoint call, and substitute in the SQL. Used by @pre_run.
# The finding process relies on the $GP_HOSTNAME and $GP_PORT to be set to the
# current postgres connection.
# Usage: set_endpoint_variable <ENDPOINT_STR><postfix>
# e.g.:
# set_endpoint_variable "@ENDPOINT1"
# This will replace "@ENDPOINT1" in the SQL statement with the corresponding endpoint name
# with postfix "1".
set_endpoint_variable() {
    export CURRENT_ENDPOINT_POSTFIX=""
    CURRENT_ENDPOINT_POSTFIX="$(echo "$1" | sed 's/@ENDPOINT//')"
    eval "local names=(\${ENDPOINT_NAME${CURRENT_ENDPOINT_POSTFIX}[@]})"
    eval "local hosts=(\"\${ENDPOINT_HOST${CURRENT_ENDPOINT_POSTFIX}[@]}\")"
    eval "ports=(\${ENDPOINT_PORT${CURRENT_ENDPOINT_POSTFIX}[@]})"
    local i=0
    for h in "${hosts[@]}" ; do
        if [ "$GP_HOSTNAME" = "$h" ] ; then
            if [ "$GP_PORT" = "${ports[$i]}" ] ; then
                create_sub "$1" "${names[$i]}"
                return
            fi
        fi
        i=$((i+1))
    done
    # echo "Cannot find endpoint for postfix '$CURRENT_ENDPOINT_POSTFIX', '$GP_HOSTNAME', '$GP_PORT'."
    echo $RAW_STR
}

# Return the retrieve token based on the $CURRENT_ENDPOINT_POSTFIX, $GP_HOSTNAME and $GP_PORT
get_retrieve_token() {
    eval "local tokens=(\${ENDPOINT_TOKEN${CURRENT_ENDPOINT_POSTFIX}[@]})"
    eval "local hosts=(\"\${ENDPOINT_HOST${CURRENT_ENDPOINT_POSTFIX}[@]}\")"
    eval "ports=(\${ENDPOINT_PORT${CURRENT_ENDPOINT_POSTFIX}[@]})"
    local i=0
    for h in "${hosts[@]}" ; do
        if [ "$GP_HOSTNAME" = "$h" ] ; then
            if [ "$GP_PORT" = "${ports[$i]}" ] ; then
                echo "${tokens[$i]}"
                return
            fi
        fi
        i=$((i+1))
    done
    echo "Can not find the token."
}
