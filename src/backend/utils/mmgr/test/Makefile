subdir=src/backend/utils/mmgr
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=aset mcxt memaccounting vmem_tracker redzone_handler runaway_cleaner idle_tracker event_version

aset_MOCK_OBJS=\
	$(top_srcdir)/src/backend/utils/error/assert.o

mcxt_MOCK_OBJS=\
	$(top_srcdir)/src/backend/utils/mmgr/memaccounting.o

vmem_tracker_MOCK_OBJS=\
	$(top_srcdir)/src/backend/storage/ipc/shmem.o \
	$(top_srcdir)/src/backend/utils/error/assert.o \
	$(top_srcdir)/src/backend/utils/error/elog.o \
	$(top_srcdir)/src/backend/utils/mmgr/event_version.o \
	$(top_srcdir)/src/backend/utils/mmgr/idle_tracker.o \
	$(top_srcdir)/src/backend/utils/mmgr/memaccounting.o \
	$(top_srcdir)/src/backend/utils/mmgr/mcxt.o \
	$(top_srcdir)/src/backend/utils/mmgr/memprot.o \
	$(top_srcdir)/src/backend/utils/mmgr/redzone_handler.o \
	$(top_srcdir)/src/backend/utils/mmgr/runaway_cleaner.o

redzone_handler_MOCK_OBJS=\
	$(top_srcdir)/src/backend/storage/ipc/shmem.o \
	$(top_srcdir)/src/backend/storage/lmgr/lwlock.o \
	$(top_srcdir)/src/backend/utils/error/elog.o \
	$(top_srcdir)/src/backend/utils/mmgr/aset.o \
	$(top_srcdir)/src/backend/utils/mmgr/mcxt.o \
	$(top_srcdir)/src/backend/utils/mmgr/memaccounting.o \
	$(top_srcdir)/src/backend/utils/mmgr/memprot.o \
	$(top_srcdir)/src/backend/utils/mmgr/vmem_tracker.o

memaccounting_MOCK_OBJS=\
	$(top_srcdir)/src/backend/access/transam/xact.o \
	$(top_srcdir)/src/backend/utils/error/assert.o \
	$(top_srcdir)/src/backend/utils/error/elog.o

runaway_cleaner_MOCK_OBJS=\
	$(top_srcdir)/src/backend/utils/error/elog.o \
	$(top_srcdir)/src/backend/utils/misc/faultinjector.o \
	$(top_srcdir)/src/backend/utils/misc/superuser.o \
	$(top_srcdir)/src/backend/utils/mmgr/idle_tracker.o

idle_tracker_MOCK_OBJS=\
	$(top_srcdir)/src/backend/utils/error/assert.o \
	$(top_srcdir)/src/backend/utils/mmgr/runaway_cleaner.o

event_version_MOCK_OBJS=\
	$(top_srcdir)/src/backend/storage/ipc/shmem.o \

redzone_handler_REAL_OBJS=\
	$(top_srcdir)/src/backend/utils/session_state.o \

include $(top_builddir)/src/backend/mock.mk
