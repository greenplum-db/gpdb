subdir=src/backend/catalog/caql
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=caqlanalyze gram

include $(top_builddir)/src/backend/mock.mk

caqlanalyze.t: \
	$(MOCK_DIR)/backend/access/common/scankey_mock.o \
	$(MOCK_DIR)/backend/access/heap/heapam_mock.o \
	$(MOCK_DIR)/backend/access/index/genam_mock.o \
	$(MOCK_DIR)/backend/utils/cache/syscache_mock.o
