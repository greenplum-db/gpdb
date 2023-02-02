# contrib/postgres_fdw/Makefile

MODULE_big = postgres_fdw
OBJS = postgres_fdw.o option.o deparse.o connection.o shippable.o $(WIN32RES)
PGFILEDESC = "postgres_fdw - foreign data wrapper for PostgreSQL"

PG_CPPFLAGS = -I$(libpq_srcdir)
SHLIB_LINK_INTERNAL = $(libpq)

EXTENSION = postgres_fdw
DATA = postgres_fdw--1.0.sql

REGRESS = gp2pg_postgres_fdw

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
SHLIB_PREREQS = submake-libpq
subdir = contrib/postgres_fdw
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

# For postgres_fdw test
export PG_PORT=5432
installcheck: prep_postgres
clean: clean_postgres
prep_postgres:
	./postgres_setup.bash
clean_postgres:
	./postgres_clean.bash
.PHONY: prep_postgres clean_postgres
