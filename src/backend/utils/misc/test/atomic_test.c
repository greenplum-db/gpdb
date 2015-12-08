#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "c.h"
#include "postgres.h"
#include "../atomic.c"

#define INT64_MAX 9223372036854775807
#define INT64_MIN -9223372036854775807
#define UINT64_MAX 18446744073709551615
#define UINT64_MIN 0


/*
 * Test gp_atomic_add_int64
 */
void
test__gp_atomic_add_int64(void **state)
{

        /* Running sub-test: gp_atomic_add_int64 small addition */
	int64 base = 25;
        int64 inc = 3;
        int64 result = 0;
        int64 expected_result = base + inc;
        result = gp_atomic_add_int64(&base, inc);
        /* Examine if the value of base has been increased by the value of inc */
        assert_true(result == expected_result && base == expected_result);
        assert_true(result <= INT64_MAX && result >= INT64_MIN && base <= INT64_MAX && base >= INT64_MIN);

        /* Running sub-test: gp_atomic_add_int64 small subtraction */
        inc = -4;
        result = 0;
        expected_result = base + inc;
        result = gp_atomic_add_int64(&base, inc);
        assert_true(result == expected_result && base == expected_result);
        assert_true(result <= INT64_MAX && result >= INT64_MIN && base <= INT64_MAX && base >= INT64_MIN);

        /* Running sub-test: gp_atomic_add_int64 huge addition */
        base = 37421634719307;
        inc  = 738246483234;
        result = 0;
        expected_result = base + inc;
        result = gp_atomic_add_int64(&base, inc);
        assert_true(result == expected_result && base == expected_result);
        assert_true(result <= INT64_MAX && result >= INT64_MIN && base <= INT64_MAX && base >= INT64_MIN);

        /* Ensure that an integer overflow occurs.*/
        inc = INT64_MAX;
        result = gp_atomic_add_int64(&base, inc);
        assert_true(base < 0);
        assert_true(result <= INT64_MAX && result >= INT64_MIN && base <= INT64_MAX && base >= INT64_MIN);

        /* Running sub-test: gp_atomic_add_int64 huge subtraction */
        base = 0;
        inc  = -32738246483234;
        result = 0;
        expected_result = base + inc;
        result = gp_atomic_add_int64(&base, inc);
        assert_true(result == expected_result && base == expected_result);
        assert_true(result <= INT64_MAX && result >= INT64_MIN && base <= INT64_MAX && base >= INT64_MIN);

        /* Ensure that an integer overflow occurs.*/
        inc = INT64_MIN;
        result = gp_atomic_add_int64(&base, inc);
        assert_true(base > 0);
        assert_true(result <= INT64_MAX && result >= INT64_MIN && base <= INT64_MAX && base >= INT64_MIN);

}


/*
 * Test gp_atomic_add_uint64
 */
void
test__gp_atomic_add_uint64(void **state)
{

	/* Running sub-test: gp_atomic_add_uint64 small addition */
	uint64 base = 25;
	int64 inc = 3;
	uint64 result = 0;
	uint64 expected_result = base + inc;
	result = gp_atomic_add_uint64(&base, inc);
	/* Examine if the value of base has been increased by the value of inc */
	assert_true(result == expected_result && base == expected_result);
	assert_true(result <= UINT64_MAX && result >= UINT64_MIN && base <= UINT64_MAX && base >= UINT64_MIN);

	/* Running sub-test: gp_atomic_add_uint64 huge addition */
	base = INT64_MAX;
	inc  = 738246483234;
	result = 0;
	expected_result = base + inc;
	result = gp_atomic_add_uint64(&base, inc);
	assert_true(result == expected_result && base == expected_result);
	assert_true(result <= UINT64_MAX && result >= UINT64_MIN && base <= UINT64_MAX && base >= UINT64_MIN);

	/* Ensure that an integer overflow occurs.*/
	base = UINT64_MAX;
	inc = 1;
	result = gp_atomic_add_uint64(&base, inc);
	assert_true(base == 0);

	/* Running sub-test: gp_atomic_add_uint64 negative inc */
#ifdef USE_ASSERT_CHECKING
	expect_any(ExceptionalCondition,conditionName);
	expect_any(ExceptionalCondition,errorType);
	expect_any(ExceptionalCondition,fileName);
	expect_any(ExceptionalCondition,lineNumber);
	will_be_called(ExceptionalCondition);

	/* inc should be either zero or a positive integer. So, negative inc should fail. */
	inc = -4;
	result = gp_atomic_add_uint64(&base, inc);
#endif

}



int
main(int argc, char* argv[]) {

	cmockery_parse_arguments(argc, argv);
	const UnitTest tests[] = {
		unit_test(test__gp_atomic_add_int64),
		unit_test(test__gp_atomic_add_uint64)
	};

	return run_tests(tests);
}
