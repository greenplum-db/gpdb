subdir=src/backend/tcop
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=postgres idle_resource_cleaner

include $(top_builddir)/src/backend/mock.mk

postgres.t: \
	$(MOCK_DIR)/backend/commands/async_mock.o \
	$(MOCK_DIR)/backend/storage/ipc/sinval_mock.o \
	$(MOCK_DIR)/backend/utils/error/elog_mock.o

idle_resource_cleaner.t: \
	$(MOCK_DIR)/backend/cdb/dispatcher/cdbgang_mock.o \
	$(MOCK_DIR)/backend/storage/lmgr/proc_mock.o
