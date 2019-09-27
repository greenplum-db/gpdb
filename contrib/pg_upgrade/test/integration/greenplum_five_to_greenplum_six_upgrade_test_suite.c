#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"

#include "scenarios/heap_table.h"
#include "scenarios/subpartitioned_heap_table.h"
#include "scenarios/ao_table.h"
#include "scenarios/aocs_table.h"

#include "utilities/gpdb5-cluster.h"
#include "utilities/gpdb6-cluster.h"

#include "utilities/test-helpers.h"

static void
setup(void **state)
{
	resetGpdbFiveDataDirectories();
	resetGpdbSixDataDirectories();
}

static void
teardown(void **state)
{
	stopGpdbFiveCluster();
	stopGpdbSixCluster();
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test_setup_teardown(test_an_ao_table_with_data_can_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_an_aocs_table_with_data_can_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_a_heap_table_with_data_can_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_a_subpartitioned_heap_table_with_data_can_be_upgraded, setup, teardown),
	};

	return run_tests(tests);
}
