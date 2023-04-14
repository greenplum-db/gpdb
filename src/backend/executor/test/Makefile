subdir=src/backend/executor
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS= instrument

include $(top_srcdir)/src/backend/mock.mk

instrument.t: $(MOCK_DIR)/backend/access/transam/distributedlog_mock.o \
	$(MOCK_DIR)/backend/access/hash/hash_mock.o \
	$(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o

