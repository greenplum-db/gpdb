#include "assertions.h"
#include <stdio.h>
#include <stdlib.h>

char *
int_to_bool_string(int value)
{
	if (value == 0) return "false";
	return "true";
}

char *
int_to_int_string(int value)
{
	char *buffer = malloc(sizeof(char*));
	sprintf(buffer, "%d", value);
	return buffer;
}

void
test_succeeded(
	const char* test_function_name,
	const char* test_file_name,
	int test_line_number)
{
	elog(
		INFO,
		"Success %s - %s:%d",
		test_function_name,
		test_file_name,
		test_line_number);
}

void
test_failed(
	char *expected,
	char *actual,
	const char *test_function_name,
	const char *test_file_name,
	int test_line_number)
{
	elog(
		WARNING,
		"expected %s, was %s in %s - %s:%d",
		expected,
		actual,
		test_function_name,
		test_file_name,
		test_line_number);
}
