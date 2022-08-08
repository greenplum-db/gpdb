#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for access/appendonly
#
#-------------------------------------------------------------------------

subdir = src/backend/access/appendonly
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global
override CPPFLAGS := -I$(libpq_srcdir) $(CPPFLAGS)

OBJS = appendonlyam_handler.o appendonlyam.o aosegfiles.o aomd.o \
	   appendonlywriter.o appendonlytid.o \
	   appendonlyblockdirectory.o appendonly_visimap.o \
	   appendonly_visimap_entry.o appendonly_visimap_store.o \
	   appendonly_compaction.o appendonly_visimap_udf.o \
	   appendonly_blkdir_udf.o aomd_filehandler.o

include $(top_srcdir)/src/backend/common.mk

