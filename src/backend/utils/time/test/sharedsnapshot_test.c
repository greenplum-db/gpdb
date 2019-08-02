#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "utils/memutils.h"
#include "utils/resowner.h"
#include "utils/faultinjector.h"
#include "utils/snapshot.h"
#include "storage/fd.h"

#include "../sharedsnapshot.c"

/*
 * Write shared snapshot to file using dumpSharedLocalSnapshot_forCursor()
 * first.  Then read the snapshot from file using
 * readSharedLocalSnapshot_forCursor().  Validate that the contents read from
 * the file match what was written.
 */
static void
test_write_read_shared_snapshot_for_cursor(void **state)
{
#define XCNT 5
	TransactionId xip[XCNT] = {100, 101, 103, 105, 109};
	xipEntryCount = XCNT;

	PGPROC writer_proc;

	TopTransactionResourceOwner = ResourceOwnerCreate(NULL, "unittest resource owner");
	CurrentResourceOwner = TopTransactionResourceOwner;
	TopTransactionContext = CurrentMemoryContext;

	/* create a dummy shared and local snapshot with 5 in-progress transactions */
	SharedSnapshotSlot slot;
	SharedLocalSnapshotSlot = &slot;
	slot.slotindex = 1;
	slot.slotid = 1;
	slot.pid = 1000;
	slot.writer_proc = &writer_proc;
	slot.writer_xact = NULL;
	slot.xid = 100;
	slot.cid = 1;
	slot.startTimestamp = 0;
	slot.QDxid = 10;
	slot.QDcid = 1;
	slot.ready = true;
	slot.segmateSync = 1;
	slot.combocidcnt = 0;
	slot.snapshot.xmin = 99;
	slot.snapshot.xmax = 110;
	slot.snapshot.xcnt = XCNT;
	slot.snapshot.xip = xip;
	slot.slotLock = NULL;

	/* assume the role of a writer to write the snapshot */
	Gp_role = GP_ROLE_EXECUTE;
	Gp_is_writer = true;

	expect_any(LWLockAcquire, l);
	expect_any(LWLockAcquire, mode);
	will_be_called(LWLockAcquire);

	expect_any_count(FaultInjector_InjectFaultIfSet, faultName, 13);
	expect_any_count(FaultInjector_InjectFaultIfSet, ddlStatement, 13);
	expect_any_count(FaultInjector_InjectFaultIfSet, databaseName, 13);
	expect_any_count(FaultInjector_InjectFaultIfSet, tableName, 13);
	will_be_called_count(FaultInjector_InjectFaultIfSet, 13);

	expect_any(LWLockRelease, l);
	will_be_called(LWLockRelease);

	MyProc = &writer_proc;
	MyProc->pid = 1000;

	/* write the snapshot to file */
	dumpSharedLocalSnapshot_forCursor();

	/* assume the role of a reader to read the snapshot */
	PGPROC reader_proc;
	MyProc = &reader_proc;
	MyProc->pid = 1234;
	lockHolderProcPtr = &writer_proc;
	Gp_is_writer = false;

	QEDtxContextInfo.segmateSync = slot.segmateSync;
	QEDtxContextInfo.distributedXid = slot.QDxid;
	QEDtxContextInfo.curcid = slot.QDcid;

	SnapshotData snapshot;
	snapshot.xip = palloc(XCNT * sizeof(TransactionId));

	/* read snapshot from the same file */
	readSharedLocalSnapshot_forCursor(&snapshot, DTX_CONTEXT_QE_READER);

	assert_true(snapshot.xcnt == XCNT);
	int i;
	for (i=0; i<XCNT; i++)
		assert_true(slot.snapshot.xip[i] == snapshot.xip[i]);
}

static void
test_boundaries_of_CreateSharedSnapshotArray(void **state)
{
	/*
	 * max_prepared_xacts is used to calculate NUM_SHARED_SNAPSHOT_SLOTS. Make
	 * it non-zero so that we actually allocate some local snapshot slots.
	 */
	max_prepared_xacts = 2;

	SharedSnapshotStruct *fakeSharedSnapshotArray = NULL;

	Size sharedSnapshotShmemSize = SharedSnapshotShmemSize();
	fakeSharedSnapshotArray = malloc(sharedSnapshotShmemSize);

	will_return(ShmemInitStruct, fakeSharedSnapshotArray);
	will_assign_value(ShmemInitStruct, foundPtr, false);
	expect_any_count(ShmemInitStruct, name, 1);
	expect_any_count(ShmemInitStruct, size, 1);
	expect_any_count(ShmemInitStruct, foundPtr, 1);

	/*
	 * Each slot in SharedSnapshotStrut has an associated dynamically allocated
	 * LWLock, so LWLockAssign should be called for each slot.
	 */
	will_be_called_count(LWLockAssign, NUM_SHARED_SNAPSHOT_SLOTS);

	CreateSharedSnapshotArray();

	for (int i=0; i<sharedSnapshotArray->maxSlots; i++)
	{
		SharedSnapshotSlot *s = &sharedSnapshotArray->slots[i];

		/*
		 * Assert that every slot xip array falls inside the boundaries of the
		 * allocated shared snapshot.
		 */
		assert_true((uint8_t *)s->snapshot.xip > (uint8_t *)fakeSharedSnapshotArray);
		assert_true((uint8_t *)s->snapshot.xip < (((uint8_t *)fakeSharedSnapshotArray) +
												sharedSnapshotShmemSize));
	}
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_boundaries_of_CreateSharedSnapshotArray),
		unit_test(test_write_read_shared_snapshot_for_cursor)
	};
	MemoryContextInit();
	InitFileAccess();
	return run_tests(tests);
}
