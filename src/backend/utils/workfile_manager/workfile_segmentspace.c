/*-------------------------------------------------------------------------
 *
 * workfile_segmentspace.c
 *	 Implementation of workfile manager per-segment disk space accounting
 *
 * Copyright (c) 2012, EMC Corp.
 *
 *-------------------------------------------------------------------------
 */

#include <postgres.h>
#include "storage/shmem.h"
#include "utils/atomic.h"
#include "cdb/cdbvars.h"
#include "utils/workfile_mgr.h"
#include "miscadmin.h"

/* Name to identify the WorkfileSegspace shared memory area by */
#define WORKFILE_SEGSPACE_SHMEM_NAME "WorkfileSegspace"

/* Pointer to the shared memory counter with the total used diskspace across segment */
static int64 *used_segspace = NULL;

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

	used_segspace = (int64 *)shmem_base;
	Assert(0 == *used_segspace);
}

/*
 * Returns the amount of shared memory needed for the WorkfileSegspace module
 */
Size
WorkfileSegspace_ShMemSize(void)
{
	return sizeof(*used_segspace);
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

	int64 total = gp_atomic_add_int64(used_segspace, bytes_to_reserve);
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
	else
	{
		/* We exceeded the logical limit. Revert and try to evict */

		int crt_attempt = 0;
		while (crt_attempt < MAX_EVICT_ATTEMPTS)
		{

			/* Revert the reserved space */
			(void) gp_atomic_add_int64(used_segspace, - bytes_to_reserve);

			CHECK_FOR_INTERRUPTS();

			int64 requested_evict = Max(MIN_EVICT_SIZE, bytes_to_reserve);
			int64 size_evicted = workfile_mgr_evict(requested_evict);

			if (size_evicted < bytes_to_reserve)
			{
				workfileError = WORKFILE_ERROR_LIMIT_PER_SEGMENT;
				/*
				 * We couldn't evict as much as we need to write. Reservation
				 * failed, notify caller.
				 */
				elog(gp_workfile_caching_loglevel,
						"Failed to reserved size " INT64_FORMAT ". Reverted back to total " INT64_FORMAT,
						bytes_to_reserve, *used_segspace);

				/* Set diskfull to true to stop any further attempts to write more data */
				WorkfileDiskspace_SetFull(true /* isFull */);

				return false;
			}

			/* Try to reserve again */
			total = gp_atomic_add_int64(used_segspace, bytes_to_reserve);
			Assert(total >= (int64) 0);

			if (total <= max_allowed_diskspace)
			{
				/* Reservation successful, we're done */
				return true;
			}

			/*
			 * Someone else snatched the space after we evicted it.
			 * Loop around and try to evict again
			 */
			crt_attempt++;
		}
	}


	/*
	 * We exceeded max_eviction_attempts and did not manage to reserve.
	 * Set diskfull to true to stop any further attempts to write more data
	 * and notify the caller.
	 */
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
	gp_atomic_add_int64(used_segspace, (commit_bytes - reserved_bytes));
	Assert(total >= (int64) 0);
}

/*
 * Returns the amount of disk space used for workfiles on this segment
 */
int64
WorkfileSegspace_GetSize()
{
	Assert(NULL != used_segspace);
	return *used_segspace;
}

/* EOF */
