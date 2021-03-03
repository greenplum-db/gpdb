/*-------------------------------------------------------------------------
 *
 * workfile_mgr.h
 *	  Interface for workfile manager
 *
 *
 * Portions Copyright (c) 2011, EMC Corp.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/workfile_mgr.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef __WORKFILE_MGR_H__
#define __WORKFILE_MGR_H__

#include "fmgr.h"
#include "storage/fd.h"
#include "lib/ilist.h"

#define WORKFILE_PREFIX_LEN		64

/*
 * Shared memory data structures.
 *
 * One workfile_set per workfile. Each workfile_set entry is owned by a backend.
 *
 * Per-query summary. These could be computed by scanning the workfile_set array,
 * but we keep a summary in a separate hash table so that we can quickly detect
 * if the per-query limit is exceeded. This is needed to enforce
 * gp_workfile_limit_files_per_query
 *
 * Local:
 *
 * In addition to the bookkeeping in shared memory, we keep an array in backend
 * private memory. The array is indexed by the virtual file descriptor, File.
 */

typedef struct WorkFileUsagePerQuery
{
	/* hash key */
	int32		session_id;
	int32		command_count;

	int32		refcount;		/* number of working sets */

	int32		num_files;
	uint64		total_bytes;

	bool		active;

} WorkFileUsagePerQuery;

typedef struct workfile_set
{
	/* Session id for the query creating the workfile set */
	int			session_id;

	/* Command count for the query creating the workfile set */
	int			command_count;

	/* Number of files in set */
	uint32		num_files;

	/* Size in bytes of the files in this workfile set */
	int64		total_bytes;

	/* Prefix of files in the workfile set */
	char		prefix[WORKFILE_PREFIX_LEN];

	/* Type of operator creating the workfile set */
	char		operator[NAMEDATALEN];

	/* Slice in which the spilling operator was */
	int			slice_id;

	WorkFileUsagePerQuery *perquery;

	dlist_node	node;

	bool		active;

	/* If the workfile is pinned, don't free it unless call workfile_mgr_close_set */
	bool		pinned;

	/* Used to track workfile_set created in current process */
	dlist_node	local_node;
} workfile_set;

/* Workfile Set operations */

extern Size WorkFileShmemSize(void);
extern void WorkFileShmemInit(void);
extern void AtEOXact_WorkFile(void);

extern void RegisterFileWithSet(File file, struct workfile_set *work_set);
extern void UpdateWorkFileSize(File file, uint64 newsize);
extern void WorkFileDeleted(File file, bool hold_lock);

extern workfile_set *workfile_mgr_create_set(const char *operator_name, const char *prefix, bool hold_pin);
extern void workfile_mgr_close_set(workfile_set *work_set);

extern Datum gp_workfile_mgr_cache_entries_internal(PG_FUNCTION_ARGS);
extern workfile_set *workfile_mgr_cache_entries_get_copy(int* num_actives);
extern uint64 WorkfileSegspace_GetSize(void);

#endif /* __WORKFILE_MGR_H__ */
