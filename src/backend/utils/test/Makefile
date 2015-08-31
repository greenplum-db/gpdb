subdir=src/backend/utils
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=session_state

session_state_MOCK_OBJS=\
	$(top_srcdir)/src/backend/storage/ipc/shmem.o \
	$(top_srcdir)/src/backend/storage/lmgr/lwlock.o \
	$(top_srcdir)/src/backend/utils/error/assert.o \
	$(top_srcdir)/src/backend/utils/error/elog.o \
	$(top_srcdir)/src/backend/utils/mmgr/runaway_cleaner.o

include $(top_builddir)/src/backend/mock.mk
