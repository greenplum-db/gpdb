#
# Makefile for optimizer
#
# src/backend/gporca/Makefile
#

subdir = src/backend/gporca/libnaucrates/src
top_builddir = ../../../../..
include $(top_builddir)/src/Makefile.global

include $(top_srcdir)/src/backend/gporca/gporca.mk

SUBDIRS     = base md operators parser statistics xml
OBJS        = CCostModelConfigSerializer.o \
              CDXLUtils.o \
              CIdGenerator.o \
              exception.o \
              init.o \
              traceflags.o

include $(top_srcdir)/src/backend/common.mk

