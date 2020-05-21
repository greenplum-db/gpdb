#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for gpopt
#
#-------------------------------------------------------------------------

subdir = src/backend/gpopt/utils
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

include $(top_srcdir)/src/backend/gpopt/gpopt.mk

OBJS = COptTasks.o CConstExprEvaluatorProxy.o CMemoryPoolPalloc.o CMemoryPoolPallocManager.o funcs.o

include $(top_srcdir)/src/backend/common.mk
