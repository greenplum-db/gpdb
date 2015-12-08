subdir=src/backend/utils/mdver
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=mdver_global_generation mdver_local_generation mdver_inv_translator

# Objects from backend, which don't need to be mocked but need to be linked.
common_REAL_OBJS=\
    $(top_srcdir)/src/backend/access/hash/hashfunc.o \
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
    $(top_srcdir)/src/backend/utils/adt/datum.o \
    $(top_srcdir)/src/backend/utils/adt/like.o \
    $(top_srcdir)/src/backend/utils/hash/hashfn.o \
    $(top_srcdir)/src/backend/utils/misc/guc.o \
    $(top_srcdir)/src/backend/utils/misc/guc_gp.o \
    $(top_srcdir)/src/backend/utils/init/globals.o \
    $(top_srcdir)/src/port/exec.o \
    $(top_srcdir)/src/port/path.o \
    $(top_srcdir)/src/port/pgsleep.o \
    $(top_srcdir)/src/port/pgstrcasecmp.o \
    $(top_srcdir)/src/port/qsort.o \
    $(top_srcdir)/src/port/thread.o \
    $(top_srcdir)/src/timezone/localtime.o \
    $(top_srcdir)/src/timezone/pgtz.o \
    $(top_srcdir)/src/backend/utils/misc/size.o


mdver_global_generation_REAL_OBJS=$(common_REAL_OBJS) \
    $(top_srcdir)/src/backend/utils/misc/atomic.o

mdver_local_generation_REAL_OBJS=$(common_REAL_OBJS)

mdver_inv_translator_REAL_OBJS=$(common_REAL_OBJS) \
    $(top_srcdir)/src/backend/utils/mdver/mdver_local_generation.o

include $(top_builddir)/src/Makefile.mock
