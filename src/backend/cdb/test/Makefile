subdir=src/backend/cdb
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=cdbtm \
	cdbbufferedread \
	cdbbackup \
	cdbfilerep \
	cdbsrlz \
	cdbdistributedsnapshot

ifeq ($(enable_segwalrep), yes)
TARGETS += cdbappendonlystorage cdbmirroredappendonly
endif

include $(top_builddir)/src/backend/mock.mk
cdbtm.t: $(MOCK_DIR)/backend/storage/lmgr/lwlock_mock.o

cdbfilerep.t: \
	$(MOCK_DIR)/backend/postmaster/fork_process_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/redzone_handler_mock.o

cdbdistributedsnapshot.t: $(MOCK_DIR)/backend/access/transam/distributedlog_mock.o

cdbappendonlystorage.t: \
	$(MOCK_DIR)/backend/access/transam/xlog_mock.o \
	$(MOCK_DIR)/backend/cdb/cdbmirroredappendonly_mock.o

cdbmirroredappendonly.t: \
	$(MOCK_DIR)/backend/storage/file/fd_mock.o \
	$(MOCK_DIR)/backend/access/transam/xlogutils_mock.o
