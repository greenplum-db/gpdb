subdir=src/bin/pg_dump
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=dumputils

override CPPFLAGS+= -I$(top_srcdir)/src/interfaces/libpq 

include $(top_srcdir)/src/Makefile.mock

dumputils.t: dumputils_test.o $(CMOCKERY_OBJS)
	$(CC) $^ $(libpq_pgport) $(LDFLAGS) $(LIBS) $(libpq) -o $@
