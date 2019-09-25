subdir=contrib/pg_upgrade/test/integration
top_srcdir=../../../..
top_builddir=../../../..
pg_upgrade_directory=../..

scenario_objs := $(patsubst %.c,%.o,$(wildcard scenarios/*.c))
utilities_objs := $(patsubst %.c,%.o,$(wildcard utilities/*.c))
scripts_objs := $(patsubst %.c,%.o,$(wildcard scripts/*.c))

OBJS = bdd-library/upgrade-bdd.o \
	$(scripts_objs) \
	$(utilities_objs) \
	$(scenario_objs) \
	greenplum_five_to_greenplum_six_upgrade_test_suite.o

EXS = scripts/gpdb6-cluster scripts/gpdb5-cluster
TARGETS = greenplum_five_to_greenplum_six_upgrade_test_suite

include $(top_srcdir)/src/Makefile.global
include $(top_srcdir)/src/Makefile.mock

all: scripts/gpdb5-cluster scripts/gpdb6-cluster

#
# Scripts
#
scripts/gpdb5-cluster: scripts/gpdb5-cluster.o utilities/gpdb5-cluster.o

scripts/gpdb6-cluster: scripts/gpdb6-cluster.o utilities/gpdb6-cluster.o


#
# Libraries
#
bdd-library/upgrade-bdd.o: override CPPFLAGS += -I .

utilities_CPPFLAGS = -I$(pg_upgrade_directory) \
	-I$(libpq_srcdir) \
	-I. \

$(utilities_objs) \
	$(scenario_objs) \
	greenplum_five_to_greenplum_six_upgrade_test_suite.o: override CPPFLAGS += $(utilities_CPPFLAGS)

#
# Tests
#
test_dependencies = bdd-library/upgrade-bdd.o \
	$(utilities_objs) \
	$(scenario_objs) \
	$(CMOCKERY_OBJS)

greenplum_five_to_greenplum_six_upgrade_test_suite.t: greenplum_five_to_greenplum_six_upgrade_test_suite.o $(test_dependencies)
	$(CC) $(CFLAGS) $^ $(libpq_pgport) $(LDFLAGS) -o $@

clean:
	rm -f $(OBJS) $(EXS)
