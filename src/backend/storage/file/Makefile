#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for storage/file
#
# IDENTIFICATION
#    $PostgreSQL: pgsql/src/backend/storage/file/Makefile,v 1.14 2010/07/02 17:03:30 rhaas Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/storage/file
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

OBJS = fd.o buffile.o copydir.o bfz.o compress_nothing.o compress_zlib.o \
	   gp_compress.o

include $(top_srcdir)/src/backend/common.mk
