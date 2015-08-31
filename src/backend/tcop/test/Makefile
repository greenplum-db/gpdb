subdir=src/backend/tcop
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=postgres

postgres_MOCK_OBJS=\
	$(top_srcdir)/src/backend/commands/async.o \
	$(top_srcdir)/src/backend/storage/ipc/sinval.o \
	$(top_srcdir)/src/backend/utils/error/elog.o

include $(top_builddir)/src/backend/mock.mk
