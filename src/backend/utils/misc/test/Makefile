subdir=src/backend/utils/misc
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=guc ps_status bitstream bitmap_compression

guc_REAL_OBJS=\
    $(top_srcdir)/src/backend/utils/adt/bool.o \

ps_status_REAL_OBJS=\
    $(top_srcdir)/src/backend/utils/adt/bool.o \

bitstream_REAL_OBJS=\
        $(top_srcdir)/src/backend/utils/error/elog.o \
        $(top_srcdir)/src/backend/utils/mb/mbutils.o \
        $(top_srcdir)/src/backend/utils/mb/wchar.o \

bitmap_compression_REAL_OBJS=\
	$(top_srcdir)/src/backend/utils/error/elog.o \
	$(top_srcdir)/src/backend/utils/mb/wchar.o \
	$(top_srcdir)/src/backend/utils/mb/mbutils.o

include $(top_builddir)/src/backend/mock.mk
