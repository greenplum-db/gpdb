# src/test/modules/test_planner/Makefile

MODULE_big = test_planner

OBJS = test_planner.o \
	src/planner_test_helpers.o \
	src/assertions.o \
	integration_tests/planner_integration_tests.o \
	$(WIN32RES)

PGFILEDESC = "test_planner"

EXTENSION = test_planner
DATA = test_planner--1.0.sql

REGRESS = test_planner

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = src/test/modules/test_planner
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

installcheck: install

test: clean all install
	psql postgres -f sql/test_planner.sql 2>&1
