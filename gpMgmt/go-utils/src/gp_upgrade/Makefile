top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

.DEFAULT_GOAL := all

THIS_MAKEFILE_DIR=$(shell pwd)
MODULE_NAME=$(shell basename $(THIS_MAKEFILE_DIR))
GO_UTILS_DIR=$(THIS_MAKEFILE_DIR)/../..
ARCH := amd64
GPDB_VERSION := $(shell ../../../../getversion --short)

# If you want to do cross-compilation,
# BUILD_TARGET=linux for linux and
# BUILD_TARGET=darwin macos.
# See go build GOOS for more information.
BRANCH := $(shell git for-each-ref --format='%(objectname) %(refname:short)' refs/heads | awk "/^$$(git rev-parse HEAD)/ {print \$$2}")
PLATFORM_POSTFIX := $(if $(BUILD_TARGET),.$(BUILD_TARGET).$(BRANCH),)
TARGET_PLATFORM := $(if $(BUILD_TARGET),GOOS=$(BUILD_TARGET) GOARCH=$(ARCH),)

.NOTPARALLEL:

all : dependencies build

# The inheritied LD_LIBRARY_PATH setting causes git clone in go get to fail.  Hence, nullifying it.
dependencies :  export LD_LIBRARY_PATH =
dependencies :
		go get -d -u github.com/greenplum-db/gp-common-go-libs/gplog
		go get github.com/blang/semver
		go get github.com/cppforlife/go-semi-semantic/version
		go get github.com/onsi/ginkgo/ginkgo
		go get golang.org/x/tools/cmd/goimports
		go get github.com/onsi/gomega
		go get golang.org/x/crypto/ssh
		go get -u github.com/golang/lint/golint
		go get github.com/alecthomas/gometalinter
		go get github.com/golang/protobuf/protoc-gen-go
		go get github.com/spf13/cobra
		go get github.com/pkg/errors
		go get google.golang.org/grpc
		go get github.com/golang/mock/gomock
		go get github.com/cloudfoundry/gosigar
		go get gopkg.in/DATA-DOG/go-sqlmock.v1
		go get github.com/lib/pq
		go get github.com/jmoiron/sqlx

# Counterfeiter is not a proper dependency of the app. It is only used occasionally to generate a test class that
# is then checked in.  At the time of that generation, it can be added back to run the dependency list, temporarily.
#		go get github.com/maxbrunsfeld/counterfeiter

format :
		gofmt -s -w .

lint :
		gometalinter --config=gometalinter.config ./...

unit :
		ginkgo -r -randomizeSuites -randomizeAllSpecs -race --skipPackage=integrations

sshd_build :
		make -C integrations/sshd

integration: install
		-gpstop -ai
		gpstart -a
		-pkill gp_upgrade_hub
		$(prefix)/bin/gp_upgrade prepare start-hub
		$(prefix)/bin/gp_upgrade check config
		ginkgo -r -randomizeAllSpecs -race integrations

test : format lint unit integration

protobuf :
		protoc -I idl/ idl/*.proto --go_out=plugins=grpc:idl
		go get github.com/golang/mock/mockgen
		mockgen -source idl/cli_to_hub.pb.go -imports ".=gp_upgrade/idl" > mock_idl/cli_to_hub_mock.pb.go
		mockgen -source idl/hub_to_agent.pb.go -imports ".=gp_upgrade/idl" > mock_idl/hub_to_agent_mock.pb.go

build :
		$(TARGET_PLATFORM) go build -ldflags "-X gp_upgrade/cli/commanders.GpdbVersion=$(GPDB_VERSION)" -o $(GO_UTILS_DIR)/bin/$(MODULE_NAME)$(PLATFORM_POSTFIX) $(MODULE_NAME)/cli
		$(TARGET_PLATFORM) go build -ldflags "-X gp_upgrade/cli/commanders.GpdbVersion=$(GPDB_VERSION)" -o $(GO_UTILS_DIR)/bin/gp_upgrade_agent$(PLATFORM_POSTFIX) $(MODULE_NAME)/agent
		$(TARGET_PLATFORM) go build -ldflags "-X gp_upgrade/cli/commanders.GpdbVersion=$(GPDB_VERSION)" -o $(GO_UTILS_DIR)/bin/gp_upgrade_hub$(PLATFORM_POSTFIX) $(MODULE_NAME)/hub

coverage: build
		ginkgo -r -cover -covermode=set
		echo "mode: set" > coverage.out && find . -name '*coverprofile' | xargs cat | grep -v mode: | sort -r | awk '{if($$1 != last) {print $$0;last=$$1}}' >> coverage.out
		find . -name '*.coverprofile' | xargs rm
		go tool cover -html=coverage.out

install : build
	mkdir -p $(prefix)/bin
	cp -p ../../bin/gp_upgrade $(prefix)/bin/
	cp -p ../../bin/gp_upgrade_hub $(prefix)/bin/
	cp -p ../../bin/gp_upgrade_agent $(prefix)/bin/

clean:
	rm -f ../../bin/gp_upgrade
	rm -f ../../bin/gp_upgrade_hub
	rm -f ../../bin/gp_upgrade_agent
	rm -rf /tmp/go-build*
	rm -rf /tmp/ginkgo*
