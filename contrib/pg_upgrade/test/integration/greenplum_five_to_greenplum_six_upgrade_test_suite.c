#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"
#include "old_tablespace_file_parser.h"

#include "scenarios/partitioned_ao_table.h"
#include "scenarios/partitioned_heap_table.h"
#include "scenarios/heterogeneous_partitioned_heap_table.h"
#include "scenarios/exchange_partitioned_heap_table.h"
#include "scenarios/partitioned_heap_table_with_a_dropped_column.h"
#include "scenarios/heap_table.h"
#include "scenarios/subpartitioned_heap_table.h"
#include "scenarios/ao_table.h"
#include "scenarios/aocs_table.h"
#include "scenarios/data_checksum_mismatch.h"
#include "scenarios/pl_function.h"
#include "scenarios/user_defined_types.h"
#include "scenarios/external_tables.h"
#include "scenarios/filespaces_to_tablespaces.h"

#include "utilities/gpdb5-cluster.h"
#include "utilities/gpdb6-cluster.h"

#include "utilities/test-upgrade-helpers.h"
#include "utilities/test-helpers.h"
#include "utilities/row-assertions.h"

static void
setup(void **state)
{
	initializePgUpgradeStatus();
	resetGpdbFiveDataDirectories();
	resetGpdbSixDataDirectories();

	matcher = NULL;
	match_failed = NULL;
}

static void
teardown(void **state)
{
	resetPgUpgradeStatus();
	stopGpdbFiveCluster();
	stopGpdbSixCluster();
}


void
OldTablespaceFileParser_invalid_access_error_for_field(int invalid_row_index, int invalid_field_index)
{
	printf("attempted to access invalid field: {row_index=%d, field_index=%d}", 
		invalid_row_index,
		invalid_field_index);

	exit(1);
}

void
OldTablespaceFileParser_invalid_access_error_for_row(int invalid_row_index)
{
	printf("attempted to access invalid row: {row_index=%d}",
	       invalid_row_index);

	exit(1);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test_setup_teardown(test_a_filespace_can_be_upgraded_into_new_tablespaces, setup, teardown),
		unit_test_setup_teardown(test_a_readable_external_table_can_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_a_partition_table_with_default_partition_after_split_can_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_a_partition_table_with_newly_added_range_partition_can_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_a_partition_table_with_newly_added_list_partition_can_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_clusters_with_different_checksum_version_cannot_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_an_ao_table_with_data_can_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_an_aocs_table_with_data_can_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_a_heap_table_with_data_can_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_a_subpartitioned_heap_table_with_data_can_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_a_partitioned_heap_table_with_data_can_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_a_partitioned_ao_table_with_data_can_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_a_partitioned_ao_table_with_data_on_multiple_segfiles_can_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_a_partitioned_aoco_table_with_data_can_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_a_partitioned_aoco_table_with_data_on_multiple_segfiles_can_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_an_exchange_partitioned_heap_table_cannot_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_a_partitioned_heap_table_with_a_dropped_column_can_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_a_partitioned_heap_table_with_differently_sized_dropped_columns_cannot_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_a_partitioned_heap_table_with_differently_aligned_fixed_dropped_columns_cannot_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_a_partitioned_heap_table_with_differently_aligned_varlen_dropped_columns_cannot_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_a_plpgsql_function_can_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_a_plpython_function_can_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_an_user_defined_type_extension_can_be_upgraded, setup, teardown),
	};

	return run_tests(tests);
}
