#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../appendonly_visimap.c"

/*
 * Simulate out-of-order tuples encountered by appendonly_delete().
 * Particularly, the case when the current visimap entry has already
 * been stashed at least once.
 */
void
test__AppendOnlyVisimapDelete_Finish_outoforder(void **state)
{
	AppendOnlyVisiMapDeleteKey key;
	AppendOnlyVisiMapDeleteData val;
	AppendOnlyVisimapDelete visiMapDelete;
	AppendOnlyVisimap visiMap;
	bool		found;

	visiMapDelete.visiMap = &visiMap;
	visiMap.visimapEntry.segmentFileNum = 2;
	visiMap.visimapEntry.firstRowNum = 32768;
	visiMap.visimapEntry.dirty = true;
	key.segno = 2;
	key.firstRowNum = 32768;
	/* should be changed by AppendOnlyVisimapDelete_Finish() */
	val.workFileOffset = 0;

	expect_value(AppendOnlyVisimapEntry_HasChanged, visiMapEntry,
				 &visiMap.visimapEntry);
	will_return(AppendOnlyVisimapEntry_HasChanged, true);

#ifdef USE_ASSERT_CHECKING

	/*
	 * AppendOnlyVisimap_Store calls Assert(AppendOnlyVisimapEntry_IsValid)
	 */
	expect_any(AppendOnlyVisimapEntry_IsValid, visiMapEntry);
	will_return(AppendOnlyVisimapEntry_IsValid, true);
#endif

	expect_any(AppendOnlyVisimapStore_Store, visiMapStore);
	expect_any(AppendOnlyVisimapStore_Store, visiMapEntry);
	will_be_called(AppendOnlyVisimapStore_Store);

	expect_any(hash_search, hashp);
	expect_value(hash_search, action, HASH_FIND);
	expect_any(hash_search, keyPtr);
	expect_any(hash_search, foundPtr);
	will_assign_value(hash_search, foundPtr, true);
	will_return(hash_search, &val);

	expect_any(hash_destroy, hashp);
	will_be_called(hash_destroy);

	expect_any(ExecWorkFile_Close, workfile);
	will_return(ExecWorkFile_Close, 0);

	expect_any(hash_get_num_entries, hashp);
	will_return(hash_get_num_entries, 0);

	AppendOnlyVisimapDelete_Finish(&visiMapDelete);
	assert_int_equal(val.workFileOffset, INT64_MAX);
}


int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test__AppendOnlyVisimapDelete_Finish_outoforder)
	};

	MemoryContextInit();

	return run_tests(tests);
}
