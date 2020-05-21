subdir=src/backend/storage/ipc
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=procarray

include $(top_srcdir)/src/backend/mock.mk
procarray.t: $(MOCK_DIR)/backend/storage/lmgr/lwlock_mock.o \
			 $(MOCK_DIR)/backend/cdb/cdbtm_mock.o

