#
# Makefile for optimizer
#
# src/backend/gporca/libgpdbcost/src/Makefile
#

subdir = src/backend/gporca/libgpdbcost/src
top_builddir = ../../../../..
include $(top_builddir)/src/Makefile.global

include $(top_srcdir)/src/backend/gporca/gporca.mk

OBJS        = CCostModelGPDB.o \
              CCostModelGPDBLegacy.o \
              CCostModelParamsGPDB.o \
              CCostModelParamsGPDBLegacy.o \
              ICostModel.o

include $(top_srcdir)/src/backend/common.mk

