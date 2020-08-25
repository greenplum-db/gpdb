MODULE_big = gpexterrorhandle
PG_CPPFLAGS = -I$(libpq_srcdir)
OBJS       = gpexterrorhandle.o

PG_LIBS = $(libpq_pgport)
DATA_built = gpexterrorhandle.sql
DATA = uninstall_gpexterrorhandle.sql
REGRESS = gpexterrorhandle
REGRESS_OPTS = --init-file=$(top_builddir)/src/test/regress/init_file --dbname=contrib_regression

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/gp_error_handling
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

installcheck: prepare_gpexterrorhandle_sql

prepare_gpexterrorhandle_sql:
	cp $(GPHOME)/share/postgresql/contrib/gpexterrorhandle.sql ./gpexterrorhandle.sql

.PHONY: prepare_gpexterrorhandle_sql
