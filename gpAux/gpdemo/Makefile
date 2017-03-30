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

export MASTER_DEMO_PORT=$(MASTER_PORT)
export DEMO_PORT_BASE=$(PORT_BASE)
export NUM_PRIMARY_MIRROR_PAIRS

all:
	$(MAKE) clean
	$(MAKE) check
	$(MAKE) cluster
	$(MAKE) probe

cluster:
	@./demo_cluster.sh
	@echo ""

probe:
	@./probe_config.sh
	@echo ""

check: 
	@./demo_cluster.sh -c
	@echo ""

clean:
	@echo "======================================================================"
	@echo "Deleting cluster.... "
	@echo "======================================================================"
	@./demo_cluster.sh -d
	@echo ""

.PHONY: all cluster probe clean check clean
