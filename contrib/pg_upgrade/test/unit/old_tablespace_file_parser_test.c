#include "cmockery_gp.h"

#include "greenplum/old_tablespace_file_parser.h"
#include "greenplum/old_tablespace_file_parser_observer.h"

static int _invalid_access_to_row_called_with_row_index;
static int _invalid_access_to_field_called_with_row_index;
static int _invalid_access_to_field_called_with_field_index;

void
OldTablespaceFileParser_invalid_access_error_for_row(int row_index)
{
	_invalid_access_to_row_called_with_row_index = row_index;
}

void
OldTablespaceFileParser_invalid_access_error_for_field(int row_index, int field_index)
{
	_invalid_access_to_field_called_with_row_index = row_index;
	_invalid_access_to_field_called_with_field_index = field_index;
}

static void
test_it_returns_number_of_rows(void **state)
{
	FILE *file = tmpfile();
	fputs("1,joe\n", file);
	rewind(file);

	OldTablespaceFileParser_Document *document = (OldTablespaceFileParser_Document*) OldTablespaceFileParser_parse_file(
		file);

	fclose(file);

	int number_of_rows = OldTablespaceFileParser_number_of_rows(document);

	OldTablespaceFileParser_clear_document(document);

	assert_int_equal(number_of_rows, 1);
}

static void test_it_returns_zero_number_of_rows_for_empty_document(void **state)
{
	FILE *file = tmpfile();

	OldTablespaceFileParser_Document *document = (OldTablespaceFileParser_Document*) OldTablespaceFileParser_parse_file(
		file);

	fclose(file);

	int number_of_rows = OldTablespaceFileParser_number_of_rows(document);

	OldTablespaceFileParser_clear_document(document);

	assert_int_equal(number_of_rows, 0);
}

static void test_it_returns_two_rows_for_document_with_two_lines(void **state)
{
	FILE *file = tmpfile();
	fputs("1,123\n", file);
	fputs("2,456\n", file);
	rewind(file);

	OldTablespaceFileParser_Document *document = (OldTablespaceFileParser_Document*) OldTablespaceFileParser_parse_file(
		file);

	int number_of_rows = OldTablespaceFileParser_number_of_rows(document);

	OldTablespaceFileParser_clear_document(document);

	assert_int_equal(number_of_rows, 2);
}

static void test_the_document_returns_row_data(void **state)
{
	FILE *file = tmpfile();
	fputs("1,123\n", file);
	fputs("2,456\n", file);
	rewind(file);

	OldTablespaceFileParser_Document *document = (OldTablespaceFileParser_Document*) OldTablespaceFileParser_parse_file(
		file);

	assert_string_equal(OldTablespaceFileParser_get_field_as_string(document,
	                                                                0,
	                                                                0), "1");
	assert_string_equal(OldTablespaceFileParser_get_field_as_string(document,
	                                                                0,
	                                                                1), "123");
	assert_string_equal(OldTablespaceFileParser_get_field_as_string(document,
	                                                                1,
	                                                                0), "2");
	assert_string_equal(OldTablespaceFileParser_get_field_as_string(document,
	                                                                1,
	                                                                1), "456");

	OldTablespaceFileParser_clear_document(document);
}

static void
test_the_document_returns_row_data_as_integers(void **state)
{
	FILE *file = tmpfile();
	fputs("1,123\n", file);
	fputs("2,456\n", file);
	rewind(file);

	OldTablespaceFileParser_Document *document = (OldTablespaceFileParser_Document*) OldTablespaceFileParser_parse_file(
		file);

	assert_int_equal(OldTablespaceFileParser_get_field_as_int(document, 0, 0), 1);
	assert_int_equal(OldTablespaceFileParser_get_field_as_int(document, 0, 1), 123);
	assert_int_equal(OldTablespaceFileParser_get_field_as_int(document, 1, 0), 2);
	assert_int_equal(OldTablespaceFileParser_get_field_as_int(document, 1, 1), 456);

	OldTablespaceFileParser_clear_document(document);
}

static void
test_invalid_access_to_a_row_is_reported(void **state)
{
	FILE *file = tmpfile();
	fputs("1,123\n", file);
	rewind(file);

	OldTablespaceFileParser_Document *document = (OldTablespaceFileParser_Document*) OldTablespaceFileParser_parse_file(
		file);

	OldTablespaceFileParser_get_field_as_int(document, 1, 0);

	assert_int_equal(_invalid_access_to_row_called_with_row_index, 1);

	OldTablespaceFileParser_get_field_as_oid(document, 2, 0);

	assert_int_equal(_invalid_access_to_row_called_with_row_index, 2);
}

static void
test_invalid_access_to_a_field_is_reported(void **state)
{
	FILE *file = tmpfile();
	fputs("1,123\n", file);
	rewind(file);

	OldTablespaceFileParser_Document *document = (OldTablespaceFileParser_Document*) OldTablespaceFileParser_parse_file(
		file);

	OldTablespaceFileParser_get_field_as_int(document, 0, 2);

	assert_int_equal(_invalid_access_to_field_called_with_row_index, 0);
	assert_int_equal(_invalid_access_to_field_called_with_field_index, 2);

	OldTablespaceFileParser_get_field_as_oid(document, 0, 3);

	assert_int_equal(_invalid_access_to_field_called_with_row_index, 0);
	assert_int_equal(_invalid_access_to_field_called_with_field_index, 3);
}

static void 
setup(void **state)
{
	_invalid_access_to_row_called_with_row_index = -1;
	_invalid_access_to_field_called_with_row_index = -1;
	_invalid_access_to_field_called_with_field_index = -1;
}

static void
teardown(void **state)
{
	
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test_setup_teardown(test_it_returns_number_of_rows, setup, teardown),
		unit_test_setup_teardown(test_it_returns_zero_number_of_rows_for_empty_document, setup, teardown),
		unit_test_setup_teardown(test_it_returns_two_rows_for_document_with_two_lines, setup, teardown),
		unit_test_setup_teardown(test_the_document_returns_row_data, setup, teardown),
		unit_test_setup_teardown(test_the_document_returns_row_data_as_integers, setup, teardown),
		unit_test_setup_teardown(test_invalid_access_to_a_row_is_reported, setup, teardown),
		unit_test_setup_teardown(test_invalid_access_to_a_field_is_reported, setup, teardown),
	};

	return run_tests(tests);
}
