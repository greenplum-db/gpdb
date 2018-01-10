subdir=src/backend/cdb
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=cdbtm \
	cdbbufferedread \
	cdbsrlz \
	cdbdistributedsnapshot

TARGETS += cdbappendonlystorage cdbappendonlyxlog

include $(top_builddir)/src/backend/mock.mk
cdbtm.t: $(MOCK_DIR)/backend/storage/lmgr/lwlock_mock.o

cdbdistributedsnapshot.t: $(MOCK_DIR)/backend/access/transam/distributedlog_mock.o

cdbappendonlystorage.t: \
	$(MOCK_DIR)/backend/access/transam/xlog_mock.o \
	$(MOCK_DIR)/backend/cdb/cdbappendonlyxlog_mock.o

cdbappendonlyxlog.t: \
	$(MOCK_DIR)/backend/storage/file/fd_mock.o \
	$(MOCK_DIR)/backend/access/transam/xlogutils_mock.o
