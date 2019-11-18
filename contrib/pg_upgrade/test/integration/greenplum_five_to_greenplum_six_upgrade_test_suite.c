#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"
#include "old_tablespace_file_parser.h"

#include "scenarios/data_checksum_mismatch.h"
#include "scenarios/filespaces_to_tablespaces.h"

#include "scenarios/data_checksum_mismatch.h"

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
		unit_test_setup_teardown(test_clusters_with_different_checksum_version_cannot_be_upgraded, setup, teardown),
	};

	return run_tests(tests);
}
