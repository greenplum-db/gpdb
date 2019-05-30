subdir=gpcontrib/pxf
top_builddir=../../..
include $(top_builddir)/src/Makefile.global

TARGETS= libchurl pxfprotocol pxfbridge pxfheaders pxfuriparser pxfutils pxffragment pxffilters

include $(top_builddir)/src/backend/mock.mk

pxfheaders.t: $(MOCK_DIR)/backend/access/external/fileam_mock.o $(MOCK_DIR)/backend/catalog/pg_exttable_mock.o

pxffragment.t: $(MOCK_DIR)/backend/cdb/cdbtm_mock.o $(top_builddir)/src/backend/utils/adt/json.o $(MOCK_DIR)/backend/cdb/cdbutil_mock.o


pxfbridge.t: $(MOCK_DIR)/backend/cdb/cdbtm_mock.o

pxffilters.t: $(MOCK_DIR)/backend/utils/cache/lsyscache_mock.o $(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o $(MOCK_DIR)/backend/utils/adt/arrayfuncs_mock.o
