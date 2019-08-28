#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"
#include "stdlib.h"


#include "../../checks.h"
#include "libpq-int.h"
#include "../../../pg_upgrade.h"

static int stubbed_number_of_rows = 0;


static struct UserDefinedIndexes query_for_indexes()
{
	struct UserDefinedIndexes indexes;
	indexes.number_of_user_defined_indexes = stubbed_number_of_rows;
	return indexes;
}


static void 
setup_rows_to_return(void)
{
	stubbed_number_of_rows = 1;
}


static void 
setup_no_rows_to_return(void)
{
	stubbed_number_of_rows = 0;
};


static ClusterInfo *make_cluster()
{
	ClusterInfo *cluster;

	cluster = malloc(sizeof(ClusterInfo));
	cluster->query_for_indexes = query_for_indexes;
	return cluster;
}


/*
 * Tests
 */
static void
test_it_returns_false_when_there_are_no_indexes_to_be_found(void **state)
{
	ClusterInfo *cluster = make_cluster();

	setup_rows_to_return();

	bool result = user_defined_indexes_check(cluster);

	assert_false(result);
}


static void
test_it_returns_true(void **state)
{
	ClusterInfo *cluster = make_cluster();

	setup_no_rows_to_return();

	bool result = user_defined_indexes_check(cluster);

	assert_true(result);
}


/*
 * Runner
 */
int main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test_it_returns_true),
		unit_test(test_it_returns_false_when_there_are_no_indexes_to_be_found)
	};

	run_tests(tests);
}