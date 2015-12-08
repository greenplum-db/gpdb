subdir=src/backend/utils/misc
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=guc ps_status bitstream bitmap_compression atomic

# Objects from backend, which don't need to be mocked but need to be linked.
common_REAL_OBJS=\
    $(top_srcdir)/src/backend/access/hash/hashfunc.o \
    $(top_srcdir)/src/backend/access/heap/tuptoaster.o \
    $(top_srcdir)/src/backend/access/transam/filerepdefs.o \
    $(top_srcdir)/src/backend/bootstrap/bootparse.o \
    $(top_srcdir)/src/backend/lib/stringinfo.o \
    $(top_srcdir)/src/backend/nodes/bitmapset.o \
    $(top_srcdir)/src/backend/nodes/equalfuncs.o \
    $(top_srcdir)/src/backend/nodes/list.o \
    $(top_srcdir)/src/backend/parser/gram.o \
    $(top_srcdir)/src/backend/regex/regcomp.o \
    $(top_srcdir)/src/backend/regex/regerror.o \
    $(top_srcdir)/src/backend/regex/regexec.o \
    $(top_srcdir)/src/backend/regex/regfree.o \
    $(top_srcdir)/src/backend/storage/page/itemptr.o \
    $(top_srcdir)/src/backend/utils/adt/arrayfuncs.o \
    $(top_srcdir)/src/backend/utils/adt/arrayutils.o \
    $(top_srcdir)/src/backend/utils/adt/bool.o \
    $(top_srcdir)/src/backend/utils/adt/datum.o \
    $(top_srcdir)/src/backend/utils/adt/like.o \
    $(top_srcdir)/src/backend/utils/adt/varlena.o \
    $(top_srcdir)/src/backend/utils/fmgr/fmgr.o \
    $(top_srcdir)/src/backend/utils/hash/hashfn.o \
    $(top_srcdir)/src/backend/utils/init/globals.o \
    $(top_srcdir)/src/port/path.o \
    $(top_srcdir)/src/port/pgstrcasecmp.o \
    $(top_srcdir)/src/port/qsort.o \
    $(top_srcdir)/src/port/thread.o \
    $(top_srcdir)/src/timezone/localtime.o \
    $(top_srcdir)/src/timezone/pgtz.o

guc_REAL_OBJS=$(common_REAL_OBJS) \
    $(top_srcdir)/src/backend/utils/misc/guc_gp.o

ps_status_REAL_OBJS=$(common_REAL_OBJS) \
    $(top_srcdir)/src/backend/utils/misc/guc.o \
    $(top_srcdir)/src/backend/utils/misc/guc_gp.o

bitstream_REAL_OBJS=\
        $(top_srcdir)/src/backend/lib/stringinfo.o \
        $(top_srcdir)/src/backend/nodes/equalfuncs.o \
        $(top_srcdir)/src/backend/utils/error/elog.o \
        $(top_srcdir)/src/backend/nodes/list.o \
        $(top_srcdir)/src/backend/parser/gram.o \
        $(top_srcdir)/src/backend/regex/regcomp.o \
        $(top_srcdir)/src/backend/regex/regerror.o \
        $(top_srcdir)/src/backend/regex/regexec.o \
        $(top_srcdir)/src/backend/regex/regfree.o \
        $(top_srcdir)/src/backend/storage/page/itemptr.o \
        $(top_srcdir)/src/backend/utils/adt/datum.o \
        $(top_srcdir)/src/backend/utils/adt/like.o \
        $(top_srcdir)/src/backend/utils/hash/hashfn.o \
        $(top_srcdir)/src/backend/utils/mb/mbutils.o \
        $(top_srcdir)/src/backend/utils/mb/wchar.o \
        $(top_srcdir)/src/backend/utils/misc/guc.o \
        $(top_srcdir)/src/backend/utils/misc/guc_gp.o \
        $(top_srcdir)/src/backend/utils/init/globals.o \
        $(top_srcdir)/src/port/pgsleep.o \
        $(top_srcdir)/src/port/path.o \
        $(top_srcdir)/src/port/pgstrcasecmp.o \
        $(top_srcdir)/src/port/qsort.o \
        $(top_srcdir)/src/port/thread.o \
        $(top_srcdir)/src/timezone/localtime.o \
        $(top_srcdir)/src/timezone/strftime.o \
        $(top_srcdir)/src/timezone/pgtz.o

bitmap_compression_REAL_OBJS=\
	$(top_srcdir)/src/backend/access/hash/hashfunc.o \
	$(top_srcdir)/src/backend/access/transam/filerepdefs.o \
	$(top_srcdir)/src/backend/bootstrap/bootparse.o \
	$(top_srcdir)/src/backend/nodes/bitmapset.o \
	$(top_srcdir)/src/backend/lib/stringinfo.o \
	$(top_srcdir)/src/backend/nodes/equalfuncs.o \
	$(top_srcdir)/src/backend/nodes/list.o \
	$(top_srcdir)/src/backend/parser/gram.o \
	$(top_srcdir)/src/backend/regex/regcomp.o \
	$(top_srcdir)/src/backend/regex/regerror.o \
	$(top_srcdir)/src/backend/regex/regexec.o \
	$(top_srcdir)/src/backend/regex/regfree.o \
	$(top_srcdir)/src/backend/storage/page/itemptr.o \
	$(top_srcdir)/src/backend/utils/adt/datum.o \
	$(top_srcdir)/src/backend/utils/adt/like.o \
	$(top_srcdir)/src/backend/utils/hash/hashfn.o \
	$(top_srcdir)/src/backend/utils/misc/bitstream.o \
	$(top_srcdir)/src/backend/utils/misc/guc.o \
	$(top_srcdir)/src/backend/utils/misc/guc_gp.o \
	$(top_srcdir)/src/backend/utils/init/globals.o \
	$(top_srcdir)/src/port/path.o \
	$(top_srcdir)/src/port/pgstrcasecmp.o \
	$(top_srcdir)/src/port/qsort.o \
	$(top_srcdir)/src/port/thread.o \
	$(top_srcdir)/src/timezone/localtime.o \
	$(top_srcdir)/src/timezone/pgtz.o \
	$(top_srcdir)/src/backend/utils/error/elog.o \
	$(top_srcdir)/src/timezone/strftime.o \
	$(top_srcdir)/src/backend/utils/mb/wchar.o \
	$(top_srcdir)/src/backend/utils/mb/mbutils.o
	
atomic_REAL_OBJS=$(common_REAL_OBJS) \
	$(top_srcdir)/src/backend/utils/misc/guc.o \
	$(top_srcdir)/src/backend/utils/misc/guc_gp.o


include $(top_builddir)/src/Makefile.mock
