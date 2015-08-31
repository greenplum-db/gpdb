subdir=src/backend/access/common
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=reloptions tupdesc

# Objects from backend, which don't need to be mocked but need to be linked.
COMMON_REAL_OBJS=\
	$(top_srcdir)/src/backend/utils/hash/dynahash.o \

reloptions_REAL_OBJS=\
	$(COMMON_REAL_OBJS) \
	$(top_srcdir)/src/backend/utils/adt/bool.o \

tupdesc_REAL_OBJS=\
	$(COMMON_REAL_OBJS) \

include $(top_builddir)/src/backend/mock.mk
