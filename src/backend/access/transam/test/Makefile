subdir=src/backend/access/transam
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=xact distributedlog xlog varsup

include $(top_srcdir)/src/backend/mock.mk

distributedlog.t: \
	$(MOCK_DIR)/backend/access/transam/slru_mock.o \
	$(MOCK_DIR)/backend/storage/lmgr/lwlock_mock.o

xact.t: \
	$(MOCK_DIR)/backend/access/transam/slru_mock.o \
	$(MOCK_DIR)/backend/storage/lmgr/lwlock_mock.o \
	$(MOCK_DIR)/backend/access/transam/subtrans_mock.o

xlog.t: \
	$(MOCK_DIR)/backend/replication/walsender_mock.o \
	$(MOCK_DIR)/backend/utils/misc/faultinjector_mock.o

varsup.t: \
	$(MOCK_DIR)/backend/access/transam/clog_mock.o \
	$(MOCK_DIR)/backend/access/transam/distributedlog_mock.o \
	$(MOCK_DIR)/backend/access/transam/subtrans_mock.o \
	$(MOCK_DIR)/backend/access/transam/xlog_mock.o \
	$(MOCK_DIR)/backend/access/transam/commit_ts_mock.o \
	$(MOCK_DIR)/backend/commands/dbcommands_mock.o \
	$(MOCK_DIR)/backend/storage/lmgr/lwlock_mock.o \
	$(MOCK_DIR)/backend/utils/error/elog_mock.o
