subdir=src/backend/utils/adt
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=date varlena

varlena_MOCK_OBJS=\
	$(top_srcdir)/src/backend/utils/error/assert.o \
	$(top_srcdir)/src/backend/utils/mb/mbutils.o \
	$(top_srcdir)/src/backend/utils/mb/wchar.o

include $(top_builddir)/src/backend/mock.mk
