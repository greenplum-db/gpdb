#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

/* Fetch definition of PG_exception_stack */
#include "postgres.h"

/*
 * Must be defined before including the module under test (xact.c) so
 * that elog_finish() calls within xact.c code will be replaced with
 * elog_finish_impl().
 */
#define elog_finish elog_finish_impl
int
elog_finish_impl(int level __attribute__((unused)),
				 int dummy __attribute__((unused)),
				 ...)
{
	if (level >= ERROR)
		siglongjmp(*PG_exception_stack, 1);
	return 0;
}

#include "../xact.c"

void helper_elog(LOG_LEVEL)
{
	expect_any(elog_start, filename);
	expect_any(elog_start, lineno);
	expect_any(elog_start, funcname);
	will_be_called(elog_start);
}

void
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
	TransactionState s;
	int i;
	ListCell   *cell;
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

void helper_ExpectLWLock(LWLockId slotLock) {
	expect_value(LWLockAcquire, lockid, slotLock);
	expect_value(LWLockAcquire, mode, LW_SHARED);
	will_be_called(LWLockAcquire);
	expect_value(LWLockRelease, lockid, slotLock);
	will_be_called(LWLockRelease);
}

void
test_IsCurrentTransactionIdForReader(void **state)
{
	PGPROC testProc = {0};

	SharedSnapshotSlot testSlot = {0};
	SharedLocalSnapshotSlot = &testSlot;
	/* lwlock is mocked, so assign any integer ID to slotLock. */
	testSlot.slotLock = 0;

	/* test: writer_proc is null */
	SharedLocalSnapshotSlot->writer_proc = NULL;
	helper_ExpectLWLock(testSlot.slotLock);
	helper_elog(ERROR);
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
	testProc.pid = 0;
	helper_ExpectLWLock(testSlot.slotLock);
	helper_elog(ERROR);
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
	testProc.xid = 0;

	helper_ExpectLWLock(testSlot.slotLock);
	assert_false(IsCurrentTransactionIdForReader(100));

	/*
	 * test: not a subtransaction - xid matches writer's top
	 * transaction ID
	 */
	testProc.xid = 100;

	helper_ExpectLWLock(testSlot.slotLock);
	helper_elog(DEBUG5);
	assert_true(IsCurrentTransactionIdForReader(100));

	/* test: subtransaction found in writer_proc cache */
	testProc.xid = 90;
	testProc.subxids.nxids = 2;
	testProc.subxids.xids[0] = 100;
	testProc.subxids.xids[1] = 110;

	helper_ExpectLWLock(testSlot.slotLock);
	assert_true(IsCurrentTransactionIdForReader(100));

	/* test: no subtransaction found in writer_proc cache */
	helper_ExpectLWLock(testSlot.slotLock);
	assert_false(IsCurrentTransactionIdForReader(120));

	/* test: overflow, with top xid matching writer's xid */
	testProc.xid = 90;
	testProc.subxids.nxids = 0;
	testProc.subxids.overflowed = true;

	helper_ExpectLWLock(testSlot.slotLock);

	expect_value(SubTransGetTopmostTransaction, xid, 100);
	will_return(SubTransGetTopmostTransaction, 90);

	assert_true(IsCurrentTransactionIdForReader(100));

	/* test: overflow, with top xid not matching writer's xid */
	testProc.xid = 80;
	testProc.subxids.nxids = 0;
	testProc.subxids.overflowed = true;

	helper_ExpectLWLock(testSlot.slotLock);

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
