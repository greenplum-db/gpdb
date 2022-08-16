MODULES = gp_percentile_agg
OBJS = gp_percentile_agg.o
EXTENSION = gp_percentile_agg
DATA = gp_percentile_agg--1.0.0.sql

REGRESS = gp_percentile_agg
REGRESS_OPTS = --load-extension gp_percentile_agg --init-file=$(top_builddir)/src/test/regress/init_file
CFLAGS=`pg_config --includedir-server`

ifdef USE_PGXS
	PG_CONFIG = pg_config
	PGXS := $(shell $(PG_CONFIG) --pgxs)
	include $(PGXS)
else
  subdir = gpcontrib/gp_percentile_agg
  top_builddir = ../..
  include $(top_builddir)/src/Makefile.global
  include $(top_srcdir)/contrib/contrib-global.mk
endif
