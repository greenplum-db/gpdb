subdir=src/backend/utils/gp
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS = segadmin

include $(top_builddir)/src/backend/mock.mk

segadmin.t: \
	$(MOCK_DIR)/backend/cdb/cdbutil_mock.o \
	$(MOCK_DIR)/backend/access/heap/heapam_mock.o \
	$(MOCK_DIR)/backend/access/common/scankey_mock.o \
	$(MOCK_DIR)/backend/utils/error/elog_mock.o \
	$(MOCK_DIR)/backend/access/index/genam_mock.o \
	$(UNIT_TEST_HELPER_DIR)/elog_helper.o
