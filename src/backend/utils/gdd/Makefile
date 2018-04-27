#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for utils/gdd
#
# IDENTIFICATION
#    src/backend/utils/gdd/Makefile
#
#-------------------------------------------------------------------------

subdir = src/backend/utils/gdd
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global
override CPPFLAGS := -I$(libpq_srcdir) $(CPPFLAGS)

OBJS = gddfuncs.o gddbackend.o gdddetector.o

include $(top_srcdir)/src/backend/common.mk
