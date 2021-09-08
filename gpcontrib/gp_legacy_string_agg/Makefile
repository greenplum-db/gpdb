#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for legacy one-argument string_agg
#
# By default, this builds against an existing PostgreSQL installation
# (the one identified by whichever pg_config is first in your path).
# Within a configured source tree, you can say "make NO_PGXS=1 all"
# to build using the surrounding source tree.
#
# IDENTIFICATION
#    gpcontrib/gp_legacy_string_agg/Makefile
#
#-------------------------------------------------------------------------

MODULES = gp_legacy_string_agg
EXTENSION = gp_legacy_string_agg
DATA = gp_legacy_string_agg--1.0.0.sql

REGRESS = gp_legacy_string_agg_tests
PG_CPPFLAGS = -I$(libpq_srcdir)

ifdef USE_PGXS
	PG_CONFIG = pg_config
	PGXS := $(shell $(PG_CONFIG) --pgxs)
	include $(PGXS)
else
	subdir = gpcontrib/gp_legacy_string_agg
	top_builddir = ../..
	include $(top_builddir)/src/Makefile.global
	include $(top_srcdir)/contrib/contrib-global.mk
endif
