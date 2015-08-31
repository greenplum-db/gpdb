subdir=src/backend/utils/mmgr
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=aset mcxt memaccounting vmem_tracker redzone_handler runaway_cleaner idle_tracker event_version

memaccounting_REAL_OBJS=\
    $(top_srcdir)/src/backend/utils/mmgr/memprot.o \
	$(top_srcdir)/src/backend/utils/mmgr/vmem_tracker.o \
	$(top_srcdir)/src/backend/utils/mmgr/mcxt.o \
	$(top_srcdir)/src/backend/utils/mmgr/aset.o \

mcxt_REAL_OBJS=\
    $(top_srcdir)/src/backend/utils/mmgr/memprot.o \
	$(top_srcdir)/src/backend/utils/mmgr/vmem_tracker.o \
	$(top_srcdir)/src/backend/utils/mmgr/aset.o \

aset_REAL_OBJS=\
    $(top_srcdir)/src/backend/utils/mmgr/memprot.o \
	$(top_srcdir)/src/backend/utils/mmgr/vmem_tracker.o \
	$(top_srcdir)/src/backend/utils/mmgr/memaccounting.o \
	$(top_srcdir)/src/backend/utils/mmgr/mcxt.o \

redzone_handler_REAL_OBJS=\
	$(top_srcdir)/src/backend/storage/lmgr/s_lock.o \
	$(top_srcdir)/src/backend/utils/session_state.o \

include $(top_builddir)/src/backend/mock.mk
