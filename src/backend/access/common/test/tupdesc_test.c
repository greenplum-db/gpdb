#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../tupdesc.c"

void
test__ReleaseTupleDesc__no_ref_count(void **state)
{
	TupleDesc td = CreateTemplateTupleDesc(2, true);

	td->tdrefcount = -1;

	/* should not do anything */
	ReleaseTupleDesc(td);

	assert_int_equal(-1, td->tdrefcount);

	td->tdrefcount = 0;

	/* should not do anything */
	ReleaseTupleDesc(td);

	assert_int_equal(0, td->tdrefcount);

	pfree(td);
}

void
test__ReleaseTupleDesc__ref_count(void **state)
{
	TupleDesc td = CreateTemplateTupleDesc(2, true);

	td->tdrefcount = 3;

	expect_any(ResourceOwnerForgetTupleDesc, owner);
	expect_value(ResourceOwnerForgetTupleDesc, tupdesc, td);
	will_be_called(ResourceOwnerForgetTupleDesc);

	/* should decrement refcount but not free */
	ReleaseTupleDesc(td);

	assert_int_equal(2, td->tdrefcount);

	pfree(td);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__ReleaseTupleDesc__no_ref_count),
		unit_test(test__ReleaseTupleDesc__ref_count)
	};

	MemoryContextInit();

	return run_tests(tests);
}
