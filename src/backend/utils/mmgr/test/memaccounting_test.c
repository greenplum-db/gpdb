#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"

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
int elevel;

static void elog_mock(int elevel, const char *fmt,...);
static void write_stderr_mock(const char *fmt,...);

/* We will capture write_stderr output using write_stderr_mock */
#define write_stderr write_stderr_mock

#define elog elog_mock

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

#include "utils/memaccounting_private.h"

#define EXPECT_EXCEPTION()     \
	expect_any(ExceptionalCondition,conditionName); \
	expect_any(ExceptionalCondition,errorType); \
	expect_any(ExceptionalCondition,fileName); \
	expect_any(ExceptionalCondition,lineNumber); \
    will_be_called_with_sideeffect(ExceptionalCondition, &_ExceptionalCondition, NULL);\

static void
elog_mock(int passed_elevel, const char *fmt, ...)
{
    va_list		ap;

    fmt = _(fmt);

    va_start(ap, fmt);

	char		buf[2048];

	vsnprintf(buf, sizeof(buf), fmt, ap);

	appendStringInfo(&outputBuffer, buf);

	elevel = passed_elevel;

	va_end(ap);
}

/*
 * Mocks the function write_stderr and captures the output in
 * the global outputBuffer
 */
static void
write_stderr_mock(const char *fmt, ...)
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

	elevel = INT_MAX;
}

/*
 * This method cleans up MemoryContext tree and
 * the MemoryAccount data structures.
 */
void
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

			MemoryAccountIdType newAccountId = CreateMemoryAccountImpl(curLimit, curOwnerType, ActiveMemoryAccountId);

			MemoryAccount* newAccount = MemoryAccounting_ConvertIdToAccount(newAccountId);
			/*
			 * Make sure we create account with valid tag, desired
			 * ownerType and provided quota limit
			 */
			assert_true(MemoryAccounting_IsLiveAccount(newAccountId));
			assert_true(newAccount->ownerType == curOwnerType);
			assert_true(newAccount->maxLimit == curLimit);
			/*
			 * We did not create any of the basic accounts (e.g., TopMemoryAccount,
			 * RolloverMemoryAccount etc.). Therefore, the parent should be the
			 * one we provided.
			 */
			assert_true(newAccount->parentId == ActiveMemoryAccountId);

			assert_true(0 == newAccount->allocated && 0 == newAccount->freed && 0 == newAccount->peak);
		}
	}
}

/*
 * Checks the separation between active and parent account. Typically
 * they are same, but we want to test the general case where they
 * are different.
 */
void
test__CreateMemoryAccountImpl__ActiveVsParent(void **state)
{
	MemoryAccountIdType tempParentAccountId = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, ActiveMemoryAccountId);
	assert_true(tempParentAccountId != ActiveMemoryAccountId);

	MemoryAccountIdType tempChildAccountId = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_SeqScan, tempParentAccountId);
	MemoryAccount *tempChildAccount = MemoryAccounting_ConvertIdToAccount(tempChildAccountId);

	/* Make sure we are not blindly using ActiveMemoryAccount */
	assert_true(tempChildAccount->parentId == tempParentAccountId);
	/* And we still have a different ActiveMemoryAccountId */
	assert_true(tempChildAccount->parentId != ActiveMemoryAccountId);
}

void
test__MemoryAccounting_Optimizer_Oustanding_Balance_Rollover(void **state)
{
	MemoryAccountIdType optimizerAccountId = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Optimizer, ActiveMemoryAccountId);
	MemoryAccountIdType tempParentAccountId = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, optimizerAccountId);

	MemoryAccounting_SwitchAccount(optimizerAccountId);
	void *ptr = Ext_OptimizerAlloc(1);
	assert_true(GetOptimizerOutstandingMemoryBalance()==1);

	MemoryAccounting_SwitchAccount(tempParentAccountId);
	MemoryAccount *currentAccount = MemoryAccounting_ConvertIdToAccount(tempParentAccountId);
	assert_true(currentAccount->allocated == 0);

	MemoryAccountIdType optimizerAccountId2 = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Optimizer, tempParentAccountId);
	MemoryAccounting_SwitchAccount(optimizerAccountId2);
	currentAccount = MemoryAccounting_ConvertIdToAccount(optimizerAccountId2);
	assert_true(currentAccount->allocated == GetOptimizerOutstandingMemoryBalance() );

}

/*
 * Checks whether the regular account creation charges the overhead
 * in the MemoryAccountMemoryAccount and SharedChunkHeadersMemoryAccount.
 */
void
test__CreateMemoryAccountImpl__TracksMemoryOverhead(void **state)
{
	MemoryAccount *tempParentAccount = MemoryAccounting_ConvertIdToAccount(
			CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, ActiveMemoryAccountId));


	uint64 prevAllocated = MemoryAccountMemoryAccount->allocated;
	uint64 prevFreed = MemoryAccountMemoryAccount->freed;
	uint64 prevOverallOutstanding = MemoryAccountingOutstandingBalance;

	uint64 prevSharedAllocated = SharedChunkHeadersMemoryAccount->allocated;
	uint64 prevSharedFreed = SharedChunkHeadersMemoryAccount->freed;

	MemoryAccountIdType tempChildAccountId = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, tempParentAccount);

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
	 * Now check SharedChunkHeadersMemoryAccount balance increase by ensuring we
	 * cannot reuse an existing sharedChunkHeader.
	 */
	MemoryAccounting_SwitchAccount(tempChildAccountId);
	void * dummy = palloc(1);

	/*
	 * The new allocation cannot share any previous header as we didn't have
	 * any Hash account. So, we should have allocated new shared header, increasing
	 * the balance of SharedChunkHeadersMemoryAccount
	 */
	assert_true(SharedChunkHeadersMemoryAccount->allocated > prevSharedAllocated);
	assert_true(SharedChunkHeadersMemoryAccount->freed == prevSharedFreed);

	pfree(dummy);
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
		CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, ActiveMemoryAccountId);
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
	/*
	 * ActiveMemoryAccountId should be set to Top, but we don't care. We only
	 * want to make sure if switching to an account will work
	 */
	assert_true(ActiveMemoryAccountId != MEMORY_OWNER_TYPE_MemAccount);
	MemoryAccountIdType oldAccountId = MemoryAccounting_SwitchAccount(MEMORY_OWNER_TYPE_MemAccount);
	assert_true(ActiveMemoryAccountId == MEMORY_OWNER_TYPE_MemAccount);
	assert_true(oldAccountId == MEMORY_OWNER_TYPE_Top);
	/* We are still in first gen. So, even for short-living account "Top" its id should equal to its enum */
	MemoryAccounting_SwitchAccount(MEMORY_OWNER_TYPE_Top);
	assert_true(ActiveMemoryAccountId == MEMORY_OWNER_TYPE_Top);
}

/*
 * This method tests whether MemoryAccountIsValid correctly
 * determines the validity of the input account.
 */
void
test__MemoryAccountIsValid__ProperValidation(void **state)
{
	int shortLivingCount = shortLivingMemoryAccountArray->accountCount;
	/* The initialization already should populate at least Top memory account */
	assert_true(NULL != shortLivingMemoryAccountArray);
	assert_true(shortLivingMemoryAccountArray->accountCount > 0);

	/* Create some short accounts */
	MemoryAccountIdType extraShortAccountId1 = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Exec_Append);
	MemoryAccountIdType extraShortAccountId2 = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Exec_Hash);
	MemoryAccountIdType extraShortAccountId3 = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Exec_SeqScan);

	MemoryAccountArray* accountArray = shortLivingMemoryAccountArray;

	/* Test when no short living account exists */
	shortLivingMemoryAccountArray = NULL;
	uint64 oldNext = nextAccountId;
	/* Bypass the assertion that live is same as next if no short account */
	nextAccountId = liveAccountStartId;

	assert_true(MemoryAccounting_IsLiveAccount(MEMORY_OWNER_TYPE_END_LONG_LIVING));
	/*
	 * If shortLivingMemoryAccountArray is NULL, any ID beyond
	 * MEMORY_OWNER_TYPE_END_LONG_LIVING will be considered invalid
	 */
	assert_false(MemoryAccounting_IsLiveAccount(MEMORY_OWNER_TYPE_END_LONG_LIVING + 1));

	/* Restoring shortLivingAccountArray to check for valid accounts */
	shortLivingMemoryAccountArray = accountArray;
	nextAccountId = oldNext;

	assert_true(MemoryAccounting_IsLiveAccount(MEMORY_OWNER_TYPE_Rollover));
	/* liveAccountStartId should be the id of Top account */
	assert_true(MemoryAccounting_IsLiveAccount(liveAccountStartId));
	assert_true(MemoryAccounting_IsLiveAccount(MEMORY_OWNER_TYPE_END_LONG_LIVING + shortLivingMemoryAccountArray->accountCount - 1));

	/* Check for a false return for boundary */
	assert_false(MemoryAccounting_IsLiveAccount(
			MEMORY_OWNER_TYPE_END_LONG_LIVING + shortLivingMemoryAccountArray->accountCount +
			1 /* Reserved 1 for invalid */));
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
	MemoryAccount *oldLogicalRoot = MemoryAccounting_ConvertIdToAccount(MEMORY_OWNER_TYPE_LogicalRoot);
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
	assert_true(MemoryAccounting_IsLiveAccount(MEMORY_OWNER_TYPE_LogicalRoot));
	assert_true(MemoryAccounting_IsLiveAccount(MEMORY_OWNER_TYPE_SharedChunkHeader));
	assert_true(MemoryAccounting_IsLiveAccount(MEMORY_OWNER_TYPE_Rollover));
	assert_true(MemoryAccounting_IsLiveAccount(MEMORY_OWNER_TYPE_MemAccount));

	MemoryAccount *root = MemoryAccounting_ConvertIdToAccount(MEMORY_OWNER_TYPE_LogicalRoot);
	/* MemoryAccountTreeLogicalRoot should be reused (i.e., MemoryAccountTreeLogicalRoot will survive the reset) */
	assert_true(oldLogicalRoot && root &&
			root->maxLimit == ULONG_LONG_MAX);

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
 * basic accounts such as TopMemoryAccount
 */
void
test__MemoryAccounting_Reset__RecreatesShortLivingAccounts(void **state)
{
	MemoryAccount *topAccount = MemoryAccounting_ConvertIdToAccount(liveAccountStartId);
	/* The first account should be Top */
	assert_true(topAccount->ownerType == MEMORY_OWNER_TYPE_Top);
	MemoryAccount *oldTop = topAccount;
	MemoryAccountIdType extraShortAccountId = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Exec_Append);
	assert_true(shortLivingMemoryAccountArray->accountCount == 2);

	/*
	 * We want to make sure that the reset process re-creates
	 * short-living accounts. A pointer comparison is not safe,
	 * as pointers may be same, even for newly created accounts.
	 * Therefore, we are marking old accounts with special maxLimit,
	 * so that we can identify if we are reusing existing accounts
	 * or creating new ones.
	 */
	oldTop->maxLimit = ULONG_LONG_MAX;

	MemoryAccountIdType oldLiveStart = liveAccountStartId;

	MemoryAccounting_Reset();
	assert_true(oldLiveStart + 2 == liveAccountStartId);

	MemoryAccount *newTop = MemoryAccounting_ConvertIdToAccount(liveAccountStartId);
	assert_true(newTop->ownerType == MEMORY_OWNER_TYPE_Top);
	assert_true(oldTop != newTop || newTop->maxLimit != ULONG_LONG_MAX);
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
		MemoryAccount *tempAccount = MemoryAccounting_ConvertIdToAccount(CreateMemoryAccountImpl
				(0, MEMORY_OWNER_TYPE_Exec_Hash, ActiveMemoryAccountId));
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
	MemoryAccountIdType newActiveAccountId = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, ActiveMemoryAccountId);

	MemoryAccountIdType oldActiveAccountId = MemoryAccounting_SwitchAccount(newActiveAccountId);
	assert_true(ActiveMemoryAccountId == newActiveAccountId);

	MemoryAccounting_Reset();
	MemoryAccount* top = MemoryAccounting_ConvertIdToAccount(liveAccountStartId);
	assert_true(MEMORY_OWNER_TYPE_Top == top->ownerType && ActiveMemoryAccountId == top->id);
}

/*
 * Tests if the MemoryAccounting_Reset() advances memory account
 * generation
 */
void
test__MemoryAccounting_Reset__AdvancesGeneration(void **state)
{
	MemoryAccountIdType liveStart = liveAccountStartId;

	MemoryAccountIdType extraShortAccountId = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Exec_Append);
	MemoryAccounting_IsLiveAccount(extraShortAccountId);

	MemoryAccountIdType nextId = nextAccountId;
	assert_true(liveStart + 1 < nextId);

	MemoryAccounting_Reset();

	/* We already created top */
	assert_true(liveAccountStartId == nextId && liveAccountStartId + 1 == nextAccountId);
	/* Previous short account is no longer valid */
	assert_false(MemoryAccounting_IsLiveAccount(extraShortAccountId));
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
 * Tests if the MemoryAccounting_GetAccountCurrentBalance() returns the current
 * balance of an account
 */
void
test__MemoryAccounting_GetAccountCurrentBalance__ResetPeakBalance(void **state)
{
	MemoryAccountIdType newAccountId = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Exec_Hash);
	MemoryAccountIdType oldAccountId = MemoryAccounting_SwitchAccount(newAccountId);

	/* Tiny alloc that requires a new SharedChunkHeader, due to a new ActiveMemoryAccount */
	assert_true(MemoryAccounting_GetAccountCurrentBalance(newAccountId) == 0);
	void *testAlloc = palloc(sizeof(int));
	MemoryAccount *memoryAccount = MemoryAccounting_ConvertIdToAccount(newAccountId);
	assert_true(MemoryAccounting_GetAccountCurrentBalance(newAccountId) == memoryAccount->allocated - memoryAccount->freed);

	pfree(testAlloc);
	assert_true(MemoryAccounting_GetAccountCurrentBalance(newAccountId) == 0);
	MemoryAccounting_SwitchAccount(oldAccountId);
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
 * sets RolloverMemoryAccount as the ActiveMemoryAccount
 */
void
test__MemoryAccounting_AdvanceMemoryAccountingGeneration__SetsActiveToRollover(void **state)
{
	MemoryAccounting_SwitchAccount(liveAccountStartId);
	assert_true(MemoryAccounting_ConvertIdToAccount(ActiveMemoryAccountId)->ownerType == MEMORY_OWNER_TYPE_Top);

	AdvanceMemoryAccountingGeneration();

	assert_true(ActiveMemoryAccountId == MEMORY_OWNER_TYPE_Rollover);
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
	MemoryAccountIdType oldAccountId = MemoryAccounting_SwitchAccount(MEMORY_OWNER_TYPE_Exec_AlienShared);
	MemoryContext newContext = AllocSetContextCreate(TopMemoryContext,
										   "TestContext",
										   ALLOCSET_DEFAULT_MINSIZE,
										   ALLOCSET_DEFAULT_INITSIZE,
										   ALLOCSET_DEFAULT_MAXSIZE);

	MemoryAccounting_SwitchAccount(oldAccountId);

	MemoryContext oldContext = MemoryContextSwitchTo(newContext);

	/*
	 * Record the balance right after the new context creation. Note: the
	 * context creation itself might increase the balance of the
	 * SharedChunkHeadersMemoryAccount
	 */
	int64 initialSharedHeaderBalance = SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed;

	/* This would trigger a new shared header with ActiveMemoryAccountId */
	void *testAlloc1 = palloc(sizeof(int));

	SharedChunkHeader *sharedHeader = ((AllocSet)newContext)->sharedHeaderList;

	/* Make sure we got the right memory account */
	assert_true(sharedHeader->memoryAccountId == ActiveMemoryAccountId);

	/* Make sure we did adjust SharedChunkHeadersMemoryAccount balance */
	assert_true(initialSharedHeaderBalance <
		(SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed));

	int64 prevSharedHeaderBalance = SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed;

	/* This would *not* trigger a new shared	 header (reuse header) */
	void *testAlloc2 = palloc(sizeof(int));

	/* Make sure no shared header balance is increased */
	assert_true(prevSharedHeaderBalance ==
			(SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed));

	/* We need a new active account to make sure that we are forcing a new SharedChunkHeader */
	MemoryAccountIdType newAccountId = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Exec_Hash);

	oldAccountId = MemoryAccounting_SwitchAccount(newAccountId);

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

static void
ValidateSerializedAccountArray(const char* bytes, uint32 totalSerialized, uint32 expectedSerialized) {
    assert_true(totalSerialized == expectedSerialized);

	MemoryAccount* combinedAccountArray = (MemoryAccount*) bytes;

	for (int i = 0; i < MEMORY_OWNER_TYPE_END_LONG_LIVING; i++)
	{
		MemoryAccount serializedAccount = combinedAccountArray[i];
		/* Serialization throws out the reserved MEMORY_OWNER_TYPE_Undefined */
		assert_true(serializedAccount.id == (i + 1));

	}
	assert_true(totalSerialized == MEMORY_OWNER_TYPE_END_LONG_LIVING
							+ (nextAccountId - liveAccountStartId));

	/* Make sure we can even verify if the liveAccountStartId is not contiguous (we already had some reset) */
	assert_true(combinedAccountArray[MEMORY_OWNER_TYPE_END_LONG_LIVING].id == liveAccountStartId);
	/* And verify that the end of the account Ids capture the entire set of accounts */
	assert_true(combinedAccountArray[totalSerialized - 1].id == (nextAccountId - 1));

	MemoryAccountIdType prevAccountId = liveAccountStartId;
	for (int i = MEMORY_OWNER_TYPE_END_LONG_LIVING; i < totalSerialized; i++)
	{
		MemoryAccount serializedAccount = combinedAccountArray[i];
		assert_true(MemoryAccounting_IsLiveAccount(serializedAccount.id));

		/* Ensure a set of contiguous account IDs */
		assert_true(i == MEMORY_OWNER_TYPE_END_LONG_LIVING || prevAccountId == (i - 1));
		prevAccountId = i;
	}
}

/* Tests if the serialization of the memory accounting tree is working */
void
test__MemoryAccounting_Serialize__Validate(void **state)
{
	StringInfoData buffer;
    initStringInfo(&buffer);

    uint32 totalSerialized = MemoryAccounting_Serialize(&buffer);

    /* Only top besides the long living accounts */
	ValidateSerializedAccountArray(buffer.data, totalSerialized, MEMORY_OWNER_TYPE_END_LONG_LIVING + 1);

	MemoryAccountIdType newAccount1 = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, liveAccountStartId /* Top */);
	MemoryAccountIdType newAccount2 = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_SeqScan, newAccount1);
	MemoryAccountIdType newAccount3 = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Sort, newAccount1);

	pfree(buffer.data);
	initStringInfo(&buffer);

	totalSerialized = MemoryAccounting_Serialize(&buffer);
    /* Long living accounts + Top + 3 new accounts */
	ValidateSerializedAccountArray(buffer.data, totalSerialized, MEMORY_OWNER_TYPE_END_LONG_LIVING + 1 + 3);

	uint64 prevLiveStart = liveAccountStartId;
	/* Ensure gap of Ids between end of long living and start of short living */
	MemoryAccounting_Reset();

	/* Capture the fact that we have gap between start of short account Ids and end of long account Ids */
	assert_true(prevLiveStart != liveAccountStartId && liveAccountStartId - MEMORY_OWNER_TYPE_END_LONG_LIVING > 1);

	pfree(buffer.data);
	initStringInfo(&buffer);
	totalSerialized = MemoryAccounting_Serialize(&buffer);

    /* Only top besides the long living accounts */
	ValidateSerializedAccountArray(buffer.data, totalSerialized, MEMORY_OWNER_TYPE_END_LONG_LIVING + 1);

	MemoryAccountIdType newAccount4 = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, liveAccountStartId /* Top */);
	MemoryAccountIdType newAccount5 = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_SeqScan, newAccount1);
	MemoryAccountIdType newAccount6 = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Sort, newAccount1);

	pfree(buffer.data);
	initStringInfo(&buffer);
	totalSerialized = MemoryAccounting_Serialize(&buffer);

    /* Long living accounts + Top + 3 new accounts */
	ValidateSerializedAccountArray(buffer.data, totalSerialized, MEMORY_OWNER_TYPE_END_LONG_LIVING + 1 + 3);
	pfree(buffer.data);
}


/*
 * Tests if the MemoryAccounting_CombinedAccountArrayToString of the memory accounting tree
 *  produces expected output
 */
void
test__MemoryAccounting_CombinedAccountArrayToString__Validate(void **state)
{
	StringInfoData serializedBytes;
	initStringInfoOfSize(&serializedBytes, MAX_OUTPUT_BUFFER_SIZE);

	MemoryAccount *topAccount = MemoryAccounting_ConvertIdToAccount(liveAccountStartId);

	MemoryAccountIdType newAccount1 = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, liveAccountStartId /* Top */);
	MemoryAccountIdType newAccount2 = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_SeqScan, newAccount1);
	MemoryAccountIdType newAccount3 = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Sort, newAccount1);

	StringInfoData str;
	initStringInfoOfSize(&str, MAX_OUTPUT_BUFFER_SIZE);

	size_t oldTopBalance = topAccount->allocated - topAccount->freed;
	size_t oldTopPeak = topAccount->peak;

	uint32 totalSerialized = MemoryAccounting_Serialize(&serializedBytes);

	char *templateString = "Root: Peak/Cur 0/0 bytes. Quota: 0 bytes.\n\
  Top: Peak/Cur " UINT64_FORMAT "/" UINT64_FORMAT " bytes. Quota: 0 bytes.\n\
    X_Hash: Peak/Cur 0/0 bytes. Quota: 0 bytes.\n\
      X_Sort: Peak/Cur 0/0 bytes. Quota: 0 bytes.\n\
      X_SeqScan: Peak/Cur 0/0 bytes. Quota: 0 bytes.\n\
  X_Alien: Peak/Cur 0/0 bytes. Quota: 0 bytes.\n\
  MemAcc: Peak/Cur " UINT64_FORMAT "/" UINT64_FORMAT " bytes. Quota: 0 bytes.\n\
  Rollover: Peak/Cur 0/0 bytes. Quota: 0 bytes.\n\
  SharedHeader: Peak/Cur " UINT64_FORMAT "/" UINT64_FORMAT " bytes. Quota: 0 bytes.\n";

	char		buf[MAX_OUTPUT_BUFFER_SIZE];

	snprintf(buf, sizeof(buf), templateString,
			topAccount->peak, topAccount->allocated - topAccount->freed,
			MemoryAccountMemoryAccount->peak, MemoryAccountMemoryAccount->allocated - MemoryAccountMemoryAccount->freed,
			SharedChunkHeadersMemoryAccount->peak, SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed);

	MemoryAccounting_CombinedAccountArrayToString(serializedBytes.data, totalSerialized, &str, 0);

	size_t newTopBalance = topAccount->allocated - topAccount->freed;
	size_t newTopPeak = topAccount->peak;

    assert_true(strcmp(str.data, buf) == 0);

    pfree(serializedBytes.data);
	pfree(str.data);
}


/* Tests if the MemoryOwnerType enum to string conversion is working */
void
test__MemoryAccounting_GetAccountName__Validate(void **state)
{
	char* longLivingNames[] = {"Root", "SharedHeader", "Rollover", "MemAcc", "X_Alien", "RelinquishedPool"};

	char* shortLivingNames[] = {"Top", "Main", "Parser", "Planner", "PlannerHook", "Optimizer", "Dispatcher", "Serializer", "Deserializer",
			"Executor", "X_Result", "X_Append", "X_Sequence", "X_BitmapAnd", "X_BitmapOr", "X_SeqScan", "X_ExternalScan",
			"X_AppendOnlyScan", "X_AOCSCAN", "X_TableScan", "X_DynamicTableScan", "X_IndexScan", "X_DynamicIndexScan", "X_BitmapIndexScan",
			"X_DynamicBitmapIndexScan",
			"X_BitmapHeapScan", "X_BitmapAppendOnlyScan", "X_TidScan", "X_SubqueryScan", "X_FunctionScan", "X_TableFunctionScan",
			"X_ValuesScan", "X_NestLoop", "X_MergeJoin", "X_HashJoin", "X_Material", "X_Sort", "X_Agg", "X_Unique", "X_Hash", "X_SetOp",
			"X_Limit", "X_Motion", "X_ShareInputScan", "X_WindowAgg", "X_Repeat", "X_DML", "X_SplitUpdate", "X_RowTrigger", "X_AssertOp",
			"X_BitmapTableScan", "X_PartitionSelector", "X_RecursiveUnion", "X_CteScan", "X_WorkTableScan"};

	/* Ensure we have all the long living accounts in the longLivingNames array */
	assert_true(sizeof(longLivingNames) / sizeof(char*) == MEMORY_OWNER_TYPE_END_LONG_LIVING);
	for (int longLivingId = MEMORY_OWNER_TYPE_START_LONG_LIVING; longLivingId <= MEMORY_OWNER_TYPE_END_LONG_LIVING; longLivingId++)
	{
		MemoryOwnerType memoryOwnerType = (MemoryOwnerType) longLivingId;
		assert_true(strcmp(MemoryAccounting_GetOwnerName(memoryOwnerType), longLivingNames[longLivingId - 1]) == 0);
	}

	assert_true(sizeof(shortLivingNames) / sizeof(char*) == MEMORY_OWNER_TYPE_END_SHORT_LIVING - MEMORY_OWNER_TYPE_START_SHORT_LIVING + 1);
	for (int shortLivingOwnerTypeId = MEMORY_OWNER_TYPE_START_SHORT_LIVING;
			shortLivingOwnerTypeId <= MEMORY_OWNER_TYPE_END_SHORT_LIVING; shortLivingOwnerTypeId++)
	{
		MemoryOwnerType shortLivingOwnerType = (MemoryOwnerType) shortLivingOwnerTypeId;
		char* name = MemoryAccounting_GetOwnerName(shortLivingOwnerType);
		char* expected = shortLivingNames[shortLivingOwnerTypeId - MEMORY_OWNER_TYPE_START_SHORT_LIVING];
		assert_true(strcmp(name, expected) == 0);
	}
}

/* Tests if the MemoryAccounting_SizeOfAccountInBytes returns correct size */
void
test__MemoryAccounting_SizeOfAccountInBytes__Validate(void **state)
{
	size_t accountSize = MemoryAccounting_SizeOfAccountInBytes();
	assert_true(accountSize == sizeof(MemoryAccount));
}

/* Tests if the MemoryAccounting_GetGlobalPeak returns correct global peak balance */
void
test__MemoryAccounting_GetGlobalPeak__Validate(void **state)
{
	uint64 peak = MemoryAccounting_GetGlobalPeak();
	assert_true(peak == MemoryAccountingPeakBalance);
}

/* Tests if the MemoryAccounting_GetAccountPeakBalance is returning the correct peak balance */
void
test__MemoryAccounting_GetAccountPeakBalance__Validate(void **state)
{
	uint64 peakBalances[] = {0, UINT32_MAX, UINT64_MAX};

	for (int accountIndex = 0; accountIndex < sizeof(peakBalances) / sizeof(uint64); accountIndex++)
	{
		uint64 peakBalance = peakBalances[accountIndex];

		MemoryAccount *newAccount = MemoryAccounting_ConvertIdToAccount(
				CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, ActiveMemoryAccountId));

		newAccount->peak = peakBalance;

		assert_true(MemoryAccounting_GetAccountPeakBalance(newAccount->id) == peakBalance);

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

		MemoryAccount *newAccount = MemoryAccounting_ConvertIdToAccount(
				CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, ActiveMemoryAccountId));

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
"Root: Peak/Cur 0/0 bytes. Quota: 0 bytes.\n\
  Top: Peak/Cur %" PRIu64 "/%" PRIu64 " bytes. Quota: 0 bytes.\n\
    X_Agg: Peak/Cur %" PRIu64 "/%" PRIu64 " bytes. Quota: 0 bytes.\n\
  X_Alien: Peak/Cur 0/0 bytes. Quota: 0 bytes.\n\
  MemAcc: Peak/Cur %" PRIu64 "/%" PRIu64 " bytes. Quota: 0 bytes.\n\
  Rollover: Peak/Cur %" PRIu64 "/%" PRIu64 " bytes. Quota: 0 bytes.\n\
    X_Hash: Peak/Cur 0/0 bytes. Quota: 0 bytes.\n\
  SharedHeader: Peak/Cur %" PRIu64 "/%" PRIu64 " bytes. Quota: 0 bytes.\n";

	MemoryAccountIdType oldAccountId = CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, ActiveMemoryAccountId);
	/* Make oldAccountId obsolete */
	MemoryAccounting_Reset();

	MemoryAccount *rollover = MemoryAccounting_ConvertIdToAccount(MEMORY_OWNER_TYPE_Rollover);
	MemoryAccount *newAccount1 = MemoryAccounting_ConvertIdToAccount(CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, oldAccountId));
	/* Although newAccount has the oldAccount as parent, the serializer will use Rollover as its parent */
	assert_true(newAccount1->parentId == oldAccountId);

	/* ActiveMemoryAccount should be Top at this point */
	MemoryAccount *newAccount2 = MemoryAccounting_ConvertIdToAccount(CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Agg, ActiveMemoryAccountId));

	MemoryAccount *topAccount = MemoryAccounting_ConvertIdToAccount(liveAccountStartId);

	void * dummy1 = palloc(NEW_ALLOC_SIZE);
	void * dummy2 = palloc(NEW_ALLOC_SIZE);

	MemoryAccounting_SwitchAccount(newAccount2->id);

	void * dummy3 = palloc(NEW_ALLOC_SIZE);

	pfree(dummy1);
	pfree(dummy2);
	pfree(dummy3);

	StringInfoData buffer;
	initStringInfoOfSize(&buffer, MAX_OUTPUT_BUFFER_SIZE);

	MemoryAccountTree *tree = ConvertMemoryAccountArrayToTree(&longLivingMemoryAccountArray[MEMORY_OWNER_TYPE_Undefined],
			shortLivingMemoryAccountArray->allAccounts, shortLivingMemoryAccountArray->accountCount);

	MemoryAccounting_ToString(&tree[MEMORY_OWNER_TYPE_LogicalRoot], &buffer, 0 /* Indentation */);

	char		buf[MAX_OUTPUT_BUFFER_SIZE];
	snprintf(buf, sizeof(buf), templateString,
			topAccount->peak, (topAccount->allocated - topAccount->freed), /* Top */
			newAccount2->peak, (newAccount2->allocated - newAccount2->freed), /* X_Agg */
			MemoryAccountMemoryAccount->peak, (MemoryAccountMemoryAccount->allocated - MemoryAccountMemoryAccount->freed), /* MemoryAccountMemoryAccount */
			rollover->peak, (rollover->allocated - rollover->freed), /* Rollover */
			SharedChunkHeadersMemoryAccount->peak, (SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed) /* SharedChunkHeadersMemoryAccount */);

    assert_true(strcmp(buffer.data, buf) == 0);

    pfree(tree);
    pfree(buffer.data);
    pfree(newAccount1);
    pfree(newAccount2);
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
"memory: account_name, account_id, parent_account_id, quota, peak, allocated, freed, current\n\
memory: Vmem, -1, -1, 0, 0, 0, 0, 0\n\
memory: Peak, -2, -2, 0, %" PRIu64 ", %" PRIu64 ", 0, %" PRIu64 "\n\
memory: Root, 1, 1, 0, 0, 0, 0, 0\n\
memory: SharedHeader, 2, 1, 0, %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 "\n\
memory: Rollover, 3, 1, 0, 0, 0, 0, 0\n\
memory: MemAcc, 4, 1, 0, %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 "\n\
memory: X_Alien, 5, 1, 0, 0, 0, 0, 0\n\
memory: RelinquishedPool, 6, 1, 0, 0, 0, 0, 0\n\
memory: Top, 7, 1, 0, %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 "\n\
memory: X_Hash, 8, 7, 0, %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 "\n";

	/* ActiveMemoryAccount should be Top at this point */
	MemoryAccount *newAccount = MemoryAccounting_ConvertIdToAccount(
			CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, ActiveMemoryAccountId));
	MemoryAccount *topAccount = MemoryAccounting_ConvertIdToAccount(liveAccountStartId);

	void * dummy1 = palloc(NEW_ALLOC_SIZE);

	MemoryAccountIdType oldAccountId = MemoryAccounting_SwitchAccount(newAccount->id);

	void * dummy2 = palloc(NEW_ALLOC_SIZE);
	void * dummy3 = palloc(NEW_ALLOC_SIZE);
	MemoryAccounting_SwitchAccount(oldAccountId);

	/* Now free in Hash account, although Hash is no longer Active */
	pfree(dummy3);

    MemoryAccounting_SaveToLog();

	char		buf[MAX_OUTPUT_BUFFER_SIZE];
	/* Hack to discount "tree" free in MemoryAccounting_SaveToLog which we did not count when walked the tree */
	uint64 hackedFreed = topAccount->freed;
	topAccount->freed = 0;

	snprintf(buf, sizeof(buf), templateString,
			MemoryAccountingPeakBalance, MemoryAccountingPeakBalance, MemoryAccountingPeakBalance,
			SharedChunkHeadersMemoryAccount->peak, SharedChunkHeadersMemoryAccount->allocated, SharedChunkHeadersMemoryAccount->freed, SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed,
			MemoryAccountMemoryAccount->peak, MemoryAccountMemoryAccount->allocated, MemoryAccountMemoryAccount->freed, MemoryAccountMemoryAccount->allocated - MemoryAccountMemoryAccount->freed,
			topAccount->peak, topAccount->allocated, topAccount->freed, topAccount->allocated - topAccount->freed,
			newAccount->peak, newAccount->allocated, newAccount->freed, newAccount->allocated - newAccount->freed);

	/* Restore hacked counters */
	topAccount->freed = hackedFreed;

    assert_true(strcmp(outputBuffer.data, buf) == 0);

	pfree(dummy1);
	pfree(dummy2);
    pfree(newAccount);
}

/*
 * Tests if the MemoryAccounting_PrettyPrint is generating the correct
 * string representation of the memory accounting tree before printing.
 */
void
test__MemoryAccounting_PrettyPrint__GeneratesCorrectString(void **state)
{

	char *templateString = "Root: Peak/Cur 0/0 bytes. Quota: 0 bytes.\n\
  Top: Peak/Cur " UINT64_FORMAT "/" UINT64_FORMAT " bytes. Quota: 0 bytes.\n\
    X_Hash: Peak/Cur " UINT64_FORMAT "/" UINT64_FORMAT " bytes. Quota: 0 bytes.\n\
  X_Alien: Peak/Cur 0/0 bytes. Quota: 0 bytes.\n\
  MemAcc: Peak/Cur " UINT64_FORMAT "/" UINT64_FORMAT " bytes. Quota: 0 bytes.\n\
  Rollover: Peak/Cur 0/0 bytes. Quota: 0 bytes.\n\
  SharedHeader: Peak/Cur " UINT64_FORMAT "/" UINT64_FORMAT " bytes. Quota: 0 bytes.\n\n";

	/* ActiveMemoryAccount should be Top at this point */
	MemoryAccount *newAccount = MemoryAccounting_ConvertIdToAccount(
			CreateMemoryAccountImpl(0, MEMORY_OWNER_TYPE_Exec_Hash, ActiveMemoryAccountId));
	MemoryAccount *topAccount = MemoryAccounting_ConvertIdToAccount(liveAccountStartId);

	void * dummy1 = palloc(NEW_ALLOC_SIZE);

	MemoryAccountIdType oldAccountId = MemoryAccounting_SwitchAccount(newAccount->id);

	void * dummy2 = palloc(NEW_ALLOC_SIZE);
	void * dummy3 = palloc(NEW_ALLOC_SIZE);
	MemoryAccounting_SwitchAccount(oldAccountId);

	/* Now free in Hash account, although Hash is no longer Active */
	pfree(dummy3);

	MemoryAccounting_PrettyPrint();

	char		buf[MAX_OUTPUT_BUFFER_SIZE];
	/* Hack to discount "tree" free in MemoryAccounting_SaveToLog which we did not count when walked the tree */
	uint64 hackedFreed = topAccount->freed;
	topAccount->freed = 0;
	snprintf(buf, sizeof(buf), templateString,
			topAccount->peak, topAccount->allocated - topAccount->freed,
			newAccount->peak, newAccount->allocated - newAccount->freed,
			MemoryAccountMemoryAccount->peak, MemoryAccountMemoryAccount->allocated - MemoryAccountMemoryAccount->freed,
			SharedChunkHeadersMemoryAccount->peak, SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed);

	/* Restore hacked counters */
	topAccount->freed = hackedFreed;

    assert_true(strcmp(outputBuffer.data, buf) == 0);

	pfree(dummy1);
	pfree(dummy2);
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
	MemoryAccount *newAccount = MemoryAccounting_ConvertIdToAccount(
			CreateMemoryAccountImpl(0, newAccountOwnerType, ActiveMemoryAccountId));
	MemoryAccount *topAccount = MemoryAccounting_ConvertIdToAccount(liveAccountStartId);

	void * dummy1 = palloc(NEW_ALLOC_SIZE);
	void * dummy2 = palloc(NEW_ALLOC_SIZE);

	MemoryAccounting_SwitchAccount(newAccount->id);

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

	int memoryOwnerTypes[] = {MEMORY_STAT_TYPE_VMEM_RESERVED, MEMORY_STAT_TYPE_MEMORY_ACCOUNTING_PEAK,
			MEMORY_OWNER_TYPE_LogicalRoot, MEMORY_OWNER_TYPE_Top, MEMORY_OWNER_TYPE_Exec_Hash,
			MEMORY_OWNER_TYPE_Exec_RelinquishedPool,
			MEMORY_OWNER_TYPE_Exec_AlienShared,
			MEMORY_OWNER_TYPE_MemAccount, MEMORY_OWNER_TYPE_Rollover,
			MEMORY_OWNER_TYPE_SharedChunkHeader};

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
			MemoryAccount *root = MemoryAccounting_ConvertIdToAccount(MEMORY_OWNER_TYPE_LogicalRoot);
			assert_true(peak == root->peak &&
					allocated == root->allocated && freed == root->freed);
		}
		else if (ownerType == MEMORY_OWNER_TYPE_Top)
		{
			assert_true(peak == topAccount->peak && allocated == topAccount->allocated && freed == topAccount->freed);
		}
		else if (ownerType ==newAccountOwnerType)
		{
			/* Verify allocated and peak, but don't verify freed, as freed will be after MemoryAccounting_SaveToFile is finished */
			assert_true(peak == newAccount->peak && allocated == newAccount->allocated);
		}
		else if (ownerType == MEMORY_OWNER_TYPE_Exec_RelinquishedPool)
		{
			assert_true(peak == RelinquishedPoolMemoryAccount->peak &&
					allocated == RelinquishedPoolMemoryAccount->allocated && freed == RelinquishedPoolMemoryAccount->freed);
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
 * Tests if we correctly convert the short and long living indexes to a combined
 * array index
 */
void
test__ConvertIdToUniversalArrayIndex__Validate(void **state)
{
	// For long living number of short living accounts don't matter to resolve to a compact index
	assert_true(ConvertIdToUniversalArrayIndex(MEMORY_OWNER_TYPE_MemAccount, MEMORY_OWNER_TYPE_START_SHORT_LIVING,
			1) == MEMORY_OWNER_TYPE_MemAccount);

	assert_true(ConvertIdToUniversalArrayIndex(MEMORY_OWNER_TYPE_MemAccount, MEMORY_OWNER_TYPE_START_SHORT_LIVING + 10,
			4) == MEMORY_OWNER_TYPE_MemAccount);

	// For gap of 10 we will still resolve to a compact index
	assert_true(ConvertIdToUniversalArrayIndex(MEMORY_OWNER_TYPE_Top + 10, MEMORY_OWNER_TYPE_START_SHORT_LIVING + 10,
			3) == MEMORY_OWNER_TYPE_Top);

	// For out of range, we should get an error
	//will_be_called(errstart);
	ConvertIdToUniversalArrayIndex(100, MEMORY_OWNER_TYPE_START_SHORT_LIVING + 10, 3);
	assert_true(elevel == ERROR && strcmp(outputBuffer.data, "Cannot map id to array index") == 0);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test_setup_teardown(test__CreateMemoryAccountImpl__ActiveVsParent, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__CreateMemoryAccountImpl__AccountProperties, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__CreateMemoryAccountImpl__TracksMemoryOverhead, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__CreateMemoryAccountImpl__AllocatesOnlyFromMemoryAccountMemoryContext, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_SwitchAccount__AccountIsSwitched, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccountIsValid__ProperValidation, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Reset__ReusesLongLivingAccounts, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Reset__RecreatesShortLivingAccounts, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Reset__ReusesMemoryAccountMemoryContext, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Reset__ResetsMemoryAccountMemoryContext, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Reset__TopIsActive, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Reset__AdvancesGeneration, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Reset__TransferRemainingToRollover, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Reset__ResetPeakBalance, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_AdvanceMemoryAccountingGeneration__SetsActiveToRollover, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_AdvanceMemoryAccountingGeneration__TransfersBalanceToRolloverWithoutMigration, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryContextReset__ResetsSharedChunkHeadersMemoryAccountBalance, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Serialize__Validate, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_GetAccountName__Validate, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_SizeOfAccountInBytes__Validate, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_GetGlobalPeak__Validate, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_GetAccountPeakBalance__Validate, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_GetBalance__Validate, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_ToString__Validate, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_SaveToLog__GeneratesCorrectString, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_SaveToFile__GeneratesCorrectString, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_PrettyPrint__GeneratesCorrectString, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_CombinedAccountArrayToString__Validate, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__ConvertIdToUniversalArrayIndex__Validate, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_GetAccountCurrentBalance__ResetPeakBalance, SetupMemoryDataStructures, TeardownMemoryDataStructures),
		unit_test_setup_teardown(test__MemoryAccounting_Optimizer_Oustanding_Balance_Rollover, SetupMemoryDataStructures, TeardownMemoryDataStructures),
	};

	return run_tests(tests);
}
