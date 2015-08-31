subdir=src/backend/commands
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=tablecmds define

# Objects from backend, which don't need to be mocked but need to be linked.
define_REAL_OBJS=\
    $(top_srcdir)/src/backend/utils/hash/dynahash.o \
    $(top_srcdir)/src/backend/catalog/namespace.o \

include $(top_builddir)/src/backend/mock.mk
