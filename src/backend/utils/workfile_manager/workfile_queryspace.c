/*-------------------------------------------------------------------------
 *
 * workfile_queryspace.c
 *	 Implementation of workfile manager per query disk space accounting
 *
 * Portions Copyright (c) 2013, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/workfile_manager/workfile_queryspace.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "port/atomics.h"
#include "storage/shmem.h"
#include "utils/workfile_mgr.h"
#include "utils/memutils.h"
#include "miscadmin.h"
#include "cdb/cdbvars.h"
#include "utils/syncrefhashtable.h"

/* Name to identify the WorkfileQueryspace shared memory area by */
#define WORKFILE_QUERYSPACE_SHMEM_NAME "WorkfileQueryspace Hashtable"

static bool isQueryspaceEltEmpty(const void *entry);
static void initQueryspaceElt(void *entry);

/* Workfile queryspace hashtable is stored here, once attached to */
SyncHT *queryspace_Hashtable = NULL;

/* Per-query entry is stored here once initialized */
QueryspaceDesc *queryEntry = NULL;

/*
 * Nesting level for executor invocations. Used to ensure we only
 * initialize/release the queryEntry at the outer most nesting level
 */
int32 querySpaceNestingLevel = 0;

/*
 * Initialize shared memory area for the WorkfileDiskspace module
 */
void
WorkfileQueryspace_Init(void)
{
	SyncHTCtl syncHTCtl;
	MemSet(&syncHTCtl, 0, sizeof(SyncHTCtl));

	syncHTCtl.entrySize = sizeof(QueryspaceDesc);
	syncHTCtl.keySize = sizeof(Queryspace_HashKey);

	syncHTCtl.hash = tag_hash;
	syncHTCtl.keyCopy = (HashCopyFunc) memcpy;
	syncHTCtl.match = (HashCompareFunc) memcmp;

	/*
	 * The maximum number of queries active at any time is <= MaxBackends.
	 * Using MaxBackends as the size of our hashtable
	 */
	syncHTCtl.numElements = MaxBackends;

	syncHTCtl.baseLWLockId = FirstWorkfileQuerySpaceLock;
	syncHTCtl.numPartitions = NUM_WORKFILE_QUERYSPACE_PARTITIONS;

	syncHTCtl.tabName = WORKFILE_QUERYSPACE_SHMEM_NAME;
	syncHTCtl.isEmptyEntry = isQueryspaceEltEmpty;
	syncHTCtl.initEntry = initQueryspaceElt;

	syncHTCtl.keyOffset = GPDB_OFFSET(QueryspaceDesc, key);
	syncHTCtl.pinCountOffset = GPDB_OFFSET(QueryspaceDesc, pinCount);

	/* Create hashtable in the top memory context so we can clean up if transaction aborts */
	MemoryContext oldcxt = MemoryContextSwitchTo(TopMemoryContext);

	queryspace_Hashtable = SyncHTCreate(&syncHTCtl);
	Assert(NULL != queryspace_Hashtable);

	MemoryContextSwitchTo(oldcxt);
}

/*
 * Returns the amount of shared memory needed for the WorkfileQueryspace module
 */
Size
WorkfileQueryspace_ShMemSize(void)
{
	return hash_estimate_size(MaxBackends, sizeof(QueryspaceDesc));
}


/*
 * Returns the amount of disk space used for workfiles for the query
 * uniquely identified by (session_id, command_count)
 *
 * Returns -1 if no corresponding entry found.
 */
int64
WorkfileQueryspace_GetSize(int session_id, int command_count)
{
	Queryspace_HashKey queryKey;
	queryKey.session_id = session_id;
	queryKey.command_count = command_count;

	QueryspaceDesc *queryEntry = (QueryspaceDesc *) SyncHTLookup(queryspace_Hashtable, &queryKey);

	int64 size = -1;
	if (NULL != queryEntry)
	{
		size = queryEntry->queryDiskspace;
		SyncHTRelease(queryspace_Hashtable, queryEntry);
	}


	return size;
}

/*
 * Reserve 'bytes' bytes to write to disk
 *   This should be called before actually writing to disk
 *
 *   If enough disk space is available, increments the per-query counter and returns true
 *   Otherwise, returns false
 */
bool
WorkfileQueryspace_Reserve(int64 bytes_to_reserve)
{
	bool success = false;

	if (NULL == queryEntry)
	{
		/*
		 * Did not find entry for this query, it must be a utility query
		 * that is spilling. No enforcing of per-query disk limit for these.
		 */
		return true;
	}

	int64 total = pg_atomic_add_fetch_u64((pg_atomic_uint64 *)&queryEntry->queryDiskspace, bytes_to_reserve);
	Assert(total >= (int64) 0);

	if (gp_workfile_limit_per_query == 0)
	{
		/* not enforced */
		success = true;
	}
	else
	{
		int64 max_allowed_diskspace = (int64) (gp_workfile_limit_per_query * 1024);
		success = (total <= max_allowed_diskspace);

		if (!success)
		{
			workfileError = WORKFILE_ERROR_LIMIT_PER_QUERY;

			/* Revert the reserved space */
			pg_atomic_sub_fetch_u64((pg_atomic_uint64 *)&queryEntry->queryDiskspace, bytes_to_reserve);
			/* Set diskfull to true to stop any further attempts to write more data */
			WorkfileDiskspace_SetFull(true /* isFull */);
		}
	}

	return success;
}
/*
 * Notify of how many bytes were actually written to disk
 *
 * This should be called after writing to disk, with the actual number
 * of bytes written. This must be less or equal than the amount we reserved
 *
 */
void
WorkfileQueryspace_Commit(int64 commit_bytes, int64 reserved_bytes)
{
	Assert(reserved_bytes >= commit_bytes);

	if (reserved_bytes == commit_bytes)
	{
		/* Nothing to do, save a hashtable look-up and just return */
		return;
	}

	/*
	 * Query might not have an entry in the queryspace hashtable if it is a
	 * utility query that is spilling.
	 * No enforcing of per-query disk limit for these.
	 */
	if (NULL != queryEntry)
	{
#if USE_ASSERT_CHECKING
		int64 total =
#endif
		pg_atomic_sub_fetch_u64((pg_atomic_uint64 *)&queryEntry->queryDiskspace, (reserved_bytes - commit_bytes));
		Assert(total >= (int64) 0);
	}
}

/*
 * Increment the number of workfiles created and check
 * if the limit has been reached.
 *
 * Returns true if we're still below the limit, false if we hit the limit.
 */
bool
WorkfileQueryspace_AddWorkfile(void)
{
	if (NULL == queryEntry)
	{
		/*
		 * Did not find entry for this query, it must be a utility query
		 * that is spilling. No enforcing of per-query disk limit for these.
		 */
		return true;
	}

	int32 workfilesCreated = pg_atomic_add_fetch_u32((pg_atomic_uint32 *)&queryEntry->workfilesCreated, 1);

	elog(gp_workfile_caching_loglevel, "Creating new workfile, query file count = %d", workfilesCreated);

	if (gp_workfile_limit_files_per_query > 0 &&
			workfilesCreated > gp_workfile_limit_files_per_query)
	{
		workfileError = WORKFILE_ERROR_LIMIT_FILES_PER_QUERY;
		return false;
	}

	return true;
}

/*
 * Decrement the number of workfiles created. This should be called when
 * closing and deleting a workfile.
 */
void
WorkfileQueryspace_SubtractWorkfile(int32 nFiles)
{
	if (NULL == queryEntry)
	{
		/*
		 * Did not find entry for this query, it must be a utility query
		 * that is spilling. No enforcing of per-query disk limit for these.
		 */
		return;
	}

	/* Subtract the number of files atomically */
	int32 workfilesLeft = pg_atomic_sub_fetch_u32((pg_atomic_uint32 *)&queryEntry->workfilesCreated, nFiles);
	elog(gp_workfile_caching_loglevel, "Closing %d workfiles, query file count = %d",
			nFiles, workfilesLeft);
	Assert(workfilesLeft >= 0);

}

/*
 * Initializes and returns the Queryspace entry for this query.
 *
 * If the entry already exists, it returns the existing one, otherwise
 * it inserts a new one in the hashtable.
 */
QueryspaceDesc *
WorkfileQueryspace_InitEntry(int session_id, int command_count)
{
	Assert(querySpaceNestingLevel >= 0);

	if (gp_workfile_limit_per_query == 0
			&& gp_workfile_limit_files_per_query == 0)
	{
		/* Per-query limits not enforced, don't allocate hashtable entry */
		return NULL;
	}

	querySpaceNestingLevel++;

	if (querySpaceNestingLevel > 1)
	{
		/*
		 * We are in a nested Executor invocation. Don't allocate a new
		 * entry, used the stored one that was created by the top-level
		 * invocation.
		 */
		return queryEntry;
	}

	Queryspace_HashKey queryKey;
	queryKey.session_id = session_id;
	queryKey.command_count = command_count;
	bool existing = false;


	Assert(NULL == queryEntry);
	queryEntry = SyncHTInsert(queryspace_Hashtable, &queryKey, &existing);

	if (!existing)
	{
		elog(gp_workfile_caching_loglevel, "Inserted entry for query (sessionid=%d, commandcnt=%d)",
				queryEntry->key.session_id, queryEntry->key.command_count);
	}

	return queryEntry;
}

/*
 * Releases the corresponding Query space entry for this query
 *
 */
void
WorkfileQueryspace_ReleaseEntry(void)
{

	if (NULL == queryEntry)
	{
		/* Already released, nothing to do */
		return;
	}

	querySpaceNestingLevel--;

	if (querySpaceNestingLevel > 0)
	{
		/*
		 * We are in a nested Executor invocation. Don't release
		 * the entry, the top-level invocation will take care of that.
		 */
		return;
	}

	int session_id = queryEntry->key.session_id;
	int command_count = queryEntry->key.command_count;

	bool deleted = SyncHTRelease(queryspace_Hashtable, queryEntry);

	/*
	 * Set the queryEntry to NULL as we may soon CHECK_FOR_INTERRUPTS, which can come back to this
	 * method if an ERROR cleanup (e.g., because of a QueryCancelPending) invokes ExecutorEnd.
	 * We don't want to release our queryEntry again.
	 */
	queryEntry = NULL;

	if (deleted)
	{
		elog(gp_workfile_caching_loglevel, "Deleted entry for query (sessionid=%d, commandcnt=%d)",
				session_id, command_count);
	}

	Assert(querySpaceNestingLevel == 0);
}

/*
 * Callback function to test if an entry in the hashtable is "empty"
 */
static bool
isQueryspaceEltEmpty(const void *entry)
{
	/*
	 * Queryspace hashtable entries can always be removed when
	 * the last backend releases it
	 */
	return true;
}

/*
 * Callback function to initialize an entry before returning to the caller
 */
static void
initQueryspaceElt(void *entry)
{
	QueryspaceDesc *hashElt = (QueryspaceDesc *) entry;
	hashElt->queryDiskspace = 0L;
	hashElt->chunksReserved = 0;
	hashElt->workfilesCreated = 0;
	hashElt->pinCount = 0;
}
