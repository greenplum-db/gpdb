subdir=src/backend/storage/lmgr
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=lock lwlock

# Objects from backend, which don't need to be mocked but need to be linked.
lock_REAL_OBJS=\
	$(top_srcdir)/src/backend/utils/hash/dynahash.o \
	$(top_srcdir)/src/backend/utils/mb/mbutils.o \
	$(top_srcdir)/src/backend/utils/mb/wchar.o \

lwlock_REAL_OBJS=\
	$(top_srcdir)/src/backend/utils/error/elog.o \
	$(top_srcdir)/src/backend/utils/hash/dynahash.o \
	$(top_srcdir)/src/backend/utils/mb/mbutils.o \
	$(top_srcdir)/src/backend/utils/mb/wchar.o \

include $(top_builddir)/src/backend/mock.mk
