#!/usr/bin/env bash
set -x

my_dir="$(dirname "$0")"
source "$my_dir/common_go.bash"

set_gopath

#Install dependencies
go get github.com/jmoiron/sqlx
go get github.com/maxbrunsfeld/counterfeiter
go get github.com/onsi/ginkgo/ginkgo
go get github.com/onsi/gomega
go get golang.org/x/tools/cmd/goimports
go get gopkg.in/DATA-DOG/go-sqlmock.v1
go get github.com/go-errors/errors
go get github.com/lib/pq

GOPATH=$GOPATH PATH=$GOPATH/bin:$PATH ginkgo -r -randomizeSuites -randomizeAllSpecs 2>&1
