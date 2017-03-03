#!/bin/bash

source $HOME/qa.sh

cd $BLDWRAP_TOP/gpMgmt

make -f Makefile.behave behave $1 TAR=tar 2>&1
