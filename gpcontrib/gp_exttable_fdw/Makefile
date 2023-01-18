MODULE_big = gp_exttable_fdw
OBJS = gp_exttable_fdw.o extaccess.o

#SHLIB_LINK += -lcurl

EXTENSION = gp_exttable_fdw
DATA = gp_exttable_fdw--1.0.sql

REGRESS = gp_exttable_fdw

HEADERS = extaccess.h

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = gpcontrib/gp_exttable_fdw
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
