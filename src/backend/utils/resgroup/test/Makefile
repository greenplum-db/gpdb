subdir=src/backend/utils/resgroup
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=resgroup

include $(top_srcdir)/src/backend/mock.mk

resgroup.t: \
	$(MOCK_DIR)/backend/commands/resgroupcmds_mock.o \
	$(MOCK_DIR)/backend/utils/init/miscinit_mock.o \
	$(MOCK_DIR)/backend/utils/misc/superuser_mock.o
