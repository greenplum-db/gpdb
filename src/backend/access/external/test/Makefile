subdir=src/backend/access/external
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=url

# Objects from backend, which don't need to be mocked but need to be linked.
COMMON_REAL_OBJS=\
	$(top_srcdir)/src/backend/utils/error/elog.o \

url_REAL_OBJS=$(COMMON_REAL_OBJS)

include $(top_builddir)/src/backend/mock.mk
