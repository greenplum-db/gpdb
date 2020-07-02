#!/usr/bin/env bash

set -e

echo "set GPHOME with first argument"
./generate-greenplum-path.sh /foo | grep -q 'GPHOME="/foo"'

echo "set PYTHONHOME if second argument is 'yes'"
./generate-greenplum-path.sh /foo yes | grep -q 'PYTHONHOME="${GPHOME}/ext/python"'

echo "do not set PYTHONHOME if second argument is not 'yes'"
[ $(./generate-greenplum-path.sh /foo no | grep -c PYTHONHOME) -eq 0 ]

echo "do not set PYTHONHOME if second argument is missing"
[ $(./generate-greenplum-path.sh /foo | grep -c PYTHONHOME) -eq 0 ]

echo "error out if no argument is given"
if ./generate-greenplum-path.sh; then
  echo "should not have passed"
  exit 1
fi
./generate-greenplum-path.sh | grep -q "Must specify a value for GPHOME"

echo "ALL TEST PASSED"
