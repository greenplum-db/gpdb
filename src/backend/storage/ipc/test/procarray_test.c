#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"
#include "postgres.h"

#include "../procarray.c"

#define SIZE_OF_IN_PROGRESS_ARRAY (10 * sizeof(DistributedTransactionId))
#define MAX_PROCS 100
VariableCacheData vcdata;
uint32 nextSnapshotId;
int num_committed_xacts;

static void
setup(void)
{
	ShmemVariableCache = &vcdata;
	shmNextSnapshotId = &nextSnapshotId;
	shmNumCommittedGxacts = &num_committed_xacts;

	/* Some imaginary LWLockId number */
	*shmNumCommittedGxacts = 0;

	allTmGxact = malloc(sizeof(TMGXACT)*MAX_PROCS);

	procArray = malloc(sizeof(ProcArrayStruct) + sizeof(int) * (MAX_PROCS - 1));
	procArray->pgprocnos[0] = 0;
	procArray->pgprocnos[1] = 1;
	procArray->pgprocnos[2] = 2;
	procArray->pgprocnos[3] = 3;
	procArray->pgprocnos[4] = 4;

	procArray->maxProcs = MAX_PROCS;
}

static void
test__CreateDistributedSnapshot(void **state)
{
	DistributedSnapshot ds;

	ds.inProgressXidArray =
		(DistributedTransactionId*)malloc(SIZE_OF_IN_PROGRESS_ARRAY);

	setup();

#ifdef USE_ASSERT_CHECKING
	expect_value_count(LWLockHeldByMe, l, ProcArrayLock, -1);
	will_return_count(LWLockHeldByMe, true, -1);
#endif

	ShmemVariableCache->latestCompletedGxid = 24;

	/* This is going to act as our gxact */
	allTmGxact[procArray->pgprocnos[0]].gxid = 20;
	allTmGxact[procArray->pgprocnos[0]].xminDistributedSnapshot = InvalidDistributedTransactionId;

	procArray->numProcs = 1;

	MyTmGxact = &allTmGxact[procArray->pgprocnos[0]];

	/********************************************************
	 * Basic case, no other in progress transaction in system
	 */
	memset(ds.inProgressXidArray, 0, SIZE_OF_IN_PROGRESS_ARRAY);
	CreateDistributedSnapshot(&ds);

	/* perform all the validations */
	assert_true(ds.xminAllDistributedSnapshots == 20);
	assert_true(ds.xmin == 20);
	assert_true(ds.xmax == 25);
	assert_true(ds.count == 0);
	assert_true(MyTmGxact->xminDistributedSnapshot == 20);

	/*************************************************************************
	 * Case where there exist in-progress having taken snpashot with lower
	 * gxid than ours. This case demonstrates when xmin of snapshot will
	 * differ from xminAllDistributedSnapshots. Also, validates xmin and xmax
	 * get adjusted correctly based on in-progress.
	 */
	allTmGxact[procArray->pgprocnos[0]].xminDistributedSnapshot = InvalidDistributedTransactionId;

	allTmGxact[procArray->pgprocnos[1]].gxid = 10;
	allTmGxact[procArray->pgprocnos[1]].xminDistributedSnapshot = 5;

	allTmGxact[procArray->pgprocnos[2]].gxid = 30;
	allTmGxact[procArray->pgprocnos[2]].xminDistributedSnapshot = 20;

	procArray->numProcs = 3;

	memset(ds.inProgressXidArray, 0, SIZE_OF_IN_PROGRESS_ARRAY);
	CreateDistributedSnapshot(&ds);

	/* perform all the validations */
	assert_true(ds.xminAllDistributedSnapshots == 5);
	assert_true(ds.xmin == 10);
	/* xmax should be latestCompletedGxid+1 */
	assert_true(ds.xmax == 25);
	assert_true(ds.count == 1);
	assert_true(ds.inProgressXidArray[0] == 10);
	assert_true(MyTmGxact->xminDistributedSnapshot == 10);

	/*************************************************************************
	 * Add more elemnets, just to have validation that in-progress array is in
	 * ascending sorted order with distributed transactions.
	 */
	allTmGxact[procArray->pgprocnos[0]].xminDistributedSnapshot = InvalidDistributedTransactionId;

	allTmGxact[procArray->pgprocnos[3]].gxid = 15;
	allTmGxact[procArray->pgprocnos[3]].xminDistributedSnapshot = 12;

	allTmGxact[procArray->pgprocnos[4]].gxid = 7;
	allTmGxact[procArray->pgprocnos[4]].xminDistributedSnapshot = 7;

	procArray->numProcs = 5;

	memset(ds.inProgressXidArray, 0, SIZE_OF_IN_PROGRESS_ARRAY);
	CreateDistributedSnapshot(&ds);
	if (ds.count > 1)
		qsort(ds.inProgressXidArray, ds.count,
				sizeof(DistributedTransactionId), DistributedSnapshotMappedEntry_Compare);

	/* perform all the validations */
	assert_true(ds.xminAllDistributedSnapshots == 5);
	assert_true(ds.xmin == 7);
	assert_true(ds.xmax == 25);
	assert_true(ds.count == 3);
	assert_true(MyTmGxact->xminDistributedSnapshot == 7);
	assert_true(ds.inProgressXidArray[0] == 7);
	assert_true(ds.inProgressXidArray[1] == 10);
	assert_true(ds.inProgressXidArray[2] == 15);

	free(ds.inProgressXidArray);
	free(allTmGxact);
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
