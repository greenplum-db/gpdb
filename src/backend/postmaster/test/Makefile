subdir=src/backend/postmaster
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=backoff \
		syslogger

backoff_REAL_OBJS=\
	$(top_srcdir)/src/backend/utils/error/elog.o \

include $(top_builddir)/src/backend/mock.mk
