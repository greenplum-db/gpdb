subdir=src/backend/libpq
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=pqcomm auth

include $(top_builddir)/src/backend/mock.mk

pqcomm_MOCK_OBJS=\
	$(top_srcdir)/src/backend/libpq/be-secure.o \
	$(top_srcdir)/src/backend/utils/error/elog.o

auth_MOCK_OBJS=\
	$(top_srcdir)/src/backend/libpq/hba.o \
	$(top_srcdir)/src/backend/utils/adt/timestamp.o \
	$(top_srcdir)/src/backend/utils/fmgr/fmgr.o

override MOCK_LIBS+= -lldap
