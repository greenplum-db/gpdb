/*-------------------------------------------------------------------------
 *
 * ext_alloc.c
 *
 * Portions Copyright (c) 2017-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/mmgr/ext_alloc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "utils/vmem_tracker.h"
#include "utils/gp_alloc.h"
#include "utils/memaccounting.h"
#include "utils/memaccounting_private.h"
#include "utils/ext_alloc.h"

/*
 * This variable is used to track memory that is not freed by Orca during a
 * single short living Optimizer account.
 *
 * Note: Although theoretically this value could overflow, before that happens
 * the underlying system is likely to hit some kind of Vmem limit
 */
uint64 OptimizerOutstandingMemoryBalance = 0;

/*
 * Allocation & Deallocation functions for GPOS
 *
 *   These functions provide GPOS the facility to have all its memory managed by gp_malloc/free
 */

void*
Ext_OptimizerAlloc(size_t size)
{
#ifdef USE_ASSERT_CHECKING
	MemoryAccount *account = MemoryAccounting_ConvertIdToAccount(ActiveMemoryAccountId);
	Assert(account->ownerType == MEMORY_OWNER_TYPE_Optimizer);
#endif

	MemoryAccounting_Allocate(ActiveMemoryAccountId, size);
	OptimizerOutstandingMemoryBalance += size;
	return gp_malloc(size);
}

void
Ext_OptimizerFree(void *ptr)
{
	void *malloc_pointer = UserPtr_GetVmemPtr(ptr);
	size_t freed_size = VmemPtr_GetUserPtrSize((VmemHeader*) malloc_pointer);
	MemoryAccounting_Free(ActiveMemoryAccountId, freed_size);
	OptimizerOutstandingMemoryBalance -= freed_size;
	gp_free(ptr);
}

uint64
GetOptimizerOutstandingMemoryBalance()
{
	return OptimizerOutstandingMemoryBalance;
}

