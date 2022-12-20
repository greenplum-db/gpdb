#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../cdbdispatchresult.c"

#define UNITTEST_NUM_SEGS 2

static struct CdbDispatchResults *
_init_cdbdisp_makeResult()
{
	struct CdbDispatchResults *results =
	(struct CdbDispatchResults *) palloc0(sizeof(*results));

	results->resultArray = palloc0(UNITTEST_NUM_SEGS * sizeof(results->resultArray[0]));
	results->resultCapacity = UNITTEST_NUM_SEGS;

	return results;
}

/*
 * Test cdbdisp_makeResult would return NULL if OOM happens
 */
static void
test__cdbdisp_makeResult__oom(void **state)
{
	CdbDispatchResult *result = NULL;

	struct CdbDispatchResults *results = _init_cdbdisp_makeResult();
	struct SegmentDatabaseDescriptor *segdbDesc =
	(struct SegmentDatabaseDescriptor *) palloc0(sizeof(struct SegmentDatabaseDescriptor));

	/*
	 * createPQExpBuffer is supposed to return NULL in OOM cases
	 */
	will_return_count(createPQExpBuffer, NULL, -1);
	expect_any_count(destroyPQExpBuffer, str, -1);
	will_be_called_count(destroyPQExpBuffer, -1);
	result = cdbdisp_makeResult(results, segdbDesc, 0);
	assert_true(result == NULL);
}

static void
test__PQprocessAoTupCounts__uses_correct_hash_function(void **state)
{
	HTAB *ht = NULL;
	int naotupcounts = 2;
	PQaoRelTupCount *entry1;
	PQaoRelTupCount *entry2;

	PQaoRelTupCount *aotupcounts = (PQaoRelTupCount *) malloc(sizeof(PQaoRelTupCount) * naotupcounts);
	/*
	 * We purposely use aorelid's 16384 and 16640 since they collide when using
	 * the default string_hash, which previously caused issues.
	 */
	aotupcounts[0].aorelid = 16384;
	aotupcounts[0].tupcount = 0;

	aotupcounts[1].aorelid = 16640;
	aotupcounts[1].tupcount = 8;

	ht = PQprocessAoTupCounts(ht, (void *) aotupcounts, naotupcounts);

	entry1 = hash_search(ht, &(aotupcounts[0].aorelid), HASH_FIND, NULL);
	entry2 = hash_search(ht, &(aotupcounts[1].aorelid), HASH_FIND, NULL);

	assert_int_not_equal(entry1->tupcount, entry2->tupcount);
	free(aotupcounts);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] =
	{
		unit_test(test__cdbdisp_makeResult__oom),
		unit_test(test__PQprocessAoTupCounts__uses_correct_hash_function)
	};

	Gp_role = GP_ROLE_DISPATCH;
	MemoryContextInit();

	return run_tests(tests);
}
