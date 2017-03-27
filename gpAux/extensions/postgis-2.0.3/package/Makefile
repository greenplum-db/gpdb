all: gppkg

BLD_TOP=../../../
GPMGMT=$(BLD_TOP)/gpMgmt
include $(BLD_TOP)/Makefile.global

OS=$(word 1,$(subst _, ,$(BLD_ARCH)))
ARCH=$(shell uname -p)

# Set to path of libs downloaded from Ivy repo
RELENG_TOOLS=$(shell cd $(BLD_TOP)/ext/$(BLD_ARCH) && pwd)

GEOS_VER=3.3.8
GEOS_REL=1
GEOS_DIR=$(RELENG_TOOLS)/geos-$(GEOS_VER)
GEOS_CONFIG=$(GEOS_DIR)/bin/geos-config
GEOS_RPM_FLAGS="--define 'geos_dir $(GEOS_DIR)' --define 'geos_ver $(GEOS_VER)' --define 'geos_rel $(GEOS_REL)'"
GEOS_RPM=geos-$(GEOS_VER)-$(GEOS_REL).$(ARCH).rpm

PROJ_VER=4.8.0
PROJ_REL=1
PROJ_DIR=$(RELENG_TOOLS)/proj-$(PROJ_VER)
PROJ_RPM_FLAGS="--define 'proj_dir $(PROJ_DIR)' --define 'proj_ver $(PROJ_VER)' --define 'proj_rel $(PROJ_REL)'"
PROJ_RPM=proj-$(PROJ_VER)-$(PROJ_REL).$(ARCH).rpm

POSTGIS_DIR=`cd ../source && pwd`
POSTGIS_VER=2.0.3
POSTGIS_PKG_VER=ossv2.0.3_pv2.0.1_gpdb4.3orca
POSTGIS_REL=2
POSTGIS_RPM_FLAGS="--define 'postgis_dir $(POSTGIS_DIR)' --define 'postgis_ver $(POSTGIS_VER)' --define 'postgis_rel $(POSTGIS_REL)' --define 'geos_ver $(GEOS_VER)' --define 'proj_ver $(PROJ_VER)'"
DEPENDENT_RPMS="$(GEOS_RPM) $(PROJ_RPM)"
POSTGIS_RPM=postgis-$(POSTGIS_VER)-$(POSTGIS_REL).$(ARCH).rpm
POSTGIS_GPPKG=postgis-$(POSTGIS_PKG_VER)-$(OS)-$(ARCH).gppkg

TARGET_GPPKG=$(POSTGIS_GPPKG)
EXTRA_CLEAN+=$(GEOS_RPM) $(PROJ_RPM) $(POSTGIS_RPM) $(POSTGIS_GPPKG)

#
# Generic rules to build gppkgs included here
#
include $(BLD_TOP)/releng/gppkg.mk

gppkg: $(GEOS_DIR) $(PROJ_DIR)
	$(MAKE) $(GEOS_RPM) RPM_FLAGS=$(GEOS_RPM_FLAGS)
	$(MAKE) $(PROJ_RPM) RPM_FLAGS=$(PROJ_RPM_FLAGS)
	$(MAKE) $(POSTGIS_RPM) RPM_FLAGS=$(POSTGIS_RPM_FLAGS)
	$(MAKE) $(POSTGIS_GPPKG) MAIN_RPM=$(POSTGIS_RPM) DEPENDENT_RPMS=$(DEPENDENT_RPMS)

.PHONY: gppkg
