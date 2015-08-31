#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../xact.c"

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

int 
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_TransactionIdIsCurrentTransactionIdInternal)
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
