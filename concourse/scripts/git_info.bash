#!/bin/bash

set -uo pipefail

# ----------------------------------------------------------------------
# Check for prerequisite utilities
# ----------------------------------------------------------------------

PREREQ_COMMANDS="git"

for COMMAND in ${PREREQ_COMMANDS}; do
    type "${COMMAND}" >/dev/null 2>&1 || { echo >&2 "Required command line utility \"${COMMAND}\" is not available.  Aborting."; exit 1; }
done

# ----------------------------------------------------------------------
# Retrieve root URI, full commit SHA1, and version
# ----------------------------------------------------------------------

URI=$( git remote -v | grep "fetch" | grep "origin" | awk '{print $2}' )
SHA1=$( git rev-parse HEAD )
VERSION=$( ./getversion --short )

# ----------------------------------------------------------------------
# Generate root JSON record
# ----------------------------------------------------------------------

echo -n "
{\"root\": {
  \"uri\": \"${URI}\",
  \"sha1\": \"${SHA1}\",
  \"version\": \"${VERSION}\"},
  \"submodules\": ["

# ----------------------------------------------------------------------
# Generate submodule records
# ----------------------------------------------------------------------

submodules=$( git submodule status )
submodule_count=$( git submodule status | wc -l | tr -d ' ' )

COUNTER=0
IFS=$'\n'

for line in $submodules; do
    SOURCE_PATH=$( echo "${line}" | cut -d " " -f3 )
    SHA1=$( echo "${line}" | cut -d " " -f2 )
    TAG=$( echo "${line}" | cut -d " " -f4 )
    COUNTER=$((COUNTER+1))

echo -n "
     {\"source_path\": \"${SOURCE_PATH}\",
      \"sha1\": \"${SHA1}\",
      \"tag\": \"${TAG//[)(]/}\"}"

    if [ "${COUNTER}" != "${submodule_count}" ]; then
        echo -n ","
    fi
done

echo "]}"
