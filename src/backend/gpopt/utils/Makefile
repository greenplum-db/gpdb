#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for gpopt
#
#-------------------------------------------------------------------------

subdir = src/backend/gpopt/utils
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

BLD_TOP = $(top_builddir)/gpAux
include $(BLD_TOP)/releng/releng.mk

include $(top_builddir)/src/backend/gpopt/gpopt.mk

OBJS = COptTasks.o CCatalogUtils.o CConstExprEvaluatorProxy.o nodeutils.o funcs.o

include $(top_srcdir)/src/backend/common.mk
