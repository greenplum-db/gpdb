subdir=src/backend/cdb/dispatcher
top_builddir = ../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=cdbdispatchresult \
		cdbgang \
		cdbdisp_query

include $(top_srcdir)/src/backend/mock.mk

cdbdispatchresult.t: \
	$(MOCK_DIR)/backend/libpq/pqexpbuffer_mock.o \
	$(MOCK_DIR)/backend/cdb/cdbrelsize_mock.o \
	$(MOCK_DIR)/backend/cdb/cdbsrlz_mock.o \
	$(MOCK_DIR)/backend/access/hash/hash_mock.o \
	$(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o \


cdbgang.t: \
	$(MOCK_DIR)/backend/catalog/namespace_mock.o \
	$(MOCK_DIR)/backend/storage/lmgr/proc_mock.o \
	$(MOCK_DIR)/backend/access/transam/xact_mock.o \
	$(MOCK_DIR)/backend/cdb/cdbutil_mock.o \
	$(MOCK_DIR)/backend/cdb/cdbfts_mock.o \
	$(MOCK_DIR)/backend/utils/misc/superuser_mock.o \
	$(MOCK_DIR)/backend/libpq/fe-connect_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/redzone_handler_mock.o \
	$(MOCK_DIR)/backend/utils/misc/faultinjector_mock.o \
	$(MOCK_DIR)/backend/access/hash/hash_mock.o \
	$(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o \

cdbdisp_query.t: \
	$(MOCK_DIR)/backend/access/transam/xlog_mock.o \
	$(MOCK_DIR)/backend/libpq/fe-exec_mock.o \
	$(MOCK_DIR)/backend/libpq/fe-misc_mock.o \
	$(MOCK_DIR)/backend/cdb/cdbfts_mock.o \
	$(MOCK_DIR)/backend/utils/misc/gpexpand_mock.o \
	$(MOCK_DIR)/backend/access/hash/hash_mock.o \
	$(MOCK_DIR)/backend/storage/ipc/latch_mock.o \
	$(MOCK_DIR)/backend/replication/logical/launcher_mock.o \
	$(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o \

include $(top_srcdir)/src/backend/mock.mk
