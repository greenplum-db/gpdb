/*-------------------------------------------------------------------------
 *
 * gp_alloc.h
 *	  This file contains declarations for an allocator that works with vmem quota.
 *
 * Copyright (c) 2016, Pivotal Inc.
 */
#ifndef GP_ACCOUNTED_ALLOC_H
#define GP_ACCOUNTED_ALLOC_H

#include "nodes/nodes.h"
#include "memaccounting.h"

extern bool
MemoryAccounting_Allocate(struct MemoryAccount* memoryAccount,
		Size allocatedSize);

extern bool
MemoryAccounting_Free(MemoryAccount* memoryAccount,
		uint16 memoryAccountGeneration,
		Size allocatedSize);

#ifdef USE_ASSERT_CHECKING
#define GP_ALLOC_DEBUG
#endif

#ifdef GP_ALLOC_DEBUG

#define ACCOUNTED_ALLOC_HEADER_CHECKSUM 0xacacacacac

typedef int64 AccountedAllocHeaderChecksumType;

#define AccountedAllocPtr_VerifyChecksum(ptr) \
	Assert(AccountedAllocPtr_GetChecksum(ptr) == ACCOUNTED_ALLOC_HEADER_CHECKSUM)

#else

#define AccountedAllocPtr_VerifyChecksum(ptr)

#endif

/*
 * The AccountedAllocHeader sits between the usable memory and any Vmem headers.
 * This provides a "light-weight" allocation API that can track memory
 * allocations and frees using GPDB's memory accounting mechanism without the
 * overhead of explicitly managing block and check allocations. In this way, we
 * can account for memory usage of external libraries to maintain a more accurate
 * account of memory consumed by GPDB.
 */
typedef struct AccountedAllocHeader
{
#ifdef GP_ALLOC_DEBUG
	/*
	 * Checksum to verify we are correctly using gp_accounted_malloc with
	 * gp_accounted_free
	 */
	AccountedAllocHeaderChecksumType checksum;
#endif
	/* Memory account where to this allocation is recorded */
	struct MemoryAccount *memoryAccount;
	/* Track account validity */
	uint16 memoryAccountGeneration;
} AccountedAllocHeader;

extern void *gp_accounted_malloc(int64 sz);
extern void *gp_accounted_realloc(void *ptr, int64 newsz);
extern void gp_accounted_free(void *ptr);

/* Get the actual usable payload address from the header */
static inline
void *AccountedAllocPtrToUserPtr(AccountedAllocHeader *ptr)
{
	return (void *)(((char*)ptr) + sizeof(AccountedAllocHeader));
}

/* Get address of the header from a user pointer */
static inline
AccountedAllocHeader *UserPtrToAccountedAllocPtr(void *ptr)
{
	return (AccountedAllocHeader *)(((char*)ptr) - sizeof(AccountedAllocHeader));
}

#ifdef GP_ALLOC_DEBUG
/* Retrieve the checksum from the header */
static inline AccountedAllocHeaderChecksumType
AccountedAllocPtr_GetChecksum(AccountedAllocHeader *ptr)
{
	return ptr->checksum;
}

/* Stores a checksum in the header for debugging purpose */
static inline
void AccountedAllocPtr_SetChecksum(AccountedAllocHeader *ptr)
{
	ptr->checksum = ACCOUNTED_ALLOC_HEADER_CHECKSUM;
}
#endif   /* GP_ALLOC_DEBUG */

/* Extract the memory account stored in the AccountedHeader */
static inline
struct MemoryAccount*
AccountedAllocPtr_GetMemoryAccount(AccountedAllocHeader *ptr)
{
	AccountedAllocPtr_VerifyChecksum(ptr);
	return ptr->memoryAccount;
}

/* Extract the memory account generation stored in the AccountedHeader */
static inline
uint16 AccountedAllocPtr_GetMemoryAccountGeneration(AccountedAllocHeader *ptr)
{
	AccountedAllocPtr_VerifyChecksum(ptr);
	return ptr->memoryAccountGeneration;
}

/* Set the memory account stored in the AccountedHeader */
static inline
struct MemoryAccount* AccountedAllocPtr_SetMemoryAccount(
		AccountedAllocHeader *ptr, struct MemoryAccount* memoryAccount)
{
	return ptr->memoryAccount = memoryAccount;
}

/* Set the memory account generation stored in the AccountedHeader */
static inline
uint16 AccountedAllocPtr_SetMemoryAccountGeneration(
		AccountedAllocHeader *ptr, uint16 memoryAccountGeneration)
{
	return ptr->memoryAccountGeneration = memoryAccountGeneration;
}

/*
 * Initialize the AccountedAlloc header with current memory account and account
 * generation
 */
static inline
void AccountedAllocPtr_Initialize(AccountedAllocHeader *ptr)
{
	AccountedAllocPtr_SetMemoryAccount(ptr, ActiveMemoryAccount);
	AccountedAllocPtr_SetMemoryAccountGeneration(ptr,
			MemoryAccountingCurrentGeneration);
#ifdef GP_ALLOC_DEBUG
	AccountedAllocPtr_SetChecksum(ptr);
#endif
}

#endif   /* GP_ACCOUNTED_ALLOC_H */
