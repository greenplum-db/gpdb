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

static bool _stubbed_old_tablespace_file_contents_is_empty = true;

static bool _populate_os_info_with_file_contents_was_called;

void
populate_os_info_with_file_contents(void)
{
	_populate_os_info_with_file_contents_was_called = true;
}

//OldTablespaceFileContents *
//get_old_tablespace_file_contents(void)
//{
//	return _stubbed_old_tablespace_file_contents;
//}

bool
old_tablespace_file_contents_exists(void)
{
	return _stubbed_old_tablespace_file_contents_is_empty != true;
}

bool _populate_gpdb6_cluster_tablespace_suffix_was_called;

static void stub_number_of_tablespaces(int stub_value)
{
	/*
	 * given the old cluster some non-null
	 * contents to signify that it is populated
	 */
	_stubbed_old_tablespace_file_contents_is_empty = false;
}

/*
 * Stub functions:
 */
void
populate_gpdb6_cluster_tablespace_suffix(ClusterInfo *cluster)
{
	_populate_gpdb6_cluster_tablespace_suffix_was_called = true;
	cluster->tablespace_suffix = "some-tablespace-suffix";
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

	_stubbed_old_tablespace_file_contents_is_empty = true;
	_populate_os_info_with_file_contents_was_called = false;
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
	old_cluster.major_version = 80400;
	new_cluster.major_version = 90400;

	init_tablespaces();

	assert_int_equal(os_info.num_old_tablespaces, 0);
}

static void
test_it_populates_old_tablespace_paths_from_greenplum_when_greenplum_has_file(void **state)
{
	stub_number_of_tablespaces(1);

	init_tablespaces();

	assert_true(_populate_os_info_with_file_contents_was_called);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test_setup_teardown(
			test_it_populates_old_tablespace_paths_from_greenplum_when_greenplum_has_file,
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
