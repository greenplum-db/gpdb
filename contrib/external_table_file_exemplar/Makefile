# contrib/external_table_file_exemplar/Makefile

MODULES = external_table_file_exemplar

EXTENSION = external_table_file_exemplar
DATA = external_table_file_exemplar--1.0.sql

# Use logic also from gp_file_fdw where it executes on the segments
REGRESS = external_table_file_exemplar

EXTRA_CLEAN = sql/external_table_file_exemplar.sql expected/external_table_file_exemplar.out

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/external_table_file_exemplar
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
