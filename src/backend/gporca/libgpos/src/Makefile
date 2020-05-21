#
# Makefile for optimizer
#
# src/backend/gporca/Makefile
#

subdir = src/backend/gporca/libgpos/src
top_builddir = ../../../../..
include $(top_builddir)/src/Makefile.global

include $(top_srcdir)/src/backend/gporca/gporca.mk

SUBDIRS     = common error io memory string task
OBJS        = _api.o utils.o

include $(top_srcdir)/src/backend/common.mk

