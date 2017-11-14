subdir=src/backend/access/transam
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=xact distributedlog xlog

ifeq ($(enable_segwalrep), yes)
TARGETS += xlogutils xlog_mm
endif

include $(top_builddir)/src/backend/mock.mk

distributedlog.t: \
	$(MOCK_DIR)/backend/access/transam/slru_mock.o \
	$(MOCK_DIR)/backend/storage/lmgr/lwlock_mock.o

xact.t: \
	$(MOCK_DIR)/backend/access/transam/slru_mock.o \
	$(MOCK_DIR)/backend/storage/lmgr/lwlock_mock.o \
	$(MOCK_DIR)/backend/access/transam/subtrans_mock.o

xlog.t: \
    $(MOCK_DIR)/backend/replication/walsender_mock.o

xlog_mm.t: \
	$(MOCK_DIR)/backend/access/transam/xlogutils_mock.o \
	$(MOCK_DIR)/backend/cdb/cdbmirroredappendonly_mock.o
