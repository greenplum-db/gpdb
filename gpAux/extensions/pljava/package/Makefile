all: gppkg

BLD_TOP=../../..
GPMGMT=$(BLD_TOP)/gpMgmt
include $(BLD_TOP)/releng/tools.mk
include $(BLD_TOP)/Makefile.global

OS=$(word 1,$(subst _, ,$(BLD_ARCH)))
ARCH=$(shell uname -p)

JRE_VER=1.6.0_32
JRE_REL=1
JRE_DIR=$(RELENG_TOOLS)/jre-$(JRE_VER)
JRE_RPM_FLAGS="--define 'jre_dir $(JRE_DIR)' --define 'jre_ver $(JRE_VER)' --define 'jre_rel $(JRE_REL)'" 
JRE_RPM=jre-$(JRE_VER)-$(JRE_REL).$(ARCH).rpm

PLJAVA_DIR=`cd .. && pwd`
PLJAVA_GPPKG_VER=ossv1.4.0_pv1.3_gpdb4.3orca
PLJAVA_VER=1.3
PLJAVA_REL=1
PLJAVA_RPM_FLAGS="--define 'pljava_dir $(PLJAVA_DIR)' --define 'pljava_ver $(PLJAVA_VER)' --define 'pljava_rel $(PLJAVA_REL)' --define 'jre_ver $(JRE_VER)'"
PLJAVA_RPM=pljava-$(PLJAVA_VER)-$(PLJAVA_REL).$(ARCH).rpm
PLJAVA_GPPKG=pljava-$(PLJAVA_GPPKG_VER)-$(OS)-$(ARCH).gppkg

TARGET_GPPKG=$(PLJAVA_GPPKG)
EXTRA_CLEAN+=$(JRE_RPM) $(PLJAVA_RPM) $(PLJAVA_GPPKG)

#
# Generic rules to build gppkgs included here
#
include $(BLD_TOP)/releng/gppkg.mk

gppkg: $(JRE_DIR)
	$(MAKE) $(JRE_RPM) RPM_FLAGS=$(JRE_RPM_FLAGS)
	PATH=$(INSTLOC)/bin:$(PATH) $(MAKE) $(PLJAVA_RPM) RPM_FLAGS=$(PLJAVA_RPM_FLAGS)
	$(MAKE) $(PLJAVA_GPPKG) MAIN_RPM=$(PLJAVA_RPM) DEPENDENT_RPMS=$(JRE_RPM)

.PHONY: gppkg
