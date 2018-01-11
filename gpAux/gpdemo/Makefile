# ----------------------------------------------------------------------
# Makefile for Greenplum Demo
# ----------------------------------------------------------------------

#
# INCLUDES
#

top_builddir = ../..
-include $(top_builddir)/src/Makefile.global

#
# VARIABLES
#

export enable_gpfdist
export with_openssl

MASTER_PORT ?= 15432
PORT_BASE ?= 25432
NUM_PRIMARY_MIRROR_PAIRS ?= 3
BLDWRAP_POSTGRES_CONF_ADDONS := fsync=off $(BLDWRAP_POSTGRES_CONF_ADDONS)

ifeq ($(WITH_MIRRORS), )
WITH_MIRRORS = true
endif

export MASTER_DEMO_PORT=$(MASTER_PORT)
export DEMO_PORT_BASE=$(PORT_BASE)
export NUM_PRIMARY_MIRROR_PAIRS
export WITH_MIRRORS
export BLDWRAP_POSTGRES_CONF_ADDONS

all:
	$(MAKE) clean
	$(MAKE) check
	$(MAKE) create-demo-cluster
	$(MAKE) probe

cluster create-demo-cluster:
ifeq ($(enable_segwalrep), yes)
	@WITH_MIRRORS=false ./demo_cluster.sh # will generate gpdemo-env.sh
ifeq ($(WITH_MIRRORS), true)
	@. ./gpdemo-env.sh; \
	./gpsegwalrep.py init --host `hostname`; \
	./gpsegwalrep.py start;
endif
	@echo ""
else
	@./demo_cluster.sh
	@echo ""
endif

probe:
	@./probe_config.sh
	@echo ""

check: 
	@./demo_cluster.sh -c
	@echo ""

clean destroy-demo-cluster:
	@echo "======================================================================"
	@echo "Deleting cluster.... "
	@echo "======================================================================"
	@./demo_cluster.sh -d
	@echo ""

.PHONY: all cluster create-demo-cluster create-segwalrep-cluster probe clean check clean destroy-demo-cluster
