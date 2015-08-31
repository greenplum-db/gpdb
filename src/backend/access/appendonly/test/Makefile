subdir=src/backend/access/appendonly
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=aomd appendonly_visimap

aomd_MOCK_OBJS=\
	$(top_srcdir)/src/backend/catalog/catalog.o

appendonly_visimap_MOCK_OBJS=\
	$(top_srcdir)/src/backend/access/appendonly/appendonly_visimap_entry.o \
	$(top_srcdir)/src/backend/access/appendonly/appendonly_visimap_store.o \
	$(top_srcdir)/src/backend/executor/execWorkfile.o \
	$(top_srcdir)/src/backend/utils/hash/dynahash.o

include $(top_builddir)/src/backend/mock.mk
