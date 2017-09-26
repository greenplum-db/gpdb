/*-------------------------------------------------------------------------
 *
 * memaccounting_private.h
 *	  This file contains declarations for memory accounting functions that
 *	  are only supposed to be used by privileged callers such a memory managers.
 *	  Other files should not include this file.
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/memaccounting_private.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef MEMACCOUNTING_PRIVATE_H
#define MEMACCOUNTING_PRIVATE_H

#include "utils/memaccounting.h"

extern MemoryAccountIdType liveAccountStartId;
extern MemoryAccountIdType nextAccountId;

/* MemoryAccount is the fundamental data structure to record memory usage */
typedef struct MemoryAccount {
	NodeTag type;
	MemoryOwnerType ownerType;

	uint64 allocated;
	uint64 freed;
	uint64 peak;
	/*
	 * Maximum targeted allocation for an owner. Peak usage can be tracked to
	 * check if the allocation is overshooting
	 */
	uint64 maxLimit;

	/*
	 * Amount of memory relinquished
	 */
	uint64 relinquishedMemory;

	/*
	 * Amount of memory acquired from relinquished pool
	 */
	uint64 acquiredMemory;

	MemoryAccountIdType id;
	MemoryAccountIdType parentId;
} MemoryAccount;

typedef struct MemoryAccountArray{
	MemoryAccountIdType accountCount;
	MemoryAccountIdType arraySize;
	// array of pointers to memory accounts of size accountCount
	MemoryAccount** allAccounts;
} MemoryAccountArray;

extern MemoryAccountArray* shortLivingMemoryAccountArray;
/* We save index 0 for undefined null account. Therefore, we need an extra entry */
extern MemoryAccount* longLivingMemoryAccountArray[MEMORY_OWNER_TYPE_END_LONG_LIVING + 1];

extern MemoryAccount *SharedChunkHeadersMemoryAccount;

extern uint64 MemoryAccountingOutstandingBalance;
extern uint64 MemoryAccountingPeakBalance;

/*
 * MemoryAccounting_IsLiveAccount
 *    Checks if an account is live.
 *
 * id: the id of the account
 */
static inline bool
MemoryAccounting_IsLiveAccount(MemoryAccountIdType id)
{
	AssertImply(NULL == shortLivingMemoryAccountArray, liveAccountStartId == nextAccountId);
	bool isValidShortLivingAccount = (id >= liveAccountStartId &&
      id < (liveAccountStartId + (NULL == shortLivingMemoryAccountArray ? 0 : shortLivingMemoryAccountArray->accountCount)));
	return isValidShortLivingAccount ||
	    ((id <= MEMORY_OWNER_TYPE_END_LONG_LIVING) && (id > MEMORY_OWNER_TYPE_Undefined)) /* Valid long living? */;
}

/*
 * MemoryAccounting_ConvertIdToAccount
 *    Converts an account ID to an account pointer.
 *
 * id: the id of the account
 */
static inline MemoryAccount*
MemoryAccounting_ConvertIdToAccount(MemoryAccountIdType id)
{
	MemoryAccount *memoryAccount = NULL;

	if (id >= liveAccountStartId)
	{
		Assert(NULL != shortLivingMemoryAccountArray);
		Assert(id < liveAccountStartId + shortLivingMemoryAccountArray->accountCount);
		memoryAccount = shortLivingMemoryAccountArray->allAccounts[id - liveAccountStartId];
	}
	else if (id <= MEMORY_OWNER_TYPE_END_LONG_LIVING)
	{
		Assert(NULL != longLivingMemoryAccountArray);
		/* 0 is reserved as undefined. So, the array index is 1 behind */
		memoryAccount = longLivingMemoryAccountArray[id];
	}
	else if (id < liveAccountStartId) /* Dead account; so use rollover */
	{
		Assert(NULL != longLivingMemoryAccountArray);
		/*
		 * For dead accounts we use a single rollover account to account for all
		 * the long living allocations. Rollover is a long-living account, so it
		 * doesn't get recreated and it accounts for all the past allocations that
		 * outlived their owner accounts.
		 */
		memoryAccount = longLivingMemoryAccountArray[MEMORY_OWNER_TYPE_Rollover];
	}

	Assert(IsA(memoryAccount, MemoryAccount));

	return memoryAccount;
}

/*
 * MemoryAccounting_Allocate
 *	 	When an allocation is made, this function will be called by the
 *	 	underlying allocator to record allocation request.
 *
 * memoryAccountId: where to record this allocation
 * allocatedSize: the final amount of memory returned by the allocator (with overhead)
 *
 * If the return value is false, the underlying memory allocator should fail.
 */
static inline bool
MemoryAccounting_Allocate(MemoryAccountIdType memoryAccountId, Size allocatedSize)
{
	MemoryAccount* memoryAccount = MemoryAccounting_ConvertIdToAccount(memoryAccountId);

	Assert(memoryAccount->allocated + allocatedSize >=
			memoryAccount->allocated);

	memoryAccount->allocated += allocatedSize;

	Size held = memoryAccount->allocated -
			memoryAccount->freed;

	memoryAccount->peak =
			Max(memoryAccount->peak, held);

	Assert(memoryAccount->allocated >=
			memoryAccount->freed);

	MemoryAccountingOutstandingBalance += allocatedSize;
	MemoryAccountingPeakBalance = Max(MemoryAccountingPeakBalance, MemoryAccountingOutstandingBalance);

	return true;
}

/*
 * MemoryAccounting_Free
 *		"One" implementation of free request handler. Each memory account
 *		can customize its free request function. When memory is deallocated,
 *		this function will be called by the underlying allocator to record deallocation.
 *		This function records the amount of memory freed.
 *
 * memoryAccount: where to record this allocation
 * context: the context where this memory belongs
 * allocatedSize: the final amount of memory returned by the allocator (with overhead)
 *
 * Note: the memoryAccount can be an invalid pointer if the generation of
 * the allocation is different than the current generation. In such case
 * this method would automatically select RolloverMemoryAccount, instead
 * of accessing an invalid pointer.
 */
static inline bool
MemoryAccounting_Free(MemoryAccountIdType memoryAccountId, Size allocatedSize)
{
	MemoryAccount* memoryAccount = MemoryAccounting_ConvertIdToAccount(memoryAccountId);

	Assert(memoryAccount->freed +
			allocatedSize >= memoryAccount->freed);

	Assert(memoryAccount->allocated >= memoryAccount->freed);

	memoryAccount->freed += allocatedSize;

	MemoryAccountingOutstandingBalance -= allocatedSize;

	return true;
}

/* Is the memory accounting framework initialized? */
static inline bool
MemoryAccounting_IsInitialized()
{
	return (NULL != longLivingMemoryAccountArray[MEMORY_OWNER_TYPE_LogicalRoot]);
}

#endif   /* MEMACCOUNTING_PRIVATE_H */
