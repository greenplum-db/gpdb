#include <stdbool.h>

#include "cmockery_gp.h"

#include "pg_upgrade.h"
#include "greenplum/pg_upgrade_greenplum.h"
#include "greenplum/old_tablespace_file_gp.h"
#include "greenplum/old_tablespace_file_gp_internal.h"
#include "greenplum/greenplum_cluster_info_internal.h"

#include "pg_upgrade_dummies.c"

static OldTablespaceFileContents *_stubbed_old_tablespace_file_contents;
OSInfo os_info;
static char **_stubbed_paths;
static int _stubbed_number_of_tablespaces;


void
set_old_tablespace_file_contents(
	OldTablespaceFileContents *new_value
	)
{
	_stubbed_old_tablespace_file_contents = new_value;
}

OldTablespaceFileContents *
get_old_tablespace_file_contents(void)
{
	return _stubbed_old_tablespace_file_contents;
}

/*
 * Test dummies
 */
void
clear_old_tablespace_file_contents(OldTablespaceFileContents *contents)
{

}

OldTablespaceFileContents *
filter_old_tablespace_file_for_dbid(OldTablespaceFileContents *contents,
                                    int gp_dbid)
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
int get_gp_dbid(GreenplumClusterInfo *info)
{
	return 999;
}

int
OldTablespaceFileContents_TotalNumberOfTablespaces(OldTablespaceFileContents *contents)
{
	if (_stubbed_old_tablespace_file_contents == NULL)
		pg_fatal("expected old_tablespace_file_contents to not be null");

	return _stubbed_number_of_tablespaces;
}

char **
OldTablespaceFileContents_GetArrayOfTablespacePaths(OldTablespaceFileContents *contents)
{
	if (_stubbed_old_tablespace_file_contents == NULL)
		pg_fatal("expected old_tablespace_file_contents to not be null");

	return _stubbed_paths;
}

static void stub_paths(char **paths)
{
	_stubbed_paths = paths;
}

static void stub_number_of_tablespaces(int n)
{
	_stubbed_number_of_tablespaces = n;
}

static void
test_it_populates_using_gpdb6_tablespace_layout(
        void **state)
{
    ClusterInfo clusterInfo;
    strcpy(clusterInfo.major_version_str, "-SOME_MAJOR_VERSION_STRING-");
    clusterInfo.controldata.cat_ver = 12345;

    populate_gpdb6_cluster_tablespace_suffix(&clusterInfo);

    assert_string_equal(clusterInfo.tablespace_suffix, "/999/GPDB_6_12345");
}

static void
test_cluster_needs_filespace_upgrade_when_the_old_cluster_version_is_before_gpdb6(
        void **state)
{
    /*
     * gpdb6 version is based on postgres 9.4
     * any version below this can be considered to have filespaces
     */
    ClusterInfo cluster;
    cluster.major_version = 90400;
    assert_false(is_gpdb_version_with_filespaces(&cluster));

    cluster.major_version = 90300;
    assert_true(is_gpdb_version_with_filespaces(&cluster));
}

static void
test_it_finds_old_tablespaces_when_provided_as_a_file(void **state)
{
	char **paths = palloc0(sizeof(char*) * 2);
	paths[0] = "/some/directory/for/999";
	paths[1] = "/some/other/directory/for/999";

	stub_paths(paths);
	stub_number_of_tablespaces(2);

	OldTablespaceFileContents *contents = palloc0(sizeof(void *));
	set_old_tablespace_file_contents(contents);

	populate_os_info_with_file_contents();

	assert_int_equal(os_info.num_old_tablespaces, 2);
	assert_string_equal("/some/directory/for/999", os_info.old_tablespaces[0]);
	assert_string_equal("/some/other/directory/for/999", os_info.old_tablespaces[1]);
}

int
main(int argc, char *argv[])
{
    cmockery_parse_arguments(argc, argv);

    const UnitTest tests[] = {
	    unit_test(
		    test_it_finds_old_tablespaces_when_provided_as_a_file
	    ),
	    unit_test(test_it_populates_using_gpdb6_tablespace_layout),
            unit_test(
                    test_cluster_needs_filespace_upgrade_when_the_old_cluster_version_is_before_gpdb6),
    };

    return run_tests(tests);
}