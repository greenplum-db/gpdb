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
ifeq ($(enable_segwalrep), yes)
OBJS += ftsprobehandler.o
else
OBJS += ftsfilerep.o ftsprobefilerep.o
endif
include $(top_srcdir)/src/backend/common.mk
