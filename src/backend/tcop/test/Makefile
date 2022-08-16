subdir=src/backend/tcop
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=postgres

include $(top_srcdir)/src/backend/mock.mk

postgres.t: \
	$(MOCK_DIR)/backend/commands/async_mock.o \
	$(MOCK_DIR)/backend/storage/ipc/sinval_mock.o \
	$(MOCK_DIR)/backend/utils/error/elog_mock.o \
	$(MOCK_DIR)/backend/access/hash/hash_mock.o \
	$(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o \
