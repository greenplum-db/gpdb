#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "access/aosegfiles.h"
#include "utils/memutils.h"


static void
test_validate_segno_returns_false_when_segno_less_than_zero(void **state)
{
	bool error_thrown = false;
  
	PG_TRY();
	{
		ValidateAppendonlySegmentDataBeforeStorage(-1);
	}
	PG_CATCH();
	{
		error_thrown = true;
	}
	PG_END_TRY();

	assert_true(error_thrown);
}

static void
test_validate_segno_returns_true_when_segno_greater_than_or_equal_to_zero(void **state)
{
	bool error_thrown = false;
  
	PG_TRY();
	{
		ValidateAppendonlySegmentDataBeforeStorage(0);
		ValidateAppendonlySegmentDataBeforeStorage(1);
		ValidateAppendonlySegmentDataBeforeStorage(10);
		ValidateAppendonlySegmentDataBeforeStorage(100);
	}
	PG_CATCH();
	{
		error_thrown = true;
	}
	PG_END_TRY();

	assert_false(error_thrown);
}

static void
test_validate_segno_throws_error_when_value_is_greater_than_maximum_number_of_concurrent_writers(void **state)
{
	bool error_thrown = false;
	int some_number_above_aorel_concurrency = 1234;
  
	PG_TRY();
	{
		ValidateAppendonlySegmentDataBeforeStorage(some_number_above_aorel_concurrency);
	}
	PG_CATCH();
	{
		error_thrown = true;
	}
	PG_END_TRY();

	assert_true(error_thrown);
}

int 
main(int argc, char* argv[]) 
{
	cmockery_parse_arguments(argc, argv);
	
	MemoryContextInit();

	const UnitTest tests[] = {
	  unit_test(test_validate_segno_returns_false_when_segno_less_than_zero),
	  unit_test(test_validate_segno_returns_true_when_segno_greater_than_or_equal_to_zero),
	  unit_test(test_validate_segno_throws_error_when_value_is_greater_than_maximum_number_of_concurrent_writers)
	};

	return run_tests(tests);
}
