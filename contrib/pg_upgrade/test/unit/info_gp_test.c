#include "cmockery_gp.h"

#include "greenplum/old_tablespace_file_contents.h"
#include "greenplum/old_tablespace_file_gp_internal.h"
#include "greenplum/info_gp.h"
#include "greenplum/info_gp_internal.h"

char *_stubbed_tablespace_path;
bool _stubbed_is_user_defined_tablespace;
ClusterInfo old_cluster;

bool old_tablespace_file_contents_exists(void)
{
	return false;
}

bool
is_gpdb_version_with_filespaces(ClusterInfo *cluster)
{
	return true;
}

void
pg_fatal(const char *fmt,...)
{
	exit(1);
}

OldTablespaceFileContents *
get_old_tablespace_file_contents()
{
	return NULL;
}

struct OldTablespaceRecordData
{
	char *path;
	bool is_user_defined;
};


static void stub_old_tablespace_file_get_tablespace_path_for_oid(char *value)
{
	_stubbed_tablespace_path = value;
}

static void stub_is_user_defined_tablespace(bool value)
{
	_stubbed_is_user_defined_tablespace = value;
}

/*
 * old_tablespace_file_contents fakes
 */
bool
is_old_tablespaces_file_empty(OldTablespaceFileContents *contents)
{
	return true;
}

OldTablespaceRecord *
old_tablespace_file_get_record(OldTablespaceFileContents *contents, Oid oid)
{
	if (_stubbed_tablespace_path == NULL) return NULL;

	OldTablespaceRecord *record = palloc0(sizeof(OldTablespaceRecord));
	record->path = _stubbed_tablespace_path;
	record->is_user_defined = _stubbed_is_user_defined_tablespace;
	return record;
}

char *
OldTablespaceRecord_GetDirectoryPath(OldTablespaceRecord *record)
{
	return record->path;
}

bool
OldTablespaceRecord_GetIsUserDefinedTablespace(OldTablespaceRecord *record)
{
	return record->is_user_defined;
}

static void *
make_fake_old_tablespace_file_contents(void)
{
	return palloc0(sizeof(void*));
}

static void
test_it_returns_no_contents_in_file_when_there_is_a_file_and_there_are_no_contents(void **state)
{
	Oid tablespace_oid = 0;

	GetTablespacePathResponse response = gp_get_tablespace_path(make_fake_old_tablespace_file_contents(), tablespace_oid);

	assert_int_equal(response.code, GetTablespacePathResponse_NOT_FOUND_IN_FILE);
	assert_int_equal(response.tablespace_path, NULL);
}

static void
test_it_returns_tablespace_path_when_tablespace_found_by_oid(void **state)
{
	Oid tablespace_oid = 1234;

	stub_old_tablespace_file_get_tablespace_path_for_oid("some_path_to_tablespace");

	GetTablespacePathResponse response = gp_get_tablespace_path(make_fake_old_tablespace_file_contents(), tablespace_oid);

	assert_int_equal(response.code, GetTablespacePathResponse_FOUND_USER_DEFINED_TABLESPACE);
	assert_string_equal(response.tablespace_path, "some_path_to_tablespace");
}

static void
test_it_returns_tablespace_path_not_found_in_file_when_not_found(void **state)
{
	Oid tablespace_oid = 1234;

	stub_old_tablespace_file_get_tablespace_path_for_oid(NULL);

	GetTablespacePathResponse response = gp_get_tablespace_path(make_fake_old_tablespace_file_contents(), tablespace_oid);

	assert_int_equal(response.code, GetTablespacePathResponse_NOT_FOUND_IN_FILE);
	assert_int_equal(response.tablespace_path, NULL);
}

static void
test_it_returns_default_tablespace_when_record_is_default(void **state)
{
	Oid tablespace_oid = 1234;

	stub_old_tablespace_file_get_tablespace_path_for_oid("some/path");
	stub_is_user_defined_tablespace(false);

	GetTablespacePathResponse response = gp_get_tablespace_path(
		make_fake_old_tablespace_file_contents(),
		tablespace_oid);

	assert_int_equal(response.code, GetTablespacePathResponse_FOUND_SYSTEM_TABLESPACE);
}

static void
setup(void **state)
{
	_stubbed_tablespace_path = NULL;
	_stubbed_is_user_defined_tablespace = true;
}

static void
teardown(void **state)
{

}

int
main(int argc, char *argv[])
{
	const UnitTest tests[] = {
		unit_test_setup_teardown(test_it_returns_no_contents_in_file_when_there_is_a_file_and_there_are_no_contents, setup, teardown),
		unit_test_setup_teardown(test_it_returns_tablespace_path_when_tablespace_found_by_oid, setup, teardown),
		unit_test_setup_teardown(test_it_returns_tablespace_path_not_found_in_file_when_not_found, setup, teardown),
		unit_test_setup_teardown(test_it_returns_default_tablespace_when_record_is_default, setup, teardown),
	};

	return run_tests(tests);
}
