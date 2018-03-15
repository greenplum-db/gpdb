#-------------------------------------------------------------------------
#
# Makefile for src/bin/pg_resetxlog
#
# Copyright (c) 1998-2010, PostgreSQL Global Development Group
#
# $PostgreSQL: pgsql/src/bin/pg_resetxlog/Makefile,v 1.24 2010/07/05 18:54:38 tgl Exp $
#
#-------------------------------------------------------------------------

PGFILEDESC = "pg_resetxlog - reset PostgreSQL WAL log"
PGAPPICON=win32

subdir = src/bin/pg_resetxlog
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

# The frontend doesn't need everything that's in LIBS, some are backend only
LIBS := $(filter-out -lresolv -lbz2, $(LIBS))
# This program isn't interactive, so doesn't need these
LIBS := $(filter-out -lreadline -ledit -ltermcap -lncurses -lcurses -lcurl -lssl -lcrypto -lz, $(LIBS))

OBJS= pg_resetxlog.o $(WIN32RES)

all: submake-libpgport pg_resetxlog

pg_resetxlog: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDFLAGS_EX) $(LIBS) -o $@$(X)

install: all installdirs
	$(INSTALL_PROGRAM) pg_resetxlog$(X) '$(DESTDIR)$(bindir)/pg_resetxlog$(X)'

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)'

uninstall:
	rm -f '$(DESTDIR)$(bindir)/pg_resetxlog$(X)'

clean distclean maintainer-clean:
	rm -f pg_resetxlog$(X) $(OBJS)
