subdir=gpAux/extensions/pxf/test
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS= libchurl pxfprotocol pxfbridge pxfheaders pxfuriparser pxfutils

include $(top_builddir)/src/backend/mock.mk

pxfheaders.t: $(MOCK_DIR)/backend/access/external/fileam_mock.o $(MOCK_DIR)/backend/catalog/pg_exttable_mock.o
