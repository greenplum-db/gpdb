#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for access/bitmap
#
#  Portions Copyright (c) 2007-2010 Greenplum Inc
#  Portions Copyright (c) 2012-Present Pivotal Software, Inc.
#  Portions Copyright (c) 2006, PostgreSQL Global Development Group
#
# IDENTIFICATION
#    src/backend/access/bitmap/Makefile
#
#-------------------------------------------------------------------------

subdir = src/backend/access/bitmap
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

OBJS = bitmaputil.o bitmapattutil.o \
	bitmappages.o bitmapinsert.o bitmapsearch.o bitmap.o bitmapxlog.o

include $(top_srcdir)/src/backend/common.mk

