#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for optimizer/plan
#
# IDENTIFICATION
#    $PostgreSQL: pgsql/src/backend/optimizer/plan/Makefile,v 1.16 2010/03/28 22:59:32 tgl Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/optimizer/plan
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

BLD_TOP = $(top_builddir)/..

OBJS = analyzejoins.o createplan.o initsplan.o planagg.o planmain.o planner.o \
	setrefs.o subselect.o \
	plangroupext.o \
	planshare.o \
	planpartition.o \
	transform.o

ifeq ($(enable_orca),yes)
OBJS += orca.o
endif

include $(top_srcdir)/src/backend/common.mk
