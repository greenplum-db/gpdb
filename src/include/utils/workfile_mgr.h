/*-------------------------------------------------------------------------
 *
 * workfile_mgr.h
 *	  Interface for workfile manager and workfile caching.
 *
 *
 * Portions Copyright (c) 2011, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/workfile_mgr.h
 *
 *
 *-------------------------------------------------------------------------
 */
#ifndef __WORKFILE_MGR_H__
#define __WORKFILE_MGR_H__

#include "postgres.h"
#include "executor/execWorkfile.h"
#include "utils/sharedcache.h"
#include "nodes/execnodes.h"
#include "utils/timestamp.h"
#include "utils/resowner.h"

/*
 * Workfile management default parameters
 */

/* Other constants */

#define	WORKFILE_SET_PREFIX "workfile_set"

/* Fixed workfile numbers common for all operators */
#define WORKFILE_NUM_ALL_PLAN 0
/* Fixed workfile numbers for each operator type */
#define WORKFILE_NUM_HASHJOIN_METADATA 1
#define WORKFILE_NUM_HASHAGG_METADATA 1
#define WORKFILE_NUM_MKSORT_METADATA 1
#define WORKFILE_NUM_MKSORT_TAPESET 2
#define WORKFILE_NUM_TUPLESTORE_DATA 1
#define WORKFILE_NUM_TUPLESTORE_LOB 2

typedef struct
{

	/* number of buckets in a spilled hashtable */
	uint32 buckets;

	/* number of leaf (that did not re-spill) files for a hashagg operator */
	uint32 num_leaf_files;

	/* type of workfiles used by this operator */
	enum ExecWorkFileType type;

	/* compression level used by bfz if applicable */
	int bfz_compress_type;

	/* work_mem for this operator at the time of the spill */
	uint64 operator_work_mem;

} workfile_set_op_metadata;

typedef uint32 workfile_set_hashkey_t;

typedef struct workfile_set
{

	/* hash value of this workfile set metadata */
	workfile_set_hashkey_t key;

	/* Number of files in set */
	uint32 no_files;

	/* Size in bytes of the files in this workfile set */
	int64 size;

	/* Real-time size of the set as it is being created (for reporting only) */
	int64 in_progress_size;

	/* Prefix of files in the workfile set */
	char path[MAXPGPATH];

	/* Type of operator creating the workfile set */
	NodeTag node_type;

	/* Slice in which the spilling operator was */
	int slice_id;

	/* Session id for the query creating the workfile set */
	int session_id;

	/* Command count for the query creating the workfile set */
	int command_count;

	/* Timestamp when the workfile set was created */
	TimestampTz session_start_time;

	/* Operator-specific metadata */
	workfile_set_op_metadata metadata;

  /*
   * To make sure we don't leak workfile_set handles on abort, we keep them in
   * a linked list. We use the ResourceOwner mechanism to free them on abort.
   */
  ResourceOwner owner;
  struct workfile_set *next;
  struct workfile_set *prev;

} workfile_set;

/* The key for an entry stored in the Queryspace Hashtable */
typedef struct Queryspace_HashKey
{
	int session_id;
	int command_count;
} Queryspace_HashKey;

/*
 * (key, value) structure that is stored in the Queryspace Hashtable
 *
 * The value contains measurements of all per-query resources tracked
 */
typedef struct QueryspaceDesc
{
	Queryspace_HashKey key;
	int32 pinCount;

	/* Total disk space used for workfiles for this query */
	int64 queryDiskspace;

	/* Number of memory chunks reserved for per-query QEs in this segment */
	int chunksReserved;

	/* Number of workfiles this query has created */
	int32 workfilesCreated;
} QueryspaceDesc;

/* Workfile Set operations */
workfile_set *workfile_mgr_create_set(enum ExecWorkFileType type, bool can_be_reused,
		PlanState *ps);
void workfile_mgr_close_set(workfile_set *work_set);
void workfile_mgr_cleanup(void);
Size workfile_mgr_shmem_size(void);
void workfile_mgr_cache_init(void);
void workfile_mgr_mark_complete(workfile_set *work_set);
Cache *workfile_mgr_get_cache(void);
int32 workfile_mgr_clear_cache(int seg_id);
void workfile_set_update_in_progress_size(workfile_set *work_set, int64 size);

/* Workfile File operations */
ExecWorkFile *workfile_mgr_create_file(workfile_set *work_set);
ExecWorkFile *workfile_mgr_create_fileno(workfile_set *work_set, uint32 file_no);
int64 workfile_mgr_close_file(workfile_set *work_set, ExecWorkFile *file);

/* Workfile diskspace operations */
void WorkfileDiskspace_Init(void);
Size WorkfileDiskspace_ShMemSize(void);
bool WorkfileDiskspace_Reserve(int64 bytes);
void WorkfileDiskspace_Commit(int64 commit_bytes, int64 reserved_bytes, bool update_query_space);
void WorkfileDiskspace_SetFull(bool isFull);
bool WorkfileDiskspace_IsFull(void);

/* Workfile segspace operations */
void WorkfileSegspace_Init(void);
Size WorkfileSegspace_ShMemSize(void);
bool WorkfileSegspace_Reserve(int64 bytes);
void WorkfileSegspace_Commit(int64 commit_bytes, int64 reserved_bytes);
int64 WorkfileSegspace_GetSize(void);


/* Workfile queryspace operations */
void WorkfileQueryspace_Init(void);
Size WorkfileQueryspace_ShMemSize(void);
int64 WorkfileQueryspace_GetSize(int session_id, int command_count);
bool WorkfileQueryspace_Reserve(int64 bytes_to_reserve);
void WorkfileQueryspace_Commit(int64 commit_bytes, int64 reserved_bytes);
QueryspaceDesc *WorkfileQueryspace_InitEntry(int session_id, int command_count);
void WorkfileQueryspace_ReleaseEntry(void);
bool WorkfileQueryspace_AddWorkfile(void);
void WorkfileQueryspace_SubtractWorkfile(int32 nFiles);

/* Workfile error reporting */
typedef enum WorkfileError
{
	WORKFILE_ERROR_LIMIT_PER_QUERY,
	WORKFILE_ERROR_LIMIT_PER_SEGMENT,
	WORKFILE_ERROR_LIMIT_FILES_PER_QUERY,
	WORKFILE_ERROR_UNKNOWN,
} WorkfileError;

/* Holds latest workfile error type */
extern WorkfileError workfileError;
void workfile_mgr_report_error(void);

#endif /* __WORKFILE_MGR_H__ */
