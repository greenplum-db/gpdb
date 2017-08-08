MODULES = gp_inject_fault
DATA_built = gp_inject_fault.sql
REGRESS = inject_fault_test
MODULEDIR = gp_inject_fault

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/gp_inject_fault
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
