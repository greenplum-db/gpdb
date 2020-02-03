#-------------------------------------------------------------------------
#
# Makefile for src/test
#
# Copyright (c) 1994, Regents of the University of California
#
# src/test/Makefile
#
#-------------------------------------------------------------------------

subdir = src/test
top_builddir = ../..
include $(top_builddir)/src/Makefile.global

SUBDIRS = perl regress isolation

SUBDIRS += walrep isolation2 kerberos modules
ifeq ($(enable_faultinjector),yes)
SUBDIRS += fsync heap_checksum
endif

$(recurse)
