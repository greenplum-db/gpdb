# contrib/pxf_fdw/Makefile

MODULE_big = pxf_fdw
OBJS = pxf_fdw.o pxf_option.o

SHLIB_LINK += -lcurl

EXTENSION = pxf_fdw
DATA = pxf_fdw--1.0.sql

# Use logic also from gp_file_fdw where it executes on the segments
REGRESS = pxf_fdw

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pxf_fdw
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif