#
# Makefile for optimizer
#
# src/backend/gporca/libgpopt/src/Makefile
#

subdir = src/backend/gporca/libgpopt/src
top_builddir = ../../../../..
include $(top_builddir)/src/Makefile.global

include $(top_srcdir)/src/backend/gporca/gporca.mk

SUBDIRS     = base engine eval mdcache metadata minidump operators optimizer search translate xforms
OBJS        = exception.o init.o

include $(top_srcdir)/src/backend/common.mk

