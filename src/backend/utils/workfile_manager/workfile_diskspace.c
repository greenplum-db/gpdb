/*-------------------------------------------------------------------------
 *
 * workfile_diskspace.c
 *	 Implementation of workfile manager disk space accounting. This is
 *	 just a wrapper for WorkfileQueryspace and WorkfileSegspace
 *
 * Portions Copyright (c) 2012, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/workfile_manager/workfile_diskspace.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "storage/shmem.h"
#include "cdb/cdbvars.h"
#include "utils/workfile_mgr.h"

/*
 * This is set when a backend fails to write to a workfile,
 * presumably because of an out-of-diskspace error (logical or physical)
 */
static bool workfile_diskfull = false;

/*
 * Initialize shared memory area for the WorkfileDiskspace module
 */
void
WorkfileDiskspace_Init(void)
{
	WorkfileQueryspace_Init();
	WorkfileSegspace_Init();
	WorkfileDiskspace_SetFull(false /* isFull*/);
}

/*
 * Returns the amount of shared memory needed for the WorkfileDiskspace module
 */
Size
WorkfileDiskspace_ShMemSize(void)
{
	return add_size(WorkfileQueryspace_ShMemSize(), WorkfileSegspace_ShMemSize());
}

/*
 * Reserve 'bytes' bytes to write to disk
 *   This should be called before actually writing to disk
 *
 *   If enough disk space is available, increments the global counter and returns true
 *   Otherwise, returns false
 *
 *   When either the query limit or the segment limit are hit, the
 *   workfile_diskfull flag is set to true by the respective subsystem to
 *   prevent further writes.
 */
bool
WorkfileDiskspace_Reserve(int64 bytes_to_reserve)
{
	if (bytes_to_reserve == 0)
	{
		return true;
	}

	bool queryspace_reserved = true;
	bool segspace_reserved = true;

	/* Try the per-query limit first, then the per-segment */
	if (gp_workfile_limit_per_query > 0)
	{
		queryspace_reserved =  WorkfileQueryspace_Reserve(bytes_to_reserve);
	}

	if (queryspace_reserved && gp_workfile_limit_per_segment > 0)
	{
		segspace_reserved = WorkfileSegspace_Reserve(bytes_to_reserve);
	}

	return (queryspace_reserved && segspace_reserved);
}

/*
 * Notify of how many bytes were actually written to disk
 *
 * This should be called after writing to disk, with the actual number
 * of bytes written. This must be less or equal than the amount we reserved
 *
 * update_query_space is true if commit should also be applied to the queryspace
 *
 * Returns the current used_diskspace after the commit
 */
void
WorkfileDiskspace_Commit(int64 commit_bytes, int64 reserved_bytes, bool update_query_space)
{
	Assert(reserved_bytes >= commit_bytes);
	if (reserved_bytes == commit_bytes)
	{
		/* Nothing to do, save some work and just return */
		return;
	}

	if ((gp_workfile_limit_per_query > 0) && update_query_space)
	{
		WorkfileQueryspace_Commit(commit_bytes, reserved_bytes);
	}

	if (gp_workfile_limit_per_segment > 0)
	{
		WorkfileSegspace_Commit(commit_bytes, reserved_bytes);
	}
}

/*
 * Sets the flag that marks if on a segment we reached the allowed amount of
 * diskspace to use for workfiles (physical or logical).
 */
void
WorkfileDiskspace_SetFull(bool isFull)
{
	workfile_diskfull = isFull;
}

/*
 * Returns true if we hit the amount of diskspace allowed for workfiles on this
 * segment (physical or logical)
 */
bool
WorkfileDiskspace_IsFull(void)
{
	return workfile_diskfull;
}

/* EOF */
