#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for utils/gp
#
# Portions Copyright (c) Greenplum 2010
# Portions Copyright (c) 2012-Present Pivotal Software, Inc.
#
#
# IDENTIFICATION
#	    src/backend/utils/gp/Makefile
#
#-------------------------------------------------------------------------

subdir = src/backend/utils/gp
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global
override CPPFLAGS := -I$(top_srcdir)/src/backend/gp_libpq_fe $(CPPFLAGS)

OBJS = segadmin.o persistentutil.o


include $(top_srcdir)/src/backend/common.mk
