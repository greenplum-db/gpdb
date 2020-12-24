/*-------------------------------------------------------------------------
 *
 * appendonlyam_handler.c
 *	  appendonly table access method code
 *
 * Portions Copyright (c) 2008, Greenplum Inc
 * Portions Copyright (c) 2020-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/appendonly/appendonlyam_handler.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/aomd.h"
#include "access/appendonlywriter.h"
#include "access/heapam.h"
#include "access/multixact.h"
#include "access/tableam.h"
#include "access/tuptoaster.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/pg_am.h"
#include "catalog/pg_appendonly_fn.h"
#include "catalog/storage.h"
#include "catalog/storage_xlog.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbvars.h"
#include "commands/vacuum.h"
#include "executor/executor.h"
#include "pgstat.h"
#include "storage/procarray.h"
#include "utils/builtins.h"
#include "utils/faultinjector.h"
#include "utils/lsyscache.h"
#include "utils/pg_rusage.h"

#define IS_BTREE(r) ((r)->rd_rel->relam == BTREE_AM_OID)

static void reset_state_cb(void *arg);

static const TableAmRoutine ao_row_methods;

typedef struct AppendOnlyDMLState
{
	Oid relationOid;
	AppendOnlyInsertDesc	insertDesc;
	AppendOnlyDeleteDesc	deleteDesc;
} AppendOnlyDMLState;


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
static struct AppendOnlyLocal
{
	AppendOnlyDMLState	   *last_used_state;
	HTAB				   *dmlDescriptorTab;

	MemoryContext			stateCxt;
	MemoryContextCallback	cb;
} appendOnlyLocal	  = {
	.last_used_state  = NULL,
	.dmlDescriptorTab = NULL,

	.stateCxt		  = NULL,
	.cb				  = {
		.func	= reset_state_cb,
		.arg	= NULL
	},
};

/* ------------------------------------------------------------------------
 * DML state related functions
 * ------------------------------------------------------------------------
 */

/*
 * This function should be called with a current memory context whose life
 * span is enough to last until the end of this command execution.
 */
static void
init_dml_local_state(void)
{
	HASHCTL hash_ctl;

	if (!appendOnlyLocal.dmlDescriptorTab)
	{
		Assert(appendOnlyLocal.stateCxt == NULL);
		appendOnlyLocal.stateCxt = AllocSetContextCreate(
												CurrentMemoryContext,
												"AppendOnly DML State Context",
												ALLOCSET_SMALL_SIZES);
		MemoryContextRegisterResetCallback(
								appendOnlyLocal.stateCxt,
							   &appendOnlyLocal.cb);

		memset(&hash_ctl, 0, sizeof(hash_ctl));
		hash_ctl.keysize = sizeof(Oid);
		hash_ctl.entrysize = sizeof(AppendOnlyDMLState);
		hash_ctl.hcxt = appendOnlyLocal.stateCxt;
		appendOnlyLocal.dmlDescriptorTab =
			hash_create("AppendOnly DML state", 128, &hash_ctl,
						HASH_CONTEXT | HASH_ELEM | HASH_BLOBS);
	}
}


/*
 * There are disctinct *_dm_state functions in order to document a bit better
 * the intention behind each one of those and keep them as thin as possible.
 */

/*
 * Create and insert a state entry for a relation. The actual descriptors will
 * be created lazily when/if needed.
 *
 * Should be called exactly once per relation.
 */
static inline AppendOnlyDMLState *
enter_dml_state(const Oid relationOid)
{
	AppendOnlyDMLState *state;
	bool				found;

	Assert(appendOnlyLocal.dmlDescriptorTab);

	state = (AppendOnlyDMLState *) hash_search(
										appendOnlyLocal.dmlDescriptorTab,
									   &relationOid,
										HASH_ENTER,
									   &found);

	Assert(!found);

	state->insertDesc = NULL;
	state->deleteDesc = NULL;

	appendOnlyLocal.last_used_state = state;
	return state;
}

/*
 * Retrieve the state information for a relation.
 * It is required that the state has been created before hand.
 */
static inline AppendOnlyDMLState *
find_dml_state(const Oid relationOid)
{
	AppendOnlyDMLState *state;
	Assert(appendOnlyLocal.dmlDescriptorTab);

	if (appendOnlyLocal.last_used_state &&
			appendOnlyLocal.last_used_state->relationOid == relationOid)
		return appendOnlyLocal.last_used_state;

	state = (AppendOnlyDMLState *) hash_search(
										appendOnlyLocal.dmlDescriptorTab,
									   &relationOid,
										HASH_FIND,
										NULL);

	Assert(state);

	appendOnlyLocal.last_used_state = state;
	return state;
}

/*
 * Remove the state information for a relation.
 * It is required that the state has been created before hand.
 *
 * Should be called exactly once per relation.
 */
static inline AppendOnlyDMLState *
remove_dml_state(const Oid relationOid)
{
	AppendOnlyDMLState *state;
	Assert(appendOnlyLocal.dmlDescriptorTab);

	state = (AppendOnlyDMLState *) hash_search(
										appendOnlyLocal.dmlDescriptorTab,
									   &relationOid,
										HASH_REMOVE,
										NULL);

	Assert(state);

	if (appendOnlyLocal.last_used_state &&
			appendOnlyLocal.last_used_state->relationOid == relationOid)
		appendOnlyLocal.last_used_state = NULL;

	return state;
}

/*
 * Although the operation param is superfluous at the momment, the signature of
 * the function is such for balance between the init and finish.
 *
 * This function should be called exactly once per relation.
 */
void
appendonly_dml_init(Relation relation, CmdType operation)
{
	init_dml_local_state();
	(void) enter_dml_state(RelationGetRelid(relation));
}

/*
 * This function should be called exactly once per relation.
 */
void
appendonly_dml_finish(Relation relation, CmdType operation)
{
	AppendOnlyDMLState *state;

	state = remove_dml_state(RelationGetRelid(relation));

	if (state->deleteDesc)
	{
		appendonly_delete_finish(state->deleteDesc);
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
		appendonly_insert_finish(state->insertDesc);
		state->insertDesc = NULL;
	}
}

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
	appendOnlyLocal.dmlDescriptorTab = NULL;
	appendOnlyLocal.last_used_state = NULL;
	appendOnlyLocal.stateCxt = NULL;
}

/*
 * Retrieve the insertDescriptor for a relation. Initialize it if needed.
 */
static AppendOnlyInsertDesc
get_insert_descriptor(const Relation relation)
{
	AppendOnlyDMLState *state;

	state = find_dml_state(RelationGetRelid(relation));

	if (state->insertDesc == NULL)
	{
		MemoryContext oldcxt;

		oldcxt = MemoryContextSwitchTo(appendOnlyLocal.stateCxt);
		state->insertDesc = appendonly_insert_init(relation,
												   ChooseSegnoForWrite(relation));
		MemoryContextSwitchTo(oldcxt);
	}

	return state->insertDesc;
}

/*
 * Retrieve the deleteDescriptor for a relation. Initialize it if needed.
 */
static AppendOnlyDeleteDesc
get_delete_descriptor(const Relation relation, bool forUpdate)
{
	AppendOnlyDMLState *state;

	state = find_dml_state(RelationGetRelid(relation));

	if (state->deleteDesc == NULL)
	{
		/*
		 * GPDB_12_MERGE_FIXME: Can we perform this check earlier on?
		 * Example during init? Idealy should be called on master node first,
		 * that way we will avoid the dispatch.
		 */
		MemoryContext oldcxt;
		if (IsolationUsesXactSnapshot())
		{
			if (forUpdate)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("updates on append-only tables are not supported in serializable transactions")));
			else
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("deletes on append-only tables are not supported in serializable transactions")));
		}

		oldcxt = MemoryContextSwitchTo(appendOnlyLocal.stateCxt);
		state->deleteDesc = appendonly_delete_init(relation);
		MemoryContextSwitchTo(oldcxt);
	}

	return state->deleteDesc;
}


/* ------------------------------------------------------------------------
 * Slot related callbacks for appendonly AM
 * ------------------------------------------------------------------------
 */

/*
 * Appendonly access method uses virtual tuples
 */
static const TupleTableSlotOps *
appendonly_slot_callbacks(Relation relation)
{
	return &TTSOpsVirtual;
}

MemTuple
ExecFetchSlotMemTuple(TupleTableSlot *slot, bool *shouldFree, MemTupleBinding *mt_bind)
{
	MemTuple		result;
	MemoryContext	oldContext;

	*shouldFree = true;

	/*
	 * In case of a non virtal tuple, make certain that the slot's values are
	 * populated, for example during a CTAS.
	 */
	if (!TTS_IS_VIRTUAL(slot))
		slot_getsomeattrs(slot, slot->tts_tupleDescriptor->natts);

	oldContext = MemoryContextSwitchTo(slot->tts_mcxt);
	result = memtuple_form(mt_bind,
						   slot->tts_values,
						   slot->tts_isnull);
	MemoryContextSwitchTo(oldContext);

	return result;
}

/* ------------------------------------------------------------------------
 * Parallel aware Seq Scan callbacks for ao_row AM
 * ------------------------------------------------------------------------
 */

static Size
appendonly_parallelscan_estimate(Relation rel)
{
	elog(ERROR, "parallel SeqScan not implemented for AO_ROW tables");
}

static Size
appendonly_parallelscan_initialize(Relation rel, ParallelTableScanDesc pscan)
{
	elog(ERROR, "parallel SeqScan not implemented for AO_ROW tables");
}

static void
appendonly_parallelscan_reinitialize(Relation rel, ParallelTableScanDesc pscan)
{
	elog(ERROR, "parallel SeqScan not implemented for AO_ROW tables");
}

/* ------------------------------------------------------------------------
 * Seq Scan callbacks for appendonly AM
 *
 * These are in appendonlyam.c
 * ------------------------------------------------------------------------
 */

/* ------------------------------------------------------------------------
 * Index Scan Callbacks for appendonly AM
 * ------------------------------------------------------------------------
 */

static IndexFetchTableData *
appendonly_index_fetch_begin(Relation rel)
{
	IndexFetchAppendOnlyData *aoscan = palloc0(sizeof(IndexFetchAppendOnlyData));

	aoscan->xs_base.rel = rel;

	/* aoscan->aofetch is initialized lazily on first fetch */

	return &aoscan->xs_base;
}

static void
appendonly_index_fetch_reset(IndexFetchTableData *scan)
{
	// GPDB_12_MERGE_FIXME: Should we close the underlying AO fetch desc here?
}

static void
appendonly_index_fetch_end(IndexFetchTableData *scan)
{
	IndexFetchAppendOnlyData *aoscan = (IndexFetchAppendOnlyData *) scan;

	if (aoscan->aofetch)
	{
		appendonly_fetch_finish(aoscan->aofetch);
		pfree(aoscan->aofetch);
		aoscan->aofetch = NULL;
	}
}

static bool
appendonly_index_fetch_tuple(struct IndexFetchTableData *scan,
							 ItemPointer tid,
							 Snapshot snapshot,
							 TupleTableSlot *slot,
							 bool *call_again, bool *all_dead)
{
	IndexFetchAppendOnlyData *aoscan = (IndexFetchAppendOnlyData *) scan;

	if (!aoscan->aofetch)
	{
		Snapshot	appendOnlyMetaDataSnapshot;

		appendOnlyMetaDataSnapshot = snapshot;
		if (appendOnlyMetaDataSnapshot == SnapshotAny)
		{
			/*
			 * the append-only meta data should never be fetched with
			 * SnapshotAny as bogus results are returned.
			 */
			appendOnlyMetaDataSnapshot = GetTransactionSnapshot();
		}

		aoscan->aofetch =
			appendonly_fetch_init(aoscan->xs_base.rel,
								  snapshot,
								  appendOnlyMetaDataSnapshot);
	}
	else
	{
		/* GPDB_12_MERGE_FIXME: Is it possible for the 'snapshot' to change
		 * between calls? Add a sanity check for that here. */
	}

	appendonly_fetch(aoscan->aofetch, (AOTupleId *) tid, slot);

	return !TupIsNull(slot);
}


/* ------------------------------------------------------------------------
 * Callbacks for non-modifying operations on individual tuples for
 * appendonly AM
 * ------------------------------------------------------------------------
 */

static bool
appendonly_fetch_row_version(Relation relation,
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
appendonly_get_latest_tid(TableScanDesc sscan,
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
appendonly_tuple_tid_valid(TableScanDesc scan, ItemPointer tid)
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
appendonly_tuple_satisfies_snapshot(Relation rel, TupleTableSlot *slot,
								Snapshot snapshot)
{
	/*
	 * AO table dose not support unique and tidscan yet.
	 */
	ereport(ERROR,
	        (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		        errmsg("feature not supported on appendoptimized relations")));
}

static TransactionId
appendonly_compute_xid_horizon_for_tuples(Relation rel,
										  ItemPointerData *tids,
										  int nitems)
{
	// GPDB_12_MERGE_FIXME: vacuum related call back.
	elog(ERROR, "not implemented yet");
}

/* ----------------------------------------------------------------------------
 *  Functions for manipulations of physical tuples for appendonly AM.
 * ----------------------------------------------------------------------------
 */

static void
appendonly_tuple_insert(Relation relation, TupleTableSlot *slot, CommandId cid,
					int options, BulkInsertState bistate)
{
	AppendOnlyInsertDesc    insertDesc;
	MemTuple				mtuple;
	bool					shouldFree = true;

	insertDesc = get_insert_descriptor(relation);
	mtuple = ExecFetchSlotMemTuple(slot, &shouldFree, insertDesc->mt_bind);

	/* Update the tuple with table oid */
	slot->tts_tableOid = RelationGetRelid(relation);

	/* Perform the insertion, and copy the resulting ItemPointer */
	appendonly_insert(insertDesc, mtuple, (AOTupleId *) &slot->tts_tid);

	pgstat_count_heap_insert(relation, 1);

	if (shouldFree)
		pfree(mtuple);
}

static void
appendonly_tuple_insert_speculative(Relation relation, TupleTableSlot *slot,
								CommandId cid, int options,
								BulkInsertState bistate, uint32 specToken)
{
 	/* GPDB_12_MERGE_FIXME: not supported. Can this function be left out completely? Or ereport()? */
	elog(ERROR, "speculative insertion not supported on AO tables");
}

static void
appendonly_tuple_complete_speculative(Relation relation, TupleTableSlot *slot,
								  uint32 specToken, bool succeeded)
{
	elog(ERROR, "speculative insertion not supported on AO tables");
}

/*
 *	appendonly_multi_insert	- insert multiple tuples into an ao relation
 *
 * This is like appendonly_tuple_insert(), but inserts multiple tuples in one
 * operation. Typicaly used by COPY. This is preferrable than calling
 * appendonly_tuple_insert() in a loop because ... WAL??
 */
static void
appendonly_multi_insert(Relation relation, TupleTableSlot **slots, int ntuples,
						CommandId cid, int options, BulkInsertState bistate)
{
	/*
	 * GPDB_12_MERGE_FIXME: Poor man's implementation for now in order to make
	 * the tests pass. Implement properly.
	 */
	for (int i = 0; i < ntuples; i++)
		appendonly_tuple_insert(relation, slots[i], cid, options, bistate);
}

static TM_Result
appendonly_tuple_delete(Relation relation, ItemPointer tid, CommandId cid,
					Snapshot snapshot, Snapshot crosscheck, bool wait,
					TM_FailureData *tmfd, bool changingPart)
{
	AppendOnlyDeleteDesc	deleteDesc;
	TM_Result				result;

	deleteDesc = get_delete_descriptor(relation, false);
	result = appendonly_delete(deleteDesc, (AOTupleId *) tid);
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
appendonly_tuple_update(Relation relation, ItemPointer otid, TupleTableSlot *slot,
					CommandId cid, Snapshot snapshot, Snapshot crosscheck,
					bool wait, TM_FailureData *tmfd,
					LockTupleMode *lockmode, bool *update_indexes)
{
	AppendOnlyInsertDesc	insertDesc;
	AppendOnlyDeleteDesc	deleteDesc;
	MemTuple				mtuple;
	TM_Result				result;
	bool					shouldFree = true;

	insertDesc = get_insert_descriptor(relation);
	deleteDesc = get_delete_descriptor(relation, true);

	/* Update the tuple with table oid */
	slot->tts_tableOid = RelationGetRelid(relation);

	mtuple = ExecFetchSlotMemTuple(slot, &shouldFree,
								   insertDesc->mt_bind);

#ifdef FAULT_INJECTOR
	FaultInjector_InjectFaultIfSet(
								   "appendonly_update",
								   DDLNotSpecified,
								   "", //databaseName
								   RelationGetRelationName(insertDesc->aoi_rel));
	/* tableName */
#endif

	result = appendonly_delete(deleteDesc, (AOTupleId *) otid);
	if (result != TM_Ok)
		return result;

	appendonly_insert(insertDesc, mtuple, (AOTupleId *) &slot->tts_tid);

	pgstat_count_heap_update(relation, false);
	/* No HOT updates with AO tables. */
	*update_indexes = true;

	if (shouldFree)
		pfree(mtuple);

	return result;
}

static TM_Result
appendonly_tuple_lock(Relation relation, ItemPointer tid, Snapshot snapshot,
				  TupleTableSlot *slot, CommandId cid, LockTupleMode mode,
				  LockWaitPolicy wait_policy, uint8 flags,
				  TM_FailureData *tmfd)
{
 	/* GPDB_12_MERGE_FIXME: not supported. Can this function be left out completely? Or ereport()? */
	elog(ERROR, "speculative insertion not supported on AO tables");
}

static void
appendonly_finish_bulk_insert(Relation relation, int options)
{
	appendonly_dml_finish(relation, CMD_INSERT);
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

/* ------------------------------------------------------------------------
 * DDL related callbacks for appendonly AM.
 * ------------------------------------------------------------------------
 */
static void
appendonly_relation_set_new_filenode(Relation rel,
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
		log_smgrcreate(newrnode, INIT_FORKNUM);
		smgrimmedsync(srel, INIT_FORKNUM);
	}

	smgrclose(srel);
}

static void
appendonly_relation_nontransactional_truncate(Relation rel)
{
	Oid ao_base_relid = RelationGetRelid(rel);

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
appendonly_relation_copy_data(Relation rel, const RelFileNode *newrnode)
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

		log_smgrcreate(newrnode, INIT_FORKNUM);
	}

	/* drop old relation, and close new one */
	RelationDropStorage(rel);
	smgrclose(dstrel);
}

static void
appendonly_vacuum_rel(Relation onerel, VacuumParams *params,
					  BufferAccessStrategy bstrategy)
{
	/*
	 * GPDB_12_MERGE_FIXME: This is a dummy function in order to proceed with
	 * the implementation of the appendonlyam_handler.
	 *
	 * It's not invoked ever, we do the AO different phases vacuuming in
	 * vacuum_rel() directly for now.
	 *
	 * A snipped implementation exists in appendonly_vacuum.c which would need
	 * to get revived here.
	 */
	return;
}

static void
appendonly_relation_copy_for_cluster(Relation OldHeap, Relation NewHeap,
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
	AppendOnlyInsertDesc	aoInsertDesc = NULL;
	MemTupleBinding*		mt_bind = NULL;
	int						write_seg_no;
	MemTuple				mtuple = NULL;
	TupleTableSlot		   *slot;
	TableScanDesc			aoscandesc;

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
	aoscandesc = appendonly_beginscan(OldHeap, GetActiveSnapshot(), 0, NULL,
										NULL, 0);
	mt_bind = create_memtuple_binding(oldTupDesc);

	while (appendonly_getnextslot(aoscandesc, ForwardScanDirection, slot))
	{
		Datum	   *slot_values;
		bool	   *slot_isnull;
		HeapTuple   tuple;

		CHECK_FOR_INTERRUPTS();

		/* Extract all the values of the tuple */
		slot_getallattrs(slot);
		slot_values = slot->tts_values;
		slot_isnull = slot->tts_isnull;
		tuple = heap_form_tuple(oldTupDesc, slot_values, slot_isnull);

		*num_tuples += 1;
		tuplesort_putheaptuple(tuplesort, tuple);
		heap_freetuple(tuple);
	}

	ExecDropSingleTupleTableSlot(slot);

	appendonly_endscan(aoscandesc);

	/*
	 * Сomplete the sort, then read out all tuples
	 * from the tuplestore and write them to the new relation.
	 */

	tuplesort_performsort(tuplesort);
	
	write_seg_no = ChooseSegnoForWrite(NewHeap);

	aoInsertDesc = appendonly_insert_init(NewHeap, write_seg_no);

	/* Insert sorted heap tuples into new storage */
	for (;;)
	{
		HeapTuple	tuple;
		uint32		prev_memtuple_len = 0;
		uint32		len;
		uint32		null_save_len;
		bool		has_nulls;

		CHECK_FOR_INTERRUPTS();

		tuple = tuplesort_getheaptuple(tuplesort, true);
		if (tuple == NULL)
			break;

		heap_deform_tuple(tuple, oldTupDesc, values, isnull);

		len = compute_memtuple_size(mt_bind, values, isnull, &null_save_len, &has_nulls);
		if (len > prev_memtuple_len)
		{
			/* Here we are trying to avoid reallocation of temp mtuple */
			if (mtuple != NULL)
				pfree(mtuple);
			mtuple = palloc(len);
			prev_memtuple_len = len;
		}

		memtuple_form_to(mt_bind, values, isnull, len, null_save_len, has_nulls,
					mtuple);

		appendonly_insert(aoInsertDesc, mtuple, &aoTupleId);
	}

	tuplesort_end(tuplesort);

	/* Finish and deallocate insertion */
	appendonly_insert_finish(aoInsertDesc);
}

static bool
appendonly_scan_analyze_next_block(TableScanDesc scan, BlockNumber blockno,
							   BufferAccessStrategy bstrategy)
{
	AppendOnlyScanDesc aoscan = (AppendOnlyScanDesc) scan;
	aoscan->targetTupleId = blockno;

	return true;
}

static bool
appendonly_scan_analyze_next_tuple(TableScanDesc scan, TransactionId OldestXmin,
							   double *liverows, double *deadrows,
							   TupleTableSlot *slot)
{
	AppendOnlyScanDesc aoscan = (AppendOnlyScanDesc) scan;
	bool		ret = false;

	/* skip several tuples if they are not sampling target */
	while (!aoscan->aos_done_all_segfiles
		   && aoscan->targetTupleId > aoscan->nextTupleId)
	{
		appendonly_getnextslot(scan, ForwardScanDirection, slot);
		aoscan->nextTupleId++;
	}

	if (!aoscan->aos_done_all_segfiles
		&& aoscan->targetTupleId == aoscan->nextTupleId)
	{
		ret = appendonly_getnextslot(scan, ForwardScanDirection, slot);
		aoscan->nextTupleId++;

		if (ret)
			*liverows += 1;
		else
			*deadrows += 1; /* if return an invisible tuple */
	}

	return ret;
}

static double
appendonly_index_build_range_scan(Relation heapRelation,
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
	AppendOnlyScanDesc aoscan;
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

		scan = table_beginscan_strat(heapRelation,	/* relation */
									 snapshot,	/* snapshot */
									 0, /* number of keys */
									 NULL,	/* scan key */
									 true,	/* buffer access strategy OK */
									 allow_sync);	/* syncscan OK? */
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

	aoscan = (AppendOnlyScanDesc) scan;

	/*
	 * If block directory is empty, it must also be built along with the index.
	 */
	Oid blkdirrelid;
	Oid blkidxrelid;

	GetAppendOnlyEntryAuxOids(RelationGetRelid(aoscan->aos_rd), NULL, NULL,
							  &blkdirrelid, &blkidxrelid, NULL, NULL);
	/*
	 * Note that block directory is created during creation of the first
	 * index.  If it is found empty, it means the block directory was created
	 * by this create index transaction.  The caller (DefineIndex) must have
	 * acquired sufficiently strong lock on the appendoptimized table such
	 * that index creation as well as insert from concurrent transactions are
	 * blocked.  We can rest assured of exclusive access to the block
	 * directory relation.
	 */
	Relation blkdir = relation_open(blkdirrelid, AccessShareLock);
	if (RelationGetNumberOfBlocks(blkdir) == 0)
	{
		/*
		 * Allocate blockDirectory in scan descriptor to let the access method
		 * know that it needs to also build the block directory while
		 * scanning.
		 */
		Assert(aoscan->blockDirectory == NULL);
		aoscan->blockDirectory = palloc0(sizeof(AppendOnlyBlockDirectory));
	}
	relation_close(blkdir, NoLock);


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
	while (appendonly_getnextslot(&aoscan->rs_base, ForwardScanDirection, slot))
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
		 * here. Is that not needed with AO tables?
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
appendonly_index_validate_scan(Relation heapRelation,
						   Relation indexRelation,
						   IndexInfo *indexInfo,
						   Snapshot snapshot,
						   ValidateIndexState *state)
{
	TableScanDesc scan;
	AppendOnlyScanDesc aoscan;
	HeapTuple	heapTuple;
	Datum		values[INDEX_MAX_KEYS];
	bool		isnull[INDEX_MAX_KEYS];
	ExprState  *predicate;
	TupleTableSlot *slot;
	EState	   *estate;
	ExprContext *econtext;
	BlockNumber root_blkno = InvalidBlockNumber;
#if 0
	OffsetNumber root_offsets[MaxHeapTuplesPerPage];
#endif
	bool		in_index[MaxHeapTuplesPerPage];

	/* state variables for the merge */
	ItemPointer indexcursor = NULL;
	ItemPointerData decoded;
	bool		tuplesort_empty = false;

	/*
	 * sanity checks
	 */
	Assert(OidIsValid(indexRelation->rd_rel->relam));

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
	 * Prepare for scan of the base relation.  We need just those tuples
	 * satisfying the passed-in reference snapshot.  We must disable syncscan
	 * here, because it's critical that we read from block zero forward to
	 * match the sorted TIDs.
	 */
	scan = table_beginscan_strat(heapRelation,	/* relation */
								 snapshot,	/* snapshot */
								 0, /* number of keys */
								 NULL,	/* scan key */
								 true,	/* buffer access strategy OK */
								 false);	/* syncscan not OK */
	aoscan = (AppendOnlyScanDesc) scan;

	/* GPDB_12_MERGE_FIXME */
#if 0
	pgstat_progress_update_param(PROGRESS_SCAN_BLOCKS_TOTAL,
								 aoscan->rs_nblocks);
#endif

	/*
	 * Scan all tuples matching the snapshot.
	 */
	while ((heapTuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
	{
		ItemPointer heapcursor = &heapTuple->t_self;
		ItemPointerData rootTuple;
		OffsetNumber root_offnum;

		CHECK_FOR_INTERRUPTS();

		state->htups += 1;

	/* GPDB_12_MERGE_FIXME */
#if 0
		if ((previous_blkno == InvalidBlockNumber) ||
			(aoscan->rs_cblock != previous_blkno))
		{
			pgstat_progress_update_param(PROGRESS_SCAN_BLOCKS_DONE,
										 aoscan->rs_cblock);
			previous_blkno = aoscan->rs_cblock;
		}
#endif

		/* Convert actual tuple TID to root TID */
		rootTuple = *heapcursor;
		root_offnum = ItemPointerGetOffsetNumber(heapcursor);

		if (HeapTupleIsHeapOnly(heapTuple))
		{
			/* GPDB_12_MERGE_FIXME: root_offsets unitialized */
#if 0
			root_offnum = root_offsets[root_offnum - 1];
			if (!OffsetNumberIsValid(root_offnum))
				ereport(ERROR,
						(errcode(ERRCODE_DATA_CORRUPTED),
						 errmsg_internal("failed to find parent tuple for heap-only tuple at (%u,%u) in table \"%s\"",
										 ItemPointerGetBlockNumber(heapcursor),
										 ItemPointerGetOffsetNumber(heapcursor),
										 RelationGetRelationName(heapRelation))));
			ItemPointerSetOffsetNumber(&rootTuple, root_offnum);
#else
			elog(ERROR, "GPDB_12_MERGE_FIXME");
#endif
		}

		/*
		 * "merge" by skipping through the index tuples until we find or pass
		 * the current root tuple.
		 */
		while (!tuplesort_empty &&
			   (!indexcursor ||
				ItemPointerCompare(indexcursor, &rootTuple) < 0))
		{
			Datum		ts_val;
			bool		ts_isnull;

			if (indexcursor)
			{
				/*
				 * Remember index items seen earlier on the current heap page
				 */
				if (ItemPointerGetBlockNumber(indexcursor) == root_blkno)
					in_index[ItemPointerGetOffsetNumber(indexcursor) - 1] = true;
			}

			tuplesort_empty = !tuplesort_getdatum(state->tuplesort, true,
												  &ts_val, &ts_isnull, NULL);
			Assert(tuplesort_empty || !ts_isnull);
			if (!tuplesort_empty)
			{
				itemptr_decode(&decoded, DatumGetInt64(ts_val));
				indexcursor = &decoded;

				/* If int8 is pass-by-ref, free (encoded) TID Datum memory */
#ifndef USE_FLOAT8_BYVAL
				pfree(DatumGetPointer(ts_val));
#endif
			}
			else
			{
				/* Be tidy */
				indexcursor = NULL;
			}
		}

		/*
		 * If the tuplesort has overshot *and* we didn't see a match earlier,
		 * then this tuple is missing from the index, so insert it.
		 */
		if ((tuplesort_empty ||
			 ItemPointerCompare(indexcursor, &rootTuple) > 0) &&
			!in_index[root_offnum - 1])
		{
			MemoryContextReset(econtext->ecxt_per_tuple_memory);

			/* Set up for predicate or expression evaluation */
			ExecStoreHeapTuple(heapTuple, slot, false);

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
			 * For the current heap tuple, extract all the attributes we use
			 * in this index, and note which are null.  This also performs
			 * evaluation of any expressions needed.
			 */
			FormIndexDatum(indexInfo,
						   slot,
						   estate,
						   values,
						   isnull);

			/*
			 * You'd think we should go ahead and build the index tuple here,
			 * but some index AMs want to do further processing on the data
			 * first. So pass the values[] and isnull[] arrays, instead.
			 */

			/*
			 * If the tuple is already committed dead, you might think we
			 * could suppress uniqueness checking, but this is no longer true
			 * in the presence of HOT, because the insert is actually a proxy
			 * for a uniqueness check on the whole HOT-chain.  That is, the
			 * tuple we have here could be dead because it was already
			 * HOT-updated, and if so the updating transaction will not have
			 * thought it should insert index entries.  The index AM will
			 * check the whole HOT-chain and correctly detect a conflict if
			 * there is one.
			 */

			index_insert(indexRelation,
						 values,
						 isnull,
						 &rootTuple,
						 heapRelation,
						 indexInfo->ii_Unique ?
						 UNIQUE_CHECK_YES : UNIQUE_CHECK_NO,
						 indexInfo);

			state->tups_inserted += 1;
		}
	}

	table_endscan(scan);

	ExecDropSingleTupleTableSlot(slot);

	FreeExecutorState(estate);

	/* These may have been pointing to the now-gone estate */
	indexInfo->ii_ExpressionsState = NIL;
	indexInfo->ii_PredicateState = NULL;
}

/* ------------------------------------------------------------------------
 * Miscellaneous callbacks for the appendonly AM
 * ------------------------------------------------------------------------
 */

/*
 * This pretends that the all the space is taken by the main fork.
 */
static uint64
appendonly_relation_size(Relation rel, ForkNumber forkNumber)
{
	Relation	pg_aoseg_rel;
	TupleDesc	pg_aoseg_dsc;
	HeapTuple	tuple;
	SysScanDesc aoscan;
	int64		result;
	Datum		eof;
	bool		isNull;
	Oid segrelid = InvalidOid;

	if (forkNumber != MAIN_FORKNUM)
		return 0;

	result = 0;

	GetAppendOnlyEntryAuxOids(rel->rd_id, NULL, &segrelid, NULL,
			NULL, NULL, NULL);

	if (segrelid == InvalidOid)
		elog(ERROR, "could not find pg_aoseg aux table for AO table \"%s\"",
			 RelationGetRelationName(rel));

	pg_aoseg_rel = table_open(segrelid, AccessShareLock);
	pg_aoseg_dsc = RelationGetDescr(pg_aoseg_rel);

	aoscan = systable_beginscan(pg_aoseg_rel, InvalidOid, false, NULL, 0, NULL);

	while ((tuple = systable_getnext(aoscan)) != NULL)
	{
		eof = fastgetattr(tuple, Anum_pg_aoseg_eof, pg_aoseg_dsc, &isNull);
		Assert(!isNull);

		result += DatumGetInt64(eof);

		CHECK_FOR_INTERRUPTS();
	}

	systable_endscan(aoscan);
	table_close(pg_aoseg_rel, AccessShareLock);

	return result;
}

/*
 * Check to see whether the table needs a TOAST table.  It does only if
 * (1) there are any toastable attributes, and (2) the maximum length
 * of a tuple could exceed TOAST_TUPLE_THRESHOLD.  (We don't want to
 * create a toast table for something like "f1 varchar(20)".)
 */
static bool
appendonly_relation_needs_toast_table(Relation rel)
{
	int32		data_length = 0;
	bool		maxlength_unknown = false;
	bool		has_toastable_attrs = false;
	TupleDesc	tupdesc = rel->rd_att;
	int32		tuple_length;
	int			i;

	for (i = 0; i < tupdesc->natts; i++)
	{
		Form_pg_attribute att = TupleDescAttr(tupdesc, i);

		if (att->attisdropped)
			continue;
		data_length = att_align_nominal(data_length, att->attalign);
		if (att->attlen > 0)
		{
			/* Fixed-length types are never toastable */
			data_length += att->attlen;
		}
		else
		{
			int32		maxlen = type_maximum_size(att->atttypid,
												   att->atttypmod);

			if (maxlen < 0)
				maxlength_unknown = true;
			else
				data_length += maxlen;
			if (att->attstorage != 'p')
				has_toastable_attrs = true;
		}
	}
	if (!has_toastable_attrs)
		return false;			/* nothing to toast? */
	if (maxlength_unknown)
		return true;			/* any unlimited-length attrs? */
	tuple_length = MAXALIGN(SizeofHeapTupleHeader +
							BITMAPLEN(tupdesc->natts)) +
		MAXALIGN(data_length);
	return (tuple_length > TOAST_TUPLE_THRESHOLD);
}

/* ------------------------------------------------------------------------
 * Planner related callbacks for the appendonly AM
 * ------------------------------------------------------------------------
 */
static void
appendonly_estimate_rel_size(Relation rel, int32 *attr_widths,
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
	fileSegTotals = GetSegFilesTotals(rel, snapshot);

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
	 * GPDB_12_MERGE_FIXME: Do not bother scanning the visimap aux table.
	 * Investigate if really needed
	 */
	return;
}


/* ------------------------------------------------------------------------
 * Executor related callbacks for the appendonly AM
 * ------------------------------------------------------------------------
 */

static bool
appendonly_scan_bitmap_next_block(TableScanDesc scan,
								  TBMIterateResult *tbmres)
{
	AppendOnlyScanDesc aoscan = (AppendOnlyScanDesc) scan;

	/* If tbmres contains no tuples, continue. */
	if (tbmres->ntuples == 0)
		return false;

	/* Make sure we never cross 15-bit offset number [MPP-24326] */
	Assert(tbmres->ntuples <= INT16_MAX + 1);

	/*
	 * Start scanning from the beginning of the offsets array (or
	 * at first "offset number" if it's a lossy page).
	 */
	aoscan->rs_cindex = 0;

	return true;
}

static bool
appendonly_scan_bitmap_next_tuple(TableScanDesc scan,
								  TBMIterateResult *tbmres,
								  TupleTableSlot *slot)
{
	AppendOnlyScanDesc aoscan = (AppendOnlyScanDesc) scan;
	OffsetNumber pseudoHeapOffset;
	ItemPointerData pseudoHeapTid;
	AOTupleId	aoTid;

	if (aoscan->aofetch == NULL)
	{
		aoscan->aofetch =
			appendonly_fetch_init(aoscan->rs_base.rs_rd,
			                      aoscan->snapshot,
			                      aoscan->appendOnlyMetaDataSnapshot);

	}

	for (;;)
	{
		/*
		 * If it's a lossy pase, iterate through all possible "offset numbers".
		 * Otherwise iterate through the array of "offset numbers".
		 */
		if (tbmres->ntuples == -1)
		{
			if (aoscan->rs_cindex == INT16_MAX + 1)
				return false;

			/*
			 * +1 to convert index to offset, since TID offsets are not zero
			 * based.
			 */
			pseudoHeapOffset = aoscan->rs_cindex + 1;
		}
		else
		{
			if (aoscan->rs_cindex == tbmres->ntuples)
				return false;

			pseudoHeapOffset = tbmres->offsets[aoscan->rs_cindex];
		}
		aoscan->rs_cindex++;

		/*
		 * Okay to fetch the tuple
		 */
		ItemPointerSet(&pseudoHeapTid, tbmres->blockno, pseudoHeapOffset);

		tbm_convert_appendonly_tid_out(&pseudoHeapTid, &aoTid);

		appendonly_fetch(aoscan->aofetch, &aoTid, slot);

		if (TupIsNull(slot))
			continue;

		pgstat_count_heap_fetch(aoscan->aos_rd);

		/* OK to return this tuple */
		return true;
	}
}

static bool
appendonly_scan_sample_next_block(TableScanDesc scan, SampleScanState *scanstate)
{
	/*
	 * GPDB_95_MERGE_FIXME: Add support for AO tables
	 */
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("invalid relation type"),
			 errhint("Sampling is only supported in heap tables.")));
}

static bool
appendonly_scan_sample_next_tuple(TableScanDesc scan, SampleScanState *scanstate,
							  TupleTableSlot *slot)
{
	/*
	 * GPDB_95_MERGE_FIXME: Add support for AO tables
	 */
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("invalid relation type"),
			 errhint("Sampling is only supported in heap tables.")));
}

/* ------------------------------------------------------------------------
 * Definition of the appendonly table access method.
 *
 * NOTE: While there is a lot of functionality shared with the ao_column access
 * method, is best for the handler methods to remain static in order to honour
 * the contract of the access method interface.
 * ------------------------------------------------------------------------
 */

static const TableAmRoutine ao_row_methods = {
	.type = T_TableAmRoutine,

	.slot_callbacks = appendonly_slot_callbacks,

	.scan_begin = appendonly_beginscan,
	.scan_end = appendonly_endscan,
	.scan_rescan = appendonly_rescan,
	.scan_getnextslot = appendonly_getnextslot,

	.parallelscan_estimate = appendonly_parallelscan_estimate,
	.parallelscan_initialize = appendonly_parallelscan_initialize,
	.parallelscan_reinitialize = appendonly_parallelscan_reinitialize,

	.index_fetch_begin = appendonly_index_fetch_begin,
	.index_fetch_reset = appendonly_index_fetch_reset,
	.index_fetch_end = appendonly_index_fetch_end,
	.index_fetch_tuple = appendonly_index_fetch_tuple,

	.tuple_insert = appendonly_tuple_insert,
	.tuple_insert_speculative = appendonly_tuple_insert_speculative,
	.tuple_complete_speculative = appendonly_tuple_complete_speculative,
	.multi_insert = appendonly_multi_insert,
	.tuple_delete = appendonly_tuple_delete,
	.tuple_update = appendonly_tuple_update,
	.tuple_lock = appendonly_tuple_lock,
	.finish_bulk_insert = appendonly_finish_bulk_insert,

	.tuple_fetch_row_version = appendonly_fetch_row_version,
	.tuple_get_latest_tid = appendonly_get_latest_tid,
	.tuple_tid_valid = appendonly_tuple_tid_valid,
	.tuple_satisfies_snapshot = appendonly_tuple_satisfies_snapshot,
	.compute_xid_horizon_for_tuples = appendonly_compute_xid_horizon_for_tuples,

	.relation_set_new_filenode = appendonly_relation_set_new_filenode,
	.relation_nontransactional_truncate = appendonly_relation_nontransactional_truncate,
	.relation_copy_data = appendonly_relation_copy_data,
	.relation_copy_for_cluster = appendonly_relation_copy_for_cluster,
	.relation_vacuum = appendonly_vacuum_rel,
	.scan_analyze_next_block = appendonly_scan_analyze_next_block,
	.scan_analyze_next_tuple = appendonly_scan_analyze_next_tuple,
	.index_build_range_scan = appendonly_index_build_range_scan,
	.index_validate_scan = appendonly_index_validate_scan,

	.relation_size = appendonly_relation_size,
	.relation_needs_toast_table = appendonly_relation_needs_toast_table,

	.relation_estimate_size = appendonly_estimate_rel_size,

	.scan_bitmap_next_block = appendonly_scan_bitmap_next_block,
	.scan_bitmap_next_tuple = appendonly_scan_bitmap_next_tuple,
	.scan_sample_next_block = appendonly_scan_sample_next_block,
	.scan_sample_next_tuple = appendonly_scan_sample_next_tuple
};

Datum
ao_row_tableam_handler(PG_FUNCTION_ARGS)
{
	PG_RETURN_POINTER(&ao_row_methods);
}
