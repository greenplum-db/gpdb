all: gppkg

BLD_TOP=../../../gpAux
GPMGMT=$(BLD_TOP)/gpMgmt
include $(BLD_TOP)/Makefile.global

OS=$(word 1,$(subst _, ,$(BLD_ARCH)))
ARCH=$(shell uname -p)

INTARRAY_DIR=`cd .. && pwd`
INTARRAY_VER=1.0
INTARRAY_PKG_VER=ossv1.1_pv1.2_gpdb4.3orca
INTARRAY_REL=1
INTARRAY_RPM_FLAGS="--define 'intarray_dir $(INTARRAY_DIR)' --define 'intarray_ver $(INTARRAY_VER)' --define 'intarray_rel $(INTARRAY_REL)'"
INTARRAY_RPM=intarray-$(INTARRAY_VER)-$(INTARRAY_REL).$(ARCH).rpm
INTARRAY_GPPKG=intarray-$(INTARRAY_PKG_VER)-$(OS)-$(ARCH).gppkg

TARGET_GPPKG=$(INTARRAY_GPPKG)
EXTRA_CLEAN+=$(INTARRAY_RPM) $(INTARRAY_GPPKG)

#
# Generic rules to build gppkgs included here
#
include $(BLD_TOP)/releng/gppkg.mk

gppkg: 
	$(MAKE) $(INTARRAY_RPM) RPM_FLAGS=$(INTARRAY_RPM_FLAGS)
	$(MAKE) $(INTARRAY_GPPKG) MAIN_RPM=$(INTARRAY_RPM)

.PHONY: gppkg
