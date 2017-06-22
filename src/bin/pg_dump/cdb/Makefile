#-------------------------------------------------------------------------
#
# Makefile for src/bin/pg_dump/cdb
#
# Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
# Portions Copyright (c) 1994, Regents of the University of California
#
# src/bin/pg_dump/cdb/Makefile
#
#-------------------------------------------------------------------------

subdir = src/bin/pg_dump/cdb
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

ifneq ($(PORTNAME), win32)
override CFLAGS += $(PTHREAD_CFLAGS) -Wno-error
endif

# The frontend doesn't need everything that's in LIBS, some are backend only
LIBS := $(filter-out -lresolv, $(LIBS))
# This program isn't interactive, so doesn't need these
LIBS := $(filter-out -lreadline -ledit -ltermcap -lncurses -lcurses -lcurl -lssl -lcrypto, $(LIBS))

ifeq ($(enable_ddboost), yes)
DDBOOSTLIB += -lDDBoost
endif

ifeq ($(enable_netbackup), yes)
NETBACKUPLIB77 += -L$(top_srcdir)/gpAux/ext/$(BLD_ARCH)/Netbackup/nbu77/lib -lxbsa64
GPBSALIB += -L$(top_builddir)/src/bin/pg_dump/cdb -lgpbsa -Wl,-rpath-link,'$(top_builddir)/gpAux/ext/$(BLD_ARCH)/Netbackup/nbu77/lib'
override CFLAGS += -I$(top_builddir)/gpAux/ext/$(BLD_ARCH)/Netbackup/nbu77/include
override CPPFLAGS += -I$(top_builddir)/gpAux/ext/$(BLD_ARCH)/Netbackup/nbu77/include
endif

PGDUMP_SRCDIR= $(top_srcdir)/src/bin/pg_dump
PGDUMP_DIR= $(top_builddir)/src/bin/pg_dump

override CPPFLAGS := -I$(PGDUMP_SRCDIR) -I$(libpq_srcdir) $(CPPFLAGS) -DBINDIR=\"$(bindir)\"
override CFLAGS += -I$(top_builddir)/src/include -I$(top_builddir)/src/interfaces/libpq -I$(top_builddir)/gpAux/ext/$(BLD_ARCH)/include

OBJS= pg_backup_db.o pg_backup_custom.o pg_backup_null.o \
		pg_backup_tar.o dumputils.o 

KEYWRDOBJS= keywords.o kwlookup.o

kwlookup.c: % : $(top_srcdir)/src/backend/parser/%
	rm -f $@ && $(LN_S) $< .

PGDUMP_SRC= pg_backup_db.c pg_backup_custom.c \
			pg_backup_null.c pg_backup_tar.c dumputils.c common.c \
			pg_dump_sort.c keywords.c

$(PGDUMP_SRC): % : $(PGDUMP_DIR)/%
	rm -f $@ && $(LN_S) $< .

all: submake-libpq submake-libpgport cdb_dump cdb_dump_agent cdb_restore \
	cdb_restore_agent gpddboost cdb_bsa_dump_agent cdb_bsa_restore_agent \
	cdb_bsa_query_agent cdb_bsa_delete_agent \
	libgpbsa.so libgpbsa77.so

ifeq ($(enable_netbackup), yes)
cdb_bsa_dump_agent cdb_bsa_restore_agent cdb_bsa_query_agent cdb_bsa_delete_agent: libgpbsa.so
endif

cdb_dump: cdb_dump.o cdb_backup_status.o cdb_seginst.o cdb_backup_state.o cdb_table.o cdb_backup_archiver.o cdb_dump_util.o cdb_dump_include.o cdb_lockbox.o common.o | $(OBJS) $(KEYWRDOBJS) $(libpq_builddir)/libpq.a
	$(CC) $(CFLAGS) $^ $(OBJS) $(KEYWRDOBJS) $(libpq) $(LDFLAGS) $(DDBOOSTLIB) $(LIBS) -o $@$(X)

cdb_dump_agent: cdb_dump_agent.o common.o pg_dump_sort.o cdb_backup_archiver.o cdb_dump_util.o cdb_seginst.o cdb_table.o cdb_backup_status.o cdb_backup_state.o cdb_dump_include.o cdb_lockbox.o | $(OBJS) $(KEYWRDOBJS) $(libpq_builddir)/libpq.a
	$(CC) $(CFLAGS) $^ $(OBJS) $(KEYWRDOBJS) $(libpq) $(LDFLAGS) $(DDBOOSTLIB) $(LIBS) -o $@$(X)

cdb_restore: cdb_restore.o cdb_backup_status.o cdb_seginst.o cdb_backup_state.o cdb_table.o cdb_dump_util.o cdb_lockbox.o dumputils.o | $(libpq_builddir)/libpq.a $(KEYWRDOBJS)
	$(CC) $(CFLAGS) $^ $(KEYWRDOBJS) $(libpq) $(LDFLAGS) $(DDBOOSTLIB) $(LIBS) -o $@$(X)

cdb_restore_agent: cdb_restore_agent.o cdb_backup_archiver.o cdb_backup_status.o cdb_dump_util.o cdb_seginst.o cdb_table.o cdb_lockbox.o | $(OBJS) $(libpq_builddir)/libpq.a $(KEYWRDOBJS)
	$(CC) $(CFLAGS) $^ $(OBJS) $(KEYWRDOBJS) $(libpq) $(LDFLAGS) $(DDBOOSTLIB) $(LIBS) -o $@$(X)

gpddboost: cdb_ddboost_util.o cdb_backup_status.o cdb_seginst.o cdb_backup_state.o cdb_table.o cdb_dump_util.o cdb_lockbox.o dumputils.o | $(libpq_builddir)/libpq.a $(KEYWRDOBJS)
	$(CC) $(CFLAGS) $^ $(KEYWRDOBJS) $(libpq) $(LDFLAGS) $(DDBOOSTLIB) $(LIBS) -o $@$(X)

libgpbsa77.so: cdb_bsa_util.c $(libpq_builddir)/libpq.a
	$(CC) $(CFLAGS_SL) $(CPPFLAGS) cdb_bsa_util.c $(libpq) $(LDFLAGS) -shared $(DDBOOSTLIB) $(NETBACKUPLIB77) $(LIBS) -o $@

libgpbsa.so: libgpbsa77.so
	cp libgpbsa77.so libgpbsa.so

cdb_bsa_dump_agent: cdb_bsa_dump_agent.o cdb_dump_util.o cdb_lockbox.o dumputils.o | $(libpq_builddir)/libpq.a $(KEYWRDOBJS)
	$(CC) $(CFLAGS) $^ $(KEYWRDOBJS) $(libpq) $(LDFLAGS) $(DDBOOSTLIB) $(GPBSALIB) $(LIBS) -o $@$(X)

cdb_bsa_restore_agent: cdb_bsa_restore_agent.o cdb_dump_util.o cdb_lockbox.o dumputils.o | $(libpq_builddir)/libpq.a $(KEYWRDOBJS)
	$(CC) $(CFLAGS) $^ $(KEYWRDOBJS) $(libpq) $(LDFLAGS) $(DDBOOSTLIB) $(GPBSALIB) $(LIBS) -o $@$(X)

cdb_bsa_query_agent: cdb_bsa_query_agent.o cdb_dump_util.o cdb_lockbox.o dumputils.o | $(libpq_builddir)/libpq.a $(KEYWRDOBJS)
	$(CC) $(CFLAGS) $^ $(KEYWRDOBJS) $(libpq) $(LDFLAGS) $(DDBOOSTLIB) $(GPBSALIB) $(LIBS) -o $@$(X)

cdb_bsa_delete_agent: cdb_bsa_delete_agent.o cdb_dump_util.o cdb_lockbox.o dumputils.o | $(libpq_builddir)/libpq.a $(KEYWRDOBJS)
	$(CC) $(CFLAGS) $^ $(KEYWRDOBJS) $(libpq) $(LDFLAGS) $(DDBOOSTLIB) $(GPBSALIB) $(LIBS) -o $@$(X)

install: all installdirs
	$(INSTALL_PROGRAM) cdb_dump$(X) $(DESTDIR)$(bindir)/gp_dump$(X)
	$(INSTALL_PROGRAM) cdb_restore$(X) $(DESTDIR)$(bindir)/gp_restore$(X)
	$(INSTALL_PROGRAM) cdb_dump_agent$(X) $(DESTDIR)$(bindir)/gp_dump_agent$(X)
	$(INSTALL_PROGRAM) cdb_restore_agent$(X) $(DESTDIR)$(bindir)/gp_restore_agent$(X)
	$(INSTALL_PROGRAM) gpddboost$(X) $(DESTDIR)$(bindir)/gpddboost$(X)
	$(INSTALL_PROGRAM) libgpbsa77.so $(DESTDIR)$(libdir)/nbu77/lib/libgpbsa.so
	$(INSTALL_PROGRAM) cdb_bsa_dump_agent$(X) $(DESTDIR)$(bindir)/gp_bsa_dump_agent$(X)
	$(INSTALL_PROGRAM) cdb_bsa_restore_agent$(X) $(DESTDIR)$(bindir)/gp_bsa_restore_agent$(X)
	$(INSTALL_PROGRAM) cdb_bsa_query_agent$(X) $(DESTDIR)$(bindir)/gp_bsa_query_agent$(X)
	$(INSTALL_PROGRAM) cdb_bsa_delete_agent$(X) $(DESTDIR)$(bindir)/gp_bsa_delete_agent$(X)

installdirs:
	$(MKDIR_P) $(DESTDIR)$(bindir)

uninstall:
	rm -f $(addprefix '$(DESTDIR)$(bindir)'/, cdb_dump$(X) cdb_dump_agent$(X) cdb_restore$(X) cdb_restore_agent$(X) cdb_bsa_dump_agent$(X) cdb_bsa_restore_agent$(X) cdb_bsa_query_agent$(X) cdb_bsa_delete_agent$(X) gpddboost$(X))

clean distclean maintainer-clean:
	rm -f cdb_dump$(X) cdb_restore$(X) cdb_dump_agent$(X) cdb_restore_agent$(X)
	rm -f cdb_bsa_dump_agent$(X) cdb_bsa_restore_agent$(X) cdb_bsa_query_agent$(X) cdb_bsa_delete_agent$(X)
	rm -f $(PGDUMP_SRC) $(OBJS) common.o dumputil.o pg_dump_sort.o
	rm -f cdb_dump.o cdb_restore.o cdb_dump_agent.o cdb_restore_agent.o
	rm -f cdb_backup_status.o cdb_dump_include.o cdb_seginst.o cdb_lockbox.o
	rm -f cdb_backup_state.o cdb_table.o cdb_dump_util.o cdb_backup_archiver.o
	rm -f cdb_ddboost_util.o cdb_bsa_dump_agent.o cdb_bsa_query_agent.o
	rm -f cdb_bsa_restore_agent.o cdb_bsa_delete_agent.o cdb_bsa_util.o
	rm -f libgpbsa*.so
	rm -f gpddboost$(X)
	rm -f kwlookup.c $(KEYWRDOBJS)
	$(MAKE) -C test clean
