subdir=src/backend/access/appendonly
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=aomd appendonly_visimap

# Objects from backend, which don't need to be mocked but need to be linked.
aomd_REAL_OBJS=\
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
	$(top_srcdir)/src/backend/utils/misc/guc.o \
	$(top_srcdir)/src/backend/utils/misc/guc_gp.o \
	$(top_srcdir)/src/backend/utils/init/globals.o \
	$(top_srcdir)/src/port/path.o \
	$(top_srcdir)/src/port/pgstrcasecmp.o \
	$(top_srcdir)/src/port/qsort.o \
	$(top_srcdir)/src/port/thread.o \
	$(top_srcdir)/src/timezone/localtime.o \
	$(top_srcdir)/src/timezone/pgtz.o

appendonly_visimap_REAL_OBJS=\
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
	$(top_srcdir)/src/backend/utils/misc/guc.o \
	$(top_srcdir)/src/backend/utils/misc/guc_gp.o \
	$(top_srcdir)/src/backend/utils/init/globals.o \
	$(top_srcdir)/src/port/path.o \
	$(top_srcdir)/src/port/pgstrcasecmp.o \
	$(top_srcdir)/src/port/qsort.o \
	$(top_srcdir)/src/port/thread.o \
	$(top_srcdir)/src/timezone/localtime.o \
	$(top_srcdir)/src/timezone/pgtz.o

include $(top_builddir)/src/backend/mock.mk
