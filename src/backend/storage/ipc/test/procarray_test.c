#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"
#include "postgres.h"

#include "../procarray.c"

#define SIZE_OF_IN_PROGRESS_ARRAY (10 * sizeof(DistributedTransactionId))
#define MAX_PROCS 100

void setup(TmControlBlock *controlBlock)
{
	PGPROC *tmp_proc;

	shmNextSnapshotId = &controlBlock->NextSnapshotId;
	shmDistribTimeStamp = &controlBlock->distribTimeStamp;
	shmNumCommittedGxacts = &controlBlock->num_committed_xacts;

	/* Some imaginary LWLockId number */
	*shmDistribTimeStamp = time(NULL);
	*shmNumCommittedGxacts = 0;

	procArray = malloc(sizeof(ProcArrayStruct) + sizeof(PGPROC *) * (MAX_PROCS - 1));
	tmp_proc = (PGPROC *)malloc(5 * sizeof(PGPROC));
	procArray->procs[0] = tmp_proc++;
	procArray->procs[1] = tmp_proc++;
	procArray->procs[2] = tmp_proc++;
	procArray->procs[3] = tmp_proc++;
	procArray->procs[4] = tmp_proc++;

	procArray->maxProcs = MAX_PROCS;
}

void
test__CreateDistributedSnapshot(void **state)
{
	TmControlBlock controlBlock;
	DistributedSnapshotWithLocalMapping distribSnapshotWithLocalMapping;
	DistributedSnapshot *ds = &distribSnapshotWithLocalMapping.ds;

	ds->inProgressXidArray =
		(DistributedTransactionId*)malloc(SIZE_OF_IN_PROGRESS_ARRAY);
	ds->maxCount = 10;

	distribSnapshotWithLocalMapping.inProgressMappedLocalXids =
		(TransactionId*) malloc(1 * sizeof(TransactionId));
	distribSnapshotWithLocalMapping.maxLocalXidsCount = 1;

	setup(&controlBlock);

#ifdef USE_ASSERT_CHECKING
	expect_value_count(LWLockHeldByMe, lockid, ProcArrayLock, -1);
	will_return_count(LWLockHeldByMe, true, -1);
#endif

	will_return_count(getMaxDistributedXid, 25, -1);

	/* This is going to act as our gxact */
	procArray->procs[0]->gxact.gxid = 20;
	procArray->procs[0]->gxact.state = DTX_STATE_ACTIVE_DISTRIBUTED;
	procArray->procs[0]->gxact.xminDistributedSnapshot = 20;
	procArray->numProcs = 1;

	MyProc = procArray->procs[0];

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
	assert_true(MyProc->gxact.xminDistributedSnapshot == 20);

	/*************************************************************************
	 * Case where there exist in-progress having taken snpashot with lower
	 * gxid than ours. This case demonstrates when xmin of snapshot will
	 * differ from xminAllDistributedSnapshots. Also, validates xmin and xmax
	 * get adjusted correctly based on in-progress.
	 */
	procArray->procs[1]->gxact.gxid = 10;
	procArray->procs[1]->gxact.state = DTX_STATE_ACTIVE_DISTRIBUTED;
	procArray->procs[1]->gxact.xminDistributedSnapshot = 5;

	procArray->procs[2]->gxact.gxid = 30;
	procArray->procs[2]->gxact.state = DTX_STATE_ACTIVE_DISTRIBUTED;
	procArray->procs[2]->gxact.xminDistributedSnapshot = 20;
	procArray->numProcs = 3;

	memset(ds->inProgressXidArray, 0, SIZE_OF_IN_PROGRESS_ARRAY);
	CreateDistributedSnapshot(&distribSnapshotWithLocalMapping);

	/* perform all the validations */
	assert_true(ds->xminAllDistributedSnapshots == 5);
	assert_true(ds->xmin == 10);
	assert_true(ds->xmax == 30);
	assert_true(ds->count == 2);
	assert_true(MyProc->gxact.xminDistributedSnapshot == 10);

	/*************************************************************************
	 * Add more elemnets, just to have validation that in-progress array is in
	 * ascending sorted order with distributed transactions.
	 */
	procArray->procs[3]->gxact.gxid = 15;
	procArray->procs[3]->gxact.state = DTX_STATE_ACTIVE_DISTRIBUTED;
	procArray->procs[3]->gxact.xminDistributedSnapshot = 12;

	procArray->procs[4]->gxact.gxid = 7;
	procArray->procs[4]->gxact.state = DTX_STATE_ACTIVE_DISTRIBUTED;
	procArray->procs[4]->gxact.xminDistributedSnapshot = 7;
	procArray->numProcs = 5;

	memset(ds->inProgressXidArray, 0, SIZE_OF_IN_PROGRESS_ARRAY);
	CreateDistributedSnapshot(&distribSnapshotWithLocalMapping);

	/* perform all the validations */
	assert_true(ds->xminAllDistributedSnapshots == 5);
	assert_true(ds->xmin == 7);
	assert_true(ds->xmax == 30);
	assert_true(ds->count == 4);
	assert_true(MyProc->gxact.xminDistributedSnapshot == 7);
	assert_true(ds->inProgressXidArray[0] == 7);
	assert_true(ds->inProgressXidArray[1] == 10);
	assert_true(ds->inProgressXidArray[2] == 15);
	assert_true(ds->inProgressXidArray[3] == 30);

	free(distribSnapshotWithLocalMapping.inProgressMappedLocalXids);
	free(ds->inProgressXidArray);
	free(procArray->procs[0]);
	free(procArray);
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
