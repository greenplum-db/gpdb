subdir=src/backend/commands
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=tablecmds

include $(top_srcdir)/src/backend/mock.mk

tablecmds.t: \
	$(MOCK_DIR)/backend/access/aocs/aocs_compaction_mock.o \
	$(MOCK_DIR)/backend/access/hash/hash_mock.o \
	$(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o
