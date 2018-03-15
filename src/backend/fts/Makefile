#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for fts
#
#-------------------------------------------------------------------------


subdir = src/backend/fts
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global
override CPPFLAGS := -I$(libpq_srcdir) $(CPPFLAGS)

OBJS = fts.o ftsprobe.o
OBJS += ftsmessagehandler.o
include $(top_srcdir)/src/backend/common.mk
