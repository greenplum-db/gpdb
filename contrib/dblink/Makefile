# contrib/dblink/Makefile
# disable dblink on MaxOS as --exclude-libs and -Bstatic are not supported
OS := $(shell uname -s)
ifeq ($(OS),Darwin)
$(error "dblink can't be built on Mac")
endif

top_builddir = ../..

MODULE_big = dblink
OBJS = dblink.o fe-connect_backend.o
PG_CPPFLAGS = -I$(libpq_srcdir) -I$(top_srcdir)/src/port
SHLIB_LINK_INTERNAL = -Wl,--exclude-libs=libpq.a -Wl,-Bstatic $(libpq) -Wl,-Bdynamic

fe-connect_backend.o: fe-connect.o
	objcopy --redefine-sym PQconnectdb=backend_PQconnectdb --keep-global-symbol=backend_PQconnectdb $< fe-connect_backend.o

fe-connect.o: fe-connect.c $(top_builddir)/src/port/pg_config_paths.h
fe-connect.c: % : $(top_builddir)/src/interfaces/libpq/%
	rm -f $@ && $(LN_S) $< .

$(top_builddir)/src/port/pg_config_paths.h:
	$(MAKE) -C $(top_builddir)/src/port pg_config_paths.h

EXTENSION = dblink
DATA = dblink--1.1.sql dblink--1.0--1.1.sql dblink--unpackaged--1.0.sql

REGRESS = paths dblink dblink_no_auth
REGRESS_OPTS = --dlpath=$(top_builddir)/src/test/regress \
	--create-role=dblink_regression_test
EXTRA_CLEAN = sql/paths.sql expected/paths.out fe-connect.c fe-connect.o

# the db name is hard-coded in the tests
override USE_MODULE_DB =

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
SHLIB_PREREQS = submake-libpq
subdir = contrib/dblink
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

REGRESS_OPTS += --init-file=$(top_builddir)/src/test/regress/init_file
