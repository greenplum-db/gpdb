#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for utils/mmgr
#
# IDENTIFICATION
#    src/backend/utils/mmgr/Makefile
#
#-------------------------------------------------------------------------

subdir = src/backend/utils/mmgr
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

OBJS = aset.o dsa.o freepage.o generation.o mcxt.o memdebug.o portalmem.o slab.o

OBJS += memprot.o vmem_tracker.o redzone_handler.o runaway_cleaner.o idle_tracker.o event_version.o

include $(top_srcdir)/src/backend/common.mk
