subdir=src/backend/access/transam
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=xact distributedlog xlog

# Objects from backend, which don't need to be mocked but need to be linked.
xact_REAL_OBJS=\
	$(top_srcdir)/src/backend/utils/error/elog.o \

xlog_REAL_OBJS=\
	$(top_srcdir)/src/backend/utils/error/elog.o \

include $(top_builddir)/src/backend/mock.mk
