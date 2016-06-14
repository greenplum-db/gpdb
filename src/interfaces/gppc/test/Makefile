#-------------------------------------------------------------------------
#
# Makefile for src/backend/libgppc test
#
# This directory is for simple regression test for one GPDB release, plus
# one-compile multi-backend test.  Set USE_PGXS=1 to build against
# installed GPDB with pg_config, otherwise it's built with source tree.
#
# Since we keep the source identical between different GPDB releases,
# this Makefile finds GP_VERSION_NUM and runs appropriate test cases only.
# We assume user libraries use PGXS, so this Makefile is a bit too verbose
# as a simple developer example.  Essentially, user libraries can follow
# normal PGXS convention except PG_CPPFLAGS and SHLIB_LINK should be set
# pointing to gppc headers and library.  Or, they can even use only pg_config
# without PGXS.
#-------------------------------------------------------------------------

MODULE_big = gppc_test
OBJS = gppc_test.o
REGRESS = gppc_basic tablefunc

ifdef USE_PGXS

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)

PG_CPPFLAGS = -I$(shell $(PG_CONFIG) --includedir)
SHLIB_LINK = -L$(shell $(PG_CONFIG) --libdir) -lgppc
include $(PGXS)

else # !USE_PGXS

subdir = src/backend/gppc/test
top_builddir = ../../../..

SHLIB_LINK = -L../ -lgppc
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk

endif # USE_PGXS

installcheck: install
