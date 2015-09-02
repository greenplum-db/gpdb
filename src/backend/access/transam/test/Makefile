subdir=src/backend/access/transam
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=xact distributedlog xlog

include $(top_builddir)/src/backend/mock.mk

distributedlog.t: \
	$(MOCK_DIR)/backend/access/transam/slru_mock.o \
	$(MOCK_DIR)/backend/storage/lmgr/lwlock_mock.o
