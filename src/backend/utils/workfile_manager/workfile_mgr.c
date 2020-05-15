/*-------------------------------------------------------------------------
 *
 * workfile_mgr.c
 *		Exposes information about BufFiles in shared memory
 *
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/workfile_manager/workfile_mgr.c
 *
 * NOTES:
 *
 * If a BufFile is created directly with BufFile* functions, it forms its
 * own workfile set.
 *
 *
 * There are three ways to use this:
 *
 * 1. Use the normal upstream BufFile functions, ignoring work files
 * altogether. A workfile set will be created for each BufFile automatically.
 * This ensures that files created by unmodified upstream code is tracked.
 *
 * 2. Use BufFile functions, and call XXX to add extra information to
 * the workfile set.
 *
 * 3. Use workfile_mgr_create_set() to create an explicit working set.
 *
 *
 * Workfile set is removed automatically when the last file associated with
 * it is closed.
 *
 *
 * The purpose of this tracking is twofold:
 * 1. Provide the gp_toolkit views, so that you can view temporary file
 * usage from a different psql session.
 *
 * 2. Enforce gp_* limits.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "cdb/cdbvars.h"
#include "funcapi.h"
#include "lib/ilist.h"
#include "storage/buffile.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "utils/builtins.h"
#include "utils/faultinjector.h"
#include "utils/workfile_mgr.h"

/*
 * GUC: Number of unique workfile sets that can be open at any time. Each
 * workfile set can contain multiple files, however. A hashjoin for example
 * will create one file per batch, but they all belong to the same workfile
 * set.
 */
int			gp_workfile_max_entries = 8192;

#define SizeOfWorkFileUsagePerQueryKey (2 * sizeof(int32));

typedef struct workfile_set WorkFileSetSharedEntry;

/*
 * Protected by WorkFileManagerLock (except for sizes, which use atomics)
 */
typedef struct
{
	dlist_head	freeList;
	dlist_head	activeList;
	int			num_active;

	uint64		total_bytes;

	HTAB	   *per_query_hash;

	WorkFileSetSharedEntry entries[];

} WorkFileShared;

static WorkFileShared *workfile_shared;

/* indexed by virtual File descriptor */
typedef struct
{
	WorkFileSetSharedEntry *work_set;	/* pointer into the shared array */
	int64		size;
} WorkFileLocalEntry;

static WorkFileLocalEntry *localEntries = NULL;
static int sizeLocalEntries = 0;

static workfile_set *workfile_mgr_create_set_internal(const char *operator_name, const char *prefix);

static void AtProcExit_WorkFile(int code, Datum arg);

static bool proc_exit_hook_registered = false;

Datum gp_workfile_mgr_cache_entries(PG_FUNCTION_ARGS);
Datum gp_workfile_mgr_used_diskspace(PG_FUNCTION_ARGS);

/*
 * Shared memory initialization
 */
Size
WorkFileShmemSize(void)
{
	Size		size;

	size = offsetof(WorkFileShared, entries);
	size = add_size(size, mul_size(gp_workfile_max_entries,
								   sizeof(WorkFileSetSharedEntry)));

	size = add_size(size, hash_estimate_size(gp_workfile_max_entries,
											 sizeof(WorkFileUsagePerQuery)));

	return size;
}

void
WorkFileShmemInit(void)
{
	Size		size;
	bool		found;

	size = offsetof(WorkFileShared, entries);
	size = add_size(size, mul_size(gp_workfile_max_entries,
								   sizeof(WorkFileSetSharedEntry)));

	workfile_shared = (WorkFileShared *)
		ShmemInitStruct("WorkFile Data", size, &found);
	if (!found)
	{
		HASHCTL hctl;

		memset(workfile_shared, 0, size);

		dlist_init(&workfile_shared->freeList);
		dlist_init(&workfile_shared->activeList);
		workfile_shared->num_active = 0;

		for (int i = 0; i < gp_workfile_max_entries; i++)
		{
			WorkFileSetSharedEntry *entry = &workfile_shared->entries[i];

			dlist_push_tail(&workfile_shared->freeList, &entry->node);
			entry->active = false;
		}

		/* Initialize per-query hashtable */
		memset(&hctl, 0, sizeof(hctl));
		hctl.keysize = SizeOfWorkFileUsagePerQueryKey;
		hctl.entrysize = sizeof(WorkFileUsagePerQuery);
		hctl.hash = tag_hash;

		workfile_shared->per_query_hash =
			ShmemInitHash("per-query workfile usage hash",
						  gp_workfile_max_entries,
						  gp_workfile_max_entries,
						  &hctl,
						  HASH_ELEM | HASH_FUNCTION);
	}
}

/*
 * Callback function, to delist all our temporary files from shared memory.
 *
 * We are about to exit, and the temporary files are about to deleted. We
 * have to delist them from shared memory before that, while we still
 * have access to shared memory. fd.c's on_proc_exit callback runs after
 * detaching from shared memory, which is too late.
 */
static void
AtProcExit_WorkFile(int code, Datum arg)
{
	int			i;

	for (i = 0; i < sizeLocalEntries; i++)
		WorkFileDeleted(i);
}

/*
 * Enlarge the local array if necessary, so that it has space for 'file'.
 */
static void
ensureLocalEntriesSize(File file)
{
	int			oldsize = sizeLocalEntries;
	int			newsize;

	if (file < 0)
		elog(ERROR, "invalid virtual file descriptor: %d", file);

	if (file < sizeLocalEntries)
		return;

	newsize = file * 2 + 5;
	if (sizeLocalEntries == 0)
		localEntries = (WorkFileLocalEntry *)
			MemoryContextAlloc(TopMemoryContext,
							   newsize * sizeof(WorkFileLocalEntry));
	else
		localEntries = (WorkFileLocalEntry *)
			repalloc(localEntries, newsize * sizeof(WorkFileLocalEntry));
	sizeLocalEntries = newsize;

	/* initialize the newly-allocated entries to all-zeros. */
	memset(&localEntries[oldsize], 0,
		   (newsize - oldsize) * sizeof(WorkFileLocalEntry));
}

/*
 * fd.c / buffile.c call these
 */

/*
 * Buffile.c calls this whenever a new BufFile is created, with a known
 * workfile set. Registers the file with the workfile set.
 */
void
RegisterFileWithSet(File file, workfile_set *work_set)
{
	WorkFileLocalEntry *localEntry;

	ensureLocalEntriesSize(file);
	localEntry = &localEntries[file];

	if (localEntry->work_set != NULL)
		elog(ERROR, "workfile is already registered with another workfile set");

	LWLockAcquire(WorkFileManagerLock, LW_EXCLUSIVE);

	Assert(work_set->active);
	Assert(work_set->perquery->active);

	localEntry->work_set = work_set;
	work_set->num_files++;
	work_set->perquery->num_files++;

	/* Enforce the limit on number of files */
	if (gp_workfile_limit_files_per_query > 0 &&
		work_set->perquery->num_files > gp_workfile_limit_files_per_query)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
				 errmsg("number of workfiles per query limit exceeded")));
	}

	LWLockRelease(WorkFileManagerLock);
}

/*
 * Update the totals of the workfile set for given file.
 *
 * If the given file is not associated with a workfile set yet,
 * a new workfile set is created.
 *
 * fd.c calls this whenever a (potentially) file is enlarged.
 */
void
UpdateWorkFileSize(File file, uint64 newsize)
{
	WorkFileLocalEntry *localEntry;
	WorkFileSetSharedEntry *work_set;
	WorkFileUsagePerQuery *perquery;
	int64		diff;

	ensureLocalEntriesSize(file);
	localEntry = &localEntries[file];

	diff = (int64) newsize - localEntry->size;
	if (diff == 0)
		return;

	if (!proc_exit_hook_registered)
	{
		/* register proc-exit hook to ensure temp files are dropped at exit */
		before_shmem_exit(AtProcExit_WorkFile, 0);
		proc_exit_hook_registered = true;
	}

	LWLockAcquire(WorkFileManagerLock, LW_EXCLUSIVE);

	/*
	 * If this file doesn't belong to a working set yet, create a new one,
	 * so that it can be tracked.
	 *
	 * XXX: perhaps we should skip this for very small files, to reduce
	 * overhead?
	 */
	if (localEntry->work_set == NULL)
	{
		Assert(localEntry->size == 0);

		work_set = workfile_mgr_create_set_internal(NULL, NULL);
		localEntry->work_set = work_set;
		work_set->num_files++;

		perquery = work_set->perquery;
		perquery->num_files++;

		/*
		 * Enforce the limit on number of files.
		 *
		 * We do this after associating the file with the set, so the
		 * shared memory will reflect the state where we are already over
		 * the limit. That seems accurate, we have already created the
		 * file, after all. Transaction abort will delete it shortly, so
		 * this is a very transient state.
		 */
		if (gp_workfile_limit_files_per_query > 0 &&
			perquery->num_files > gp_workfile_limit_files_per_query)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
					 errmsg("number of workfiles per query limit exceeded")));
		}

	}
	else
	{
		work_set = localEntry->work_set;
		perquery = work_set->perquery;
	}
	Assert(work_set->active);
	Assert(perquery->active);

	/*
	 * If the file is being enlarged, enforce the limits.
	 */
	if (diff > 0)
	{
		if (gp_workfile_limit_per_query > 0 &&
			(perquery->total_bytes + diff + 1023) / 1024 > gp_workfile_limit_per_query)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
					 errmsg("workfile per query size limit exceeded")));
		}
		if (gp_workfile_limit_per_segment &&
			(workfile_shared->total_bytes + diff) / 1024 > gp_workfile_limit_per_segment)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
					 errmsg("workfile per segment size limit exceeded")));
		}
	}

	/*
	 * Update the summaries in shared memory.
	 */
	work_set->total_bytes += diff;
	perquery->total_bytes += diff;
	workfile_shared->total_bytes += diff;

	/* also update the local entry */
	localEntry->size = newsize;

	LWLockRelease(WorkFileManagerLock);
}

void
WorkFileDeleted(File file)
{
	WorkFileLocalEntry *localEntry;
	WorkFileSetSharedEntry *work_set;
	WorkFileUsagePerQuery *perquery;
	int64		oldsize;

	if (file < 0)
		elog(ERROR, "invalid virtual file descriptor: %d", file);
	if (file >= sizeLocalEntries)
		return;		/* not a tracked work file */
	localEntry = &localEntries[file];

	if (!localEntry->work_set)
		return;		/* not a tracked work file */

	LWLockAcquire(WorkFileManagerLock, LW_EXCLUSIVE);

	work_set = localEntry->work_set;
	perquery = work_set->perquery;
	oldsize = localEntry->size;

	Assert(work_set->active);
	Assert(perquery->active);

	/*
	 * Update the summaries in shared memory
	 */
	Assert(work_set->num_files > 0);
	work_set->num_files--;
	work_set->total_bytes -= oldsize;

	Assert(perquery->num_files > 0);
	perquery->num_files--;
	perquery->total_bytes -= oldsize;

	workfile_shared->total_bytes -= oldsize;

	/*
	 * Update the workfile_set. If this was the last file in this
	 * set, remove it.
	 */
	if (work_set->num_files == 0)
	{
		perquery->refcount--;

		Assert(work_set->total_bytes == 0);
		/* Unlink from the active list */
		dlist_delete(&work_set->node);
		workfile_shared->num_active--;

		/* and add to the free list */
		dlist_push_head(&workfile_shared->freeList, &work_set->node);
		work_set->active = false;

		/*
		 * Similarly, update / remove the per-query entry.
		 */
		if (perquery->refcount == 0)
		{
			bool		found;

			Assert(perquery->total_bytes == 0);

			perquery->active = false;
			(void) hash_search(workfile_shared->per_query_hash,
							   perquery,
							   HASH_REMOVE, &found);
			if (!found)
				elog(PANIC, "per-query hash table is corrupt");
		}
	}

	localEntry->size = 0;
	localEntry->work_set = NULL;

	LWLockRelease(WorkFileManagerLock);
}


/*
 * Public API for creating a workfile set
 */
workfile_set *
workfile_mgr_create_set(const char *operator_name, const char *prefix)
{
	workfile_set *work_set;

	if (!proc_exit_hook_registered)
	{
		/* register proc-exit hook to ensure temp files are dropped at exit */
		before_shmem_exit(AtProcExit_WorkFile, 0);
		proc_exit_hook_registered = true;
	}

	LWLockAcquire(WorkFileManagerLock, LW_EXCLUSIVE);

	work_set = workfile_mgr_create_set_internal(operator_name, prefix);

	LWLockRelease(WorkFileManagerLock);

	return work_set;
}

/*
 * lock must be held
 */
static workfile_set *
workfile_mgr_create_set_internal(const char *operator_name, const char *prefix)
{
	WorkFileUsagePerQuery *perquery;
	bool		found;
	WorkFileUsagePerQuery key;
	workfile_set *work_set;

	Assert(proc_exit_hook_registered);

	if (dlist_is_empty(&workfile_shared->freeList))
	{
		/* Could not acquire another entry from the cache - we filled it up */
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
				 errmsg("could not create workfile manager entry: exceeded number of concurrent spilling queries")));
	}

	/*
	 * Find our per-query entry (or allocate, on first use)
	 */
	key.session_id = gp_session_id;
	key.command_count = gp_command_count;
	perquery = (WorkFileUsagePerQuery *) hash_search(workfile_shared->per_query_hash,
													 &key,
													 HASH_ENTER_NULL, &found);
	if (!perquery)
	{
		/*
		 * The hash table was full. (The hash table is sized so that this
		 * should never happen.)
		 */
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
				 errmsg("could not create workfile manager entry: per-query hash table is full")));
	}
	if (!found)
	{
		perquery->num_files = 0;
		perquery->total_bytes = 0;
		perquery->active = true;
	}
	perquery->refcount++;
	Assert(perquery->active);

	/*
	 * Allocate a workfile_set entry, and initialize it.
	 */
	work_set = dlist_container(WorkFileSetSharedEntry, node,
							   dlist_pop_head_node(&workfile_shared->freeList));
	Assert(!work_set->active);
	dlist_push_head(&workfile_shared->activeList, &work_set->node);
	workfile_shared->num_active++;

	work_set->session_id = gp_session_id;
	work_set->command_count = gp_command_count;
	work_set->slice_id = currentSliceId;
	work_set->perquery = perquery;
	work_set->num_files = 0;
	work_set->total_bytes = 0;
	work_set->active = true;

	if (operator_name)
		strlcpy(work_set->operator, operator_name, sizeof(work_set->operator));
	else
		work_set->operator[0] = '\0';

	if (prefix)
	{
		strlcpy(work_set->prefix, prefix, sizeof(work_set->prefix));
	}
	else if (operator_name)
	{
		snprintf(work_set->prefix, sizeof(work_set->prefix), "%s_%d",
				 operator_name,
				 (int) (work_set - workfile_shared->entries) + 1);
	}
	else
	{
		snprintf(work_set->prefix, sizeof(work_set->prefix), "workfile_set_%d",
				 (int) (work_set - workfile_shared->entries) + 1);
	}

	return work_set;
}

void
workfile_mgr_close_set(workfile_set *work_set)
{
	/* no op */
}

static void
workfile_report_inconsistency(void)
{
	dlist_node *node;
	dlist_iter		iter;
	int			num_freeList = 0;
	int			num_activeList = 0;
	dlist_foreach(iter, &workfile_shared->freeList)
	{
		num_freeList++;
		node = iter.cur;
		if (node->next == NULL || node->next->prev != node)
			ereport(LOG, (errmsg("workfile freeList is corrupted: "
							"node = %p, next = %p, next->prev = %p",
							node, node->next, node->next ? node->next->prev : NULL)));
	}
	if (!dlist_is_empty(&workfile_shared->freeList))
	{
		node = &workfile_shared->freeList.head;
		if (node->next == NULL || node->next->prev != node)
			ereport(LOG, (errmsg("workfile freeList is corrupted: "
							"node = %p, next = %p, next->prev = %p",
							node, node->next, node->next ? node->next->prev : NULL)));
	}
	dlist_foreach(iter, &workfile_shared->activeList)
	{
		num_activeList++;
		node = iter.cur;
		if (node->next == NULL || node->next->prev != node)
			ereport(LOG, (errmsg("workfile activeList is corrupted: "
							"node = %p, next = %p, next->prev = %p",
							node, node->next, node->next ? node->next->prev : NULL)));

	}
	if (!dlist_is_empty(&workfile_shared->activeList))
	{
		node = &workfile_shared->activeList.head;
		if (node->next == NULL || node->next->prev != node)
			ereport(LOG, (errmsg("workfile activeList is corrupted: "
							"node = %p, next = %p, next->prev = %p",
							node, node->next, node->next ? node->next->prev : NULL)));
	}

	ereport(PANIC,
		(errmsg("num_active = %d, the actual size = %d, total size = %d, free size = %d",
		  workfile_shared->num_active, num_activeList,
		  gp_workfile_max_entries, num_freeList)));
}

/*
 * Function copying all workfile cache entries for one segment
 */
workfile_set *
workfile_mgr_cache_entries_get_copy(int *num_active)
{
	int				num_entries;
	workfile_set	*copied_entries;
	dlist_iter		iter;
	int				i;

	LWLockAcquire(WorkFileManagerLock, LW_SHARED);

	num_entries = workfile_shared->num_active;
	copied_entries = (WorkFileSetSharedEntry *) palloc0(num_entries * sizeof(WorkFileSetSharedEntry));

	i = 0;
	dlist_foreach(iter, &workfile_shared->activeList)
	{
		WorkFileSetSharedEntry *e = dlist_container(WorkFileSetSharedEntry, node, iter.cur);

		memcpy(&copied_entries[i], e, sizeof(WorkFileSetSharedEntry));
		i++;
	}
	/* if i != num_entries, the shared memory for workfile is inconsistent. */
	if (i != num_entries)
		workfile_report_inconsistency();

	LWLockRelease(WorkFileManagerLock);

	*num_active = num_entries;
	return copied_entries;
}

/*
 * User-visible function, for the view
 */

typedef struct
{
	int			num_entries;
	int			index;

	WorkFileSetSharedEntry *copied_entries;
} get_entries_cxt;

/*
 * Function returning all workfile cache entries for one segment
 */
Datum
gp_workfile_mgr_cache_entries_internal(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	get_entries_cxt *cxt;

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/* Switch to memory context appropriate for multiple function calls */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/*
		 * Build a tuple descriptor for our result type
		 * The number and type of attributes have to match the definition of the
		 * view gp_workfile_mgr_cache_entries
		 */
#define NUM_CACHE_ENTRIES_ELEM 8
		TupleDesc tupdesc = CreateTemplateTupleDesc(NUM_CACHE_ENTRIES_ELEM, false);

		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "segid", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "prefix", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "size", INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "operation", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "slice", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "sessionid", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "commandid", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "numfiles", INT4OID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/*
		 * Now copy all the active entries from shared memory.
		 */
		cxt = (get_entries_cxt *) palloc(sizeof(get_entries_cxt));
		cxt->copied_entries = workfile_mgr_cache_entries_get_copy(&cxt->num_entries);
		cxt->index = 0;

		funcctx->user_fctx = cxt;
		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	cxt = (get_entries_cxt *) funcctx->user_fctx;

	while (cxt->index < cxt->num_entries)
	{
		WorkFileSetSharedEntry *work_set = &cxt->copied_entries[cxt->index];
		Datum		values[NUM_CACHE_ENTRIES_ELEM];
		bool		nulls[NUM_CACHE_ENTRIES_ELEM];
		HeapTuple tuple;
		Datum		result;

		MemSet(nulls, 0, sizeof(nulls));

		values[0] = Int32GetDatum(GpIdentity.segindex);
		values[1] = CStringGetTextDatum(work_set->prefix);
		values[2] = Int64GetDatum(work_set->total_bytes);
		values[3] = CStringGetTextDatum(work_set->operator);
		values[4] = UInt32GetDatum(work_set->slice_id);
		values[5] = UInt32GetDatum(work_set->session_id);
		values[6] = UInt32GetDatum(work_set->command_count);
		values[7] = UInt32GetDatum(work_set->num_files);

		cxt->index++;

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		SRF_RETURN_NEXT(funcctx, result);
	}

	SRF_RETURN_DONE(funcctx);
}

/*
 * Get the total number of bytes used across all workfiles
 */
uint64
WorkfileSegspace_GetSize(void)
{
	uint64		result;

	LWLockAcquire(WorkFileManagerLock, LW_SHARED);

	result = workfile_shared->total_bytes;

	LWLockRelease(WorkFileManagerLock);

	return result;
}
