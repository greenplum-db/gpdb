subdir=src/backend/cdb
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=cdbbufferedread \
	cdbbackup \
	cdbdisp \
	cdbfilerep \
	cdbgang

cdbgang_MOCK_OBJS=\
	$(top_srcdir)/src/backend/catalog/namespace.o \
	$(top_srcdir)/src/backend/storage/lmgr/proc.o \
	$(top_srcdir)/src/backend/utils/mmgr/redzone_handler.o

cdbdisp_MOCK_OBJS=\
	$(top_srcdir)/src/backend/cdb/cdbrelsize.o \
	$(top_srcdir)/src/backend/cdb/cdbsrlz.o \
	$(top_srcdir)/src/backend/utils/error/assert.o \
	$(top_srcdir)/src/backend/executor/execUtils.o

cdbfilerep_MOCK_OBJS=\
	$(top_srcdir)/src/backend/postmaster/fork_process.o \
	$(top_srcdir)/src/backend/utils/mmgr/redzone_handler.o

include $(top_builddir)/src/backend/mock.mk
