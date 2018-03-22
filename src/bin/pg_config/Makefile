#-------------------------------------------------------------------------
#
# Makefile for src/bin/pg_config
#
# Copyright (c) 1998-2009, PostgreSQL Global Development Group
#
# $PostgreSQL: pgsql/src/bin/pg_config/Makefile,v 1.21 2009/01/01 17:23:54 momjian Exp $
#
#-------------------------------------------------------------------------

PGFILEDESC = "pg_config - report configuration information"
subdir = src/bin/pg_config
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

OBJS=   pg_config.o $(WIN32RES)

# The frontend doesn't need everything that's in LIBS, some are backend only
LIBS := $(filter-out -lresolv -lbz2 -lnetsnmp, $(LIBS))
# This program isn't interactive, so doesn't need these
LIBS := $(filter-out -lreadline -ledit -ltermcap -lncurses -lcurses -lcurl -lssl -lcrypto -lz, $(LIBS))



# don't include subdirectory-path-dependent -I and -L switches
STD_CPPFLAGS := $(filter-out -I%,$(CPPFLAGS))
STD_CFLAGS := $(filter-out -I%,$(CFLAGS))
STD_LDFLAGS := $(filter-out -L%,$(LDFLAGS))
STD_LIBS := $(filter-out -L%,$(LIBS))

override CPPFLAGS += -DVAL_CONFIGURE="\"$(configure_args)\""
override CPPFLAGS += -DVAL_CC="\"$(CC)\""
override CPPFLAGS += -DVAL_CPPFLAGS="\"$(STD_CPPFLAGS)\""
override CPPFLAGS += -DVAL_CFLAGS="\"$(STD_CFLAGS)\""
override CPPFLAGS += -DVAL_CFLAGS_SL="\"$(CFLAGS_SL)\""
override CPPFLAGS += -DVAL_LDFLAGS="\"$(STD_LDFLAGS)\""
override CPPFLAGS += -DVAL_LDFLAGS_SL="\"$(LDFLAGS_SL)\""
override CPPFLAGS += -DVAL_LIBS="\"$(STD_LIBS)\""

all: submake-libpgport pg_config

pg_config: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) $(LIBS) -o $@$(X)

install: all installdirs
	$(INSTALL_SCRIPT) pg_config$(X) '$(DESTDIR)$(bindir)/pg_config$(X)'

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)'

uninstall:
	rm -f '$(DESTDIR)$(bindir)/pg_config$(X)'

clean distclean maintainer-clean:
	rm -f pg_config$(X) $(OBJS)

check:
	$(prove_check)

installcheck:
	$(prove_installcheck)
