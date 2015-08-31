subdir=src/backend/utils/resscheduler
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=memquota

# Objects from backend, which don't need to be mocked but need to be linked.
memquota_REAL_OBJS=\
    $(top_srcdir)/src/backend/utils/hash/dynahash.o \

include $(top_builddir)/src/backend/mock.mk
