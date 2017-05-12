#!/usr/bin/env bash
set -x

my_dir="$(dirname "$0")"
source "$my_dir/common_go.bash"

set_gopath

GOPATH=$GOPATH go build -o . -o gpbackup
