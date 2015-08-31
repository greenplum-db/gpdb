subdir=src/backend/utils/hash
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=dynahash

# Objects from backend, which don't need to be mocked but need to be linked.
dynahash_REAL_OBJS=\
	$(top_srcdir)/src/backend/utils/error/elog.o \
	$(top_srcdir)/src/backend/utils/hash/dynahash.o \
	$(top_srcdir)/src/backend/utils/mb/mbutils.o \
	$(top_srcdir)/src/backend/utils/mb/wchar.o \

include $(top_builddir)/src/backend/mock.mk
