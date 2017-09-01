/*-------------------------------------------------------------------------
 *
 * workfile_segmentspace.c
 *	 Implementation of workfile manager per-segment disk space accounting
 *
 * Portions Copyright (c) 2012, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/workfile_manager/workfile_segmentspace.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "port/atomics.h"
#include "storage/shmem.h"
#include "cdb/cdbvars.h"
#include "utils/workfile_mgr.h"
#include "miscadmin.h"

/* Name to identify the WorkfileSegspace shared memory area by */
#define WORKFILE_SEGSPACE_SHMEM_NAME "WorkfileSegspace"

/* Pointer to the shared memory counter with the total used diskspace across segment */
static pg_atomic_uint64 *used_segspace = NULL;

/*
 * Initialize shared memory area for the WorkfileSegspace module
 */
void
WorkfileSegspace_Init(void)
{
	bool attach = false;
	/* Allocate or attach to shared memory area */
	void *shmem_base = ShmemInitStruct(WORKFILE_SEGSPACE_SHMEM_NAME,
			WorkfileSegspace_ShMemSize(),
			&attach);

	/*
	 * Make sure it is 64-bit aligned, since we will are using 64 bit
	 * atomic operations on it
	 */
	used_segspace = (pg_atomic_uint64 *)(TYPEALIGN(sizeof(pg_atomic_uint64), shmem_base));
	Assert(0 == used_segspace->value);
}

/*
 * Returns the amount of shared memory needed for the WorkfileSegspace module
 */
Size
WorkfileSegspace_ShMemSize(void)
{
	/*
	 * Reserve 16 bytes instead of just 8. In case the pointer returned
	 * is not 64-bit aligned, we'll align it after allocation, and we might
	 * need the extra space.
	 */
	return 2 * sizeof(*used_segspace);
}

/*
 * Reserve 'bytes' bytes to write to disk
 *   This should be called before actually writing to disk
 *
 *   If enough disk space is available, increments the global counter and returns true
 *   Otherwise, sets the workfile_diskfull flag to true and returns false
 */
bool
WorkfileSegspace_Reserve(int64 bytes_to_reserve)
{
	Assert(NULL != used_segspace);

	int64 total = pg_atomic_add_fetch_u64(used_segspace, bytes_to_reserve);
	Assert(total >= (int64) 0);

	if (gp_workfile_limit_per_segment == 0)
	{
		/* not enforced */
		return true;
	}

	int64 max_allowed_diskspace = (int64) (gp_workfile_limit_per_segment * 1024);
	if (total <= max_allowed_diskspace)
	{
		return true;
	}

	/* We exceeded the logical limit. Revert the reserved space */
	(void) pg_atomic_sub_fetch_u64(used_segspace, bytes_to_reserve);

	workfileError = WORKFILE_ERROR_LIMIT_PER_SEGMENT;

	/* Set diskfull to true to stop any further attempts to write more data */
	WorkfileDiskspace_SetFull(true /* isFull */);
	return false;
}

/*
 * Notify of how many bytes were actually written to disk
 *
 * This should be called after writing to disk, with the actual number
 * of bytes written. This must be less or equal than the amount we reserved
 *
 * Returns the current used_diskspace after the commit
 */
void
WorkfileSegspace_Commit(int64 commit_bytes, int64 reserved_bytes)
{
	Assert(NULL != used_segspace);
	Assert(reserved_bytes >= commit_bytes);

#if USE_ASSERT_CHECKING
	int64 total = 
#endif
	pg_atomic_sub_fetch_u64(used_segspace, (reserved_bytes - commit_bytes));
	Assert(total >= (int64) 0);
}

/*
 * Returns the amount of disk space used for workfiles on this segment
 */
int64
WorkfileSegspace_GetSize()
{
	Assert(NULL != used_segspace);
	return used_segspace->value;
}

/* EOF */
