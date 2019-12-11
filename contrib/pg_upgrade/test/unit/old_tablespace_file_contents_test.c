#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cmockery_gp.h"

#include "greenplum/old_tablespace_file_contents.h"

#include "greenplum/old_tablespace_file_parser.h"
#include "greenplum/old_tablespace_file_parser_observer.h"

struct OldTablespaceFileParser_DocumentData {};

int _faked_number_of_rows = -1;
char *fields[10][10];

char *
OldTablespaceFileParser_get_field_as_string(OldTablespaceFileParser_Document *document, int row_index, int field_index)
{
	return fields[row_index][field_index];
}

int
OldTablespaceFileParser_get_field_as_int(OldTablespaceFileParser_Document *document, int row_index, int field_index)
{
	return atoi(fields[row_index][field_index]);
}

Oid
OldTablespaceFileParser_get_field_as_oid(OldTablespaceFileParser_Document *document, int row_index, int field_index)
{
	return (Oid) strtoul(fields[row_index][field_index], NULL, 10);
}


int
OldTablespaceFileParser_number_of_rows(OldTablespaceFileParser_Document *document)
{
	return _faked_number_of_rows;
}

void
OldTablespaceFileParser_clear_document(OldTablespaceFileParser_Document *document)
{
	for (int row_index = 0; row_index < 10; row_index++)
		for (int column_index = 0; column_index < 10; column_index++)
			pfree(fields[row_index][column_index]);
}


static void stub_number_of_rows(int new_number_of_rows)
{
	_faked_number_of_rows = new_number_of_rows;
}

static void stub_field(int row_index, int field_index, char *value)
{
	fields[row_index][field_index] = strdup(value);
}

OldTablespaceFileParser_Document *
OldTablespaceFileParser_parse_file(FILE *file)
{
	OldTablespaceFileParser_Document *document = malloc(sizeof(OldTablespaceFileParser_Document));
	return document;
}

static void setup(void **state)
{
	_faked_number_of_rows = -1;
	
	for (int i = 0; i < 10; i++)
		for (int j = 0; j < 10; j++)
			fields[i][j] = NULL;
}

static void teardown(void **state)
{
	
}

static void
test_it_finds_old_tablespaces_when_provided_as_a_file(void **state)
{
	stub_number_of_rows(2);

	stub_field(0, 0, "123");
	stub_field(0, 1, "456");
	stub_field(0, 2, "some_space_name");
	stub_field(0, 3, "/some/directory/for/999");
	stub_field(0, 4, "0");

	stub_field(1, 0, "888");
	stub_field(1, 1, "777");
	stub_field(1, 2, "some_other_space_name");
	stub_field(1, 3, "/some/other/directory/for/999");
	stub_field(1, 4, "0");

	char *path = "/tmp/some/path";
	OldTablespaceFileContents *contents = parse_old_tablespace_file_contents(path);

	assert_int_equal(
		OldTablespaceFileContents_TotalNumberOfTablespaces(contents),
		2);

	char **tablespaces = OldTablespaceFileContents_GetArrayOfTablespacePaths(contents);
	assert_string_equal(tablespaces[0], "/some/directory/for/999");
	assert_string_equal(tablespaces[1], "/some/other/directory/for/999");
}

static void
test_it_returns_zero_tablespaces_when_content_is_empty(void **state)
{
	stub_number_of_rows(0);

	char *path = "/tmp/some/path";
	OldTablespaceFileContents *contents = parse_old_tablespace_file_contents(path);

	assert_int_equal(
		OldTablespaceFileContents_TotalNumberOfTablespaces(contents),
		0);
}

static void
test_it_can_filter_old_contents_by_dbid(void **state)
{
	stub_number_of_rows(2);

	stub_field(0, 0, "1");
	stub_field(0, 1, "123");
	stub_field(0, 2, "some_space_name");
	stub_field(0, 3, "/some/directory/for/123");
	stub_field(0, 4, "0");

	stub_field(1, 0, "2");
	stub_field(1, 1, "456");
	stub_field(1, 2, "some_other_space_name");
	stub_field(1, 3, "/some/other/directory/for/456");
	stub_field(1, 4, "0");

	OldTablespaceFileContents *contents = parse_old_tablespace_file_contents("some path");

	OldTablespaceFileContents *filteredContents = filter_old_tablespace_file_for_dbid(contents, 2);

	assert_int_equal(1,
		OldTablespaceFileContents_TotalNumberOfTablespaces(filteredContents));

	char **tablespace_paths = OldTablespaceFileContents_GetArrayOfTablespacePaths(filteredContents);
	assert_string_equal("/some/other/directory/for/456",
		tablespace_paths[0]);
}

static void
test_it_can_return_the_path_of_a_tablespace_for_a_given_oid(void **state)
{
	stub_number_of_rows(3);

	stub_field(0, 0, "1");
	stub_field(0, 1, "123");
	stub_field(0, 2, "some_space_name");
	stub_field(0, 3, "some path");
	stub_field(0, 4, "0");

	stub_field(1, 0, "1");
	stub_field(1, 1, "456");
	stub_field(1, 2, "some_space_name");
	stub_field(1, 3, "some other path");
	stub_field(1, 4, "0");

	stub_field(2, 0, "2");
	stub_field(2, 1, "123");
	stub_field(2, 2, "some_space_name");
	stub_field(2, 3, "some random path");
	stub_field(2, 4, "0");

	OldTablespaceFileContents *contents = parse_old_tablespace_file_contents("/some/path");

	char *found_path = "";
	OldTablespaceRecord *record;

	Oid current_oid = 123;
	record     = old_tablespace_file_get_record(contents, current_oid);
	found_path = OldTablespaceRecord_GetDirectoryPath(record);
	assert_int_not_equal((void *)found_path, NULL);
	assert_string_equal("some path", found_path);

	current_oid = 456;
	record     = old_tablespace_file_get_record(contents, current_oid);
	found_path = OldTablespaceRecord_GetDirectoryPath(record);
	assert_int_not_equal((void *)found_path, NULL);
	assert_string_equal("some other path", found_path);
}

static void
test_it_can_get_segment_records_from_contents(void **state)
{
	stub_number_of_rows(2);

	stub_field(0, 0, "1");
	stub_field(0, 1, "123");
	stub_field(0, 2, "some_space_name");
	stub_field(0, 3, "some path");
	stub_field(0, 4, "1");

	stub_field(1, 0, "2");
	stub_field(1, 1, "456");
	stub_field(1, 2, "some_space_name");
	stub_field(1, 3, "some path");
	stub_field(1, 4, "0");

	OldTablespaceFileContents *contents = parse_old_tablespace_file_contents("/some/path");

	OldTablespaceRecord **records = OldTablespaceFileContents_GetTablespaceRecords(contents);

	assert_string_equal(OldTablespaceRecord_GetTablespaceName(records[0]), "some_space_name");
	assert_string_equal(OldTablespaceRecord_GetDirectoryPath(records[0]), "some path");

	assert_true(OldTablespaceRecord_GetIsUserDefinedTablespace(records[0]));
	assert_false(OldTablespaceRecord_GetIsUserDefinedTablespace(records[1]));
}

static void
test_it_can_return_a_specific_tablespace_record_by_tablespace_name_and_dbid(void **state)
{
	stub_number_of_rows(3);

	stub_field(0, 0, "1");
	stub_field(0, 1, "123");
	stub_field(0, 2, "some_space_name");
	stub_field(0, 3, "some path for 123");
	stub_field(0, 4, "0");

	stub_field(1, 0, "2");
	stub_field(1, 1, "456");
	stub_field(1, 2, "some_space_name");
	stub_field(1, 3, "some path for 456");
	stub_field(1, 4, "0");

	stub_field(2, 0, "2");
	stub_field(2, 1, "789");
	stub_field(2, 2, "some_other_space_name");
	stub_field(2, 3, "some path for 789");
	stub_field(2, 4, "0");

	OldTablespaceFileContents *contents = parse_old_tablespace_file_contents("/some/path");

	OldTablespaceRecord *record = OldTablespaceFileContents_GetTablespaceRecord(contents,
		2, "some_other_space_name");

	assert_string_equal(OldTablespaceRecord_GetDirectoryPath(record), "some path for 789");

	OldTablespaceRecord *missingRecord = OldTablespaceFileContents_GetTablespaceRecord(contents,
		1, "some_other_space_name");

	assert_true(missingRecord == NULL);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test_setup_teardown(test_it_finds_old_tablespaces_when_provided_as_a_file, setup, teardown),
		unit_test_setup_teardown(test_it_returns_zero_tablespaces_when_content_is_empty, setup, teardown),
		unit_test_setup_teardown(test_it_can_filter_old_contents_by_dbid, setup, teardown),
		unit_test_setup_teardown(test_it_can_return_the_path_of_a_tablespace_for_a_given_oid, setup, teardown),
		unit_test_setup_teardown(test_it_can_get_segment_records_from_contents, setup, teardown),
		unit_test_setup_teardown(test_it_can_return_a_specific_tablespace_record_by_tablespace_name_and_dbid, setup, teardown),
	};

	return run_tests(tests);
}
