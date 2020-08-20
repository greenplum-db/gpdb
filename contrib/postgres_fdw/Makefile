# contrib/postgres_fdw/Makefile
# disable postgre_fdw on MaxOS as --exclude-libs and -Bstatic are not supported
OS := $(shell uname -s)
ifeq ($(OS),Darwin)
$(error "postgres_fdw can't be built on Mac")
endif

MODULE_big = postgres_fdw
OBJS = postgres_fdw.o option.o deparse.o connection.o

PG_CPPFLAGS = -I$(libpq_srcdir)
SHLIB_LINK_INTERNAL = -Wl,--exclude-libs=libpq.a -Wl,-Bstatic $(libpq) -Wl,-Bdynamic

EXTENSION = postgres_fdw
DATA = postgres_fdw--1.0.sql

REGRESS = postgres_fdw
REGRESS_OPTS = --load-extension=$(EXTENSION)

# the db name is hard-coded in the tests
override USE_MODULE_DB =

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

# For test
# If you want to change the PG_PORT, please update
# both the below PG_PORT and the port in test/sql/postgres_fdw.sql
# which is the option of foreign server
export PG_PORT=5432
installcheck: prep_postgres
prep_postgres:
	sh postgres_setup.sh

.PHONY: prep_postgres
