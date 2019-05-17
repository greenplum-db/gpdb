#-------------------------------------------------------------------------
#
# Makefile for src/bin/pg_rewind
#
# Portions Copyright (c) 2013-2015, PostgreSQL Global Development Group
#
# src/bin/pg_rewind/Makefile
#
#-------------------------------------------------------------------------

PGFILEDESC = "pg_rewind - repurpose an old master server as standby"
PGAPPICON = win32

subdir = src/bin/pg_rewind
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

PG_CPPFLAGS = -I$(libpq_srcdir)
PG_LIBS = $(libpq_pgport)

override CPPFLAGS := -I$(libpq_srcdir) -DFRONTEND $(CPPFLAGS)

OBJS	= pg_rewind.o parsexlog.o xlogreader.o datapagemap.o timeline.o \
	fetch.o file_ops.o copy_fetch.o libpq_fetch.o filemap.o logging.o compat.o \
	$(WIN32RES)

EXTRA_CLEAN = xlogreader.c compat.c

REGRESS = basictest extrafiles databases pg_xlog_symlink unclean_shutdown \
	simple_no_rewind_required ao_rewind bitmaptest \
	tablespaces_objects_created_before_promotion \
	tablespaces_objects_created_after_promotion \
	tablespaces_objects_removed_after_promotion
REGRESS_OPTS=--init-file=./regress_init_file --use-existing --launcher=./launcher

all: pg_rewind

pg_rewind: $(OBJS) | submake-libpq submake-libpgport
	$(CC) $(CFLAGS) $^ $(libpq_pgport) $(LDFLAGS) $(LDFLAGS_EX) $(LIBS) -o $@$(X)

xlogreader.c: % : $(top_srcdir)/src/backend/access/transam/%
	rm -f $@ && $(LN_S) $< .

# compat.c is symlinked from the PostgreSQL sources.
compat.c: % : $(top_srcdir)/contrib/pg_xlogdump/%
	rm -f $@ && $(LN_S) $< .

install: all installdirs
	$(INSTALL_PROGRAM) pg_rewind$(X) '$(DESTDIR)$(bindir)/pg_rewind$(X)'

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)'

uninstall:
	rm -f '$(DESTDIR)$(bindir)/pg_rewind$(X)'

clean distclean maintainer-clean:
	rm -f pg_rewind$(X) $(OBJS) xlogreader.c
	rm -rf tmp_check_local tmp_check_remote

# GPDB_95_MERGE_FIXME: 9.4 version of tests were used instead of 9.5 since 9.5
# once have dependency on common TAP framework functions which are not yet
# present in GPDB. When 9.5 is fully merged, retire these 9.4 related tests and
# use the TAP tests instead.

check-local:
	echo "Running tests against local data directory, in copy-mode"
	bindir=$(bindir) TEST_SUITE="local" $(MAKE) pg_regress_installcheck

check-remote:
	echo "Running tests against a running standby, via libpq"
	bindir=$(bindir) TEST_SUITE="remote" $(MAKE) pg_regress_installcheck

check-both: check-local check-remote

# against installed postmaster
pg_regress_installcheck:
	$(pg_regress_installcheck) $(REGRESS_OPTS) $(REGRESS)

installcheck: check-both
