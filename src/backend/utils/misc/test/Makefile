subdir=src/backend/utils/misc
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=ps_status bitstream bitmap_compression

include $(top_builddir)/src/backend/mock.mk

guc.t: \
	$(MOCK_DIR)/backend/storage/ipc/shmem_mock.o \
	$(MOCK_DIR)/backend/utils/misc/superuser_mock.o
