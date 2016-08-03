subdir=src/backend/cdb/dispatcher
top_builddir = ../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=cdbdisp_query \
		cdbdispatchresult \
		cdbgang

include $(top_builddir)/src/backend/mock.mk

cdbdisp_query.t: \
	$(MOCK_DIR)/backend/executor/execUtils_mock.o \
	$(MOCK_DIR)/backend/cdb/cdbrelsize_mock.o \
	$(MOCK_DIR)/backend/cdb/cdbsrlz_mock.o \
	$(MOCK_DIR)/backend/cdb/dispatcher/cdbgang_mock.o \
	$(MOCK_DIR)/backend/cdb/dispatcher/cdbdisp_dtx_mock.o \
	$(MOCK_DIR)/backend/utils/cache/lsyscache_mock.o \

cdbdispatchresult.t: \
	$(MOCK_DIR)/backend/gp_libpq_fe/pqexpbuffer_mock.o \
	$(MOCK_DIR)/backend/cdb/cdbrelsize_mock.o \
	$(MOCK_DIR)/backend/cdb/cdbsrlz_mock.o \

cdbgang.t: \
	$(MOCK_DIR)/backend/catalog/namespace_mock.o \
	$(MOCK_DIR)/backend/storage/lmgr/proc_mock.o \
	$(MOCK_DIR)/backend/access/transam/xact_mock.o \
	$(MOCK_DIR)/backend/cdb/cdbutil_mock.o \
	$(MOCK_DIR)/backend/cdb/cdbfts_mock.o \
	$(MOCK_DIR)/backend/utils/misc/superuser_mock.o \
	$(MOCK_DIR)/backend/gp_libpq_fe/fe-connect_mock.o \
	$(MOCK_DIR)/backend/utils/mmgr/redzone_handler_mock.o

include $(top_builddir)/src/backend/mock.mk
