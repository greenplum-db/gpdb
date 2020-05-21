subdir=src/backend/utils/cache
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=lsyscache

include $(top_srcdir)/src/backend/mock.mk

lsyscache.t: \
	$(MOCK_DIR)/backend/utils/cache/syscache_mock.o \
