/*-------------------------------------------------------------------------
 *
 * mpool.c
 *	  Fast memory pool to manage lots of variable sizes of objects. This pool
 *    does not support free individual objects, but you can release all
 *    objects as a whole.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "utils/memutils.h"

#define PRINT(x) elog x
#undef PRINT
#define PRINT(x)  

#define MPOOL_BLOCK_SIZE (64 * 1024)

struct MPool 
{
	MemoryContextData *parent;
	MemoryContextData *context;

	/*
	 * Total number of bytes are allocated through the memory
	 * context.
	 */
	uint64 total_bytes_allocated;

	/* How many bytes are used by the caller. */
	uint64 bytes_used;

	/*
	 * When a new allocation request arrives, and the current block
	 * does not have enough space for this request, we waste those
	 * several bytes at the end of the block. This variable stores
	 * total number of these wasted bytes.
	 */
	uint64 bytes_wasted;

	/* The latest allocated block of available space. */
	void *start;
	void *end;
};

static void
mpool_init(MPool *mpool)
{
	Assert(mpool != NULL);
	mpool->total_bytes_allocated = 0;
	mpool->bytes_used = 0;
	mpool->bytes_wasted = 0;
	mpool->start = NULL;
	mpool->end = NULL;
}

/*
 * Create a MPool object and initialize its variables.
 */
MPool *
mpool_create_with_context(MemoryContext parent, MemoryContext context)
{
	MPool *mpool = MemoryContextAlloc(parent, sizeof(MPool));
	Assert(parent != NULL);
	mpool->parent = parent;
	mpool->context = context;
	mpool_init(mpool);

	return mpool;
}

/*
 * Return a pointer to a space with the given 'size'.
 */
void *
mpool_alloc(MPool *mpool, Size size)
{
	void *alloc_space;
	
	size = MAXALIGN(size);

	if (mpool->start == NULL ||
		(char *)mpool->end - (char *)mpool->start < size)
	{
		Size alloc_size;
		
		if (mpool->start != NULL)
		{
			Assert(mpool->end != NULL);
			mpool->bytes_wasted = (char *)mpool->end - (char *)mpool->start;
		}
		
		alloc_size = MPOOL_BLOCK_SIZE;

		if (size > MPOOL_BLOCK_SIZE)
			alloc_size = size;

		mpool->start = MemoryContextAlloc(mpool->context, alloc_size);
		mpool->end = (char *)mpool->start + alloc_size;

		mpool->total_bytes_allocated += alloc_size;
	}
	

	Assert(mpool->start != NULL && mpool->end != NULL &&
		   (char *)mpool->end - (char *)mpool->start >= size);

	alloc_space = mpool->start;
	mpool->start = (char *)mpool->start + size;
	Assert(mpool->start <= mpool->end);

	mpool->bytes_used += size;

	return alloc_space;
}

/*
 * Release all objects in the pool, and reset the memory context.
 */
void
mpool_reset(MPool *mpool)
{
	Assert(mpool != NULL && mpool->context != NULL);
	Assert(MemoryContextIsValid(mpool->context));

	elog(DEBUG2, "MPool: total_bytes_allocated=" INT64_FORMAT 
		 ", bytes_used=" INT64_FORMAT ", bytes_wasted=" INT64_FORMAT,
		 mpool->total_bytes_allocated, mpool->bytes_used,
		 mpool->bytes_wasted);

	MemoryContextReset(mpool->context);
	mpool_init(mpool);
}

/*
 * Delete the MPool object and its related space.
 */
void
mpool_delete(MPool *mpool)
{
	Assert(mpool != NULL && mpool->context != NULL);
	Assert(MemoryContextIsValid(mpool->context));
	
	mpool_reset(mpool);
	MemoryContextDelete(mpool->context);
	pfree(mpool);
}


uint64 mpool_total_bytes_allocated(MPool *mpool)
{
	Assert(mpool != NULL);
	return mpool->total_bytes_allocated;
}

uint64 mpool_bytes_used(MPool *mpool)
{
	Assert(mpool != NULL);
	return mpool->bytes_used;
}
