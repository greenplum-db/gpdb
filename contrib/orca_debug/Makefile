MODULE_big = orca_debug
OBJS = orca_debug.o
DATA_built = orca_debug.sql
DATA = uninstall_orca_debug.sql

REGRESS = orca_udfs

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/orca_debug
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

override CPPFLAGS := -fPIC $(CPPFLAGS)

CC=${CXX}
LDFLAGS_SL+=-lgpopt -lnaucrates -lgpdbcost -lgpos  -lxerces-c-3.1

orca_debug.o: orca_debug.cpp

