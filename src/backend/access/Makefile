#
# Makefile for the access methods module
#
# src/backend/access/Makefile
#

subdir = src/backend/access
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

SUBDIRS	    = brin common gin gist hash heap index nbtree rmgrdesc spgist \
			  table tablesample transam

# GPDB specific subdirs are added here to maintain
# alignment with upstream
SUBDIRS	  += external   \
	     bitmap     \
	     appendonly \
	     aocs

include $(top_srcdir)/src/backend/common.mk
