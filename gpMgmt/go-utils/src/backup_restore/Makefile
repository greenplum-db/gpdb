top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

ifeq ($(enable_gpbackup), no)
all:
		@echo "Configuration not enabled for gpbackup"

else

all: build
SHELL := /bin/bash
.DEFAULT_GOAL := test
MODULE_NAME=gpbackup
DIR_PATH=$(shell dirname `pwd`)

.PHONY : build

DEST = .

format :
		goimports -w .
		go fmt .

test :
		./test_gpbackup.bash

ci : test

build :
		./build_gpbackup.bash

build_rhel:
		env GOOS=linux GOARCH=amd64 ./build_gpbackup.bash

build_osx:
		env GOOS=darwin GOARCH=amd64 ./build_gpbackup.bash

clean :
		rm -f $(MODULE_NAME)
		rm -rf /tmp/ginkgo*
endif
