subdir=src/backend/tcop
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=postgres

# GPDB_93_MERGE_FIXME: idle_resource_cleaner target is removed from unit tests
# because the support for idle session and gang hooks was removed during merge
# conflict resolution. If those hooks are needed then bring it back and then
# enable the tests or delete the tests.

include $(top_srcdir)/src/backend/mock.mk

postgres.t: \
	$(MOCK_DIR)/backend/commands/async_mock.o \
	$(MOCK_DIR)/backend/storage/ipc/sinval_mock.o \
	$(MOCK_DIR)/backend/utils/error/elog_mock.o

idle_resource_cleaner.t: \
	$(MOCK_DIR)/backend/cdb/dispatcher/cdbgang_mock.o \
	$(MOCK_DIR)/backend/storage/lmgr/proc_mock.o
