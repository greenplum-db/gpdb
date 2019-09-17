subdir=contrib/pg_upgrade/test/integration
top_srcdir=../../../..
top_builddir=../../../..
pg_upgrade_directory=../..


TARGETS = greenplum_five_to_greenplum_six_upgrade_test


include $(top_srcdir)/src/Makefile.mock
include $(top_srcdir)/src/Makefile.global


test_flags = -I $(pg_upgrade_directory) \
	-I$(libpq_srcdir) \
	-g -O0


compile = $(CC) $(CFLAGS) $(CPPFLAGS) 
compile_test = $(compile) $(test_flags) 


integration_test_dependencies = $(libpq_pgport) 


#
# Scripts
#
scripts/gpdb5-cluster: scripts/gpdb5-cluster.c utilities/gpdb5-cluster.o
	$(compile) $^ -o scripts/gpdb5-cluster 


scripts/gpdb6-cluster: scripts/gpdb6-cluster.c utilities/gpdb6-cluster.o
	$(compile) $^ -o scripts/gpdb6-cluster 


#
# Libraries
#
utilities/gpdb5-cluster.o: ./utilities/gpdb5-cluster.c
	$(compile_test) -c $< -o $@


utilities/gpdb6-cluster.o: ./utilities/gpdb6-cluster.c
	$(compile_test) -c $< -o $@


utilities/query-helpers.o: ./utilities/query-helpers.c
	$(compile_test) -c $< -o $@

#
# Tests
#
test_dependencies = utilities/gpdb5-cluster.o \
	utilities/gpdb6-cluster.o \
	utilities/query-helpers.o \
	utilities/upgrade-helpers.o \
	$(CMOCKERY_OBJS)


greenplum_five_to_greenplum_six_upgrade_test.t: greenplum_five_to_greenplum_six_upgrade_test.c $(test_dependencies) 
	$(compile_test) \
		$^ \
		$(integration_test_dependencies) \
		-o $@


#
# Build all
#
all: scripts/gpdb5-cluster scripts/gpdb6-cluster greenplum_five_to_greenplum_six_upgrade_test.t


