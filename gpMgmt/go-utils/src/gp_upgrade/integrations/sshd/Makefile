SHELL := /bin/bash
.DEFAULT_GOAL := all
THIS_MAKEFILE_DIR=$(dir $(lastword $(MAKEFILE_LIST)))
GO_UTILS_DIR=$(THIS_MAKEFILE_DIR)/../../../..

all : build

build :
	go get
	go build -o $(GO_UTILS_DIR)/bin/sshd
