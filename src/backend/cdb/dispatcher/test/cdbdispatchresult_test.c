#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../cdbdispatchresult.c"

#define UNITTEST_NUM_SEGS 2

struct CdbDispatchResults *
_init_cdbdisp_makeResult()
{
	struct CdbDispatchResults *results =
		(struct CdbDispatchResults *) palloc0(sizeof(*results));

	results->resultArray = palloc0(UNITTEST_NUM_SEGS * sizeof(results->resultArray[0]));
	results->resultCapacity = UNITTEST_NUM_SEGS;

	return results;
}

PQExpBuffer __wrap_createPQExpBuffer(void)
{
	return (PQExpBuffer) mock();
}

void
__wrap_destroyPQExpBuffer(PQExpBuffer str)
{
	mock();
}

void
test__cdbdisp_makeResult__oom(void **state)
{
	CdbDispatchResult *result = NULL;

	struct CdbDispatchResults *results = _init_cdbdisp_makeResult();
	struct SegmentDatabaseDescriptor *segdbDesc =
		(struct SegmentDatabaseDescriptor *) palloc0(sizeof(struct SegmentDatabaseDescriptor));

	will_return(__wrap_createPQExpBuffer, NULL);
	will_be_called(__wrap_destroyPQExpBuffer);
	result = cdbdisp_makeResult(results, segdbDesc, 0);
	assert_true(result == NULL);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

    const UnitTest tests[] =
    {
        unit_test(test__cdbdisp_makeResult__oom)
    };

	Gp_role = GP_ROLE_DISPATCH;
	MemoryContextInit();

    return run_tests(tests);
}
