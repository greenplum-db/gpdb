#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "utils/memutils.h"

#include "../instrument.c"

#define SIZE_OF_IN_PROGRESS_ARRAY (10 * sizeof(DistributedTransactionId))

static void
test__GetTmid_Test(void **state)
{
	assert_true(sizeof(pg_time_t) > sizeof(int32));
	/*
	 * For very large PgStartTime values, result from timestamptz_to_time_t either overflows or equals to -1,
	 * and should not match the value casted to int32 from gp_gettmid_helper()
	 */
	TimestampTz delta = 0x4000000000000000 >> 14;
	for (TimestampTz time = 0x7FFFFFFFFFFFFFFF - delta; time > 0x3fffffffffffffff; time -= delta) {
		PgStartTime = time;
		int32 tmid = gp_gettmid_helper();
		pg_time_t res = timestamptz_to_time_t(PgStartTime);
		assert_false((int64)tmid - res == 0);
	}
	/*
	 * For smaller PgStartTime values, the result from timestamptz_to_time_t casted to int32
	 * should match the result from gp_gettmid_helper()
	 */
	delta /= 4;
	for (TimestampTz time = 0x3ffffffffffff - delta; time >= 0; time -= delta) {
		PgStartTime = time;
		int32 tmid = gp_gettmid_helper();
		pg_time_t res = timestamptz_to_time_t(PgStartTime);
		assert_true((int64)tmid - res == 0);
	}

	/* gp_gettmid_helper should return -1 for negative PgStartTime */
	PgStartTime = -100;
	int32 tmid = gp_gettmid_helper();
	assert_true(tmid == -1);

	/* gp_gettmid_helper should return -1 for very large PgStartTime value */
	PgStartTime = 0x7FFFFFFFFFFFFFFF - delta;
	tmid = gp_gettmid_helper();
	assert_true(tmid == -1);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] =
	{
		unit_test(test__GetTmid_Test)
	};

	MemoryContextInit();

	return run_tests(tests);
}
