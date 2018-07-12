#!/usr/bin/env bash

source $HOME/qa.sh

cd $BLDWRAP_TOP/gpMgmt

make -f Makefile.behave behave "$*" TAR=tar 2>&1
