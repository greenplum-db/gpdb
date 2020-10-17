subdir=src/backend/utils/hash
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=dynahash

include $(top_srcdir)/src/backend/mock.mk

dynahash.t: $(MOCK_DIR)/backend/access/transam/xact_mock.o \
	$(MOCK_DIR)/backend/access/hash/hash_mock.o \
	$(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o \
