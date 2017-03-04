#!/usr/bin/env bash

pushd "./commands/sshd"
go get
go build -o "$GOPATH/bin/test/sshd"
RET=$?
popd
exit $RET
