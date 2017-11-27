#-------------------------------------------------------------------------
#
# Makefile for src/bin/scripts
#
# Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
# Portions Copyright (c) 1994, Regents of the University of California
#
# $PostgreSQL: pgsql/src/bin/scripts/Makefile,v 1.42 2009/03/07 02:09:22 alvherre Exp $
#
#-------------------------------------------------------------------------

PGFILEDESC = "PostgreSQL utility"
subdir = src/bin/scripts
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

# The frontend doesn't need everything that's in LIBS, some are backend only
LIBS := $(filter-out -lresolv -lbz2, $(LIBS))
# This program isn't interactive, so doesn't need these
LIBS := $(filter-out -lreadline -ledit -ltermcap -lncurses -lcurses -lcurl -lssl -lcrypto -lz, $(LIBS))


PROGRAMS = createdb createlang createuser dropdb droplang dropuser clusterdb vacuumdb reindexdb

override CPPFLAGS := -I$(top_srcdir)/src/bin/pg_dump -I$(top_srcdir)/src/bin/psql -I$(libpq_srcdir) $(CPPFLAGS)

# I don't think we really need libpgport for any of the scripts, but
# if we don't have it, we'd need to filter out -lpgport, otherwise we get a link error on solaris and win32
# we should revisit this and see if that is the better way to go.

all: submake-libpq submake-libpgport $(PROGRAMS)

%: %.o $(WIN32RES)
	$(CC) $(CFLAGS) $^ $(libpq_pgport) $(LDFLAGS) $(LIBS) -o $@$(X)

createdb: createdb.o common.o dumputils.o kwlookup.o keywords.o
createlang: createlang.o common.o print.o mbprint.o
createuser: createuser.o common.o dumputils.o kwlookup.o keywords.o
dropdb: dropdb.o common.o dumputils.o kwlookup.o keywords.o
droplang: droplang.o common.o print.o mbprint.o
dropuser: dropuser.o common.o dumputils.o kwlookup.o keywords.o
clusterdb: clusterdb.o common.o dumputils.o kwlookup.o keywords.o
vacuumdb: vacuumdb.o common.o
reindexdb: reindexdb.o common.o dumputils.o kwlookup.o keywords.o

dumputils.c keywords.c: % : $(top_srcdir)/src/bin/pg_dump/%
	rm -f $@ && $(LN_S) $< .

print.c mbprint.c : % : $(top_srcdir)/src/bin/psql/%
	rm -f $@ && $(LN_S) $< .

kwlookup.c: % : $(top_srcdir)/src/backend/parser/%
	rm -f $@ && $(LN_S) $< .

install: all installdirs
	$(INSTALL_PROGRAM) createdb$(X)   '$(DESTDIR)$(bindir)'/createdb$(X)
	$(INSTALL_PROGRAM) dropdb$(X)     '$(DESTDIR)$(bindir)'/dropdb$(X)
	$(INSTALL_PROGRAM) createlang$(X) '$(DESTDIR)$(bindir)'/createlang$(X)
	$(INSTALL_PROGRAM) droplang$(X)   '$(DESTDIR)$(bindir)'/droplang$(X)
	$(INSTALL_PROGRAM) createuser$(X) '$(DESTDIR)$(bindir)'/createuser$(X)
	$(INSTALL_PROGRAM) dropuser$(X)   '$(DESTDIR)$(bindir)'/dropuser$(X)
	$(INSTALL_PROGRAM) clusterdb$(X)  '$(DESTDIR)$(bindir)'/clusterdb$(X)
	$(INSTALL_PROGRAM) vacuumdb$(X)   '$(DESTDIR)$(bindir)'/vacuumdb$(X)
	$(INSTALL_PROGRAM) reindexdb$(X)  '$(DESTDIR)$(bindir)'/reindexdb$(X)

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)'

uninstall:
	rm -f $(addprefix '$(DESTDIR)$(bindir)'/, $(addsuffix $(X), $(PROGRAMS)))


clean distclean maintainer-clean:
	rm -f $(addsuffix $(X), $(PROGRAMS)) $(addsuffix .o, $(PROGRAMS))
	rm -f common.o dumputils.o kwlookup.o keywords.o print.o mbprint.o $(WIN32RES)
	rm -f dumputils.c print.c mbprint.c kwlookup.c keywords.c
