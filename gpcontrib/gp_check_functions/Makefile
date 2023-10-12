EXTENSION = gp_check_functions
DATA = gp_check_functions--1.0.0.sql
MODULES = gp_check_functions
# REGRESS testing is covered by the main suite test 'gp_check_files' as we need the custom tablespace directory support

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = gpcontrib/gp_check_functions
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
