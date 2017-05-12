all: build
SHELL := /bin/bash
.DEFAULT_GOAL := test
MODULE_NAME=gpbackup
DIR_PATH=$(shell dirname `pwd`)

.PHONY : build

DEST = .

GOFLAGS := -o $(DEST)
dependencies :
		go get github.com/jmoiron/sqlx
		go get github.com/maxbrunsfeld/counterfeiter
		go get github.com/onsi/ginkgo/ginkgo
		go get github.com/onsi/gomega
		go get golang.org/x/tools/cmd/goimports
		go get gopkg.in/DATA-DOG/go-sqlmock.v1
		go get github.com/go-errors/errors
		go get github.com/lib/pq

format :
		goimports -w .
		go fmt .

ginkgo : depend
		ginkgo -r -randomizeSuites -randomizeAllSpecs 2>&1

test : ginkgo

ci : ginkgo

depend : dependencies

build : 
		go build $(GOFLAGS) -o $(MODULE_NAME)

build_rhel:
		env GOOS=linux GOARCH=amd64 go build $(GOFLAGS) -o $(MODULE_NAME)

build_osx:
		env GOOS=darwin GOARCH=amd64 go build $(GOFLAGS) -o $(MODULE_NAME)

install: all installdirs
		$(INSTALL_PROGRAM) gpbackup$(X) '$(DESTDIR)$(bindir)/gpbackup$(X)'

installdirs:
		$(MKDIR_P) '$(DESTDIR)$(bindir)'

clean :
		rm -f $(MODULE_NAME)
		rm -rf /tmp/go-build*
		rm -rf /tmp/ginkgo*
