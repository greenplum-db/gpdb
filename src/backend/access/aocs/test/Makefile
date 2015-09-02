subdir=src/backend/access/aocs
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=aocsam

include $(top_builddir)/src/backend/mock.mk

aocsam.t: \
	$(MOCK_DIR)/backend/cdb/cdbappendonlystorageread_mock.o \
	$(MOCK_DIR)/backend/catalog/pg_attribute_encoding_mock.o \
	$(MOCK_DIR)/backend/utils/datumstream/datumstream_mock.o
