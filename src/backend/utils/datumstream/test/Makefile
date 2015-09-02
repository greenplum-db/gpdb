subdir=src/backend/utils/datumstream
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=datumstreamblock

datumstreamblock_REAL_OBJS=\
        $(top_srcdir)/src/backend/utils/error/elog.o \
        $(top_srcdir)/src/backend/parser/gram.o \
        $(top_srcdir)/src/backend/utils/adt/datum.o \
        $(top_srcdir)/src/backend/utils/adt/like.o \
        $(top_srcdir)/src/backend/utils/mb/wchar.o \
        $(top_srcdir)/src/backend/utils/misc/guc.o \
        $(top_srcdir)/src/backend/utils/misc/guc_gp.o \
        $(top_srcdir)/src/backend/utils/init/globals.o \
        $(top_srcdir)/src/timezone/localtime.o \
        $(top_srcdir)/src/timezone/strftime.o \
        $(top_srcdir)/src/timezone/pgtz.o \
	$(top_srcdir)/src/backend/utils/datumstream/datumstream.o \
	$(top_srcdir)/src/backend/utils/error/assert.o

include $(top_builddir)/src/backend/mock.mk
