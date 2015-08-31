#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../date.c"

/*
 * MPP-18509, avoid date_in from buffer overrun.
 */
void
test__date_in_overrun(void **state)
{
	bool	errored = false;
	PG_TRY();
	{
		DirectFunctionCall1(date_in, CStringGetTextDatum("."));
	}
	PG_CATCH();
	{
		errored = true;
	}
	PG_END_TRY();

	assert_true(errored);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__date_in_overrun),
	};

	MemoryContextInit();

	return run_tests(tests);
}

