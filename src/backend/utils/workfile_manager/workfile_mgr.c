/*-------------------------------------------------------------------------
 *
 * workfile_mgr.c
 *		Exposes information about BufFiles in shared memory
 *
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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
 * it is closed excepted the pinned workfile_set, which should be removed by
 * workfile_mgr_close_set().
 *
 * Distinguish by the workfile_set life cycle, the workfile_set could be class
 * into several classes.
 * 1. workfile_set for a temporary workfile with interXact false(see buffile.c).
 * Once the file closed, the workfile_set will also get freed.
 * And if current transaction aborted, the workfile's resource owner is
 * responsible for closing it if the workfile is leaked, which also removes
 * the workfile_set.
 *
 * 2. workfile_set for a temporary workfile with interXact true(see buffile.c).
 * Once the file closed, the workfile_set will also get removed.
 * Since the workfile doesn't register to a resource owner, WorkFileLocalCtl
 * is responsible for removing its corresponding workfile_set in
 * AtProcExit_WorkFile(which gets called when process exit).
 *
 * 3. workfile_set is pinned and saved to prevent unexpected free operation.
 * When the workfile_set gets pinned, after all the file in the workfile_set
 * gets closed, we don't free the workfile_set, cause we may register new workfile
 * later. Call workfile_mgr_close_set to explicitly remove the workfile_set.
 * To deal with aborting, since the resource owner of the workfiles in the
 * workfile_set will close the workfiles, and this operation will not
 * remove workfile_set when no file exists in the workfile_set. We have to free
 * these kinds of workfile_sets when transaction ends in AtEOXact_WorkFile.
 * If the workfile_set contains interXact workfiles, we still rely on
 * WorkFileLocalCtl to remove workfile_sets in AtProcExit_WorkFile. But currently,
 * we don't have this kind of usage.
 *
 * NOTE: if a workfile_set gets created but no workfile register into it(in case of
 * error, RegisterFileWithSet not called), it will never have a chance to be removed
 * automatically through file close. Then we have to check and remove these
 * workfile_sets when the transaction end to prevent overflow in AtEOXact_WorkFile.
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
#include "storage/lwlock.h"
#include "storage/shmem.h"
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

/*
 * WorkFileLocalEntry - track workfile_set for each workfile.
 *
 * work_set - the workfile_set in shared memory for a workfile.
 * size - the size of the workfile.
 */
typedef struct
{
	WorkFileSetSharedEntry	   *work_set;
	int64						size;
} WorkFileLocalEntry;

/*
 * WorkFileLocalCtl, local control struct for workfiles
 *
 * entries - The workfile created in current process will be stored in the the entries
 * indexed by virtual File descriptor.
 * sizeLocalEntries - The total number of workfiles created.
 * localList - track the workfile_sets created in current process.
 */
typedef struct
{
	WorkFileLocalEntry	   *entries;
	int						sizeLocalEntries;
	bool					initialized;
	dlist_head				localList;
} WorkFileLocalCtl;

static WorkFileLocalCtl localCtl = {NULL, 0, false};

static void remove_workfile_set_if_possible(workfile_set* work_set);
static workfile_set *workfile_mgr_create_set_internal(const char *operator_name, const char *prefix);
static void pin_workset(workfile_set *work_set);
static void unpin_workset(workfile_set *work_set);

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
	dlist_mutable_iter		iter;

	if (!localCtl.initialized)
		return;
	dlist_foreach_modify(iter, &localCtl.localList)
	{
		workfile_set	   *work_set = dlist_container(workfile_set, local_node, iter.cur);
		workfile_mgr_close_set(work_set);
	}
}

/*
 * Cleanup the workfile_sets at the end of transaction.
 * When transaction aborts, we should clean workfile_sets which
 * don't contain any workfiles.
 * Here we don't care about whether resource owner close workfiles first,
 * or we unpin the workset first. Under either order, the expected workfile_set
 * should be freed. Currently we put it after resource owner release logic.
*/
void
AtEOXact_WorkFile(void)
{
	dlist_mutable_iter		iter;

	if (!localCtl.initialized)
		return;

	LWLockAcquire(WorkFileManagerLock, LW_EXCLUSIVE);
	dlist_foreach_modify(iter, &localCtl.localList)
	{
		workfile_set	   *work_set = dlist_container(workfile_set, local_node, iter.cur);
		Assert(work_set->active && work_set->perquery->active);

		/*
		 * Currently, all pinned workfile_sets are calling workfile_mgr_close_set
		 * in transaction. So we should unpin it for aborting.
		 */
		unpin_workset(work_set);

		/*Remove the workfile_sets that don't contain any workfiles */
		remove_workfile_set_if_possible(work_set);
	}
	LWLockRelease(WorkFileManagerLock);
}

/*
 * Enlarge the local array if necessary, so that it has space for 'file'.
 */
static void
ensureLocalEntriesSize(File file)
{
	int			oldsize = localCtl.sizeLocalEntries;
	int			newsize;

	if (file < 0)
		elog(ERROR, "invalid virtual file descriptor: %d", file);

	if (file < oldsize)
		return;

	newsize = file * 2 + 5;
	if (oldsize == 0)
	{
		localCtl.entries = (WorkFileLocalEntry *)
			MemoryContextAlloc(TopMemoryContext,
							   newsize * sizeof(WorkFileLocalEntry));
	}
	else
		localCtl.entries = (WorkFileLocalEntry *)
			repalloc(localCtl.entries, newsize * sizeof(WorkFileLocalEntry));
	localCtl.sizeLocalEntries = newsize;

	/* initialize the newly-allocated entries to all-zeros. */
	memset(&localCtl.entries[oldsize], 0,
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
	ensureLocalEntriesSize(file);

	if (localCtl.entries[file].work_set != NULL)
		elog(ERROR, "workfile is already registered with another workfile set");

	LWLockAcquire(WorkFileManagerLock, LW_EXCLUSIVE);

	if (!work_set->active || !work_set->perquery->active)
		ereport(PANIC,
				(errmsg("Register file to a non-active workfile_set/per-query summary is illegal")));

	localCtl.entries[file].work_set = work_set;
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
	localEntry = &localCtl.entries[file];

	work_set = localEntry->work_set;
	/*
	 * We got here only when the file's fdstate is FD_WORKFILE, and that
	 * means the file is registered to a work set.
	 */
	Assert(work_set);
	perquery = work_set->perquery;

	diff = (int64) newsize - localEntry->size;
	if (diff == 0)
		return;

	LWLockAcquire(WorkFileManagerLock, LW_EXCLUSIVE);

	if (!work_set->active)
	{
		ereport(WARNING,
				(errmsg("workfile_set %s for current file is freed which should not happen.", work_set->prefix),
				 errprintstack(true)));
		LWLockRelease(WorkFileManagerLock);
		return;
	}

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

/*
 * WorkFileDeleted - Delete the file from it's workfile_set, update
 * the stats for the workfile_set.
 *
 * If the file is the last one in the workfile_set, and it's workfile_set
 * is not pinned, free the workfile set.
 */
void
WorkFileDeleted(File file, bool hold_lock)
{
	WorkFileLocalEntry *localEntry;
	WorkFileSetSharedEntry *work_set;
	WorkFileUsagePerQuery *perquery;
	int64		oldsize;

	if (file < 0)
		elog(ERROR, "invalid virtual file descriptor: %d", file);
	if (file >= localCtl.sizeLocalEntries)
		return;		/* not a tracked work file */
	localEntry = &localCtl.entries[file];

	if (!localEntry->work_set)
		return;		/* not a tracked work file */

	if (hold_lock)
		LWLockAcquire(WorkFileManagerLock, LW_EXCLUSIVE);

	work_set = localEntry->work_set;
	perquery = work_set->perquery;

	if (!work_set->active || !work_set->perquery->active)
		ereport(PANIC,
				(errmsg("workfile_set/per-query summarry is not active")));

	/*
	 * Update the summaries in shared memory
	 */
	oldsize = localEntry->size;
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
	remove_workfile_set_if_possible(work_set);

	localEntry->size = 0;
	localEntry->work_set = NULL;

	if (hold_lock)
		LWLockRelease(WorkFileManagerLock);
}

/*
 * Remove the workfile_set if possible. The workfile_set must be unpinned
 * and no workfile remains.
 * WorkFileManagerLock must be held.
 */
static void
remove_workfile_set_if_possible(workfile_set* work_set)
{
	WorkFileUsagePerQuery *perquery = work_set->perquery;

	/*
	 * Update the workfile_set. If there was no file exist in this
	 * set, remove it unless the set is pinned.
	 */
	if (work_set->num_files == 0 && !work_set->pinned)
	{
		perquery->refcount--;

		Assert(work_set->total_bytes == 0);
		/* Unlink from the active list */
		dlist_delete(&work_set->node);
		workfile_shared->num_active--;

		/* and add to the free list */
		dlist_push_head(&workfile_shared->freeList, &work_set->node);
		work_set->active = false;

		dlist_delete(&work_set->local_node);

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
}


/*
 * Public API for creating a workfile set
 *
 * When hold_pin is set to true, the caller should use workfile_mgr_close_set
 * to remove the workfile_set.
 */
workfile_set *
workfile_mgr_create_set(const char *operator_name, const char *prefix, bool hold_pin)
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

	if (hold_pin)
		pin_workset(work_set);

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
	work_set->pinned = false;

	/* Track all workfile_sets created in current process */
	if (!localCtl.initialized)
	{
		dlist_init(&localCtl.localList);
		localCtl.initialized = true;
	}
	dlist_push_tail(&localCtl.localList, &work_set->local_node);

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

/*
 * Pin the workfile_set to prevent unexpected free operation.
 */
static void
pin_workset(workfile_set *work_set)
{
	work_set->pinned = true;
}

/*
 * Unping the workfile_set.
 */
static void
unpin_workset(workfile_set *work_set)
{
	work_set->pinned = false;
}

/*
 * workfile_mgr_close_set - force close and free the workfile_set.
 *
 * Normally, for unpinned workfile_set, it'll get freed until the last
 * file closed in it. workfile_mgr_close_set will force free the workfile_set
 * not matter it gets pinned or not.
 */
void
workfile_mgr_close_set(workfile_set *work_set)
{
	int			i;

	LWLockAcquire(WorkFileManagerLock, LW_EXCLUSIVE);

	if (!work_set->active)
		ereport(PANIC, (errmsg("workfile_set is not active")));

	unpin_workset(work_set);

	/* Delete files which in current workfile set */
	for (i = 0; i < localCtl.sizeLocalEntries; i++)
	{
		if (localCtl.entries[i].work_set == work_set)
			WorkFileDeleted(i, false);
	}

	/*
	 * Since the workfile_mgr_close_set could be called at any moment to
	 * force close and free the workfile_set, so current workfile_set
	 * could have empty registered file yet.
	 */
	if (work_set->active)
	{
		if (work_set->num_files > 0)
		{
			WorkFileUsagePerQuery *perquery = work_set->perquery;

			ereport(WARNING,
					(errmsg("workfile_set %s still contains files for unknow reason.", work_set->prefix),
					 errprintstack(true)));

			if (!perquery->active)
				ereport(PANIC, (errmsg("per-query summarry is not active")));

			Assert(work_set->total_bytes > 0);
			Assert(perquery->num_files > 0);
			perquery->num_files -= work_set->num_files;
			perquery->total_bytes -= work_set->total_bytes;

			work_set->num_files = 0;
			work_set->total_bytes = 0;
		}

		Assert(work_set->num_files == 0);
		remove_workfile_set_if_possible(work_set);
	}

	LWLockRelease(WorkFileManagerLock);
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
		TupleDesc tupdesc = CreateTemplateTupleDesc(NUM_CACHE_ENTRIES_ELEM);

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
