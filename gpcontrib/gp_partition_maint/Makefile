MODULES = gp_partition_maint

EXTENSION = gp_partition_maint
DATA = gp_partition_maint--1.0.sql

REGRESS = partition_maint_test

PG_CPPFLAGS = -I$(libpq_srcdir)

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/gp_partition_maint
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
