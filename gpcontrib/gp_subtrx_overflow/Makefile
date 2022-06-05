EXTENSION = gp_subtrx_overflow
DATA = gp_subtrx_overflow--1.0.0.sql
REGRESS = subtrx_overflow_test
MODULES = gp_subtrx_overflow


ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = gpcontrib/gp_subtrx_overflow
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif