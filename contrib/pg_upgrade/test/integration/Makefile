subdir=contrib/pg_upgrade/test/integration
top_srcdir=../../../..
top_builddir=../../../..
pg_upgrade_directory=../..

OBJS = scripts/gpdb5-cluster.o \
	utilities/gpdb5-cluster.o \
	scripts/gpdb6-cluster.o \
	utilities/gpdb6-cluster.o \
	utilities/query-helpers.o \
	utilities/upgrade-helpers.o \
	bdd-library/upgrade-bdd.o \
	scenarios/heap_table.o \
	scenarios/ao_table.o \
	greenplum_five_to_greenplum_six_upgrade_test.o

EXS = scripts/gpdb6-cluster scripts/gpdb5-cluster
TARGETS = greenplum_five_to_greenplum_six_upgrade_test

include $(top_srcdir)/src/Makefile.global
include $(top_srcdir)/src/Makefile.mock

all: scripts/gpdb5-cluster scripts/gpdb6-cluster

utilities_CPPFLAGS = -I$(pg_upgrade_directory) \
	-I$(libpq_srcdir) \
	-I.\


#
# Scripts
#
scripts/gpdb5-cluster: scripts/gpdb5-cluster.o utilities/gpdb5-cluster.o

scripts/gpdb6-cluster: scripts/gpdb6-cluster.o utilities/gpdb6-cluster.o


#
# Libraries
#
utilities/gpdb5-cluster.o \
	utilities/gpdb6-cluster.o \
	utilities/query-helpers.o \
	bdd-library/upgrade-bdd.o \
	scenarios/heap_table.o \
	scenarios/ao_table.o \
	greenplum_five_to_greenplum_six_upgrade_test.o: override CPPFLAGS += $(utilities_CPPFLAGS)

#
# Tests
#
test_dependencies = utilities/gpdb5-cluster.o \
	utilities/gpdb6-cluster.o \
	utilities/query-helpers.o \
	utilities/upgrade-helpers.o \
	bdd-library/upgrade-bdd.o \
	scenarios/ao_table.o \
	scenarios/heap_table.o \
	$(CMOCKERY_OBJS)


greenplum_five_to_greenplum_six_upgrade_test.t: greenplum_five_to_greenplum_six_upgrade_test.o $(test_dependencies)
	$(CC) $(CFLAGS) $^ $(libpq_pgport) $(LDFLAGS) -o $@

clean:
	rm -f $(OBJS) $(EXS)
