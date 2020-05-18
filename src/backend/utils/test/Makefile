subdir=src/backend/utils
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=session_state

include $(top_srcdir)/src/backend/mock.mk

session_state.t: \
	$(MOCK_DIR)/backend/storage/ipc/shmem_mock.o \
	$(MOCK_DIR)/backend/storage/lmgr/lwlock_mock.o \
	$(MOCK_DIR)/backend/utils/error/assert_mock.o \
	$(MOCK_DIR)/backend/utils/error/elog_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/runaway_cleaner_mock.o
