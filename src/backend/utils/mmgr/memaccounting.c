/*
 * Copyright (c) 2013 EMC Corporation All Rights Reserved
 *
 * This software is protected, without limitation, by copyright law
 * and international treaties. Use of this software and the intellectual
 * property contained therein is expressly limited to the terms and
 * conditions of the License Agreement under which it is provided by
 * or on behalf of EMC.
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

#define MEMORY_REPORT_FILE_NAME_LENGTH 255

/* Saves serializer context info during walking the memory account tree */
typedef struct MemoryAccountSerializerCxt
{
	/* How many was serialized in the buffer */
	int memoryAccountCount;
	/* Where to serialize */
	StringInfoData *buffer;
	char *prefix; /* Prefix to add before each line */
} MemoryAccountSerializerCxt;

/* Saves the deserializer context */
typedef struct MemoryAccountDeserializerCxt
{
	/* total MemoryAccount to deserialize */
	int memoryAccountCount;
	/* The raw bytes from where to deserialize */
	StringInfoData *buffer;

	/*
	 * We build MemoryAccount array in-place, reusing the memory of "buffer".
	 * The root will have a pseudo MemoryAccount (as we don't have a tree, and
	 * RolloverMemoryAccount, and TopMemoryAccount both are top-level) and everything
	 * will be direct/indirect children of it.
	 */
	MemoryAccount *root; /* Array of MemoryAccount [0...memoryAccountCount-1] */
} MemoryAccountDeserializerCxt;

/*
 ******************************************************
 * Internal methods declarations
 */
static void
CheckMemoryAccountingLeak(void);

static void
InitializeMemoryAccount(MemoryAccount *newAccount, long maxLimit,
		MemoryOwnerType ownerType, MemoryAccount *parentAccount);

static MemoryAccount*
CreateMemoryAccountImpl(long maxLimit,
		MemoryOwnerType ownerType, MemoryAccount* parent);

typedef CdbVisitOpt (*MemoryAccountVisitor)(MemoryAccount *memoryAccount,
		void *context, const uint32 depth, uint32 parentWalkSerial,
		uint32 curWalkSerial);

static void
AdvanceMemoryAccountingGeneration(void);

static void
HandleMemoryAccountingGenerationOverflow(MemoryContext context);

static void
HandleMemoryAccountingGenerationOverflowChildren(MemoryContext context);

static void
InitMemoryAccounting(void);

static CdbVisitOpt
MemoryAccountWalkNode(MemoryAccount *memoryAccount,
		MemoryAccountVisitor visitor, void *context, uint32 depth,
		uint32 *totalWalked, uint32 parentWalkSerial);

static CdbVisitOpt
MemoryAccountWalkKids(MemoryAccount *memoryAccount,
		MemoryAccountVisitor visitor, void *context, uint32 depth,
		uint32 *totalWalked, uint32 parentWalkSerial);

static CdbVisitOpt
MemoryAccountToString(MemoryAccount *memoryAccount, void *context,
		uint32 depth, uint32 parentWalkSerial, uint32 curWalkSerial);

static CdbVisitOpt
SerializeMemoryAccount(MemoryAccount *memoryAccount, void *context,
		uint32 depth, uint32 parentWalkSerial, uint32 curWalkSerial);

static CdbVisitOpt
MemoryAccountToCSV(MemoryAccount *memoryAccount, void *context,
		uint32 depth, uint32 parentWalkSerial, uint32 curWalkSerial);

static CdbVisitOpt
MemoryAccountToLog(MemoryAccount *memoryAccount, void *context,
		uint32 depth, uint32 parentWalkSerial, uint32 curWalkSerial);

static void
SaveMemoryBufToDisk(struct StringInfoData *memoryBuf, char *prefix);

/*****************************************************************************
 * Global memory accounting variables
 */

/*
 * This is the root of the memory accounting tree. All other accounts go
 * under this node. We call it "logical root" as no one should ever use it
 * or be aware of it. After creation of "logical root" to hold the root of
 * the tree, we immediately create other "useful" nodes such as "Rollover"
 * and "Top" and switch to "Top". Logical root can only have two children:
 * TopMemoryAccount and RolloverMemoryAccount
 */
MemoryAccount *MemoryAccountTreeLogicalRoot = NULL;

/* TopMemoryAccount is the father of all memory accounts EXCEPT RolloverMemoryAccount */
MemoryAccount *TopMemoryAccount = NULL;
/*
 * ActiveMemoryAccount is used by memory allocator to record the allocation.
 * However, deallocation uses the allocator information and ignores ActiveMemoryAccount
 */
MemoryAccount *ActiveMemoryAccount = NULL;
/* MemoryAccountMemoryAccount saves the memory overhead of memory accounting itself */
MemoryAccount *MemoryAccountMemoryAccount = NULL;

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
 * Total outstanding (i.e., allocated - freed) memory across all
 * memory accounts, including RolloverMemoryAccount
 */
uint64 MemoryAccountingOutstandingBalance = 0;

/*
 * Peak memory observed across all allocations/deallocations since
 * last MemoryAccounting_Reset() call
 */
uint64 MemoryAccountingPeakBalance = 0;

/*
 * The generation of current memory accounting tree.
 * Each allocation would save this generation in its
 * header. Later on during deallocation of memory if
 * the account generation of the memory does not match
 * the current generation, then we don't attempt to
 * release accounting for memory chunk's memory account.
 * Instead we release in bulk all the live allocations'
 * accounting into the RollOverMemoryAccount during
 * generation change.
 */
uint16 MemoryAccountingCurrentGeneration = 0;

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
	if (MemoryAccountTreeLogicalRoot)
	{
		/* No one should create child context under MemoryAccountMemoryContext */
		Assert(MemoryAccountMemoryContext->firstchild == NULL);

		AdvanceMemoryAccountingGeneration();
		CheckMemoryAccountingLeak();

		TopMemoryAccount = NULL;
		AlienExecutorMemoryAccount = NULL;

		/* Outstanding balance will come from either the rollover or the shared chunk header account */
		Assert((RolloverMemoryAccount->allocated - RolloverMemoryAccount->freed) +
				(SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed) ==
				MemoryAccountingOutstandingBalance);
		MemoryAccounting_ResetPeakBalance();
	}

	InitMemoryAccounting();
}

/*
 * MemoryAccounting_ResetPeakBalance
 *		Resets the peak memory account balance by setting it to the current balance.
 */
void
MemoryAccounting_ResetPeakBalance()
{
	MemoryAccountingPeakBalance = MemoryAccountingOutstandingBalance;
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
MemoryAccount*
MemoryAccounting_CreateAccount(long maxLimitInKB, MemoryOwnerType ownerType)
{
	return CreateMemoryAccountImpl(maxLimitInKB * 1024, ownerType, ActiveMemoryAccount);
}

/*
 * MemoryAccounting_SwitchAccount
 *		Switches the memory account. From this point on, any allocation will
 *		be charged to this new account. Note: we always charge deallocation to
 *		the owner of the memory who originally allocated that memory.
 *
 * desiredAccount: The account to switch to.
 */
MemoryAccount*
MemoryAccounting_SwitchAccount(MemoryAccount* desiredAccount)
{
	/* If memory monitoring is not enabled, we only allow switching to internal accounts */
	Assert(desiredAccount != NULL);

	MemoryAccount* oldAccount = ActiveMemoryAccount;

	ActiveMemoryAccount = desiredAccount;
	return oldAccount;
}

/*
 * MemoryAccounting_GetPeak
 *		Returns peak memory
 *
 * memoryAccount: The concerned account.
 */
uint64
MemoryAccounting_GetPeak(MemoryAccount * memoryAccount)
{
	return memoryAccount->peak;
}

/*
 * MemoryAccounting_GetBalance
 *		Returns current outstanding balance
 *
 * memoryAccount: The concerned account.
 */
uint64
MemoryAccounting_GetBalance(MemoryAccount * memoryAccount)
{
	return memoryAccount->allocated -
			memoryAccount->freed;
}

/*
 * MemoryAccounting_GetAccountName
 *		Converts MemoryAccount enum values to a descriptive string for reporting
 *		purpose.
 *
 *		We use a trick to save some coding. We currently set executor node's
 *		memory accounting enum values to the same one as their plan node's
 *		enum values. That way we can just pass the plan node's enum values in
 *		the CreateMemoryAccount() call.
 *
 * memoryAccount: The account whose name will be generated
 */
const char*
MemoryAccounting_GetAccountName(MemoryAccount *memoryAccount)
{
	switch (memoryAccount->ownerType)
	{
		case MEMORY_OWNER_TYPE_LogicalRoot:
			return "Root";
		case MEMORY_OWNER_TYPE_Rollover:
			return "Rollover";
		case MEMORY_OWNER_TYPE_SharedChunkHeader:
			return "SharedHeader";
		case MEMORY_OWNER_TYPE_Top:
			return "Top";
		case MEMORY_OWNER_TYPE_MemAccount:
			return "MemAcc";
		case MEMORY_OWNER_TYPE_MainEntry:
			return "Main";
		case MEMORY_OWNER_TYPE_Parser:
			return "Parser";
		case MEMORY_OWNER_TYPE_Planner:
			return "Planner";
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
			return "X_Bitmap";
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
		case MEMORY_OWNER_TYPE_Exec_Window:
			return "X_Window";
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
		case MEMORY_OWNER_TYPE_Exec_AlienShared:
			return "X_Alien";
		case MEMORY_OWNER_TYPE_Exec_BitmapTableScan:
			return "X_BitmapTableScan";
		case MEMORY_OWNER_TYPE_Exec_PartitionSelector:
			return "X_PartitionSelector";
		case MEMORY_OWNER_TYPE_Exec_ResilientJoin:
			return "X_ResilientJoin";
		case MEMORY_OWNER_TYPE_Exec_HashDummy:
			return "X_HashDummy";
		default:
			Assert(false);
			break;
	}

	return "Error";
}

/*
 * MemoryAccounting_Serialize
 * 		Serializes the current memory accounting tree into the "buffer"
 */
uint32
MemoryAccounting_Serialize(StringInfoData *buffer)
{
	MemoryAccountSerializerCxt cxt;
	cxt.buffer = buffer;
	cxt.prefix = NULL;

	cxt.memoryAccountCount = 0;
	uint32 totalWalked = 0;
	MemoryAccountWalkNode(MemoryAccountTreeLogicalRoot, SerializeMemoryAccount, &cxt, 0, &totalWalked, totalWalked);

	return totalWalked;
}
/*
 * MemoryAccounting_Deserialize:
 * 		Constructs the MemoryAccountTree in-place. NB: the function does not allocate
 * 		any new memory. Instead it assumes a binary set of bits that originally
 * 		represented the memory accounting tree. It updates the "serialized bits" to
 * 		correctly restores the original pointers that represents the tree.
 * 		Only the "logical root" is allocated in this function in the current memory context.
 * 		If the serialized bits have shorter life span than expected, it is callers
 * 		responsibility to correctly memcpy into the longer living context as appropriate.
 *
 * serializedBits: The array of bits that represents the serialized bits of the
 * 		memory accounting tree
 *
 * memoryAccountCount: Number of memory account node to expect in the serializedBits
 */
SerializedMemoryAccount*
MemoryAccounting_Deserialize(const void *serializedBits, uint32 memoryAccountCount)
{
    /*
     * Array pointers to different memory accounts in the buffer for indexing to expedite
     * tree construction
     */
    SerializedMemoryAccount **allAccounts = (SerializedMemoryAccount**)palloc0(memoryAccountCount * sizeof(SerializedMemoryAccount*));
    MemoryAccount **lastSeenChildren = (MemoryAccount**)palloc0(memoryAccountCount * sizeof(MemoryAccount*));

    SerializedMemoryAccount *memoryAccounts = (SerializedMemoryAccount*)serializedBits;
    for (int memAccSerial = 0; memAccSerial < memoryAccountCount; memAccSerial++)
    {
    	SerializedMemoryAccount *curMemoryAccount = &memoryAccounts[memAccSerial];
    	Assert(MemoryAccountIsValid(&curMemoryAccount->memoryAccount));

    	curMemoryAccount->memoryAccount.firstChild = NULL;
    	curMemoryAccount->memoryAccount.nextSibling = NULL;

    	allAccounts[curMemoryAccount->memoryAccountSerial] = curMemoryAccount;

    	if (curMemoryAccount->memoryAccountSerial == curMemoryAccount->parentMemoryAccountSerial)
    	{
			/* Only "logical root" can be parent less */
			Assert(curMemoryAccount->memoryAccountSerial == 0);

			/* We don't need to adjust parent and sibling pointers */
			continue;
    	}

    	SerializedMemoryAccount *parentMemoryAccount = allAccounts[curMemoryAccount->parentMemoryAccountSerial];
        	Assert(MemoryAccountIsValid(&parentMemoryAccount->memoryAccount));

        MemoryAccount *lastSeenChild = lastSeenChildren[curMemoryAccount->parentMemoryAccountSerial];

        if (!lastSeenChild)
        {
    		parentMemoryAccount->memoryAccount.firstChild = &(curMemoryAccount->memoryAccount);
        }
        else
        {
    		lastSeenChild->nextSibling = &curMemoryAccount->memoryAccount;
        }

		lastSeenChildren[curMemoryAccount->parentMemoryAccountSerial] = &(curMemoryAccount->memoryAccount);
		curMemoryAccount->memoryAccount.parentAccount = &(parentMemoryAccount->memoryAccount);
    }

    pfree(allAccounts);
    pfree(lastSeenChildren);

    return memoryAccounts;
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
void
MemoryAccounting_ToString(MemoryAccount *root, StringInfoData *str, uint32 indentation)
{
	MemoryAccountSerializerCxt cxt;
	cxt.buffer = str;
	cxt.memoryAccountCount = 0;
	cxt.prefix = NULL;

	uint32 totalWalked = 0;
	MemoryAccountWalkNode(root, MemoryAccountToString, &cxt, 0 + indentation, &totalWalked, totalWalked);
}

/*
 * MemoryAccounting_ToCSV
 *		Converts a memory account tree rooted at "root" to string using tree
 *		walker and repeated calls of MemoryAccountToCSV
 *
 * root: The root of the tree (used recursively)
 * str: The output buffer
 * prefix: A common prefix for each csv line
 */
void
MemoryAccounting_ToCSV(MemoryAccount *root, StringInfoData *str, char *prefix)
{
	MemoryAccountSerializerCxt cxt;
	cxt.buffer = str;
	cxt.memoryAccountCount = 0;
	cxt.prefix = prefix;

	uint32 totalWalked = 0;

	int64 vmem_reserved = VmemTracker_GetMaxReservedVmemBytes();

	/*
	 * Add vmem reserved as reported by memprot. We report the vmem reserved in the
	 * "allocated" and "peak" fields. We set the freed to 0.
	 */
	appendStringInfo(str, "%s,%d,%u,%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
			prefix, MEMORY_STAT_TYPE_VMEM_RESERVED,
			totalWalked /* Child walk serial */, totalWalked /* Parent walk serial */,
			(int64) 0 /* Quota */, vmem_reserved /* Peak */, vmem_reserved /* Allocated */, (int64) 0 /* Freed */);

	/*
	 * Add peak memory observed from inside memory accounting among all allocations.
	 */
	appendStringInfo(str, "%s,%d,%u,%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
			prefix, MEMORY_STAT_TYPE_MEMORY_ACCOUNTING_PEAK,
			totalWalked /* Child walk serial */, totalWalked /* Parent walk serial */,
			(int64) 0 /* Quota */, MemoryAccountingPeakBalance /* Peak */,
			MemoryAccountingPeakBalance /* Allocated */, (int64) 0 /* Freed */);

	MemoryAccountWalkNode(root, MemoryAccountToCSV, &cxt, 0, &totalWalked, totalWalked);
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

	MemoryAccounting_ToString(MemoryAccountTreeLogicalRoot, &memBuf, 0);

	elog(WARNING, "%s\n", memBuf.data);

	pfree(memBuf.data);
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
	appendStringInfo(&prefix, "%s,%s,%s,%u,%u,%d,%" PRIu64 ",%d,%d",
		memory_profiler_run_id, memory_profiler_dataset_id, memory_profiler_query_id,
		memory_profiler_dataset_size, statement_mem, gp_session_id, GetCurrentStatementStartTimestamp(),
		currentSliceId, GpIdentity.segindex);
	MemoryAccounting_ToCSV(MemoryAccountTreeLogicalRoot, &memBuf, prefix.data);
	SaveMemoryBufToDisk(&memBuf, prefix.data);

	pfree(prefix.data);
	pfree(memBuf.data);
}

/*
 * MemoryAccounting_SaveToLog
 *		Saves the current memory accounting tree in the log.
 *
 * Returns the number of memory accounts written to log.
 */
uint32
MemoryAccounting_SaveToLog()
{
	MemoryAccountSerializerCxt cxt;
	cxt.buffer = NULL;
	cxt.memoryAccountCount = 0;
	cxt.prefix = NULL;

	uint32 totalWalked = 0;

	int64 vmem_reserved = VmemTracker_GetMaxReservedVmemBytes();

	/* Write the header for the subsequent lines of memory usage information */
    write_stderr("memory: account_name, child_id, parent_id, quota, peak, allocated, freed, current\n");

    write_stderr("memory: %s, %u, %u, %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 "\n", "Vmem",
    		totalWalked /* Child walk serial */, totalWalked /* Parent walk serial */,
			(int64) 0 /* Quota */, vmem_reserved /* Peak */, vmem_reserved /* Allocated */, (int64) 0 /* Freed */, vmem_reserved /* Current */);

    write_stderr("memory: %s, %u, %u, %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 "\n", "Peak",
    		totalWalked /* Child walk serial */, totalWalked /* Parent walk serial */,
			(int64) 0 /* Quota */, MemoryAccountingPeakBalance /* Peak */, MemoryAccountingPeakBalance /* Allocated */, (int64) 0 /* Freed */, MemoryAccountingPeakBalance /* Current */);

	MemoryAccountWalkNode(MemoryAccountTreeLogicalRoot, MemoryAccountToLog, &cxt, 0, &totalWalked, totalWalked);

	return totalWalked;
}

/*****************************************************************************
 *	  PRIVATE ROUTINES FOR MEMORY ACCOUNTING								 *
 *****************************************************************************/

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
InitializeMemoryAccount(MemoryAccount *newAccount, long maxLimit, MemoryOwnerType ownerType, MemoryAccount *parentAccount)
{
	newAccount->ownerType = ownerType;

	/*
	 * Maximum targeted allocation for an owner. Peak usage can be
	 * tracked to check if the allocation is overshooting
	 */
    newAccount->maxLimit = maxLimit;

	newAccount->allocated = 0;
	newAccount->freed = 0;
	newAccount->peak = 0;

    newAccount->parentAccount = parentAccount;

	/* We don't include sub-accounts in the main memory accounting tree */
    if (parentAccount != NULL)
    {
		newAccount->nextSibling = parentAccount->firstChild;
		parentAccount->firstChild = newAccount;
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
static MemoryAccount*
CreateMemoryAccountImpl(long maxLimit, MemoryOwnerType ownerType, MemoryAccount* parent)
{
	/* We don't touch the oldContext. We create all MemoryAccount in MemoryAccountMemoryContext */
    MemoryContext oldContext = NULL;
	MemoryAccount* newAccount = NULL; /* Return value */
	/* Used for switching temporarily to MemoryAccountMemoryAccount ownership to account for the instrumentation overhead */
	MemoryAccount *oldAccount = NULL;

	/*
	 * Rollover is a special MemoryAccount that resides at the
	 * TopMemoryContext, and not under MemoryAccountMemoryContext
	 */
    Assert(ownerType == MEMORY_OWNER_TYPE_LogicalRoot || ownerType == MEMORY_OWNER_TYPE_SharedChunkHeader ||
    		ownerType == MEMORY_OWNER_TYPE_Rollover || ownerType == MEMORY_OWNER_TYPE_MemAccount ||
    		(MemoryAccountMemoryContext != NULL && MemoryAccountMemoryAccount != NULL));

    if (ownerType == MEMORY_OWNER_TYPE_SharedChunkHeader || ownerType == MEMORY_OWNER_TYPE_Rollover || ownerType == MEMORY_OWNER_TYPE_MemAccount || ownerType == MEMORY_OWNER_TYPE_Top)
    {
    	/* Set the "logical root" as the parent of two top account */
    	parent = MemoryAccountTreeLogicalRoot;
    }

    /*
     * Other than logical root, no long-living account should have children
     * and only logical root is allowed to have no parent
     */
    Assert((parent == NULL && ownerType == MEMORY_OWNER_TYPE_LogicalRoot) ||
    		(parent != RolloverMemoryAccount && parent != SharedChunkHeadersMemoryAccount &&
    		parent != MemoryAccountMemoryAccount));

    /*
     * Only SharedChunkHeadersMemoryAccount, Rollover, MemoryAccountMemoryAccount
     * and Top can be under "logical root"
     */
    Assert(parent != MemoryAccountTreeLogicalRoot ||
    		ownerType == MEMORY_OWNER_TYPE_LogicalRoot ||
    		ownerType == MEMORY_OWNER_TYPE_SharedChunkHeader ||
    		ownerType == MEMORY_OWNER_TYPE_Rollover ||
    		ownerType == MEMORY_OWNER_TYPE_MemAccount ||
    		ownerType == MEMORY_OWNER_TYPE_Top);

    /* Long-living accounts need TopMemoryContext */
    if (ownerType == MEMORY_OWNER_TYPE_LogicalRoot ||
    		ownerType == MEMORY_OWNER_TYPE_SharedChunkHeader ||
    		ownerType == MEMORY_OWNER_TYPE_Rollover ||
    		ownerType == MEMORY_OWNER_TYPE_MemAccount)
    {
    	oldContext = MemoryContextSwitchTo(TopMemoryContext);
    }
    else
    {
    	oldContext = MemoryContextSwitchTo(MemoryAccountMemoryContext);
        oldAccount = MemoryAccounting_SwitchAccount(MemoryAccountMemoryAccount);
    }

	newAccount = makeNode(MemoryAccount);
	InitializeMemoryAccount(newAccount, maxLimit, ownerType, parent);

	if (oldAccount != NULL)
	{
		MemoryAccounting_SwitchAccount(oldAccount);
	}

    MemoryContextSwitchTo(oldContext);

    return newAccount;
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
MemoryAccountWalkNode(MemoryAccount *memoryAccount, MemoryAccountVisitor visitor,
			        void *context, uint32 depth, uint32 *totalWalked, uint32 parentWalkSerial)
{
    CdbVisitOpt     whatnext;

    if (memoryAccount == NULL)
    {
        whatnext = CdbVisit_Walk;
    }
    else
    {
    	uint32 curWalkSerial = *totalWalked;
        whatnext = visitor(memoryAccount, context, depth, parentWalkSerial, *totalWalked);
        *totalWalked = *totalWalked + 1;

        if (whatnext == CdbVisit_Walk)
        {
            whatnext = MemoryAccountWalkKids(memoryAccount, visitor, context, depth + 1, totalWalked, curWalkSerial);
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
MemoryAccountWalkKids(MemoryAccount      *memoryAccount,
			        MemoryAccountVisitor visitor,
			        void           *context, uint32 depth, uint32 *totalWalked, uint32 parentWalkSerial)
{
    if (memoryAccount == NULL)
        return CdbVisit_Walk;

    /* Traverse children accounts */
    for (MemoryAccount *child = memoryAccount->firstChild; child != NULL; child = child->nextSibling)
    {
    	MemoryAccountWalkNode(child, visitor, context, depth, totalWalked, parentWalkSerial);
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
MemoryAccountToString(MemoryAccount *memoryAccount, void *context, uint32 depth,
		uint32 parentWalkSerial, uint32 curWalkSerial)
{
	if (memoryAccount == NULL) return CdbVisit_Walk;

	MemoryAccountSerializerCxt *memAccountCxt = (MemoryAccountSerializerCxt*) context;

    appendStringInfoFill(memAccountCxt->buffer, 2 * depth, ' ');

    /* We print only integer valued memory consumption, in standard GPDB KB unit */
    appendStringInfo(memAccountCxt->buffer, "%s: Peak %" PRIu64 "K bytes. Quota: %" PRIu64 "K bytes.\n",
		MemoryAccounting_GetAccountName(memoryAccount),
		memoryAccount->peak / 1024, memoryAccount->maxLimit / 1024);

    memAccountCxt->memoryAccountCount++;

    return CdbVisit_Walk;
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
MemoryAccountToCSV(MemoryAccount *memoryAccount, void *context, uint32 depth, uint32 parentWalkSerial, uint32 curWalkSerial)
{
	if (memoryAccount == NULL) return CdbVisit_Walk;

	MemoryAccountSerializerCxt *memAccountCxt = (MemoryAccountSerializerCxt*) context;

	/*
	 * PREFIX, ownerType, curWalkSerial, parentWalkSerial, maxLimit, peak, allocated, freed
	 */
	appendStringInfo(memAccountCxt->buffer, "%s,%u,%u,%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
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

/**
 * MemoryAccountToLog:
 * 		A visitor function that writes a memory account node information in the log.
 * 		The full tree is represented in CSV using a tree walk and a repeated call
 * 		of this function from the tree walker
 *
 * memoryAccount: The memory account which will be represented as CSV
 * context: context information to pass between successive function call
 * depth: The depth in the tree for current node. Used to generate indentation.
 * parentWalkSerial: parent node's "walk serial"
 * curWalkSerial: current node's "walk serial"
 */
static CdbVisitOpt
MemoryAccountToLog(MemoryAccount *memoryAccount, void *context, uint32 depth, uint32 parentWalkSerial, uint32 curWalkSerial)
{
	if (memoryAccount == NULL) return CdbVisit_Walk;

	MemoryAccountSerializerCxt *memAccountCxt = (MemoryAccountSerializerCxt*) context;

    write_stderr("memory: %s, %u, %u, %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 "\n", MemoryAccounting_GetAccountName(memoryAccount),
    		curWalkSerial /* Child walk serial */, parentWalkSerial /* Parent walk serial */,
			(int64) memoryAccount->maxLimit /* Quota */,
	memoryAccount->peak /* Peak */,
	memoryAccount->allocated /* Allocated */,
	memoryAccount->freed /* Freed */, (memoryAccount->allocated - memoryAccount->freed) /* Current */);
    memAccountCxt->memoryAccountCount++;

    return CdbVisit_Walk;
}

/**
 * SerializeMemoryAccount:
 * 		A visitor function that serializes a particular memory account. Called
 * 		from the walker to serialize the whole tree.
 *
 * memoryAccount: The memory account which will be represented as CSV
 * context: context information to pass between successive function call
 * depth: The depth in the tree for current node. Used to generate indentation.
 * parentWalkSerial: parent node's "walk serial"
 * curWalkSerial: current node's "walk serial"
 */
static CdbVisitOpt
SerializeMemoryAccount(MemoryAccount *memoryAccount, void *context, uint32 depth,
		uint32 parentWalkSerial, uint32 curWalkSerial)
{
	if (memoryAccount == NULL) return CdbVisit_Walk;

	Assert(MemoryAccountIsValid(memoryAccount));

	MemoryAccountSerializerCxt *memAccountCxt = (MemoryAccountSerializerCxt*) context;

	SerializedMemoryAccount serializedMemoryAccount;
	serializedMemoryAccount.type = T_SerializedMemoryAccount;
	serializedMemoryAccount.memoryAccount = *memoryAccount;
	serializedMemoryAccount.memoryAccountSerial = curWalkSerial;
	serializedMemoryAccount.parentMemoryAccountSerial = parentWalkSerial;

	appendBinaryStringInfo(memAccountCxt->buffer, &serializedMemoryAccount, sizeof(SerializedMemoryAccount));

    memAccountCxt->memoryAccountCount++;

    return CdbVisit_Walk;
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
	Assert(TopMemoryAccount == NULL);
	Assert(AlienExecutorMemoryAccount == NULL);

	/*
	 * Order of creation:
	 *
	 * 1. Root
	 * 2. SharedChunkHeadersMemoryAccount
	 * 3. RolloverMemoryAccount
	 * 4. MemoryAccountMemoryAccount
	 * 5. MemoryAccountMemoryContext
	 *
	 * The above 5 survive reset (MemoryAccountMemoryContext
	 * gets reset, but not deleted).
	 *
	 * Next set ActiveMemoryAccount = MemoryAccountMemoryAccount
	 *
	 * Note, don't set MemoryAccountMemoryAccount before creating
	 * MemoryAccuontMemoryContext, as that will put the balance
	 * of MemoryAccountMemoryContext in the MemoryAccountMemoryAccount,
	 * preventing it from going to 0, upon reset of MemoryAccountMemoryContext.
	 * MemoryAccountMemoryAccount should only contain balance from actual
	 * account creation, not the overhead of the context.
	 *
	 * Once the long living accounts are done and MemoryAccountMemoryAccount
	 * is the ActiveMemoryAccount, we proceed to create TopMemoryAccount
	 *
	 * This ensures the following:
	 *
	 * 1. Accounting is triggered only if the ActiveMemoryAccount is non-null
	 *
	 * 2. If the ActiveMemoryAccount is non-null, we are guaranteed to have
	 * SharedChunkHeadersMemoryAccount, so that we can account shared chunk headers
	 *
	 * 3. All short-living accounts (including Top) are accounted in MemoryAccountMemoryAccount
	 *
	 * 4. All short-living accounts are allocated in MemoryAccountMemoryContext
	 *
	 * 5. Number of allocations in the aset.c with nullAccountHeader is very small
	 * as we turn on ActiveMemoryAccount immediately after we allocate long-living
	 * accounts (only the memory to host long-living accounts are unaccounted, which
	 * are very few and their overhead is already known).
	 */

	if (MemoryAccountTreeLogicalRoot == NULL)
	{
		/*
		 * All the long living accounts are created together, so if logical root
		 * is null, then other long-living accounts should be the null too
		 */
		Assert(SharedChunkHeadersMemoryAccount == NULL && RolloverMemoryAccount == NULL);
		Assert(TopMemoryAccount == NULL && MemoryAccountMemoryAccount == NULL && MemoryAccountMemoryContext == NULL);

		MemoryAccountTreeLogicalRoot = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_LogicalRoot);

		SharedChunkHeadersMemoryAccount = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_SharedChunkHeader);

		RolloverMemoryAccount = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Rollover);

		MemoryAccountMemoryAccount = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_MemAccount);

		/* Now initiate the memory accounting system. */
		MemoryAccountMemoryContext = AllocSetContextCreate(TopMemoryContext,
											 "MemoryAccountMemoryContext",
											 ALLOCSET_DEFAULT_MINSIZE,
											 ALLOCSET_DEFAULT_INITSIZE,
											 ALLOCSET_DEFAULT_MAXSIZE);

		/*
		 * Temporarily active MemoryAccountMemoryAccount. Once
		 * all the setup is done, TopMemoryAccount will become
		 * the ActiveMemoryAccount
		 */
		ActiveMemoryAccount = MemoryAccountMemoryAccount;
	}
	else
	{
		/* Long-living setup is already done, so re-initialize those */
		/* If "logical root" is pre-existing, "rollover" should also be pre-existing */
		Assert(MemoryAccountTreeLogicalRoot->nextSibling == NULL &&
				RolloverMemoryAccount != NULL && SharedChunkHeadersMemoryAccount != NULL &&
				MemoryAccountMemoryAccount != NULL);

		/* Ensure tree integrity */
		Assert(MemoryAccountMemoryAccount->firstChild == NULL &&
				SharedChunkHeadersMemoryAccount->firstChild == NULL &&
				RolloverMemoryAccount->firstChild == NULL);

		/*
		 * First child of MemoryAccountTreeLogicalRoot should be Top, which
		 * had been obliterated during MemoryAccountMemoryContextReset. So,
		 * we don't attempt to verify the first child of MeomryAccountTreeLogicalRoot
		 */
		Assert(MemoryAccountMemoryAccount->nextSibling == RolloverMemoryAccount &&
				RolloverMemoryAccount->nextSibling == SharedChunkHeadersMemoryAccount &&
				SharedChunkHeadersMemoryAccount->nextSibling == NULL);

		/*
		 * We will loose previous TopMemoryAccount during InitMemoryAccounting. So,
		 * we need to readjust the "logical root" children pointers.
		 */
	 	MemoryAccountTreeLogicalRoot->firstChild = MemoryAccountMemoryAccount;
	 	MemoryAccountMemoryAccount->nextSibling = RolloverMemoryAccount;
	 	RolloverMemoryAccount->nextSibling = SharedChunkHeadersMemoryAccount;
	 	SharedChunkHeadersMemoryAccount->nextSibling = NULL;
	 	RolloverMemoryAccount->firstChild = NULL;
	 	SharedChunkHeadersMemoryAccount->firstChild = NULL;
	}

	TopMemoryAccount = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Top);
	/* For AlienExecutorMemoryAccount we need TopMemoryAccount as parent */
	ActiveMemoryAccount = TopMemoryAccount;

	AlienExecutorMemoryAccount = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Exec_AlienShared);
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
	ActiveMemoryAccount = RolloverMemoryAccount;
	MemoryContextReset(MemoryAccountMemoryContext);
	Assert(MemoryAccountMemoryAccount->allocated == MemoryAccountMemoryAccount->freed &&
			MemoryAccountMemoryAccount->firstChild == NULL);

	/*
	 * Reset MemoryAccountMemoryAccount so that one query doesn't take the blame
	 * of another query. Note, this is different than RolloverMemoryAccount,
	 * whose purpose is to carry balance between multiple reset
	 */
	MemoryAccountMemoryAccount->allocated = 0;
	MemoryAccountMemoryAccount->freed = 0;
	MemoryAccountMemoryAccount->peak = 0;

	/* Everything except the SharedChunkHeadersMemoryAccount rolls over */
	RolloverMemoryAccount->allocated = (MemoryAccountingOutstandingBalance -
			(SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed));
	RolloverMemoryAccount->freed = 0;

	/*
	 * Rollover's peak includes SharedChunkHeadersMemoryAccount to give us
	 * an idea of highest seen peak across multiple reset. Note: MemoryAccountingPeakBalance
	 * includes SharedChunkHeadersMemoryAccount balance.
	 */
	RolloverMemoryAccount->peak = Max(RolloverMemoryAccount->peak, MemoryAccountingPeakBalance);

	uint16 prevGeneration = MemoryAccountingCurrentGeneration;
	MemoryAccountingCurrentGeneration = MemoryAccountingCurrentGeneration + 1;

	if (prevGeneration > MemoryAccountingCurrentGeneration)
	{
		/* Overflow happened, we need to adjust for generation */
		elog(LOG, "Migrating all allocated memory chunks to generation %u due to generation counter exhaustion and QE recycling", MemoryAccountingCurrentGeneration);
		HandleMemoryAccountingGenerationOverflow(TopMemoryContext);
	}

	Assert((RolloverMemoryAccount->allocated - RolloverMemoryAccount->freed) == (MemoryAccountingOutstandingBalance -
			(SharedChunkHeadersMemoryAccount->allocated - SharedChunkHeadersMemoryAccount->freed)));
	Assert(RolloverMemoryAccount->peak >= MemoryAccountingPeakBalance);
}

/*
 * HandleMemoryAccountingGenerationOverflow
 * 		Descends the memory context tree rooted at "context" and calls update_generation
 * 		on each context. The update_generation is supposed to look for every active chunks
 * 		and set their ownership to RolloverAccount.The update_generation method will also
 * 		set the generation of every active chunk to MemoryAccountingCurrentGeneration.
 *
 * Parameters:
 * 		context: The memory context whose children will be descended
 */
static void
HandleMemoryAccountingGenerationOverflow(MemoryContext context)
{
	AssertArg(MemoryContextIsValid(context));

	/* save a function call in common case where there are no children */
	if (context->firstchild != NULL)
	{
		HandleMemoryAccountingGenerationOverflowChildren(context);
	}

	(*context->methods.update_generation) (context);
}

/*
 * HandleMemoryAccountingGenerationOverflowChildren
 * 		Companion method for HandleMemoryAccountingGenerationOverflow(). Used for
 * 		walking the memory context tree.
 *
 * Parameters:
 * 		context: The memory context whose children will be descended
 */
static void
HandleMemoryAccountingGenerationOverflowChildren(MemoryContext context)
{
	MemoryContext child;

	AssertArg(MemoryContextIsValid(context));

	for (child = context->firstchild; child != NULL; child = child->nextchild)
	{
		HandleMemoryAccountingGenerationOverflow(child);
	}
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
