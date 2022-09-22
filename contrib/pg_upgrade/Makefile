# contrib/pg_upgrade/Makefile

top_builddir = ../..

PGFILEDESC = "pg_upgrade - an in-place binary upgrade utility"
PGAPPICON = win32

PROGRAM  = pg_upgrade
OBJS = check.o controldata.o dump.o exec.o file.o function.o info.o \
       option.o page.o parallel.o pg_upgrade.o relfilenode.o server.o \
       tablespace.o util.o version.o version_old_8_3.o $(WIN32RES)

# Source files specific to Greenplum
OBJS += greenplum/aotable.o greenplum/version_gp.o \
		greenplum/check_gp.o greenplum/reporting.o \
		greenplum/aomd_filehandler.o greenplum/option_gp.o \
		greenplum/controldata_gp.o greenplum/frozenxids_gp.o \
		greenplum/old_tablespace_file_contents.o greenplum/old_tablespace_file_parser.o \
		greenplum/tablespace_gp.o greenplum/info_gp.o greenplum/old_tablespace_file_gp.o \
		greenplum/server_gp.o greenplum/greenplum_cluster_info.o

PG_CPPFLAGS  = -DFRONTEND -DDLSUFFIX=\"$(DLSUFFIX)\" -I$(srcdir) -I$(libpq_srcdir)
PG_LIBS = $(libpq_pgport)

EXTRA_CLEAN = analyze_new_cluster.sh delete_old_cluster.sh log/ tmp_check/ \
              pg_upgrade_dump_globals.sql \
              pg_upgrade_dump_*.custom pg_upgrade_*.log

EXTRA_CLEAN += clusterConfigPostgresAddonsFile clusterConfigFile gpdemo-env.sh \
              hostfile regression.diffs aomd_filehandler.c old_tablespaces.txt

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pg_upgrade
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

greenplum/aomd_filehandler.c: $(top_srcdir)/src/backend/access/appendonly/aomd_filehandler.c
	rm -f $@ && $(LN_S) ../$< $@

check: test_gpdb.sh all
	bash $< -C -r -s -o $(top_builddir)/gpAux/gpdemo/datadirs/ -b $(DESTDIR)$(bindir)

installcheck: test_gpdb.sh all
	bash $< -o $(top_builddir)/gpAux/gpdemo/datadirs/ -b $(DESTDIR)$(bindir)

perfcheck: test_gpdb.sh all
	bash $< -p -r -o $(top_builddir)/gpAux/gpdemo/datadirs/ -b $(DESTDIR)$(bindir)
