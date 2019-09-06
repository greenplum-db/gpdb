#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "utils/memutils.h"
#include "access/appendonlytid.h"

#include "../appendonly_visimap_entry.c"

static void
test__AppendOnlyVisimapEntry_GetFirstRowNum(void **state)
{
	int64 result, expected;
	ItemPointerData fake_ctid;
	AOTupleId  *tupleId = (AOTupleId *) &fake_ctid;

	/* 5 is less than APPENDONLY_VISIMAP_MAX_RANGE so should get 0 */
	AOTupleIdInit(tupleId, 1, 5);
	expected = 0;

	result = AppendOnlyVisimapEntry_GetFirstRowNum(NULL, tupleId);
	assert_true(result == expected);

	/* test to make sure we can go above INT32_MAX */
	AOTupleIdInit(tupleId, 1, 3000000000);
	expected = 2999975936;

	result = AppendOnlyVisimapEntry_GetFirstRowNum(NULL, tupleId);
	assert_true(result == expected);
}

static void
test__AppendOnlyVisimapEntry_CoversTuple(void **state)
{
	bool result;
	ItemPointerData fake_ctid;
	AOTupleId  *tupleId = (AOTupleId *) &fake_ctid;

	AOTupleIdInit(tupleId, 1, 5);

	AppendOnlyVisimapEntry* visiMapEntry = malloc(sizeof(AppendOnlyVisimapEntry));;

	/* Check to see if we have an invalid AppendOnlyVisimapEntry. */
	visiMapEntry->segmentFileNum = -1;
	visiMapEntry->firstRowNum = 32768;
	result = AppendOnlyVisimapEntry_CoversTuple(visiMapEntry, tupleId);
	assert_false(result);

	/* Check to see if we have mismatched segment file numbers. */
	visiMapEntry->segmentFileNum = 2;
	result = AppendOnlyVisimapEntry_CoversTuple(visiMapEntry, tupleId);
	assert_false(result);

	/* Tuple not covered by visimap entry. */
	visiMapEntry->segmentFileNum = 1;
	result = AppendOnlyVisimapEntry_CoversTuple(visiMapEntry, tupleId);
	assert_false(result);

	/* Tuple is covered by visimap entry. */
	visiMapEntry->firstRowNum = 0;
	result = AppendOnlyVisimapEntry_CoversTuple(visiMapEntry, tupleId);
	assert_true(result);

	/* Tuple is covered by visimap entry above INT32_MAX row number. */
	AOTupleIdInit(tupleId, 1, 3000000000);
	visiMapEntry->firstRowNum = 2999975936;
	result = AppendOnlyVisimapEntry_CoversTuple(visiMapEntry, tupleId);
	assert_true(result);
}


int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test__AppendOnlyVisimapEntry_GetFirstRowNum),
		unit_test(test__AppendOnlyVisimapEntry_CoversTuple)
	};

	MemoryContextInit();

	return run_tests(tests);
}
