/*-------------------------------------------------------------------------
 *
 * workfile_mgr.c
 *	 Implementation of workfile manager and workfile caching.
 *
 * Portions Copyright (c) 2011, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/workfile_manager/workfile_mgr.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <unistd.h>
#include <sys/stat.h>

#include "utils/workfile_mgr.h"
#include "miscadmin.h"
#include "cdb/cdbvars.h"
#include "nodes/print.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "postmaster/primary_mirror_mode.h"
#include "utils/debugbreak.h"

#define WORKFILE_SET_MASK  "XXXXXXXXXX"

/* Type of temp file to use for storing the plan */
#define WORKFILE_PLAN_FILE_TYPE BUFFILE

/* Information needed to populate a new workfile_set structure */
typedef struct workset_info
{
	ExecWorkFileType file_type;
	NodeTag nodeType;
	TimestampTz session_start_time;
	uint64 operator_work_mem;
	char *dir_path;
} workset_info;

/* Counter to keep track of workfile segspace used without a workfile set. */
static int64 used_segspace_not_in_workfile_set;

/* Forward declarations */
static void workfile_mgr_populate_set(const void *resource, const void *param);
static void workfile_mgr_cleanup_set(const void *resource);
static void workfile_mgr_delete_set_directory(char *workset_path);
static void workfile_mgr_unlink_directory(const char *dirpath);
static StringInfo get_name_from_nodeType(const NodeTag node_type);
static uint64 get_operator_work_mem(PlanState *ps);
static char *create_workset_directory(NodeTag node_type, int slice_id);

static workfile_set *open_workfile_sets = NULL;
static bool workfile_sets_resowner_callback_registered = false;


static void
workfile_set_free_callback(ResourceReleasePhase phase,
					 bool isCommit,
					 bool isTopLevel,
					 void *arg)
{
	workfile_set *curr;
	workfile_set *next;

	if (phase != RESOURCE_RELEASE_AFTER_LOCKS)
		return;

	next = open_workfile_sets;
	while (next)
	{
		curr = next;
		next = curr->next;

		if (curr->owner == CurrentResourceOwner)
		{
			if (isCommit)
				elog(WARNING, "workfile_set reference leak: %p still referenced", curr);
			workfile_mgr_close_set(curr);
		}
	}
}


/* Workfile manager cache is stored here, once attached to */
Cache *workfile_mgr_cache = NULL;

/* Workfile error type */
WorkfileError workfileError = WORKFILE_ERROR_UNKNOWN;

/*
 * Initialize the cache in shared memory, or attach to an existing one
 *
 */
void
workfile_mgr_cache_init(void)
{
	CacheCtl cacheCtl;
	MemSet(&cacheCtl, 0, sizeof(CacheCtl));

	cacheCtl.maxSize = gp_workfile_max_entries;
	cacheCtl.cacheName = "Workfile Manager Cache";
	cacheCtl.entrySize = sizeof(workfile_set);
	cacheCtl.keySize = sizeof(((workfile_set *)0)->key);
	cacheCtl.keyOffset = GPDB_OFFSET(workfile_set, key);

	cacheCtl.hash = int32_hash;
	cacheCtl.keyCopy = (HashCopyFunc) memcpy;
	cacheCtl.match = (HashCompareFunc) memcmp;
	cacheCtl.cleanupEntry = workfile_mgr_cleanup_set;
	cacheCtl.populateEntry = workfile_mgr_populate_set;

	cacheCtl.baseLWLockId = FirstWorkfileMgrLock;
	cacheCtl.numPartitions = NUM_WORKFILEMGR_PARTITIONS;

	workfile_mgr_cache = Cache_Create(&cacheCtl);
	Assert(NULL != workfile_mgr_cache);

	/*
	 * Initialize the WorkfileDiskspace and WorkfileQueryspace APIs
	 * to track disk space usage
	 */
	WorkfileDiskspace_Init();

	used_segspace_not_in_workfile_set = 0;
}

/*
 * Returns pointer to the workfile manager cache
 */
Cache *
workfile_mgr_get_cache(void)
{
	Assert(NULL != workfile_mgr_cache);
	return workfile_mgr_cache;
}

/*
 * compute the size of shared memory for the workfile manager
 */
Size
workfile_mgr_shmem_size(void)
{
	return Cache_SharedMemSize(gp_workfile_max_entries, sizeof(workfile_set)) +
			WorkfileDiskspace_ShMemSize() + WorkfileQueryspace_ShMemSize();
}



/*
 * Retrieves the operator name.
 * Result is palloc-ed in the current memory context.
 */
static StringInfo
get_name_from_nodeType(const NodeTag node_type)
{
	StringInfo operator_name = makeStringInfo();

	switch ( node_type )
	{
		case T_AggState:
			appendStringInfoString(operator_name,"Agg");
			break;
		case T_HashJoinState:
			appendStringInfoString(operator_name,"HashJoin");
			break;
		case T_MaterialState:
			appendStringInfoString(operator_name,"Material");
			break;
		case T_SortState:
			appendStringInfoString(operator_name,"Sort");
			break;
		case T_Invalid:
			/* When spilling from a builtin function, we don't have a valid node type */
			appendStringInfoString(operator_name,"BuiltinFunction");
			break;
		default:
			Assert(false && "Operator not supported by the workfile manager");
	}

	return operator_name;
}

/*
 * Create a new file set
 *   type is the WorkFileType for the files: BUFFILE or BFZ
 *   can_be_reused: if set to false, then we don't insert this set into the cache,
 *     since the caller is telling us there is no point. This can happen for
 *     example when spilling during index creation.
 *   ps is the PlanState for the subtree rooted at the operator
 *   snapshot contains snapshot information for the current transaction
 *
 */
workfile_set *
workfile_mgr_create_set(enum ExecWorkFileType type, bool can_be_reused, PlanState *ps)
{
	Assert(NULL != workfile_mgr_cache);

	Plan *plan = NULL;
	if (ps != NULL)
	{
		plan = ps->plan;
	}

	AssertImply(can_be_reused, plan != NULL);

	NodeTag node_type = T_Invalid;
	if (ps != NULL)
	{
		node_type = ps->type;
	}
	char *dir_path = create_workset_directory(node_type, currentSliceId);


	if (!workfile_sets_resowner_callback_registered)
	{
		RegisterResourceReleaseCallback(workfile_set_free_callback, NULL);
		workfile_sets_resowner_callback_registered = true;
	}

	/* Create parameter info for the populate function */
	workset_info set_info;
	set_info.file_type = type;
	set_info.nodeType = node_type;
	set_info.dir_path = dir_path;
	set_info.session_start_time = GetCurrentTimestamp();
	set_info.operator_work_mem = get_operator_work_mem(ps);

	CacheEntry *newEntry = Cache_AcquireEntry(workfile_mgr_cache, &set_info);

	if (NULL == newEntry)
	{
		/* Clean up the directory we created. */
		workfile_mgr_delete_set_directory(dir_path);

		/* Could not acquire another entry from the cache - we filled it up */
		ereport(ERROR,
				(errmsg("could not create workfile manager entry: exceeded number of concurrent spilling queries")));
	}

	/* Path has now been copied to the workfile_set. We can free it */
	pfree(dir_path);

	/* Complete initialization of the entry with post-acquire actions */
	Assert(NULL != newEntry);
	workfile_set *work_set = CACHE_ENTRY_PAYLOAD(newEntry);
	Assert(work_set != NULL);

	elog(gp_workfile_caching_loglevel, "new spill file set. key=0x%x prefix=%s opMemKB=" INT64_FORMAT,
			work_set->key, work_set->path, work_set->metadata.operator_work_mem);

	return work_set;
}

/*
 * Creates the workset directory and returns the path.
 * Throws an error if path or directory cannot be created.
 *
 * Returns the name of the directory created.
 * The name returned is palloc-ed in the current memory context.
 *
 */
static char *
create_workset_directory(NodeTag node_type, int slice_id)
{
	/* Create base directory here. We need database relative path */
	StringInfo tmp_dirpath = makeStringInfo();

	appendStringInfo(tmp_dirpath,
					"%s/%s",
					getCurrentTempFilePath,
					PG_TEMP_FILES_DIR);

	if (tmp_dirpath->len > MAXPGPATH)
	{
		ereport(ERROR, (errmsg("cannot generate path %s/%s",
				getCurrentTempFilePath,
				PG_TEMP_FILES_DIR)));
	}

	mkdir(tmp_dirpath->data, S_IRWXU);
	pfree(tmp_dirpath->data);
	pfree(tmp_dirpath);

	/* Create workset directory here */
	StringInfo operator_name = get_name_from_nodeType(node_type);
 	StringInfo workset_path_masked = makeStringInfo();

	appendStringInfo(workset_path_masked,
			"%s/%s/%s_%s_Slice%d.%s",
			 getCurrentTempFilePath,
			 PG_TEMP_FILES_DIR,
			 WORKFILE_SET_PREFIX,
			 operator_name->data,
			 slice_id,
			 WORKFILE_SET_MASK);

	if (workset_path_masked->len > MAXPGPATH)
	{
		ereport(ERROR, (errmsg("cannot generate path %s/%s/%s_%s_Slice%d.%s",
			getCurrentTempFilePath,
			PG_TEMP_FILES_DIR,
			WORKFILE_SET_PREFIX,
			operator_name->data,
			slice_id,
			WORKFILE_SET_MASK)));
	}

	char *workset_path_unmasked = gp_mkdtemp(workset_path_masked->data);
	if (workset_path_unmasked == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				errmsg("could not create spill file directory: %m")));
	}

	char *final_path = (char *) palloc0(MAXPGPATH);

	/* Initialize result path. Strip prefix from path since bfz/fd add the getCurrentTempFilePath to it */
	strlcpy(final_path,
			workset_path_unmasked + strlen(getCurrentTempFilePath) + 1,
			MAXPGPATH);

	if ( strlen(workset_path_unmasked + strlen(getCurrentTempFilePath) + 1)
			> MAXPGPATH )
	{
			ereport(ERROR, (errmsg("cannot generate path %s",
					workset_path_unmasked + strlen(getCurrentTempFilePath) + 1)));
	}

	pfree(workset_path_masked->data);
	pfree(workset_path_masked);
	pfree(operator_name->data);
	pfree(operator_name);

	return final_path;
}

/*
 * SharedCache callback. Populates a newly acquired workfile_set before
 * returning it to the caller.
 */
static void
workfile_mgr_populate_set(const void *resource, const void *param)
{
	Assert(NULL != resource);
	Assert(NULL != param);

	workfile_set *work_set = (workfile_set *) resource;
	workset_info *set_info = (workset_info *) param;

	work_set->metadata.operator_work_mem = set_info->operator_work_mem;

	work_set->no_files = 0;
	work_set->size = 0L;
	work_set->in_progress_size = 0L;
	work_set->node_type = set_info->nodeType;
	work_set->metadata.type = set_info->file_type;
	work_set->metadata.bfz_compress_type = gp_workfile_compress_algorithm;
	work_set->metadata.num_leaf_files = 0;
	work_set->slice_id = currentSliceId;
	work_set->session_id = gp_session_id;
	work_set->command_count = gp_command_count;
	work_set->session_start_time = set_info->session_start_time;

	work_set->owner = CurrentResourceOwner;
	work_set->next = open_workfile_sets;
	work_set->prev = NULL;
	if (open_workfile_sets)
		open_workfile_sets->prev = work_set;
	open_workfile_sets = work_set;

	Assert(strlen(set_info->dir_path) < MAXPGPATH);
	strlcpy(work_set->path, set_info->dir_path, MAXPGPATH);
}

/*
 * Determine operatorMemKB for this operator.
 * For HashJoin, this is given by the right child, for everyone else it is the actual node.
 *
 * If PlanState is NULL (e.g. when spilling from a built-in function), return 0.
 */
static uint64
get_operator_work_mem(PlanState *ps)
{
	if (NULL == ps)
	{
		return 0;
	}

	PlanState *psOp = ps;
	if (IsA(ps,HashJoinState))
	{
		Assert(IsA(ps->righttree, HashState));
		psOp = ps->righttree;
	}

	return PlanStateOperatorMemKB(psOp);
}

/*
 * Physically delete a spill set. Path must not include database prefix.
 */
static void
workfile_mgr_delete_set_directory(char *workset_path)
{
	/* Add filespace prefix to path */
	char *reldirpath = (char*)palloc(PATH_MAX);
	if (snprintf(reldirpath, PATH_MAX, "%s/%s", getCurrentTempFilePath, workset_path) > PATH_MAX)
	{
		ereport(ERROR, (errmsg("cannot generate path %s/%s", getCurrentTempFilePath,
                        workset_path)));
	}

	Assert(reldirpath != NULL);

	workfile_mgr_unlink_directory(reldirpath);
	pfree(reldirpath);
}

/*
 * Physically delete a spill file set. Path is assumed to be database relative.
 */
static void
workfile_mgr_unlink_directory(const char *dirpath)
{

	elog(gp_workfile_caching_loglevel, "deleting spill file set directory %s", dirpath);

	int res = rmtree(dirpath,true);

	if (!res)
	{
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				errmsg("could not remove spill file directory")));
	}

}

/*
 * Workfile-manager specific function to clean up before releasing a
 * workfile set from the cache.
 *
 */
static void
workfile_mgr_cleanup_set(const void *resource)
{
	workfile_set *work_set = (workfile_set *) resource;

	ereport(gp_workfile_caching_loglevel,
			(errmsg("workfile mgr cleanup deleting set: key=0x%0xd, size=" INT64_FORMAT
					" in_progress_size=" INT64_FORMAT " path=%s",
					work_set->key,
					work_set->size,
					work_set->in_progress_size,
					work_set->path),
					errprintstack(true)));

	workfile_mgr_delete_set_directory(work_set->path);

	/*
	 * The most accurate size of a workset is recorded in work_set->in_progress_size.
	 * work_set->size is only updated when we close a file, so it lags behind
	 */

	Assert(work_set->in_progress_size >= work_set->size);
	int64 size_to_delete = work_set->in_progress_size;

	elog(gp_workfile_caching_loglevel, "Subtracting " INT64_FORMAT " from workfile diskspace", size_to_delete);

	/*
	 * When subtracting the size of this workset from our accounting,
	 * only update the per-query counter if we created the workset.
	 * In that case, the state is ACQUIRED, otherwise is CACHED or DELETED
	 */
	CacheEntry *cacheEntry = CACHE_ENTRY_HEADER(resource);
	bool update_query_space = (cacheEntry->state == CACHE_ENTRY_ACQUIRED);

	WorkfileDiskspace_Commit(0, size_to_delete, update_query_space);
}

/*
 * Close a spill file set. If we're planning to re-use it, insert it in the
 * cache. If not, let the cleanup routine delete the files and free up memory.
 */
void
workfile_mgr_close_set(workfile_set *work_set)
{
	Assert(work_set!=NULL);
	/* Although work_set is in shared memory only this process has access to it */
	if (work_set->prev)
		work_set->prev->next = work_set->next;
	else
		open_workfile_sets = work_set->next;
	if (work_set->next)
		work_set->next->prev = work_set->prev;

	elog(gp_workfile_caching_loglevel, "closing workfile set: location: %s, size=" INT64_FORMAT
			" in_progress_size=" INT64_FORMAT,
		 work_set->path,
		 work_set->size, work_set->in_progress_size);

	CacheEntry *cache_entry = CACHE_ENTRY_HEADER(work_set);
	Cache_Release(workfile_mgr_cache, cache_entry);
}

/*
 * This function is called at transaction commit or abort to delete closed
 * workfiles.
 */
void
workfile_mgr_cleanup(void)
{
	Assert(NULL != workfile_mgr_cache);
	Cache_SurrenderClientEntries(workfile_mgr_cache);
	WorkfileDiskspace_Commit(0, used_segspace_not_in_workfile_set, false /* update_query_space */);
	used_segspace_not_in_workfile_set = 0;
}

/*
 * Updates the in-progress size of a workset while it is being created.
 */
void
workfile_set_update_in_progress_size(workfile_set *work_set, int64 size)
{
	if (NULL != work_set)
	{
		work_set->in_progress_size += size;
		Assert(work_set->in_progress_size >= 0);
	}
	else
	{
		used_segspace_not_in_workfile_set += size;
		Assert(used_segspace_not_in_workfile_set >= 0);
	}
}

/*
 * Reports corresponding error message when the query or segment size limit is exceeded.
 */
void 
workfile_mgr_report_error(void)
{
	char* message = NULL;

	switch(workfileError)
	{
		case WORKFILE_ERROR_LIMIT_PER_QUERY:
				message = "workfile per query size limit exceeded";
				break;
		case WORKFILE_ERROR_LIMIT_PER_SEGMENT:
				message = "workfile per segment size limit exceeded";
				break;
		case WORKFILE_ERROR_LIMIT_FILES_PER_QUERY:
				message = "number of workfiles per query limit exceeded";
				break;
		default:
				message = "could not write to workfile";
				break;
	}

	ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_RESOURCES),
		errmsg("%s", message)));
}

/* EOF */
