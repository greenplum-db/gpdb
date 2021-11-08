#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for endpoint (CDB2)
#
#-------------------------------------------------------------------------


subdir = src/backend/cdb/endpoint
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

override CPPFLAGS := -I$(libpq_srcdir) $(CPPFLAGS)

OBJS = cdbendpoint.o cdbendpointutils.o cdbendpointretrieve.o

include $(top_srcdir)/src/backend/common.mk
