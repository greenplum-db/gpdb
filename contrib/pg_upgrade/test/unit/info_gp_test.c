#include "cmockery_gp.h"

#include "old_tablespace_file_contents.h"
#include "info_gp.h"

char *_stubbed_tablespace_path;

static void stub_old_tablespace_file_get_tablespace_path_for_oid(char *value)
{
	_stubbed_tablespace_path = value;
}

/*
 * old_tablespace_file_contents fakes
 */
bool
is_old_tablespaces_file_empty(OldTablespaceFileContents *contents)
{
	return true;
}

char *old_tablespace_file_get_tablespace_path_for_oid(OldTablespaceFileContents *contents, Oid oid)
{
	return _stubbed_tablespace_path;
}

static void *
make_fake_old_tablespace_file_contents(void)
{
	return palloc0(sizeof(void*));
}

static void
test_it_returns_no_file_status_when_there_is_not_a_file(void **state)
{
	Oid tablespace_oid = 0;

	GetTablespacePathResponse response = gp_get_tablespace_path(NULL, tablespace_oid);

	assert_int_equal(response.code, GetTablespacePathResponse_MISSING_FILE);
	assert_int_equal(response.tablespace_path, NULL);
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

	assert_int_equal(response.code, GetTablespacePathResponse_FOUND);
	assert_string_equal(response.tablespace_path, "some_path_to_tablespace/1234");
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
setup(void **state)
{
	_stubbed_tablespace_path = NULL;
}

static void
teardown(void **state)
{

}

int
main(int argc, char *argv[])
{
	const UnitTest tests[] = {
		unit_test_setup_teardown(test_it_returns_no_file_status_when_there_is_not_a_file, setup, teardown),
		unit_test_setup_teardown(test_it_returns_no_contents_in_file_when_there_is_a_file_and_there_are_no_contents, setup, teardown),
		unit_test_setup_teardown(test_it_returns_tablespace_path_when_tablespace_found_by_oid, setup, teardown),
		unit_test_setup_teardown(test_it_returns_tablespace_path_not_found_in_file_when_not_found, setup, teardown),
	};

	return run_tests(tests);
}
