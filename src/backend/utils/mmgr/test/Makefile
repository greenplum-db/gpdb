subdir=src/backend/utils/mmgr
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=aset memaccounting vmem_tracker redzone_handler runaway_cleaner idle_tracker event_version memprot

include $(top_builddir)/src/backend/mock.mk

aset.t: $(MOCK_DIR)/backend/utils/error/assert_mock.o

vmem_tracker.t: \
	$(MOCK_DIR)/backend/storage/ipc/shmem_mock.o \
	$(MOCK_DIR)/backend/utils/error/assert_mock.o \
	$(MOCK_DIR)/backend/utils/error/elog_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/event_version_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/idle_tracker_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/memaccounting_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/mcxt_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/memprot_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/redzone_handler_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/runaway_cleaner_mock.o

memprot.t: \
	$(MOCK_DIR)/backend/utils/mmgr/vmem_tracker_mock.o \
	$(MOCK_DIR)/backend/utils/error/assert_mock.o \
	$(MOCK_DIR)/backend/utils/error/elog_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/event_version_mock.o 

redzone_handler.t: \
	$(MOCK_DIR)/backend/storage/ipc/shmem_mock.o \
	$(MOCK_DIR)/backend/storage/lmgr/lwlock_mock.o \
	$(MOCK_DIR)/backend/utils/error/elog_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/aset_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/mcxt_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/memaccounting_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/memprot_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/vmem_tracker_mock.o

memaccounting.t: \
	$(MOCK_DIR)/backend/access/transam/xact_mock.o \
	$(MOCK_DIR)/backend/utils/error/assert_mock.o \
	$(MOCK_DIR)/backend/utils/error/elog_mock.o

runaway_cleaner.t: \
	$(MOCK_DIR)/backend/utils/error/elog_mock.o \
	$(MOCK_DIR)/backend/access/transam/xact_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/mcxt_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/memaccounting_mock.o \
	$(MOCK_DIR)/backend/utils/misc/faultinjector_mock.o \
	$(MOCK_DIR)/backend/utils/misc/superuser_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/idle_tracker_mock.o

idle_tracker.t: \
	$(MOCK_DIR)/backend/utils/error/assert_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/runaway_cleaner_mock.o

event_version.t: $(MOCK_DIR)/backend/storage/ipc/shmem_mock.o
