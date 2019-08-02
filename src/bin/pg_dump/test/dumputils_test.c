#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "c.h"
#include "../dumputils.c"


/*
 * Test for custom_fmtopts_string()
 * This function receives a string in the form of "key value key value"
 * and converts it to "key = value,key = value" format.
 * (Example input:  formatter E'fixedwidth_in' null E' ' preserve_blanks E'on')
 */
static void 
test__custom_fmtopts_string(void **state)
{
	char* result = custom_fmtopts_string(NULL);
	assert_true(result==NULL);

	result = custom_fmtopts_string("");
	assert_string_equal(result, "");

	free(result);

	result = custom_fmtopts_string("formatter E'fixedwidth_in' null E' '");
	assert_string_equal(result, "formatter = 'fixedwidth_in',null = ' '");

	free(result);

	result = custom_fmtopts_string("formatter E'fixedwidth_in' comma E'\\'' null E' '");
	assert_string_equal(result, "formatter = 'fixedwidth_in',comma = '\\'',null = ' '");

	free(result);

	result = custom_fmtopts_string("formatter E'fixedwidth_in' null E' ' preserve_blanks E'on' comma E'\\''");
	assert_string_equal(result, "formatter = 'fixedwidth_in',null = ' ',preserve_blanks = 'on',comma = '\\''");

	free(result);

}


int 
main(int argc, char* argv[]) 
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
			unit_test(test__custom_fmtopts_string)
	};
	return run_tests(tests);
}
