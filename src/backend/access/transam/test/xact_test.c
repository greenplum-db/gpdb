#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

/* Fetch definition of PG_exception_stack */
#include "postgres.h"

#undef TransactionIdDidAbortForReader
#define TransactionIdDidAbortForReader(xid) \
	mock_TransactionIdDidAbortForReader(xid)
/* Mock it so that XIDs > 100 are treated as aborted. */
static bool
mock_TransactionIdDidAbortForReader(TransactionId xid)
{
	return xid > 100;
}

#include "../xact.c"

static void
test_TransactionIdIsCurrentTransactionIdInternal(void **state)
{
	bool flag = false;
	TransactionId failure_xid1 = 255;
	TransactionId failure_xid2 = 567;
	TransactionId failure_xid3 = 5;
	TransactionId passed_xid = 299;
	TransactionId aborted_xid = 467;
	TransactionId child_xid1 = 100;
	TransactionId child_xid2 = 320;

	TransactionState p = CurrentTransactionState;
	TransactionState s = NULL;
	int i;
	int child_count = 1;

	for (i = 6; i< 500; i++)
	{
		/*
		 * Lets skip to add the failure_xid, so that it reports not found
		 */
		if (i == failure_xid1)
			continue;
	
		if ((i == (child_xid1+child_count)) || (i == (child_xid2+child_count)))
		{
			AtSubCommit_childXids();
			assert_true(s->parent->childXids != NULL);
#if 0
			/* Enable to see details */
			printf("\nMain=%d, parent=%d", s->transactionId, s->parent->transactionId);
			foreach(cell, s->parent->childXids)
			{
				printf(" child=%d, ", lfirst_xid(cell));
			}
#endif

			Assert(s != NULL);
			p = s->parent;
			CurrentTransactionState = p;
			pfree(s);

			child_count++;
			if (child_count == 11)
				child_count = 1;
		}

		s = (TransactionState)
				MemoryContextAllocZero(TopTransactionContext, sizeof(TransactionStateData));
		s->transactionId = i;
		s->parent = p;
		s->nestingLevel = p->nestingLevel + 1;

		/*
		 * Lets set state for aborted_xid
		 */
		if (i == aborted_xid)
		{
			s->state = TRANS_ABORT;
		}

		p = s;

		fastNodeCount++;
		if (fastNodeCount == NUM_NODES_TO_SKIP_FOR_FAST_SEARCH)
		{
			fastNodeCount = 0;
			s->fastLink = previousFastLink;
			previousFastLink = s;
		}

		CurrentTransactionState = p;
	}

#if 0
	/* Enable to see details */
	while (p)
	{
		if (p->fastLink == NULL)
		{
			printf("%d, ", p->transactionId);
		}
		else
		{
			printf("FAST Linked %d to node %d, ", p->transactionId, p->fastLink->transactionId);
		}

		foreach(cell, p->childXids)
		{
			printf(" child=%d, ", lfirst_xid(cell));
		}

		p = p->parent;
	}
#endif

	flag = TransactionIdIsCurrentTransactionIdInternal(failure_xid1);
	assert_int_equal(flag, 0);

	flag = TransactionIdIsCurrentTransactionIdInternal(failure_xid2);
	assert_int_equal(flag, 0);

	flag = TransactionIdIsCurrentTransactionIdInternal(failure_xid3);
	assert_int_equal(flag, 0);

	flag = TransactionIdIsCurrentTransactionIdInternal(passed_xid);
	assert_int_equal(flag, 1);

	flag = TransactionIdIsCurrentTransactionIdInternal(aborted_xid);
	assert_int_equal(flag, 0);

	flag = TransactionIdIsCurrentTransactionIdInternal(child_xid1+6);
	assert_int_equal(flag, 1);

	flag = TransactionIdIsCurrentTransactionIdInternal(child_xid2+3);
	assert_int_equal(flag, 1);

}

static void
helper_ExpectLWLock()
{
	expect_any(LWLockAcquire, l);
	expect_value(LWLockAcquire, mode, LW_SHARED);
	will_return(LWLockAcquire, true);
	expect_any(LWLockRelease, l);
	will_be_called(LWLockRelease);
}

static void
test_IsCurrentTransactionIdForReader(void **state)
{
	PGPROC testProc = {{0}};
	PGXACT testXAct = {0};
	LWLock localslotLock;

	SharedSnapshotSlot testSlot = {0};
	SharedLocalSnapshotSlot = &testSlot;
	/* lwlock is mocked, so assign any integer ID to slotLock. */
	testSlot.slotLock = &localslotLock;

	/* test: writer_proc is null */
	SharedLocalSnapshotSlot->writer_proc = NULL;
	helper_ExpectLWLock();
	PG_TRY();
	{
		IsCurrentTransactionIdForReader(100);
		assert_false("No elog(ERROR, ...) was called");
	}
	PG_CATCH();
	{
	}
	PG_END_TRY();

	/* test: writer_proc->pid is invalid */
	testSlot.writer_proc = &testProc;
	testSlot.writer_xact = &testXAct;
	testProc.pid = 0;
	helper_ExpectLWLock();
	PG_TRY();
	{
		IsCurrentTransactionIdForReader(100);
		assert_false("No elog(ERROR, ...) was called");
	}
	PG_CATCH();
	{
	}
	PG_END_TRY();

	/*
	 * test: writer_proc->xid is invalid, e.g. lazy xid not assigned
	 * xid yet.
	 */
	testProc.pid = 1234;
	testXAct.xid = 0;

	helper_ExpectLWLock();
	assert_false(IsCurrentTransactionIdForReader(100));

	/*
	 * test: not a subtransaction - xid matches writer's top
	 * transaction ID
	 */
	testXAct.xid = 100;

	helper_ExpectLWLock();
	assert_true(IsCurrentTransactionIdForReader(100));

	/* test: subtransaction found in writer_proc cache */
	testXAct.xid = 90;
	testXAct.nxids = 2;
	testProc.subxids.xids[0] = 100;
	testProc.subxids.xids[1] = 110;

	helper_ExpectLWLock();
	assert_true(IsCurrentTransactionIdForReader(100));

	/* test: no subtransaction found in writer_proc cache */
	helper_ExpectLWLock();
	assert_false(IsCurrentTransactionIdForReader(120));

	/* test: overflow, with top xid matching writer's xid */
	testXAct.xid = 90;
	testXAct.nxids = 0;
	testXAct.overflowed = true;

	helper_ExpectLWLock();

	expect_value(SubTransGetTopmostTransaction, xid, 100);
	will_return(SubTransGetTopmostTransaction, 90);

	assert_true(IsCurrentTransactionIdForReader(100));

	/* test: overflow, with top xid not matching writer's xid */
	testXAct.xid = 80;
	testXAct.nxids = 0;
	testXAct.overflowed = true;

	helper_ExpectLWLock();

	expect_value(SubTransGetTopmostTransaction, xid, 100);
	will_return(SubTransGetTopmostTransaction, 90);

	assert_false(IsCurrentTransactionIdForReader(100));
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_TransactionIdIsCurrentTransactionIdInternal),
		unit_test(test_IsCurrentTransactionIdForReader)
	};

	MemoryContextInit();
	TopTransactionContext =
	  AllocSetContextCreate(TopMemoryContext,
				"mock TopTransactionContext",
				ALLOCSET_DEFAULT_MINSIZE,
				ALLOCSET_DEFAULT_INITSIZE,
				ALLOCSET_DEFAULT_MAXSIZE);

	return run_tests(tests);
}
