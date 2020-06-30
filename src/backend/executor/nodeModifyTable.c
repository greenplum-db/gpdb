/*-------------------------------------------------------------------------
 *
 * nodeModifyTable.c
 *	  routines to handle ModifyTable nodes.
 *
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeModifyTable.c
 *
 *-------------------------------------------------------------------------
 */
/* INTERFACE ROUTINES
 *		ExecInitModifyTable - initialize the ModifyTable node
 *		ExecModifyTable		- retrieve the next tuple from the node
 *		ExecEndModifyTable	- shut down the ModifyTable node
 *		ExecReScanModifyTable - rescan the ModifyTable node
 *
 *	 NOTES
 *		Each ModifyTable node contains a list of one or more subplans,
 *		much like an Append node.  There is one subplan per result relation.
 *		The key reason for this is that in an inherited UPDATE command, each
 *		result relation could have a different schema (more or different
 *		columns) requiring a different plan tree to produce it.  In an
 *		inherited DELETE, all the subplans should produce the same output
 *		rowtype, but we might still find that different plans are appropriate
 *		for different child relations.
 *
 *		If the query specifies RETURNING, then the ModifyTable returns a
 *		RETURNING tuple after completing each row insert, update, or delete.
 *		It must be called again to continue the operation.  Without RETURNING,
 *		we just loop within the node until all the work is done, then
 *		return NULL.  This avoids useless call/return overhead.
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "access/tupconvert.h"
#include "access/xact.h"
#include "commands/trigger.h"
#include "executor/executor.h"
#include "executor/nodeModifyTable.h"
#include "foreign/fdwapi.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/tqual.h"

#include "access/transam.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbpartition.h"
#include "cdb/cdbvars.h"
#include "executor/execDML.h"
#include "parser/parsetree.h"
#include "utils/lsyscache.h"
#include "utils/snapmgr.h"


static bool ExecOnConflictUpdate(ModifyTableState *mtstate,
					 ResultRelInfo *resultRelInfo,
					 ItemPointer conflictTid,
					 TupleTableSlot *planSlot,
					 TupleTableSlot *excludedSlot,
					 EState *estate,
					 bool canSetTag,
					 TupleTableSlot **returning);

static Oid
table_insert(ResultRelInfo *resultRelInfo, EState *estate, TupleTableSlot *slot,
			 Oid tuple_oid, ItemPointer lastTid, CommandId cid);
static HTSU_Result
table_update(ResultRelInfo *resultRelInfo, EState *estate, ItemPointer tupleid, TupleTableSlot *slot,
			 CommandId cid,
			 Snapshot crosscheck_snapshot,
			 bool wait,
			 HeapUpdateFailureData *hufd, LockTupleMode *lockmode, bool *wasHotUpdate);
static HTSU_Result
table_delete(ResultRelInfo *resultRelInfo, EState *estate, bool isUpdate, ItemPointer tupleid,
			 CommandId cid,
			 Snapshot crosscheck_snapshot,
			 bool wait,
			 HeapUpdateFailureData *hufd);

/*
 * Verify that the tuples to be produced by INSERT or UPDATE match the
 * target relation's rowtype
 *
 * We do this to guard against stale plans.  If plan invalidation is
 * functioning properly then we should never get a failure here, but better
 * safe than sorry.  Note that this is called after we have obtained lock
 * on the target rel, so the rowtype can't change underneath us.
 *
 * The plan output is represented by its targetlist, because that makes
 * handling the dropped-column case easier.
 */
static void
ExecCheckPlanOutput(Relation resultRel, List *targetList)
{
	TupleDesc	resultDesc = RelationGetDescr(resultRel);
	int			attno = 0;
	ListCell   *lc;

	foreach(lc, targetList)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);
		Form_pg_attribute attr;

		if (tle->resjunk)
			continue;			/* ignore junk tlist items */

		if (attno >= resultDesc->natts)
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("table row type and query-specified row type do not match"),
					 errdetail("Query has too many columns.")));
		attr = resultDesc->attrs[attno++];

		if (!attr->attisdropped)
		{
			/* Normal case: demand type match */
			if (exprType((Node *) tle->expr) != attr->atttypid)
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("table row type and query-specified row type do not match"),
						 errdetail("Table has type %s at ordinal position %d, but query expects %s.",
								   format_type_be(attr->atttypid),
								   attno,
							 format_type_be(exprType((Node *) tle->expr)))));
		}
		else
		{
			/*
			 * For a dropped column, we can't check atttypid (it's likely 0).
			 * In any case the planner has most likely inserted an INT4 null.
			 * What we insist on is just *some* NULL constant.
			 */
			/* GPDB_96_MERGE_FIXME: the subplan can be a Motion, so that the NULLs
			 * are transferred through the Motion node.
			 */
#if 0
			if (!IsA(tle->expr, Const) ||
				!((Const *) tle->expr)->constisnull)
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("table row type and query-specified row type do not match"),
						 errdetail("Query provides a value for a dropped column at ordinal position %d.",
								   attno)));
#endif
		}
	}
	if (attno != resultDesc->natts)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
		  errmsg("table row type and query-specified row type do not match"),
				 errdetail("Query has too few columns.")));
}

/*
 * ExecProcessReturning --- evaluate a RETURNING list
 *
 * projectReturning: RETURNING projection info for current result rel
 * tupleSlot: slot holding tuple actually inserted/updated/deleted
 * planSlot: slot holding tuple returned by top subplan node
 *
 * Note: If tupleSlot is NULL, the FDW should have already provided econtext's
 * scan tuple.
 *
 * Returns a slot holding the result tuple
 */
static TupleTableSlot *
ExecProcessReturning(ProjectionInfo *projectReturning,
					 TupleTableSlot *tupleSlot,
					 TupleTableSlot *planSlot)
{
	ExprContext *econtext = projectReturning->pi_exprContext;

	/*
	 * Reset per-tuple memory context to free any expression evaluation
	 * storage allocated in the previous cycle.
	 */
	ResetExprContext(econtext);

	/* Make tuple and any needed join variables available to ExecProject */
	if (tupleSlot)
		econtext->ecxt_scantuple = tupleSlot;
	else
	{
		HeapTuple	tuple;

		/*
		 * RETURNING expressions might reference the tableoid column, so
		 * initialize t_tableOid before evaluating them.
		 */
		Assert(!TupIsNull(econtext->ecxt_scantuple));
		tuple = ExecMaterializeSlot(econtext->ecxt_scantuple);
#if 0
		tuple->t_tableOid = RelationGetRelid(resultRelInfo->ri_RelationDesc);
#endif
	}
	econtext->ecxt_outertuple = planSlot;

	/* Compute the RETURNING expressions */
	return ExecProject(projectReturning, NULL);
}

/*
 * ExecCheckHeapTupleVisible -- verify heap tuple is visible
 *
 * It would not be consistent with guarantees of the higher isolation levels to
 * proceed with avoiding insertion (taking speculative insertion's alternative
 * path) on the basis of another tuple that is not visible to MVCC snapshot.
 * Check for the need to raise a serialization failure, and do so as necessary.
 */
static void
ExecCheckHeapTupleVisible(Relation rel,
						  EState *estate,
						  HeapTuple tuple,
						  Buffer buffer)
{
	if (!IsolationUsesXactSnapshot())
		return;

	if (!HeapTupleSatisfiesVisibility(rel, tuple, estate->es_snapshot, buffer))
		ereport(ERROR,
				(errcode(ERRCODE_T_R_SERIALIZATION_FAILURE),
			 errmsg("could not serialize access due to concurrent update")));
}

/*
 * ExecCheckTIDVisible -- convenience variant of ExecCheckHeapTupleVisible()
 */
static void
ExecCheckTIDVisible(EState *estate,
					ResultRelInfo *relinfo,
					ItemPointer tid)
{
	Relation	rel = relinfo->ri_RelationDesc;
	Buffer		buffer;
	HeapTupleData tuple;

	/* Redundantly check isolation level */
	if (!IsolationUsesXactSnapshot())
		return;

	tuple.t_self = *tid;
	if (!heap_fetch(rel, SnapshotAny, &tuple, &buffer, false, NULL))
		elog(ERROR, "failed to fetch conflicting tuple for ON CONFLICT");
	ExecCheckHeapTupleVisible(rel, estate, &tuple, buffer);
	ReleaseBuffer(buffer);
}

/* ----------------------------------------------------------------
 *		ExecInsert
 *
 *		For INSERT, we have to insert the tuple into the target relation
 *		and insert appropriate tuples into the index relations.
 *
 *		Returns RETURNING result if any, otherwise NULL.
 *
 * If the target table is partitioned, the input tuple in 'parentslot'
 * is in the shape required for the parent table. This function will
 * look up the ResultRelInfo of the target partition, and form a
 * tuple suitable for the target partition. (It can be different, if
 * there are dropped columns in the parent, but not the partition,
 * for example.)
 *
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecInsert(ModifyTableState *mtstate,
		   TupleTableSlot *parentslot,
		   TupleTableSlot *planSlot,
		   List *arbiterIndexes,
		   OnConflictAction onconflict,
		   EState *estate,
		   bool canSetTag,
		   bool isUpdate,
		   Oid	tupleOid)
{
	ResultRelInfo *resultRelInfo;
	Relation	resultRelationDesc;
	Oid			newId;
	List	   *recheckIndexes = NIL;

	ItemPointerData lastTid;
	Oid			tuple_oid = tupleOid;
	ProjectionInfo *projectReturning;
	TupleTableSlot *slot;

	/*
	 * RETURNING is only present in the parent ResultRelInfo, so stash
	 * that before looking up the ResultRelInfo of the target partition.
	 */
	projectReturning = estate->es_result_relation_info->ri_projectReturning;

	/*
	 * get information on the (current) result relation
	 */
	if (estate->es_result_partitions)
	{
		resultRelInfo = slot_get_partition(parentslot, estate, true);

		/*
		 * Check whether the user provided the correct leaf part only if required.
		 * For isUpdate, the check for resultRelInfo equals to target partitioned relation id
		 * has been done by checkPartitionUpdate before. So we don't need to check again here.
		 */
		if (!dml_ignore_target_partition_check && !isUpdate)
		{
			Assert(NULL != estate->es_result_partitions->part &&
					NULL != resultRelInfo->ri_RelationDesc);

			List *resultRelations = estate->es_plannedstmt->resultRelations;
			/*
			 * Only inheritance can generate multiple result relations and inheritance
			 * is not compatible with partitions. As we are in inserting in partitioned
			 * table, we should not have more than one resultRelation
			 */
			Assert(list_length(resultRelations) == 1);
			/* We only have one resultRelations entry where the user originally intended to insert */
			int rteIdxForUserRel = linitial_int(resultRelations);
			Assert (rteIdxForUserRel > 0);
			Oid userProvidedRel = InvalidOid;

			if (1 == rteIdxForUserRel)
			{
				/* Optimization for typical case */
				userProvidedRel = ((RangeTblEntry *) estate->es_plannedstmt->rtable->head->data.ptr_value)->relid;
			}
			else
			{
				userProvidedRel = getrelid(rteIdxForUserRel, estate->es_plannedstmt->rtable);
			}

			/* Error out if user provides a leaf partition that does not match with our calculated partition */
			if (userProvidedRel != estate->es_result_partitions->part->parrelid &&
				userProvidedRel != resultRelInfo->ri_RelationDesc->rd_id)
			{
				ereport(ERROR,
						(errcode(ERRCODE_CHECK_VIOLATION),
						 errmsg("trying to insert row into wrong partition"),
						 errdetail("Expected partition: %s, provided partition: %s.",
							resultRelInfo->ri_RelationDesc->rd_rel->relname.data,
							estate->es_result_relation_info->ri_RelationDesc->rd_rel->relname.data)));
			}
		}
		estate->es_result_relation_info = resultRelInfo;
	}
	else
	{
		resultRelInfo = estate->es_result_relation_info;
	}

	resultRelationDesc = resultRelInfo->ri_RelationDesc;

	/*
	 * If the result relation has OIDs, force the tuple's OID to zero so that
	 * heap_insert will assign a fresh OID.  Usually the OID already will be
	 * zero at this point, but there are corner cases where the plan tree can
	 * return a tuple extracted literally from some table with the same
	 * rowtype.
	 *
	 * XXX if we ever wanted to allow users to assign their own OIDs to new
	 * rows, this'd be the place to do it.  For the moment, we make a point of
	 * doing this before calling triggers, so that a user-supplied trigger
	 * could hack the OID if desired.
	 *
	 * GPDB: In PostgreSQL, here we set the Oid in the HeapTuple, which is a
	 * local copy at this point. But in GPDB, we don't materialize the tuple
	 * yet, because we might need a MemTuple or a HeapTuple depending on
	 * what kind of a table this is (or neither for an AOCS table, since
	 * aocs_insert() works directly off the slot). So we keep the Oid in a
	 * local variable for now, and only set it in the tuple just before the
	 * call to heap/appendonly_insert().
	 */
	if (resultRelationDesc->rd_rel->relhasoids)
	{
		/*
		 * But if this is really an UPDATE, try to preserve the old OID.
		 */
		if (isUpdate && tuple_oid == InvalidOid)
		{
			GenericTuple gtuple;

			gtuple = ExecFetchSlotGenericTuple(parentslot);

			if (!is_memtuple(gtuple))
				tuple_oid = HeapTupleGetOid((HeapTuple) gtuple);
			else
				tuple_oid = MemTupleGetOidDirect((MemTuple) gtuple);
		}
	}

	slot = reconstructPartitionTupleSlot(parentslot, resultRelInfo);

	if (resultRelationDesc->rd_rel->relkind == RELKIND_FOREIGN_TABLE &&
		estate->es_result_partitions &&
		estate->es_result_partitions->part->parrelid != 0)
	{
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("insert into foreign partitions not supported")));
	}

	Assert(slot != NULL);

	/*
	 * BEFORE ROW INSERT Triggers.
	 *
	 * Note: We fire BEFORE ROW TRIGGERS for every attempted insertion in an
	 * INSERT ... ON CONFLICT statement.  We cannot check for constraint
	 * violations before firing these triggers, because they can change the
	 * values to insert.  Also, they can run arbitrary user-defined code with
	 * side-effects that we can't cancel by just not inserting the tuple.
	 */
	if (resultRelInfo->ri_TrigDesc &&
		resultRelInfo->ri_TrigDesc->trig_insert_before_row &&
		!isUpdate)
	{
		slot = ExecBRInsertTriggers(estate, resultRelInfo, slot);

		if (slot == NULL)		/* "do nothing" */
			return NULL;

		/* trigger might have changed tuple */
		if (resultRelationDesc->rd_rel->relhasoids)
		{
			if (TupHasHeapTuple(slot))
			{
				HeapTuple trigtup = TupGetHeapTuple(slot);

				tuple_oid = HeapTupleGetOid(trigtup);
			}
		}
	}

	/* INSTEAD OF ROW INSERT Triggers */
	if (resultRelInfo->ri_TrigDesc &&
		resultRelInfo->ri_TrigDesc->trig_insert_instead_row)
	{
		slot = ExecIRInsertTriggers(estate, resultRelInfo, slot);

		if (slot == NULL)		/* "do nothing" */
			return NULL;

		newId = InvalidOid;
	}
	else if (resultRelInfo->ri_FdwRoutine)
	{
		/*
		 * insert into foreign table: let the FDW do it
		 */
		slot = resultRelInfo->ri_FdwRoutine->ExecForeignInsert(estate,
															   resultRelInfo,
															   slot,
															   planSlot);

		if (slot == NULL)		/* "do nothing" */
			return NULL;

#if 0
		/* FDW might have changed tuple */
		/*
		 * GPDB: Greenplum does not allow triggers/contraints to reference
		 * system columns which makes the t_tableOid initialization reduntant.
		 */
		tuple = ExecMaterializeSlot(slot);

		/*
		 * AFTER ROW Triggers or RETURNING expressions might reference the
		 * tableoid column, so initialize t_tableOid before evaluating them.
		 */
		tuple->t_tableOid = RelationGetRelid(resultRelationDesc);
#endif

		newId = InvalidOid;
	}
	else
	{
		/*
		 * Constraints might reference the tableoid column, so initialize
		 * t_tableOid before evaluating them.
		 */
#if 0
		/*
		 * GPDB: Greenplum does not allow triggers/contraints to reference
		 * system columns which makes the t_tableOid initialization reduntant.
		 */
		tuple->t_tableOid = RelationGetRelid(resultRelationDesc);
#endif

		/*
		 * Check any RLS INSERT WITH CHECK policies
		 *
		 * ExecWithCheckOptions() will skip any WCOs which are not of the kind
		 * we are looking for at this point.
		 */
		if (resultRelInfo->ri_WithCheckOptions != NIL)
			ExecWithCheckOptions(isUpdate ? WCO_RLS_UPDATE_CHECK : WCO_RLS_INSERT_CHECK,
								 resultRelInfo, slot, estate);

		/*
		 * Check the constraints of the tuple
		 */
		if (resultRelationDesc->rd_att->constr)
			ExecConstraints(resultRelInfo, slot, estate);

		/*
		 * GPDB_95_MERGE_FIXME: enable speculative insertion in heaptable only.
		 */
		if (onconflict != ONCONFLICT_NONE && resultRelInfo->ri_NumIndices > 0 &&
			RelationIsHeap(resultRelationDesc))
		{
			/* Perform a speculative insertion. */
			HeapTuple tuple;
			uint32		specToken;
			ItemPointerData conflictTid;
			bool		specConflict;

			/*
			 * Do a non-conclusive check for conflicts first.
			 *
			 * We're not holding any locks yet, so this doesn't guarantee that
			 * the later insert won't conflict.  But it avoids leaving behind
			 * a lot of canceled speculative insertions, if you run a lot of
			 * INSERT ON CONFLICT statements that do conflict.
			 *
			 * We loop back here if we find a conflict below, either during
			 * the pre-check, or when we re-check after inserting the tuple
			 * speculatively.
			 */
	vlock:
			specConflict = false;
			if (!ExecCheckIndexConstraints(slot, estate, &conflictTid,
										   arbiterIndexes))
			{
				/* committed conflict tuple found */
				if (onconflict == ONCONFLICT_UPDATE)
				{
					/*
					 * In case of ON CONFLICT DO UPDATE, execute the UPDATE
					 * part.  Be prepared to retry if the UPDATE fails because
					 * of another concurrent UPDATE/DELETE to the conflict
					 * tuple.
					 */
					TupleTableSlot *returning = NULL;

					if (ExecOnConflictUpdate(mtstate, resultRelInfo,
											 &conflictTid, planSlot, slot,
											 estate, canSetTag, &returning))
					{
						InstrCountFiltered2(&mtstate->ps, 1);
						return returning;
					}
					else
						goto vlock;
				}
				else
				{
					/*
					 * In case of ON CONFLICT DO NOTHING, do nothing. However,
					 * verify that the tuple is visible to the executor's MVCC
					 * snapshot at higher isolation levels.
					 */
					Assert(onconflict == ONCONFLICT_NOTHING);
					ExecCheckTIDVisible(estate, resultRelInfo, &conflictTid);
					InstrCountFiltered2(&mtstate->ps, 1);
					return NULL;
				}
			}

			tuple = ExecMaterializeSlot(slot);
			if (resultRelationDesc->rd_rel->relhasoids)
				HeapTupleSetOid(tuple, tuple_oid);

			/*
			 * Before we start insertion proper, acquire our "speculative
			 * insertion lock".  Others can use that to wait for us to decide
			 * if we're going to go ahead with the insertion, instead of
			 * waiting for the whole transaction to complete.
			 */
			specToken = SpeculativeInsertionLockAcquire(GetCurrentTransactionId());
			HeapTupleHeaderSetSpeculativeToken(tuple->t_data, specToken);

			/* insert the tuple, with the speculative token */
			newId = heap_insert(resultRelationDesc,
								tuple,
								estate->es_output_cid, 0, NULL,
								GetCurrentTransactionId());
			lastTid = tuple->t_self;

			/* insert index entries for tuple */
			recheckIndexes = ExecInsertIndexTuples(slot, &lastTid,
												   estate, true, &specConflict,
												   arbiterIndexes);

			/* adjust the tuple's state accordingly */
			if (!specConflict)
				heap_finish_speculative(resultRelationDesc, tuple);
			else
				heap_abort_speculative(resultRelationDesc, tuple);

			/*
			 * Wake up anyone waiting for our decision.  They will re-check
			 * the tuple, see that it's no longer speculative, and wait on our
			 * XID as if this was a regularly inserted tuple all along.  Or if
			 * we killed the tuple, they will see it's dead, and proceed as if
			 * the tuple never existed.
			 */
			SpeculativeInsertionLockRelease(GetCurrentTransactionId());

			/*
			 * If there was a conflict, start from the beginning.  We'll do
			 * the pre-check again, which will now find the conflicting tuple
			 * (unless it aborts before we get there).
			 */
			if (specConflict)
			{
				list_free(recheckIndexes);
				goto vlock;
			}

			/* Since there was no insertion conflict, we're done */
		}
		else
		{
			/*
			 * insert the tuple normally.
			 */
			newId = table_insert(resultRelInfo, estate, slot, tuple_oid, &lastTid,
								 estate->es_output_cid);

			/* insert index entries for tuple */
			if (resultRelInfo->ri_NumIndices > 0)
				recheckIndexes = ExecInsertIndexTuples(slot, &lastTid,
													   estate, false, NULL,
													   arbiterIndexes);
		}
	}
	if (canSetTag)
	{
		(estate->es_processed)++;
		estate->es_lastoid = newId;
		setLastTid(&lastTid);
	}

	slot->tts_tableOid = RelationGetRelid(resultRelationDesc);

	/* AFTER ROW INSERT Triggers */
	if (resultRelInfo->ri_TrigDesc &&
		resultRelInfo->ri_TrigDesc->trig_insert_after_row &&
		!isUpdate)
	{
		HeapTuple tuple = ExecMaterializeSlot(slot);
		ExecARInsertTriggers(estate, resultRelInfo, tuple, recheckIndexes);
	}

	list_free(recheckIndexes);

	/*
	 * Check any WITH CHECK OPTION constraints from parent views.  We are
	 * required to do this after testing all constraints and uniqueness
	 * violations per the SQL spec, so we do it after actually inserting the
	 * record into the heap and all indexes.
	 *
	 * ExecWithCheckOptions will elog(ERROR) if a violation is found, so the
	 * tuple will never be seen, if it violates the WITH CHECK OPTION.
	 *
	 * ExecWithCheckOptions() will skip any WCOs which are not of the kind we
	 * are looking for at this point.
	 */
	if (resultRelInfo->ri_WithCheckOptions != NIL)
		ExecWithCheckOptions(WCO_VIEW_CHECK, resultRelInfo, slot, estate);

	/* Process RETURNING if present */
	if (projectReturning)
		return ExecProcessReturning(projectReturning, parentslot, planSlot);

	return NULL;
}

/* ----------------------------------------------------------------
 *		ExecDelete
 *
 *		DELETE is like UPDATE, except that we delete the tuple and no
 *		index modifications are needed.
 *
 *		When deleting from a table, tupleid identifies the tuple to
 *		delete and oldtuple is NULL.  When deleting from a view,
 *		oldtuple is passed to the INSTEAD OF triggers and identifies
 *		what to delete, and tupleid is invalid.  When deleting from a
 *		foreign table, tupleid is invalid; the FDW has to figure out
 *		which row to delete using data from the planSlot.  oldtuple is
 *		passed to foreign table triggers; it is NULL when the foreign
 *		table has no relevant triggers.
 *
 *		In GPDB, DELETE can be part of an update operation when
 *		there is a preceding SplitUpdate node.
 *
 *		Returns RETURNING result if any, otherwise NULL.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecDelete(ItemPointer tupleid,
		   int32 segid,
		   HeapTuple oldtuple,
		   TupleTableSlot *planSlot,
		   EPQState *epqstate,
		   EState *estate,
		   bool canSetTag,
		   bool isUpdate)
{
	PlanGenerator planGen = estate->es_plannedstmt->planGen;
	ResultRelInfo *resultRelInfo;
	Relation	resultRelationDesc;
	HTSU_Result result;
	HeapUpdateFailureData hufd;
	TupleTableSlot *slot = NULL;

	/*
	 * Sanity check the distribution of the tuple to prevent
	 * potential data corruption in case users manipulate data
	 * incorrectly (e.g. insert data on incorrect segment through
	 * utility mode) or there is bug in code, etc.
	 */
	if (segid != GpIdentity.segindex)
		elog(ERROR, "distribution key of the tuple doesn't belong to "
			 "current segment (actually from seg%d)", segid);

	/*
	 * get information on the (current) result relation
	 */
	if (estate->es_result_partitions && planGen == PLANGEN_OPTIMIZER)
	{
		Assert(estate->es_result_partitions->part->parrelid);

#ifdef USE_ASSERT_CHECKING
		Oid parent = estate->es_result_partitions->part->parrelid;
#endif

		/* Obtain part for current tuple. */
		resultRelInfo = slot_get_partition(planSlot, estate, true);
		estate->es_result_relation_info = resultRelInfo;

#ifdef USE_ASSERT_CHECKING
		Oid part = RelationGetRelid(resultRelInfo->ri_RelationDesc);
#endif

		Assert(parent != part);
	}
	else
	{
		resultRelInfo = estate->es_result_relation_info;
	}
	resultRelationDesc = resultRelInfo->ri_RelationDesc;

	/* BEFORE ROW DELETE Triggers */
	if (resultRelInfo->ri_TrigDesc &&
		resultRelInfo->ri_TrigDesc->trig_delete_before_row &&
		!isUpdate)
	{
		bool		dodelete;

		dodelete = ExecBRDeleteTriggers(estate, epqstate, resultRelInfo,
										tupleid, oldtuple);

		if (!dodelete)			/* "do nothing" */
			return NULL;
	}

	if (resultRelationDesc->rd_rel->relkind == RELKIND_FOREIGN_TABLE &&
		estate->es_result_partitions &&
		estate->es_result_partitions->part->parrelid != 0)
	{
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("delete from foreign partitions not supported")));
	}

	/* INSTEAD OF ROW DELETE Triggers */
	if (resultRelInfo->ri_TrigDesc &&
		resultRelInfo->ri_TrigDesc->trig_delete_instead_row)
	{
		bool		dodelete;

		Assert(oldtuple != NULL);

		dodelete = ExecIRDeleteTriggers(estate, resultRelInfo, oldtuple);

		if (!dodelete)			/* "do nothing" */
			return NULL;
	}
	else if (resultRelInfo->ri_FdwRoutine)
	{
#if 0
		/* See comment regarding t_tableOid bellow */
		HeapTuple	tuple;
#endif

		/*
		 * delete from foreign table: let the FDW do it
		 *
		 * We offer the trigger tuple slot as a place to store RETURNING data,
		 * although the FDW can return some other slot if it wants.  Set up
		 * the slot's tupdesc so the FDW doesn't need to do that for itself.
		 */
		slot = estate->es_trig_tuple_slot;
		if (slot->tts_tupleDescriptor != RelationGetDescr(resultRelationDesc))
			ExecSetSlotDescriptor(slot, RelationGetDescr(resultRelationDesc));

		slot = resultRelInfo->ri_FdwRoutine->ExecForeignDelete(estate,
															   resultRelInfo,
															   slot,
															   planSlot);

		if (slot == NULL)		/* "do nothing" */
			return NULL;

		/*
		 * RETURNING expressions might reference the tableoid column, so
		 * initialize t_tableOid before evaluating them.
		 */
		if (slot->PRIVATE_tts_flags & TTS_ISEMPTY)
			ExecStoreAllNullTuple(slot);

#if 0
		/*
		 * GPDB: Greenplum does not allow triggers/contraints to reference
		 * system columns which makes the t_tableOid initialization reduntant.
		 */
		tuple = ExecMaterializeSlot(slot);
		tuple->t_tableOid = RelationGetRelid(resultRelationDesc);
#endif
	}
	else
	{
		/*
		 * delete the tuple
		 *
		 * Note: if es_crosscheck_snapshot isn't InvalidSnapshot, we check
		 * that the row to be deleted is visible to that snapshot, and throw a
		 * can't-serialize error if not. This is a special-case behavior
		 * needed for referential integrity updates in transaction-snapshot
		 * mode transactions.
		 */
ldelete:;
		result = table_delete(resultRelInfo, estate, isUpdate, tupleid,
							  estate->es_output_cid,
							  estate->es_crosscheck_snapshot,
							  true /* wait for commit */ ,
							  &hufd);
		switch (result)
		{
			case HeapTupleSelfUpdated:

				/*
				 * The target tuple was already updated or deleted by the
				 * current command, or by a later command in the current
				 * transaction.  The former case is possible in a join DELETE
				 * where multiple tuples join to the same target tuple. This
				 * is somewhat questionable, but Postgres has always allowed
				 * it: we just ignore additional deletion attempts.
				 *
				 * The latter case arises if the tuple is modified by a
				 * command in a BEFORE trigger, or perhaps by a command in a
				 * volatile function used in the query.  In such situations we
				 * should not ignore the deletion, but it is equally unsafe to
				 * proceed.  We don't want to discard the original DELETE
				 * while keeping the triggered actions based on its deletion;
				 * and it would be no better to allow the original DELETE
				 * while discarding updates that it triggered.  The row update
				 * carries some information that might be important according
				 * to business rules; so throwing an error is the only safe
				 * course.
				 *
				 * If a trigger actually intends this type of interaction, it
				 * can re-execute the DELETE and then return NULL to cancel
				 * the outer delete.
				 *
				 * In GPDB, for AO tables HeapTupleSelfUpdated is returned only
				 * in case of same command tuple update based on visimap dirty
				 * list checking. Also, hufd is not initialized and can't be for
				 * AO case, as visimap update within same command happens at end
				 * of command.
				 */
				if (!RelationIsAppendOptimized(resultRelationDesc) && hufd.cmax != estate->es_output_cid)
					ereport(ERROR,
							(errcode(ERRCODE_TRIGGERED_DATA_CHANGE_VIOLATION),
							 errmsg("tuple to be updated was already modified by an operation triggered by the current command"),
							 errhint("Consider using an AFTER trigger instead of a BEFORE trigger to propagate changes to other rows.")));

				/* Else, already deleted by self; nothing to do */

				/*-------
				 * In an scenario in which R(a,b) and S(a,b) have
				 *        R               S
				 *    ________         ________
				 *     (1, 1)           (1, 2)
				 *                      (1, 7)
				 *
				 *  An update query such as:
				 *   UPDATE R SET a = S.b  FROM S WHERE R.b = S.a;
				 *
				 *  will have an non-deterministic output. The tuple in R
				 * can be updated to (2,1) or (7,1).
				 * Since the introduction of SplitUpdate, these queries will
				 * send multiple requests to delete the same tuple. Therefore,
				 * in order to avoid a non-deterministic output,
				 * an error is reported in such scenario.
				 *
				 * ORCA requires this check as it does not support multiple
				 * updates to a row by the same query
				 *-------
				*/
				if (isUpdate)
				{
					ereport(ERROR,
							(errcode(ERRCODE_IN_FAILED_SQL_TRANSACTION ),
							 errmsg("multiple updates to a row by the same query is not allowed")));
				}
				return NULL;

			case HeapTupleMayBeUpdated:
				break;

			case HeapTupleUpdated:
				if (IsolationUsesXactSnapshot())
					ereport(ERROR,
							(errcode(ERRCODE_T_R_SERIALIZATION_FAILURE),
							 errmsg("could not serialize access due to concurrent update")));

				/*
				 * If the DELETE operator is generated by
				 * SplitUpdate node (SplitUpdate node is generated for updating
				 * the distributed keys), we raise an error.
				 *
				 * The root cause is SplitUpdate will split the origin tuple
				 * to two tuples, one is for deleting and the other one
				 * is for inserting. we can skip the deleting tuple if we
				 * found the tuple is updated concurrently by another
				 * transaction, but it is difficult to skip the inserting
				 * tuple, so it will leads to more tuples after updating.
				 */
				if (isUpdate)
				{
					ereport(ERROR,
							(errcode(ERRCODE_T_R_SERIALIZATION_FAILURE ),
							 errmsg("concurrent updates distribution keys on the same row is not allowed")));
				}

				if (!ItemPointerEquals(tupleid, &hufd.ctid))
				{
					TupleTableSlot *epqslot;

					epqslot = EvalPlanQual(estate,
										   epqstate,
										   resultRelationDesc,
										   resultRelInfo->ri_RangeTableIndex,
										   LockTupleExclusive,
										   &hufd.ctid,
										   hufd.xmax);
					if (!TupIsNull(epqslot))
					{
						*tupleid = hufd.ctid;
						goto ldelete;
					}
				}
				/* tuple already deleted; nothing to do */
				return NULL;

			default:
				elog(ERROR, "unrecognized heap_delete status: %u", result);
				return NULL;
		}

		/*
		 * Note: Normally one would think that we have to delete index tuples
		 * associated with the heap tuple now...
		 *
		 * ... but in POSTGRES, we have no need to do this because VACUUM will
		 * take care of it later.  We can't delete index tuples immediately
		 * anyway, since the tuple is still visible to other transactions.
		 */
	}

	if (canSetTag)
		(estate->es_processed)++;

	/*
	 * Note: Normally one would think that we have to delete index tuples
	 * associated with the heap tuple now...
	 *
	 * ... but in POSTGRES, we have no need to do this because VACUUM will
	 * take care of it later.  We can't delete index tuples immediately
	 * anyway, since the tuple is still visible to other transactions.
	 */

	if (!RelationIsAppendOptimized(resultRelationDesc) && !isUpdate)
	{
		/* AFTER ROW DELETE Triggers */
		ExecARDeleteTriggers(estate, resultRelInfo, tupleid, oldtuple);
	}

	/* Process RETURNING if present */
	/*
	 * In a split update, the processed rows are returned by the INSERT
	 * of the new row, not the DELETE of the old one.
	 */
	if (resultRelInfo->ri_projectReturning && !isUpdate)
	{
		/*
		 * We have to put the target tuple into a slot, which means first we
		 * gotta fetch it.  We can use the trigger tuple slot.
		 */
		TupleTableSlot *rslot;
		HeapTupleData deltuple;
		Buffer		delbuffer;

		if (resultRelInfo->ri_FdwRoutine)
		{
			/* FDW must have provided a slot containing the deleted row */
			Assert(!TupIsNull(slot));
			delbuffer = InvalidBuffer;
		}
		else
		{
			/*
			 * If it's an AO table, we cannot fetch the old tuple. Punt.
			 *
			 * We could use the same AO fetcher mechanism that we use for
			 * (bitmap) index scans on AO tables, but that only works if
			 * the AO table has a block directory, which it might not have,
			 * if there are no indexes on it.
			 */
			if (!RelationIsHeap(resultRelationDesc))
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("DELETE RETURNING is not supported on appendonly tables")));

			slot = estate->es_trig_tuple_slot;
			if (oldtuple != NULL)
			{
				deltuple = *oldtuple;
				delbuffer = InvalidBuffer;
			}
			else
			{
				deltuple.t_self = *tupleid;
				if (!heap_fetch(resultRelationDesc, SnapshotAny,
								&deltuple, &delbuffer, false, NULL))
					elog(ERROR, "failed to fetch deleted tuple for DELETE RETURNING");
			}

			if (slot->tts_tupleDescriptor != RelationGetDescr(resultRelationDesc))
				ExecSetSlotDescriptor(slot, RelationGetDescr(resultRelationDesc));
			ExecStoreHeapTuple(&deltuple, slot, InvalidBuffer, false);
		}

		rslot = ExecProcessReturning(resultRelInfo->ri_projectReturning, slot, planSlot);

		/*
		 * Before releasing the target tuple again, make sure rslot has a
		 * local copy of any pass-by-reference values.
		 */
		ExecMaterializeSlot(rslot);

		ExecClearTuple(slot);
		if (BufferIsValid(delbuffer))
			ReleaseBuffer(delbuffer);

		return rslot;
	}

	return NULL;
}

/*
 * Check if the tuple being updated will stay in the same part and throw ERROR
 * if not.  This check is especially necessary for default partition that has
 * no constraint on it.  partslot is the tuple being updated, and
 * resultRelInfo is the target relation of this update.  Call this only
 * estate has valid es_result_partitions.
 */
static void
checkPartitionUpdate(EState *estate, TupleTableSlot *partslot,
					 ResultRelInfo *resultRelInfo)
{
	Relation	resultRelationDesc = resultRelInfo->ri_RelationDesc;
	AttrNumber	max_attr;
	TupleTableSlot *parentslot = NULL;
	Datum	   *values;
	bool	   *nulls;
	TupleDesc	tupdesc;
	Oid			targetid;

	Assert(estate->es_partition_state != NULL &&
		   estate->es_partition_state->accessMethods != NULL);
	if (!estate->es_partition_state->accessMethods->part_cxt)
		estate->es_partition_state->accessMethods->part_cxt =
			GetPerTupleExprContext(estate)->ecxt_per_tuple_memory;

	Assert(PointerIsValid(estate->es_result_partitions));

	/*
	 * As opposed to INSERT, resultRelation here is the same child part
	 * as scan origin.  However, the partition selection is done with the
	 * parent partition's attribute numbers, so if this result (child) part
	 * has physically-different attribute numbers due to dropped columns,
	 * we should map the child attribute numbers to the parent's attribute
	 * numbers to perform the partition selection.
	 *
	 * To determine whether we need to remap, or if the child partition
	 * has compatible physical shape with the parent, we compare the
	 * tuple descriptors.  If we find we need to map attribute numbers,
	 * max_partition_attr could also be bogus for this child part,
	 * so we end up materializing the whole columns using slot_getallattrs().
	 * The purpose of this code is just to prevent the tuple from
	 * incorrectly staying in default partition that has no constraint
	 * (parts with constraint will throw an error if the tuple is changing
	 * partition keys to out of part value anyway.)
	 */

	/*
	 * Check this at the first pass only to avoid repeated tupledesc
	 * comparisons.
	 */
	if (resultRelInfo->ri_PartCheckTupDescMatch == 0)
	{
		TupleDesc	resultTupdesc, parentTupdesc;
		Oid			parentRelid;
		Relation	parentRel;

		/*
		 * We are on a child part, let's see the tuple descriptor looks like
		 * the parent's one.  Probably this won't cause deadlock because
		 * DML should have opened the parent table with appropriate lock.
		 */

		parentRelid = estate->es_result_partitions->part->parrelid;
		parentRel = relation_open(parentRelid, AccessShareLock);
		resultTupdesc = RelationGetDescr(resultRelationDesc);
		parentTupdesc = RelationGetDescr(parentRel);
		if (!equalTupleDescs(resultTupdesc, parentTupdesc, false))
		{
			TupleConversionMap *map;
			MemoryContext	oldcontext;

			/* Tuple looks different.  Construct attribute mapping. */
			oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);
			map_part_attrs(resultRelationDesc, parentRel, &map, true);
			MemoryContextSwitchTo(oldcontext);

			/* And save it for later use. */
			resultRelInfo->ri_PartCheckMap = map;
			resultRelInfo->ri_PartitionParentSlot = MakeSingleTupleTableSlot(parentTupdesc);
			resultRelInfo->ri_PartCheckTupDescMatch = -1;
		}
		else
			resultRelInfo->ri_PartCheckTupDescMatch = 1;

		relation_close(parentRel, NoLock);
	}

	if (resultRelInfo->ri_PartCheckMap != NULL)
	{
		/*
		 * We need to map the attribute numbers to parent's one, to
		 * select the would-be destination relation, since all partition
		 * rules are based on the parent relation's tuple descriptor.
		 * max_partition_attr can be bogus as well, so don't use it.
		 */
		parentslot = execute_attr_map_slot(resultRelInfo->ri_PartCheckMap->attrMap,
										   partslot,
										   resultRelInfo->ri_PartitionParentSlot);

		/* Now we have values/nulls in parent's view. */
		slot_getallattrs(parentslot);
	}
	else
	{
		/*
		 * map == NULL means we can just fetch values/nulls from the
		 * current slot.
		 */
		parentslot = partslot;
		max_attr = estate->es_partition_state->max_partition_attr;
		slot_getsomeattrs(partslot, max_attr);
	}

	/* And select the destination relation that this tuple would go to. */
	values = slot_get_values(parentslot);
	nulls = slot_get_isnull(parentslot);
	tupdesc = parentslot->tts_tupleDescriptor;
	targetid = selectPartition(estate->es_result_partitions, values,
							   nulls, tupdesc,
							   estate->es_partition_state->accessMethods);

	if (!OidIsValid(targetid))
		ereport(ERROR,
				(errcode(ERRCODE_NO_PARTITION_FOR_PARTITIONING_KEY),
				 errmsg("no partition for partitioning key")));

	if (RelationGetRelid(resultRelationDesc) != targetid)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("moving tuple from partition \"%s\" to partition \"%s\" not supported",
						get_rel_name(RelationGetRelid(resultRelationDesc)),
						get_rel_name(targetid))));
}

/* ----------------------------------------------------------------
 *		ExecUpdate
 *
 *		note: we can't run UPDATE queries with transactions
 *		off because UPDATEs are actually INSERTs and our
 *		scan will mistakenly loop forever, updating the tuple
 *		it just inserted..  This should be fixed but until it
 *		is, we don't want to get stuck in an infinite loop
 *		which corrupts your database..
 *
 *		When updating a table, tupleid identifies the tuple to
 *		update and oldtuple is NULL.  When updating a view, oldtuple
 *		is passed to the INSTEAD OF triggers and identifies what to
 *		update, and tupleid is invalid.  When updating a foreign table,
 *		tupleid is invalid; the FDW has to figure out which row to
 *		update using data from the planSlot.  oldtuple is passed to
 *		foreign table triggers; it is NULL when the foreign table has
 *		no relevant triggers.
 *
 *		Returns RETURNING result if any, otherwise NULL.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecUpdate(ItemPointer tupleid,
		   HeapTuple oldtuple,
		   TupleTableSlot *slot,
		   TupleTableSlot *planSlot,
		   int32 segid, /* gpdb specific parameter, check if tuple to update is from local */
		   EPQState *epqstate,
		   EState *estate,
		   bool canSetTag)
{
	ResultRelInfo *resultRelInfo;
	Relation	resultRelationDesc;
	HTSU_Result result;
	HeapUpdateFailureData hufd;
	List	   *recheckIndexes = NIL;
	ItemPointerData lastTid;
	bool		wasHotUpdate;

	/*
	 * abort the operation if not running transactions
	 */
	if (IsBootstrapProcessingMode())
		elog(ERROR, "cannot UPDATE during bootstrap");

	/*
	 * Sanity check the distribution of the tuple to prevent
	 * potential data corruption in case users manipulate data
	 * incorrectly (e.g. insert data on incorrect segment through
	 * utility mode) or there is bug in code, etc.
	 */
	if (segid != GpIdentity.segindex)
		elog(ERROR, "distribution key of the tuple doesn't belong to "
			 "current segment (actually from seg%d)", segid);

	/*
	 * get information on the (current) result relation
	 */
	resultRelInfo = estate->es_result_relation_info;
	resultRelationDesc = resultRelInfo->ri_RelationDesc;

	if (resultRelationDesc->rd_rel->relkind == RELKIND_FOREIGN_TABLE &&
		estate->es_result_partitions &&
		estate->es_result_partitions->part->parrelid != 0)
	{
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("updating foreign partitions not supported")));
	}

	/*
	 * get the heap tuple out of the tuple table slot, making sure we have a
	 * writable copy
	 */

	if (RelationIsAppendOptimized(resultRelationDesc))
	{
		/*
		 * It is necessary to reconstruct a logically compatible tuple to
		 * a physically compatible tuple.  The slot's tuple descriptor comes
		 * from the projection target list, which doesn't indicate dropped
		 * columns, and MemTuple cannot deal with cases without converting
		 * the target list back into the original relation's tuple desc.
		 */
		slot = reconstructPartitionTupleSlot(slot, resultRelInfo);
	}

	/* see if this update would move the tuple to a different partition */
	if (estate->es_result_partitions)
		checkPartitionUpdate(estate, slot, resultRelInfo);

	/* BEFORE ROW UPDATE Triggers */
	if (resultRelInfo->ri_TrigDesc &&
		resultRelInfo->ri_TrigDesc->trig_update_before_row)
	{
		slot = ExecBRUpdateTriggers(estate, epqstate, resultRelInfo,
									tupleid, oldtuple, slot);

		if (slot == NULL)		/* "do nothing" */
			return NULL;
	}

	/* INSTEAD OF ROW UPDATE Triggers */
	if (resultRelInfo->ri_TrigDesc &&
		resultRelInfo->ri_TrigDesc->trig_update_instead_row)
	{
		slot = ExecIRUpdateTriggers(estate, resultRelInfo,
									oldtuple, slot);

		if (slot == NULL)		/* "do nothing" */
			return NULL;

		/* trigger might have changed tuple */
	}
	else if (resultRelInfo->ri_FdwRoutine)
	{
		/*
		 * update in foreign table: let the FDW do it
		 */
		slot = resultRelInfo->ri_FdwRoutine->ExecForeignUpdate(estate,
															   resultRelInfo,
															   slot,
															   planSlot);

		if (slot == NULL)		/* "do nothing" */
			return NULL;

#if 0
		/* FDW might have changed tuple */
		/*
		 * GPDB: Greenplum does not allow triggers/contraints to reference
		 * system columns which makes the t_tableOid initialization reduntant.
		 */
		tuple = ExecMaterializeSlot(slot);

		/*
		 * AFTER ROW Triggers or RETURNING expressions might reference the
		 * tableoid column, so initialize t_tableOid before evaluating them.
		 */
		tuple->t_tableOid = RelationGetRelid(resultRelationDesc);
#endif
	}
	else
	{
		LockTupleMode lockmode;

		/*
		 * Constraints might reference the tableoid column, so initialize
		 * t_tableOid before evaluating them.
		 */
#if 0
		/*
		 * GPDB: Greenplum does not allow triggers/contraints to reference
		 * system columns which makes the t_tableOid initialization reduntant.
		 */
		tuple->t_tableOid = RelationGetRelid(resultRelationDesc);
#endif

		/*
		 * Check any RLS UPDATE WITH CHECK policies
		 *
		 * If we generate a new candidate tuple after EvalPlanQual testing, we
		 * must loop back here and recheck any RLS policies and constraints.
		 * (We don't need to redo triggers, however.  If there are any BEFORE
		 * triggers then trigger.c will have done heap_lock_tuple to lock the
		 * correct tuple, so there's no need to do them again.)
		 *
		 * ExecWithCheckOptions() will skip any WCOs which are not of the kind
		 * we are looking for at this point.
		 */
		lreplace:;
		if (resultRelInfo->ri_WithCheckOptions != NIL)
			ExecWithCheckOptions(WCO_RLS_UPDATE_CHECK,
								 resultRelInfo, slot, estate);

		/*
		 * Check the constraints of the tuple
		 */
		if (resultRelationDesc->rd_att->constr)
			ExecConstraints(resultRelInfo, slot, estate);

		/*
		 * replace the heap tuple
		 *
		 * Note: if es_crosscheck_snapshot isn't InvalidSnapshot, we check
		 * that the row to be updated is visible to that snapshot, and throw a
		 * can't-serialize error if not. This is a special-case behavior
		 * needed for referential integrity updates in transaction-snapshot
		 * mode transactions.
		 */
		lastTid = *tupleid;
		result = table_update(resultRelInfo, estate, &lastTid, slot,
							  estate->es_output_cid,
							  estate->es_crosscheck_snapshot,
							  true /* wait for commit */ ,
							  &hufd, &lockmode, &wasHotUpdate);

		switch (result)
		{
			case HeapTupleSelfUpdated:

				/*
				 * The target tuple was already updated or deleted by the
				 * current command, or by a later command in the current
				 * transaction.  The former case is possible in a join UPDATE
				 * where multiple tuples join to the same target tuple. This
				 * is pretty questionable, but Postgres has always allowed it:
				 * we just execute the first update action and ignore
				 * additional update attempts.
				 *
				 * The latter case arises if the tuple is modified by a
				 * command in a BEFORE trigger, or perhaps by a command in a
				 * volatile function used in the query.  In such situations we
				 * should not ignore the update, but it is equally unsafe to
				 * proceed.  We don't want to discard the original UPDATE
				 * while keeping the triggered actions based on it; and we
				 * have no principled way to merge this update with the
				 * previous ones.  So throwing an error is the only safe
				 * course.
				 *
				 * If a trigger actually intends this type of interaction, it
				 * can re-execute the UPDATE (assuming it can figure out how)
				 * and then return NULL to cancel the outer update.
				 *
				 * In GPDB, for AO tables HeapTupleSelfUpdated is returned only
				 * in case of same command tuple update based on visimap dirty
				 * list checking. Also, hufd is not initialized and can't be for
				 * AO case, as visimap update within same command happens at end
				 * of command.
				 */
				if (!RelationIsAppendOptimized(resultRelationDesc) && hufd.cmax != estate->es_output_cid)
					ereport(ERROR,
							(errcode(ERRCODE_TRIGGERED_DATA_CHANGE_VIOLATION),
									errmsg("tuple to be updated was already modified by an operation triggered by the current command"),
									errhint("Consider using an AFTER trigger instead of a BEFORE trigger to propagate changes to other rows.")));

				/* Else, already updated by self; nothing to do */
				return NULL;

			case HeapTupleMayBeUpdated:
				break;

			case HeapTupleUpdated:
				if (IsolationUsesXactSnapshot())
					ereport(ERROR,
							(errcode(ERRCODE_T_R_SERIALIZATION_FAILURE),
									errmsg("could not serialize access due to concurrent update")));
				if (!ItemPointerEquals(tupleid, &hufd.ctid))
				{
					TupleTableSlot *epqslot;

					epqslot = EvalPlanQual(estate,
										   epqstate,
										   resultRelationDesc,
										   resultRelInfo->ri_RangeTableIndex,
										   lockmode,
										   &hufd.ctid,
										   hufd.xmax);
					if (!TupIsNull(epqslot))
					{
						*tupleid = hufd.ctid;
						slot = ExecFilterJunk(resultRelInfo->ri_junkFilter, epqslot);
						goto lreplace;
					}
				}
				/* tuple already deleted; nothing to do */
				return NULL;

			default:
				elog(ERROR, "unrecognized heap_update status: %u", result);
				return NULL;
		}

		/*
		 * Note: instead of having to update the old index tuples associated
		 * with the heap tuple, all we do is form and insert new index tuples.
		 * This is because UPDATEs are actually DELETEs and INSERTs, and index
		 * tuple deletion is done later by VACUUM (see notes in ExecDelete).
		 * All we do here is insert new index tuples.  -cim 9/27/89
		 */

		/*
		 * insert index entries for tuple
		 *
		 * Note: heap_update returns the tid (location) of the new tuple in
		 * the t_self field.
		 *
		 * If it's a HOT update, we mustn't insert new index entries.
		 */
		if (resultRelInfo->ri_NumIndices > 0 && !wasHotUpdate)
			recheckIndexes = ExecInsertIndexTuples(slot, &lastTid,
												   estate, false, NULL, NIL);
	}
	if (canSetTag)
		(estate->es_processed)++;

	/* AFTER ROW UPDATE Triggers */
	if (resultRelInfo->ri_TrigDesc &&
		resultRelInfo->ri_TrigDesc->trig_update_after_row &&
		RelationIsHeap(resultRelationDesc))
	{
		HeapTuple tuple = ExecMaterializeSlot(slot);

		ExecARUpdateTriggers(estate, resultRelInfo, tupleid, oldtuple, tuple,
						 recheckIndexes);
	}

	list_free(recheckIndexes);

	/*
	 * Check any WITH CHECK OPTION constraints from parent views.  We are
	 * required to do this after testing all constraints and uniqueness
	 * violations per the SQL spec, so we do it after actually updating the
	 * record in the heap and all indexes.
	 *
	 * ExecWithCheckOptions() will skip any WCOs which are not of the kind we
	 * are looking for at this point.
	 */
	if (resultRelInfo->ri_WithCheckOptions != NIL)
		ExecWithCheckOptions(WCO_VIEW_CHECK, resultRelInfo, slot, estate);

	/* Process RETURNING if present */
	if (resultRelInfo->ri_projectReturning)
		return ExecProcessReturning(resultRelInfo->ri_projectReturning, slot, planSlot);

	return NULL;
}

/*
 * ExecOnConflictUpdate --- execute UPDATE of INSERT ON CONFLICT DO UPDATE
 *
 * Try to lock tuple for update as part of speculative insertion.  If
 * a qual originating from ON CONFLICT DO UPDATE is satisfied, update
 * (but still lock row, even though it may not satisfy estate's
 * snapshot).
 *
 * Returns true if if we're done (with or without an update), or false if
 * the caller must retry the INSERT from scratch.
 */
static bool
ExecOnConflictUpdate(ModifyTableState *mtstate,
					 ResultRelInfo *resultRelInfo,
					 ItemPointer conflictTid,
					 TupleTableSlot *planSlot,
					 TupleTableSlot *excludedSlot,
					 EState *estate,
					 bool canSetTag,
					 TupleTableSlot **returning)
{
	ExprContext *econtext = mtstate->ps.ps_ExprContext;
	Relation	relation = resultRelInfo->ri_RelationDesc;
	List	   *onConflictSetWhere = resultRelInfo->ri_onConflictSetWhere;
	HeapTupleData tuple;
	HeapUpdateFailureData hufd;
	LockTupleMode lockmode;
	HTSU_Result test;
	Buffer		buffer;

	/* Determine lock mode to use */
	lockmode = ExecUpdateLockMode(estate, resultRelInfo);

	/*
	 * Lock tuple for update.  Don't follow updates when tuple cannot be
	 * locked without doing so.  A row locking conflict here means our
	 * previous conclusion that the tuple is conclusively committed is not
	 * true anymore.
	 */
	tuple.t_self = *conflictTid;
	test = heap_lock_tuple(relation, &tuple, estate->es_output_cid,
						   lockmode, LockWaitBlock, false, &buffer,
						   &hufd);
	switch (test)
	{
		case HeapTupleMayBeUpdated:
			/* success! */
			break;

		case HeapTupleInvisible:

			/*
			 * This can occur when a just inserted tuple is updated again in
			 * the same command. E.g. because multiple rows with the same
			 * conflicting key values are inserted.
			 *
			 * This is somewhat similar to the ExecUpdate()
			 * HeapTupleSelfUpdated case.  We do not want to proceed because
			 * it would lead to the same row being updated a second time in
			 * some unspecified order, and in contrast to plain UPDATEs
			 * there's no historical behavior to break.
			 *
			 * It is the user's responsibility to prevent this situation from
			 * occurring.  These problems are why SQL-2003 similarly specifies
			 * that for SQL MERGE, an exception must be raised in the event of
			 * an attempt to update the same row twice.
			 */
			if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmin(tuple.t_data)))
				ereport(ERROR,
						(errcode(ERRCODE_CARDINALITY_VIOLATION),
						 errmsg("ON CONFLICT DO UPDATE command cannot affect row a second time"),
						 errhint("Ensure that no rows proposed for insertion within the same command have duplicate constrained values.")));

			/* This shouldn't happen */
			elog(ERROR, "attempted to lock invisible tuple");
			break;

		case HeapTupleSelfUpdated:

			/*
			 * This state should never be reached. As a dirty snapshot is used
			 * to find conflicting tuples, speculative insertion wouldn't have
			 * seen this row to conflict with.
			 */
			elog(ERROR, "unexpected self-updated tuple");
			break;

		case HeapTupleUpdated:
			if (IsolationUsesXactSnapshot())
				ereport(ERROR,
						(errcode(ERRCODE_T_R_SERIALIZATION_FAILURE),
						 errmsg("could not serialize access due to concurrent update")));

			/*
			 * Tell caller to try again from the very start.
			 *
			 * It does not make sense to use the usual EvalPlanQual() style
			 * loop here, as the new version of the row might not conflict
			 * anymore, or the conflicting tuple has actually been deleted.
			 */
			ReleaseBuffer(buffer);
			return false;

		default:
			elog(ERROR, "unrecognized heap_lock_tuple status: %u", test);
	}

	/*
	 * Success, the tuple is locked.
	 *
	 * Reset per-tuple memory context to free any expression evaluation
	 * storage allocated in the previous cycle.
	 */
	ResetExprContext(econtext);

	/*
	 * Verify that the tuple is visible to our MVCC snapshot if the current
	 * isolation level mandates that.
	 *
	 * It's not sufficient to rely on the check within ExecUpdate() as e.g.
	 * CONFLICT ... WHERE clause may prevent us from reaching that.
	 *
	 * This means we only ever continue when a new command in the current
	 * transaction could see the row, even though in READ COMMITTED mode the
	 * tuple will not be visible according to the current statement's
	 * snapshot.  This is in line with the way UPDATE deals with newer tuple
	 * versions.
	 */
	ExecCheckHeapTupleVisible(resultRelInfo->ri_RelationDesc, estate, &tuple, buffer);

	/* Store target's existing tuple in the state's dedicated slot */
	ExecStoreHeapTuple(&tuple, mtstate->mt_existing, buffer, false);

	/*
	 * Make tuple and any needed join variables available to ExecQual and
	 * ExecProject.  The EXCLUDED tuple is installed in ecxt_innertuple, while
	 * the target's existing tuple is installed in the scantuple.  EXCLUDED
	 * has been made to reference INNER_VAR in setrefs.c, but there is no
	 * other redirection.
	 */
	econtext->ecxt_scantuple = mtstate->mt_existing;
	econtext->ecxt_innertuple = excludedSlot;
	econtext->ecxt_outertuple = NULL;

	if (!ExecQual(onConflictSetWhere, econtext, false))
	{
		ReleaseBuffer(buffer);
		InstrCountFiltered1(&mtstate->ps, 1);
		return true;			/* done with the tuple */
	}

	if (resultRelInfo->ri_WithCheckOptions != NIL)
	{
		/*
		 * Check target's existing tuple against UPDATE-applicable USING
		 * security barrier quals (if any), enforced here as RLS checks/WCOs.
		 *
		 * The rewriter creates UPDATE RLS checks/WCOs for UPDATE security
		 * quals, and stores them as WCOs of "kind" WCO_RLS_CONFLICT_CHECK,
		 * but that's almost the extent of its special handling for ON
		 * CONFLICT DO UPDATE.
		 *
		 * The rewriter will also have associated UPDATE applicable straight
		 * RLS checks/WCOs for the benefit of the ExecUpdate() call that
		 * follows.  INSERTs and UPDATEs naturally have mutually exclusive WCO
		 * kinds, so there is no danger of spurious over-enforcement in the
		 * INSERT or UPDATE path.
		 */
		ExecWithCheckOptions(WCO_RLS_CONFLICT_CHECK, resultRelInfo,
							 mtstate->mt_existing,
							 mtstate->ps.state);
	}

	/* Project the new tuple version */
	ExecProject(resultRelInfo->ri_onConflictSetProj, NULL);

	/*
	 * Note that it is possible that the target tuple has been modified in
	 * this session, after the above heap_lock_tuple. We choose to not error
	 * out in that case, in line with ExecUpdate's treatment of similar cases.
	 * This can happen if an UPDATE is triggered from within ExecQual(),
	 * ExecWithCheckOptions() or ExecProject() above, e.g. by selecting from a
	 * wCTE in the ON CONFLICT's SET.
	 */

	/* Execute UPDATE with projection */
	*returning = ExecUpdate(&tuple.t_self, NULL,
							mtstate->mt_conflproj, planSlot,
							GpIdentity.segindex,
							&mtstate->mt_epqstate, mtstate->ps.state,
							canSetTag);

	ReleaseBuffer(buffer);
	return true;
}


/*
 * Process BEFORE EACH STATEMENT triggers
 */
static void
fireBSTriggers(ModifyTableState *node)
{
	switch (node->operation)
	{
		case CMD_INSERT:
			ExecBSInsertTriggers(node->ps.state, node->resultRelInfo);
			if (node->mt_onconflict == ONCONFLICT_UPDATE)
				ExecBSUpdateTriggers(node->ps.state,
									 node->resultRelInfo);
			break;
		case CMD_UPDATE:
			ExecBSUpdateTriggers(node->ps.state, node->resultRelInfo);
			break;
		case CMD_DELETE:
			ExecBSDeleteTriggers(node->ps.state, node->resultRelInfo);
			break;
		default:
			elog(ERROR, "unknown operation");
			break;
	}
}

/*
 * Process AFTER EACH STATEMENT triggers
 */
static void
fireASTriggers(ModifyTableState *node)
{
	switch (node->operation)
	{
		case CMD_INSERT:
			if (node->mt_onconflict == ONCONFLICT_UPDATE)
				ExecASUpdateTriggers(node->ps.state,
									 node->resultRelInfo);
			ExecASInsertTriggers(node->ps.state, node->resultRelInfo);
			break;
		case CMD_UPDATE:
			ExecASUpdateTriggers(node->ps.state, node->resultRelInfo);
			break;
		case CMD_DELETE:
			ExecASDeleteTriggers(node->ps.state, node->resultRelInfo);
			break;
		default:
			elog(ERROR, "unknown operation");
			break;
	}
}


/* ----------------------------------------------------------------
 *	   ExecModifyTable
 *
 *		Perform table modifications as required, and return RETURNING results
 *		if needed.
 * ----------------------------------------------------------------
 */
TupleTableSlot *
ExecModifyTable(ModifyTableState *node)
{
	EState	   *estate = node->ps.state;
	PlanGenerator planGen = estate->es_plannedstmt->planGen;
	CmdType		operation = node->operation;
	ResultRelInfo *saved_resultRelInfo;
	ResultRelInfo *resultRelInfo;
	PlanState  *subplanstate;
	JunkFilter *junkfilter;
	AttrNumber  action_attno;
	AttrNumber  segid_attno;
	AttrNumber  tupleoid_attno;
	TupleTableSlot *slot;
	TupleTableSlot *planSlot;
	ItemPointer tupleid = NULL;
	ItemPointerData tuple_ctid;
	HeapTupleData oldtupdata;
	HeapTuple	oldtuple;

	/*
	 * This should NOT get called during EvalPlanQual; we should have passed a
	 * subplan tree to EvalPlanQual, instead.  Use a runtime test not just
	 * Assert because this condition is easy to miss in testing.  (Note:
	 * although ModifyTable should not get executed within an EvalPlanQual
	 * operation, we do have to allow it to be initialized and shut down in
	 * case it is within a CTE subplan.  Hence this test must be here, not in
	 * ExecInitModifyTable.)
	 */
	if (estate->es_epqTuple != NULL)
		elog(ERROR, "ModifyTable should not be called during EvalPlanQual");

	/*
	 * If we've already completed processing, don't try to do more.  We need
	 * this test because ExecPostprocessPlan might call us an extra time, and
	 * our subplan's nodes aren't necessarily robust against being called
	 * extra times.
	 */
	if (node->mt_done)
		return NULL;

	if (Gp_role == GP_ROLE_EXECUTE && !Gp_is_writer)
	{
		elog(ERROR, "INSERT/UPDATE/DELETE must be executed by a writer segworker group");
	}

	/*
	 * On first call, fire BEFORE STATEMENT triggers before proceeding.
	 */
	if (node->fireBSTriggers)
	{
		fireBSTriggers(node);
		node->fireBSTriggers = false;
	}

	/* Preload local variables */
	resultRelInfo = node->resultRelInfo + node->mt_whichplan;
	subplanstate = node->mt_plans[node->mt_whichplan];
	junkfilter = resultRelInfo->ri_junkFilter;
	action_attno = resultRelInfo->ri_action_attno;
	segid_attno = resultRelInfo->ri_segid_attno;
	tupleoid_attno = resultRelInfo->ri_tupleoid_attno;

	/*
	 * es_result_relation_info must point to the currently active result
	 * relation while we are within this ModifyTable node.  Even though
	 * ModifyTable nodes can't be nested statically, they can be nested
	 * dynamically (since our subplan could include a reference to a modifying
	 * CTE).  So we have to save and restore the caller's value.
	 */
	saved_resultRelInfo = estate->es_result_relation_info;

	estate->es_result_relation_info = resultRelInfo;

	/*
	 * Fetch rows from subplan(s), and execute the required table modification
	 * for each row.
	 */
	for (;;)
	{
		/*
		 * Reset the per-output-tuple exprcontext.  This is needed because
		 * triggers expect to use that context as workspace.  It's a bit ugly
		 * to do this below the top level of the plan, however.  We might need
		 * to rethink this later.
		 */
		ResetPerTupleExprContext(estate);

		planSlot = ExecProcNode(subplanstate);

		if (TupIsNull(planSlot))
		{
			/* advance to next subplan if any */
			node->mt_whichplan++;
			if (node->mt_whichplan < node->mt_nplans)
			{
				estate->es_result_relation_info = estate->es_result_relations + node->mt_whichplan;
				resultRelInfo = estate->es_result_relation_info;
				subplanstate = node->mt_plans[node->mt_whichplan];
				junkfilter = estate->es_result_relation_info->ri_junkFilter;
				action_attno = estate->es_result_relation_info->ri_action_attno;
				segid_attno = estate->es_result_relation_info->ri_segid_attno;
				tupleoid_attno = estate->es_result_relation_info->ri_tupleoid_attno;
				EvalPlanQualSetPlan(&node->mt_epqstate, subplanstate->plan,
									node->mt_arowmarks[node->mt_whichplan]);
				continue;
			}
			else
				break;
		}

		/*
		 * If resultRelInfo->ri_usesFdwDirectModify is true, all we need to do
		 * here is compute the RETURNING expressions.
		 */
		if (resultRelInfo->ri_usesFdwDirectModify)
		{
			Assert(resultRelInfo->ri_projectReturning);

			/*
			 * A scan slot containing the data that was actually inserted,
			 * updated or deleted has already been made available to
			 * ExecProcessReturning by IterateDirectModify, so no need to
			 * provide it here.
			 */
			slot = ExecProcessReturning(resultRelInfo->ri_projectReturning, NULL, planSlot);

			estate->es_result_relation_info = saved_resultRelInfo;
			return slot;
		}

		EvalPlanQualSetSlot(&node->mt_epqstate, planSlot);
		slot = planSlot;

		int32 segid = GpIdentity.segindex;
		int action = -1;
		Oid tupleoid = InvalidOid;;

		oldtuple = NULL;
		if (junkfilter != NULL)
		{
			/*
			 * extract the 'ctid' or 'wholerow' junk attribute.
			 */
			if (operation == CMD_UPDATE || operation == CMD_DELETE)
			{
				char		relkind;
				Datum		datum;
				bool		isNull;

				relkind = resultRelInfo->ri_RelationDesc->rd_rel->relkind;
				if (relkind == RELKIND_RELATION || relkind == RELKIND_MATVIEW)
				{
					datum = ExecGetJunkAttribute(slot,
												 junkfilter->jf_junkAttNo,
												 &isNull);
					/* shouldn't ever get a null result... */
					if (isNull)
						elog(ERROR, "ctid is NULL");

					tupleid = (ItemPointer) DatumGetPointer(datum);
					tuple_ctid = *tupleid;		/* be sure we don't free
												 * ctid!! */
					tupleid = &tuple_ctid;
				}

				/*
				 * Use the wholerow attribute, when available, to reconstruct
				 * the old relation tuple.
				 *
				 * Foreign table updates have a wholerow attribute when the
				 * relation has an AFTER ROW trigger.  Note that the wholerow
				 * attribute does not carry system columns.  Foreign table
				 * triggers miss seeing those, except that we know enough here
				 * to set t_tableOid.  Quite separately from this, the FDW may
				 * fetch its own junk attrs to identify the row.
				 *
				 * Other relevant relkinds, currently limited to views, always
				 * have a wholerow attribute.
				 */
				else if (AttributeNumberIsValid(junkfilter->jf_junkAttNo))
				{
					datum = ExecGetJunkAttribute(slot,
												 junkfilter->jf_junkAttNo,
												 &isNull);
					/* shouldn't ever get a null result... */
					if (isNull)
						elog(ERROR, "wholerow is NULL");

					oldtupdata.t_data = DatumGetHeapTupleHeader(datum);
					oldtupdata.t_len =
						HeapTupleHeaderGetDatumLength(oldtupdata.t_data);
					ItemPointerSetInvalid(&(oldtupdata.t_self));
					/* Historically, view triggers see invalid t_tableOid. */
#if 0
					oldtupdata.t_tableOid =
						(relkind == RELKIND_VIEW) ? InvalidOid :
						RelationGetRelid(resultRelInfo->ri_RelationDesc);
#endif
					oldtuple = &oldtupdata;
				}
				else
					Assert(relkind == RELKIND_FOREIGN_TABLE);

				/*
				 * Extract GPDB-specific junk attributes.
				 */
				if (AttributeNumberIsValid(segid_attno))
				{
					datum = ExecGetJunkAttribute(slot,
												 segid_attno,
												 &isNull);
					/* shouldn't ever get a null result... */
					if (isNull)
						elog(ERROR, "action is NULL");

					segid = DatumGetInt32(datum);
				}
				if (AttributeNumberIsValid(action_attno))
				{
					datum = ExecGetJunkAttribute(slot,
												 action_attno,
												 &isNull);
					/* shouldn't ever get a null result... */
					if (isNull)
						elog(ERROR, "gp_segment_id is NULL");

					action = DatumGetInt32(datum);
				}
				if (AttributeNumberIsValid(tupleoid_attno))
				{
					datum = ExecGetJunkAttribute(slot,
												 tupleoid_attno,
												 &isNull);
					/* shouldn't ever get a null result... */
					if (isNull)
						elog(ERROR, "tupleoid is NULL");

					tupleoid = DatumGetInt32(datum);
				}
			}

			/*
			 * apply the junkfilter if needed.
			 */
			if (operation != CMD_DELETE)
				slot = ExecFilterJunk(junkfilter, slot);
		}

		switch (operation)
		{
			case CMD_INSERT:
				slot = ExecInsert(node, slot, planSlot,
								node->mt_arbiterindexes, node->mt_onconflict,
								  estate, node->canSetTag,
								  false /* isUpdate */, InvalidOid /* tupleOid */);
				break;
			case CMD_UPDATE:
				if (!AttributeNumberIsValid(action_attno))
				{
					/* normal non-split UPDATE */
					slot = ExecUpdate(tupleid, oldtuple, slot, planSlot, segid,
									  &node->mt_epqstate, estate,
									  node->canSetTag);
				}
				else if (DML_INSERT == action)
				{
					if (estate->es_result_partitions && planGen == PLANGEN_PLANNER)
						checkPartitionUpdate(estate, slot, estate->es_result_relation_info);

					slot = ExecInsert(node, slot, planSlot, node->mt_arbiterindexes,
									  node->mt_onconflict, estate, node->canSetTag,
									  true /* isUpdate */, tupleoid);
				}
				else /* DML_DELETE */
				{
					slot = ExecDelete(tupleid, segid, oldtuple, planSlot,
									  &node->mt_epqstate, estate, false,
									  true /* isUpdate */);
				}
				break;
			case CMD_DELETE:
				slot = ExecDelete(tupleid, segid, oldtuple, planSlot,
								  &node->mt_epqstate, estate, node->canSetTag,
								  false /* isUpdate */);
				break;
			default:
				elog(ERROR, "unknown operation");
				break;
		}

		/*
		 * If the target is a partitioned table, ExecInsert / ExecUpdate /
		 * ExecDelete might have changed es_result_relation_info to point to
		 * a partition, instead of the top-level table. Reset it. (It would
		 * be more tidy if those functions cleaned up after themselves, but
		 * it's more robust to do it here just once.)
		 */
		estate->es_result_relation_info = resultRelInfo;

		/*
		 * If we got a RETURNING result, return it to caller.  We'll continue
		 * the work on next call.
		 */
		if (slot)
		{
			estate->es_result_relation_info = saved_resultRelInfo;
			return slot;
		}
	}

	/* Restore es_result_relation_info before exiting */
	estate->es_result_relation_info = saved_resultRelInfo;

	/*
	 * We're done, but fire AFTER STATEMENT triggers before exiting.
	 */
	/* In GPDB, don't fire statement triggers in reader processes */
	if (Gp_role != GP_ROLE_EXECUTE || Gp_is_writer)
		fireASTriggers(node);

	node->mt_done = true;

	return NULL;
}

/* ----------------------------------------------------------------
 *		ExecInitModifyTable
 * ----------------------------------------------------------------
 */
ModifyTableState *
ExecInitModifyTable(ModifyTable *node, EState *estate, int eflags)
{
	ModifyTableState *mtstate;
	CmdType		operation = node->operation;
	int			nplans = list_length(node->plans);
	ResultRelInfo *saved_resultRelInfo;
	ResultRelInfo *resultRelInfo;
	TupleDesc	tupDesc;
	Plan	   *subplan;
	ListCell   *l;
	int			i;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	/*
	 * create state structure
	 */
	mtstate = makeNode(ModifyTableState);
	mtstate->ps.plan = (Plan *) node;
	mtstate->ps.state = estate;
	mtstate->ps.targetlist = NIL;		/* not actually used */

	mtstate->operation = operation;
	mtstate->canSetTag = node->canSetTag;
	mtstate->mt_done = false;

	mtstate->mt_plans = (PlanState **) palloc0(sizeof(PlanState *) * nplans);
	mtstate->resultRelInfo = estate->es_result_relations + node->resultRelIndex;
	mtstate->mt_arowmarks = (List **) palloc0(sizeof(List *) * nplans);
	mtstate->mt_nplans = nplans;
	mtstate->mt_onconflict = node->onConflictAction;
	mtstate->mt_arbiterindexes = node->arbiterIndexes;

	/* set up epqstate with dummy subplan data for the moment */
	EvalPlanQualInit(&mtstate->mt_epqstate, estate, NULL, NIL, node->epqParam);

	if (CMD_UPDATE == operation)
	{
		mtstate->mt_isSplitUpdates = (bool *) palloc0(nplans * sizeof(bool));
		if (node->isSplitUpdates)
		{
			if (list_length(node->isSplitUpdates) != nplans)
				elog(ERROR, "ModifyTable node is missing is-split-update information");

			i = 0;
			foreach(l, node->isSplitUpdates)
				mtstate->mt_isSplitUpdates[i++] = (bool) lfirst_int(l);
		}
	}

	/* GPDB: Don't fire statement-triggers in QE reader processes */
	if (Gp_role != GP_ROLE_EXECUTE || Gp_is_writer)
		mtstate->fireBSTriggers = true;

	/*
	 * call ExecInitNode on each of the plans to be executed and save the
	 * results into the array "mt_plans".  This is also a convenient place to
	 * verify that the proposed target relations are valid and open their
	 * indexes for insertion of new index entries.  Note we *must* set
	 * estate->es_result_relation_info correctly while we initialize each
	 * sub-plan; ExecContextForcesOids depends on that!
	 */
	saved_resultRelInfo = estate->es_result_relation_info;

	resultRelInfo = mtstate->resultRelInfo;
	i = 0;
	foreach(l, node->plans)
	{
		subplan = (Plan *) lfirst(l);

		/* Initialize the usesFdwDirectModify flag */
		resultRelInfo->ri_usesFdwDirectModify = bms_is_member(i,
												 node->fdwDirectModifyPlans);

		/*
		 * Verify result relation is a valid target for the current operation
		 */
		CheckValidResultRel(resultRelInfo->ri_RelationDesc, operation);

		/*
		 * If there are indices on the result relation, open them and save
		 * descriptors in the result relation info, so that we can add new
		 * index entries for the tuples we add/update.  We need not do this
		 * for a DELETE, however, since deletion doesn't affect indexes. Also,
		 * inside an EvalPlanQual operation, the indexes might be open
		 * already, since we share the resultrel state with the original
		 * query.
		 */
		if (resultRelInfo->ri_RelationDesc->rd_rel->relhasindex &&
			operation != CMD_DELETE &&
			resultRelInfo->ri_IndexRelationDescs == NULL)
			ExecOpenIndices(resultRelInfo, mtstate->mt_onconflict != ONCONFLICT_NONE);

		/* Now init the plan for this result rel */
		estate->es_result_relation_info = resultRelInfo;
		mtstate->mt_plans[i] = ExecInitNode(subplan, estate, eflags);

		/* Also let FDWs init themselves for foreign-table result rels */
		if (!resultRelInfo->ri_usesFdwDirectModify &&
			resultRelInfo->ri_FdwRoutine != NULL &&
			resultRelInfo->ri_FdwRoutine->BeginForeignModify != NULL)
		{
			List	   *fdw_private = (List *) list_nth(node->fdwPrivLists, i);

			resultRelInfo->ri_FdwRoutine->BeginForeignModify(mtstate,
															 resultRelInfo,
															 fdw_private,
															 i,
															 eflags);
		}

		resultRelInfo++;
		i++;
	}

	estate->es_result_relation_info = saved_resultRelInfo;

	/*
	 * Initialize any WITH CHECK OPTION constraints if needed.
	 */
	resultRelInfo = mtstate->resultRelInfo;
	i = 0;
	foreach(l, node->withCheckOptionLists)
	{
		List	   *wcoList = (List *) lfirst(l);
		List	   *wcoExprs = NIL;
		ListCell   *ll;

		foreach(ll, wcoList)
		{
			WithCheckOption *wco = (WithCheckOption *) lfirst(ll);
			ExprState  *wcoExpr = ExecInitExpr((Expr *) wco->qual,
											   mtstate->mt_plans[i]);

			wcoExprs = lappend(wcoExprs, wcoExpr);
		}

		resultRelInfo->ri_WithCheckOptions = wcoList;
		resultRelInfo->ri_WithCheckOptionExprs = wcoExprs;
		resultRelInfo++;
		i++;
	}

	/*
	 * Initialize RETURNING projections if needed.
	 */
	if (node->returningLists)
	{
		TupleTableSlot *slot;
		ExprContext *econtext;

		/*
		 * Initialize result tuple slot and assign its rowtype using the first
		 * RETURNING list.  We assume the rest will look the same.
		 */
		tupDesc = ExecTypeFromTL((List *) linitial(node->returningLists),
								 false);

		/* Set up a slot for the output of the RETURNING projection(s) */
		ExecInitResultTupleSlot(estate, &mtstate->ps);
		ExecAssignResultType(&mtstate->ps, tupDesc);
		slot = mtstate->ps.ps_ResultTupleSlot;

		/* Need an econtext too */
		econtext = CreateExprContext(estate);
		mtstate->ps.ps_ExprContext = econtext;

		/*
		 * Build a projection for each result rel.
		 */
		resultRelInfo = mtstate->resultRelInfo;
		foreach(l, node->returningLists)
		{
			List	   *rlist = (List *) lfirst(l);
			List	   *rliststate;

			resultRelInfo->ri_returningList = rlist;
			rliststate = (List *) ExecInitExpr((Expr *) rlist, &mtstate->ps);
			resultRelInfo->ri_projectReturning =
				ExecBuildProjectionInfo(rliststate, econtext, slot,
									 resultRelInfo->ri_RelationDesc->rd_att);
			resultRelInfo++;
		}
	}
	else
	{
		/*
		 * We still must construct a dummy result tuple type, because InitPlan
		 * expects one (maybe should change that?).
		 */
		tupDesc = ExecTypeFromTL(NIL, false);
		ExecInitResultTupleSlot(estate, &mtstate->ps);
		ExecAssignResultType(&mtstate->ps, tupDesc);

		mtstate->ps.ps_ExprContext = NULL;
	}

	/*
	 * If needed, Initialize target list, projection and qual for ON CONFLICT
	 * DO UPDATE.
	 */
	resultRelInfo = mtstate->resultRelInfo;
	if (node->onConflictAction == ONCONFLICT_UPDATE)
	{
		ExprContext *econtext;
		ExprState  *setexpr;
		TupleDesc	tupDesc;

		/* insert may only have one plan, inheritance is not expanded */
		Assert(nplans == 1);

		/* already exists if created by RETURNING processing above */
		if (mtstate->ps.ps_ExprContext == NULL)
			ExecAssignExprContext(estate, &mtstate->ps);

		econtext = mtstate->ps.ps_ExprContext;

		/* initialize slot for the existing tuple */
		mtstate->mt_existing = ExecInitExtraTupleSlot(mtstate->ps.state);
		ExecSetSlotDescriptor(mtstate->mt_existing,
							  resultRelInfo->ri_RelationDesc->rd_att);

		/* carried forward solely for the benefit of explain */
		mtstate->mt_excludedtlist = node->exclRelTlist;

		/* create target slot for UPDATE SET projection */
		tupDesc = ExecTypeFromTL((List *) node->onConflictSet,
						 resultRelInfo->ri_RelationDesc->rd_rel->relhasoids);
		mtstate->mt_conflproj = ExecInitExtraTupleSlot(mtstate->ps.state);
		ExecSetSlotDescriptor(mtstate->mt_conflproj, tupDesc);

		/* build UPDATE SET expression and projection state */
		setexpr = ExecInitExpr((Expr *) node->onConflictSet, &mtstate->ps);
		resultRelInfo->ri_onConflictSetProj =
			ExecBuildProjectionInfo((List *) setexpr, econtext,
									mtstate->mt_conflproj,
									resultRelInfo->ri_RelationDesc->rd_att);

		/* build DO UPDATE WHERE clause expression */
		if (node->onConflictWhere)
		{
			ExprState  *qualexpr;

			qualexpr = ExecInitExpr((Expr *) node->onConflictWhere,
									&mtstate->ps);

			resultRelInfo->ri_onConflictSetWhere = (List *) qualexpr;
		}
	}

	/*
	 * If we have any secondary relations in an UPDATE or DELETE, they need to
	 * be treated like non-locked relations in SELECT FOR UPDATE, ie, the
	 * EvalPlanQual mechanism needs to be told about them.  Locate the
	 * relevant ExecRowMarks.
	 */
	foreach(l, node->rowMarks)
	{
		PlanRowMark *rc = (PlanRowMark *) lfirst(l);
		ExecRowMark *erm;
		bool		isdistributed = false;

		Assert(IsA(rc, PlanRowMark));

		/* ignore "parent" rowmarks; they are irrelevant at runtime */
		if (rc->isParent)
			continue;

		/*
		 * Like in preprocess_targetlist, ignore distributed tables.
		 */
		/*
		 * GPDB_91_MERGE_FIXME: we are largely just ignoring locking altogether.
		 * Perhaps that's OK as long as we take a full table lock on any UPDATEs
		 * or DELETEs. But sure doesn't seem right. Can we do better?
		 */
		{
			RangeTblEntry *rte = rt_fetch(rc->rti, estate->es_plannedstmt->rtable);

			if (rte->rtekind == RTE_RELATION)
			{
				Relation relation = heap_open(rte->relid, NoLock);
				if (GpPolicyIsPartitioned(relation->rd_cdbpolicy))
					isdistributed = true;
				heap_close(relation, NoLock);
				if (isdistributed)
					continue;
			}
		}
		if (Gp_role == GP_ROLE_EXECUTE)
		{
			/*
			 * In the executor, we don't have information on which tables are
			 * distributed. Assume that everything is; we wouldn't be running this
			 * slice on an entry table otherwise.
			 */
			continue;
		}

		/* find ExecRowMark (same for all subplans) */
		erm = ExecFindRowMark(estate, rc->rti, false);

		/* build ExecAuxRowMark for each subplan */
		for (i = 0; i < nplans; i++)
		{
			ExecAuxRowMark *aerm;

			subplan = mtstate->mt_plans[i]->plan;
			aerm = ExecBuildAuxRowMark(erm, subplan->targetlist);
			mtstate->mt_arowmarks[i] = lappend(mtstate->mt_arowmarks[i], aerm);
		}
	}

	/* select first subplan */
	mtstate->mt_whichplan = 0;
	subplan = (Plan *) linitial(node->plans);
	EvalPlanQualSetPlan(&mtstate->mt_epqstate, subplan,
						mtstate->mt_arowmarks[0]);

	/*
	 * Initialize the junk filter(s) if needed.  INSERT queries need a filter
	 * if there are any junk attrs in the tlist.  UPDATE and DELETE always
	 * need a filter, since there's always a junk 'ctid' or 'wholerow'
	 * attribute present --- no need to look first.
	 *
	 * If there are multiple result relations, each one needs its own junk
	 * filter.  Note multiple rels are only possible for UPDATE/DELETE, so we
	 * can't be fooled by some needing a filter and some not.
	 *
	 * This section of code is also a convenient place to verify that the
	 * output of an INSERT or UPDATE matches the target table(s).
	 */
	{
		bool		junk_filter_needed = false;

		switch (operation)
		{
			case CMD_INSERT:
				foreach(l, subplan->targetlist)
				{
					TargetEntry *tle = (TargetEntry *) lfirst(l);

					if (tle->resjunk)
					{
						junk_filter_needed = true;
						break;
					}
				}
				break;
			case CMD_UPDATE:
			case CMD_DELETE:
				junk_filter_needed = true;
				break;
			default:
				elog(ERROR, "unknown operation");
				break;
		}

		if (junk_filter_needed)
		{
			resultRelInfo = mtstate->resultRelInfo;
			for (i = 0; i < nplans; i++)
			{
				JunkFilter *j;

				subplan = mtstate->mt_plans[i]->plan;
				if (operation == CMD_INSERT || operation == CMD_UPDATE)
					ExecCheckPlanOutput(resultRelInfo->ri_RelationDesc,
										subplan->targetlist);

				j = ExecInitJunkFilter(subplan->targetlist,
							resultRelInfo->ri_RelationDesc->rd_att->tdhasoid,
									   ExecInitExtraTupleSlot(estate));

				if (operation == CMD_UPDATE || operation == CMD_DELETE)
				{
					/* For UPDATE/DELETE, find the appropriate junk attr now */
					char		relkind;

					relkind = resultRelInfo->ri_RelationDesc->rd_rel->relkind;
					if (relkind == RELKIND_RELATION ||
						relkind == RELKIND_MATVIEW)
					{
						j->jf_junkAttNo = ExecFindJunkAttribute(j, "ctid");
						if (!AttributeNumberIsValid(j->jf_junkAttNo))
							elog(ERROR, "could not find junk ctid column");

						/* Extra GPDB junk columns */
						resultRelInfo->ri_segid_attno = ExecFindJunkAttribute(j, "gp_segment_id");
						if (!AttributeNumberIsValid(resultRelInfo->ri_segid_attno))
							elog(ERROR, "could not find junk gp_segment_id column");

						if (operation == CMD_UPDATE && mtstate->mt_isSplitUpdates[i])
						{
							resultRelInfo->ri_action_attno = ExecFindJunkAttribute(j, "DMLAction");
							if (!AttributeNumberIsValid(resultRelInfo->ri_action_attno))
								elog(ERROR, "could not find junk action column");

							if (resultRelInfo->ri_RelationDesc->rd_rel->relhasoids)
							{
								resultRelInfo->ri_tupleoid_attno = ExecFindJunkAttribute(j, "oid");
								if (!AttributeNumberIsValid(resultRelInfo->ri_tupleoid_attno))
									elog(ERROR, "could not find junk tupleoid column");
							}
						}
					}
					else if (relkind == RELKIND_FOREIGN_TABLE)
					{
						/*
						 * When there is an AFTER trigger, there should be a
						 * wholerow attribute.
						 */
						j->jf_junkAttNo = ExecFindJunkAttribute(j, "wholerow");
					}
					else
					{
						j->jf_junkAttNo = ExecFindJunkAttribute(j, "wholerow");
						if (!AttributeNumberIsValid(j->jf_junkAttNo))
							elog(ERROR, "could not find junk wholerow column");
					}
				}

				resultRelInfo->ri_junkFilter = j;
				resultRelInfo++;
			}
		}
		else
		{
			if (operation == CMD_INSERT)
				ExecCheckPlanOutput(mtstate->resultRelInfo->ri_RelationDesc,
									subplan->targetlist);
		}
	}

	/*
	 * Set up a tuple table slot for use for trigger output tuples. In a plan
	 * containing multiple ModifyTable nodes, all can share one such slot, so
	 * we keep it in the estate.
	 */
	if (estate->es_trig_tuple_slot == NULL)
		estate->es_trig_tuple_slot = ExecInitExtraTupleSlot(estate);

	/*
	 * Lastly, if this is not the primary (canSetTag) ModifyTable node, add it
	 * to estate->es_auxmodifytables so that it will be run to completion by
	 * ExecPostprocessPlan.  (It'd actually work fine to add the primary
	 * ModifyTable node too, but there's no need.)  Note the use of lcons not
	 * lappend: we need later-initialized ModifyTable nodes to be shut down
	 * before earlier ones.  This ensures that we don't throw away RETURNING
	 * rows that need to be seen by a later CTE subplan.
	 */
	if (Gp_role == GP_ROLE_EXECUTE || Gp_role == GP_ROLE_UTILITY)
	{
		/*
		 * We do not need this unless in executor or with utility role. Note
		 * This was added for the data modifying CTE feature but there are other
		 * cases could run into this also.
		 */
		if (!mtstate->canSetTag)
			estate->es_auxmodifytables = lcons(mtstate,
											   estate->es_auxmodifytables);
	}

	return mtstate;
}

/* ----------------------------------------------------------------
 *		ExecEndModifyTable
 *
 *		Shuts down the plan.
 *
 *		Returns nothing of interest.
 * ----------------------------------------------------------------
 */
void
ExecEndModifyTable(ModifyTableState *node)
{
	int			i;

	/*
	 * Allow any FDWs to shut down
	 */
	for (i = 0; i < node->mt_nplans; i++)
	{
		ResultRelInfo *resultRelInfo = node->resultRelInfo + i;

		if (!resultRelInfo->ri_usesFdwDirectModify &&
			resultRelInfo->ri_FdwRoutine != NULL &&
			resultRelInfo->ri_FdwRoutine->EndForeignModify != NULL)
			resultRelInfo->ri_FdwRoutine->EndForeignModify(node->ps.state,
														   resultRelInfo);
	}

	/*
	 * Free the exprcontext
	 */
	ExecFreeExprContext(&node->ps);

	/*
	 * clean out the tuple table
	 */
	ExecClearTuple(node->ps.ps_ResultTupleSlot);

	/*
	 * Terminate EPQ execution if active
	 */
	EvalPlanQualEnd(&node->mt_epqstate);

	/*
	 * shut down subplans
	 */
	for (i = 0; i < node->mt_nplans; i++)
		ExecEndNode(node->mt_plans[i]);
}

void
ExecReScanModifyTable(ModifyTableState *node)
{
	/*
	 * Currently, we don't need to support rescan on ModifyTable nodes. The
	 * semantics of that would be a bit debatable anyway.
	 */
	elog(ERROR, "ExecReScanModifyTable is not implemented");
}

void
ExecSquelchModifyTable(ModifyTableState *node)
{
	/*
	 * ModifyTable nodes must run to completion when asked to Squelch so
	 * that we don't risk losing modifications which should be performed
	 * regardless of any LIMIT's or other forms for projections which could
	 * end up causing a squelch to happen.
	 */
	for (;;)
	{
		TupleTableSlot *result;

		result = ExecModifyTable(node);
		if (!result)
			break;
	}
}

/*
 * table_insert/update/delete()
 *
 * Helper functions for ExecInsert(), ExecUpdate(), ExecDelete() to deal with
 * different kinds of tables in GPDB; appendonly, AOCO, external and heap.
 * PostgreSQL uses just heap_insert/update/delete(), up to v12, and in v12,
 * the new tableam API functions.
 */
static Oid
table_insert(ResultRelInfo *resultRelInfo, EState *estate, TupleTableSlot *slot,
			 Oid tuple_oid, ItemPointer lastTid, CommandId cid)
{
	Relation	resultRelationDesc = resultRelInfo->ri_RelationDesc;
	Oid			newId;

	if (RelationIsAoRows(resultRelationDesc))
	{
		MemTuple	mtuple;

		if (resultRelInfo->ri_aoInsertDesc == NULL)
		{
			/* Set the pre-assigned fileseg number to insert into */
			ResultRelInfoChooseSegno(resultRelInfo);

			resultRelInfo->ri_aoInsertDesc =
				appendonly_insert_init(resultRelationDesc,
									   resultRelInfo->ri_aosegno,
									   false);
		}

		mtuple = ExecFetchSlotMemTuple(slot);
		newId = appendonly_insert(resultRelInfo->ri_aoInsertDesc, mtuple, tuple_oid, (AOTupleId *) lastTid);
	}
	else if (RelationIsAoCols(resultRelationDesc))
	{
		if (resultRelInfo->ri_aocsInsertDesc == NULL)
		{
			ResultRelInfoChooseSegno(resultRelInfo);
			resultRelInfo->ri_aocsInsertDesc = aocs_insert_init(resultRelationDesc,
																resultRelInfo->ri_aosegno, false);
		}

		newId = aocs_insert(resultRelInfo->ri_aocsInsertDesc, slot);
		*lastTid = *slot_get_ctid(slot);
	}
	else if (RelationIsHeap(resultRelationDesc))
	{
		HeapTuple tuple;

		/*
		 * get the heap tuple out of the tuple table slot, making sure we have a
		 * writable copy. (heap_insert() will scribble on the tuple)
		 */
		tuple = ExecMaterializeSlot(slot);
		if (resultRelationDesc->rd_rel->relhasoids)
			HeapTupleSetOid(tuple, tuple_oid);

		/*
		 * Note: heap_insert returns the tid (location) of the new tuple
		 * in the t_self field.
		 */
		newId = heap_insert(resultRelationDesc,
							tuple,
							cid, 0, NULL,
							GetCurrentTransactionId());
		*lastTid = tuple->t_self;
	}
	else
	{
		elog(ERROR, "invalid relation type");
	}

	return newId;
}

static HTSU_Result
table_update(ResultRelInfo *resultRelInfo, EState *estate, ItemPointer tupleid, TupleTableSlot *slot,
			 CommandId cid,
			 Snapshot crosscheck_snapshot,
			 bool wait,
			 HeapUpdateFailureData *hufd, LockTupleMode *lockmode, bool *wasHotUpdate)
{
	Relation	resultRelationDesc = resultRelInfo->ri_RelationDesc;
	HTSU_Result result;

	if (RelationIsHeap(resultRelationDesc))
	{
		HeapTuple tuple;

		tuple = ExecMaterializeSlot(slot);

		result = heap_update(resultRelationDesc, tupleid, tuple,
							 cid,
							 crosscheck_snapshot,
							 wait,
							 hufd, lockmode);

		*tupleid = tuple->t_self;
		*wasHotUpdate = HeapTupleIsHeapOnly(tuple) != 0;
	}
	else if (RelationIsAoRows(resultRelationDesc))
	{
		MemTuple mtuple;
		ItemPointerData lastTid;

		if (IsolationUsesXactSnapshot())
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("updates on append-only tables are not supported in serializable transactions")));
		}

		if (resultRelInfo->ri_updateDesc == NULL)
		{
			ResultRelInfoChooseSegno(resultRelInfo);
			resultRelInfo->ri_updateDesc = (AppendOnlyUpdateDesc)
				appendonly_update_init(resultRelationDesc, GetActiveSnapshot(),
									   resultRelInfo->ri_aosegno);
		}

		mtuple = ExecFetchSlotMemTuple(slot);
		result = appendonly_update(resultRelInfo->ri_updateDesc,
								   mtuple, (AOTupleId *) tupleid, (AOTupleId *) &lastTid);
		*tupleid = lastTid;
		*wasHotUpdate = false;
	}
	else if (RelationIsAoCols(resultRelationDesc))
	{
		ItemPointerData lastTid;

		if (IsolationUsesXactSnapshot())
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("updates on append-only tables are not supported in serializable transactions")));
		}

		if (resultRelInfo->ri_updateDesc == NULL)
		{
			ResultRelInfoChooseSegno(resultRelInfo);
			resultRelInfo->ri_updateDesc = (AppendOnlyUpdateDesc)
				aocs_update_init(resultRelationDesc, resultRelInfo->ri_aosegno);
		}

		result = aocs_update(resultRelInfo->ri_updateDesc,
							 slot, (AOTupleId *) tupleid, (AOTupleId *) &lastTid);
		*tupleid = lastTid;
		*wasHotUpdate = false;
	}
	else
	{
		elog(ERROR, "invalid relation type");
	}

	return result;
}

static HTSU_Result
table_delete(ResultRelInfo *resultRelInfo, EState *estate, bool isUpdate,
			 ItemPointer tupleid,
			 CommandId cid,
			 Snapshot crosscheck_snapshot,
			 bool wait,
			 HeapUpdateFailureData *hufd)
{
	Relation	resultRelationDesc = resultRelInfo->ri_RelationDesc;
	HTSU_Result result;

	if (RelationIsHeap(resultRelationDesc))
	{
		result = heap_delete(resultRelationDesc, tupleid,
							 cid,
							 crosscheck_snapshot,
							 wait,
							 hufd);
	}
	else if (RelationIsAoRows(resultRelationDesc))
	{
		if (IsolationUsesXactSnapshot())
		{
			if (!isUpdate)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("deletes on append-only tables are not supported in serializable transactions")));
			else
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("updates on append-only tables are not supported in serializable transactions")));
		}

		if (resultRelInfo->ri_deleteDesc == NULL)
		{
			resultRelInfo->ri_deleteDesc =
				appendonly_delete_init(resultRelationDesc, GetActiveSnapshot());
		}

		result = appendonly_delete(resultRelInfo->ri_deleteDesc, (AOTupleId *) tupleid);
	}
	else if (RelationIsAoCols(resultRelationDesc))
	{
		if (IsolationUsesXactSnapshot())
		{
			if (!isUpdate)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("deletes on append-only tables are not supported in serializable transactions")));
			else
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("updates on append-only tables are not supported in serializable transactions")));
		}

		if (resultRelInfo->ri_deleteDesc == NULL)
		{
			resultRelInfo->ri_deleteDesc =
				aocs_delete_init(resultRelationDesc);
		}

		AOTupleId *aoTupleId = (AOTupleId *) tupleid;
		result = aocs_delete(resultRelInfo->ri_deleteDesc, aoTupleId);
	}
	else
	{
		elog(ERROR, "invalid relation type");
	}

	return result;
}
