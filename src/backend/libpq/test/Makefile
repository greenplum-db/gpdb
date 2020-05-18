subdir=src/backend/libpq
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=pqcomm auth

include $(top_srcdir)/src/backend/mock.mk

auth.t: $(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o

pqcomm.t: \
	$(MOCK_DIR)/backend/libpq/be-secure_mock.o \
	$(MOCK_DIR)/backend/utils/error/elog_mock.o

auth.t: \
	$(MOCK_DIR)/backend/libpq/hba_mock.o \
	$(MOCK_DIR)/backend/utils/adt/timestamp_mock.o \
	$(MOCK_DIR)/backend/utils/cache/syscache_mock.o \
	$(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o
