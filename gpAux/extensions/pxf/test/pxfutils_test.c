#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

/* Define UNIT_TESTING so that the extension can skip declaring PG_MODULE_MAGIC */
#define UNIT_TESTING

/* include unit under test */
#include "../src/pxfutils.c"

void
test_normalize_key_name(void **state)
{
	char	   *replaced = normalize_key_name("bar");

	assert_string_equal(replaced, "X-GP-BAR");
	pfree(replaced);

	replaced = normalize_key_name("FOO");
	assert_string_equal(replaced, "X-GP-FOO");
	pfree(replaced);

	/* test null string */
	MemoryContext old_context = CurrentMemoryContext;

	PG_TRY();
	{
		replaced = normalize_key_name(NULL);
		assert_false("Expected Exception");
	}
	PG_CATCH();
	{
		MemoryContextSwitchTo(old_context);
		ErrorData  *edata = CopyErrorData();

		assert_true(edata->elevel == ERROR);
		char	   *expected_message = pstrdup("internal error in pxfutils.c:normalize_key_name. Parameter key is null or empty.");

		assert_string_equal(edata->message, expected_message);
		pfree(expected_message);
	}
	PG_END_TRY();

	/* test empty string */
	PG_TRY();
	{
		replaced = normalize_key_name("");
		assert_false("Expected Exception");
	}
	PG_CATCH();
	{
		MemoryContextSwitchTo(old_context);
		ErrorData  *edata = CopyErrorData();

		assert_true(edata->elevel == ERROR);
		char	   *expected_message = pstrdup("internal error in pxfutils.c:normalize_key_name. Parameter key is null or empty.");

		assert_string_equal(edata->message, expected_message);
		pfree(expected_message);
	}
	PG_END_TRY();

}

void
test_get_authority(void **state)
{
	char	*authority = get_authority();
	assert_string_equal(authority, "localhost:51200");
	pfree(authority);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test_normalize_key_name),
		unit_test(test_get_authority)
	};

	MemoryContextInit();

	return run_tests(tests);
}
