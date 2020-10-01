subdir=src/backend/utils/resource_manager
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=memquota

include $(top_srcdir)/src/backend/mock.mk

memquota.t: $(MOCK_DIR)/backend/access/hash/hash_mock.o \
	$(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o \
