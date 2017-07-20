SHELL := /bin/bash
.DEFAULT_GOAL := all

# Not all platforms have a realpath binary in PATH, most notably macOS doesn't,
# so provide an alternative implementation. Code inspired by:
# http://stackoverflow.com/questions/3572030/bash-script-absolute-path-with-osx
define realpath
	[[ $(1) = /* ]] && echo "$(1)" || echo "$(PWD)/$({1#./})"
endef

THIS_MAKEFILE_DIR=$(dir $(call realpath,$(lastword $(MAKEFILE_LIST))))
MODULE_NAME=$(shell basename $(THIS_MAKEFILE_DIR))
GO_UTILS_DIR=$(THIS_MAKEFILE_DIR)/../..
ARCH := amd64
GPDB_VERSION := $(shell ../../../../getversion --short)

.NOTPARALLEL:

all : build

dependencies :
		go get github.com/cppforlife/go-semi-semantic/version
		go get github.com/onsi/ginkgo/ginkgo
		go get golang.org/x/tools/cmd/goimports
		go get github.com/onsi/gomega
		go get github.com/jessevdk/go-flags
		go get golang.org/x/crypto/ssh
		go get -u github.com/golang/lint/golint
		go get github.com/alecthomas/gometalinter
		go get
# Counterfeiter is not a proper dependency of the app. It is only used occasionally to generate a test class that
# is then checked in.  At the time of that generation, it can be added back to run the dependency list, temporarily.
#		go get github.com/maxbrunsfeld/counterfeiter

format : dependencies
		goimports -w .
		go fmt .

lint :	dependencies
		! gofmt -l . | read
		gometalinter --config=gometalinter.config ./...

unit : dependencies
		ginkgo -r -randomizeSuites -randomizeAllSpecs -race --skipPackage=integrations

sshd_build : dependencies
		make -C integrations/sshd

integration: dependencies
		ginkgo -r -randomizeAllSpecs -race integrations

test : format unit integration lint

push : format
		git pull -r && make test && git push

build : format
		go build -ldflags "-X gp_upgrade/commands.GpdbVersion=$(GPDB_VERSION)" -o $(GO_UTILS_DIR)/bin/$(MODULE_NAME)

coverage: dependencies format build
		./scripts/run_coverage.sh

linux :
		GOOS=$@ GOARCH=$(ARCH) go build -ldflags "-X gp_upgrade/commands.GpdbVersion=$(GPDB_VERSION)" -o $(GO_UTILS_DIR)/bin/$(MODULE_NAME).$@
darwin :
		GOOS=$@ GOARCH=$(ARCH) go build -ldflags "-X gp_upgrade/commands.GpdbVersion=$(GPDB_VERSION)" -o $(GO_UTILS_DIR)/bin/$(MODULE_NAME).$@

platforms : linux darwin

install :
	mkdir -p $(prefix)/bin
	cp -p ../../bin/gp_upgrade $(prefix)/bin/

clean:
	rm -f ../../bin/gp_upgrade
