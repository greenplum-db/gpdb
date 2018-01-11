subdir=src/backend/access/appendonly
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=aomd appendonly_visimap appendonlywriter

include $(top_builddir)/src/backend/mock.mk

aomd.t: $(MOCK_DIR)/backend/catalog/catalog_mock.o

appendonly_visimap.t: \
	$(MOCK_DIR)/backend/access/appendonly/appendonly_visimap_entry_mock.o \
	$(MOCK_DIR)/backend/access/appendonly/appendonly_visimap_store_mock.o \
	$(MOCK_DIR)/backend/executor/execWorkfile_mock.o \
	$(MOCK_DIR)/backend/utils/hash/dynahash_mock.o

appendonlywriter.t: \
	$(MOCK_DIR)/backend/utils/hash/dynahash_mock.o
