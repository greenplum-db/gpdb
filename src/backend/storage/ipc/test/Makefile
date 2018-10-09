subdir=src/backend/storage/ipc
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=procarray shmem shmem_fake

include $(top_builddir)/src/backend/mock.mk
procarray.t: $(MOCK_DIR)/backend/storage/lmgr/lwlock_mock.o \
			 $(MOCK_DIR)/backend/cdb/cdbtm_mock.o

shmem.t: ../shmem.o \
		 $(MOCK_DIR)/backend/storage/lmgr/s_lock_mock.o

shmem_fake.t: $(TEST_DOUBLE_DIR)/backend/storage/ipc/shmem_fake.o