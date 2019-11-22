#include <stdbool.h>

#include "cmockery_gp.h"

#include "pg_upgrade.h"
#include "pg_upgrade_dummies.c"

/*
 * Test dummies
 */
void clear_old_tablespace_file_contents(OldTablespaceFileContents *contents)
{
	
}

OldTablespaceFileContents *
filter_old_tablespace_file_for_dbid(OldTablespaceFileContents *contents, int gp_dbid)
{
	return NULL;
}

OldTablespaceFileContents * 
parse_old_tablespace_file_contents(const char *path)
{
	return NULL;
}

/*
 * Tests
 */

static void
test_it_populates_using_gpdb6_tablespace_layout(
	void **state)
{
	ClusterInfo clusterInfo;
	strcpy(clusterInfo.major_version_str, "-SOME_MAJOR_VERSION_STRING-");
	clusterInfo.controldata.cat_ver = 12345;
	clusterInfo.gp_dbid             = 999;

	populate_gpdb6_cluster_tablespace_suffix(&clusterInfo);

	assert_string_equal(clusterInfo.tablespace_suffix, "/999/GPDB_6_12345");
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_it_populates_using_gpdb6_tablespace_layout),
	};

	return run_tests(tests);
}