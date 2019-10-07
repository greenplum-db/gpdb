subdir=contrib/pg_upgrade/test/integration
top_srcdir=../../../..
top_builddir=../../../..
pg_upgrade_directory=../..

scenario_objs := $(patsubst %.c,%.o,$(wildcard scenarios/*.c))
utilities_objs := $(patsubst %.c,%.o,$(wildcard utilities/*.c))
scripts_objs := $(patsubst %.c,%.o,$(wildcard scripts/*.c))

OBJS = bdd-library/bdd.o \
	$(scripts_objs) \
	$(utilities_objs) \
	$(scenario_objs) \
	greenplum_five_to_greenplum_six_upgrade_test_suite.o \
	tablespace_gp_test.o

EXS = scripts/gpdb6-cluster \
	scripts/gpdb5-cluster

TARGETS = greenplum_five_to_greenplum_six_upgrade_test_suite \
	tablespace_gp_test

include $(top_srcdir)/src/Makefile.global
include $(top_srcdir)/src/Makefile.mock

all: scripts/gpdb5-cluster scripts/gpdb6-cluster scripts/pg-upgrade-copy-from-master

#
# Scripts
#
$(scripts_objs): override CPPFLAGS += -I .

scripts/gpdb5-cluster: scripts/gpdb5-cluster.o utilities/gpdb5-cluster.o

scripts/gpdb6-cluster: scripts/gpdb6-cluster.o utilities/gpdb6-cluster.o

override LDFLAGS += -lasan $(libpq_pgport)
override CPPFLAGS += -I $(pg_upgrade_directory) -I$(libpq_srcdir)

scripts/pg-upgrade-copy-from-master.o: override CPPFLAGS += -I $(pg_upgrade_directory)

scripts/pg-upgrade-copy-from-master: scripts/pg-upgrade-copy-from-master.o \
	utilities/pg-upgrade-copy.o \
	$(pg_upgrade_directory)/old_tablespace_file_contents.o \
	$(pg_upgrade_directory)/old_tablespace_file_parser.o

#
# Libraries
#
bdd-library/bdd.o: override CPPFLAGS += -I .

utilities_CPPFLAGS = -I$(pg_upgrade_directory) \
	-I$(libpq_srcdir) \
	-I .

$(utilities_objs) \
	$(scenario_objs) \
	greenplum_five_to_greenplum_six_upgrade_test_suite.o: override CPPFLAGS += $(utilities_CPPFLAGS)

#
# Tests
#
test_dependencies = bdd-library/bdd.o \
	$(utilities_objs) \
	$(scenario_objs) \
	$(CMOCKERY_OBJS) \
	$(pg_upgrade_directory)/old_tablespace_file_contents.o \
	$(pg_upgrade_directory)/old_tablespace_file_parser.o

debugging_flags = -Og -g -fsanitize=address -Wextra -Wpedantic -pedantic-errors -Werror

compile_test = $(CC) $(CFLAGS) $(debugging_flags) $^ $(libpq_pgport) $(LDFLAGS) -o $@

greenplum_five_to_greenplum_six_upgrade_test_suite.t: greenplum_five_to_greenplum_six_upgrade_test_suite.o $(test_dependencies)
	$(compile_test)

tablespace_gp_test.t: tablespace_gp_test.o $(test_dependencies) \
	$(pg_upgrade_directory)/tablespace_gp.o \
	$(pg_upgrade_directory)/util.o \
	$(pg_upgrade_directory)/server.o \
	$(pg_upgrade_directory)/exec.o \
	$(pg_upgrade_directory)/reporting.o
	$(compile_test)

clean:
	rm -f $(OBJS) $(EXS)
