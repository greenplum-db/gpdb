SHELL := /bin/bash
CURRENT_DIR=$(shell dirname `pwd`)
.DEFAULT_GOAL := all

all : build

build :
	go get
	go build -o "$(CURRENT_DIR)/../../../../bin/test/sshd"
