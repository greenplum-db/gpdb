#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../aset.c"

#define NEW_ALLOC_SIZE 1024

extern MemoryAccount* MemoryAccountMemoryAccount;
extern MemoryAccount* RolloverMemoryAccount;
extern MemoryAccount* AlienExecutorMemoryAccount;

extern MemoryAccountIdType liveAccountStartId;
extern MemoryAccountIdType nextAccountId;

#undef PG_RE_THROW
#define PG_RE_THROW() siglongjmp(*PG_exception_stack, 1)

/*
 * This method will emulate the real ExceptionalCondition
 * function by re-throwing the exception, essentially falling
 * back to the next available PG_CATCH();
 */
static void
_ExceptionalCondition()
{
     PG_RE_THROW();
}

/*
 * This method sets up MemoryContext tree as well as
 * the basic MemoryAccount data structures.
 */
static void
SetupMemoryDataStructures(void **state)
{
	MemoryContextInit();
}

/*
 * This method cleans up MemoryContext tree and
 * the MemoryAccount data structures.
 */
static void
TeardownMemoryDataStructures(void **state)
{
	/*
	 * Ensure that no existing allocation refers to any short-living accounts. All
	 * short living accounts live in MemoryAccountMemoryAccount which is soon going
	 * to be reset via TopMemoryContext reset.
	 */
	MemoryAccounting_Reset();
	MemoryAccounting_SwitchAccount(MEMORY_OWNER_TYPE_Rollover);

	MemoryContextReset(TopMemoryContext); /* TopMemoryContext deletion is not supported */

	/* These are needed to be NULL for calling MemoryContextInit() */
	TopMemoryContext = NULL;
	CurrentMemoryContext = NULL;

	/*
	 * Memory accounts related variables need to be NULL before we
	 * try to setup memory account data structure again during the
	 * execution of the next test.
	 */
	MemoryAccountMemoryAccount = NULL;
	RolloverMemoryAccount = NULL;
	SharedChunkHeadersMemoryAccount = NULL;
	AlienExecutorMemoryAccount = NULL;
	MemoryAccountMemoryContext = NULL;

	ActiveMemoryAccountId = MEMORY_OWNER_TYPE_Undefined;

	for (int longLivingIdx = MEMORY_OWNER_TYPE_LogicalRoot; longLivingIdx <= MEMORY_OWNER_TYPE_END_LONG_LIVING; longLivingIdx++)
	{
		longLivingMemoryAccountArray[longLivingIdx] = NULL;
	}

	shortLivingMemoryAccountArray = NULL;

	liveAccountStartId = MEMORY_OWNER_TYPE_START_SHORT_LIVING;
	nextAccountId = MEMORY_OWNER_TYPE_START_SHORT_LIVING;
}


/*
 * Any change of global outstanding allocation balance should come
 * from the combination of active memory account balance increase
 * and any shared header allocation (SharedChunkHeadersMemoryAccount
 * balance increase)
 */
static void
test__MemoryAccounting_Allocate__ChargesOnlyActiveAccount(void **state)
{
	MemoryAccountIdType newActiveAccountId = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Exec_Hash);

	/* Make sure we have a new active account other than Rollover */
	MemoryAccounting_SwitchAccount(newActiveAccountId);

	assert_true(ActiveMemoryAccountId == newActiveAccountId);

	uint64 prevOutstanding = MemoryAccountingOutstandingBalance;

	uint64 prevSharedHeaderAlloc = SharedChunkHeadersMemoryAccount->allocated;

	void *testAlloc = palloc(NEW_ALLOC_SIZE);

	MemoryAccount* newActiveAccount = MemoryAccounting_ConvertIdToAccount(newActiveAccountId);
	/*
	 * Any change of outstanding balance is coming from new allocation
	 * and the associated shared header allocation
	 */
	assert_true(MemoryAccountingOutstandingBalance - prevOutstanding > 0);
	assert_true((newActiveAccount->allocated - newActiveAccount->freed) +
			(SharedChunkHeadersMemoryAccount->allocated - prevSharedHeaderAlloc) ==
					(MemoryAccountingOutstandingBalance - prevOutstanding));

	/*
	 * We need to pfree as we have allocated this memory
	 * in the TopMemoryContext with a short-living memory account
	 */
	pfree(testAlloc);
}

/*
 * Tests whether we adjust the outstanding balance during new
 * allocation
 */
static void
test__MemoryAccounting_Allocate__AdjustsOutstanding(void **state)
{
	uint64 prevOutstanding = MemoryAccountingOutstandingBalance;

	void *testAlloc = palloc(NEW_ALLOC_SIZE);

	/* Outstanding balance should at least increase by NEW_ALLOC_SIZE */
	assert_true((MemoryAccountingOutstandingBalance - prevOutstanding) >= NEW_ALLOC_SIZE);

	/*
	 * We need to pfree as we have allocated this memory
	 * in the TopMemoryContext with a short-living memory account
	 */
	pfree(testAlloc);
}

/* Tests whether we adjust the peak balance during new allocation */
static void
test__MemoryAccounting_Allocate__AdjustsPeak(void **state)
{
	uint64 prevPeak = MemoryAccountingPeakBalance;
	uint64 prevOutstanding = MemoryAccountingOutstandingBalance;

	/* We need at least newAllocSize to get a new peak */
	uint64 newAllocSize = prevPeak - prevOutstanding + 1;

	void *testAlloc = palloc(newAllocSize);

	/* We should have a new peak */
	assert_true(MemoryAccountingPeakBalance > prevPeak);

	/*
	 * We need to pfree as we have allocated this memory
	 * in the TopMemoryContext with a short-living memory account
	 */
	pfree(testAlloc);
}

/*
 * Tests whether we free old generation memory from rollover account,
 * irrespective of their owner account
 */
static void
test__MemoryAccounting_Free__FreesOldGenFromRollover(void **state)
{
	assert_true(ActiveMemoryAccountId == MEMORY_OWNER_TYPE_Top);

	MemoryAccount* activeAccount = MemoryAccounting_ConvertIdToAccount(ActiveMemoryAccountId);
	uint64 activeBalance = activeAccount->allocated - activeAccount->freed;
	uint64 oldRolloverBalance = RolloverMemoryAccount->allocated - RolloverMemoryAccount->freed;

	void *testAlloc = palloc(NEW_ALLOC_SIZE);

	assert_true(activeBalance < (activeAccount->allocated - activeAccount->freed));

	MemoryAccounting_Reset();

	uint64 rolloverBalance = RolloverMemoryAccount->allocated - RolloverMemoryAccount->freed;

	/* Rollover should assume the ownership of prev-generation allocations */
	assert_true(rolloverBalance > 0 && rolloverBalance > oldRolloverBalance);

	pfree(testAlloc);

	/*
	 * After we freed our previous allocation, the rollover balance
	 * should be reduced by at least NEW_ALLOC_SIZE amount
	 */
	assert_true(RolloverMemoryAccount->allocated - RolloverMemoryAccount->freed <=
			(rolloverBalance - NEW_ALLOC_SIZE));
}

/* Tests whether we adjust outstanding balance during freeing of memory */
static void
test__MemoryAccounting_Free__AdjustsOutstanding(void **state)
{
	uint64 oldOutstandingBalance = MemoryAccountingOutstandingBalance;

	void *testAlloc = palloc(NEW_ALLOC_SIZE);

	/* Outstanding balance should be increased by at least the allocation size */
	assert_true(MemoryAccountingOutstandingBalance >= (oldOutstandingBalance + NEW_ALLOC_SIZE));

	pfree(testAlloc);

	/* Outstanding balance should be back to original */
	assert_true(MemoryAccountingOutstandingBalance ==
			oldOutstandingBalance);
}

/* Tests whether the peak balance is unchanged during free operation */
static void
test__MemoryAccounting_Free__KeepsPeakUnchanged(void **state)
{
	uint64 prevPeak = MemoryAccountingPeakBalance;
	uint64 prevOutstanding = MemoryAccountingOutstandingBalance;

	/*
	 * We want a new peak, which requires an allocation of at
	 * least newAllocSize
	 */
	uint64 newAllocSize = prevPeak - prevOutstanding + 1;

	void *testAlloc = palloc(newAllocSize);

	assert_true(MemoryAccountingPeakBalance > prevPeak);

	uint64 newPeak = MemoryAccountingPeakBalance;

	pfree(testAlloc);

	/* Peak should not be altered by MemoryAccounting_Free() */
	assert_true(MemoryAccountingPeakBalance == newPeak);
}

/* Tests whether the header sharing is working */
static void
test__AllocAllocInfo__SharesHeader(void **state)
{
	void *testAlloc1 = palloc(NEW_ALLOC_SIZE);

	void *testAlloc2 = palloc(NEW_ALLOC_SIZE);

	StandardChunkHeader *header1 = (StandardChunkHeader *)
		((char *) testAlloc1 - STANDARDCHUNKHEADERSIZE);

	StandardChunkHeader *header2 = (StandardChunkHeader *)
		((char *) testAlloc2 - STANDARDCHUNKHEADERSIZE);

	/* Both should share the same sharedHeader */
	assert_true(header1->sharedHeader == header2->sharedHeader);

	pfree(testAlloc1);
	pfree(testAlloc2);
}

/*
 * Tests whether we charge shared chunk header account during an allocation
 * when header sharing is not possible
 */
static void
test__AllocAllocInfo__ChargesSharedChunkHeadersMemoryAccount(void **state)
{
	MemoryAccountIdType newActiveAccountId = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Exec_Hash);

	/* Make sure we have a new active account to force a new shared header allocation */
	MemoryAccounting_SwitchAccount(newActiveAccountId);

	uint64 prevSharedBalance = SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed;

	void *testAlloc = palloc(NEW_ALLOC_SIZE);

	uint64 newSharedBalance = SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed;

	/*
	 * A new sharedHeader should record the associated memory overhead
	 * in the SharedChunkHeadersMemoryAccount
	 */
	assert_true(prevSharedBalance < newSharedBalance);

	pfree(testAlloc);
}

/* Tests whether we use null account header if there is ActiveMemoryAccountId is invalid */
static void
test__AllocAllocInfo__UsesNullAccountHeader(void **state)
{
	MemoryContext newContext = AllocSetContextCreate(ErrorContext,
													 "TestContext",
													 ALLOCSET_DEFAULT_MINSIZE,
													 ALLOCSET_DEFAULT_INITSIZE,
													 ALLOCSET_DEFAULT_MAXSIZE);

	AllocSet newSet = (AllocSet)newContext;
	assert_true(newSet->sharedHeaderList == NULL);
	assert_true(newSet->nullAccountHeader == NULL);

	/* Turning off memory monitoring */
	ActiveMemoryAccountId = MEMORY_OWNER_TYPE_Undefined;

	/* Also make SharedChunkHeadersMemoryAccount undefined to avoid assert failure */
	SharedChunkHeadersMemoryAccount = NULL;

	/* Allocate from the new context which should trigger a nullAccountHeader creation*/
	void *testAlloc = MemoryContextAlloc(newContext, NEW_ALLOC_SIZE);

	StandardChunkHeader *header = (StandardChunkHeader *)
		((char *) testAlloc - STANDARDCHUNKHEADERSIZE);

	/* The new chunk should use the nullAccountHeader of the Aset */
	assert_true(header->sharedHeader != NULL && header->sharedHeader == newSet->nullAccountHeader);

	pfree(testAlloc);

	MemoryContextDelete(newContext);

	SharedChunkHeadersMemoryAccount = MemoryAccounting_ConvertIdToAccount(MEMORY_OWNER_TYPE_SharedChunkHeader);
}

/*
 * Tests whether header sharing works even if the desired header
 * is not at the head of sharedHeaderList
 */
static void
test__AllocAllocInfo__LooksAheadInSharedHeaderList(void **state)
{
	AllocSet set = (AllocSet)CurrentMemoryContext;

	/*
	 * This should create or reuse one of the sharedHeader
	 * in the current context's sharedHeaderList
	 */
	void *testAlloc1 = palloc(NEW_ALLOC_SIZE);

	MemoryAccountIdType newActiveAccount1 = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Exec_Hash);
	/* Make sure we have a new active account to force a new shared header allocation */
	MemoryAccountIdType oldActiveAccount = MemoryAccounting_SwitchAccount(newActiveAccount1);

	/* This will trigger a new sharedHeader creation because of the new ActiveMemoryAccountId */
	void *testAlloc2 = palloc(NEW_ALLOC_SIZE);

	StandardChunkHeader *header1 = (StandardChunkHeader *)
		((char *) testAlloc1 - STANDARDCHUNKHEADERSIZE);

	StandardChunkHeader *header2 = (StandardChunkHeader *)
		((char *) testAlloc2 - STANDARDCHUNKHEADERSIZE);

	/* The old shared header should not be at the head of sharedHeaderList */
	assert_true(set->sharedHeaderList != header1->sharedHeader);
	assert_true(header1->sharedHeader != header2->sharedHeader);

	/*
	 * This should restore old active account, so any further allocation
	 * should reuse header1, which is now *not* at the head of sharedHeaderList
	 */
	MemoryAccounting_SwitchAccount(oldActiveAccount);

	void *testAlloc3 = palloc(NEW_ALLOC_SIZE);
	StandardChunkHeader *header3 = (StandardChunkHeader *)
		((char *) testAlloc3 - STANDARDCHUNKHEADERSIZE);

	/* As we switched to previous account, we should be able to share the header */
	assert_true(header3->sharedHeader == header1->sharedHeader);

	/* Now we are triggering a third sharedHeader creation */
	MemoryAccountIdType newActiveAccount2 = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Exec_Hash);
	/* Make sure we have a new active account to force a new shared header allocation */
	oldActiveAccount = MemoryAccounting_SwitchAccount(newActiveAccount2);

	void *testAlloc4 = palloc(NEW_ALLOC_SIZE);
	StandardChunkHeader *header4 = (StandardChunkHeader *)
		((char *) testAlloc4 - STANDARDCHUNKHEADERSIZE);

	/* Make sure header4 got a new sharedHeader as we switched to a *new* memory account */
	assert_true(header4->sharedHeader != header1->sharedHeader &&
			header4->sharedHeader != header2->sharedHeader && header4->sharedHeader != header1->sharedHeader);

	/* And the latest sharedHeader should be at the head of sharedHeaderList */
	assert_true(header4->sharedHeader == set->sharedHeaderList);

	/*
	 * Now restore the original account, making the very original
	 * sharedHeader eligible to be reused (a 3 level reuse lookahead)
	 */
	MemoryAccounting_SwitchAccount(oldActiveAccount);

	void *testAlloc5 = palloc(NEW_ALLOC_SIZE);
	StandardChunkHeader *header5 = (StandardChunkHeader *)
		((char *) testAlloc5 - STANDARDCHUNKHEADERSIZE);

	/*
	 * Make sure we were able to dig up the original sharedHeader
	 * by digging through the sharedHeaderList
	 */
	assert_true(header5->sharedHeader == header1->sharedHeader);

	pfree(testAlloc1);
	pfree(testAlloc2);
	pfree(testAlloc3);
	pfree(testAlloc4);
	pfree(testAlloc5);
}

/* Tests whether we ignore sharedHeader if it is not from the current generation */
static void
test__AllocAllocInfo__IgnoresOldGenSharedHeaders(void **state)
{
	void *testAlloc1 = palloc(NEW_ALLOC_SIZE);
	StandardChunkHeader *header1 = (StandardChunkHeader *)
		((char *) testAlloc1 - STANDARDCHUNKHEADERSIZE);

	MemoryAccounting_Reset();

	/* Should not reuse sharedHeader as the generation has changed */
	void *testAlloc2 = palloc(NEW_ALLOC_SIZE);
	StandardChunkHeader *header2 = (StandardChunkHeader *)
		((char *) testAlloc2 - STANDARDCHUNKHEADERSIZE);

	/*
	 * No header sharing as the generation has changed during
	 * the call of MemoryAccounting_Reset()
	 */
	assert_true(header1->sharedHeader != header2->sharedHeader);

	pfree(testAlloc1);
	pfree(testAlloc2);
}

/*
 * Tests whether we correctly allocate a new shared header and insert it
 * into the sharedHeaderList
 */
static void
test__AllocAllocInfo__InsertsIntoSharedHeaderList(void **state)
{
	AllocSet set = (AllocSet)CurrentMemoryContext;

	/*
	 * This should create or reuse one of the sharedHeader
	 * in the current context's sharedHeaderList
	 */
	void *testAlloc1 = palloc(NEW_ALLOC_SIZE);

	MemoryAccountIdType newActiveAccount1 = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Exec_Hash);
	/* Make sure we have a new active account to force a new shared header allocation */
	MemoryAccounting_SwitchAccount(newActiveAccount1);

	/* This will trigger a new sharedHeader creation because of the new ActiveMemoryAccountId */
	void *testAlloc2 = palloc(NEW_ALLOC_SIZE);

	StandardChunkHeader *header1 = (StandardChunkHeader *)
		((char *) testAlloc1 - STANDARDCHUNKHEADERSIZE);

	StandardChunkHeader *header2 = (StandardChunkHeader *)
		((char *) testAlloc2 - STANDARDCHUNKHEADERSIZE);

	/* Now we are triggering a third sharedHeader creation */
	MemoryAccountIdType newActiveAccount2 = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Exec_Hash);
	/* Make sure we have a new active account to force a new shared header allocation */
	MemoryAccounting_SwitchAccount(newActiveAccount2);

	void *testAlloc3 = palloc(NEW_ALLOC_SIZE);
	StandardChunkHeader *header3 = (StandardChunkHeader *)
		((char *) testAlloc3 - STANDARDCHUNKHEADERSIZE);

	/* Verify that the sharedHeaderList linked list looks good */
	assert_true(set->sharedHeaderList == header3->sharedHeader &&
			set->sharedHeaderList->next == header2->sharedHeader &&
			set->sharedHeaderList->next->next == header1->sharedHeader &&
			set->sharedHeaderList->next->next->next == NULL);

	/*
	 * Create one more account to force creation of another sharedHeader, which
	 * should replace the earliest sharedHeader in the sharedHeaderList. We only
	 * look ahead up to depth 3, but we maintain all the sharedHeaders in the
	 * sharedHeaderList
	 */
	MemoryAccountIdType newActiveAccount3 = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Exec_Hash);
	MemoryAccounting_SwitchAccount(newActiveAccount3);

	void *testAlloc4 = palloc(NEW_ALLOC_SIZE);
	StandardChunkHeader *header4 = (StandardChunkHeader *)
		((char *) testAlloc4 - STANDARDCHUNKHEADERSIZE);

	/*
	 * Verify that the sharedHeaderList linked list can go beyond
	 * 3 levels of lookahead by testing linked list up to depth 4
	 */
	assert_true(set->sharedHeaderList == header4->sharedHeader &&
			set->sharedHeaderList->next == header3->sharedHeader &&
			set->sharedHeaderList->next->next == header2->sharedHeader &&
			set->sharedHeaderList->next->next->next == header1->sharedHeader &&
			set->sharedHeaderList->next->next->next->next == NULL);

	pfree(testAlloc1);
	pfree(testAlloc2);
	pfree(testAlloc3);
	pfree(testAlloc4);
}

/* Tests whether SharedChunkHeadersMemoryAccount ignores the overhead of null Account header */
static void
test__AllocFreeInfo__SharedChunkHeadersMemoryAccountIgnoresNullHeader(void **state)
{
	/* This has two parts:
	 * 1. Check the assertion that nullAccountHeader creation requires
	 * SharedChunkHeadersMemoryAccount to be NULL
	 *
	 * 2. Reset the context that hosts the nullAccountHeader, and make sure
	 * that SharedChunkHeadersMemoryAccount balance does not get changed
	 */

	MemoryContext newContext = AllocSetContextCreate(ErrorContext,
													 "TestContext",
													 ALLOCSET_DEFAULT_MINSIZE,
													 ALLOCSET_DEFAULT_INITSIZE,
													 ALLOCSET_DEFAULT_MAXSIZE);

	AllocSet newSet = (AllocSet)newContext;
	assert_true(newSet->sharedHeaderList == NULL);
	assert_true(newSet->nullAccountHeader == NULL);

	MemoryAccountIdType oldActive = ActiveMemoryAccountId;
	MemoryAccount *oldShared = SharedChunkHeadersMemoryAccount;

	/* Turning off memory monitoring */
	ActiveMemoryAccountId = MEMORY_OWNER_TYPE_Undefined;

	#ifdef USE_ASSERT_CHECKING
	    expect_any(ExceptionalCondition,conditionName);
	    expect_any(ExceptionalCondition,errorType);
	    expect_any(ExceptionalCondition,fileName);
	    expect_any(ExceptionalCondition,lineNumber);

	    will_be_called_with_sideeffect(ExceptionalCondition, &_ExceptionalCondition, NULL);

	    /* Test if within memory-limit strings cause assertion failure */
		PG_TRY();
		{
			/*
			 * ActiveMemoryAccountId is invalid, but SharedChunkHeadersMemoryAccount is
			 * *not* null. This should trigger an assertion
			 */
			MemoryContextAlloc(newContext, NEW_ALLOC_SIZE);

			assert_true(false);
		}
		PG_CATCH();
		{
		}
		PG_END_TRY();
	#endif

	/*
	 * Now make SharedChunkHeadersMemoryAccount NULL to avoid assert failure
	 * and allow creation of a nullAccountHeader
	 */
	SharedChunkHeadersMemoryAccount = NULL;

	/* Allocate from the new context which should trigger a nullAccountHeader creation*/
	void *testAlloc = MemoryContextAlloc(newContext, NEW_ALLOC_SIZE);

	StandardChunkHeader *header = (StandardChunkHeader *)
		((char *) testAlloc - STANDARDCHUNKHEADERSIZE);

	/*
	 * Assert if we are using nullAccountHeader, as we have simulated absence
	 * of memory monitoring
	 */
	assert_true(header->sharedHeader != NULL && header->sharedHeader == newSet->nullAccountHeader);

	/*
	 * Restore the SharedChunkHeadersMemoryAccount so that we can turn on
	 * sharedHeader balance releasing if a sharedHeader is deleted (except
	 * nullAccountHeader of course)
	 */
	SharedChunkHeadersMemoryAccount = oldShared;
	/* Activate the memory accounting */
	ActiveMemoryAccountId = oldActive;

	uint64 sharedBalance = SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed;

	pfree(testAlloc);

	/*
	 * As testAlloc is holding a pointer to nullAccountHeader, releasing that allocation
	 * should not change SharedChunkHeadersMemoryAccount balance
	 */
	assert_true(sharedBalance == SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed);

	MemoryContextDelete(newContext);
}

/* Tests whether null Account header never gets freed */
static void
test__AllocFreeInfo__ReusesNullHeader(void **state)
{
	MemoryContext newContext = AllocSetContextCreate(ErrorContext,
													 "TestContext",
													 ALLOCSET_DEFAULT_MINSIZE,
													 ALLOCSET_DEFAULT_INITSIZE,
													 ALLOCSET_DEFAULT_MAXSIZE);

	AllocSet newSet = (AllocSet)newContext;
	assert_true(newSet->sharedHeaderList == NULL);
	assert_true(newSet->nullAccountHeader == NULL);

	MemoryAccountIdType oldActive = ActiveMemoryAccountId;
	MemoryAccount* oldShared = SharedChunkHeadersMemoryAccount;

	/* Turning off memory monitoring */
	ActiveMemoryAccountId = MEMORY_OWNER_TYPE_Undefined;
	/* Also make SharedChunkHeadersMemoryAccount NULL to avoid assert failure */
	SharedChunkHeadersMemoryAccount = NULL;

	/* Allocate from the new context which should trigger a nullAccountHeader creation*/
	void *testAlloc1 = MemoryContextAlloc(newContext, NEW_ALLOC_SIZE);
	StandardChunkHeader *header1 = (StandardChunkHeader *)
		((char *) testAlloc1 - STANDARDCHUNKHEADERSIZE);

	/* Allocate another, and check if the nullAccountHeader is reused */
	void *testAlloc2 = MemoryContextAlloc(newContext, NEW_ALLOC_SIZE);
	StandardChunkHeader *header2 = (StandardChunkHeader *)
		((char *) testAlloc2 - STANDARDCHUNKHEADERSIZE);

	/*
	 * Make sure that the nullAccountHeader doesn't get reallocated
	 * every time and a singleton nullAccountHeader is shared between
	 * any chunks that are allocated before memory accounting is enabled
	 */
	assert_true(header1->sharedHeader != NULL && header1->sharedHeader == newSet->nullAccountHeader &&
			header1->sharedHeader == header2->sharedHeader);

	ActiveMemoryAccountId = oldActive;
	SharedChunkHeadersMemoryAccount = oldShared;

	pfree(testAlloc1);
	pfree(testAlloc2);

	MemoryContextDelete(newContext);
}

/* Tests whether we are promptly freeing shared header that is no longer shared */
static void
test__AllocFreeInfo__FreesObsoleteHeader(void **state)
{
	MemoryAccountIdType newActiveAccount = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Exec_Hash);

	/* Make sure we have a new active account to force a new shared header allocation */
	MemoryAccounting_SwitchAccount(newActiveAccount);

	uint64 prevSharedBalance = SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed;

	void *testAlloc = palloc(NEW_ALLOC_SIZE);

	uint64 newSharedBalance = SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed;

	assert_true(prevSharedBalance < newSharedBalance);

	StandardChunkHeader *header = (StandardChunkHeader *)
		((char *) testAlloc - STANDARDCHUNKHEADERSIZE);

	AllocSet set = (AllocSet)CurrentMemoryContext;

	SharedChunkHeader *headerPointer = header->sharedHeader;
	assert_true(set->sharedHeaderList == headerPointer);

	/*
	 * As no one else is using the sharedHeader of testAlloc,
	 * we should release upon pfree of testAlloc
	 */
	pfree(testAlloc);

	/*
	 * The sharedHeaderList should no longer store the sharedHeader
	 * that we have just released. Note: the headerPointer is now
	 * and invalid pointer as we have already freed the shared header
	 * memory
	 */
	assert_true(set->sharedHeaderList != headerPointer);
}

/* Tests whether a free operation decreases balance of only the owning account */
static void
test__AllocFreeInfo__FreesOnlyOwnerAccount(void **state)
{
	MemoryAccountIdType newAccountId = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Exec_Hash);
	MemoryAccountIdType oldActiveAccountId = MemoryAccounting_SwitchAccount(newAccountId);

	/* This chunk should record newAccount as the owner */
	void *testAlloc = palloc(NEW_ALLOC_SIZE);

	MemoryAccounting_SwitchAccount(oldActiveAccountId);

	assert_true(ActiveMemoryAccountId != newAccountId);

	MemoryAccount* activeAccount = MemoryAccounting_ConvertIdToAccount(ActiveMemoryAccountId);
	MemoryAccount* newAccount = MemoryAccounting_ConvertIdToAccount(newAccountId);
	uint64 originalActiveBalance = activeAccount->allocated - activeAccount->freed;
	uint64 newAccountBalance = newAccount->allocated - newAccount->freed;
	uint64 newAccountFreed = newAccount->freed;
	uint64 sharedFreed = SharedChunkHeadersMemoryAccount->freed;
	uint64 prevOutstanding = MemoryAccountingOutstandingBalance;

	pfree(testAlloc);

	/*
	 * Make sure that the active account is unchanged while the owner account
	 * balance was reduced
	 */
	assert_true(activeAccount->allocated - activeAccount->freed == originalActiveBalance);
	/* Balance was released from newAccount */
	assert_true(newAccount->allocated - newAccount->freed <= newAccountBalance - NEW_ALLOC_SIZE);

	/* The shared header should be released */
	assert_true(SharedChunkHeadersMemoryAccount->freed - sharedFreed > 0);

	/* All the difference in outstanding balance should come from newAccount
	 * (which was the owner of the chunk) balance change and the resulting
	 * shared header release
	 */
	assert_true(prevOutstanding - MemoryAccountingOutstandingBalance ==
			newAccount->freed - newAccountFreed + SharedChunkHeadersMemoryAccount->freed - sharedFreed);
}

/* Tests whether a large allocation allocates dedicated block */
static void
test__AllocSetAllocImpl__LargeAllocInNewBlock(void **state)
{
	int chunkSize = ALLOC_CHUNK_LIMIT + 1;
	/* This chunk should record newAccount as the owner */
	void *testAlloc = palloc(chunkSize);

	AllocChunk	chunk = AllocPointerGetChunk(testAlloc);

	AllocBlock block = (AllocBlock)(((char*)chunk) - ALLOC_BLOCKHDRSZ);
	AllocSet set = (AllocSet)CurrentMemoryContext;

	/*
	 * Make sure that the block pointer is the same as the *second* block in the blocks
	 * list (we insert the new block after the head, to keep the head available for
	 * more chunk allocation, if it is not entirely used yet). Also, ensure that block's
	 * freeptr is set to endptr (i.e., block is exclusively owned by this chunk).
	 */
	assert_true(set->blocks != NULL && set->blocks->next != NULL && set->blocks->next == block && UserPtr_GetEndPtr(set->blocks->next) == set->blocks->next->freeptr);

	pfree(testAlloc);
}

/* Tests whether a large allocation updates memory accounting outstanding balance */
static void
test__AllocSetAllocImpl__LargeAllocInOutstandingBalance(void **state)
{
	uint64 prevOutstanding = MemoryAccountingOutstandingBalance;

	int chunkSize = ALLOC_CHUNK_LIMIT + 1;
	/* This chunk should record newAccount as the owner */
	void *testAlloc = palloc(chunkSize);

	/*
	 * Outstanding balance should increase by at least the chunkSize
	 */
	assert_true(prevOutstanding + chunkSize <= MemoryAccountingOutstandingBalance);

	pfree(testAlloc);
}

/* Tests whether a large allocation updates balance of the active memory account */
static void
test__AllocSetAllocImpl__LargeAllocInActiveMemoryAccount(void **state)
{
	MemoryAccountIdType newActiveAccountId = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Exec_Hash);

	/* Make sure we have a new active account other than Rollover */
	MemoryAccounting_SwitchAccount(newActiveAccountId);

	uint64 prevOutstanding = MemoryAccountingOutstandingBalance;

	uint64 prevSharedHeaderAlloc = SharedChunkHeadersMemoryAccount->allocated;

	uint64 prevActiveAlloc = MemoryAccounting_GetAccountCurrentBalance(ActiveMemoryAccountId);

	int chunkSize = ALLOC_CHUNK_LIMIT + 1;
	/* This chunk should record newAccount as the owner */
	void *testAlloc = palloc(chunkSize);

	MemoryAccount *newActiveAccount = MemoryAccounting_ConvertIdToAccount(newActiveAccountId);
	/*
	 * All the new allocation should go to active memory account, and the
	 * SharedChunkHeadersMemoryAccount should contribute to add up to
	 * the outstanding balance change
	 */
	assert_true((newActiveAccount->allocated >= prevActiveAlloc + chunkSize) &&
			(SharedChunkHeadersMemoryAccount->allocated > prevSharedHeaderAlloc) &&
			(newActiveAccount->allocated - newActiveAccount->freed) +
			(SharedChunkHeadersMemoryAccount->allocated - prevSharedHeaderAlloc) ==
					(MemoryAccountingOutstandingBalance - prevOutstanding));

	/*
	 * We need to pfree as we have allocated this memory
	 * in the TopMemoryContext with a short-living memory account
	 */
	pfree(testAlloc);
}

/*
 * Tests whether reallocating a small chunk into a large allocation
 * updates memory accounting outstanding balance
 */
static void
test__AllocSetRealloc__AdjustsOutstandingBalance(void **state)
{
	/* This chunk should record newAccount as the owner */
	void *testAlloc = palloc(NEW_ALLOC_SIZE);

	AllocChunk chunk = AllocPointerGetChunk(testAlloc);
	int prevSize = chunk->size;

	uint64 prevOutstanding = MemoryAccountingOutstandingBalance;

	/* Just double the allocation, which should still be under the ALLOC_CHUNK_LIMIT */
	testAlloc = repalloc(testAlloc, NEW_ALLOC_SIZE * 2);

	chunk = AllocPointerGetChunk(testAlloc);
	int newSize = chunk->size;

	uint64 balanceChange = newSize - prevSize;

	/*
	 * Outstanding balance should increase by the "balanceChange"
	 */
	assert_true(balanceChange > 0 && prevOutstanding + balanceChange == MemoryAccountingOutstandingBalance);

	prevOutstanding = MemoryAccountingOutstandingBalance;

	prevSize = newSize;

	int bigChunkSize = ALLOC_CHUNK_LIMIT + 1;
	testAlloc = repalloc(testAlloc, bigChunkSize);

	chunk = AllocPointerGetChunk(testAlloc);

	newSize = chunk->size;

	balanceChange = newSize - prevSize;

	/*
	 * Outstanding balance should increase by the "balanceChange"
	 */
	assert_true(balanceChange > 0 && prevOutstanding + balanceChange == MemoryAccountingOutstandingBalance);

	pfree(testAlloc);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test_setup_teardown(test__MemoryAccounting_Allocate__ChargesOnlyActiveAccount, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Allocate__AdjustsOutstanding, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Allocate__AdjustsPeak, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Free__FreesOldGenFromRollover, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Free__AdjustsOutstanding, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Free__KeepsPeakUnchanged, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__AllocAllocInfo__SharesHeader, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__AllocAllocInfo__ChargesSharedChunkHeadersMemoryAccount, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__AllocAllocInfo__UsesNullAccountHeader, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__AllocAllocInfo__LooksAheadInSharedHeaderList, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__AllocAllocInfo__IgnoresOldGenSharedHeaders, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__AllocAllocInfo__InsertsIntoSharedHeaderList, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__AllocFreeInfo__SharedChunkHeadersMemoryAccountIgnoresNullHeader, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__AllocFreeInfo__ReusesNullHeader, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__AllocFreeInfo__FreesObsoleteHeader, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__AllocFreeInfo__FreesOnlyOwnerAccount, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Allocate__ChargesOnlyActiveAccount, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__AllocSetAllocImpl__LargeAllocInNewBlock, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__AllocSetAllocImpl__LargeAllocInOutstandingBalance, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__AllocSetAllocImpl__LargeAllocInActiveMemoryAccount, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__AllocSetRealloc__AdjustsOutstandingBalance, SetupMemoryDataStructures, TeardownMemoryDataStructures),
	};

	return run_tests(tests);
}
