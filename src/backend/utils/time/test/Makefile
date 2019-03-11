subdir=src/backend/utils/time
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=sharedsnapshot

include $(top_builddir)/src/backend/mock.mk

sharedsnapshot.t: \
	$(MOCK_DIR)/backend/storage/ipc/shmem_mock.o \
	$(MOCK_DIR)/backend/storage/lmgr/lwlock_mock.o \
