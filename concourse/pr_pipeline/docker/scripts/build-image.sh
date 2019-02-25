#!/usr/bin/env bash

set -e

docker build . -t pivotaldata/greenplum-server-ubuntu-18
