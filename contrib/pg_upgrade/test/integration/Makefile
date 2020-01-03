#
# Makefile for upgrade regress tests
#


subdir = contrib/pg_upgrade/test/integration
top_builddir = ../../../..
pg_upgrade_directory=../..
pg_upgrade_greenplum_directory=../../greenplum
TARGETS = tablespace_gp_test \
	greenplum_five_to_greenplum_six_upgrade_test_suite


include $(top_builddir)/src/Makefile.global
include $(top_builddir)/src/Makefile.mock

NAME = upgrade_regress

EXS = scripts/gpdb6-cluster scripts/gpdb5-cluster scripts/upgrade-cluster
include $(top_builddir)/src/Makefile.shlib

 # where to find psql for testing an existing installation
PSQLDIR = $(bindir)

ifeq ($(PORTNAME), win32)
LDLIBS += -lws2_32
endif

override CPPFLAGS := -I$(srcdir) -I$(libpq_srcdir) -I$(top_builddir)/src/test/regress $(CPPFLAGS)
override LDLIBS := $(libpq_pgport) $(LDLIBS)

all: pg_upgrade_regress$(X) all-lib data scripts/gpdb5-cluster scripts/gpdb6-cluster scripts/upgrade-cluster scripts/reset-cluster scripts/pg-upgrade-copy-from-master

utilities_CPPFLAGS = -I$(pg_upgrade_directory) \
	-I .

pg_regress.o:
	$(MAKE) -C $(top_builddir)/src/test/regress pg_regress.o
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/regress/pg_regress.o .

gpstringsubs.pl:
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/regress/gpstringsubs.pl

gpdiff.pl: atmsort.pm explain.pm GPTest.pm
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/regress/gpdiff.pl

GPTest.pm:
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/regress/GPTest.pm

atmsort.pm:
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/regress/atmsort.pm

explain.pm:
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/regress/explain.pm

data:
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/regress/data

pg_upgrade_regress$(X): upgrade_main.o pg_regress.o submake-libpq submake-libpgport
	$(CC) $(CFLAGS) $(filter %.o,$^) $(libpq_pgport) $(LDFLAGS) $(LDFLAGS_EX) $(LIBS) -o $@

sql_isolation_testcase.py:
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/isolation2/sql_isolation_testcase.py

install: all gpdiff.pl gpstringsubs.pl atmsort.pm sql_isolation_testcase.py greenplum_five_to_greenplum_six_upgrade_test_suite.t

installcheck: install
	./test.sh

installcheck-all: install
	./test.sh
	./greenplum_five_to_greenplum_six_upgrade_test_suite.t
	./tablespace_gp_test.t

scenario_objs := $(patsubst %.c,%.o,$(wildcard scenarios/*.c))
utilities_objs := $(patsubst %.c,%.o,$(wildcard utilities/*.c))
scripts_objs := $(patsubst %.c,%.o,$(wildcard scripts/*.c))

#
## Scripts
#
$(scripts_objs): override CPPFLAGS += -I .
scripts/gpdb5-cluster: scripts/gpdb5-cluster.o utilities/gpdb5-cluster.o
scripts/gpdb6-cluster: scripts/gpdb6-cluster.o utilities/gpdb6-cluster.o

scripts/upgrade-cluster: utilities/cluster-upgrade.o \
	scripts/upgrade-cluster.o \
	$(pg_upgrade_greenplum_directory)/old_tablespace_file_parser.o \
	$(pg_upgrade_greenplum_directory)/old_tablespace_file_contents.o \
	utilities/pg-upgrade-copy.o \
	utilities/test-upgrade-helpers.o

scripts/reset-cluster: scripts/reset-cluster.o utilities/test-helpers.o utilities/query-helpers.o

override CPPFLAGS += -I $(pg_upgrade_directory) -I$(libpq_srcdir)

scripts/pg-upgrade-copy-from-master.o: override CPPFLAGS += -I $(pg_upgrade_directory)
scripts/pg-upgrade-copy-from-master: scripts/pg-upgrade-copy-from-master.o \
	utilities/pg-upgrade-copy.o \
	$(pg_upgrade_greenplum_directory)/old_tablespace_file_contents.o \
	$(pg_upgrade_greenplum_directory)/old_tablespace_file_parser.o

#
## Libraries
#
override CPPFLAGS += -I . -I $(pg_upgrade_directory) -I$(libpq_srcdir)

#
## Tests
#
test_dependencies = bdd-library/bdd.o \
	$(utilities_objs) \
	$(scenario_objs) \
	$(CMOCKERY_OBJS) \
	$(pg_upgrade_greenplum_directory)/old_tablespace_file_contents.o \
	$(pg_upgrade_greenplum_directory)/old_tablespace_file_parser.o

debugging_flags = -Og -g

compile_test = $(CC) $(CFLAGS) $(debugging_flags) $^ $(libpq_pgport) $(LDFLAGS) -o $@

tablespace_gp_test.t: tablespace_gp_test.o $(test_dependencies) \
	$(pg_upgrade_greenplum_directory)/tablespace_gp.o \
	$(pg_upgrade_greenplum_directory)/option_gp.o \
	$(pg_upgrade_greenplum_directory)/old_tablespace_file_gp.o \
	$(pg_upgrade_greenplum_directory)/server_gp.o \
	$(pg_upgrade_greenplum_directory)/greenplum_cluster_info.o \
	$(pg_upgrade_greenplum_directory)/reporting.o \
	$(pg_upgrade_directory)/util.o \
	$(pg_upgrade_directory)/server.o \
	$(pg_upgrade_directory)/exec.o \
	$(CMOCKERY_OBJS)
	$(compile_test)


greenplum_five_to_greenplum_six_upgrade_test_suite.t: greenplum_five_to_greenplum_six_upgrade_test_suite.o $(test_dependencies) utilities/gpdb6-cluster.o 
	$(compile_test)

clean distclean:
	rm -f upgrade_regress.so pg_upgrade_regress$(X) $(OBJS) isolation2_main.o *.o *.t
	rm -f pg_regress.o
	rm -f gpstringsubs.pl gpdiff.pl atmsort.pm explain.pm
	rm -f data
	rm -rf $(pg_regress_clean_files)
	rm -rf $(EXS)
