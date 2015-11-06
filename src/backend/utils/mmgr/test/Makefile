subdir=src/backend/utils/mmgr
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=aset mcxt memaccounting vmem_tracker redzone_handler runaway_cleaner idle_tracker event_version

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
    $(top_srcdir)/src/backend/utils/init/globals.o \
    $(top_srcdir)/src/port/exec.o \
    $(top_srcdir)/src/port/path.o \
    $(top_srcdir)/src/port/pgsleep.o \
    $(top_srcdir)/src/port/pgstrcasecmp.o \
    $(top_srcdir)/src/port/qsort.o \
    $(top_srcdir)/src/port/thread.o \
    $(top_srcdir)/src/timezone/localtime.o \
    $(top_srcdir)/src/timezone/pgtz.o \
    $(top_srcdir)/src/backend/utils/misc/size.o \

memaccounting_REAL_OBJS=$(common_REAL_OBJS) \
    $(top_srcdir)/src/backend/utils/mmgr/memprot.o \
	$(top_srcdir)/src/backend/utils/mmgr/vmem_tracker.o \
	$(top_srcdir)/src/backend/utils/mmgr/mcxt.o \
	$(top_srcdir)/src/backend/utils/mmgr/aset.o \

mcxt_REAL_OBJS=$(common_REAL_OBJS) \
    $(top_srcdir)/src/backend/utils/mmgr/memprot.o \
	$(top_srcdir)/src/backend/utils/mmgr/vmem_tracker.o \
	$(top_srcdir)/src/backend/utils/mmgr/aset.o \

aset_REAL_OBJS=$(common_REAL_OBJS) \
    $(top_srcdir)/src/backend/utils/mmgr/memprot.o \
	$(top_srcdir)/src/backend/utils/mmgr/vmem_tracker.o \
	$(top_srcdir)/src/backend/utils/mmgr/memaccounting.o \
	$(top_srcdir)/src/backend/utils/mmgr/mcxt.o \

vmem_tracker_REAL_OBJS=$(common_REAL_OBJS) \
	$(top_srcdir)/src/backend/storage/lmgr/s_lock.o \
	$(top_srcdir)/src/backend/utils/misc/atomic.o \

redzone_handler_REAL_OBJS=$(common_REAL_OBJS) \
	$(top_srcdir)/src/backend/storage/lmgr/s_lock.o \
	$(top_srcdir)/src/backend/utils/misc/atomic.o \
	$(top_srcdir)/src/backend/utils/session_state.o \

runaway_cleaner_REAL_OBJS=$(common_REAL_OBJS) \
	$(top_srcdir)/src/backend/utils/misc/atomic.o \

idle_tracker_REAL_OBJS=$(common_REAL_OBJS) \
	$(top_srcdir)/src/backend/utils/misc/atomic.o \

event_version_REAL_OBJS=$(common_REAL_OBJS) \
	$(top_srcdir)/src/backend/utils/misc/atomic.o \

include $(top_builddir)/src/Makefile.mock
