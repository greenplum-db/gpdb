subdir=src/backend/catalog/core
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=catcore

# Objects from backend, which don't need to be mocked but need to be linked.
catcore_REAL_OBJS= \
	$(top_srcdir)/src/backend/nodes/copyfuncs.o \
	$(top_srcdir)/src/backend/utils/error/elog.o \
	$(top_srcdir)/src/backend/utils/mb/mbutils.o \
	$(top_srcdir)/src/backend/utils/mb/wchar.o \

include $(top_builddir)/src/backend/mock.mk
