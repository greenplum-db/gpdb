subdir=src/backend/cdb
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=cdbbufferedread \
	cdbdistributedsnapshot

TARGETS += cdbappendonlyxlog

include $(top_srcdir)/src/backend/mock.mk

cdbdistributedsnapshot.t: $(MOCK_DIR)/backend/access/transam/distributedlog_mock.o

cdbappendonlyxlog.t: \
	$(MOCK_DIR)/backend/storage/file/fd_mock.o \
	$(MOCK_DIR)/backend/access/transam/xlogutils_mock.o
