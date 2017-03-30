#!/usr/bin/env bash

pushd "./commands/sshd" 1>/dev/null
  go get
  go build -o "$GOPATH/bin/test/sshd"
  RET=$?
popd 1>/dev/null
exit $RET
