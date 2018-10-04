subdir=src/backend/cdb
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=cdbbufferedread \
	cdbsrlz \
	cdbdistributedsnapshot \
	cdbutil

TARGETS += cdbappendonlyxlog

include $(top_builddir)/src/backend/mock.mk

cdbdistributedsnapshot.t: $(MOCK_DIR)/backend/access/transam/distributedlog_mock.o

cdbappendonlyxlog.t: \
	$(MOCK_DIR)/backend/storage/file/fd_mock.o \
	$(MOCK_DIR)/backend/access/transam/xlogutils_mock.o

cdbutil.t: \
	$(MOCK_DIR)/backend/access/heap/heapam_mock.o \
	$(MOCK_DIR)/backend/access/common/scankey_mock.o \
	$(MOCK_DIR)/backend/utils/error/elog_mock.o \
	$(MOCK_DIR)/backend/access/index/genam_mock.o \
	../cdbutil.o \
    $(UNIT_TEST_HELPER_DIR)/elog_helper.o
