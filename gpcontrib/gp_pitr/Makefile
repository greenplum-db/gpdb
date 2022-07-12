EXTENSION = gp_pitr
MODULES = gp_pitr

EXTENSION_VERSION = 1.1


DATA = gp_pitr--1.1.sql gp_pitr--1.0--1.1.sql

OBJS = gp_pitr.o

MODULE_big = gp_pitr


ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = gpcontrib/gp_pitr
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
