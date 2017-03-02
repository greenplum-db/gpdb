#!/usr/bin/env bash

pushd "./commands/sshd"
go get
go build
RET=$?
popd
exit $RET
