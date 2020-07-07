#!/usr/bin/env bash

set -e

echo "set PYTHONHOME if first argument is 'yes'"
./generate-greenplum-path.sh yes | grep -q 'PYTHONHOME="${GPHOME}/ext/python"'

echo "do not set PYTHONHOME if first argument is not 'yes'"
[ $(./generate-greenplum-path.sh no | grep -c PYTHONHOME) -eq 0 ]

echo "do not set PYTHONHOME if first argument is missing"
[ $(./generate-greenplum-path.sh | grep -c PYTHONHOME) -eq 0 ]

echo "ALL TEST PASSED"
