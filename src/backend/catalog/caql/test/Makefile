subdir=src/backend/catalog/caql
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=catquery caqlanalyze gram 

COMMON_REAL_OBJS = \
	$(top_srcdir)/src/backend/nodes/copyfuncs.o \
	$(top_srcdir)/src/backend/utils/error/elog.o \
	$(top_srcdir)/src/backend/utils/mb/mbutils.o \
	$(top_srcdir)/src/backend/utils/mb/wchar.o \

caqlanalyze_REAL_OBJS=$(COMMON_REAL_OBJS)

gram_REAL_OBJS=$(COMMON_REAL_OBJS) \
	$(top_srcdir)/src/backend/catalog/caql/gram.o \

catquery_REAL_OBJS=$(COMMON_REAL_OBJS)

include $(top_builddir)/src/backend/mock.mk

gram.t: gram_upper

gram_upper:
	$(MAKE) -C ..
