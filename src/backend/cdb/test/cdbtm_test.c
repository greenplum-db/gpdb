#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"
#include "postgres.h"

#include "../cdbtm.c"

#define SIZE_OF_IN_PROGRESS_ARRAY (10 * sizeof(DistributedTransactionId))

void setup(TmControlBlock *controlBlock, TMGXACT *gxact_array)
{
	shmNumGxacts = &controlBlock->num_active_xacts;
	shmGxactArray = gxact_array;
	shmGIDSeq = &controlBlock->seqno;
	shmNextSnapshotId = &controlBlock->NextSnapshotId;
	shmDistribTimeStamp = &controlBlock->distribTimeStamp;

	/* Some imaginary LWLockId number */
	controlBlock->ControlLock = 1000;
	shmControlLock = controlBlock->ControlLock;
	*shmDistribTimeStamp = time(NULL);
	*shmGIDSeq = 25;
	*shmNumGxacts = 0;

	TMGXACT *tmp_gxact = (TMGXACT*)malloc(5 * sizeof(TMGXACT));
	shmGxactArray[0] = tmp_gxact++;
	shmGxactArray[1] = tmp_gxact++;
	shmGxactArray[2] = tmp_gxact++;
	shmGxactArray[3] = tmp_gxact++;
	shmGxactArray[4] = tmp_gxact++;
}

void
test__CreateDistributedSnapshot(void **state)
{
	TMGXACT gxact_array[5];
	TmControlBlock controlBlock;
	DistributedSnapshotWithLocalMapping distribSnapshotWithLocalMapping;
	DistributedSnapshot *ds = &distribSnapshotWithLocalMapping.ds;

	ds->inProgressXidArray =
		(DistributedTransactionId*)malloc(SIZE_OF_IN_PROGRESS_ARRAY);
	ds->maxCount = 10;

	distribSnapshotWithLocalMapping.inProgressMappedLocalXids =
		(TransactionId*) malloc(1 * sizeof(TransactionId));
	distribSnapshotWithLocalMapping.maxLocalXidsCount = 1;

	setup(&controlBlock, &gxact_array);

	expect_value_count(LWLockAcquire, lockid, shmControlLock, -1);
	expect_value_count(LWLockAcquire, mode, LW_EXCLUSIVE, -1);
	will_be_called_count(LWLockAcquire, -1);
	expect_value_count(LWLockRelease, lockid, shmControlLock, -1);
	will_be_called_count(LWLockRelease, -1);

	/* This is going to act as our gxact */
	shmGxactArray[0]->gxid = 20;
	shmGxactArray[0]->state = DTX_STATE_ACTIVE_DISTRIBUTED;
	shmGxactArray[0]->xminDistributedSnapshot = 20;
	(*shmNumGxacts)++;
	currentGxact = shmGxactArray[0];

	/********************************************************
	 * Basic case, no other in progress transaction in system
	 */
	memset(ds->inProgressXidArray, 0, SIZE_OF_IN_PROGRESS_ARRAY);
	CreateDistributedSnapshot(&distribSnapshotWithLocalMapping);

	/* perform all the validations */
	assert_true(ds->xminAllDistributedSnapshots == 20);
	assert_true(ds->xmin == 20);
	assert_true(ds->xmax == 25);
	assert_true(ds->count == 0);
	assert_true(currentGxact->xminDistributedSnapshot == 20);

	/*************************************************************************
	 * Case where there exist in-progress having taken snpashot with lower
	 * gxid than ours. This case demonstrates when xmin of snapshot will
	 * differ from xminAllDistributedSnapshots. Also, validates xmin and xmax
	 * get adjusted correctly based on in-progress.
	 */
	shmGxactArray[1]->gxid = 10;
	shmGxactArray[1]->state = DTX_STATE_ACTIVE_DISTRIBUTED;
	shmGxactArray[1]->xminDistributedSnapshot = 5;
	(*shmNumGxacts)++;

	shmGxactArray[2]->gxid = 30;
	shmGxactArray[2]->state = DTX_STATE_ACTIVE_DISTRIBUTED;
	shmGxactArray[2]->xminDistributedSnapshot = 20;
	(*shmNumGxacts)++;

	memset(ds->inProgressXidArray, 0, SIZE_OF_IN_PROGRESS_ARRAY);
	CreateDistributedSnapshot(&distribSnapshotWithLocalMapping);

	/* perform all the validations */
	assert_true(ds->xminAllDistributedSnapshots == 5);
	assert_true(ds->xmin == 10);
	assert_true(ds->xmax == 30);
	assert_true(ds->count == 2);
	assert_true(currentGxact->xminDistributedSnapshot == 10);

	/*************************************************************************
	 * Add more elemnets, just to have validation that in-progress array is in
	 * ascending sorted order with distributed transactions.
	 */
	shmGxactArray[3]->gxid = 15;
	shmGxactArray[3]->state = DTX_STATE_ACTIVE_DISTRIBUTED;
	shmGxactArray[3]->xminDistributedSnapshot = 12;
	(*shmNumGxacts)++;

	shmGxactArray[4]->gxid = 7;
	shmGxactArray[4]->state = DTX_STATE_ACTIVE_DISTRIBUTED;
	shmGxactArray[4]->xminDistributedSnapshot = 7;
	(*shmNumGxacts)++;

	memset(ds->inProgressXidArray, 0, SIZE_OF_IN_PROGRESS_ARRAY);
	CreateDistributedSnapshot(&distribSnapshotWithLocalMapping);

	/* perform all the validations */
	assert_true(ds->xminAllDistributedSnapshots == 5);
	assert_true(ds->xmin == 7);
	assert_true(ds->xmax == 30);
	assert_true(ds->count == 4);
	assert_true(currentGxact->xminDistributedSnapshot == 7);
	assert_true(ds->inProgressXidArray[0] == 7);
	assert_true(ds->inProgressXidArray[1] == 10);
	assert_true(ds->inProgressXidArray[2] == 15);
	assert_true(ds->inProgressXidArray[3] == 30);

	free(distribSnapshotWithLocalMapping.inProgressMappedLocalXids);
	free(ds->inProgressXidArray);
	free(shmGxactArray[0]);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] =
	{
		unit_test(test__CreateDistributedSnapshot)
	};

	MemoryContextInit();

	return run_tests(tests);
}
