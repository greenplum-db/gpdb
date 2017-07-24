#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for fts
#
#-------------------------------------------------------------------------


subdir = src/backend/fts
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global
override CPPFLAGS := -I$(top_srcdir)/src/backend/gp_libpq_fe $(CPPFLAGS)

OBJS = fts.o ftsprobe.o

ifeq ($(enable_segwalrep), yes)
OBJS := $(OBJS) ftswalrep.o
else
OBJS := $(OBJS) ftsfilerep.o
endif

include $(top_srcdir)/src/backend/common.mk
