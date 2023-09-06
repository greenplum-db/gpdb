MODULE_big = gp_inject_fault
OBJS = gp_inject_fault.o

EXTENSION = gp_inject_fault
DATA = gp_inject_fault--1.0.sql

REGRESS = inject_fault_test

PG_CPPFLAGS = -I$(libpq_srcdir)
SHLIB_LINK_INTERNAL = $(libpq)

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
