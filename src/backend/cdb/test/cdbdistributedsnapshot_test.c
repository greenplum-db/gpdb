#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "utils/memutils.h"

#include "../cdbdistributedsnapshot.c"

#define SIZE_OF_IN_PROGRESS_ARRAY (10 * sizeof(DistributedTransactionId))

static void
test__DistributedSnapshotWithLocalMapping_CommittedTest(void **state)
{
	DistributedSnapshotCommitted retval;
	DistributedTransactionTimeStamp timeStamp = time(NULL);
	DistributedSnapshotWithLocalMapping dslm;
	DistributedSnapshot *ds = &dslm.ds;

	/* Static initializations */
	{
		dslm.minCachedLocalXid = InvalidTransactionId;
		dslm.maxCachedLocalXid = InvalidTransactionId;
		dslm.currentLocalXidsCount = 0;

		dslm.inProgressMappedLocalXids =
			(TransactionId*)malloc(5 * sizeof(TransactionId));
		dslm.maxLocalXidsCount = 5;

		ds->inProgressXidArray =
			(DistributedTransactionId*)malloc(SIZE_OF_IN_PROGRESS_ARRAY);
		ds->maxCount = 10;
		ds->distribSnapshotId = 12345;
		ds->distribTransactionTimeStamp = timeStamp;
	}

	/*
	 * Define how distributed xids will map to localXids. For the purpose of
	 * the testing keep it extremely simple distribXid == 10 * localXid.
	 */
	{
		expect_any_count(DistributedLog_CommittedCheck, distribTimeStamp, -1);
		expect_any_count(DistributedLog_CommittedCheck, distribXid, -1);
		will_return_count(DistributedLog_CommittedCheck, true, -1);

		expect_value(DistributedLog_CommittedCheck, localXid, 10);
		will_assign_value(DistributedLog_CommittedCheck, distribXid, 10 * 10);
		will_assign_value(DistributedLog_CommittedCheck,
						  distribTimeStamp, timeStamp);

		expect_value(DistributedLog_CommittedCheck, localXid, 20);
		will_assign_value(DistributedLog_CommittedCheck, distribXid, 10 * 20);
		will_assign_value(DistributedLog_CommittedCheck,
						  distribTimeStamp, timeStamp);

		expect_value(DistributedLog_CommittedCheck, localXid, 5);
		will_assign_value(DistributedLog_CommittedCheck, distribXid, 10 * 5);
		will_assign_value(DistributedLog_CommittedCheck,
						  distribTimeStamp, timeStamp);

		expect_value(DistributedLog_CommittedCheck, localXid, 15);
		will_assign_value(DistributedLog_CommittedCheck, distribXid, 10 * 15);
		will_assign_value(DistributedLog_CommittedCheck,
						  distribTimeStamp, timeStamp);
	}

	/* Empty in-progress array test */
	{
		ds->xminAllDistributedSnapshots = 3;
		ds->xmin = 3;
		ds->xmax = 3;
		ds->count = 0;

		retval = DistributedSnapshotWithLocalMapping_CommittedTest(&dslm, 10, false);
		assert_true(retval == DISTRIBUTEDSNAPSHOT_COMMITTED_INPROGRESS);
		assert_true(dslm.currentLocalXidsCount == 0);
		assert_true(dslm.minCachedLocalXid == InvalidTransactionId);
		assert_true(dslm.maxCachedLocalXid == InvalidTransactionId);
	}

	/* Populated in-progress array for validating the local cache working */
	{
		ds->xminAllDistributedSnapshots = 3;
		ds->xmin = 3;
		ds->xmax = 300;
		ds->count = 3;
		ds->inProgressXidArray[0] = 50;
		ds->inProgressXidArray[1] = 100;
		ds->inProgressXidArray[2] = 200;
	}

	/* First time the local xid cache should get populated */
	retval = DistributedSnapshotWithLocalMapping_CommittedTest(&dslm, 10, false);
	assert_true(retval == DISTRIBUTEDSNAPSHOT_COMMITTED_INPROGRESS);
	assert_true(dslm.currentLocalXidsCount == 1);
	assert_true(dslm.minCachedLocalXid == 10);
	assert_true(dslm.maxCachedLocalXid == 10);
	assert_true(dslm.inProgressMappedLocalXids[0] == 10);

	/*
	 * Next call nothing should change in snapshot's local xid cache. Also,
	 * importantly based on cache
	 * DistributedSnapshotWithLocalMapping_CommittedTest() should return early
	 * and not reach to reversemap distributed xid again. With the beauty of
	 * this framework its getting validated, as if
	 * DistributedSnapshotWithLocalMapping_CommittedTest() doesn't correctly
	 * return based on cache, would call DistributedLog_CommittedCheck() and
	 * since we coded above that it should get called only once with localXid
	 * == 10, verifies the return was based on cache.
	 */
	retval = DistributedSnapshotWithLocalMapping_CommittedTest(&dslm, 10, false);
	assert_true(retval == DISTRIBUTEDSNAPSHOT_COMMITTED_INPROGRESS);
	assert_true(dslm.currentLocalXidsCount == 1);
	assert_true(dslm.minCachedLocalXid == 10);
	assert_true(dslm.maxCachedLocalXid == 10);
	assert_true(dslm.inProgressMappedLocalXids[0] == 10);

	/* Now lets simulate we got tuple with xid=20 */
	retval = DistributedSnapshotWithLocalMapping_CommittedTest(&dslm, 20, false);
	assert_true(retval == DISTRIBUTEDSNAPSHOT_COMMITTED_INPROGRESS);
	assert_true(dslm.currentLocalXidsCount == 2);
	assert_true(dslm.minCachedLocalXid == 10);
	assert_true(dslm.maxCachedLocalXid == 20);
	assert_true(dslm.inProgressMappedLocalXids[0] == 10);
	assert_true(dslm.inProgressMappedLocalXids[1] == 20);

	/* Now lets simulate we got tuple with xid=5 */
	retval = DistributedSnapshotWithLocalMapping_CommittedTest(&dslm, 5, false);
	assert_true(retval == DISTRIBUTEDSNAPSHOT_COMMITTED_INPROGRESS);
	assert_true(dslm.currentLocalXidsCount == 3);
	assert_true(dslm.minCachedLocalXid == 5);
	assert_true(dslm.maxCachedLocalXid == 20);
	assert_true(dslm.inProgressMappedLocalXids[0] == 10);
	assert_true(dslm.inProgressMappedLocalXids[1] == 20);
	assert_true(dslm.inProgressMappedLocalXids[2] == 5);

	/*
	 * Lets revalidate that local cache is working and
	 * DistributedSnapshotWithLocalMapping_CommittedTest() returns result
	 * based on local cache when more than one element is present in cache.
	 */
	retval = DistributedSnapshotWithLocalMapping_CommittedTest(&dslm, 20, false);
	assert_true(retval == DISTRIBUTEDSNAPSHOT_COMMITTED_INPROGRESS);
	assert_true(dslm.currentLocalXidsCount == 3);
	assert_true(dslm.minCachedLocalXid == 5);
	assert_true(dslm.maxCachedLocalXid == 20);
	assert_true(dslm.inProgressMappedLocalXids[0] == 10);
	assert_true(dslm.inProgressMappedLocalXids[1] == 20);
	assert_true(dslm.inProgressMappedLocalXids[2] == 5);

	/*
	 * Test where local cache should not be touched, if distributedXid is not
	 * in-progress.
	 */
	retval = DistributedSnapshotWithLocalMapping_CommittedTest(&dslm, 15, false);
	assert_true(retval == DISTRIBUTEDSNAPSHOT_COMMITTED_VISIBLE);
	assert_true(dslm.currentLocalXidsCount == 3);
	assert_true(dslm.minCachedLocalXid == 5);
	assert_true(dslm.maxCachedLocalXid == 20);
	assert_true(dslm.inProgressMappedLocalXids[0] == 10);
	assert_true(dslm.inProgressMappedLocalXids[1] == 20);
	assert_true(dslm.inProgressMappedLocalXids[2] == 5);

	free(ds->inProgressXidArray);
	free(dslm.inProgressMappedLocalXids);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] =
	{
		unit_test(test__DistributedSnapshotWithLocalMapping_CommittedTest)
	};

	MemoryContextInit();

	return run_tests(tests);
}
