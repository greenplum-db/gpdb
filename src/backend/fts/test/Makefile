subdir=src/backend/fts
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

ifeq ($(enable_segwalrep), yes)
TARGETS=ftsprobehandler

include $(top_builddir)/src/backend/mock.mk

ftsprobehandler.t: \
    $(MOCK_DIR)/backend/utils/error/assert_mock.o \
    $(MOCK_DIR)/backend/utils/error/elog_mock.o \
    $(MOCK_DIR)/backend/storage/lmgr/lwlock_mock.o \
    $(MOCK_DIR)/backend/replication/gp_replication_mock.o \
    $(MOCK_DIR)/backend/lib/stringinfo_mock.o \
    $(MOCK_DIR)/backend/libpq/pqformat_mock.o \
    $(MOCK_DIR)/backend/libpq/pqcomm_mock.o
endif
