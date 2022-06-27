EXTENSION = gp_subtransaction_overflow
DATA = gp_subtransaction_overflow--1.0.0.sql
REGRESS = subtransaction_overflow_test
MODULES = gp_subtransaction_overflow


ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = gpcontrib/gp_subtransaction_overflow
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
