#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for dispatcher(CDB2)
#
#-------------------------------------------------------------------------

subdir = src/backend/cdb/dispatcher
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

override CPPFLAGS += -I$(top_srcdir)/src/backend/gp_libpq_fe -I$(top_srcdir)/src/port -I$(top_srcdir)/src/backend/utils/misc

OBJS = cdbconn.o cdbdisp.o cdbdisp_thread.o cdbdisp_async.o cdbdispatchresult.o cdbdisp_dtx.o cdbdisp_query.o cdbgang.o cdbgang_thread.o cdbgang_async.o cdbpq.o
include $(top_srcdir)/src/backend/common.mk
