MODULES = gp_parallel_retrieve_cursor

EXTENSION = gp_parallel_retrieve_cursor
DATA = gp_parallel_retrieve_cursor--1.0.sql

OBJS = gp_parallel_retrieve_cursor.o
PG_CPPFLAGS = -I$(libpq_srcdir)

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/gp_parallel_retrieve_cursor
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
