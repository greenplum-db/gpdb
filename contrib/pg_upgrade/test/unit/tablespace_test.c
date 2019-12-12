#include <stdbool.h>

#include "cmockery_gp.h"

#include "pg_upgrade.h"

#include "greenplum/old_tablespace_file_gp.h"
#include "greenplum/pg_upgrade_greenplum.h"

#include "pg_upgrade_dummies.c"

/* 
 * Test state
 */
ClusterInfo new_cluster;
ClusterInfo old_cluster;
ClusterInfo *test_cluster;

OSInfo      os_info;
OSInfo      *test_os_info;

OldTablespaceFileContents *old_tablespace_file_contents;

OldTablespaceFileContents *
get_old_tablespace_file_contents(void)
{
	return old_tablespace_file_contents;
}

bool
old_tablespace_file_contents_exists(void)
{
	return get_old_tablespace_file_contents() != NULL;
}

bool _is_old_tablespaces_file_empty;
bool _populate_gpdb6_cluster_tablespace_suffix_was_called;

char **_stubbed_tablespace_paths = NULL;
int _stubbed_number_of_tablespaces = 0;

static void stub_number_of_tablespaces(int stub_value)
{
	/*
	 * given the old cluster some non-null
	 * contents to signify that it is populated
	 */
	old_tablespace_file_contents = palloc0(sizeof(void *));

	_stubbed_number_of_tablespaces = stub_value;
}

static void stub_tablespace_paths(char **paths)
{
	_stubbed_tablespace_paths = paths;
}

static void
assert_contents_exist(OldTablespaceFileContents *contents)
{
	if (contents == NULL)
		fail_msg("unexpected null old tablespace file contents.");
}

char **
OldTablespaceFileContents_GetArrayOfTablespacePaths(OldTablespaceFileContents *contents)
{
	assert_contents_exist(contents);

	return _stubbed_tablespace_paths;
}

int
OldTablespaceFileContents_TotalNumberOfTablespaces(OldTablespaceFileContents *contents)
{
	assert_contents_exist(contents);

	return _stubbed_number_of_tablespaces;
}

/*
 * Stub functions:
 */

/*
 * implements is_old_tablespaces_file_empty to return stubbed value
 */
bool
is_old_tablespaces_file_empty(OldTablespaceFileContents *contents)
{
	return _is_old_tablespaces_file_empty;
}

void
populate_gpdb6_cluster_tablespace_suffix(ClusterInfo *cluster)
{
	_populate_gpdb6_cluster_tablespace_suffix_was_called = true;
	cluster->tablespace_suffix = "some-tablespace-suffix";
}

/* 
 * allows test to stub value for is_old_tablespaces_file_empty
 */
static void 
stub_is_old_tablespaces_file_empty(bool value)
{
	_is_old_tablespaces_file_empty = value;
}

/*
 * Test hooks
 */
static void
setup(void **state)
{
	test_cluster = malloc(sizeof(ClusterInfo));
	test_os_info = malloc(sizeof(OSInfo));
	old_cluster  = *test_cluster;
	os_info = *test_os_info;

	old_tablespace_file_contents = NULL;
	stub_is_old_tablespaces_file_empty(true);

	_populate_gpdb6_cluster_tablespace_suffix_was_called = false;
}

static void
teardown(void **state)
{
	free(test_cluster);
	free(test_os_info);
}

/*
 * Tests
 */
static void
test_when_postgres_version_8_4_tablespace_directory_suffix_remains_empty(void **state)
{
	old_cluster.major_version = 80400;

	init_tablespaces();

	assert_string_equal(old_cluster.tablespace_suffix, "");
}

static void
test_when_postgres_version_is_before_8_4_tablespace_directory_suffix_remains_empty(
	void **state)
{
	old_cluster.major_version = 80300;

	init_tablespaces();

	assert_string_equal(old_cluster.tablespace_suffix, "");
}

static void
test_when_postgres_version_newer_than_8_4_tablespace_directory_suffix_contains_PG_version_and_catalog_version(
	void **state)
{
	old_cluster.major_version = 80500;
	strcpy(old_cluster.major_version_str, "-SOME_MAJOR_VERSION_STRING-");
	old_cluster.controldata.cat_ver = 12345;

	init_tablespaces();

	assert_string_equal(old_cluster.tablespace_suffix,
	                    "/PG_-SOME_MAJOR_VERSION_STRING-_12345");
}

static void
test_when_postgres_version_matches_gpdb6_postgres_version_tablespace_directory_suffix_contains_GPDB6_tablespace_layout(
	void **state)
{
	old_cluster.major_version = 90400;

	init_tablespaces();

	assert_true(_populate_gpdb6_cluster_tablespace_suffix_was_called);
}

static void
test_when_file_is_empty_populate_is_not_called(
	void **state)
{
	old_cluster.gp_dbid = 999;
	old_cluster.major_version = 80400;
	new_cluster.major_version = 90400;

	stub_is_old_tablespaces_file_empty(true);

	init_tablespaces();

	assert_int_equal(os_info.num_old_tablespaces, 0);
}

static void
test_it_finds_old_tablespaces_when_provided_as_a_file(void **state)
{
	old_cluster.gp_dbid = 999;
	old_cluster.major_version = 80400;
	new_cluster.major_version = 90400;

	stub_is_old_tablespaces_file_empty(false);

	stub_number_of_tablespaces(2);

	char *tablespace_paths[] = {
		"/some/directory/for/999",
		"/some/other/directory/for/999"
	};

	stub_tablespace_paths(tablespace_paths);

	init_tablespaces();

	assert_int_equal(os_info.num_old_tablespaces, 2);
	assert_string_equal("/some/directory/for/999", os_info.old_tablespaces[0]);
	assert_string_equal("/some/other/directory/for/999", os_info.old_tablespaces[1]);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test_setup_teardown(
			test_it_finds_old_tablespaces_when_provided_as_a_file,
			setup,
			teardown),
		unit_test_setup_teardown(
			test_when_postgres_version_is_before_8_4_tablespace_directory_suffix_remains_empty,
			setup,
			teardown),
		unit_test_setup_teardown(
			test_when_postgres_version_8_4_tablespace_directory_suffix_remains_empty,
			setup,
			teardown),
		unit_test_setup_teardown(
			test_when_postgres_version_newer_than_8_4_tablespace_directory_suffix_contains_PG_version_and_catalog_version,
			setup,
			teardown),
		unit_test_setup_teardown(
			test_when_postgres_version_matches_gpdb6_postgres_version_tablespace_directory_suffix_contains_GPDB6_tablespace_layout,
			setup,
			teardown),
		unit_test_setup_teardown(
			test_when_file_is_empty_populate_is_not_called,
			setup,
			teardown),
	};

	return run_tests(tests);
}
