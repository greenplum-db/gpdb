subdir=src/backend/storage/lmgr
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=lock

include $(top_srcdir)/src/backend/mock.mk

lock.t: $(MOCK_DIR)/backend/access/hash/hash_mock.o \
	$(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o
