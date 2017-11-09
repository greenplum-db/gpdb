/*-------------------------------------------------------------------------
 *
 * memaccounting.c
 *
 * Portions Copyright (c) 2013 EMC Corporation All Rights Reserved
 * Portions Copyright (c) 2013-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/mmgr/memaccounting.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "nodes/memnodes.h"
#include "inttypes.h"
#include "utils/palloc.h"
#include "utils/memutils.h"
#include "nodes/plannodes.h"
#include "cdb/cdbdef.h"
#include "cdb/cdbvars.h"
#include "access/xact.h"
#include "miscadmin.h"
#include "utils/vmem_tracker.h"
#include "utils/memaccounting_private.h"
#include "utils/gp_alloc.h"
#include "utils/ext_alloc.h"

#define MEMORY_REPORT_FILE_NAME_LENGTH 255
#define SHORT_LIVING_MEMORY_ACCOUNT_ARRAY_INIT_LEN 64

/* Saves serializer context info during walking the memory account array */
typedef struct MemoryAccountSerializerCxt
{
	/* How many was serialized in the buffer */
	int memoryAccountCount;
	/* Where to serialize */
	StringInfoData *buffer;
	/* Prefix to add before each line */
	char *prefix;
} MemoryAccountSerializerCxt;

/*
 * A tree representation of the memory account array.
 *
 * Our existing "rendering" algorithms expect a tree. So, a quick way of
 * reusing these functions is to convert the array to a tree.
 */
typedef struct MemoryAccountTree {
	MemoryAccount *account;
	struct MemoryAccountTree *firstChild;
	struct MemoryAccountTree *nextSibling;
} MemoryAccountTree;

/*
 * Account Ids monotonically increases as new accounts are created. Many of these
 * accounts may no longer exist as we drop accounts at the end of each statement.
 * Therefore, we save a marker Id to indicate the start Id of "live" accounts. These
 * live accounts are directly related to the currently executing statement.
 *
 * Note: only short-living accounts are tested with "liveness". Long living accounts
 * have fixed Ids that run between MEMORY_OWNER_TYPE_START_LONG_LIVING and
 * MEMORY_OWNER_TYPE_END_LONG_LIVING and these Ids are smaller than liveAccountStartId
 * but are still considered live for the duration of a QE, across multiple statements
 * and transactions.
 */
MemoryAccountIdType liveAccountStartId = MEMORY_OWNER_TYPE_START_SHORT_LIVING;
/*
 * A monotonically increasing counter to get the next Id at the time of creation of
 * a new short-living account.
 */
MemoryAccountIdType nextAccountId = MEMORY_OWNER_TYPE_START_SHORT_LIVING;

/*
 ******************************************************
 * Internal methods declarations
 */
static void
CheckMemoryAccountingLeak(void);

static void
InitializeMemoryAccount(MemoryAccount *newAccount, long maxLimit,
		MemoryOwnerType ownerType, MemoryAccountIdType parentAccountId);

static MemoryAccountIdType
CreateMemoryAccountImpl(long maxLimit,
		MemoryOwnerType ownerType, MemoryAccountIdType parentId);

typedef CdbVisitOpt (*MemoryAccountVisitor)(MemoryAccountTree *memoryAccountTreeNode,
		void *context, const uint32 depth, uint32 parentWalkSerial,
		uint32 curWalkSerial);

static void
AdvanceMemoryAccountingGeneration(void);

static void
MemoryAccounting_ToString(MemoryAccountTree *root, StringInfoData *str, uint32 indentation);

static void
InitMemoryAccounting(void);

static MemoryAccountTree*
ConvertMemoryAccountArrayToTree(MemoryAccount** longLiving, MemoryAccount** shortLiving, MemoryAccountIdType shortLivingCount);

static CdbVisitOpt
MemoryAccountTreeWalkNode(MemoryAccountTree *memoryAccountTreeNode,
		MemoryAccountVisitor visitor, void *context, uint32 depth,
		uint32 *totalWalked, uint32 parentWalkSerial);

static CdbVisitOpt
MemoryAccountTreeWalkKids(MemoryAccountTree *memoryAccountTreeNode,
		MemoryAccountVisitor visitor, void *context, uint32 depth,
		uint32 *totalWalked, uint32 parentWalkSerial);

static CdbVisitOpt
MemoryAccountToString(MemoryAccountTree *memoryAccount, void *context,
		uint32 depth, uint32 parentWalkSerial, uint32 curWalkSerial);

static CdbVisitOpt
MemoryAccountToCSV(MemoryAccountTree *memoryAccount, void *context,
		uint32 depth, uint32 parentWalkSerial, uint32 curWalkSerial);

static void
PsuedoAccountsToCSV(StringInfoData *str, char *prefix);

static void
SaveMemoryBufToDisk(struct StringInfoData *memoryBuf, char *prefix);

static const char*
MemoryAccounting_GetOwnerName(MemoryOwnerType ownerType);

static void
MemoryAccounting_ResetPeakBalance(void);

static uint64
MemoryAccounting_GetBalance(MemoryAccount* memoryAccount);

/*****************************************************************************
 * Global memory accounting variables, some are only visible via memaccounting_private.h
 */

/*
 * ActiveMemoryAccountId is used by memory allocator to record the allocation.
 * However, deallocation uses the allocator information and ignores ActiveMemoryAccount
 */
MemoryAccountIdType ActiveMemoryAccountId = MEMORY_OWNER_TYPE_Undefined;
/* MemoryAccountMemoryAccount saves the memory overhead of memory accounting itself */
MemoryAccount *MemoryAccountMemoryAccount = NULL;

// Array of accounts available
MemoryAccountArray* shortLivingMemoryAccountArray = NULL;
/*
 * Long living accounts live during the entire lifespan of a QE across all the statements
 * and have fixed Ids that run between MEMORY_OWNER_TYPE_START_LONG_LIVING
 * and MEMORY_OWNER_TYPE_END_LONG_LIVING. They are also singleton per-owner. So,
 * only one account exist per-owner type.
 *
 * Note: index 0 is saved for MEMORY_OWNER_TYPE_Undefined
 */
MemoryAccount* longLivingMemoryAccountArray[MEMORY_OWNER_TYPE_END_LONG_LIVING + 1] = {NULL};

/*
 * SharedChunkHeadersMemoryAccount is used to track all the allocations
 * made for shared chunk headers
 */
MemoryAccount *SharedChunkHeadersMemoryAccount = NULL;

/*
 * RolloverMemoryAccount is used during resetting memory accounting to record
 * allocations that were not freed before the reset process
 */
MemoryAccount *RolloverMemoryAccount = NULL;
/*
 * AlienExecutorMemoryAccount is a shared executor node account across all the
 * plan nodes that are not supposed to execute in current slice
 */
MemoryAccount *AlienExecutorMemoryAccount = NULL;

/*
 * RelinqishedPoolMemoryAccount is a shared executor account that tracks amount
 * of additional free memory available for execution
 */
MemoryAccount *RelinquishedPoolMemoryAccount = NULL;

/*
 * Total outstanding (i.e., allocated - freed) memory across all
 * memory accounts, including RolloverMemoryAccount
 */
uint64 MemoryAccountingOutstandingBalance = 0;

/*
 * Peak memory observed across all allocations/deallocations since
 * last MemoryAccounting_Reset() call
 */
uint64 MemoryAccountingPeakBalance = 0;

/******************************************/
/********** Public interface **************/

/*
 * MemoryAccounting_Reset
 *		Resets the memory account. "Reset" in memory accounting means wiping
 *		off all the accounts' balance clean, and transferring the ownership
 *		of all live allocations to the "rollover" account.
 */
void
MemoryAccounting_Reset()
{
	/*
	 * Attempt to reset only if we already have setup memory accounting
	 * and the memory monitoring is ON
	 */
	if (MemoryAccounting_IsInitialized())
	{
		if (gp_dump_memory_usage)
		{
			MemoryAccounting_SaveToFile(currentSliceId);
		}

		/* No one should create child context under MemoryAccountMemoryContext */
		Assert(MemoryAccountMemoryContext->firstchild == NULL);

		AdvanceMemoryAccountingGeneration();
		CheckMemoryAccountingLeak();

		/* Outstanding balance will come from either the rollover or the shared chunk header account */
		Assert((RolloverMemoryAccount->allocated - RolloverMemoryAccount->freed) +
				(SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed) +
				(AlienExecutorMemoryAccount->allocated - AlienExecutorMemoryAccount->freed) ==
						MemoryAccountingOutstandingBalance);
		MemoryAccounting_ResetPeakBalance();
	}

	InitMemoryAccounting();
}

/*
 * MemoryAccounting_DeclareDone
 * 		Increments the RelinquishedPoolMemoryAccount by the difference between the current
 * 		Memory Account's quota and allocated amount.
 * 		This should only be called when a MemoryAccount is certain that it will not
 * 		allocate any more memory
 */
uint64
MemoryAccounting_DeclareDone()
{
	MemoryAccount *currentAccount = MemoryAccounting_ConvertIdToAccount(ActiveMemoryAccountId);
	uint64 relinquished = 0;
	if (currentAccount->maxLimit > 0 && currentAccount->maxLimit > currentAccount->allocated)
	{
		relinquished = currentAccount->maxLimit - currentAccount->allocated;
		RelinquishedPoolMemoryAccount->allocated += relinquished;
		currentAccount->relinquishedMemory = relinquished;
	}

	elog(DEBUG2, "Memory Account %d relinquished %lu bytes of memory", currentAccount->ownerType, relinquished);
	return relinquished;
}

uint64
MemoryAccounting_RequestQuotaIncrease()
{
	MemoryAccount *currentAccount = MemoryAccounting_ConvertIdToAccount(ActiveMemoryAccountId);

	uint64 result = RelinquishedPoolMemoryAccount->allocated;
	currentAccount->acquiredMemory = result;
	RelinquishedPoolMemoryAccount->allocated = 0;
	return result;
}

/*
 * MemoryAccounting_CreateAccount
 *		Public method to create a memory account. We use this to force outside
 *		world to accept the current memory account as the parent of the new memory
 *		account.
 *
 * maxLimitInKB: The quota information of this account in KB unit
 * ownerType: Owner type (e.g., different executor nodes). The memory
 * 		accounts are hierarchical, so using the tree location we will differentiate
 * 		between owners of same type (e.g., two table scan owners).
 */
MemoryAccountIdType
MemoryAccounting_CreateAccount(long maxLimitInKB, MemoryOwnerType ownerType)
{
	MemoryAccountIdType parentId = MEMORY_OWNER_TYPE_Undefined;

	if (MEMORY_OWNER_TYPE_Undefined == ActiveMemoryAccountId)
	{
		/*
		 * If there is no active account, assign logical root as the parent.
		 * This means the logical root will have itself as parent.
		 */
		parentId = MEMORY_OWNER_TYPE_LogicalRoot;
	}
	else
	{
		parentId = ActiveMemoryAccountId;
	}
	return CreateMemoryAccountImpl(maxLimitInKB * 1024, ownerType, parentId);
}

/*
 * MemoryAccounting_SwitchAccount
 *		Switches the memory account. From this point on, any allocation will
 *		be charged to this new account. Note: we always charge deallocation to
 *		the owner of the memory who originally allocated that memory.
 *
 * desiredAccount: The account to switch to.
 */
MemoryAccountIdType
MemoryAccounting_SwitchAccount(MemoryAccountIdType desiredAccountId)
{
	/*
	 * We cannot validate the actual account pointer as there is no guarantee
	 * that we will have live accounts all the time. We will dynamically
	 * switch to Rollover for dead accounts. But, for now, we at least
	 * assert that the passed accountId was "ever" created.
	 *
	 * Note: this assumes no overflow in the nextAccountId. For now, we don't have
	 * overflow. But, in the future if we implement overflow for our 64 bit id
	 * counter, we will need to get rid of this Assert or be more smart about it
	 */
	Assert(desiredAccountId < nextAccountId);
	MemoryAccountIdType oldAccountId = ActiveMemoryAccountId;

	ActiveMemoryAccountId = desiredAccountId;
	return oldAccountId;
}

/*
 * MemoryAccounting_SizeOfAccountInBytes
 *		Returns the number of bytes needed to hold a memory account.
 *
 *		This method is useful to "outside" world to allocate appropriate buffer to
 *		hold a memory account without having access to MemoryAccount struct.
 */
size_t
MemoryAccounting_SizeOfAccountInBytes()
{
	return sizeof(MemoryAccount);
}

/*
 * MemoryAccounting_GetAccountPeakBalance
 *		Returns peak memory usage of an owner
 *
 * memoryAccountId: The concerned account.
 */
uint64
MemoryAccounting_GetAccountPeakBalance(MemoryAccountIdType memoryAccountId)
{
	return MemoryAccounting_ConvertIdToAccount(memoryAccountId)->peak;
}

/*
 * MemoryAccounting_GetAccountCurrentBalance
 *		Returns current memory usage of an owner
 *
 * memoryAccountId: The concerned account.
 */
uint64
MemoryAccounting_GetAccountCurrentBalance(MemoryAccountIdType memoryAccountId)
{
	MemoryAccount *account = MemoryAccounting_ConvertIdToAccount(memoryAccountId);
	return account->allocated - account->freed;
}

/*
 * MemoryAccounting_GetBalance
 *		Returns current outstanding balance
 *
 * memoryAccount: The concerned account.
 */
uint64
MemoryAccounting_GetBalance(MemoryAccount* memoryAccount)
{
	return memoryAccount->allocated - memoryAccount->freed;
}

/*
 * MemoryAccounting_GetGlobalPeak
 *		Returns global peak balance across all accounts
 */
uint64
MemoryAccounting_GetGlobalPeak()
{
	return MemoryAccountingPeakBalance;
}

/*
 * MemoryAccounting_Serialize
 * 		Serializes the current memory accounting tree into the "buffer"
 */
uint32
MemoryAccounting_Serialize(StringInfoData *buffer)
{
	START_MEMORY_ACCOUNT(MEMORY_OWNER_TYPE_MemAccount);
	{
		/* Ignore undefined account */
		for (MemoryAccountIdType longIdx = MEMORY_OWNER_TYPE_LogicalRoot; longIdx <= MEMORY_OWNER_TYPE_END_LONG_LIVING; longIdx++)
		{
			MemoryAccount *longLivingAccount = MemoryAccounting_ConvertIdToAccount(longIdx);
			appendBinaryStringInfo(buffer, longLivingAccount, sizeof(MemoryAccount));
		}

		for (int i = 0; i < shortLivingMemoryAccountArray->accountCount; i++)
		{
			MemoryAccount* shortLivingAccount = shortLivingMemoryAccountArray->allAccounts[i];
			appendBinaryStringInfo(buffer, shortLivingAccount, sizeof(MemoryAccount));
		}
	}
	END_MEMORY_ACCOUNT();
	return MEMORY_OWNER_TYPE_END_LONG_LIVING + shortLivingMemoryAccountArray->accountCount;
}

/*
 * MemoryAccounting_PrettyPrint
 *    Prints (using elog-WARNING) the current memory accounting tree. Useful debugging
 *    tool from inside gdb.
 */
void
MemoryAccounting_PrettyPrint()
{
	StringInfoData memBuf;
	initStringInfo(&memBuf);

	MemoryAccountTree *tree = ConvertMemoryAccountArrayToTree(&longLivingMemoryAccountArray[MEMORY_OWNER_TYPE_Undefined],
			shortLivingMemoryAccountArray->allAccounts, shortLivingMemoryAccountArray->accountCount);

	MemoryAccounting_ToString(&tree[MEMORY_OWNER_TYPE_LogicalRoot], &memBuf, 0);

	elog(WARNING, "%s\n", memBuf.data);

	pfree(memBuf.data);
}

/*
 * MemoryAccounting_CombinedAccountArrayToString
 *    Converts a unified array of long and short living accounts to string.
 */
void
MemoryAccounting_CombinedAccountArrayToString(void *accountArrayBytes,
		MemoryAccountIdType accountCount, StringInfoData *str, uint32 indentation)
{
	MemoryAccount *combinedArray = (MemoryAccount *) accountArrayBytes;
	/* 1 extra account pointer to reserve for undefined account */
	MemoryAccount **combinedArrayOfPointers = palloc(sizeof(MemoryAccount *) * (accountCount + 1));
	combinedArrayOfPointers[MEMORY_OWNER_TYPE_Undefined] = NULL;

	for (MemoryAccountIdType idx = 0; idx < accountCount; idx++)
	{
		combinedArrayOfPointers[idx + 1] = &combinedArray[idx];
	}
	MemoryAccountTree *tree = ConvertMemoryAccountArrayToTree(&combinedArrayOfPointers[MEMORY_OWNER_TYPE_Undefined],
			&combinedArrayOfPointers[MEMORY_OWNER_TYPE_START_SHORT_LIVING], accountCount - MEMORY_OWNER_TYPE_END_LONG_LIVING);

	MemoryAccounting_ToString(&tree[MEMORY_OWNER_TYPE_LogicalRoot], str, indentation);

	pfree(tree);
	pfree(combinedArrayOfPointers);
}

/*
 * MemoryAccounting_SaveToFile
 *		Saves the current memory accounting tree into a CSV file
 *
 * currentSliceId: The currently executing slice ID
 */
void
MemoryAccounting_SaveToFile(int currentSliceId)
{
	StringInfoData prefix;
	StringInfoData memBuf;
	initStringInfo(&prefix);
	initStringInfo(&memBuf);

	/* run_id, dataset_id, query_id, scale_factor, gp_session_id, current_statement_timestamp, slice_id, segment_idx, */
	appendStringInfo(&prefix, "%s,%s,%s,%u," UINT64_FORMAT ",%d," UINT64_FORMAT ",%d,%d",
			memory_profiler_run_id, memory_profiler_dataset_id, memory_profiler_query_id,
			memory_profiler_dataset_size, GetStatementMem(), gp_session_id, GetCurrentStatementStartTimestamp(),
			currentSliceId, GpIdentity.segindex);

	MemoryAccountTree *tree = ConvertMemoryAccountArrayToTree(&longLivingMemoryAccountArray[MEMORY_OWNER_TYPE_Undefined],
			shortLivingMemoryAccountArray->allAccounts, shortLivingMemoryAccountArray->accountCount);
	PsuedoAccountsToCSV(&memBuf, prefix.data);

	MemoryAccountSerializerCxt cxt;
	cxt.buffer = &memBuf;
	cxt.memoryAccountCount = 0;
	cxt.prefix = prefix.data;

	uint32 totalWalked = 0;

	MemoryAccountTreeWalkNode(&tree[MEMORY_OWNER_TYPE_LogicalRoot], MemoryAccountToCSV, &cxt, 0, &totalWalked, totalWalked);
	SaveMemoryBufToDisk(&memBuf, prefix.data);

	pfree(prefix.data);
	pfree(memBuf.data);
	pfree(tree);
}

/*
 * MemoryAccounting_SaveToLog
 *		Saves the current memory accounting tree in the log.
 */
void
MemoryAccounting_SaveToLog()
{
	int64 vmem_reserved = VmemTracker_GetMaxReservedVmemBytes();

	/* Write the header for the subsequent lines of memory usage information */
	write_stderr("memory: account_name, account_id, parent_account_id, quota, peak, allocated, freed, current\n");

	write_stderr("memory: %s, %d, %d, " UINT64_FORMAT ", " UINT64_FORMAT ", " UINT64_FORMAT ", " UINT64_FORMAT ", " UINT64_FORMAT "\n", "Vmem",
			MEMORY_STAT_TYPE_VMEM_RESERVED /* Id */, MEMORY_STAT_TYPE_VMEM_RESERVED /* Parent Id */,
			(int64) 0 /* Quota */, vmem_reserved /* Peak */, vmem_reserved /* Allocated */, (int64) 0 /* Freed */, vmem_reserved /* Current */);

	write_stderr("memory: %s, %d, %d, " UINT64_FORMAT ", " UINT64_FORMAT ", " UINT64_FORMAT ", " UINT64_FORMAT ", " UINT64_FORMAT "\n", "Peak",
			MEMORY_STAT_TYPE_MEMORY_ACCOUNTING_PEAK /* Id */, MEMORY_STAT_TYPE_MEMORY_ACCOUNTING_PEAK /* Parent Id */,
			(int64) 0 /* Quota */, MemoryAccountingPeakBalance /* Peak */, MemoryAccountingPeakBalance /* Allocated */, (int64) 0 /* Freed */, MemoryAccountingPeakBalance /* Current */);


	/* Write long living accounts */
	for (MemoryAccountIdType longLivingAccountId = MEMORY_OWNER_TYPE_START_LONG_LIVING; longLivingAccountId <= MEMORY_OWNER_TYPE_END_LONG_LIVING; ++longLivingAccountId)
	{
		MemoryAccount *longLivingAccount = MemoryAccounting_ConvertIdToAccount(longLivingAccountId);

		write_stderr("memory: %s, " UINT64_FORMAT ", " UINT64_FORMAT ", " UINT64_FORMAT ", " UINT64_FORMAT ", " UINT64_FORMAT ", " UINT64_FORMAT ", " UINT64_FORMAT "\n", MemoryAccounting_GetOwnerName(longLivingAccount->ownerType),
				longLivingAccount->id /* Id*/, longLivingAccount->parentId /* Parent Id */,
				(int64) longLivingAccount->maxLimit /* Quota */,
				longLivingAccount->peak /* Peak */,
				longLivingAccount->allocated /* Allocated */,
				longLivingAccount->freed /* Freed */, (longLivingAccount->allocated - longLivingAccount->freed) /* Current */);
	}

	/* Write short living accounts, if exists */
	if (NULL != shortLivingMemoryAccountArray)
	{
		for (MemoryAccountIdType shortLivingAccountId = 0; shortLivingAccountId < shortLivingMemoryAccountArray->accountCount; shortLivingAccountId++)
		{
			MemoryAccount* shortLivingAccount = shortLivingMemoryAccountArray->allAccounts[shortLivingAccountId];

			write_stderr("memory: %s, " UINT64_FORMAT ", " UINT64_FORMAT ", " UINT64_FORMAT ", " UINT64_FORMAT ", " UINT64_FORMAT ", " UINT64_FORMAT ", " UINT64_FORMAT "\n", MemoryAccounting_GetOwnerName(shortLivingAccount->ownerType),
					shortLivingAccount->id /* Id */, shortLivingAccount->parentId /* Parent Id */,
					(int64) shortLivingAccount->maxLimit /* Quota */,
					shortLivingAccount->peak /* Peak */,
					shortLivingAccount->allocated /* Allocated */,
					shortLivingAccount->freed /* Freed */, (shortLivingAccount->allocated - shortLivingAccount->freed) /* Current */);
		}
	}
}

/*
 * Get string output of the current Optimizer Memory account. This is used only in
 */
void
MemoryAccounting_ExplainAppendCurrentOptimizerAccountInfo(StringInfoData *str)
{

	MemoryAccountIdType shortLivingCount = shortLivingMemoryAccountArray->accountCount;

	for (MemoryAccountIdType shortLivingArrayIdx = 0; shortLivingArrayIdx < shortLivingCount; ++shortLivingArrayIdx)
	{
		MemoryAccount *shortLivingAccount = shortLivingMemoryAccountArray->allAccounts[shortLivingArrayIdx];
		if (shortLivingAccount->ownerType == MEMORY_OWNER_TYPE_Optimizer)
		{
			appendStringInfo(str, "\n  ORCA Memory used: peak %.0fK bytes  allocated %.0fK bytes  freed %.0fK bytes ",
							 ceil((double) shortLivingAccount->peak / 1024L),
							 ceil((double) shortLivingAccount->allocated / 1024L),
							 ceil((double) shortLivingAccount->freed / 1024L));
			break;
		}
	}
}

/*****************************************************************************
 *	  PRIVATE ROUTINES FOR MEMORY ACCOUNTING								 *
 *****************************************************************************/

/* Initializes all the long living accounts */
static void
InitLongLivingAccounts() {
	/*
	 * All the long living accounts are created together, so if logical root
	 * is null, then other long-living accounts should be the null too
	 */
	Assert(SharedChunkHeadersMemoryAccount == NULL && RolloverMemoryAccount == NULL && MemoryAccountMemoryAccount == NULL && AlienExecutorMemoryAccount == NULL);
	Assert(MemoryAccountMemoryContext == NULL);
	/* Ensure we are in TopMemoryContext as we are creating long living accounts that don't die */
	Assert(CurrentMemoryContext == TopMemoryContext);
	for (int longLivingIdx = MEMORY_OWNER_TYPE_LogicalRoot;
			longLivingIdx <= MEMORY_OWNER_TYPE_END_LONG_LIVING;
			longLivingIdx++)
	{
		/* For long living account */
#ifdef USE_ASSERT_CHECKING
		MemoryAccountIdType newAccountId =
#endif
		MemoryAccounting_CreateAccount(0, longLivingIdx);
		Assert(longLivingIdx == newAccountId);
	}
	/* Now set all the global memory accounts */
	SharedChunkHeadersMemoryAccount =
			longLivingMemoryAccountArray[MEMORY_OWNER_TYPE_SharedChunkHeader];
	RolloverMemoryAccount =
			longLivingMemoryAccountArray[MEMORY_OWNER_TYPE_Rollover];
	MemoryAccountMemoryAccount =
			longLivingMemoryAccountArray[MEMORY_OWNER_TYPE_MemAccount];
	AlienExecutorMemoryAccount =
			longLivingMemoryAccountArray[MEMORY_OWNER_TYPE_Exec_AlienShared];
	RelinquishedPoolMemoryAccount =
			longLivingMemoryAccountArray[MEMORY_OWNER_TYPE_Exec_RelinquishedPool];

}

/* Initializes all the short living accounts */
static void
InitShortLivingMemoryAccounts()
{
	Assert(NULL == shortLivingMemoryAccountArray);
	Assert(NULL != MemoryAccountMemoryContext && MemoryAccountMemoryAccount != NULL);

	MemoryContext oldContext = MemoryContextSwitchTo(MemoryAccountMemoryContext);
	MemoryAccountIdType oldAccountId = MemoryAccounting_SwitchAccount((MemoryAccountIdType)MEMORY_OWNER_TYPE_MemAccount);

	shortLivingMemoryAccountArray = palloc0(sizeof(MemoryAccountArray));
	shortLivingMemoryAccountArray->accountCount = 0;
	shortLivingMemoryAccountArray->allAccounts = (MemoryAccount**) palloc0(SHORT_LIVING_MEMORY_ACCOUNT_ARRAY_INIT_LEN * sizeof(MemoryAccount*));
	shortLivingMemoryAccountArray->arraySize = SHORT_LIVING_MEMORY_ACCOUNT_ARRAY_INIT_LEN;

	MemoryAccounting_SwitchAccount(oldAccountId);
	MemoryContextSwitchTo(oldContext);
}

/* Add a long living account to the longLivingMemoryAccountArray */
static void
AddToLongLivingAccountArray(MemoryAccount *newAccount)
{
	Assert(newAccount->id <= MEMORY_OWNER_TYPE_END_LONG_LIVING);
	Assert(NULL == longLivingMemoryAccountArray[newAccount->id]);
	longLivingMemoryAccountArray[newAccount->id] = newAccount;
}

/* Add a short living account to the shortLivingMemoryAccountArray */
static void
AddToShortLivingAccountArray(MemoryAccount *newAccount)
{
	Assert(shortLivingMemoryAccountArray->accountCount == newAccount->id - liveAccountStartId);
	MemoryAccountIdType arraySize = shortLivingMemoryAccountArray->arraySize;
	MemoryAccountIdType needAtleast = shortLivingMemoryAccountArray->accountCount + 1;

	/* If we need more entries in the array, resize the array */
	if (arraySize < needAtleast)
	{
		MemoryAccountIdType newArraySize = arraySize * 2;
		shortLivingMemoryAccountArray->allAccounts = repalloc(shortLivingMemoryAccountArray->allAccounts,
				sizeof(MemoryAccount*) * newArraySize);
		shortLivingMemoryAccountArray->arraySize = newArraySize;

		memset(&shortLivingMemoryAccountArray->allAccounts[shortLivingMemoryAccountArray->accountCount], 0,
				(shortLivingMemoryAccountArray->arraySize - shortLivingMemoryAccountArray->accountCount) * sizeof(MemoryAccount*));
	}

	Assert(shortLivingMemoryAccountArray->arraySize > shortLivingMemoryAccountArray->accountCount);
	Assert(NULL == shortLivingMemoryAccountArray->allAccounts[shortLivingMemoryAccountArray->accountCount]);
	shortLivingMemoryAccountArray->allAccounts[shortLivingMemoryAccountArray->accountCount++] = newAccount;
}

/*
 * InitializeMemoryAccount
 *		Initialize the common data structures of a newly created memory account
 *
 * newAccount: the account to initialize
 * maxLimit: quota of the account (0 means no quota)
 * ownerType: the memory owner type for this account
 * parentAccount: the parent account of this account
 */
static void
InitializeMemoryAccount(MemoryAccount *newAccount, long maxLimit, MemoryOwnerType ownerType, MemoryAccountIdType parentAccountId)
{
	/*
	 * MEMORY_OWNER_TYPE_EXECUTOR_END is set to the last valid executor account type.
	 * I.e., it is inclusive in the valid values of ownerType
	 */
	Assert(ownerType != MEMORY_OWNER_TYPE_Undefined && ownerType <= MEMORY_OWNER_TYPE_EXECUTOR_END);

	newAccount->ownerType = ownerType;

	/*
	 * Maximum targeted allocation for an owner. Peak usage can be
	 * tracked to check if the allocation is overshooting
	 */
	newAccount->maxLimit = maxLimit;

	newAccount->allocated = 0;

	/*
	 * Every call to ORCA to optimize a query maps to a new short living memory
	 * account. However, the nature of Orca's memory usage is that it holds data
	 * in a cache. Thus, GetOptimizerOutstandingMemoryBalance() returns the
	 * current amount of memory that Orca has not yet freed according to the
	 * Memory Accounting framework. Each new Orca memory account will start off
	 * its 'allocated' amount from the outstanding amount. This approach ensures
	 * that when Orca does release memory it allocated during an earlier
	 * generation that the accounting math does not lead to an underflow but
	 * properly accounts for the outstanding amount.
	 */
	if (ownerType == MEMORY_OWNER_TYPE_Optimizer)
	{
		elog(DEBUG2, "Rolling over previous outstanding Optimizer allocated memory %lu", GetOptimizerOutstandingMemoryBalance());
		newAccount->allocated = GetOptimizerOutstandingMemoryBalance();
	}
	newAccount->freed = 0;
	newAccount->peak = 0;
	newAccount->relinquishedMemory = 0;
	newAccount->acquiredMemory = 0;
	newAccount->parentId = parentAccountId;

	if (ownerType <= MEMORY_OWNER_TYPE_END_LONG_LIVING)
	{
		newAccount->id = ownerType;
		AddToLongLivingAccountArray(newAccount);
	}
	else
	{
		newAccount->id = nextAccountId++;
		AddToShortLivingAccountArray(newAccount);
	}
}

/*
 * CreateMemoryAccountImpl
 *		Allocates and initializes a memory account.
 *
 * maxLimit: The quota information of this account
 * ownerType: Gross owner type (e.g., different executor nodes). The memory
 * 		accounts are hierarchical, so using the tree location we will differentiate
 * 		between owners of same gross type (e.g., two sequential scan owners).
 * parent: The parent account of this account.
 */
static MemoryAccountIdType
CreateMemoryAccountImpl(long maxLimit, MemoryOwnerType ownerType, MemoryAccountIdType parentId)
{
	Assert(liveAccountStartId > MEMORY_OWNER_TYPE_END_LONG_LIVING);

	/* We don't touch the oldContext. We create all MemoryAccount in MemoryAccountMemoryContext */
	MemoryContext oldContext = NULL;
	MemoryAccount* newAccount = NULL; /* Return value */
	/* Used for switching temporarily to MemoryAccountMemoryAccount ownership to account for the instrumentation overhead */
	MemoryAccountIdType oldAccountId = MEMORY_OWNER_TYPE_Undefined;

	/*
	 * Rollover is a special MemoryAccount that resides at the
	 * TopMemoryContext, and not under MemoryAccountMemoryContext
	 */
	Assert(ownerType == MEMORY_OWNER_TYPE_LogicalRoot || ownerType == MEMORY_OWNER_TYPE_SharedChunkHeader ||
			ownerType == MEMORY_OWNER_TYPE_Rollover ||
			ownerType == MEMORY_OWNER_TYPE_MemAccount ||
			ownerType == MEMORY_OWNER_TYPE_Exec_AlienShared ||
			ownerType == MEMORY_OWNER_TYPE_Exec_RelinquishedPool ||
			(MemoryAccountMemoryContext != NULL && MemoryAccountMemoryAccount != NULL));

	if (ownerType <= MEMORY_OWNER_TYPE_END_LONG_LIVING || ownerType == MEMORY_OWNER_TYPE_Top)
	{
		/* Set the "logical root" as the parent of all long living accounts */
		parentId = MEMORY_OWNER_TYPE_LogicalRoot;
	}

	/*
	 * Other than logical root and AlienShared, no long-living account should have children
	 * and only logical root is allowed to have no parent
	 */
	Assert((parentId == MEMORY_OWNER_TYPE_LogicalRoot || parentId == MEMORY_OWNER_TYPE_Exec_AlienShared
			|| parentId > MEMORY_OWNER_TYPE_END_LONG_LIVING));

	/*
	 * Only SharedChunkHeadersMemoryAccount, Rollover, MemoryAccountMemoryAccount,
	 * AlienExecutorAccount and Top can be under "logical root"
	 */
	AssertImply(parentId == MEMORY_OWNER_TYPE_LogicalRoot, ownerType <= MEMORY_OWNER_TYPE_END_LONG_LIVING ||
			ownerType == MEMORY_OWNER_TYPE_Top);

	/* Long-living accounts need TopMemoryContext */
	if (ownerType <= MEMORY_OWNER_TYPE_END_LONG_LIVING)
	{
		oldContext = MemoryContextSwitchTo(TopMemoryContext);
	}
	else
	{
		oldContext = MemoryContextSwitchTo(MemoryAccountMemoryContext);
		oldAccountId = MemoryAccounting_SwitchAccount((MemoryAccountIdType)MEMORY_OWNER_TYPE_MemAccount);
	}

	newAccount = makeNode(MemoryAccount);
	InitializeMemoryAccount(newAccount, maxLimit, ownerType, parentId);

	if (oldAccountId != MEMORY_OWNER_TYPE_Undefined)
	{
		MemoryAccounting_SwitchAccount(oldAccountId);
	}

	MemoryContextSwitchTo(oldContext);

	return newAccount->id;
}

/*
 * CheckMemoryAccountingLeak
 *		Checks for leaks (i.e., memory accounts with balance) after everything
 *		is reset.
 */
static void
CheckMemoryAccountingLeak()
{
	/* Just an API. Not yet implemented. */
}

/*
 * Returns a combined array index where the long and short living accounts are together.
 * The first MEMORY_OWNER_TYPE_END_LONG_LIVING number of indices are reserved for long
 * living accounts and short living accounts follow after that.
 */
static MemoryAccountIdType
ConvertIdToUniversalArrayIndex(MemoryAccountIdType id, MemoryAccountIdType liveStartId,
		MemoryAccountIdType shortLivingCount)
{
	if (id >= MEMORY_OWNER_TYPE_LogicalRoot && id <= MEMORY_OWNER_TYPE_END_LONG_LIVING)
	{
		return id;
	}
	else if (id >= liveStartId && id < liveStartId + shortLivingCount)
	{
		return id - liveStartId + MEMORY_OWNER_TYPE_END_LONG_LIVING + 1;
	}
	else
	{
		/* Note, we cannot map ID to rollover here as we expect a live account id */
		elog(ERROR, "Cannot map id to array index");
	}

	/* Keep the compiler happy */
	return MEMORY_OWNER_TYPE_Undefined;
}

static void
AddChild(MemoryAccountTree *treeArray, MemoryAccount *childAccount, MemoryAccount* parentAccount,
		MemoryAccountIdType liveStartId, MemoryAccountIdType shortLivingCount)
{
	MemoryAccountIdType childId = childAccount->id;
	MemoryAccountIdType parentId = parentAccount->id;

	Assert(parentId != MEMORY_OWNER_TYPE_Undefined && childId != MEMORY_OWNER_TYPE_Undefined);
	AssertImply(childId == MEMORY_OWNER_TYPE_LogicalRoot, parentId == MEMORY_OWNER_TYPE_LogicalRoot);

	MemoryAccountIdType childArrayIndex = ConvertIdToUniversalArrayIndex(childId, liveStartId, shortLivingCount);
	MemoryAccountTree *childNode = &treeArray[childArrayIndex];
	childNode->account = childAccount;

	if (childId != MEMORY_OWNER_TYPE_LogicalRoot)
	{
		MemoryAccountIdType parentArrayIndex = ConvertIdToUniversalArrayIndex(parentId, liveStartId, shortLivingCount);

		Assert(parentId == MEMORY_OWNER_TYPE_Undefined || childId == MEMORY_OWNER_TYPE_LogicalRoot ||
				treeArray[parentArrayIndex].account != NULL);

		MemoryAccountTree *parentNode = &treeArray[parentArrayIndex];
		childNode->nextSibling = parentNode->firstChild;
		parentNode->firstChild = childNode;
	}
}

static MemoryAccountTree*
ConvertMemoryAccountArrayToTree(MemoryAccount** longLiving, MemoryAccount** shortLiving, MemoryAccountIdType shortLivingCount)
{
	MemoryAccountTree *treeArray = palloc0(sizeof(MemoryAccountTree) *
			(shortLivingCount + MEMORY_OWNER_TYPE_END_LONG_LIVING + 1));

	/* Ignore "undefined" account in the tree */
	for (MemoryAccountIdType longLivingArrayIdx = MEMORY_OWNER_TYPE_LogicalRoot;
			longLivingArrayIdx <= MEMORY_OWNER_TYPE_END_LONG_LIVING; longLivingArrayIdx++)
	{
		MemoryAccount *longLivingAccount = longLiving[longLivingArrayIdx];
		Assert(longLivingAccount->parentId <= MEMORY_OWNER_TYPE_END_LONG_LIVING);
		MemoryAccount *parentAccount = longLiving[longLivingAccount->parentId];

		/* Long living accounts don't care about liveStartId and shortLivingCount */
		AddChild(treeArray, longLivingAccount, parentAccount, 0 /* liveStartId */, 0 /* shortLivingCount */);
	}

	if (shortLivingCount > 0)
	{
		Assert(NULL != shortLiving);
		MemoryAccountIdType liveStartId = shortLiving[0]->id;
		/* Ensure that the range of IDs for short living accounts are dense */
		Assert(shortLiving[shortLivingCount - 1]->id == liveStartId + shortLivingCount - 1);

		for (MemoryAccountIdType shortLivingArrayIdx = 0; shortLivingArrayIdx < shortLivingCount; shortLivingArrayIdx++)
		{
			MemoryAccount* shortLivingAccount = shortLiving[shortLivingArrayIdx];
			MemoryAccount* parentAccount = NULL;
			if (shortLivingAccount->parentId <= MEMORY_OWNER_TYPE_END_LONG_LIVING)
			{
				parentAccount = longLiving[shortLivingAccount->parentId];
			}
			else if (shortLivingAccount->parentId >= liveStartId)
			{
				parentAccount = shortLiving[shortLivingAccount->parentId - liveStartId];
			}
			else /* Dead parent account. Use Rollover as parent */
			{
				parentAccount = longLiving[MEMORY_OWNER_TYPE_Rollover];
			}

			Assert(NULL != parentAccount);
			AddChild(treeArray, shortLivingAccount, parentAccount, liveStartId, shortLivingCount);
		}
	}

	/* The children adding process shouldn't touch the reserved undefined account tree node */
	Assert(treeArray[MEMORY_OWNER_TYPE_Undefined].account == NULL);
	return treeArray;
}

/**
 * MemoryAccountWalkNode:
 * 		Walks one node of the MemoryAccount tree, and calls MemoryAccountWalkKids
 * 		to walk children recursively (given the walk is sanctioned by the current node).
 * 		For each walk it calls the provided function to do some "processing"
 *
 * memoryAccount: The current memory account to walk
 * visitor: The function pointer that is interested to process this node
 * context: context information to pass between successive "walker" call
 * depth: The depth in the tree for current node
 * totalWalked: An in/out parameter that will reflect how many nodes are walked
 * 		so far. Useful to generate node serial ("walk serial") automatically
 * parentWalkSerial: parent node's "walk serial".
 *
 * NB: totalWalked and parentWalkSerial can be used to generate a pointer
 * agnostic representation of the tree.
 */
static CdbVisitOpt
MemoryAccountTreeWalkNode(MemoryAccountTree *memoryAccountTreeNode, MemoryAccountVisitor visitor,
		void *context, uint32 depth, uint32 *totalWalked, uint32 parentWalkSerial)
{
	CdbVisitOpt     whatnext;

	if (memoryAccountTreeNode == NULL)
	{
		whatnext = CdbVisit_Walk;
	}
	else
	{
		uint32 curWalkSerial = *totalWalked;
		whatnext = visitor(memoryAccountTreeNode, context, depth, parentWalkSerial, *totalWalked);
		*totalWalked = *totalWalked + 1;

		if (whatnext == CdbVisit_Walk)
		{
			whatnext = MemoryAccountTreeWalkKids(memoryAccountTreeNode, visitor, context, depth + 1, totalWalked, curWalkSerial);
		}
		else if (whatnext == CdbVisit_Skip)
		{
			whatnext = CdbVisit_Walk;
		}
	}
	Assert(whatnext != CdbVisit_Skip);
	return whatnext;
}

/**
 * MemoryAccountWalkKids:
 * 		Called from MemoryAccountWalkNode to recursively walk the children
 *
 * memoryAccount: The parent memory account whose children to walk
 * visitor: The function pointer that is interested to process these nodes
 * context: context information to pass between successive "walker" call
 * depth: The depth in the tree for current node
 * totalWalked: An in/out parameter that will reflect how many nodes are walked
 * 		so far. Useful to generate node serial ("walk serial") automatically
 * parentWalkSerial: parent node's "walk serial".
 *
 * NB: totalWalked and parentWalkSerial can be used to generate a pointer
 * agnostic representation of the tree.
 */
static CdbVisitOpt
MemoryAccountTreeWalkKids(MemoryAccountTree *memoryAccountTreeNode,
		MemoryAccountVisitor visitor,
		void           *context, uint32 depth, uint32 *totalWalked, uint32 parentWalkSerial)
{
	if (memoryAccountTreeNode == NULL)
		return CdbVisit_Walk;

	/* Traverse children accounts */
	for (MemoryAccountTree *child = memoryAccountTreeNode->firstChild; child != NULL; child = child->nextSibling)
	{
		MemoryAccountTreeWalkNode(child, visitor, context, depth, totalWalked, parentWalkSerial);
	}

	return CdbVisit_Walk;
}

/**
 * MemoryAccountToString:
 * 		A visitor function that can convert a memory account to string.
 * 		Called repeatedly from the walker to convert the entire tree to string
 * 		through MemoryAccountTreeToString.
 *
 * memoryAccount: The memory account which will be represented as string
 * context: context information to pass between successive function call
 * depth: The depth in the tree for current node. Used to generate indentation.
 * parentWalkSerial: parent node's "walk serial". Not used right now. Part
 * 		of the uniform "walker" function prototype
 * curWalkSerial: current node's "walk serial". Not used right now. Part
 * 		of the uniform "walker" function prototype
 */
static CdbVisitOpt
MemoryAccountToString(MemoryAccountTree *memoryAccountTreeNode, void *context, uint32 depth,
		uint32 parentWalkSerial, uint32 curWalkSerial)
{
	if (memoryAccountTreeNode == NULL) return CdbVisit_Walk;

	MemoryAccount *memoryAccount = memoryAccountTreeNode->account;

	if (memoryAccount->ownerType == MEMORY_OWNER_TYPE_Exec_RelinquishedPool) return CdbVisit_Walk;

	MemoryAccountSerializerCxt *memAccountCxt = (MemoryAccountSerializerCxt*) context;

	appendStringInfoFill(memAccountCxt->buffer, 2 * depth, ' ');

	Assert(memoryAccount->peak >= MemoryAccounting_GetBalance(memoryAccount));
	/* We print only integer valued memory consumption, in standard GPDB KB unit */
	appendStringInfo(memAccountCxt->buffer, "%s: Peak/Cur " UINT64_FORMAT "/" UINT64_FORMAT " bytes. Quota: " UINT64_FORMAT " bytes.",
			MemoryAccounting_GetOwnerName(memoryAccount->ownerType),
			memoryAccount->peak, MemoryAccounting_GetBalance(memoryAccount), memoryAccount->maxLimit);
	if (memoryAccount->relinquishedMemory > 0)
		appendStringInfo(memAccountCxt->buffer, " Relinquished Memory: " UINT64_FORMAT " bytes. ", memoryAccount->relinquishedMemory);
	if (memoryAccount->acquiredMemory > 0)
		appendStringInfo(memAccountCxt->buffer, " Acquired Additional Memory: " UINT64_FORMAT " bytes.", memoryAccount->acquiredMemory);
	appendStringInfo(memAccountCxt->buffer, "\n");

	memAccountCxt->memoryAccountCount++;

	return CdbVisit_Walk;
}

/*
 * MemoryAccounting_GetOwnerName
 *		Returns the human readable name of an owner
 */
static const char*
MemoryAccounting_GetOwnerName(MemoryOwnerType ownerType)
{
	switch (ownerType)
	{
	/* Long living accounts */
	case MEMORY_OWNER_TYPE_LogicalRoot:
		return "Root";
	case MEMORY_OWNER_TYPE_SharedChunkHeader:
		return "SharedHeader";
	case MEMORY_OWNER_TYPE_Rollover:
		return "Rollover";
	case MEMORY_OWNER_TYPE_MemAccount:
		return "MemAcc";
	case MEMORY_OWNER_TYPE_Exec_AlienShared:
		return "X_Alien";
	case MEMORY_OWNER_TYPE_Exec_RelinquishedPool:
		return "RelinquishedPool";

		/* Short living accounts */
	case MEMORY_OWNER_TYPE_Top:
		return "Top";
	case MEMORY_OWNER_TYPE_MainEntry:
		return "Main";
	case MEMORY_OWNER_TYPE_Parser:
		return "Parser";
	case MEMORY_OWNER_TYPE_Planner:
		return "Planner";
	case MEMORY_OWNER_TYPE_PlannerHook:
		return "PlannerHook";
	case MEMORY_OWNER_TYPE_Optimizer:
		return "Optimizer";
	case MEMORY_OWNER_TYPE_Dispatcher:
		return "Dispatcher";
	case MEMORY_OWNER_TYPE_Serializer:
		return "Serializer";
	case MEMORY_OWNER_TYPE_Deserializer:
		return "Deserializer";

	case MEMORY_OWNER_TYPE_EXECUTOR:
		return "Executor";
	case MEMORY_OWNER_TYPE_Exec_Result:
		return "X_Result";
	case MEMORY_OWNER_TYPE_Exec_Append:
		return "X_Append";
	case MEMORY_OWNER_TYPE_Exec_Sequence:
		return "X_Sequence";
	case MEMORY_OWNER_TYPE_Exec_BitmapAnd:
		return "X_BitmapAnd";
	case MEMORY_OWNER_TYPE_Exec_BitmapOr:
		return "X_BitmapOr";
	case MEMORY_OWNER_TYPE_Exec_SeqScan:
		return "X_SeqScan";
	case MEMORY_OWNER_TYPE_Exec_ExternalScan:
		return "X_ExternalScan";
	case MEMORY_OWNER_TYPE_Exec_AppendOnlyScan:
		return "X_AppendOnlyScan";
	case MEMORY_OWNER_TYPE_Exec_AOCSScan:
		return "X_AOCSCAN";
	case MEMORY_OWNER_TYPE_Exec_TableScan:
		return "X_TableScan";
	case MEMORY_OWNER_TYPE_Exec_DynamicTableScan:
		return "X_DynamicTableScan";
	case MEMORY_OWNER_TYPE_Exec_IndexScan:
		return "X_IndexScan";
	case MEMORY_OWNER_TYPE_Exec_DynamicIndexScan:
		return "X_DynamicIndexScan";
	case MEMORY_OWNER_TYPE_Exec_BitmapIndexScan:
		return "X_BitmapIndexScan";
	case MEMORY_OWNER_TYPE_Exec_BitmapHeapScan:
		return "X_BitmapHeapScan";
	case MEMORY_OWNER_TYPE_Exec_BitmapAppendOnlyScan:
		return "X_BitmapAppendOnlyScan";
	case MEMORY_OWNER_TYPE_Exec_TidScan:
		return "X_TidScan";
	case MEMORY_OWNER_TYPE_Exec_SubqueryScan:
		return "X_SubqueryScan";
	case MEMORY_OWNER_TYPE_Exec_FunctionScan:
		return "X_FunctionScan";
	case MEMORY_OWNER_TYPE_Exec_TableFunctionScan:
		return "X_TableFunctionScan";
	case MEMORY_OWNER_TYPE_Exec_ValuesScan:
		return "X_ValuesScan";
	case MEMORY_OWNER_TYPE_Exec_NestLoop:
		return "X_NestLoop";
	case MEMORY_OWNER_TYPE_Exec_MergeJoin:
		return "X_MergeJoin";
	case MEMORY_OWNER_TYPE_Exec_HashJoin:
		return "X_HashJoin";
	case MEMORY_OWNER_TYPE_Exec_Material:
		return "X_Material";
	case MEMORY_OWNER_TYPE_Exec_Sort:
		return "X_Sort";
	case MEMORY_OWNER_TYPE_Exec_Agg:
		return "X_Agg";
	case MEMORY_OWNER_TYPE_Exec_Unique:
		return "X_Unique";
	case MEMORY_OWNER_TYPE_Exec_Hash:
		return "X_Hash";
	case MEMORY_OWNER_TYPE_Exec_SetOp:
		return "X_SetOp";
	case MEMORY_OWNER_TYPE_Exec_Limit:
		return "X_Limit";
	case MEMORY_OWNER_TYPE_Exec_Motion:
		return "X_Motion";
	case MEMORY_OWNER_TYPE_Exec_ShareInputScan:
		return "X_ShareInputScan";
	case MEMORY_OWNER_TYPE_Exec_WindowAgg:
		return "X_WindowAgg";
	case MEMORY_OWNER_TYPE_Exec_Repeat:
		return "X_Repeat";
	case MEMORY_OWNER_TYPE_Exec_DML:
		return "X_DML";
	case MEMORY_OWNER_TYPE_Exec_SplitUpdate:
		return "X_SplitUpdate";
	case MEMORY_OWNER_TYPE_Exec_RowTrigger:
		return "X_RowTrigger";
	case MEMORY_OWNER_TYPE_Exec_AssertOp:
		return "X_AssertOp";
	case MEMORY_OWNER_TYPE_Exec_BitmapTableScan:
		return "X_BitmapTableScan";
	case MEMORY_OWNER_TYPE_Exec_PartitionSelector:
		return "X_PartitionSelector";
	case MEMORY_OWNER_TYPE_Exec_RecursiveUnion:
		return "X_RecursiveUnion";
	case MEMORY_OWNER_TYPE_Exec_CteScan:
		return "X_CteScan";
	case MEMORY_OWNER_TYPE_Exec_WorkTableScan:
		return "X_WorkTableScan";
	default:
		Assert(false);
		break;
	}

	return "Error";
}

/**
 * MemoryAccountToCSV:
 * 		A visitor function that formats a memory account node information in CSV
 * 		format. The full tree is represented in CSV using a tree walk and a repeated
 * 		call of this function from the tree walker
 *
 * memoryAccount: The memory account which will be represented as CSV
 * context: context information to pass between successive function call
 * depth: The depth in the tree for current node. Used to generate indentation.
 * parentWalkSerial: parent node's "walk serial"
 * curWalkSerial: current node's "walk serial"
 */
static CdbVisitOpt
MemoryAccountToCSV(MemoryAccountTree *memoryAccountTreeNode, void *context, uint32 depth, uint32 parentWalkSerial, uint32 curWalkSerial)
{
	if (memoryAccountTreeNode == NULL) return CdbVisit_Walk;

	MemoryAccount *memoryAccount = memoryAccountTreeNode->account;

	MemoryAccountSerializerCxt *memAccountCxt = (MemoryAccountSerializerCxt*) context;

	/*
	 * PREFIX, ownerType, curWalkSerial, parentWalkSerial, maxLimit, peak, allocated, freed
	 */
	appendStringInfo(memAccountCxt->buffer, "%s,%u,%u,%u," UINT64_FORMAT "," UINT64_FORMAT "," UINT64_FORMAT "," UINT64_FORMAT "\n",
			memAccountCxt->prefix, memoryAccount->ownerType,
			curWalkSerial, parentWalkSerial,
			memoryAccount->maxLimit,
			memoryAccount->peak,
			memoryAccount->allocated,
			memoryAccount->freed
	);

	memAccountCxt->memoryAccountCount++;

	return CdbVisit_Walk;
}

/*
 * PsuedoAccountsToCSV
 *		Formats pseudo accounts such as vmem reserved and overall peak to CSV
 *
 * root: The root of the tree (used recursively)
 * str: The output buffer
 * prefix: A common prefix for each csv line
 */
static void
PsuedoAccountsToCSV(StringInfoData *str, char *prefix)
{
	int64 vmem_reserved = VmemTracker_GetMaxReservedVmemBytes();

	/*
	 * Add vmem reserved as reported by memprot. We report the vmem reserved in the
	 * "allocated" and "peak" fields. We set the freed to 0.
	 */
	appendStringInfo(str, "%s,%d,%u,%u," UINT64_FORMAT "," UINT64_FORMAT "," UINT64_FORMAT "," UINT64_FORMAT "\n",
			prefix, MEMORY_STAT_TYPE_VMEM_RESERVED,
			0 /* Child walk serial */, 0 /* Parent walk serial */,
			(int64) 0 /* Quota */, vmem_reserved /* Peak */, vmem_reserved /* Allocated */, (int64) 0 /* Freed */);

	/*
	 * Add peak memory observed from inside memory accounting among all allocations.
	 */
	appendStringInfo(str, "%s,%d,%u,%u," UINT64_FORMAT "," UINT64_FORMAT "," UINT64_FORMAT "," UINT64_FORMAT "\n",
			prefix, MEMORY_STAT_TYPE_MEMORY_ACCOUNTING_PEAK,
			0 /* Child walk serial */, 0 /* Parent walk serial */,
			(int64) 0 /* Quota */, MemoryAccountingPeakBalance /* Peak */,
			MemoryAccountingPeakBalance /* Allocated */, (int64) 0 /* Freed */);
}

/*
 * MemoryAccounting_ToString
 *		Converts a memory account tree rooted at "root" to string using tree
 *		walker and repeated calls of MemoryAccountToString
 *
 * root: The root of the tree (used recursively)
 * str: The output buffer
 * indentation: The indentation of the root
 */
static void
MemoryAccounting_ToString(MemoryAccountTree *root, StringInfoData *str, uint32 indentation)
{
	MemoryAccountSerializerCxt cxt;
	cxt.buffer = str;
	cxt.memoryAccountCount = 0;
	cxt.prefix = NULL;

	uint32 totalWalked = 0;
	MemoryAccountTreeWalkNode(root, MemoryAccountToString, &cxt, 0 + indentation, &totalWalked, totalWalked);
}

/*
 * InitMemoryAccounting
 *		Internal method that should only be used to initialize a memory accounting
 *		subsystem. Creates basic data structure such as the MemoryAccountMemoryContext,
 *		TopMemoryAccount, MemoryAccountMemoryAccount
 */
static void
InitMemoryAccounting()
{
	/* Either we never initialized or we are re-initializing after reset and rollover should be the active owner */
	Assert(ActiveMemoryAccountId == MEMORY_OWNER_TYPE_Undefined || ActiveMemoryAccountId == MEMORY_OWNER_TYPE_Rollover);

	/*
	 * We have long and short living accounts. Long living accounts are created only
	 * once and they live for the entire duration of a QE. We create all the long living
	 * accounts first, only if they were never created before.
	 *
	 * The memory accounting framework is a self-auditing framework in terms of memory usage.
	 * All the accounts are created in MemoryAccountMemoryContext and tracked in a special
	 * account MemoryAccountMemoryAccount. The MemoryAccountMemoryContext is created only
	 * once, during the first initialization. Subsequently, this context is reset once per-statement
	 * to prevent accumulating memory in accounting data structures.
	 *
	 * As MemoryAccountMemoryContext is reset once per-statement, we don't create the long living
	 * accounts in that context. Rather the long living accounts are created in TopMemoryContext.
	 *
	 * Once the long living accounts are created we set the MemoryAccountMemoryAccount
	 * as the ActiveMemoryAccount and proceed with the creation of short living accounts.
	 *
	 * This process ensures the following:
	 *
	 * 1. Accounting is triggered only if the ActiveMemoryAccount is non-null
	 *
	 * 2. If the ActiveMemoryAccount is non-null, we are guaranteed to have
	 * SharedChunkHeadersMemoryAccount (which is a long living account), so that we can account
	 * shared chunk headers.
	 *
	 * 3. All short-living accounts are accounted in MemoryAccountMemoryAccount
	 *
	 * 4. All short-living accounts are allocated in MemoryAccountMemoryContext
	 *
	 * 5. Number of allocations in the aset.c with nullAccountHeader is very small
	 * as we turn on ActiveMemoryAccount immediately after we allocate long-living
	 * accounts (only the memory to host long-living accounts are unaccounted, which
	 * are very few and their overhead is already known).
	 */

	if (!MemoryAccounting_IsInitialized())
	{
		/*
		 * All the long living accounts are created together, so if logical root
		 * is null, then other long-living accounts should be the null too
		 */
		InitLongLivingAccounts();

		/*
		 * Now create the context that contains all short-living accounts to ensure timely
		 * free of all the short-living account memory.
		 */
		MemoryAccountMemoryContext = AllocSetContextCreate(TopMemoryContext,
				"MemoryAccountMemoryContext",
				ALLOCSET_DEFAULT_MINSIZE,
				ALLOCSET_DEFAULT_INITSIZE,
				ALLOCSET_DEFAULT_MAXSIZE);

	}
	else
	{
		/* Long-living setup is already done, so re-initialize those */
		/* If "logical root" is pre-existing, "rollover" should also be pre-existing */
		Assert(RolloverMemoryAccount != NULL && SharedChunkHeadersMemoryAccount != NULL &&
				MemoryAccountMemoryAccount != NULL && AlienExecutorMemoryAccount != NULL);

		/* Ensure tree integrity */
		Assert(MemoryAccountMemoryAccount->parentId == MEMORY_OWNER_TYPE_LogicalRoot &&
				SharedChunkHeadersMemoryAccount->parentId == MEMORY_OWNER_TYPE_LogicalRoot &&
				RolloverMemoryAccount->parentId == MEMORY_OWNER_TYPE_LogicalRoot &&
				AlienExecutorMemoryAccount->parentId == MEMORY_OWNER_TYPE_LogicalRoot);
	}

	InitShortLivingMemoryAccounts();

	/* For AlienExecutorMemoryAccount we need TopMemoryAccount as parent */
	ActiveMemoryAccountId = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Top);
}

/* Clear out all the balance of an account */
static void
ClearAccount(MemoryAccount* memoryAccount)
{
	memoryAccount->allocated = 0;
	memoryAccount->freed = 0;
	memoryAccount->peak = 0;
}

/*
 * AdvanceMemoryAccountingGeneration
 * 		Saves the outstanding balance of current generation into RolloverAccount,
 * 		and advances the current generation.
 */
static void
AdvanceMemoryAccountingGeneration()
{
	/*
	 * We are going to wipe off MemoryAccountMemoryContext, so
	 * change ActiveMemoryAccount to RolloverMemoryAccount which
	 * is the only account to survive this MemoryAccountMemoryContext
	 * reset.
	 */
	MemoryAccounting_SwitchAccount((MemoryAccountIdType)MEMORY_OWNER_TYPE_Rollover);

	MemoryContextReset(MemoryAccountMemoryContext);
	Assert(MemoryAccountMemoryAccount->allocated == MemoryAccountMemoryAccount->freed);
	shortLivingMemoryAccountArray = NULL;

	/*
	 * Reset MemoryAccountMemoryAccount so that one query doesn't take the blame
	 * of another query. Note, this is different than RolloverMemoryAccount,
	 * whose purpose is to carry balance between multiple reset
	 */
	ClearAccount(MemoryAccountMemoryAccount);

	/* Everything except the SharedChunkHeadersMemoryAccount and AlienExecutorMemoryAccount rolls over */
	RolloverMemoryAccount->allocated = (MemoryAccountingOutstandingBalance -
			(SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed) -
			(AlienExecutorMemoryAccount->allocated - AlienExecutorMemoryAccount->freed));
	RolloverMemoryAccount->freed = 0;

	/*
	 * Rollover's peak includes SharedChunkHeadersMemoryAccount to give us
	 * an idea of highest seen peak across multiple reset. Note: MemoryAccountingPeakBalance
	 * includes SharedChunkHeadersMemoryAccount balance.
	 */
	RolloverMemoryAccount->peak = Max(RolloverMemoryAccount->peak, MemoryAccountingPeakBalance);

	/*
	 * Reset the RelinquishedPool Long living account, the amount should not be carried between memory account generations
	 */
	RelinquishedPoolMemoryAccount->allocated = 0;
	RelinquishedPoolMemoryAccount->peak = 0;
	RelinquishedPoolMemoryAccount->freed = 0;

	liveAccountStartId = nextAccountId;

	Assert(RolloverMemoryAccount->peak >= MemoryAccountingPeakBalance);
}

/*
 * MemoryAccounting_ResetPeakBalance
 *		Resets the peak memory account balance by setting it to the current balance.
 */
static void
MemoryAccounting_ResetPeakBalance()
{
	MemoryAccountingPeakBalance = MemoryAccountingOutstandingBalance;
}

/*
 * SaveMemoryBufToDisk
 *    Saves the memory account information in a file. The file name is auto
 *    generated using gp_session_id, gp_command_count and the passed time stamp
 *
 * memoryBuf: The buffer where the momory tree is serialized in (typically) csv form.
 * prefix: A file name prefix that can be used to uniquely identify the file's content
 */
static void
SaveMemoryBufToDisk(struct StringInfoData *memoryBuf, char *prefix)
{
	char fileName[MEMORY_REPORT_FILE_NAME_LENGTH];

	Assert((strlen("pg_log/") + strlen("memory_") + strlen(prefix) + strlen(".mem")) < MEMORY_REPORT_FILE_NAME_LENGTH);
	snprintf(fileName, MEMORY_REPORT_FILE_NAME_LENGTH, "%s/memory_%s.mem", "pg_log", prefix);

	FILE *file = fopen(fileName, "w");

	if (file == NULL)
	{
		elog(ERROR, "Could not write memory usage information. Failed to open file: %s", fileName);
	}

	uint64 bytes = fwrite(memoryBuf->data, 1, memoryBuf->len, file);

	if (bytes != memoryBuf->len)
	{
		insist_log(false, "Could not write memory usage information. Attempted to write %d", memoryBuf->len);
	}

	fclose(file);
}
