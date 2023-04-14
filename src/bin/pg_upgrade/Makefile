# src/bin/pg_upgrade/Makefile

PGFILEDESC = "pg_upgrade - an in-place binary upgrade utility"
PGAPPICON = win32

subdir = src/bin/pg_upgrade
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

OBJS = check.o controldata.o dump.o exec.o file.o function.o info.o \
       option.o parallel.o pg_upgrade.o relfilenode.o server.o \
       tablespace.o util.o version.o $(WIN32RES)

# Source files specific to Greenplum
OBJS += greenplum/aotable.o greenplum/version_gp.o \
		greenplum/check_gp.o greenplum/reporting.o \
		greenplum/aomd_filehandler.o greenplum/option_gp.o \
		greenplum/controldata_gp.o

override CPPFLAGS := -DDLSUFFIX=\"$(DLSUFFIX)\" -I$(srcdir) -I$(libpq_srcdir) $(CPPFLAGS)
LDFLAGS_INTERNAL += -L$(top_builddir)/src/fe_utils -lpgfeutils $(libpq_pgport)

all: pg_upgrade

pg_upgrade: $(OBJS) | submake-libpq submake-libpgport submake-libpgfeutils
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDFLAGS_EX) $(LIBS) -o $@$(X)

install: all installdirs
	$(INSTALL_PROGRAM) pg_upgrade$(X) '$(DESTDIR)$(bindir)/pg_upgrade$(X)'

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)'

uninstall:
	rm -f '$(DESTDIR)$(bindir)/pg_upgrade$(X)'

clean distclean maintainer-clean:
	rm -f pg_upgrade$(X) $(OBJS)
	rm -rf analyze_new_cluster.sh delete_old_cluster.sh log/ tmp_check/ \
	       loadable_libraries.txt reindex_hash.sql \
	       pg_upgrade_dump_globals.sql \
	       pg_upgrade_dump_*.custom pg_upgrade_*.log \
		   clusterConfigPostgresAddonsFile clusterConfigFile gpdemo-env.sh \
		   hostfile regression.diffs aomd_filehandler.c

greenplum/aomd_filehandler.c: $(top_srcdir)/src/backend/access/appendonly/aomd_filehandler.c
	rm -f $@ && cd greenplum && $(LN_S) ../$< aomd_filehandler.c

# GPDB_95_MERGE_FIXME:
# GPDB want to run installcheck of pg_upgrade in the end of installcheck-world
# let installcheck be a no-op, so "make -C src/bin/ installcheck" does nothing on
# src/bin/pg_upgrade, the real test of pg_upgrade will be triggered by
# "make check" in the end of installcheck-world, see GNUmakefile.in

# When $(MAKE) is present, make automatically infers that this is a
# recursive make. which is not actually what we want here, as that
# e.g. prevents output synchronization from working (as make thinks
# that the subsidiary make knows how to deal with that itself, but
# we're invoking a shell script that doesn't know). Referencing
# $(MAKE) indirectly avoids that behaviour.
# See https://www.gnu.org/software/make/manual/html_node/MAKE-Variable.html#MAKE-Variable
NOTSUBMAKEMAKE=$(MAKE)

check: test_gpdb.sh all
	bash $< -C -r -s -o $(top_builddir)/gpAux/gpdemo/datadirs/ -b $(DESTDIR)$(bindir)

perfcheck: test_gpdb.sh all
	bash $< -p -r -o $(top_builddir)/gpAux/gpdemo/datadirs/ -b $(DESTDIR)$(bindir)

# installcheck is not supported because there's no meaningful way to test
# pg_upgrade against a single already-running server
