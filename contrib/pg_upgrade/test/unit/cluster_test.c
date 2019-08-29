#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"


#include "pg_upgrade.h"



void test_init_cluster_sets_cluster_to_utility_mode(void **state)
{
	ClusterInfo cluster;

	cluster.use_utility_mode = false;

	init_cluster(&cluster);

	assert_true(cluster.use_utility_mode);
}


int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_init_cluster_sets_cluster_to_utility_mode)
	};

	return run_tests(tests);
}
