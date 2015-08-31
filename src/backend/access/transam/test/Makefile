subdir=src/backend/access/transam
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=xact distributedlog xlog

distributedlog_MOCK_OBJS=\
	$(top_srcdir)/src/backend/access/transam/slru.o \
	$(top_srcdir)/src/backend/storage/lmgr/lwlock.o

include $(top_builddir)/src/backend/mock.mk
