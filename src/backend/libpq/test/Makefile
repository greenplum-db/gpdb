subdir=src/backend/libpq
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=pqcomm auth

auth_REAL_OBJS=\
    $(top_srcdir)/src/port/pgsleep.o \

include $(top_builddir)/src/backend/mock.mk

auth_MOCK_OBJS=\
	$(top_srcdir)/src/backend/utils/fmgr/fmgr.o

override MOCK_LIBS+= -lldap
