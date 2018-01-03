subdir=src/backend/fts
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=ftsmessagehandler ftsprobe fts

include $(top_builddir)/src/backend/mock.mk

ftsmessagehandler.t: \
    $(MOCK_DIR)/backend/utils/error/assert_mock.o \
    $(MOCK_DIR)/backend/storage/lmgr/lwlock_mock.o \
    $(MOCK_DIR)/backend/replication/gp_replication_mock.o \
    $(MOCK_DIR)/backend/lib/stringinfo_mock.o \
    $(MOCK_DIR)/backend/libpq/pqformat_mock.o \
    $(MOCK_DIR)/backend/libpq/pqcomm_mock.o \
    $(MOCK_DIR)/backend/tcop/dest_mock.o \
    $(MOCK_DIR)/backend/postmaster/postmaster_mock.o

ftsprobe.t: \
    $(MOCK_DIR)/backend/utils/error/elog_mock.o \
    $(MOCK_DIR)/backend/libpq/fe-exec_mock.o \
    $(MOCK_DIR)/backend/libpq/fe-connect_mock.o \
    $(MOCK_DIR)/backend/cdb/cdbthreadlog_mock.o

fts.t: \
    $(MOCK_DIR)/backend/access/transam/xact_mock.o \
    $(MOCK_DIR)/backend/utils/time/snapmgr_mock.o \
    $(MOCK_DIR)/backend/access/heap/heapam_mock.o \
    $(MOCK_DIR)/backend/access/common/heaptuple_mock.o \
    $(MOCK_DIR)/backend/catalog/indexing_mock.o \
    $(MOCK_DIR)/backend/access/common/scankey_mock.o \
    $(MOCK_DIR)/backend/access/index/genam_mock.o
