#-------------------------------------------------------------------------
#
# Makefile for src/bin/initdb
#
# Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
# Portions Copyright (c) 1994, Regents of the University of California
#
# $PostgreSQL: pgsql/src/bin/initdb/Makefile,v 1.57 2009/01/01 17:23:53 momjian Exp $
#
#-------------------------------------------------------------------------

PGFILEDESC = "initdb - initialize a new database cluster"
subdir = src/bin/initdb
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

override CPPFLAGS := -DFRONTEND -I$(libpq_srcdir) $(CPPFLAGS)

OBJS=	initdb.o encnames.o pqsignal.o $(WIN32RES)

all: submake-libpgport initdb

# The frontend doesn't need everything that's in LIBS, some are backend only
LIBS := $(filter-out -lresolv -lbz2, $(LIBS))
# This program isn't interactive, so doesn't need these
LIBS := $(filter-out -lreadline -ledit -ltermcap -lncurses -lcurses -lcurl -lssl -lcrypto -lz, $(LIBS))

initdb: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) $(LIBS) -o $@$(X)

# We used to pull in all of libpq to get encnames and pqsignal, but that
# exposes us to risks of version skew if we link to a shared library.
# Do it the hard way, instead, so that we're statically linked.

encnames.c: % : $(top_srcdir)/src/backend/utils/mb/%
	rm -f $@ && $(LN_S) $< .

pqsignal.c: % : $(top_srcdir)/src/interfaces/libpq/%
	rm -f $@ && $(LN_S) $< .

install: all installdirs
	$(INSTALL_PROGRAM) initdb$(X) '$(DESTDIR)$(bindir)/initdb$(X)'

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)'

uninstall:
	rm -f '$(DESTDIR)$(bindir)/initdb$(X)'

clean distclean maintainer-clean:
	rm -f initdb$(X) $(OBJS) encnames.c pqsignal.c


# ensure that changes in datadir propagate into object file
initdb.o: initdb.c $(top_builddir)/src/Makefile.global

check:
	$(prove_check)

installcheck:
	$(prove_installcheck)
