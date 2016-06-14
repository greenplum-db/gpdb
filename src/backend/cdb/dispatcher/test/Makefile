subdir=src/backend/cdb/dispatcher
top_builddir = ../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=cdbdisp_query \
		cdbdispatchresult

include $(top_builddir)/src/backend/mock.mk

cdbdisp_query.t: \
	$(MOCK_DIR)/backend/executor/execUtils_mock.o \
	$(MOCK_DIR)/backend/cdb/cdbrelsize_mock.o \
	$(MOCK_DIR)/backend/cdb/cdbsrlz_mock.o \

cdbdispatchresult.t: \
	$(MOCK_DIR)/backend/gp_libpq_fe/pqexpbuffer_mock.o \
	$(MOCK_DIR)/backend/cdb/cdbrelsize_mock.o \
	$(MOCK_DIR)/backend/cdb/cdbsrlz_mock.o \

include $(top_builddir)/src/backend/mock.mk
