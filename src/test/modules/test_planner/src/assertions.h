#include "postgres.h"
#include "utils/elog.h"

char* int_to_bool_string(int value);
char* int_to_int_string(int value);

void test_succeeded(
	const char *test_function_name,
	const char *test_file_name,
	int test_line_number);

void test_failed(
	char *expected,
	char *actual,
	const char *test_function_name,
	const char *test_file_name,
	int test_line_number);

#define assert_that_bool(actual, expected) \
	if (expected == actual) { \
		test_succeeded(__func__, __FILE__, __LINE__); \
	} else { \
		test_failed(int_to_bool_string(expected), int_to_bool_string(actual), __func__, __FILE__, __LINE__); \
	}

#define assert_that_int(actual, expected) \
	if (expected == actual) { \
		test_succeeded(__func__, __FILE__, __LINE__); \
	} else { \
		test_failed(int_to_int_string(expected), int_to_int_string(actual), __func__, __FILE__, __LINE__); \
	}

#define is_equal_to(expected) (expected)
