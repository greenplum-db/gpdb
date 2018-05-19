#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for access/common
#
# IDENTIFICATION
#    src/backend/access/common/Makefile
#
#-------------------------------------------------------------------------

subdir = src/backend/access/common
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

OBJS = heaptuple.o indextuple.o printtup.o reloptions.o scankey.o \
	tupconvert.o tupdesc.o

OBJS += bufmask.o memtuple.o reloptions_gp.o

include $(top_srcdir)/src/backend/common.mk
