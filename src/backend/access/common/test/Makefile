subdir=src/backend/access/common
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=reloptions tupdesc

tupdesc_MOCK_OBJS=\
	$(top_srcdir)/src/backend/utils/resowner/resowner.o

include $(top_builddir)/src/backend/mock.mk
