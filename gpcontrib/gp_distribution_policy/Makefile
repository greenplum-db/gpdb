MODULE_big = gp_distribution_policy

EXTENSION = gp_distribution_policy
DATA = gp_distribution_policy--1.0.sql

OBJS       = gp_distribution_policy.o
PG_CPPFLAGS = -I$(libpq_srcdir)

REGRESS = gp_distribution_policy

ifdef USE_PGXS
PGXS := $(shell pg_config --pgxs)
include $(PGXS)
else
subdir = contrib/gp_distribution_policy
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

