#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../cdbbackup.c"

void
test__parse_prefix_from_params_valid_param(void **state)
{
    char *pszPassThroughParameters = strdup("--prefixfoo");
    char *result = parse_prefix_from_params(pszPassThroughParameters);
    free(pszPassThroughParameters);

    assert_true(strcmp(result, "foo") == 0);
}

void
test__parse_prefix_from_params_no_param(void **state)
{
    char *pszPassThroughParameters = strdup("--prefix");
    char *result = parse_prefix_from_params(pszPassThroughParameters);
    free(pszPassThroughParameters);
    
    assert_true(result == NULL);
}

void
test__parse_prefix_from_params_no_prefix(void **state)
{
    char *pszPassThroughParameters = strdup("foo");
    char *result = parse_prefix_from_params(pszPassThroughParameters);
    free(pszPassThroughParameters);
    
    assert_true(result == NULL);
}

void
test__parse_prefix_from_params_null_param(void **state)
{
    char *pszPassThroughParameters = NULL;
    char *result = parse_prefix_from_params(pszPassThroughParameters);
    free(pszPassThroughParameters);
    
    assert_true(result == NULL);
}

void
test__parse_prefix_from_params_space(void **state)
{
    char *pszPassThroughParameters = strdup("--prefix foo");
    char *result = parse_prefix_from_params(pszPassThroughParameters);
    free(pszPassThroughParameters);
    
    assert_true(strcmp(result, "foo") == 0);
}

void
test__parse_prefix_from_params_truncate(void **state)
{
    char *pszPassThroughParameters = strdup("--prefix foo bar");
    char *result = parse_prefix_from_params(pszPassThroughParameters);
    
    assert_true(strcmp(result, "foo") == 0);
    assert_true(strstr(pszPassThroughParameters, "bar") != NULL);
    free(pszPassThroughParameters);
}

void
test__parse_prefix_from_params_with_pre_incremental(void **state)
{
    char *pszPassThroughParameters = strdup("--incremental --prefix foo");
    char *result = parse_prefix_from_params(pszPassThroughParameters);
    free(pszPassThroughParameters);
    
    assert_true(strcmp(result, "foo") == 0);
}

void
test_parse_option_from_params_1(void **state)
{
	char *pszPassThroughParameters = strdup("--netbackup-service-host mdw --netbackup-policy test_policy --netbackup-schedule test_schedule");
	char *parseOption = strdup("--netbackup-service-host");
	char *result = parse_option_from_params(pszPassThroughParameters, parseOption);
	free(pszPassThroughParameters);
	free(parseOption);

	assert_true(strcmp(result, "mdw") == 0);
}

void
test_parse_option_from_params_2(void **state)
{
	char *pszPassThroughParameters = NULL;
	char *parseOption = strdup("--netbackup-service-host");
	char *result = parse_option_from_params(pszPassThroughParameters, parseOption);
	free(parseOption);

	assert_true(result == NULL);
}

void
test_parse_option_from_params_3(void **state)
{
	char *pszPassThroughParameters = strdup("--netbackup-service-host mdw --netbackup-policy test_policy --netbackup-schedule test_schedule");
	char *parseOption = NULL;
	char *result = parse_option_from_params(pszPassThroughParameters, parseOption);
	free(pszPassThroughParameters);

	assert_true(result == NULL);
}

void
test_parse_option_from_params_4(void **state)
{
	char *pszPassThroughParameters = strdup("--netbackup-policy test_policy --netbackup-schedule test_schedule");
	char *parseOption = strdup("--netbackup-service-host");
	char *result = parse_option_from_params(pszPassThroughParameters, parseOption);
	free(pszPassThroughParameters);
	free(parseOption);

	assert_true(result == NULL);
}

void
test_parse_option_from_params_5(void **state)
{
	char *pszPassThroughParameters = strdup("--netbackup-service-host mdw --netbackup-policy test_policy --netbackup-schedule");
	char *parseOption = strdup("--netbackup-schedule");
	char *result = parse_option_from_params(pszPassThroughParameters, parseOption);
	free(pszPassThroughParameters);
	free(parseOption);

	assert_true(result == NULL);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__parse_prefix_from_params_valid_param),
		unit_test(test__parse_prefix_from_params_no_param),
		unit_test(test__parse_prefix_from_params_null_param),
		unit_test(test__parse_prefix_from_params_space),
		unit_test(test__parse_prefix_from_params_truncate),
		unit_test(test__parse_prefix_from_params_no_prefix),
		unit_test(test__parse_prefix_from_params_with_pre_incremental),
		unit_test(test_parse_option_from_params_1),
		unit_test(test_parse_option_from_params_2),
		unit_test(test_parse_option_from_params_3),
		unit_test(test_parse_option_from_params_4),
		unit_test(test_parse_option_from_params_5)
	};

	MemoryContextInit();

	return run_tests(tests);
}
