#!/usr/bin/env bash

set -x

killall postgres
rm -rf /tmp/.s*
rm -rf $MASTER_DATA_DIRECTORY
