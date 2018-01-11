subdir=src/backend/utils/init
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=postinit

include $(top_builddir)/src/backend/mock.mk

postinit.t: \
	$(MOCK_DIR)/backend/utils/error/elog_mock.o \
	$(MOCK_DIR)/backend/storage/lmgr/proc_mock.o
