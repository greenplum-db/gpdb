EXTENSION = gp_toolkit
DATA = gp_toolkit--1.1--1.2.sql gp_toolkit--1.0--1.1.sql gp_toolkit--1.0.sql \
		gp_toolkit--1.2--1.3.sql gp_toolkit--1.3.sql
MODULE_big = gp_toolkit
ifeq ($(shell uname -s), Linux)
OBJS = resgroup.o
else
OBJS = resgroup-dummy.o
endif

REGRESS = resource_manager_restore_to_none gp_toolkit resource_manager_switch_to_queue gp_toolkit_resqueue gp_toolkit_ao_funcs
EXTRA_REGRESS_OPTS = --init-file=$(top_builddir)/src/test/regress/init_file

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = gpcontrib/gp_toolkit
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
