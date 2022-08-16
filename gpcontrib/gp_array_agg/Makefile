MODULES = gp_array_agg
OBJS = gp_array_agg.o
EXTENSION = gp_array_agg
DATA = gp_array_agg--1.0.0.sql

REGRESS = gp_array_agg
CFLAGS=`pg_config --includedir-server`

ifdef USE_PGXS
	PG_CONFIG = pg_config
	PGXS := $(shell $(PG_CONFIG) --pgxs)
	include $(PGXS)
else
  subdir = gpcontrib/gp_array_agg
  top_builddir = ../..
  include $(top_builddir)/src/Makefile.global
  include $(top_srcdir)/contrib/contrib-global.mk
endif
