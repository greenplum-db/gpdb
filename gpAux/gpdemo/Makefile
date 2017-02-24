# ----------------------------------------------------------------------
# Makefile for Greenplum Demo
# ----------------------------------------------------------------------

SHELL=/bin/bash

#
# VARIABLES
#

MASTER_PORT?=15432
PORT_BASE?=25432
NUM_PRIMARY_MIRROR_PAIRS?=3

all:
	$(MAKE) clean
	$(MAKE) check
	$(MAKE) cluster
	$(MAKE) probe

cluster:	
	@ MASTER_DEMO_PORT=$(MASTER_PORT) DEMO_PORT_BASE=$(PORT_BASE) NUM_PRIMARY_MIRROR_PAIRS=$(NUM_PRIMARY_MIRROR_PAIRS) ./demo_cluster.sh
	@ echo ""

probe:
	@ MASTER_DEMO_PORT=$(MASTER_PORT) DEMO_PORT_BASE=$(PORT_BASE) NUM_PRIMARY_MIRROR_PAIRS=$(NUM_PRIMARY_MIRROR_PAIRS) ./probe_config.sh
	@ echo ""

.PHONY : clean 

check: 
	@ MASTER_DEMO_PORT=$(MASTER_PORT) DEMO_PORT_BASE=$(PORT_BASE) NUM_PRIMARY_MIRROR_PAIRS=$(NUM_PRIMARY_MIRROR_PAIRS) ./demo_cluster.sh -c
	@ echo ""

clean:
	@ echo "======================================================================"
	@ echo "Deleting cluster.... "
	@ echo "======================================================================"
	@ MASTER_DEMO_PORT=$(MASTER_PORT) DEMO_PORT_BASE=$(PORT_BASE) NUM_PRIMARY_MIRROR_PAIRS=$(NUM_PRIMARY_MIRROR_PAIRS) ./demo_cluster.sh -d
	@ echo ""
