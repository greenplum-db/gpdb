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

REGRESS =
ifeq ($(TEST_PGFDW),1)
REGRESS += gp2pg_postgres_fdw
endif
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

# For postgres_fdw test
# If you want to change the PG_PORT, please update
# both the below PG_PORT and the port in test/sql/postgres_fdw.sql
# which is the option of foreign server

ifeq ($(TEST_PGFDW),1)
installcheck: prep_postgres
clean: clean_postgres
export PG_PORT=5432
prep_postgres:
	./postgres_setup.bash
clean_postgres:
	./postgres_clean.bash
.PHONY: prep_postgres clean_postgres
endif
