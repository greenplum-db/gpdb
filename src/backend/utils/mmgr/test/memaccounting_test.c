#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "utils/gp_alloc.h"
#include "utils/gp_accounted_alloc.h"

/*
 * We assume the maximum output size from any memory accounting output
 * generation functions will be 2K bytes. That way we reserve that much
 * during initStringInfoOfSize so that we don't allocate any
 * additional memory (and as a side effect change the memory accounting
 * tree) during the output generation process.
 */
#define MAX_OUTPUT_BUFFER_SIZE 2048
#define NEW_ALLOC_SIZE 1024
#define ALLOC_CHUNKHDRSZ	MAXALIGN(sizeof(StandardChunkHeader))

#define AllocPointerGetChunk(ptr)	\
					((StandardChunkHeader *)(((char *)(ptr)) - ALLOC_CHUNKHDRSZ))

static StringInfoData outputBuffer;

static void write_stderr_mock(const char *fmt,...);

/* We will capture write_stderr output using write_stderr_mock */
#define write_stderr write_stderr_mock

/* We will capture fwrite output using fwrite_mock */
#undef fwrite
#define fwrite fwrite_mock

/*
 * We need to override fopen and fclose to ensure that we don't attempt to write
 * anything to the disk. This might fail on the pulse as pg_log directory is not
 * set and there might be a permission issue
 */
#undef fopen
/* Return arbitrary pointer so that we don't bypass fwrite completely */
#define fopen(fileName, mode) 0xabcdef;

#undef fclose
#define fclose(fileHandle)

void write_stderr_mock(const char *fmt,...);

#include "../memaccounting.c"

#define EXPECT_EXCEPTION()     \
	expect_any(ExceptionalCondition,conditionName); \
	expect_any(ExceptionalCondition,errorType); \
	expect_any(ExceptionalCondition,fileName); \
	expect_any(ExceptionalCondition,lineNumber); \
    will_be_called_with_sideeffect(ExceptionalCondition, &_ExceptionalCondition, NULL);\

/*
 * Mocks the function write_stderr and captures the output in
 * the global outputBuffer
 */
static void
write_stderr_mock(const char *fmt,...)
{
    va_list		ap;

    fmt = _(fmt);

    va_start(ap, fmt);

	char		buf[2048];

	vsnprintf(buf, sizeof(buf), fmt, ap);

	appendStringInfo(&outputBuffer, buf);

	va_end(ap);
}

/*
 * Mocks the function fwrite and captures the output in
 * the global outputBuffer
 */
int
fwrite_mock(const char *data, Size size, Size count, FILE *file)
{
	appendStringInfo(&outputBuffer, data);

	return count;
}

#define PG_RE_THROW() siglongjmp(*PG_exception_stack, 1)
/*
 * This method will emulate the real ExceptionalCondition
 * function by re-throwing the exception, essentially falling
 * back to the next available PG_CATCH();
 */
void
_ExceptionalCondition()
{
     PG_RE_THROW();
}

/*
 * This method sets up MemoryContext tree as well as
 * the basic MemoryAccount data structures.
 */
void SetupMemoryDataStructures(void **state)
{
	MemoryContextInit();

	initStringInfoOfSize(&outputBuffer, MAX_OUTPUT_BUFFER_SIZE);
}

/*
 * This method cleans up MemoryContext tree and
 * the MemoryAccount data structures.
 */
void TeardownMemoryDataStructures(void **state)
{
	pfree(outputBuffer.data);

	/*
	 * TopMemoryContext deletion is not supported, so
	 * we are just resetting it. Note: even reset
	 * operation on TopMemoryContext is not supported,
	 * but for our purpose (as we will not be running
	 * anything after this call), it works to clean up
	 * for the next unit test.
	 */
	MemoryContextReset(TopMemoryContext);

	/* These are needed to be NULL for calling MemoryContextInit() */
	TopMemoryContext = NULL;
	CurrentMemoryContext = NULL;

	/*
	 * Memory accounts related variables need to be NULL before we
	 * try to setup memory account data structure again during the
	 * execution of the next test.
	 */
	MemoryAccountTreeLogicalRoot = NULL;
	TopMemoryAccount = NULL;
	MemoryAccountMemoryAccount = NULL;
	RolloverMemoryAccount = NULL;
	SharedChunkHeadersMemoryAccount = NULL;
	ActiveMemoryAccount = NULL;
	AlienExecutorMemoryAccount = NULL;
	MemoryAccountMemoryContext = NULL;

	/*
	 * We don't want to carry over peak to another test as this might
	 * interfere during testing string conversion functions
	 */
	MemoryAccountingPeakBalance = MemoryAccountingOutstandingBalance;
}

/*
 * Checks if the created account has correct owner type and quota set.
 */
void
test__CreateMemoryAccountImpl__AccountProperties(void **state)
{
	uint64 limits[] = {0, 2048, ULONG_LONG_MAX};
	MemoryOwnerType memoryOwnerTypes[] = {MEMORY_OWNER_TYPE_Planner, MEMORY_OWNER_TYPE_Exec_Hash};

	for (int i = 0; i < sizeof(limits)/sizeof(uint64); i++)
	{
		uint64 curLimit = limits[i];

		for (int j = 0; j < sizeof(memoryOwnerTypes)/sizeof(MemoryOwnerType); j++)
		{
			MemoryOwnerType curOwnerType = memoryOwnerTypes[j];

			MemoryAccount *newAccount = CreateMemoryAccountImpl(curLimit, curOwnerType, ActiveMemoryAccount);

			/*
			 * Make sure we create account with valid tag, desired
			 * ownerType and provided quota limit
			 */
			assert_true(MemoryAccountIsValid(newAccount));
			assert_true(newAccount->ownerType == curOwnerType);
			assert_true(newAccount->maxLimit == curLimit);
			/*
			 * We did not create any of the basic accounts (e.g., TopMemoryAccount,
			 * RolloverMemoryAccount etc.). Therefore, the parent should be the
			 * one we provided.
			 */
			assert_true(newAccount->parentAccount == ActiveMemoryAccount);

			assert_true(0 == newAccount->allocated && 0 == newAccount->freed && 0 == newAccount->peak);
		}
	}
}

/*
 * Checks if a new account creation results in an expected memory accounting
 * tree.
 */
void
test__CreateMemoryAccountImpl__TreeStructure(void **state)
{
	MemoryAccount *topFirstChild = TopMemoryAccount->firstChild;
	MemoryAccount *tempParentAccount = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, TopMemoryAccount);

	MemoryAccount *tempFirstChildAccount = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_SeqScan, tempParentAccount);
	MemoryAccount *tempSecondChildAccount = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_SeqScan, tempParentAccount);

	/*
	 * The following assertions are validating the tree structure.
	 * When we add a child to memory accounting tree, we point to
	 * it via parent's firstChild pointer. We construct the tree
	 * by using the nextSibling pointer of the child. So, if we
	 * add a child, the child will become the firstChild of the
	 * parent, and the child's nextSibling will point to whatever
	 * the parent's firstChild was pointing previously
	 */
	assert_true(TopMemoryAccount->firstChild == tempParentAccount);
	assert_true(topFirstChild == AlienExecutorMemoryAccount);
	assert_true(tempParentAccount->nextSibling == topFirstChild);
	assert_true(tempParentAccount->firstChild == tempSecondChildAccount);
	assert_true(tempSecondChildAccount->nextSibling == tempFirstChildAccount);
	assert_true(NULL == tempSecondChildAccount->firstChild);
	assert_true(NULL == tempFirstChildAccount->firstChild);
	assert_true(NULL == tempFirstChildAccount->nextSibling);
}

/*
 * Checks the separation between active and parent account. Typically
 * they are same, but we want to test the general case where they
 * are different.
 */
void
test__CreateMemoryAccountImpl__ActiveVsParent(void **state)
{
	MemoryAccount *tempParentAccount = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, TopMemoryAccount);

	MemoryAccount *oldActiveAccount = MemoryAccounting_SwitchAccount(TopMemoryAccount);

	MemoryAccount *tempChildAccount = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_SeqScan, tempParentAccount);

	MemoryAccounting_SwitchAccount(oldActiveAccount);

	/* Make sure we are not blindly using ActiveMemoryAccount */
	assert_true(tempChildAccount->parentAccount == tempParentAccount);
}

/*
 * Checks whether the core accounts have their pre-determined parents
 * regardless of the provided one.
 */
void
test__CreateMemoryAccountImpl__AutoDetectParent(void **state)
{
	assert_true(ActiveMemoryAccount == TopMemoryAccount);

	/*
	 * Only the short-living accounts' memory gets accounted against
	 * MemoryAccountMemoryAccount. The long-living one doesn't
	 * even attempt to change memory account. This causes a segv
	 * for this test-case if we don't explicitly switch our
	 * active memory account to something long-living account
	 * (note: the active memory account is currently set to
	 * TopMemoryAccount, which is short-living), as we are
	 * creating long-living accounts. Therefore, we explicitly
	 * switch to a long living account.
	 */
	MemoryAccounting_SwitchAccount(MemoryAccountMemoryAccount);

	/* This will create a short-living account in MemoryAccountMemoryContext */
	MemoryAccount *tempParentAccount = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, TopMemoryAccount);

	/*
	 * Any short-living account should be created in MemoryAccountMemoryContext
	 * to ensure timely release of memory upon MemoryAccounting_Reset()
	 */
	assert_true(MemoryContextContains(MemoryAccountMemoryContext, tempParentAccount));

	/*
	 * We must not switch to tempParentAccount, as we will end up creating a longer-living
	 * account under short-living account
	 */
	MemoryAccount *tempShared = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_SharedChunkHeader, tempParentAccount);
	/*
	 * SharedChunkHeadersMemoryAccount is a long-living account, so it should be
	 * created under TopMemoryContext to survive MemoryAccountMemoryContext reset
	 */
	assert_true(MemoryContextContains(TopMemoryContext, tempShared));

	MemoryAccount *tempRollover = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Rollover, tempParentAccount);
	/* Rollover is also a long-living account. Therefore it should reside in TopMemoryContext */
	assert_true(MemoryContextContains(TopMemoryContext, tempRollover));

	MemoryAccount *tempMemAccount = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_MemAccount, tempParentAccount);
	/* MemoryAccountMemoryAccount is another long-living account */
	assert_true(MemoryContextContains(TopMemoryContext, tempMemAccount));

	MemoryAccount *tempTop = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Top, tempParentAccount);
	/*
	 * TopMemoryAccount is not a long-living account, and therefore should
	 * be created under MemoryAccountMemoryAccount
	 */
	assert_true(MemoryContextContains(MemoryAccountMemoryContext, tempTop));

	/*
	 * Verify that all the long living accounts have logical tree root
	 * as their parent
	 */
	assert_true(tempShared->parentAccount == MemoryAccountTreeLogicalRoot);
	assert_true(tempRollover->parentAccount == MemoryAccountTreeLogicalRoot);
	assert_true(tempMemAccount->parentAccount == MemoryAccountTreeLogicalRoot);
	/* Top is the only short-living account that goes under the logical root */
	assert_true(tempTop->parentAccount == MemoryAccountTreeLogicalRoot);
	/*
	 * Any other short-living account other than Top should be a direct
	 * or indirect descendant of Top
	 */
	assert_true(tempParentAccount->parentAccount == TopMemoryAccount);
}

/*
 * Checks whether the regular account creation charges the overhead
 * in the MemoryAccountMemoryAccount and SharedChunkHeadersMemoryAccount.
 */
void
test__CreateMemoryAccountImpl__TracksMemoryOverhead(void **state)
{
	MemoryAccount *tempParentAccount = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, TopMemoryAccount);

	uint64 prevAllocated = MemoryAccountMemoryAccount->allocated;
	uint64 prevFreed = MemoryAccountMemoryAccount->freed;
	uint64 prevOverallOutstanding = MemoryAccountingOutstandingBalance;

	uint64 prevSharedAllocated = SharedChunkHeadersMemoryAccount->allocated;
	uint64 prevSharedFreed = SharedChunkHeadersMemoryAccount->freed;

	MemoryAccount *tempChildAccount = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, tempParentAccount);

	/* Only MemoryAccountMemoryAccount changed, and nothing else changed */
	assert_true((MemoryAccountMemoryAccount->allocated - prevAllocated) +
			(SharedChunkHeadersMemoryAccount->allocated - prevSharedAllocated) ==
			(MemoryAccountingOutstandingBalance - prevOverallOutstanding));

	/* Make sure we saw an increase of balance in MemoryAccountMemoryAccount */
	assert_true(MemoryAccountMemoryAccount->allocated > prevAllocated);
	/*
	 * All the accounts are created in MemoryAccountMemoryContext, with
	 * MemoryAccountMemoryAccount as the ActiveMemoryAccount. So, they
	 * should find a shared header, without increasing SharedChunkHeadersMemoryAccount
	 * balance
	 */
	assert_true(SharedChunkHeadersMemoryAccount->allocated == prevSharedAllocated);
	/* Nothing was freed from MemoryAccountMemoryAccount */
	assert_true(MemoryAccountMemoryAccount->freed == prevFreed);
	assert_true(SharedChunkHeadersMemoryAccount->freed == prevSharedFreed);

	/*
	 * Now check SharedChunkHeadersMemoryAccount balance increase by advancing
	 * account generation and invalidating the sharedHeaderList.
	 */
	MemoryAccountingCurrentGeneration++;

	tempChildAccount = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, tempParentAccount);

	/*
	 * The new account cannot share any previous header due to generation
	 * advancement. So, we should see a balance increase in SharedChunkHeadersMemoryAccount
	 */
	assert_true(SharedChunkHeadersMemoryAccount->allocated > prevSharedAllocated);

	/* Free this memory as we are going back to previous generation to ensure proper teardown */
	pfree(tempChildAccount);

	/*
	 * Go back to previous generation to ensure proper teardown
	 * (should subtract from the chunks' respective accounts, not the rollover)
	 */
	MemoryAccountingCurrentGeneration--;
}

/*
 * Checks whether the regular account creation charges the overhead
 * in the MemoryAccountMemoryContext.
 */
void
test__CreateMemoryAccountImpl__AllocatesOnlyFromMemoryAccountMemoryContext(void **state)
{
	uint64 MemoryAccountMemoryContextOldAlloc = MemoryAccountMemoryContext->allBytesAlloc;
	uint64 MemoryAccountMemoryContextOldFreed = MemoryAccountMemoryContext->allBytesFreed;

	uint64 TopMemoryContextOldAlloc = TopMemoryContext->allBytesAlloc;
	uint64 TopMemoryContextOldFreed = TopMemoryContext->allBytesFreed;

	/*
	 * Increase MemoryAccountMemoryContext balance by creating enough accounts
	 * to trigger a new block reservation in allocation set. Note, creating
	 * one account may not increase the balance, as the balance is in terms
	 * of blocks reserved, and not in terms of actual usage.
	 */
	for (int i = 0; i <= ALLOCSET_DEFAULT_INITSIZE / sizeof(MemoryAccount); i++)
	{
		MemoryAccount *tempAccount = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, ActiveMemoryAccount);
	}

	/* Make sure the TopMemoryContext is reflecting the new allocations */
	assert_true(TopMemoryContextOldAlloc > 0 && TopMemoryContext->allBytesAlloc > TopMemoryContextOldAlloc);
	/* MemoryAccountMemoryContext should reflect a balance increase */
	assert_true(MemoryAccountMemoryContextOldAlloc > 0 && MemoryAccountMemoryContext->allBytesAlloc > MemoryAccountMemoryContextOldAlloc);
	/*
	 * The entire TopMemoryContext balance change should be due to
	 * MemoryAccountMemoryContext balance change
	 */
	assert_true((MemoryAccountMemoryContext->allBytesAlloc - MemoryAccountMemoryContextOldAlloc) ==
			(TopMemoryContext->allBytesAlloc - TopMemoryContextOldAlloc));

	/* Nothing should be freed */
	assert_true(TopMemoryContext->allBytesFreed == TopMemoryContextOldFreed);
	assert_true(MemoryAccountMemoryContext->allBytesFreed == MemoryAccountMemoryContextOldFreed);
}

/*
 * This method tests whether MemoryAccounting_SwitchAccount
 * actually switches the ActiveMemoryAccount to the correct
 * one.
 */
void
test__MemoryAccounting_SwitchAccount__AccountIsSwitched(void **state)
{
	MemoryAccount *newAccount = 0xabcdefab;
	ActiveMemoryAccount = 0xbafedcba;
	MemoryAccount *oldActiveMemoryAccount = ActiveMemoryAccount;

	MemoryAccount *oldAccount = MemoryAccounting_SwitchAccount(newAccount);

	assert_true(oldAccount == oldActiveMemoryAccount);
	assert_true(ActiveMemoryAccount == newAccount);
}

/*
 * This method tests whether MemoryAccounting_SwitchAccount
 * triggers an assertion failure in the event of a NULL input
 * account.
 */
void
test__MemoryAccounting_SwitchAccount__RequiresNonNullAccount(void **state)
{
	MemoryAccount *nullAccount = NULL;
	ActiveMemoryAccount = makeNode(MemoryAccount);

#ifdef USE_ASSERT_CHECKING
    expect_any(ExceptionalCondition,conditionName);
    expect_any(ExceptionalCondition,errorType);
    expect_any(ExceptionalCondition,fileName);
    expect_any(ExceptionalCondition,lineNumber);

    will_be_called_with_sideeffect(ExceptionalCondition, &_ExceptionalCondition, NULL);

    /* Test if within memory-limit strings cause assertion failure */
	PG_TRY();
	{
		MemoryAccount *oldAccount = MemoryAccounting_SwitchAccount(nullAccount);
		assert_true(false);
	}
	PG_CATCH();
	{
	}
	PG_END_TRY();
#endif

	pfree(ActiveMemoryAccount);
}

/*
 * This method tests whether MemoryAccountIsValid correctly
 * determines the validity of the input account.
 */
void
test__MemoryAccountIsValid__ProperValidation(void **state)
{
	MemoryAccount *nullInvalidAccount = NULL;

	/* Create a valid node without the proper header (i.e., MemoryAccount header type) */
	MemoryAccount *invalidAccountWithHeader = makeNode(SerializedMemoryAccount);

	/* Create a MemoryAccount sized data structure without the header information */
	MemoryAccount *invalidAccountWithoutHeader = palloc0(sizeof(MemoryAccount));

	/* Finally, the real one, where we create a MemoryAccount type node */
	MemoryAccount *validAccount = makeNode(MemoryAccount);

	bool isNullValid = MemoryAccountIsValid(nullInvalidAccount);
	bool isInvalidHeaderValid = MemoryAccountIsValid(invalidAccountWithHeader);
	bool isNoHeaderValid = MemoryAccountIsValid(invalidAccountWithoutHeader);
	bool isValidAccountValid = MemoryAccountIsValid(validAccount);

	/* NULL account is not a valid account */
	assert_false(isNullValid);
	/* An account without proper header (T_MemoryAccount) is invalid */
	assert_false(isInvalidHeaderValid);
	/* An account without any header is invalid */
	assert_false(isNoHeaderValid);
	/* A valid account is one with the proper header */
	assert_true(isValidAccountValid);

	pfree(invalidAccountWithHeader);
	pfree(invalidAccountWithoutHeader);
	pfree(validAccount);
}

/*
 * Tests if the MemoryAccounting_Reset() reuses the long living accounts
 * such as SharedChunkHeadersMemoryAccount, RolloverMemoryAccount,
 * MemoryAccountMemoryAccount and MemoryAccountTreeLogicalRoot
 * (i.e., they should not be recreated)
 */
void
test__MemoryAccounting_Reset__ReusesLongLivingAccounts(void **state)
{
	MemoryAccount *oldLogicalRoot = MemoryAccountTreeLogicalRoot;
	MemoryAccount *oldSharedChunkHeadersMemoryAccount = SharedChunkHeadersMemoryAccount;
	MemoryAccount *oldRollover = RolloverMemoryAccount;
	MemoryAccount *oldMemoryAccount = MemoryAccountMemoryAccount;

	/*
	 * We want to make sure that the reset process preserves
	 * long-living accounts. A pointer comparison is not safe,
	 * as pointers may be same, even for newly created accounts.
	 * Therefore, we are marking old accounts with special
	 * maxLimit, so that we can identify if we are reusing
	 * existing accounts or creating new ones.
	 */
	oldLogicalRoot->maxLimit = ULONG_LONG_MAX;
	oldSharedChunkHeadersMemoryAccount->maxLimit = ULONG_LONG_MAX;
	oldRollover->maxLimit = ULONG_LONG_MAX;
	oldMemoryAccount->maxLimit = ULONG_LONG_MAX;

	MemoryAccounting_Reset();

	/* Make sure we have a valid set of accounts */
	assert_true(MemoryAccountIsValid(MemoryAccountTreeLogicalRoot));
	assert_true(MemoryAccountIsValid(SharedChunkHeadersMemoryAccount));
	assert_true(MemoryAccountIsValid(RolloverMemoryAccount));
	assert_true(MemoryAccountIsValid(MemoryAccountMemoryAccount));

	/* MemoryAccountTreeLogicalRoot should be reused (i.e., MemoryAccountTreeLogicalRoot will survive the reset) */
	assert_true(oldLogicalRoot && MemoryAccountTreeLogicalRoot && MemoryAccountTreeLogicalRoot->maxLimit == ULONG_LONG_MAX);

	/* SharedChunkHeadersMemoryAccount should be reused (i.e., SharedChunkHeadersMemoryAccount will survive the reset) */
	assert_true(oldSharedChunkHeadersMemoryAccount && SharedChunkHeadersMemoryAccount &&
			SharedChunkHeadersMemoryAccount->maxLimit == ULONG_LONG_MAX);

	/* RolloverMemoryAccount should be reused (i.e., rollover will survive the reset) */
	assert_true(oldRollover && RolloverMemoryAccount && RolloverMemoryAccount->maxLimit == ULONG_LONG_MAX);

	/* MemoryAccountMemoryAccount should be reused (i.e., MemoryAccountMemoryAccount will survive the reset) */
	assert_true(oldMemoryAccount && MemoryAccountMemoryAccount && MemoryAccountMemoryAccount->maxLimit == ULONG_LONG_MAX);
}

/*
 * Tests if the MemoryAccounting_Reset() recreates all the short-living
 * basic accounts such as TopMemoryAccount and AlienExecutorMemoryAccount
 */
void
test__MemoryAccounting_Reset__RecreatesShortLivingAccounts(void **state)
{
	MemoryAccount *oldTop = TopMemoryAccount;
	MemoryAccount *oldAlien = AlienExecutorMemoryAccount;

	/*
	 * We want to make sure that the reset process re-creates
	 * short-living accounts. A pointer comparison is not safe,
	 * as pointers may be same, even for newly created accounts.
	 * Therefore, we are marking old accounts with special maxLimit,
	 * so that we can identify if we are reusing existing accounts
	 * or creating new ones.
	 */
	oldTop->maxLimit = ULONG_LONG_MAX;
	oldAlien->maxLimit = ULONG_LONG_MAX;

	MemoryAccounting_Reset();

	/* Make sure we have a valid set of accounts */
	assert_true(MemoryAccountIsValid(TopMemoryAccount));
	assert_true(MemoryAccountIsValid(AlienExecutorMemoryAccount));
 	assert_true(ActiveMemoryAccount == TopMemoryAccount);

 	/* TopMemoryAccount should be newly created */
	assert_true(oldTop && TopMemoryAccount && TopMemoryAccount->maxLimit != ULONG_LONG_MAX);
	/* AlienExecutorMemoryAccount should be newly created */
	assert_true(oldAlien && AlienExecutorMemoryAccount && AlienExecutorMemoryAccount->maxLimit != ULONG_LONG_MAX);
}

/*
 * Tests if the MemoryAccounting_Reset() reuses MemoryAccountMemoryContext,
 * i.e., it does not recreate this account
 */
void
test__MemoryAccounting_Reset__ReusesMemoryAccountMemoryContext(void **state)
{
#define CONTEXT_MARKER "ABCDEFG"

	MemoryContext oldMemContext = MemoryAccountMemoryContext;

	/*
	 * For validation that we are reusing old MemoryAccountMemoryContext,
	 * we set the name to CONTEXT_MARKER
	 */
	oldMemContext->name = CONTEXT_MARKER;

	MemoryAccounting_Reset();

	/* MemoryAccountMemoryContext should be reused (i.e., not dropped and recreated) */
	assert_true(oldMemContext == MemoryAccountMemoryContext &&
			strlen(MemoryAccountMemoryContext->name) == strlen(CONTEXT_MARKER));
}

/*
 * Tests if the MemoryAccounting_Reset() resets MemoryAccountMemoryContext
 * to drop all the previous generation memory accounts
 */
void
test__MemoryAccounting_Reset__ResetsMemoryAccountMemoryContext(void **state)
{
	int64 oldMemContextBalance = MemoryAccountMemoryContext->allBytesAlloc - MemoryAccountMemoryContext->allBytesFreed;

	int numAccountsToCreate = (ALLOCSET_DEFAULT_INITSIZE / sizeof(MemoryAccount)) + 1;
	/*
	 * Increase MemoryAccountMemoryContext balance by creating enough accounts
	 * to trigger a new block reservation in allocation set. Note, creating
	 * one account may not increase the balance, as the balance is in terms
	 * of blocks reserved, and not in terms of actual usage.
	 */
	for (int i = 0; i < numAccountsToCreate; i++)
	{
		MemoryAccount *tempAccount = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, ActiveMemoryAccount);
	}

	assert_true(oldMemContextBalance <
		(MemoryAccountMemoryContext->allBytesAlloc - MemoryAccountMemoryContext->allBytesFreed));

	/* Record the extra balance for checking that the reset process will clear out this balance */
	oldMemContextBalance = (MemoryAccountMemoryContext->allBytesAlloc - MemoryAccountMemoryContext->allBytesFreed);

	MemoryAccounting_Reset();

	int64 newMemContextBalance = MemoryAccountMemoryContext->allBytesAlloc - MemoryAccountMemoryContext->allBytesFreed;

	/* Reset process should at least free all the accounts that we have created (and overhead per-account) */
	assert_true(newMemContextBalance < (oldMemContextBalance - numAccountsToCreate * sizeof(MemoryAccount)));
}

/*
 * Tests if the MemoryAccounting_Reset() sets TopMemoryAccount
 * as the ActiveMemoryAccount
 */
void
test__MemoryAccounting_Reset__TopIsActive(void **state)
{
	MemoryAccount *newActiveAccount = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, ActiveMemoryAccount);

	MemoryAccount *oldActiveAccount = MemoryAccounting_SwitchAccount(newActiveAccount);

	assert_true(ActiveMemoryAccount == newActiveAccount);

	MemoryAccounting_Reset();

	assert_true(ActiveMemoryAccount == TopMemoryAccount);
}

/*
 * Tests if the MemoryAccounting_Reset() advances memory account
 * generation, so that previous generation accounts do not
 * cause segfault
 */
void
test__MemoryAccounting_Reset__AdvancesGeneration(void **state)
{
	uint16 oldGeneration = MemoryAccountingCurrentGeneration;

	MemoryAccounting_Reset();

	/*
	 * We don't test for generation migration here, which will be handled
	 * in the test case for AdvanceMemoryAccountingGeneration
	 */
	assert_true(oldGeneration < MemoryAccountingCurrentGeneration);
}

/*
 * Tests if the MemoryAccounting_Reset() transfers any remaining
 * memory account balance into a dedicated rollover account
 */
void
test__MemoryAccounting_Reset__TransferRemainingToRollover(void **state)
{

	uint64 oldOutstandingBal = MemoryAccountingOutstandingBalance;

	int * tempAlloc = palloc(NEW_ALLOC_SIZE);

	/* There will be header overhead */
	assert_true(oldOutstandingBal + NEW_ALLOC_SIZE < MemoryAccountingOutstandingBalance);

	uint64 newOutstandingBalance = MemoryAccountingOutstandingBalance;

	/* The amount of memory used to create all the memory accounts */
	uint64 oldMemoryAccountBalance = MemoryAccountMemoryAccount->allocated - MemoryAccountMemoryAccount->freed;

	MemoryAccounting_Reset();

	/*
	 * Memory accounts will be dropped, and their memory will be released. So, everything
	 * but that should go into RolloverMemoryAccount
	 */
	assert_true((newOutstandingBalance - oldMemoryAccountBalance -
			(SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed)) ==
					(RolloverMemoryAccount->allocated - RolloverMemoryAccount->freed));

	pfree(tempAlloc);
}

/*
 * Tests if the MemoryAccounting_Reset() resets the high water
 * mark (i.e., peak balance)
 */
void
test__MemoryAccounting_Reset__ResetPeakBalance(void **state)
{
	/* First allocate new memory to push the peak balance higher */
	int * tempAlloc = palloc(NEW_ALLOC_SIZE);

	pfree(tempAlloc);

	/* Peak balance should be bigger than outstanding due the pfree call */
	assert_true(MemoryAccountingPeakBalance > MemoryAccountingOutstandingBalance);

	uint64 oldMemoryAccountingPeakBalance = MemoryAccountingPeakBalance;

	MemoryAccounting_Reset();

	/* After reset, peak balance should be back to outstanding balance */
	assert_true(MemoryAccountingPeakBalance == MemoryAccountingOutstandingBalance);
}

/*
 * This method tests that the memory accounting reset
 * process properly initializes the basic data structures.
 */
void
test__MemoryAccounting_Reset__TreeStructure(void **state)
{
	MemoryAccounting_Reset();

	assert_true(NULL != MemoryAccountTreeLogicalRoot);
	/* First child of logical root should be TopMemoryAccount */
	assert_true(MemoryAccountTreeLogicalRoot->firstChild == TopMemoryAccount);
	/*
	 * nextSibling points to the parent's next child. As logical
	 * root does not have any parent, so the next sibling should be NULL
	 */
	assert_true(NULL == MemoryAccountTreeLogicalRoot->nextSibling);
	/* AlienExecutorMemoryAccount is the only child of TopMemoryAccount */
	assert_true(TopMemoryAccount->firstChild == AlienExecutorMemoryAccount);
	/* MemoryAccountMemoryAccount is the next child of logical root (first child is Top) */
	assert_true(TopMemoryAccount->nextSibling == MemoryAccountMemoryAccount);
	/* RolloverMemoryAccount is the next child of logical root */
	assert_true(MemoryAccountMemoryAccount->nextSibling == RolloverMemoryAccount);
	/* SharedChunkHeadersMemoryAccount is the next child of logical root */
	assert_true(RolloverMemoryAccount->nextSibling == SharedChunkHeadersMemoryAccount);
	/* SharedChunkHeadersMemoryAccount is the last child of logical root */
	assert_true(SharedChunkHeadersMemoryAccount->nextSibling == NULL);

	/* AlienExecutorMemoryAccount is a leaf node */
	assert_true(NULL == AlienExecutorMemoryAccount->firstChild);
	/* AlienExecutorAccount should not have any sibling */
	assert_true(AlienExecutorMemoryAccount->nextSibling == NULL);

	/* All long-living nodes except logical tree root should be leaf node */
	assert_true(NULL == SharedChunkHeadersMemoryAccount->firstChild);
	assert_true(NULL == RolloverMemoryAccount->firstChild);
	assert_true(NULL == MemoryAccountMemoryAccount->firstChild);
}

/*
 * Tests if the MemoryAccounting_AdvanceMemoryAccountingGeneration()
 * transfers the entire outstanding balance to RolloverMemoryAccount
 * when there is no generation overflow, and so we don't need to
 * switch all shared headers' ownership to Rollover (i.e., no migration
 * of the shared headers's ownership to rollover happened)
 */
void
test__MemoryAccounting_AdvanceMemoryAccountingGeneration__TransfersBalanceToRolloverWithoutMigration(void **state)
{
	/* First allocate some memory so that we have something to rollover (do not free) */
	palloc(NEW_ALLOC_SIZE);

	uint64 oldRolloverBalance = RolloverMemoryAccount->allocated - RolloverMemoryAccount->freed;

	/* As we have new allocations, rollover must be smaller than outstanding */
	assert_true(oldRolloverBalance < MemoryAccountingOutstandingBalance);

	AdvanceMemoryAccountingGeneration();

	uint64 newRolloverBalance = RolloverMemoryAccount->allocated - RolloverMemoryAccount->freed;

	/* The entire outstanding balance except SharedChunkHeadersMemoryAccount balance should now be in rollover's bucket */
	assert_true((newRolloverBalance + (SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed)) ==
			MemoryAccountingOutstandingBalance);
}

/*
 * Tests if the MemoryAccounting_AdvanceMemoryAccountingGeneration()
 * transfers the entire outstanding balance to RolloverMemoryAccount
 * during generation overflow (i.e., migrates chunks from former
 * owner to Rollover)
 */
void
test__MemoryAccounting_AdvanceMemoryAccountingGeneration__TransfersBalanceToRolloverWithMigration(void **state)
{
	/* First allocate some memory so that we have something to rollover (do not free) */
	palloc(NEW_ALLOC_SIZE);

	uint64 oldRolloverBalance = RolloverMemoryAccount->allocated - RolloverMemoryAccount->freed;

	assert_true(oldRolloverBalance < MemoryAccountingOutstandingBalance);

	/*
	 * Force a generation counter overflow (and trigger a migration)
	 * upon next AdvanceMemoryAccountingGeneration() call
	 */
	MemoryAccountingCurrentGeneration = USHRT_MAX;

	/* Prepare for a elog() call during migration */
	will_be_called(elog_start);
	expect_any(elog_start, filename);
	expect_any(elog_start, lineno);
	expect_any(elog_start, funcname);
	will_be_called(elog_finish);
	expect_any(elog_finish, elevel);
	expect_any(elog_finish, fmt);

	/*
	 * Give all the balance to Rollover to pass the assertion check during MemoryAccounting_Free
	 * where it subtracts any outstanding balance from Rollover because of generation mismatch
	 * and expects to see a positive balance. Also, make sure that we pass the assertion check
	 * in AdvanceMemoryAccountGeneration() where it checks that all the MemoryAccountMemoryAccount
	 * balances have been freed (MemoryAccountMemoryAccount is not supposed to carry any balance
	 * from previous generation, but due to artificial generation jump, it looks like it is carrying
	 * from earlier generation. We want to fix this.)
	 */
	RolloverMemoryAccount->allocated = MemoryAccountMemoryAccount->allocated - MemoryAccountMemoryAccount->freed;
	RolloverMemoryAccount->freed = 0;
	/* Act as if we don't have any carryover */
	MemoryAccountMemoryAccount->freed = MemoryAccountMemoryAccount->allocated;

	AdvanceMemoryAccountingGeneration();

	/* Generation counter should wrap around */
	assert_true(MemoryAccountingCurrentGeneration == 0);

	uint64 newRolloverBalance = RolloverMemoryAccount->allocated - RolloverMemoryAccount->freed;

	/* The entire outstanding balance except SharedChunkHeadersMemoryAccount balance should now be in rollover's bucket */
	assert_true((newRolloverBalance + (SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed)) ==
			MemoryAccountingOutstandingBalance && newRolloverBalance > oldRolloverBalance);
}

/*
 * Tests if the MemoryAccounting_AdvanceMemoryAccountingGeneration()
 * sets RolloverMemoryAccount as the ActiveMemoryAccount
 */
void
test__MemoryAccounting_AdvanceMemoryAccountingGeneration__SetsActiveToRollover(void **state)
{
	MemoryAccounting_SwitchAccount(TopMemoryAccount);

	AdvanceMemoryAccountingGeneration();

	assert_true(RolloverMemoryAccount == ActiveMemoryAccount);
}

/*
 * Tests if the MemoryAccounting_AdvanceMemoryAccountingGeneration()
 * advances generation in non-migration (no generation overflow) case
 */
void
test__MemoryAccounting_AdvanceMemoryAccountingGeneration__AdvancesGeneration(void **state)
{
	uint16 oldGeneration = MemoryAccountingCurrentGeneration;

	AdvanceMemoryAccountingGeneration();

	assert_true((oldGeneration + 1) == MemoryAccountingCurrentGeneration);
}

/*
 * Tests if the MemoryAccounting_AdvanceMemoryAccountingGeneration()
 * preserves the chunk header (for performance reason) if there
 * is no generation overflow
 */
void
test__MemoryAccounting_AdvanceMemoryAccountingGeneration__PreservesChunkHeader(void **state)
{
	MemoryAccount *newActiveAccount = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, ActiveMemoryAccount);

	/* Make sure we have a new active account other than Rollover */
	MemoryAccount *oldActiveAccount = MemoryAccounting_SwitchAccount(newActiveAccount);

	assert_true(ActiveMemoryAccount == newActiveAccount);

	/* Establish at least 3 level depths of tree (TopMemoryContext->ErrorContext->leafContext) */
	MemoryContext *leafContext = AllocSetContextCreate(ErrorContext,
			   "TestContext",
			   ALLOCSET_DEFAULT_MINSIZE,
			   ALLOCSET_DEFAULT_INITSIZE,
			   ALLOCSET_DEFAULT_MAXSIZE);

	/*
	 * We created at least a depth 3 memory context tree, and we want to allocate
	 * memory in all three levels. Then we want to make sure the AdvanceMemoryAccountingGeneration
	 * doesn't change any of these chunks' headers
	 */
	int *topAlloc = MemoryContextAlloc(TopMemoryContext, NEW_ALLOC_SIZE);
	int *errorAlloc = MemoryContextAlloc(ErrorContext, NEW_ALLOC_SIZE);
	int *leafAlloc = MemoryContextAlloc(leafContext, NEW_ALLOC_SIZE);

	StandardChunkHeader *topAllocChunk = AllocPointerGetChunk(topAlloc);
	StandardChunkHeader *errorAllocChunk = AllocPointerGetChunk(errorAlloc);
	StandardChunkHeader *leafAllocChunk = AllocPointerGetChunk(leafAlloc);

	uint16 oldGeneration = MemoryAccountingCurrentGeneration;

	/*
	 * Make sure the chunks are from current generation and are owned
	 * by an account that is soon to be extinct
	 */
	assert_true(topAllocChunk->sharedHeader->memoryAccountGeneration == MemoryAccountingCurrentGeneration &&
			topAllocChunk->sharedHeader->memoryAccount == newActiveAccount);
	assert_true(errorAllocChunk->sharedHeader->memoryAccountGeneration == MemoryAccountingCurrentGeneration &&
			errorAllocChunk->sharedHeader->memoryAccount == newActiveAccount);
	assert_true(leafAllocChunk->sharedHeader->memoryAccountGeneration == MemoryAccountingCurrentGeneration &&
			leafAllocChunk->sharedHeader->memoryAccount == newActiveAccount);

	AdvanceMemoryAccountingGeneration();

	/* Now make sure that the chunks are unchanged, as this is a non-migration case */
	assert_true(topAllocChunk->sharedHeader->memoryAccountGeneration == oldGeneration &&
			topAllocChunk->sharedHeader->memoryAccount == newActiveAccount);
	assert_true(errorAllocChunk->sharedHeader->memoryAccountGeneration == oldGeneration &&
			errorAllocChunk->sharedHeader->memoryAccount == newActiveAccount);
	assert_true(leafAllocChunk->sharedHeader->memoryAccountGeneration == oldGeneration &&
			leafAllocChunk->sharedHeader->memoryAccount == newActiveAccount);
}

/*
 * Tests if the MemoryAccounting_AdvanceMemoryAccountingGeneration()
 * changes all shared chunk headers if there is a generation overflow
 */
void
test__MemoryAccounting_AdvanceMemoryAccountingGeneration__MigratesChunkHeaders(void **state)
{
	MemoryAccountingCurrentGeneration = 10;

	/* Make sure we have a new active account other than Rollover */
	MemoryAccount *newActiveAccount = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, ActiveMemoryAccount);

	MemoryAccount *oldActiveAccount = MemoryAccounting_SwitchAccount(newActiveAccount);

	assert_true(ActiveMemoryAccount == newActiveAccount);

	assert_true(TopMemoryContext != NULL && ErrorContext != NULL && ErrorContext->parent == TopMemoryContext);

	/* Establish at least 3 level depths of tree (TopMemoryContext->ErrorContext->leafContext) */
	MemoryContext *leafContext = AllocSetContextCreate(ErrorContext,
			   "TestContext",
			   ALLOCSET_DEFAULT_MINSIZE,
			   ALLOCSET_DEFAULT_INITSIZE,
			   ALLOCSET_DEFAULT_MAXSIZE);

	/*
	 * We created at least a depth 3 memory context tree, and we want to allocate
	 * memory in all three levels. Then we want to make sure the AdvanceMemoryAccountingGeneration
	 * recursively traverse the tree to change every allocated chunk's header
	 */
	int *topAlloc = MemoryContextAlloc(TopMemoryContext, NEW_ALLOC_SIZE);
	int *errorAlloc = MemoryContextAlloc(ErrorContext, NEW_ALLOC_SIZE);
	int *leafAlloc = MemoryContextAlloc(leafContext, NEW_ALLOC_SIZE);

	StandardChunkHeader *topAllocChunk = AllocPointerGetChunk(topAlloc);
	StandardChunkHeader *errorAllocChunk = AllocPointerGetChunk(errorAlloc);
	StandardChunkHeader *leafAllocChunk = AllocPointerGetChunk(leafAlloc);

	uint16 oldGeneration = MemoryAccountingCurrentGeneration;

	/*
	 * Make sure the chunks are from older generation and are owned
	 * by an account that is soon to be extinct
	 */
	assert_true(topAllocChunk->sharedHeader->memoryAccountGeneration == MemoryAccountingCurrentGeneration &&
			topAllocChunk->sharedHeader->memoryAccount == newActiveAccount);
	assert_true(errorAllocChunk->sharedHeader->memoryAccountGeneration == MemoryAccountingCurrentGeneration &&
			errorAllocChunk->sharedHeader->memoryAccount == newActiveAccount);
	assert_true(leafAllocChunk->sharedHeader->memoryAccountGeneration == MemoryAccountingCurrentGeneration &&
			leafAllocChunk->sharedHeader->memoryAccount == newActiveAccount);

	/* Check the boundary, where we are just 1 short of generation migration */
	MemoryAccountingCurrentGeneration = USHRT_MAX - 1;

	/*
	 * Give all the balance to Rollover to pass the assertion check during MemoryAccounting_Free
	 * where it subtracts any outstanding balance from Rollover because of generation mismatch
	 * and expects to see a positive balance. Also, make sure that we pass the assertion check
	 * in AdvanceMemoryAccountGeneration() where it checks that all the MemoryAccountMemoryAccount
	 * balances have been freed (MemoryAccountMemoryAccount is not supposed to carry any balance
	 * from previous generation, but due to artificial generation jump, it looks like it is carrying
	 * from earlier generation. We want to fix this.)
	 */
	RolloverMemoryAccount->allocated = MemoryAccountMemoryAccount->allocated - MemoryAccountMemoryAccount->freed;
	/* Act as if we don't have any carryover */
	MemoryAccountMemoryAccount->freed = MemoryAccountMemoryAccount->allocated;

	AdvanceMemoryAccountingGeneration();

	/* This should be a non-migration case and the chunk headers should be unchanged */
	assert_false(topAllocChunk->sharedHeader->memoryAccountGeneration == 0 &&
			topAllocChunk->sharedHeader->memoryAccount == newActiveAccount);
	assert_false(errorAllocChunk->sharedHeader->memoryAccountGeneration == 0 &&
			errorAllocChunk->sharedHeader->memoryAccount == newActiveAccount);
	assert_false(leafAllocChunk->sharedHeader->memoryAccountGeneration == 0 &&
			leafAllocChunk->sharedHeader->memoryAccount == newActiveAccount);

	/*
	 * Now push the generation to the max, so that the next generation advancement
	 * triggers a migration
	 */
	MemoryAccountingCurrentGeneration = USHRT_MAX;

	will_be_called(elog_start);
	expect_any(elog_start, filename);
	expect_any(elog_start, lineno);
	expect_any(elog_start, funcname);
	will_be_called(elog_finish);
	expect_any(elog_finish, elevel);
	expect_any(elog_finish, fmt);

	AdvanceMemoryAccountingGeneration();

	/*
	 * Make sure that the generation migration process changed
	 * headers of all levels of tree nodes. All the allocations
	 * should now be owned by RolloverMemoryAccount and their
	 * generation should be set to 0
	 */
	assert_true(topAllocChunk->sharedHeader->memoryAccountGeneration == 0 &&
			topAllocChunk->sharedHeader->memoryAccount == RolloverMemoryAccount);
	assert_true(errorAllocChunk->sharedHeader->memoryAccountGeneration == 0 &&
			errorAllocChunk->sharedHeader->memoryAccount == RolloverMemoryAccount);
	assert_true(leafAllocChunk->sharedHeader->memoryAccountGeneration == 0 &&
			leafAllocChunk->sharedHeader->memoryAccount == RolloverMemoryAccount);
}

/*
 * Tests if the MemoryContextReset() resets SharedChunkHeadersMemoryAccount balance
 */
void
test__MemoryContextReset__ResetsSharedChunkHeadersMemoryAccountBalance(void **state)
{
	/*
	 * Note: we are allocating the context itself in TopMemoryContext. This
	 * will crash during the reset of TopMemoryContext, as before resetting
	 * TopMemoryContext we will reset MemoryAccountMemoryContext which is a
	 * child of TopMemoryContext, therefore making all the memory accounts
	 * disappear. Now, normally we don't reset TopMemoryContext, but in unit
	 * test teardown we will try to reset TopMemoryContext, which will try
	 * to free this particular chunk which hosted the context struct itself.
	 * The memory account of the sharedHeader is however long gone during
	 * the MemoryAccountMemoryContext reset. So, we have to carefully set
	 * to a long-living active memory account to prevent a crash in the teardown
	 */
	MemoryAccount *oldAccount = MemoryAccounting_SwitchAccount(MemoryAccountMemoryAccount);
	MemoryContext newContext = AllocSetContextCreate(TopMemoryContext,
										   "TestContext",
										   ALLOCSET_DEFAULT_MINSIZE,
										   ALLOCSET_DEFAULT_INITSIZE,
										   ALLOCSET_DEFAULT_MAXSIZE);

	MemoryAccounting_SwitchAccount(oldAccount);

	MemoryContext oldContext = MemoryContextSwitchTo(newContext);

	/*
	 * Record the balance right after the new context creation. Note: the
	 * context creation itself might increase the balance of the
	 * SharedChunkHeadersMemoryAccount
	 */
	int64 initialSharedHeaderBalance = SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed;

	/* This would trigger a new shared header with ActiveMemoryAccount */
	void *testAlloc1 = palloc(sizeof(int));

	SharedChunkHeader *sharedHeader = ((AllocSet)newContext)->sharedHeaderList;

	/* Make sure we got the right memory account, and the right generation */
	assert_true(sharedHeader->memoryAccount == ActiveMemoryAccount &&
			sharedHeader->memoryAccountGeneration == MemoryAccountingCurrentGeneration);

	/* Make sure we did adjust SharedChunkHeadersMemoryAccount balance */
	assert_true(initialSharedHeaderBalance <
		(SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed));

	int64 prevSharedHeaderBalance = SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed;

	/* This would *not* trigger a new shared header (reuse header) */
	void *testAlloc2 = palloc(sizeof(int));

	/* Make sure no shared header balance is increased */
	assert_true(prevSharedHeaderBalance ==
			(SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed));

	/* We need a new active account to make sure that we are forcing a new SharedChunkHeader */
	MemoryAccount *newAccount = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Exec_Hash);

	oldAccount = MemoryAccounting_SwitchAccount(newAccount);

	/* Tiny alloc that requires a new SharedChunkHeader, due to a new ActiveMemoryAccount */
	void *testAlloc3 = palloc(sizeof(int));

	/* We should see an increase in SharedChunkHeadersMemoryAccount balance */
	assert_true(prevSharedHeaderBalance <
		(SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed));

	/*
	 * This should drop the SharedChunkHeadersMemoryAccount balance to the initial level
	 * (right after the new context creation)
	 */
	MemoryContextReset(newContext);
	assert_true(initialSharedHeaderBalance ==
			(SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed));
}

/*
 * Compares two memory accounting trees (the original tree and
 * the deserialized version of that tree after it was serialized)
 * to see if they are identical
 *
 * Parameters:
 * 		deserializedRoot: the root of the deserialized memory accounting sub-tree
 * 		originalRoot: the root of the original memory accounting sub-tree
 *
 * 	Returns true if the sub-tree is identical in terms of ownerType and maxLimit
 */
bool
compareDeserializedVsOriginalMemoryAccountingTree(MemoryAccount *deserializedRoot, MemoryAccount *originalRoot)
{
	if (deserializedRoot == NULL && originalRoot == NULL)
	{
		/* Identical tree */
		return true;
	}
	else if (deserializedRoot == NULL || originalRoot == NULL)
	{
		/* We exhausted one before the other */
		return false;
	}

	/*
	 * Cannot compare allocated and freed as the serialization process
	 * might allocate/free additional memory while walking the tree,
	 * introducing mismatch of allocated/freed between serialized and
	 * original version of an account
	 */
	if (deserializedRoot->ownerType != originalRoot->ownerType ||
			deserializedRoot->maxLimit != originalRoot->maxLimit)
	{
		return false;
	}

	bool identicalChildTree = compareDeserializedVsOriginalMemoryAccountingTree(deserializedRoot->firstChild, originalRoot->firstChild);

	if (!identicalChildTree)
	{
		return false;
	}

	MemoryAccount *deserializedSibling = NULL;
	MemoryAccount *originalSibling = NULL;

	bool identicalSibling = true;

	for (deserializedSibling = deserializedRoot->nextSibling,
			originalSibling = originalRoot->nextSibling;
			identicalSibling == true && deserializedSibling != NULL && originalSibling != NULL;
			deserializedSibling = deserializedSibling->nextSibling,
					originalSibling = originalSibling->nextSibling)
	{
		identicalSibling = compareDeserializedVsOriginalMemoryAccountingTree(deserializedSibling, originalSibling);
	}

	if (!identicalSibling || deserializedSibling != NULL || originalSibling != NULL)
	{
		return false;
	}

	return true;
}

/* Tests if the serialization and deserialization of the memory accounting tree is working */
void
test__MemoryAccounting_Serialize_Deserialize__Validate(void **state)
{
	StringInfoData buffer;
    initStringInfo(&buffer);

    uint32 totalSerialized = MemoryAccounting_Serialize(&buffer);

    /*
     * We haven't created any new account, so we should have
     * MemoryAccountTreeLogicalRoot, SharedChunkHeadersMemoryAccount,
     * RolloverMemoryAccount, TopMemoryAccount, and AlienExecutorMemoryAccount
     */
    assert_true(totalSerialized == 6);

    SerializedMemoryAccount *serializedTree = MemoryAccounting_Deserialize(buffer.data, totalSerialized);

    assert_true(compareDeserializedVsOriginalMemoryAccountingTree(&serializedTree->memoryAccount, MemoryAccountTreeLogicalRoot));

    /* This will also free serializedTree, as the deserialization is done in place */
    pfree(buffer.data);

	MemoryAccount *newAccount1 = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, TopMemoryAccount);
	MemoryAccount *newAccount2 = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_SeqScan, newAccount1);
	MemoryAccount *newAccount3 = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Sort, newAccount1);

	initStringInfo(&buffer);

	totalSerialized = MemoryAccounting_Serialize(&buffer);

	assert_true(totalSerialized == 9);

    serializedTree = MemoryAccounting_Deserialize(buffer.data, totalSerialized);
    assert_true(compareDeserializedVsOriginalMemoryAccountingTree(&serializedTree->memoryAccount, MemoryAccountTreeLogicalRoot));

    pfree(buffer.data);
    pfree(newAccount1);
    pfree(newAccount2);
    pfree(newAccount3);
}


/* Tests if the MemoryOwnerType enum to string conversion is working */
void
test__MemoryAccounting_GetAccountName__Validate(void **state)
{
#define LONG_LIVING_START 10
#define SHORT_LIVING_NON_OPERATOR_START 101
#define SHORT_LIVING_OPERATOR_START 1000

	char* longLivingNames[] = {"Root", "SharedHeader", "Rollover", "MemAcc"};

	char* shortLivingNonOpNames[] = {"Top", "Main", "Parser", "Planner", "Optimizer", "Dispatcher", "Serializer", "Deserializer"};

	char* shortLivingOpNames[] = {"Executor", "X_Result", "X_Append", "X_Sequence", "X_Bitmap", "X_BitmapOr", "X_SeqScan", "X_ExternalScan", "X_AppendOnlyScan", "X_AOCSCAN", "X_TableScan", "X_DynamicTableScan", "X_IndexScan", "X_DynamicIndexScan", "X_BitmapIndexScan",
			"X_BitmapHeapScan", "X_BitmapAppendOnlyScan", "X_TidScan", "X_SubqueryScan", "X_FunctionScan", "X_TableFunctionScan", "X_ValuesScan", "X_NestLoop", "X_MergeJoin", "X_HashJoin", "X_Material", "X_Sort", "X_Agg", "X_Unique", "X_Hash", "X_SetOp", "X_Limit",
			"X_Motion", "X_ShareInputScan", "X_Window", "X_Repeat", "X_DML", "X_SplitUpdate", "X_RowTrigger", "X_AssertOp","X_Alien"};

	for (int longLivingIndex = 0; longLivingIndex < sizeof(longLivingNames) / sizeof(char*); longLivingIndex++)
	{
		int memoryOwnerType = LONG_LIVING_START + longLivingIndex;
		MemoryAccount *newAccount = CreateMemoryAccountImpl(0, memoryOwnerType, ActiveMemoryAccount);

		assert_true(strcmp(MemoryAccounting_GetAccountName(newAccount), longLivingNames[longLivingIndex]) == 0);

		pfree(newAccount);
	}

	for (int shortLivingNonOpIndex = 0; shortLivingNonOpIndex < sizeof(shortLivingNonOpNames) / sizeof(char*); shortLivingNonOpIndex++)
	{
		int memoryOwnerType = SHORT_LIVING_NON_OPERATOR_START + shortLivingNonOpIndex;
		MemoryAccount *newAccount = CreateMemoryAccountImpl(0, memoryOwnerType, ActiveMemoryAccount);

		assert_true(strcmp(MemoryAccounting_GetAccountName(newAccount), shortLivingNonOpNames[shortLivingNonOpIndex]) == 0);

		pfree(newAccount);
	}

	for (int shortLivingOpIndex = 0; shortLivingOpIndex < sizeof(shortLivingOpNames) / sizeof(char*); shortLivingOpIndex++)
	{
		int memoryOwnerType = SHORT_LIVING_OPERATOR_START + shortLivingOpIndex;
		MemoryAccount *newAccount = CreateMemoryAccountImpl(0, memoryOwnerType, ActiveMemoryAccount);

		assert_true(strcmp(MemoryAccounting_GetAccountName(newAccount), shortLivingOpNames[shortLivingOpIndex]) == 0);

		pfree(newAccount);
	}
}


/* Tests if the MemoryAccounting_GetPeak is returning the correct peak balance */
void
test__MemoryAccounting_GetPeak__Validate(void **state)
{
	uint64 peakBalances[] = {0, UINT32_MAX, UINT64_MAX};

	for (int accountIndex = 0; accountIndex < sizeof(peakBalances) / sizeof(uint64); accountIndex++)
	{
		uint64 peakBalance = peakBalances[accountIndex];

		MemoryAccount *newAccount = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, ActiveMemoryAccount);

		newAccount->peak = peakBalance;

		assert_true(MemoryAccounting_GetPeak(newAccount) == peakBalance);

		pfree(newAccount);
	}
}

/* Tests if the MemoryAccounting_GetBalance is returning the current balance */
void
test__MemoryAccounting_GetBalance__Validate(void **state)
{
	uint64 allAllocated[] = {0, UINT32_MAX, UINT64_MAX, UINT64_MAX};
	uint64 allFreed[] = {0, UINT16_MAX, UINT64_MAX, 0};

	for (int accountIndex = 0; accountIndex < sizeof(allAllocated) / sizeof(uint64); accountIndex++)
	{
		uint64 allocated = allAllocated[accountIndex];
		uint64 freed = allFreed[accountIndex];

		MemoryAccount *newAccount = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, ActiveMemoryAccount);

		newAccount->allocated = allocated;
		newAccount->freed = freed;

		assert_true(MemoryAccounting_GetBalance(newAccount) == (allocated - freed));

		pfree(newAccount);
	}
}

/*
 * Tests if the MemoryAccounting_ToString is correctly converting
 * a memory accounting tree to string
 */
void
test__MemoryAccounting_ToString__Validate(void **state)
{
	char *templateString =
"Root: Peak 0K bytes. Quota: 0K bytes.\n\
  Top: Peak %" PRIu64 "K bytes. Quota: 0K bytes.\n\
    X_Hash: Peak %" PRIu64 "K bytes. Quota: 0K bytes.\n\
    X_Alien: Peak 0K bytes. Quota: 0K bytes.\n\
  MemAcc: Peak 0K bytes. Quota: 0K bytes.\n\
  Rollover: Peak 0K bytes. Quota: 0K bytes.\n\
  SharedHeader: Peak 0K bytes. Quota: 0K bytes.\n";

	/* ActiveMemoryAccount should be Top at this point */
	MemoryAccount *newAccount = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, ActiveMemoryAccount);

	void * dummy1 = palloc(NEW_ALLOC_SIZE);
	void * dummy2 = palloc(NEW_ALLOC_SIZE);

	MemoryAccounting_SwitchAccount(newAccount);

	void * dummy3 = palloc(NEW_ALLOC_SIZE);

	pfree(dummy1);
	pfree(dummy2);
	pfree(dummy3);

	StringInfoData buffer;
	initStringInfoOfSize(&buffer, MAX_OUTPUT_BUFFER_SIZE);

    MemoryAccounting_ToString(MemoryAccountTreeLogicalRoot, &buffer, 0 /* Indentation */);

	char		buf[MAX_OUTPUT_BUFFER_SIZE];
	snprintf(buf, sizeof(buf), templateString, TopMemoryAccount->peak / 1024, newAccount->peak / 1024);

    assert_true(strcmp(buffer.data, buf) == 0);

    pfree(buffer.data);
    pfree(newAccount);
}

/*
 * Tests if the MemoryAccounting_SaveToLog is generating the correct
 * string representation of the memory accounting tree before saving
 * it to log.
 *
 * Note: we don't test the exact log saving here.
 */
void
test__MemoryAccounting_SaveToLog__GeneratesCorrectString(void **state)
{
	char *templateString =
"memory: account_name, child_id, parent_id, quota, peak, allocated, freed, current\n\
memory: Vmem, 0, 0, 0, 0, 0, 0, 0\n\
memory: Peak, 0, 0, 0, %" PRIu64 ", %" PRIu64 ", 0, %" PRIu64 "\n\
memory: Root, 0, 0, 0, 0, 0, 0, 0\n\
memory: Top, 1, 0, 0, %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 "\n\
memory: X_Hash, 2, 1, 0, %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 "\n\
memory: X_Alien, 3, 1, 0, 0, 0, 0, 0\n\
memory: MemAcc, 4, 0, 0, %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 "\n\
memory: Rollover, 5, 0, 0, 0, 0, 0, 0\n\
memory: SharedHeader, 6, 0, 0, %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 "\n";

	/* ActiveMemoryAccount should be Top at this point */
	MemoryAccount *newAccount = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, ActiveMemoryAccount);

	void * dummy1 = palloc(NEW_ALLOC_SIZE);
	void * dummy2 = palloc(NEW_ALLOC_SIZE);

	MemoryAccounting_SwitchAccount(newAccount);

	void * dummy3 = palloc(NEW_ALLOC_SIZE);

	pfree(dummy1);
	pfree(dummy2);
	pfree(dummy3);

    MemoryAccounting_SaveToLog();

	char		buf[MAX_OUTPUT_BUFFER_SIZE];
	snprintf(buf, sizeof(buf), templateString,
			MemoryAccountingPeakBalance, MemoryAccountingPeakBalance, MemoryAccountingPeakBalance,
			TopMemoryAccount->peak, TopMemoryAccount->allocated, TopMemoryAccount->freed, TopMemoryAccount->allocated - TopMemoryAccount->freed,
			newAccount->peak, newAccount->allocated, newAccount->freed, newAccount->allocated - newAccount->freed,
			MemoryAccountMemoryAccount->peak, MemoryAccountMemoryAccount->allocated, MemoryAccountMemoryAccount->freed, MemoryAccountMemoryAccount->allocated - MemoryAccountMemoryAccount->freed,
			SharedChunkHeadersMemoryAccount->peak, SharedChunkHeadersMemoryAccount->allocated, SharedChunkHeadersMemoryAccount->freed, SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed);

    assert_true(strcmp(outputBuffer.data, buf) == 0);

    pfree(newAccount);
}

/*
 * Tests if the MemoryAccounting_SaveToFile is generating the correct
 * string representation of memory accounting tree.
 *
 * Note: we don't test the exact file saving here.
 */
void
test__MemoryAccounting_SaveToFile__GeneratesCorrectString(void **state)
{
	MemoryOwnerType newAccountOwnerType = MEMORY_OWNER_TYPE_Exec_Hash;

	/* ActiveMemoryAccount should be Top at this point */
	MemoryAccount *newAccount = CreateMemoryAccountImpl(0, newAccountOwnerType, ActiveMemoryAccount);

	void * dummy1 = palloc(NEW_ALLOC_SIZE);
	void * dummy2 = palloc(NEW_ALLOC_SIZE);

	MemoryAccounting_SwitchAccount(newAccount);

	void * dummy3 = palloc(NEW_ALLOC_SIZE);

	pfree(dummy1);
	pfree(dummy2);
	pfree(dummy3);

	will_return(GetCurrentStatementStartTimestamp, 999);

	memory_profiler_run_id = "test";
	memory_profiler_dataset_id = "unittest";
	memory_profiler_query_id = "q";
	memory_profiler_dataset_size = INT_MAX;

	GpIdentity.segindex = CHAR_MAX;

	MemoryAccounting_SaveToFile(10 /* Arbitrary slice id */);

	char *token = strtok(outputBuffer.data, "\n");
	int lineNo = 0;

	int memoryOwnerTypes[] = {-1, -2, 10, 101, 1029, 1040, 13, 12, 11};

	char runId[80];
	char datasetId[80];
	char queryId[80];
	int datasetSize = 0;
	int stmtMem = 0;
	int sessionId = 0;
	int stmtTimestamp = 0;
	int sliceId = 0;
	int segIndex = 0;
	int ownerType = 0;
	int childSerial = 0;
	int parentSerial = 0;
	int quota = 0;
	int peak = 0;
	int allocated = 0;
	int freed = 0;

	while (token)
	{
		sscanf(token, "%4s,%8s,%1s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
				runId, datasetId, queryId, &datasetSize, &stmtMem, &sessionId, &stmtTimestamp,
				&sliceId, &segIndex, &ownerType, &childSerial, &parentSerial, &quota, &peak, &allocated, &freed);

		/* Ensure that prefix is correctly maintained */
		assert_true(strcmp(runId, memory_profiler_run_id) == 0);
		assert_true(strcmp(datasetId, memory_profiler_dataset_id) == 0);
		assert_true(strcmp(queryId, memory_profiler_query_id) == 0);

		/* Ensure that proper serial of owners was maintained */
		assert_true(ownerType == memoryOwnerTypes[lineNo]);

		if (ownerType ==  MEMORY_STAT_TYPE_VMEM_RESERVED)
		{
			int64 vmem_reserved = VmemTracker_GetMaxReservedVmemBytes();

			assert_true(peak == vmem_reserved && allocated == vmem_reserved && freed == 0);
		}
		else if (ownerType == MEMORY_STAT_TYPE_MEMORY_ACCOUNTING_PEAK)
		{
			assert_true(peak == MemoryAccountingPeakBalance && allocated == MemoryAccountingPeakBalance && freed == 0);
		}
		else if (ownerType == MEMORY_OWNER_TYPE_LogicalRoot)
		{
			assert_true(peak == MemoryAccountTreeLogicalRoot->peak &&
					allocated == MemoryAccountTreeLogicalRoot->allocated && freed == MemoryAccountTreeLogicalRoot->freed);
		}
		else if (ownerType == MEMORY_OWNER_TYPE_Top)
		{
			assert_true(peak == TopMemoryAccount->peak && allocated == TopMemoryAccount->allocated && freed == TopMemoryAccount->freed);
		}
		else if (ownerType ==newAccountOwnerType)
		{
			/* Verify allocated and peak, but don't verify freed, as freed will be after MemoryAccounting_SaveToFile is finished */
			assert_true(peak == newAccount->peak && allocated == newAccount->allocated);
		}
		else if (ownerType == MEMORY_OWNER_TYPE_Exec_AlienShared)
		{
			assert_true(peak == AlienExecutorMemoryAccount->peak &&
					allocated == AlienExecutorMemoryAccount->allocated && freed == AlienExecutorMemoryAccount->freed);
		}
		else if (ownerType == MEMORY_OWNER_TYPE_MemAccount)
		{
			assert_true(peak == MemoryAccountMemoryAccount->peak &&
					allocated == MemoryAccountMemoryAccount->allocated && freed == MemoryAccountMemoryAccount->freed);
		}
		else if (ownerType == MEMORY_OWNER_TYPE_Rollover)
		{
			assert_true(peak == RolloverMemoryAccount->peak &&
					allocated == RolloverMemoryAccount->allocated && freed == RolloverMemoryAccount->freed);
		}
		else if (ownerType == MEMORY_OWNER_TYPE_SharedChunkHeader)
		{
			/* SharedChunkHeadersMemoryAccount was also changed after the output was generated. So, don't compare the freed field */
			assert_true(peak == SharedChunkHeadersMemoryAccount->peak &&
					allocated == SharedChunkHeadersMemoryAccount->allocated);
		}
		else
		{
			assert_true(false);
		}

		token = strtok(NULL, "\n");

		lineNo++;
	}
    pfree(newAccount);
}

/*
 * Tests basic functionality of accounting allocators to record and
 * retrieve allocation information from the active memory account.
 */
void
test__MemoryAccounting__AccountedAllocators(void **state)
{
	const size_t allocation_size = 30;
	size_t allocated = 0;
	void *ptr1, *ptr2;

	/* Switch to a new memory account */
	START_MEMORY_ACCOUNT(MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Optimizer));

	/* Reset the account for calculations */
	MemoryAccounting_Reset();
	assert_true(MemoryAccounting_GetBalance(ActiveMemoryAccount) == 0);

	/* Check if we account for allocation in the account balance */
	ptr1 = gp_accounted_malloc(allocation_size);
	allocated += UserPtr_GetVmemPtrSize(UserPtrToAccountedAllocPtr(ptr1));
	assert_true(MemoryAccounting_GetBalance(ActiveMemoryAccount) == allocated);

	/* Allocate some more */
	ptr2 = gp_accounted_malloc(2 * allocation_size);
	allocated += UserPtr_GetVmemPtrSize(UserPtrToAccountedAllocPtr(ptr2));
	assert_true(MemoryAccounting_GetBalance(ActiveMemoryAccount) == allocated);

	gp_accounted_free(ptr1);
	gp_accounted_free(ptr2);

	/* Check if we accounted for all allocated memory freed in account balance */
	assert_true(MemoryAccounting_GetBalance(ActiveMemoryAccount) == 0);
	/* Check if we accounted peak allocation in the account peak */
	assert_true(MemoryAccounting_GetPeak(ActiveMemoryAccount) == allocated);

	END_MEMORY_ACCOUNT();
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test_setup_teardown(test__CreateMemoryAccountImpl__ActiveVsParent, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__CreateMemoryAccountImpl__TreeStructure, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__CreateMemoryAccountImpl__AutoDetectParent, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__CreateMemoryAccountImpl__AccountProperties, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__CreateMemoryAccountImpl__TracksMemoryOverhead, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__CreateMemoryAccountImpl__AllocatesOnlyFromMemoryAccountMemoryContext, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_SwitchAccount__AccountIsSwitched, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_SwitchAccount__RequiresNonNullAccount, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccountIsValid__ProperValidation, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Reset__TreeStructure, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Reset__ReusesLongLivingAccounts, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Reset__RecreatesShortLivingAccounts, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Reset__ReusesMemoryAccountMemoryContext, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Reset__ResetsMemoryAccountMemoryContext, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Reset__TopIsActive, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Reset__AdvancesGeneration, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Reset__TransferRemainingToRollover, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Reset__ResetPeakBalance, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_AdvanceMemoryAccountingGeneration__AdvancesGeneration, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_AdvanceMemoryAccountingGeneration__SetsActiveToRollover, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_AdvanceMemoryAccountingGeneration__MigratesChunkHeaders, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_AdvanceMemoryAccountingGeneration__PreservesChunkHeader, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_AdvanceMemoryAccountingGeneration__TransfersBalanceToRolloverWithMigration, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_AdvanceMemoryAccountingGeneration__TransfersBalanceToRolloverWithoutMigration, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryContextReset__ResetsSharedChunkHeadersMemoryAccountBalance, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Serialize_Deserialize__Validate, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_GetAccountName__Validate, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_GetPeak__Validate, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_GetBalance__Validate, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_ToString__Validate, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_SaveToLog__GeneratesCorrectString, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_SaveToFile__GeneratesCorrectString, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting__AccountedAllocators, SetupMemoryDataStructures, TeardownMemoryDataStructures),
	};

	return run_tests(tests);
}
