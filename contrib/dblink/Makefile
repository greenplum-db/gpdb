# $PostgreSQL: pgsql/contrib/dblink/Makefile,v 1.15 2007/11/10 23:59:50 momjian Exp $

MODULE_big = dblink
PG_CPPFLAGS = -I$(top_srcdir)/src/backend/gp_libpq_fe
OBJS	= dblink.o

DATA_built = dblink.sql 
DATA = uninstall_dblink.sql 
REGRESS = dblink
REGRESS_OPTS = --init-file=$(top_builddir)/src/test/regress/init_file --dbname=contrib_regression

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/dblink
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

installcheck: prepare_dblink_sql

prepare_dblink_sql:
	cp $(GPHOME)/share/postgresql/contrib/dblink.sql ./dblink.sql

.PHONY: prepare_dblink_sql
