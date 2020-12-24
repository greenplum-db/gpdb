#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for utils/gp
#
# Portions Copyright (c) Greenplum 2010
# Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
#
#
# IDENTIFICATION
#	    src/backend/utils/gp/Makefile
#
#-------------------------------------------------------------------------

subdir = src/backend/utils/gp
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global
override CPPFLAGS := -I$(libpq_srcdir) $(CPPFLAGS)

OBJS = segadmin.o


include $(top_srcdir)/src/backend/common.mk
