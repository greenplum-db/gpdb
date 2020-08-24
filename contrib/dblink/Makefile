# $PostgreSQL: pgsql/contrib/dblink/Makefile,v 1.15 2007/11/10 23:59:50 momjian Exp $
# disable dblink on MaxOS as --exclude-libs and -Bstatic are not supported
OS := $(shell uname -s)
ifneq ($(OS),Darwin)
	MODULE_big = dblink
	PG_CPPFLAGS = -I$(libpq_srcdir)
	SHLIB_LINK = -Wl,--exclude-libs=libpq.a -Wl,-Bstatic $(libpq) -Wl,-Bdynamic
	OBJS	= dblink.o

	DATA_built = dblink.sql
	DATA = uninstall_dblink.sql
endif

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
