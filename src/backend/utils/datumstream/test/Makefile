subdir=src/backend/utils/datumstream
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=datumstreamblock

datumstreamblock_REAL_OBJS=\
        $(top_srcdir)/src/backend/utils/error/elog.o \
        $(top_srcdir)/src/backend/utils/mb/wchar.o \
	$(top_srcdir)/src/backend/utils/datumstream/datumstream.o \
	$(top_srcdir)/src/backend/utils/error/assert.o

include $(top_builddir)/src/backend/mock.mk
