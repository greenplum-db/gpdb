/*--------------------------------------------------------------------------
 *
 * aocsam_handler.c
 *	  Append only columnar access methods handler
 *
 * Portions Copyright (c) 2009-2010, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/aocs/aocsam_handler.c
 *
 *--------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/aomd.h"
#include "access/appendonlywriter.h"
#include "access/heapam.h"
#include "access/multixact.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/gp_fastsequence.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/pg_appendonly.h"
#include "catalog/storage.h"
#include "catalog/storage_xlog.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbvars.h"
#include "commands/vacuum.h"
#include "executor/executor.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "pgstat.h"
#include "storage/lmgr.h"
#include "storage/procarray.h"
#include "storage/smgr.h"
#include "utils/builtins.h"
#include "utils/faultinjector.h"
#include "utils/lsyscache.h"
#include "utils/pg_rusage.h"

#define IS_BTREE(r) ((r)->rd_rel->relam == BTREE_AM_OID)

/*
 * Used for bitmapHeapScan. Also look at the comment in cdbaocsam.h regarding
 * AOCSScanDescIdentifier.
 *
 * In BitmapHeapScans, it is needed to keep track of two distict fetch
 * descriptors. One for direct fetches, and another one for recheck fetches. The
 * distinction allows for a different set of columns to be populated in each
 * case. During initialiazation of this structure, it is required to populate
 * the proj array accordingly. It is later, during the actual fetching of the
 * tuple, that the corresponding fetch descriptor will be lazily initialized.
 *
 * Finally, in this struct, state between next_block and next_tuple calls is
 * kept, in order to minimize the work that is done in the latter.
 */
typedef struct AOCSBitmapScanData
{
	TableScanDescData rs_base;	/* AM independent part of the descriptor */

	enum AOCSScanDescIdentifier descIdentifier;

	Snapshot	appendOnlyMetaDataSnapshot;

	enum
	{
		NO_RECHECK,
		RECHECK
	} whichDesc;

	struct {
		struct AOCSFetchDescData   *bitmapFetch;
		bool					   *proj;
	} bitmapScanDesc[2];

	int	rs_cindex;	/* current tuple's index tbmres->offset or -1 */
} *AOCSBitmapScan;

typedef struct AOCODMLState
{
	Oid relationOid;
	AOCSInsertDesc insertDesc;
	AOCSDeleteDesc deleteDesc;
	AOCSUniqueCheckDesc uniqueCheckDesc;
} AOCODMLState;

static void reset_state_cb(void *arg);
/*
 * GPDB_12_MERGE_FIXME: This is a temporary state of things. A locally stored
 * state is needed currently because there is no viable place to store this
 * information outside of the table access method. Ideally the caller should be
 * responsible for initializing a state and passing it over using the table
 * access method api.
 *
 * Until this is in place, the local state is not to be accessed directly but
 * only via the *_dml_state functions.
 * It contains:
 *		a quick look up member for the common case
 *		a hash table which keeps per relation information
 *		a memory context that should be long lived enough and is
 *			responsible for reseting the state via its reset cb
 */
typedef struct AOCODMLStates
{
	AOCODMLState           *last_used_state;
	HTAB				   *state_table;

	MemoryContext			stateCxt;
	MemoryContextCallback	cb;
} AOCODMLStates;

static AOCODMLStates aocoDMLStates;

/*
 * There are two cases that we are called from, during context destruction
 * after a successful completion and after a transaction abort. Only in the
 * second case we should not have cleaned up the DML state and the entries in
 * the hash table. We need to reset our global state. The actual clean up is
 * taken care elsewhere.
 */
static void
reset_state_cb(void *arg)
{
	aocoDMLStates.state_table = NULL;
	aocoDMLStates.last_used_state = NULL;
	aocoDMLStates.stateCxt = NULL;
}


/*
 * Initialize the backend local AOCODMLStates object for this backend for the
 * current DML or DML-like command (if not already initialized).
 *
 * This function should be called with a current memory context whose life
 * span is enough to last until the end of this command execution.
 */
static void
init_aoco_dml_states()
{
	HASHCTL hash_ctl;

	if (!aocoDMLStates.state_table)
	{
		Assert(aocoDMLStates.stateCxt == NULL);
		aocoDMLStates.stateCxt = AllocSetContextCreate(
			CurrentMemoryContext,
			"AppendOnly DML State Context",
			ALLOCSET_SMALL_SIZES);

		aocoDMLStates.cb.func = reset_state_cb;
		aocoDMLStates.cb.arg = NULL;
		MemoryContextRegisterResetCallback(aocoDMLStates.stateCxt,
										   &aocoDMLStates.cb);

		memset(&hash_ctl, 0, sizeof(hash_ctl));
		hash_ctl.keysize = sizeof(Oid);
		hash_ctl.entrysize = sizeof(AOCODMLState);
		hash_ctl.hcxt = aocoDMLStates.stateCxt;
		aocoDMLStates.state_table =
			hash_create("AppendOnly DML state", 128, &hash_ctl,
			            HASH_CONTEXT | HASH_ELEM | HASH_BLOBS);
	}
}

/*
 * Create and insert a state entry for a relation. The actual descriptors will
 * be created lazily when/if needed.
 *
 * Should be called exactly once per relation.
 */
static inline void
init_dml_state(const Oid relationOid)
{
	AOCODMLState *state;
	bool				found;

	Assert(aocoDMLStates.state_table);

	state = (AOCODMLState *) hash_search(aocoDMLStates.state_table,
										 &relationOid,
										 HASH_ENTER,
										 &found);

	Assert(!found);

	state->insertDesc = NULL;
	state->deleteDesc = NULL;
	state->uniqueCheckDesc = NULL;

	aocoDMLStates.last_used_state = state;
}

/*
 * Retrieve the state information for a relation.
 * It is required that the state has been created before hand.
 */
static inline AOCODMLState *
find_dml_state(const Oid relationOid)
{
	AOCODMLState *state;
	Assert(aocoDMLStates.state_table);

	if (aocoDMLStates.last_used_state &&
		aocoDMLStates.last_used_state->relationOid == relationOid)
		return aocoDMLStates.last_used_state;

	state = (AOCODMLState *) hash_search(aocoDMLStates.state_table,
										 &relationOid,
										 HASH_FIND,
										 NULL);

	Assert(state);

	aocoDMLStates.last_used_state = state;
	return state;
}

/*
 * Remove the state information for a relation.
 * It is required that the state has been created before hand.
 *
 * Should be called exactly once per relation.
 */
static inline AOCODMLState *
remove_dml_state(const Oid relationOid)
{
	AOCODMLState *state;
	Assert(aocoDMLStates.state_table);

	state = (AOCODMLState *) hash_search(aocoDMLStates.state_table,
										 &relationOid,
										 HASH_REMOVE,
										 NULL);

	Assert(state);

	if (aocoDMLStates.last_used_state &&
		aocoDMLStates.last_used_state->relationOid == relationOid)
		aocoDMLStates.last_used_state = NULL;

	return state;
}

/*
 * Provides an opportunity to create backend-local state to be consulted during
 * the course of the current DML or DML-like command, for the given relation.
 */
void
aoco_dml_init(Relation relation)
{
	init_aoco_dml_states();
	init_dml_state(RelationGetRelid(relation));
}

/*
 * Provides an opportunity to clean up backend-local state set up for the
 * current DML or DML-like command, for the given relation.
 */
void
aoco_dml_finish(Relation relation)
{
	AOCODMLState *state;

	state = remove_dml_state(RelationGetRelid(relation));

	if (state->deleteDesc)
	{
		aocs_delete_finish(state->deleteDesc);
		state->deleteDesc = NULL;

		/*
		 * Bump up the modcount. If we inserted something (meaning that
		 * this was an UPDATE), we can skip this, as the insertion bumped
		 * up the modcount already.
		 */
		if (!state->insertDesc)
			AORelIncrementModCount(relation);
	}

	if (state->insertDesc)
	{
		Assert(state->insertDesc->aoi_rel == relation);
		aocs_insert_finish(state->insertDesc);
		state->insertDesc = NULL;
	}

	if (state->uniqueCheckDesc)
	{
		AppendOnlyBlockDirectory_End_forSearch(state->uniqueCheckDesc->blockDirectory);
		pfree(state->uniqueCheckDesc->blockDirectory);
		state->uniqueCheckDesc->blockDirectory = NULL;
		pfree(state->uniqueCheckDesc);
		state->uniqueCheckDesc = NULL;
	}

}

/*
 * Retrieve the insertDescriptor for a relation. Initialize it if its absent.
 * 'num_rows': Number of rows to be inserted. (NUM_FAST_SEQUENCES if we don't
 * know it beforehand). This arg is not used if the descriptor already exists.
 */
static AOCSInsertDesc
get_or_create_aoco_insert_descriptor(const Relation relation, int64 num_rows)
{
	AOCODMLState *state;

	state = find_dml_state(RelationGetRelid(relation));

	if (state->insertDesc == NULL)
	{
		MemoryContext oldcxt;
		AOCSInsertDesc insertDesc;

		oldcxt = MemoryContextSwitchTo(aocoDMLStates.stateCxt);
		insertDesc = aocs_insert_init(relation,
									  ChooseSegnoForWrite(relation),
									  num_rows);
		/*
		 * If we have a unique index, insert a placeholder block directory row to
		 * entertain uniqueness checks from concurrent inserts. See
		 * AppendOnlyBlockDirectory_InsertPlaceholder() for details.
		 *
		 * Note: For AOCO tables, we need to only insert a placeholder block
		 * directory row for the 1st non-dropped column. This is because
		 * during a uniqueness check, only the first non-dropped column's block
		 * directory entry is consulted. (See AppendOnlyBlockDirectory_CoversTuple())
		 */
		if (relationHasUniqueIndex(relation))
		{
			int 				firstNonDroppedColumn = -1;
			int64 				firstRowNum;
			DatumStreamWrite 	*dsw;
			BufferedAppend 		*bufferedAppend;
			int64 				fileOffset;

			for(int i = 0; i < relation->rd_att->natts; i++)
			{
				if (!relation->rd_att->attrs[i].attisdropped) {
					firstNonDroppedColumn = i;
					break;
				}
			}
			Assert(firstNonDroppedColumn != -1);

			dsw = insertDesc->ds[firstNonDroppedColumn];
			firstRowNum = dsw->blockFirstRowNum;
			bufferedAppend = &dsw->ao_write.bufferedAppend;
			fileOffset = BufferedAppendNextBufferPosition(bufferedAppend);

			AppendOnlyBlockDirectory_InsertPlaceholder(&insertDesc->blockDirectory,
													   firstRowNum,
													   fileOffset,
													   firstNonDroppedColumn);
		}
		state->insertDesc = insertDesc;
		MemoryContextSwitchTo(oldcxt);
	}

	return state->insertDesc;
}


/*
 * Retrieve the deleteDescriptor for a relation. Initialize it if needed.
 */
static AOCSDeleteDesc
get_or_create_delete_descriptor(const Relation relation, bool forUpdate)
{
	AOCODMLState *state;

	state = find_dml_state(RelationGetRelid(relation));

	if (state->deleteDesc == NULL)
	{
		MemoryContext oldcxt;

		oldcxt = MemoryContextSwitchTo(aocoDMLStates.stateCxt);
		state->deleteDesc = aocs_delete_init(relation);
		MemoryContextSwitchTo(oldcxt);
	}

	return state->deleteDesc;
}

static AOCSUniqueCheckDesc
get_or_create_unique_check_desc(Relation relation, Snapshot snapshot)
{
	AOCODMLState *state = find_dml_state(RelationGetRelid(relation));

	if (!state->uniqueCheckDesc)
	{
		MemoryContext oldcxt;
		AOCSUniqueCheckDesc uniqueCheckDesc;

		oldcxt = MemoryContextSwitchTo(aocoDMLStates.stateCxt);
		uniqueCheckDesc = palloc0(sizeof(AOCSUniqueCheckDescData));
		uniqueCheckDesc->blockDirectory = palloc0(sizeof(AppendOnlyBlockDirectory));
		AppendOnlyBlockDirectory_Init_forSearch(uniqueCheckDesc->blockDirectory,
												snapshot, NULL, -1, relation,
												relation->rd_att->natts, false, NULL);
		state->uniqueCheckDesc = uniqueCheckDesc;
		MemoryContextSwitchTo(oldcxt);
	}

	return state->uniqueCheckDesc;
}

/*
 * AO_COLUMN access method uses virtual tuples
 */
static const TupleTableSlotOps *
aoco_slot_callbacks(Relation relation)
{
	return &TTSOpsVirtual;
}

struct ExtractcolumnContext
{
	bool	   *cols;
	AttrNumber	natts;
	bool		found;
};

static bool
extractcolumns_walker(Node *node, struct ExtractcolumnContext *ecCtx)
{
	if (node == NULL)
		return false;

	if (IsA(node, Var))
	{
		Var *var = (Var *)node;

		if (IS_SPECIAL_VARNO(var->varno))
			return false;

		if (var->varattno > 0 && var->varattno <= ecCtx->natts)
		{
			ecCtx->cols[var->varattno -1] = true;
			ecCtx->found = true;
		}
		/*
		 * If all attributes are included,
		 * set all entries in mask to true.
		 */
		else if (var->varattno == 0)
		{
			for (AttrNumber attno = 0; attno < ecCtx->natts; attno++)
				ecCtx->cols[attno] = true;
			ecCtx->found = true;

			return true;
		}

		return false;
	}

	return expression_tree_walker(node, extractcolumns_walker, (void *)ecCtx);
}

static bool
extractcolumns_from_node(Node *expr, bool *cols, AttrNumber natts)
{
	struct ExtractcolumnContext	ecCtx;

	ecCtx.cols	= cols;
	ecCtx.natts = natts;
	ecCtx.found = false;

	extractcolumns_walker(expr, &ecCtx);

	return  ecCtx.found;
}

static TableScanDesc
aoco_beginscan_extractcolumns(Relation rel, Snapshot snapshot,
							  List *targetlist, List *qual,
							  uint32 flags)
{
	AOCSScanDesc	aoscan;
	AttrNumber		natts = RelationGetNumberOfAttributes(rel);
	bool		   *cols;
	bool			found = false;

	cols = palloc0(natts * sizeof(*cols));

	found |= extractcolumns_from_node((Node *)targetlist, cols, natts);
	found |= extractcolumns_from_node((Node *)qual, cols, natts);

	/*
	 * In some cases (for example, count(*)), targetlist and qual may be null,
	 * extractcolumns_walker will return immediately, so no columns are specified.
	 * We always scan the first column.
	 */
	if (!found)
		cols[0] = true;

	aoscan = aocs_beginscan(rel,
							snapshot,
							cols,
							flags);

	pfree(cols);

	return (TableScanDesc)aoscan;
}

static TableScanDesc
aoco_beginscan_extractcolumns_bm(Relation rel, Snapshot snapshot,
								 List *targetlist, List *qual,
								 List *bitmapqualorig,
								 uint32 flags)
{
	AOCSBitmapScan 	aocsBitmapScan;
	AttrNumber		natts = RelationGetNumberOfAttributes(rel);
	bool		   *proj;
	bool		   *projRecheck;
	bool			found;

	aocsBitmapScan = palloc0(sizeof(*aocsBitmapScan));
	aocsBitmapScan->descIdentifier = AOCSBITMAPSCANDATA;

	aocsBitmapScan->rs_base.rs_rd = rel;
	aocsBitmapScan->rs_base.rs_snapshot = snapshot;
	aocsBitmapScan->rs_base.rs_flags = flags;

	proj = palloc0(natts * sizeof(*proj));
	projRecheck = palloc0(natts * sizeof(*projRecheck));

	if (snapshot == SnapshotAny)
		aocsBitmapScan->appendOnlyMetaDataSnapshot = GetTransactionSnapshot();
	else
		aocsBitmapScan->appendOnlyMetaDataSnapshot = snapshot;

	found = extractcolumns_from_node((Node *)targetlist, proj, natts);
	found |= extractcolumns_from_node((Node *)qual, proj, natts);

	memcpy(projRecheck, proj, natts * sizeof(*projRecheck));
	if (extractcolumns_from_node((Node *)bitmapqualorig, projRecheck, natts))
	{
		/*
		 * At least one column needs to be projected in non-recheck case.
		 * Otherwise, the AO_COLUMN fetch code may skip visimap checking because
		 * there are no columns to be scanned and we may get wrong results.
		 */
		if (!found)
			proj[0] = true;
	}
	else if (!found)
	{
		/* XXX can we have no columns to project at all? */		
		proj[0] = projRecheck[0] = true;
	}

	aocsBitmapScan->bitmapScanDesc[NO_RECHECK].proj = proj;
	aocsBitmapScan->bitmapScanDesc[RECHECK].proj = projRecheck;

	return (TableScanDesc)aocsBitmapScan;
}

/*
 * This function intentionally ignores key and nkeys
 */
static TableScanDesc
aoco_beginscan(Relation relation,
               Snapshot snapshot,
               int nkeys, struct ScanKeyData *key,
               ParallelTableScanDesc pscan,
               uint32 flags)
{
	AOCSScanDesc	aoscan;

	/* Parallel scan not supported for AO_COLUMN tables */
	Assert(pscan == NULL);

	aoscan = aocs_beginscan(relation,
							snapshot,
							NULL,
							flags);

	return (TableScanDesc) aoscan;
}

static void
aoco_endscan(TableScanDesc scan)
{
	AOCSScanDesc	aocsScanDesc;
	AOCSBitmapScan  aocsBitmapScan;

	aocsScanDesc = (AOCSScanDesc) scan;
	if (aocsScanDesc->descIdentifier == AOCSSCANDESCDATA)
	{
		aocs_endscan(aocsScanDesc);
		return;
	}

	Assert(aocsScanDesc->descIdentifier ==  AOCSBITMAPSCANDATA);
	aocsBitmapScan = (AOCSBitmapScan) scan;

	if (aocsBitmapScan->bitmapScanDesc[NO_RECHECK].bitmapFetch)
		aocs_fetch_finish(aocsBitmapScan->bitmapScanDesc[NO_RECHECK].bitmapFetch);
	if (aocsBitmapScan->bitmapScanDesc[RECHECK].bitmapFetch)
		aocs_fetch_finish(aocsBitmapScan->bitmapScanDesc[RECHECK].bitmapFetch);

	pfree(aocsBitmapScan->bitmapScanDesc[NO_RECHECK].proj);
	pfree(aocsBitmapScan->bitmapScanDesc[RECHECK].proj);
}

static void
aoco_rescan(TableScanDesc scan, ScanKey key,
                  bool set_params, bool allow_strat,
                  bool allow_sync, bool allow_pagemode)
{
	AOCSScanDesc  aoscan = (AOCSScanDesc) scan;

	if (aoscan->descIdentifier == AOCSSCANDESCDATA)
		aocs_rescan(aoscan);
}

static bool
aoco_getnextslot(TableScanDesc scan, ScanDirection direction, TupleTableSlot *slot)
{
	AOCSScanDesc  aoscan = (AOCSScanDesc)scan;

	ExecClearTuple(slot);
	if (aocs_getnext(aoscan, direction, slot))
	{
		ExecStoreVirtualTuple(slot);
		pgstat_count_heap_getnext(aoscan->rs_base.rs_rd);

		return true;
	}

	return false;
}

static Size
aoco_parallelscan_estimate(Relation rel)
{
	elog(ERROR, "parallel SeqScan not implemented for AO_COLUMN tables");
}

static Size
aoco_parallelscan_initialize(Relation rel, ParallelTableScanDesc pscan)
{
	elog(ERROR, "parallel SeqScan not implemented for AO_COLUMN tables");
}

static void
aoco_parallelscan_reinitialize(Relation rel, ParallelTableScanDesc pscan)
{
	elog(ERROR, "parallel SeqScan not implemented for AO_COLUMN tables");
}

static IndexFetchTableData *
aoco_index_fetch_begin(Relation rel)
{
	IndexFetchAOCOData *aocoscan = palloc0(sizeof(IndexFetchAOCOData));

	aocoscan->xs_base.rel = rel;

	/* aocoscan other variables are initialized lazily on first fetch */

	return &aocoscan->xs_base;
}

static void
aoco_index_fetch_reset(IndexFetchTableData *scan)
{
	/*
	 * Unlike Heap, we don't release the resources (fetch descriptor and its
	 * members) here because it is more like a global data structure shared
	 * across scans, rather than an iterator to yield a granularity of data.
	 * 
	 * Additionally, should be aware of that no matter whether allocation or
	 * release on fetch descriptor, it is considerably expensive.
	 */
	return;
}

static void
aoco_index_fetch_end(IndexFetchTableData *scan)
{
	IndexFetchAOCOData *aocoscan = (IndexFetchAOCOData *) scan;

	if (aocoscan->aocofetch)
	{
		aocs_fetch_finish(aocoscan->aocofetch);
		pfree(aocoscan->aocofetch);
		aocoscan->aocofetch = NULL;
	}

	if (aocoscan->proj)
	{
		pfree(aocoscan->proj);
		aocoscan->proj = NULL;
	}

	pfree(aocoscan);
}

static bool
aoco_index_fetch_tuple(struct IndexFetchTableData *scan,
                             ItemPointer tid,
                             Snapshot snapshot,
                             TupleTableSlot *slot,
                             bool *call_again, bool *all_dead)
{
	IndexFetchAOCOData *aocoscan = (IndexFetchAOCOData *) scan;
	bool found = false;

	if (!aocoscan->aocofetch)
	{
		Snapshot	appendOnlyMetaDataSnapshot;
		int			natts;

		/* Initiallize the projection info, assumes the whole row */
		Assert(!aocoscan->proj);
		natts = RelationGetNumberOfAttributes(scan->rel);
		aocoscan->proj = palloc(natts * sizeof(*aocoscan->proj));
		MemSet(aocoscan->proj, true, natts * sizeof(*aocoscan->proj));

		appendOnlyMetaDataSnapshot = snapshot;
		if (appendOnlyMetaDataSnapshot == SnapshotAny)
		{
			/*
			 * the append-only meta data should never be fetched with
			 * SnapshotAny as bogus results are returned.
			 */
			appendOnlyMetaDataSnapshot = GetTransactionSnapshot();
		}

		aocoscan->aocofetch = aocs_fetch_init(aocoscan->xs_base.rel,
											  snapshot,
											  appendOnlyMetaDataSnapshot,
											  aocoscan->proj);
	}
	/*
	 * There is no reason to expect changes on snapshot between tuple
	 * fetching calls after fech_init is called, treat it as a
	 * programming error in case of occurrence.
	 */
	Assert(aocoscan->aocofetch->snapshot == snapshot);

	ExecClearTuple(slot);

	if (aocs_fetch(aocoscan->aocofetch, (AOTupleId *) tid, slot))
	{
		ExecStoreVirtualTuple(slot);
		found = true;
	}

	/*
	 * Currently, we don't determine this parameter. By contract, it is to be
	 * set to true iff we can determine that this row is dead to all
	 * transactions. Failure to set this will lead to use of a garbage value
	 * in certain code, such as that for unique index checks.
	 * This is typically used for HOT chains, which we don't support.
	 */
	if (all_dead)
		*all_dead = false;

	/* Currently, we don't determine this parameter. By contract, it is to be
	 * set to true iff there is another tuple for the tid, so that we can prompt
	 * the caller to call index_fetch_tuple() again for the same tid.
	 * This is typically used for HOT chains, which we don't support.
	 */
	if (call_again)
		*call_again = false;

	return found;
}

/*
 * Check if a visible tuple exists given the tid and a snapshot. This is
 * currently used to determine uniqueness checks.
 *
 * We determine existence simply by checking if a *visible* block directory
 * entry covers the given tid.
 *
 * There is no need to fetch the tuple (we actually can't reliably do so as
 * we might encounter a placeholder row in the block directory)
 */
static bool
aoco_index_fetch_tuple_exists(Relation rel,
							  ItemPointer tid,
							  Snapshot snapshot,
							  bool *all_dead)
{
	AOCSUniqueCheckDesc 		uniqueCheckDesc;
	AppendOnlyBlockDirectory 	*blockDirectory;
	AOTupleId 					*aoTupleId = (AOTupleId *) tid;

#ifdef USE_ASSERT_CHECKING
	int			segmentFileNum = AOTupleIdGet_segmentFileNum(aoTupleId);
	int64		rowNum = AOTupleIdGet_rowNum(aoTupleId);

	Assert(segmentFileNum != InvalidFileSegNumber);
	Assert(rowNum != InvalidAORowNum);
	/*
	 * Since this can only be called in the context of a unique index check, the
	 * snapshots that are supplied can only be non-MVCC snapshots: SELF and DIRTY.
	 */
	Assert(snapshot->snapshot_type == SNAPSHOT_SELF ||
		   snapshot->snapshot_type == SNAPSHOT_DIRTY);
#endif

	/*
	 * Currently, we don't determine this parameter. By contract, it is to be
	 * set to true iff we can determine that this row is dead to all
	 * transactions. Failure to set this will lead to use of a garbage value
	 * in certain code, such as that for unique index checks.
	 * This is typically used for HOT chains, which we don't support.
	 */
	if (all_dead)
		*all_dead = false;

	/*
	 * FIXME: for when we want CREATE UNIQUE INDEX CONCURRENTLY to work
	 * Unique constraint violation checks with SNAPSHOT_SELF are currently
	 * required to support CREATE UNIQUE INDEX CONCURRENTLY. Currently, the
	 * sole placeholder row inserted at first insert might not be visible to
	 * the snapshot, if it was already updated by its actual first row. So,
	 * we would need to flush a placeholder row at the beginning of each new
	 * in-memory minipage. Currently, CREATE INDEX CONCURRENTLY isn't
	 * supported, so we assume such a check satisfies SNAPSHOT_SELF.
	 */
	if (snapshot->snapshot_type == SNAPSHOT_SELF)
		return true;

	uniqueCheckDesc = get_or_create_unique_check_desc(rel, snapshot);
	blockDirectory = uniqueCheckDesc->blockDirectory;
	return AppendOnlyBlockDirectory_CoversTuple(blockDirectory, aoTupleId);
}

static void
aoco_tuple_insert(Relation relation, TupleTableSlot *slot, CommandId cid,
                        int options, BulkInsertState bistate)
{

	AOCSInsertDesc          insertDesc;

	/*
	 * Note: since we don't know how many rows will actually be inserted (as we
	 * don't know how many rows are visible), we provide the default number of
	 * rows to bump gp_fastsequence by.
	 */
	insertDesc = get_or_create_aoco_insert_descriptor(relation, NUM_FAST_SEQUENCES);

	aocs_insert(insertDesc, slot);

	pgstat_count_heap_insert(relation, 1);
}

static void
aoco_tuple_insert_speculative(Relation relation, TupleTableSlot *slot,
                                    CommandId cid, int options,
                                    BulkInsertState bistate, uint32 specToken)
{
	/* GPDB_12_MERGE_FIXME: not supported. Can this function be left out completely? Or ereport()? */
	elog(ERROR, "speculative insertion not supported on AO_COLUMN tables");
}

static void
aoco_tuple_complete_speculative(Relation relation, TupleTableSlot *slot,
                                      uint32 specToken, bool succeeded)
{
	elog(ERROR, "speculative insertion not supported on AO_COLUMN tables");
}

/*
 *	aoco_multi_insert	- insert multiple tuples into an ao relation
 *
 * This is like aoco_tuple_insert(), but inserts multiple tuples in one
 * operation. Typically used by COPY.
 *
 * In the ao_column AM, we already realize the benefits of batched WAL (WAL is
 * generated only when the insert buffer is full). There is also no page locking
 * that we can optimize, as ao_column relations don't use the PG buffer cache.
 * So, this is a thin layer over aoco_tuple_insert() with one important
 * optimization: We allocate the insert desc with ntuples up front, which can
 * reduce the number of gp_fast_sequence allocations.
 */
static void
aoco_multi_insert(Relation relation, TupleTableSlot **slots, int ntuples,
                        CommandId cid, int options, BulkInsertState bistate)
{
	(void) get_or_create_aoco_insert_descriptor(relation, ntuples);
	for (int i = 0; i < ntuples; i++)
		aoco_tuple_insert(relation, slots[i], cid, options, bistate);
}

static TM_Result
aoco_tuple_delete(Relation relation, ItemPointer tid, CommandId cid,
				  Snapshot snapshot, Snapshot crosscheck, bool wait,
				  TM_FailureData *tmfd, bool changingPart)
{
	AOCSDeleteDesc deleteDesc;
	TM_Result	result;

	deleteDesc = get_or_create_delete_descriptor(relation, false);
	result = aocs_delete(deleteDesc, (AOTupleId *) tid);
	if (result == TM_Ok)
		pgstat_count_heap_delete(relation);
	else if (result == TM_SelfModified)
	{
		/*
		 * The visibility map entry has been set and it was in this command.
		 *
		 * Our caller might want to investigate tmfd to decide on appropriate
		 * action. Set it here to match expectations. The uglyness here is
		 * preferrable to having to inspect the relation's am in the caller.
		 */
		tmfd->cmax = cid;
	}

	return result;
}


static TM_Result
aoco_tuple_update(Relation relation, ItemPointer otid, TupleTableSlot *slot,
				  CommandId cid, Snapshot snapshot, Snapshot crosscheck,
				  bool wait, TM_FailureData *tmfd,
				  LockTupleMode *lockmode, bool *update_indexes)
{
	AOCSInsertDesc insertDesc;
	AOCSDeleteDesc deleteDesc;
	TM_Result	result;

	/*
	 * Note: since we don't know how many rows will actually be inserted (as we
	 * don't know how many rows are visible), we provide the default number of
	 * rows to bump gp_fastsequence by.
	 */
	insertDesc = get_or_create_aoco_insert_descriptor(relation, NUM_FAST_SEQUENCES);
	deleteDesc = get_or_create_delete_descriptor(relation, true);

	/* Update the tuple with table oid */
	slot->tts_tableOid = RelationGetRelid(relation);

#ifdef FAULT_INJECTOR
	FaultInjector_InjectFaultIfSet(
								   "appendonly_update",
								   DDLNotSpecified,
								   "", //databaseName
								   RelationGetRelationName(insertDesc->aoi_rel));
	/* tableName */
#endif

	result = aocs_delete(deleteDesc, (AOTupleId *) otid);
	if (result != TM_Ok)
		return result;

	aocs_insert(insertDesc, slot);

	pgstat_count_heap_update(relation, false);
	/* No HOT updates with AO tables. */
	*update_indexes = true;

	return result;
}

static TM_Result
aoco_tuple_lock(Relation relation, ItemPointer tid, Snapshot snapshot,
                      TupleTableSlot *slot, CommandId cid, LockTupleMode mode,
                      LockWaitPolicy wait_policy, uint8 flags,
                      TM_FailureData *tmfd)
{
	/* GPDB_12_MERGE_FIXME: not supported. Can this function be left out completely? Or ereport()? */
	elog(ERROR, "speculative insertion not supported on AO tables");
}

static void
aoco_finish_bulk_insert(Relation relation, int options)
{
	aoco_dml_finish(relation);
}




/* ------------------------------------------------------------------------
 * Callbacks for non-modifying operations on individual tuples for heap AM
 * ------------------------------------------------------------------------
 */

static bool
aoco_fetch_row_version(Relation relation,
                             ItemPointer tid,
                             Snapshot snapshot,
                             TupleTableSlot *slot)
{
	/*
	 * This is a generic interface. It is currently used in three distinct
	 * cases, only one of which is currently invoking it for AO tables.
	 * This is DELETE RETURNING. In order to return the slot via the tid for
	 * AO tables one would have to scan the block directory and the visibility
	 * map. A block directory is not guarranteed to exist. Even if it exists, a
	 * state would have to be created and dropped for every tuple look up since
	 * this interface does not allow for the state to be passed around. This is
	 * a very costly operation to be performed per tuple lookup. Furthermore, if
	 * a DELETE operation is currently on the fly, the corresponding visibility
	 * map entries will not have been finalized into a visibility map tuple.
	 *
	 * Error out with feature not supported. Given that this is a generic
	 * interface, we can not really say which feature is that, although we do
	 * know that is DELETE RETURNING.
	 */
	ereport(ERROR,
	        (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		        errmsg("feature not supported on appendoptimized relations")));
}

static void
aoco_get_latest_tid(TableScanDesc sscan,
                          ItemPointer tid)
{
	/*
	 * Tid scans are not supported for appendoptimized relation. This function
	 * should not have been called in the first place, but if it is called,
	 * better to error out.
	 */
	ereport(ERROR,
	        (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		        errmsg("feature not supported on appendoptimized relations")));
}

static bool
aoco_tuple_tid_valid(TableScanDesc scan, ItemPointer tid)
{
	/*
	 * Tid scans are not supported for appendoptimized relation. This function
	 * should not have been called in the first place, but if it is called,
	 * better to error out.
	 */
	ereport(ERROR,
	        (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		        errmsg("feature not supported on appendoptimized relations")));
}

static bool
aoco_tuple_satisfies_snapshot(Relation rel, TupleTableSlot *slot,
                                    Snapshot snapshot)
{
	/*
	 * AO_COLUMN table dose not support unique and tidscan yet.
	 */
	ereport(ERROR,
	        (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		        errmsg("feature not supported on appendoptimized relations")));
}

static TransactionId
aoco_compute_xid_horizon_for_tuples(Relation rel,
                                          ItemPointerData *tids,
                                          int nitems)
{
	// GPDB_12_MERGE_FIXME: vacuum related call back.
	elog(ERROR, "not implemented yet");
}

/* ------------------------------------------------------------------------
 * DDL related callbacks for ao_column AM.
 * ------------------------------------------------------------------------
 */
static void
aoco_relation_set_new_filenode(Relation rel,
							   const RelFileNode *newrnode,
							   char persistence,
							   TransactionId *freezeXid,
							   MultiXactId *minmulti)
{
	SMgrRelation srel;

	/*
	 * Append-optimized tables do not contain transaction information in
	 * tuples.
	 */
	*freezeXid = *minmulti = InvalidTransactionId;

	/*
	 * No special treatment is needed for new AO_ROW/COLUMN relation. Create
	 * the underlying disk file storage for the relation.  No clean up is
	 * needed, RelationCreateStorage() is transactional.
	 *
	 * Segment files will be created when / if needed.
	 */
	srel = RelationCreateStorage(*newrnode, persistence, SMGR_AO);

	/*
	 * If required, set up an init fork for an unlogged table so that it can
	 * be correctly reinitialized on restart.  An immediate sync is required
	 * even if the page has been logged, because the write did not go through
	 * shared_buffers and therefore a concurrent checkpoint may have moved the
	 * redo pointer past our xlog record.  Recovery may as well remove it
	 * while replaying, for example, XLOG_DBASE_CREATE or XLOG_TBLSPC_CREATE
	 * record. Therefore, logging is necessary even if wal_level=minimal.
	 */
	if (persistence == RELPERSISTENCE_UNLOGGED)
	{
		Assert(rel->rd_rel->relkind == RELKIND_RELATION ||
			   rel->rd_rel->relkind == RELKIND_MATVIEW ||
			   rel->rd_rel->relkind == RELKIND_TOASTVALUE);
		smgrcreate(srel, INIT_FORKNUM, false);
		log_smgrcreate(newrnode, INIT_FORKNUM, SMGR_AO);
		smgrimmedsync(srel, INIT_FORKNUM);
	}

	smgrclose(srel);
}

/* helper routine to call open a rel and call heap_truncate_one_rel() on it */
static void
heap_truncate_one_relid(Oid relid)
{
	if (OidIsValid(relid))
	{
		Relation rel = relation_open(relid, AccessExclusiveLock);
		heap_truncate_one_rel(rel);
		relation_close(rel, NoLock);
	}
}

static void
aoco_relation_nontransactional_truncate(Relation rel)
{
	Oid			ao_base_relid = RelationGetRelid(rel);
	Oid			aoseg_relid = InvalidOid;
	Oid			aoblkdir_relid = InvalidOid;
	Oid			aovisimap_relid = InvalidOid;

	ao_truncate_one_rel(rel);

	/* Also truncate the aux tables */
	GetAppendOnlyEntryAuxOids(ao_base_relid, NULL,
	                          &aoseg_relid,
	                          &aoblkdir_relid, NULL,
	                          &aovisimap_relid, NULL);

	heap_truncate_one_relid(aoseg_relid);
	heap_truncate_one_relid(aoblkdir_relid);
	heap_truncate_one_relid(aovisimap_relid);
}

static void
aoco_relation_copy_data(Relation rel, const RelFileNode *newrnode)
{
	SMgrRelation dstrel;

	/*
	 * Use the "AO-specific" (non-shared buffers backed storage) SMGR
	 * implementation
	 */
	dstrel = smgropen(*newrnode, rel->rd_backend, SMGR_AO);
	RelationOpenSmgr(rel);

	/*
	 * Create and copy all forks of the relation, and schedule unlinking of
	 * old physical files.
	 *
	 * NOTE: any conflict in relfilenode value will be caught in
	 * RelationCreateStorage().
	 */
	RelationCreateStorage(*newrnode, rel->rd_rel->relpersistence, SMGR_AO);

	copy_append_only_data(rel->rd_node, *newrnode, rel->rd_backend, rel->rd_rel->relpersistence);

	/*
	 * For append-optimized tables, no forks other than the main fork should
	 * exist with the exception of unlogged tables.  For unlogged AO tables,
	 * INIT_FORK must exist.
	 */
	if (rel->rd_rel->relpersistence == RELPERSISTENCE_UNLOGGED)
	{
		Assert (smgrexists(rel->rd_smgr, INIT_FORKNUM));

		/*
		 * INIT_FORK is empty, creating it is sufficient, no need to copy
		 * contents from source to destination.
		 */
		smgrcreate(dstrel, INIT_FORKNUM, false);

		log_smgrcreate(newrnode, INIT_FORKNUM, SMGR_AO);
	}

	/* drop old relation, and close new one */
	RelationDropStorage(rel);
	smgrclose(dstrel);
}

static void
aoco_vacuum_rel(Relation onerel, VacuumParams *params,
                      BufferAccessStrategy bstrategy)
{
	/*
	 * Implemented but not invoked, we do the AO_COLUMN different phases vacuuming by
	 * calling ao_vacuum_rel() in vacuum_rel() directly for now.
	 */
	ao_vacuum_rel(onerel, params, bstrategy);

	return;
}

static void
aoco_relation_copy_for_cluster(Relation OldHeap, Relation NewHeap,
                                     Relation OldIndex, bool use_sort,
                                     TransactionId OldestXmin,
                                     TransactionId *xid_cutoff,
                                     MultiXactId *multi_cutoff,
                                     double *num_tuples,
                                     double *tups_vacuumed,
                                     double *tups_recently_dead)
{
	TupleDesc	oldTupDesc;
	TupleDesc	newTupDesc;
	int			natts;
	Datum	   *values;
	bool	   *isnull;
	TransactionId FreezeXid;
	MultiXactId MultiXactCutoff;
	Tuplesortstate *tuplesort;
	PGRUsage	ru0;

	AOTupleId				aoTupleId;
	AOCSInsertDesc			idesc = NULL;
	int						write_seg_no;
	AOCSScanDesc			scan = NULL;
	TupleTableSlot		   *slot;

	pg_rusage_init(&ru0);

	/*
	 * Curently AO storage lacks cost model for IndexScan, thus IndexScan
	 * is not functional. In future, probably, this will be fixed and CLUSTER
	 * command will support this. Though, random IO over AO on TID stream
	 * can be impractical anyway.
	 * Here we are sorting data on on the lines of heap tables, build a tuple
	 * sort state and sort the entire AO table using the index key, rewrite
	 * the table, one tuple at a time, in order as returned by tuple sort state.
	 */
	if (OldIndex == NULL || !IS_BTREE(OldIndex))
		ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					errmsg("cannot cluster append-optimized table \"%s\"", RelationGetRelationName(OldHeap)),
					errdetail("Append-optimized tables can only be clustered against a B-tree index")));

	/*
	 * Their tuple descriptors should be exactly alike, but here we only need
	 * assume that they have the same number of columns.
	 */
	oldTupDesc = RelationGetDescr(OldHeap);
	newTupDesc = RelationGetDescr(NewHeap);
	Assert(newTupDesc->natts == oldTupDesc->natts);

	/* Preallocate values/isnull arrays to deform heap tuples after sort */
	natts = newTupDesc->natts;
	values = (Datum *) palloc(natts * sizeof(Datum));
	isnull = (bool *) palloc(natts * sizeof(bool));

	/*
	 * If the OldHeap has a toast table, get lock on the toast table to keep
	 * it from being vacuumed.  This is needed because autovacuum processes
	 * toast tables independently of their main tables, with no lock on the
	 * latter.  If an autovacuum were to start on the toast table after we
	 * compute our OldestXmin below, it would use a later OldestXmin, and then
	 * possibly remove as DEAD toast tuples belonging to main tuples we think
	 * are only RECENTLY_DEAD.  Then we'd fail while trying to copy those
	 * tuples.
	 *
	 * We don't need to open the toast relation here, just lock it.  The lock
	 * will be held till end of transaction.
	 */
	if (OldHeap->rd_rel->reltoastrelid)
		LockRelationOid(OldHeap->rd_rel->reltoastrelid, AccessExclusiveLock);

	/* use_wal off requires smgr_targblock be initially invalid */
	Assert(RelationGetTargetBlock(NewHeap) == InvalidBlockNumber);

	/*
	 * Compute sane values for FreezeXid and CutoffMulti with regular
	 * VACUUM machinery to avoidconfising existing CLUSTER code.
	 */
	vacuum_set_xid_limits(OldHeap, 0, 0, 0, 0,
						  &OldestXmin, &FreezeXid, NULL, &MultiXactCutoff,
						  NULL);

	/*
	 * FreezeXid will become the table's new relfrozenxid, and that mustn't go
	 * backwards, so take the max.
	 */
	if (TransactionIdPrecedes(FreezeXid, OldHeap->rd_rel->relfrozenxid))
		FreezeXid = OldHeap->rd_rel->relfrozenxid;

	/*
	 * MultiXactCutoff, similarly, shouldn't go backwards either.
	 */
	if (MultiXactIdPrecedes(MultiXactCutoff, OldHeap->rd_rel->relminmxid))
		MultiXactCutoff = OldHeap->rd_rel->relminmxid;

	/* return selected values to caller */
	*xid_cutoff = FreezeXid;
	*multi_cutoff = MultiXactCutoff;

	tuplesort = tuplesort_begin_cluster(oldTupDesc, OldIndex,
											maintenance_work_mem, NULL, false);


	/* Log what we're doing */
	ereport(DEBUG2,
			(errmsg("clustering \"%s.%s\" using sequential scan and sort",
					get_namespace_name(RelationGetNamespace(OldHeap)),
					RelationGetRelationName(OldHeap))));

	/* Scan through old table to convert data into tuples for sorting */
	slot = table_slot_create(OldHeap, NULL);

	scan = aocs_beginscan(OldHeap, GetActiveSnapshot(),
						  NULL /* proj */,
						  0 /* flags */);

	while (aocs_getnext(scan, ForwardScanDirection, slot))
	{
		Datum	   *slot_values;
		bool	   *slot_isnull;
		HeapTuple   tuple;
		CHECK_FOR_INTERRUPTS();

		slot_getallattrs(slot);
		slot_values = slot->tts_values;
		slot_isnull = slot->tts_isnull;

		tuple = heap_form_tuple(oldTupDesc, slot_values, slot_isnull);

		*num_tuples += 1;
		tuplesort_putheaptuple(tuplesort, tuple);
		heap_freetuple(tuple);
	}

	ExecDropSingleTupleTableSlot(slot);
	aocs_endscan(scan);


	/*
	 * Сomplete the sort, then read out all tuples
	 * from the tuplestore and write them to the new relation.
	 */

	tuplesort_performsort(tuplesort);
	
	write_seg_no = ChooseSegnoForWrite(NewHeap);

	idesc = aocs_insert_init(NewHeap, write_seg_no, (int64) *num_tuples);

	/* Insert sorted heap tuples into new storage */
	for (;;)
	{
		HeapTuple	tuple;

		CHECK_FOR_INTERRUPTS();

		tuple = tuplesort_getheaptuple(tuplesort, true);
		if (tuple == NULL)
			break;

		heap_deform_tuple(tuple, oldTupDesc, values, isnull);
		aocs_insert_values(idesc, values, isnull, &aoTupleId);
	}

	tuplesort_end(tuplesort);

	/* Finish and deallocate insertion */
	aocs_insert_finish(idesc);
}

static bool
aoco_scan_analyze_next_block(TableScanDesc scan, BlockNumber blockno,
                                   BufferAccessStrategy bstrategy)
{
	AOCSScanDesc aoscan = (AOCSScanDesc) scan;
	aoscan->targetTupleId = blockno;

	return true;
}

static bool
aoco_scan_analyze_next_tuple(TableScanDesc scan, TransactionId OldestXmin,
                                   double *liverows, double *deadrows,
                                   TupleTableSlot *slot)
{
	AOCSScanDesc aoscan = (AOCSScanDesc) scan;
	bool		ret = false;

	/* skip several tuples if they are not sampling target */
	while (aoscan->targetTupleId > aoscan->nextTupleId)
	{
		aoco_getnextslot(scan, ForwardScanDirection, slot);
		aoscan->nextTupleId++;
	}

	if (aoscan->targetTupleId == aoscan->nextTupleId)
	{
		ret = aoco_getnextslot(scan, ForwardScanDirection, slot);
		aoscan->nextTupleId++;

		if (ret)
			*liverows += 1;
		else
			*deadrows += 1; /* if return an invisible tuple */
	}

	return ret;
}

static double
aoco_index_build_range_scan(Relation heapRelation,
                                  Relation indexRelation,
                                  IndexInfo *indexInfo,
                                  bool allow_sync,
                                  bool anyvisible,
                                  bool progress,
                                  BlockNumber start_blockno,
                                  BlockNumber numblocks,
                                  IndexBuildCallback callback,
                                  void *callback_state,
                                  TableScanDesc scan)
{
	AOCSScanDesc aocoscan;
	bool		is_system_catalog;
	bool		checking_uniqueness;
	Datum		values[INDEX_MAX_KEYS];
	bool		isnull[INDEX_MAX_KEYS];
	double		reltuples;
	ExprState  *predicate;
	TupleTableSlot *slot;
	EState	   *estate;
	ExprContext *econtext;
	Snapshot	snapshot;
	bool		need_unregister_snapshot = false;
	bool		need_create_blk_directory = false;
	List	   *tlist = NIL;
	List	   *qual = indexInfo->ii_Predicate;
	Oid			blkdirrelid;
	Oid			blkidxrelid;
	TransactionId OldestXmin;

	/*
	 * sanity checks
	 */
	Assert(OidIsValid(indexRelation->rd_rel->relam));

	/* Remember if it's a system catalog */
	is_system_catalog = IsSystemRelation(heapRelation);

	/* Appendoptimized catalog tables are not supported. */
	Assert(!is_system_catalog);
	/* Appendoptimized tables have no data on master. */
	if (IS_QUERY_DISPATCHER())
		return 0;

	/* See whether we're verifying uniqueness/exclusion properties */
	checking_uniqueness = (indexInfo->ii_Unique ||
		indexInfo->ii_ExclusionOps != NULL);

	/*
	 * "Any visible" mode is not compatible with uniqueness checks; make sure
	 * only one of those is requested.
	 */
	Assert(!(anyvisible && checking_uniqueness));

	/*
	 * Need an EState for evaluation of index expressions and partial-index
	 * predicates.  Also a slot to hold the current tuple.
	 */
	estate = CreateExecutorState();
	econtext = GetPerTupleExprContext(estate);
	slot = table_slot_create(heapRelation, NULL);

	/* Arrange for econtext's scan tuple to be the tuple under test */
	econtext->ecxt_scantuple = slot;

	/* Set up execution state for predicate, if any. */
	predicate = ExecPrepareQual(indexInfo->ii_Predicate, estate);

	/*
	 * Prepare for scan of the base relation.  In a normal index build, we use
	 * SnapshotAny because we must retrieve all tuples and do our own time
	 * qual checks (because we have to index RECENTLY_DEAD tuples). In a
	 * concurrent build, or during bootstrap, we take a regular MVCC snapshot
	 * and index whatever's live according to that.
	 */
	OldestXmin = InvalidTransactionId;

	/* okay to ignore lazy VACUUMs here */
	if (!IsBootstrapProcessingMode() && !indexInfo->ii_Concurrent)
		OldestXmin = GetOldestXmin(heapRelation, PROCARRAY_FLAGS_VACUUM);

	/*
	 * If block directory is empty, it must also be built along with the index.
	 */
	GetAppendOnlyEntryAuxOids(RelationGetRelid(heapRelation), NULL, NULL,
							  &blkdirrelid, &blkidxrelid, NULL, NULL);

	Relation blkdir = relation_open(blkdirrelid, AccessShareLock);

	need_create_blk_directory = RelationGetNumberOfBlocks(blkdir) == 0;
	relation_close(blkdir, NoLock);

	if (!scan)
	{
		/*
		 * Serial index build.
		 *
		 * Must begin our own heap scan in this case.  We may also need to
		 * register a snapshot whose lifetime is under our direct control.
		 */
		if (!TransactionIdIsValid(OldestXmin))
		{
			snapshot = RegisterSnapshot(GetTransactionSnapshot());
			need_unregister_snapshot = true;
		}
		else
			snapshot = SnapshotAny;

		/*
		 * Scan all columns if we need to create block directory.
		 */
		if (need_create_blk_directory)
		{
			scan = table_beginscan_strat(heapRelation,	/* relation */
										 snapshot,		/* snapshot */
										 0,		/* number of keys */
										 NULL,		/* scan key */
										 true,			/* buffer access strategy OK */
										 allow_sync);	/* syncscan OK? */
		}
		else
		{
			/*
			 * if block directory has created, we can only scan needed column.
			 */
			for (int i = 0; i < indexInfo->ii_NumIndexAttrs; i++)
			{
				AttrNumber attrnum = indexInfo->ii_IndexAttrNumbers[i];
				Form_pg_attribute attr = TupleDescAttr(RelationGetDescr(heapRelation), attrnum - 1);
				Var *var = makeVar(i,
								   attrnum,
								   attr->atttypid,
								   attr->atttypmod,
								   attr->attcollation,
								   0);

				/* Build a target list from index info */
				tlist = lappend(tlist,
								makeTargetEntry((Expr *) var,
												list_length(tlist) + 1,
												NULL,
												false));
			}

			/* Push down target list and qual to scan */
			scan = table_beginscan_es(heapRelation,	/* relation */
									  snapshot,		/* snapshot */
									  tlist,		/* targetlist */
									  qual);		/* qual */
		}
	}
	else
	{
		/*
		 * Parallel index build.
		 *
		 * Parallel case never registers/unregisters own snapshot.  Snapshot
		 * is taken from parallel heap scan, and is SnapshotAny or an MVCC
		 * snapshot, based on same criteria as serial case.
		 */
		Assert(!IsBootstrapProcessingMode());
		Assert(allow_sync);
		snapshot = scan->rs_snapshot;
	}

	aocoscan = (AOCSScanDesc) scan;

	/*
	 * Note that block directory is created during creation of the first
	 * index.  If it is found empty, it means the block directory was created
	 * by this create index transaction.  The caller (DefineIndex) must have
	 * acquired sufficiently strong lock on the appendoptimized table such
	 * that index creation as well as insert from concurrent transactions are
	 * blocked.  We can rest assured of exclusive access to the block
	 * directory relation.
	 */
	if (need_create_blk_directory)
	{
		/*
		 * Allocate blockDirectory in scan descriptor to let the access method
		 * know that it needs to also build the block directory while
		 * scanning.
		 */
		Assert(aocoscan->blockDirectory == NULL);
		aocoscan->blockDirectory = palloc0(sizeof(AppendOnlyBlockDirectory));
	}


	/* GPDB_12_MERGE_FIXME */
#if 0
	/* Publish number of blocks to scan */
	if (progress)
	{
		BlockNumber nblocks;

		if (aoscan->rs_base.rs_parallel != NULL)
		{
			ParallelBlockTableScanDesc pbscan;

			pbscan = (ParallelBlockTableScanDesc) aoscan->rs_base.rs_parallel;
			nblocks = pbscan->phs_nblocks;
		}
		else
			nblocks = aoscan->rs_nblocks;

		pgstat_progress_update_param(PROGRESS_SCAN_BLOCKS_TOTAL,
									 nblocks);
	}
#endif

	/*
	 * Must call GetOldestXmin() with SnapshotAny.  Should never call
	 * GetOldestXmin() with MVCC snapshot. (It's especially worth checking
	 * this for parallel builds, since ambuild routines that support parallel
	 * builds must work these details out for themselves.)
	 */
	Assert(snapshot == SnapshotAny || IsMVCCSnapshot(snapshot));
	Assert(snapshot == SnapshotAny ? TransactionIdIsValid(OldestXmin) :
	       !TransactionIdIsValid(OldestXmin));
	Assert(snapshot == SnapshotAny || !anyvisible);

	/* set our scan endpoints */
	if (!allow_sync)
	{
	}
	else
	{
		/* syncscan can only be requested on whole relation */
		Assert(start_blockno == 0);
		Assert(numblocks == InvalidBlockNumber);
	}

	reltuples = 0;

	/*
	 * Scan all tuples in the base relation.
	 */
	while (aoco_getnextslot(&aocoscan->rs_base, ForwardScanDirection, slot))
	{
		bool		tupleIsAlive;

		CHECK_FOR_INTERRUPTS();

		/*
		 * GPDB_12_MERGE_FIXME: How to properly do a partial scan? Currently,
		 * we scan the whole table, and throw away tuples that are not in the
		 * range. That's clearly very inefficient.
		 */
		if (ItemPointerGetBlockNumber(&slot->tts_tid) < start_blockno ||
			(numblocks != InvalidBlockNumber && ItemPointerGetBlockNumber(&slot->tts_tid) >= numblocks))
			continue;

		/* GPDB_12_MERGE_FIXME */
#if 0
		/* Report scan progress, if asked to. */
		if (progress)
		{
			BlockNumber blocks_done = appendonly_scan_get_blocks_done(aoscan);

			if (blocks_done != previous_blkno)
			{
				pgstat_progress_update_param(PROGRESS_SCAN_BLOCKS_DONE,
											 blocks_done);
				previous_blkno = blocks_done;
			}
		}
#endif

		/*
		 * appendonly_getnext did the time qual check
		 *
		 * GPDB_12_MERGE_FIXME: in heapam, we do visibility checks in SnapshotAny case
		 * here. Is that not needed with AO_COLUMN tables?
		 */
		tupleIsAlive = true;
		reltuples += 1;

		MemoryContextReset(econtext->ecxt_per_tuple_memory);

		/*
		 * In a partial index, discard tuples that don't satisfy the
		 * predicate.
		 */
		if (predicate != NULL)
		{
			if (!ExecQual(predicate, econtext))
				continue;
		}

		/*
		 * For the current heap tuple, extract all the attributes we use in
		 * this index, and note which are null.  This also performs evaluation
		 * of any expressions needed.
		 */
		FormIndexDatum(indexInfo,
		               slot,
		               estate,
		               values,
		               isnull);

		/*
		 * You'd think we should go ahead and build the index tuple here, but
		 * some index AMs want to do further processing on the data first.  So
		 * pass the values[] and isnull[] arrays, instead.
		 */

		/* Call the AM's callback routine to process the tuple */
		/*
		 * GPDB: the callback is modified to accept ItemPointer as argument
		 * instead of HeapTuple.  That allows the callback to be reused for
		 * appendoptimized tables.
		 */
		callback(indexRelation, &slot->tts_tid, values, isnull, tupleIsAlive,
		         callback_state);

	}

	/* GPDB_12_MERGE_FIXME */
#if 0
	/* Report scan progress one last time. */
	if (progress)
	{
		BlockNumber blks_done;

		if (aoscan->rs_base.rs_parallel != NULL)
		{
			ParallelBlockTableScanDesc pbscan;

			pbscan = (ParallelBlockTableScanDesc) aoscan->rs_base.rs_parallel;
			blks_done = pbscan->phs_nblocks;
		}
		else
			blks_done = aoscan->rs_nblocks;

		pgstat_progress_update_param(PROGRESS_SCAN_BLOCKS_DONE,
									 blks_done);
	}
#endif

	table_endscan(scan);

	/* we can now forget our snapshot, if set and registered by us */
	if (need_unregister_snapshot)
		UnregisterSnapshot(snapshot);

	ExecDropSingleTupleTableSlot(slot);

	FreeExecutorState(estate);

	/* These may have been pointing to the now-gone estate */
	indexInfo->ii_ExpressionsState = NIL;
	indexInfo->ii_PredicateState = NULL;

	return reltuples;
}

static void
aoco_index_validate_scan(Relation heapRelation,
                               Relation indexRelation,
                               IndexInfo *indexInfo,
                               Snapshot snapshot,
                               ValidateIndexState *state)
{
	elog(ERROR, "not implemented yet");
}

/* ------------------------------------------------------------------------
 * Miscellaneous callbacks for the heap AM
 * ------------------------------------------------------------------------
 */

/*
 * This pretends that the all the space is taken by the main fork.
 * Returns the compressed size.
 */
static uint64
aoco_relation_size(Relation rel, ForkNumber forkNumber)
{
	AOCSFileSegInfo	  **allseg;
	Snapshot			snapshot;
	uint64				totalbytes	= 0;
	int					totalseg;

	if (forkNumber != MAIN_FORKNUM)
		return totalbytes;

	snapshot = RegisterSnapshot(GetLatestSnapshot());
	allseg = GetAllAOCSFileSegInfo(rel, snapshot, &totalseg, NULL);
	for (int seg = 0; seg < totalseg; seg++)
	{
		for (int attr = 0; attr < RelationGetNumberOfAttributes(rel); attr++)
		{
			AOCSVPInfoEntry		*entry;

			/*
			 * AWAITING_DROP segments might be missing information for some
			 * (newly-added) columns.
			 */
			if (attr < allseg[seg]->vpinfo.nEntry)
			{
				entry = getAOCSVPEntry(allseg[seg], attr);
				/* Always return the compressed size */
				totalbytes += entry->eof;
			}

			CHECK_FOR_INTERRUPTS();
		}
	}

	if (allseg)
	{
		FreeAllAOCSSegFileInfo(allseg, totalseg);
		pfree(allseg);
	}
	UnregisterSnapshot(snapshot);

	return totalbytes;
}

static bool
aoco_relation_needs_toast_table(Relation rel)
{
	/*
	 * AO_COLUMN never used the toasting, don't create the toast table from
	 * Greenplum 7
	 */
	return false;
}

/* ------------------------------------------------------------------------
 * Planner related callbacks for the heap AM
 * ------------------------------------------------------------------------
 */
static void
aoco_estimate_rel_size(Relation rel, int32 *attr_widths,
                             BlockNumber *pages, double *tuples,
                             double *allvisfrac)
{
	FileSegTotals  *fileSegTotals;
	Snapshot		snapshot;

	*pages = 1;
	*tuples = 1;
	*allvisfrac = 0;

	if (Gp_role == GP_ROLE_DISPATCH)
		return;

	snapshot = RegisterSnapshot(GetLatestSnapshot());
	fileSegTotals = GetAOCSSSegFilesTotals(rel, snapshot);

	*tuples = (double)fileSegTotals->totaltuples;

	/* Quick exit if empty */
	if (*tuples == 0)
	{
		UnregisterSnapshot(snapshot);
		*pages = 0;
		return;
	}

	Assert(fileSegTotals->totalbytesuncompressed > 0);
	*pages = RelationGuessNumberOfBlocksFromSize(
					(uint64)fileSegTotals->totalbytesuncompressed);

	UnregisterSnapshot(snapshot);
	
	/*
	 * Do not bother scanning the visimap aux table.
	 * Investigate if really needed.
	 * 
	 * Refer to the comments at the end of function
	 * appendonly_estimate_rel_size().
	 */

	return;
}

/* ------------------------------------------------------------------------
 * Executor related callbacks for the heap AM
 * ------------------------------------------------------------------------
 */
static bool
aoco_scan_bitmap_next_block(TableScanDesc scan,
                                  TBMIterateResult *tbmres)
{
	AOCSBitmapScan	aocsBitmapScan = (AOCSBitmapScan)scan;

	/* Make sure we never cross 15-bit offset number [MPP-24326] */
	Assert(tbmres->ntuples <= INT16_MAX + 1);

	/*
	 * Start scanning from the beginning of the offsets array (or
	 * at first "offset number" if it's a lossy page).
	 * In nodeBitmapHeapscan.c's BitmapHeapNext. After call
	 * `table_scan_bitmap_next_block` and return false, it doesn't
	 * clean the tbmres. Then it'll call aoco_scan_bitmap_next_tuple
	 * to try to get tuples from the skipped page, and it'll return false.
	 * Althouth aoco_scan_bitmap_next_tuple works fine.
	 * But it still be better to set these init value before return in case
	 * of wrong init value.
	 */
	aocsBitmapScan->rs_cindex = 0;

	/* If tbmres contains no tuples, continue. */
	if (tbmres->ntuples == 0)
		return false;

	/*
	 * which descriptor to be used for fetching the data
	 */
	aocsBitmapScan->whichDesc = (tbmres->recheck) ? RECHECK : NO_RECHECK;

	return true;
}

static bool
aoco_scan_bitmap_next_tuple(TableScanDesc scan,
							TBMIterateResult *tbmres,
							TupleTableSlot *slot)
{
	AOCSBitmapScan	aocsBitmapScan = (AOCSBitmapScan)scan;
	AOCSFetchDesc	aocoFetchDesc;
	OffsetNumber	pseudoOffset;
	ItemPointerData	pseudoTid;
	AOTupleId		aoTid;
	int				numTuples;

	aocoFetchDesc = aocsBitmapScan->bitmapScanDesc[aocsBitmapScan->whichDesc].bitmapFetch;
	if (aocoFetchDesc == NULL)
	{
		aocoFetchDesc = aocs_fetch_init(aocsBitmapScan->rs_base.rs_rd,
										aocsBitmapScan->rs_base.rs_snapshot,
										aocsBitmapScan->appendOnlyMetaDataSnapshot,
										aocsBitmapScan->bitmapScanDesc[aocsBitmapScan->whichDesc].proj);
		aocsBitmapScan->bitmapScanDesc[aocsBitmapScan->whichDesc].bitmapFetch = aocoFetchDesc;
	}

	ExecClearTuple(slot);

	/* ntuples == -1 indicates a lossy page */
	numTuples = (tbmres->ntuples == -1) ? INT16_MAX + 1 : tbmres->ntuples;
	while (aocsBitmapScan->rs_cindex < numTuples)
	{
		/*
		 * If it's a lossy page, iterate through all possible "offset numbers".
		 * Otherwise iterate through the array of "offset numbers".
		 */
		if (tbmres->ntuples == -1)
		{
			/*
			 * +1 to convert index to offset, since TID offsets are not zero
			 * based.
			 */
			pseudoOffset = aocsBitmapScan->rs_cindex + 1;
		}
		else
			pseudoOffset = tbmres->offsets[aocsBitmapScan->rs_cindex];

		aocsBitmapScan->rs_cindex++;

		/*
		 * Okay to fetch the tuple
		 */
		ItemPointerSet(&pseudoTid, tbmres->blockno, pseudoOffset);
		tbm_convert_appendonly_tid_out(&pseudoTid, &aoTid);

		if (aocs_fetch(aocoFetchDesc, &aoTid, slot))
		{
			/* OK to return this tuple */
			ExecStoreVirtualTuple(slot);
			pgstat_count_heap_fetch(aocsBitmapScan->rs_base.rs_rd);

			return true;
		}
	}

	/* Done with this block */
	return false;
}

static bool
aoco_scan_sample_next_block(TableScanDesc scan, SampleScanState *scanstate)
{
	/*
	 * GPDB_95_MERGE_FIXME: Add support for AO_COLUMN tables
	 */
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("invalid relation type"),
			 errhint("Sampling is only supported in heap tables.")));
}

static bool
aoco_scan_sample_next_tuple(TableScanDesc scan, SampleScanState *scanstate,
                                  TupleTableSlot *slot)
{
	/*
	 * GPDB_95_MERGE_FIXME: Add support for AO_COLUMN tables
	 */
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("invalid relation type"),
			 errhint("Sampling is only supported in heap tables.")));
}

/* ------------------------------------------------------------------------
 * Definition of the AO_COLUMN table access method.
 *
 * NOTE: While there is a lot of functionality shared with the appendoptimized
 * access method, is best for the hanlder methods to remain static in order to
 * honour the contract of the access method interface.
 * ------------------------------------------------------------------------
 */
static const TableAmRoutine ao_column_methods = {
	.type = T_TableAmRoutine,
	.slot_callbacks = aoco_slot_callbacks,

	/*
	 * GPDB: it is needed to extract the column information for
	 * scans before calling beginscan. This can not happen in beginscan because
	 * the needed information is not available at that time. It is the caller's
	 * responsibility to choose to call aoco_beginscan_extractcolumns or
	 * aoco_beginscan.
	 */
	.scan_begin_extractcolumns = aoco_beginscan_extractcolumns,

	/*
	 * GPDB: Like above but for bitmap scans.
	 */
	.scan_begin_extractcolumns_bm = aoco_beginscan_extractcolumns_bm,

	.scan_begin = aoco_beginscan,
	.scan_end = aoco_endscan,
	.scan_rescan = aoco_rescan,
	.scan_getnextslot = aoco_getnextslot,

	.parallelscan_estimate = aoco_parallelscan_estimate,
	.parallelscan_initialize = aoco_parallelscan_initialize,
	.parallelscan_reinitialize = aoco_parallelscan_reinitialize,

	.index_fetch_begin = aoco_index_fetch_begin,
	.index_fetch_reset = aoco_index_fetch_reset,
	.index_fetch_end = aoco_index_fetch_end,
	.index_fetch_tuple = aoco_index_fetch_tuple,
	.index_fetch_tuple_exists = aoco_index_fetch_tuple_exists,

	.dml_init = aoco_dml_init,
	.dml_finish = aoco_dml_finish,

	.tuple_insert = aoco_tuple_insert,
	.tuple_insert_speculative = aoco_tuple_insert_speculative,
	.tuple_complete_speculative = aoco_tuple_complete_speculative,
	.multi_insert = aoco_multi_insert,
	.tuple_delete = aoco_tuple_delete,
	.tuple_update = aoco_tuple_update,
	.tuple_lock = aoco_tuple_lock,
	.finish_bulk_insert = aoco_finish_bulk_insert,

	.tuple_fetch_row_version = aoco_fetch_row_version,
	.tuple_get_latest_tid = aoco_get_latest_tid,
	.tuple_tid_valid = aoco_tuple_tid_valid,
	.tuple_satisfies_snapshot = aoco_tuple_satisfies_snapshot,
	.compute_xid_horizon_for_tuples = aoco_compute_xid_horizon_for_tuples,

	.relation_set_new_filenode = aoco_relation_set_new_filenode,
	.relation_nontransactional_truncate = aoco_relation_nontransactional_truncate,
	.relation_copy_data = aoco_relation_copy_data,
	.relation_copy_for_cluster = aoco_relation_copy_for_cluster,
	.relation_vacuum = aoco_vacuum_rel,
	.scan_analyze_next_block = aoco_scan_analyze_next_block,
	.scan_analyze_next_tuple = aoco_scan_analyze_next_tuple,
	.index_build_range_scan = aoco_index_build_range_scan,
	.index_validate_scan = aoco_index_validate_scan,

	.relation_size = aoco_relation_size,
	.relation_needs_toast_table = aoco_relation_needs_toast_table,

	.relation_estimate_size = aoco_estimate_rel_size,

	.scan_bitmap_next_block = aoco_scan_bitmap_next_block,
	.scan_bitmap_next_tuple = aoco_scan_bitmap_next_tuple,
	.scan_sample_next_block = aoco_scan_sample_next_block,
	.scan_sample_next_tuple = aoco_scan_sample_next_tuple
};

Datum
ao_column_tableam_handler(PG_FUNCTION_ARGS)
{
	PG_RETURN_POINTER(&ao_column_methods);
}
