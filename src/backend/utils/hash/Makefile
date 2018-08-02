#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for utils/hash
#
# IDENTIFICATION
#    src/backend/utils/hash/Makefile
#
#-------------------------------------------------------------------------

subdir = src/backend/utils/hash
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

#override CPPFLAGS :=-msse4.2 $(CPPFLAGS)

OBJS = dynahash.o hashfn.o

include $(top_srcdir)/src/backend/common.mk
