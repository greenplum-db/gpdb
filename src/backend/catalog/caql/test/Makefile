subdir=src/backend/catalog/caql
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=catquery caqlanalyze gram 

include $(top_builddir)/src/backend/mock.mk

caqlanalyze_MOCK_OBJS=\
	$(top_srcdir)/src/backend/access/common/scankey.o \
	$(top_srcdir)/src/backend/access/heap/heapam.o \
	$(top_srcdir)/src/backend/access/index/genam.o \
	$(top_srcdir)/src/backend/utils/cache/syscache.o

gram.t: gram_upper

gram_upper:
	$(MAKE) -C ..
