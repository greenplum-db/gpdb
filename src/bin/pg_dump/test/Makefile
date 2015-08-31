subdir=src/bin/pg_dump
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=dumputils

override CPPFLAGS+= -I$(top_srcdir)/src/interfaces/libpq 

include $(top_builddir)/src/Makefile.mock

# The frontend doesn't need everything that's in LIBS, some are backend only
LIBS := $(filter-out -lresolv, $(LIBS))
# This program isn't interactive, so doesn't need these
LIBS := $(filter-out -lreadline -ledit -ltermcap -lncurses -lcurses -lcurl -lssl -lcrypto, $(LIBS))

dumputils.t: dumputils_test.o $(CMOCKERY_OBJS) ../keywords.o ../kwlookup.o
	$(CC) $^ $(libpq_pgport) $(LDFLAGS) $(LIBS) $(libpq) -o $@
