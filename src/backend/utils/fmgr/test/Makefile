subdir=src/backend/utils/fmgr
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=dfmgr

include $(top_srcdir)/src/backend/mock.mk

dfmgr.t: $(MOCK_DIR)/backend/utils/error/elog_mock.o \
	$(MOCK_DIR)/backend/access/hash/hashpage_mock.o \
	$(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o

