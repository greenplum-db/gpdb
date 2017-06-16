#!/usr/bin/env bash

pushd "./integrations/sshd" 1>/dev/null
  go get
  go build -o "$GOPATH/bin/test/sshd"
  RET=$?
popd 1>/dev/null
exit $RET
