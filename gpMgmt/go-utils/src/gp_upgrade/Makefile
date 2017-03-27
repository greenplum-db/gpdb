SHELL := /bin/bash
.DEFAULT_GOAL := test
MODULE_NAME=$(shell basename `pwd`)
DIR_PATH=$(shell dirname `pwd`)

.PHONY : build

DEST = bin/gp_upgrade

GOFLAGS := -o $(DEST)

export GOPATH := $(DIR_PATH)/..
export PATH := $(PATH):$(GOPATH)/bin

dependencies :
		go get github.com/onsi/ginkgo/ginkgo
		go get golang.org/x/tools/cmd/goimports
		go get github.com/maxbrunsfeld/counterfeiter
		go get github.com/onsi/gomega
		go get github.com/jessevdk/go-flags
		go get github.com/mattn/go-sqlite3
		go get

format : dependencies
		goimports -w .
		go fmt .

ginkgo : dependencies
		ginkgo -r -randomizeSuites -randomizeAllSpecs -race 2>&1

sshd_build :
		./compile_test_sshd_server.sh

test : format sshd_build ginkgo

ci : ginkgo

build :
		mkdir -p build
		go build $(GOFLAGS) -o $(GOPATH)/bin/$(MODULE_NAME)
