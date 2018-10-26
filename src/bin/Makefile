#-------------------------------------------------------------------------
#
# Makefile for src/bin (client programs)
#
# Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
# Portions Copyright (c) 1994, Regents of the University of California
#
# src/bin/Makefile
#
#-------------------------------------------------------------------------

subdir = src/bin
top_builddir = ../..
include $(top_builddir)/src/Makefile.global

unittest-check:
	$(MAKE) -C pg_dump/test check

SUBDIRS = initdb pg_ctl pg_dump \
	psql scripts pg_config pg_controldata pg_resetxlog pg_basebackup pg_rewind

ifeq ($(PORTNAME), win32)
SUBDIRS += pgevent
else
ALWAYS_SUBDIRS += pgevent
endif
ifeq ($(enable_gpfdist), yes)
SUBDIRS+=gpfdist
endif

$(recurse)
$(recurse_always)
