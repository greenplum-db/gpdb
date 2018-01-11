subdir=src/backend/utils/gp
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=persistentutil

include $(top_builddir)/src/backend/mock.mk

persistentutil.t: \
	$(MOCK_DIR)/backend/storage/file/fd_mock.o \
	$(MOCK_DIR)/backend/access/heap/heapam_mock.o \
	$(MOCK_DIR)/backend/utils/fmgr/funcapi_mock.o
