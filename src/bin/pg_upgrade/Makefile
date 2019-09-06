# src/bin/pg_upgrade/Makefile

PGFILEDESC = "pg_upgrade - an in-place binary upgrade utility"
PGAPPICON = win32

subdir = src/bin/pg_upgrade
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

OBJS = check.o controldata.o dump.o exec.o file.o function.o info.o \
       option.o page.o parallel.o pg_upgrade.o relfilenode.o server.o \
       tablespace.o util.o version.o version_old_8_3.o $(WIN32RES)

# Source files specific to Greenplum
OBJS += aotable.o gpdb4_heap_convert.o version_gp.o \
		check_gp.o file_gp.o reporting.o aomd_filehandler.o

override CPPFLAGS := -DDLSUFFIX=\"$(DLSUFFIX)\" -I$(srcdir) -I$(libpq_srcdir) $(CPPFLAGS)


all: pg_upgrade

pg_upgrade: $(OBJS) | submake-libpq submake-libpgport
	$(CC) $(CFLAGS) $^ $(libpq_pgport) $(LDFLAGS) $(LDFLAGS_EX) $(LIBS) -o $@$(X)

install: all installdirs
	$(INSTALL_PROGRAM) pg_upgrade$(X) '$(DESTDIR)$(bindir)/pg_upgrade$(X)'

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)'

uninstall:
	rm -f '$(DESTDIR)$(bindir)/pg_upgrade$(X)'

clean distclean maintainer-clean:
	rm -f pg_upgrade$(X) $(OBJS)
	rm -rf analyze_new_cluster.sh delete_old_cluster.sh log/ tmp_check/ \
	       pg_upgrade_dump_globals.sql \
	       pg_upgrade_dump_*.custom pg_upgrade_*.log \
		   clusterConfigPostgresAddonsFile clusterConfigFile gpdemo-env.sh \
		   hostfile regression.diffs aomd_filehandler.c

aomd_filehandler.c: % : $(top_srcdir)/src/backend/access/appendonly/%
	rm -f $@ && $(LN_S) $< .

check: test_gpdb.sh all
	bash $< -C -r -s -o $(top_builddir)/gpAux/gpdemo/datadirs/ -b $(DESTDIR)$(bindir)

# GPDB_95_MERGE_FIXME:
# GPDB want to run installcheck of pg_upgrade in the end of installcheck-world
# let installcheck be a no-op, so "make -C src/bin/ installcheck" does nothing on
# src/bin/pg_upgrade, the real test of pg_upgrade will be triggerred by
# "make check" in the end of installcheck-world, see GNUmakefile.in
#
#installcheck: test_gpdb.sh all
#	bash $< -o $(top_builddir)/gpAux/gpdemo/datadirs/ -b $(DESTDIR)$(bindir)

perfcheck: test_gpdb.sh all
	bash $< -p -r -o $(top_builddir)/gpAux/gpdemo/datadirs/ -b $(DESTDIR)$(bindir)
