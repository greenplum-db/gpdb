# contrib/pxf_fdw/Makefile

MODULE_big = pxf_fdw
OBJS = pxf_fdw.o pxf_bridge.o pxf_deparse.o pxf_filter.o pxf_fragment.o pxf_header.o pxf_option.o libchurl.o

SHLIB_LINK += -lcurl

EXTENSION = pxf_fdw
DATA = pxf_fdw--1.0.sql

REGRESS = pxf_fdw_wrapper pxf_fdw_server pxf_fdw_user_mapping pxf_fdw_foreign_table

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = gpcontrib/pxf_fdw
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
