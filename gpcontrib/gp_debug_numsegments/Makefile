MODULE_big = gp_debug_numsegments

EXTENSION = gp_debug_numsegments
DATA = gp_debug_numsegments--1.0.sql

OBJS       = gp_debug_numsegments.o
PG_CPPFLAGS = -I$(libpq_srcdir)

REGRESS = default_numsegments reset_numsegments

ifdef USE_PGXS
PGXS := $(shell pg_config --pgxs)
include $(PGXS)
else
subdir = contrib/gp_debug_numsegments
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
