subdir=src/backend/utils/misc
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS = ps_status bitstream bitmap_compression

ifeq ($(enable_segwalrep), yes)
TARGETS += guc_gp
endif

include $(top_builddir)/src/backend/mock.mk

guc_gp.t: \
    $(MOCK_DIR)/backend/utils/misc/superuser_mock.o \
    $(MOCK_DIR)/backend/storage/lmgr/lwlock_mock.o \
    $(MOCK_DIR)/backend/utils/mmgr/mcxt_mock.o
