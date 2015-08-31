subdir=src/backend/utils/fmgr
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=dfmgr

dfmgr_REAL_OBJS=\
	$(top_srcdir)/src/backend/utils/mmgr/mcxt.o \
	$(top_srcdir)/src/backend/utils/mmgr/memaccounting.o \
	$(top_srcdir)/src/backend/utils/mmgr/aset.o \
	$(top_srcdir)/src/backend/utils/mmgr/memprot.o

include $(top_builddir)/src/backend/mock.mk
