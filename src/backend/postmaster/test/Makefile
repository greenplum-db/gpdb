subdir=src/backend/postmaster
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=backoff \
		syslogger

backoff_MOCK_OBJS=\
	$(top_srcdir)/src/backend/utils/error/assert.o

include $(top_builddir)/src/backend/mock.mk
