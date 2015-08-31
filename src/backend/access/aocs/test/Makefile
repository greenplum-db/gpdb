subdir=src/backend/access/aocs
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=aocsam

aocsam_MOCK_OBJS=\
	$(top_srcdir)/src/backend/cdb/cdbappendonlystorageread.o \
	$(top_srcdir)/src/backend/catalog/pg_attribute_encoding.o \
	$(top_srcdir)/src/backend/utils/datumstream/datumstream.o

include $(top_builddir)/src/backend/mock.mk
