/*-------------------------------------------------------------------------
 *
 * nodeDML.c
 *	  Implementation of nodeDML.
 *
 * Portions Copyright (c) 2012, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeDML.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"

#include "cdb/cdbpartition.h"
#include "cdb/cdbvars.h"
#include "commands/tablecmds.h"
#include "executor/execDML.h"
#include "executor/instrument.h"
#include "executor/nodeDML.h"
#include "utils/memutils.h"

/* DML default memory */
#define DML_MEM 1

/*
 * Estimated Memory Usage of DML Node.
 * */
void
ExecDMLExplainEnd(PlanState *planstate, struct StringInfoData *buf)
{
	planstate->instrument->execmemused += DML_MEM;
}

/*
 * Executes INSERT and DELETE DML operations. The
 * action is specified within the TupleTableSlot at
 * plannode->actionColIdx.The ctid of the tuple to delete
 * is in position plannode->ctidColIdx in the current slot.
 * */
TupleTableSlot*
ExecDML(DMLState *node)
{

	PlanState *outerNode = outerPlanState(node);
	DML *plannode = (DML *) node->ps.plan;

	Assert(outerNode != NULL);

	TupleTableSlot *slot = ExecProcNode(outerNode);

	if (TupIsNull(slot))
	{
		return NULL;
	}

	bool isnull = false;
	int action = DatumGetUInt32(slot_getattr(slot, plannode->actionColIdx, &isnull));
	Assert(!isnull);

	bool isUpdate = false;
	if (node->ps.state->es_plannedstmt->commandType == CMD_UPDATE)
	{
		isUpdate = true;
	}

	Assert(action == DML_INSERT || action == DML_DELETE);


	/*
	 * Reset per-tuple memory context to free any expression evaluation
	 * storage allocated in the previous tuple cycle.
	 */
	ExprContext *econtext = node->ps.ps_ExprContext;
	ResetExprContext(econtext);

	/* Prepare cleaned-up tuple by projecting it and filtering junk columns */
	econtext->ecxt_outertuple = slot;
	TupleTableSlot *projectedSlot = ExecProject(node->ps.ps_ProjInfo, NULL);

	/* remove 'junk' columns from tuple */
	node->cleanedUpSlot = ExecFilterJunk(node->junkfilter, projectedSlot);

	/* GPDB_91_MERGE_FIXME:
	 * This kind of node is used by ORCA only. If in the future ORCA still uses
	 * DML node, canSetTag should be saved in DML plan node and init-ed by
	 * copying canSetTag value from the parse tree.
	 *
	 * For !isUpdate case, ExecInsert() and ExecDelete() should use canSetTag
	 * value from parse tree, however for isUpdate case, it seems that
	 * ExecInsert() is run after ExecDelete() so canSetTag should be set
	 * properly in ExecInsert().
	 */
	if (DML_INSERT == action)
	{
		/* Respect any given tuple Oid when updating a tuple. */
		if (isUpdate && plannode->tupleoidColIdx != 0)
		{
			Oid			oid;
			HeapTuple	htuple;

			isnull = false;
			oid = slot_getattr(slot, plannode->tupleoidColIdx, &isnull);
			htuple = ExecFetchSlotHeapTuple(node->cleanedUpSlot);
			Assert(htuple == node->cleanedUpSlot->PRIVATE_tts_heaptuple);
			HeapTupleSetOid(htuple, oid);
		}

		/*
		 * The plan origin is required since ExecInsert performs different
		 * actions depending on the type of plan (constraint enforcement and
		 * triggers.)
		 */
		ExecInsert(node->cleanedUpSlot,
				   NULL,
				   node->ps.state,
				   true, /* GPDB_91_MERGE_FIXME: canSetTag, where to get this? */
				   PLANGEN_OPTIMIZER /* Plan origin */,
				   isUpdate,
				   InvalidOid);
	}
	else /* DML_DELETE */
	{
		Datum ctid = slot_getattr(slot, plannode->ctidColIdx, &isnull);

		Assert(!isnull);

		ItemPointer  tupleid = (ItemPointer) DatumGetPointer(ctid);
		ItemPointerData tuple_ctid = *tupleid;
		tupleid = &tuple_ctid;

		/*
		 * Sanity check the distribution of the tuple to prevent potential
		 * data corruption in case users manipulate data incorrectly (e.g.
		 * insert data on incorrect segments through utility mode) or there is
		 * bug in code, etc.
		 */
		if (AttributeNumberIsValid(node->segid_attno))
		{
			int32 segid = DatumGetInt32(slot_getattr(slot, node->segid_attno, &isnull));
			Assert(!isnull);

			if (segid != GpIdentity.segindex)
				elog(ERROR, "distribution key of the tuple doesn't belong to "
					 "current segment (actually from seg%d)", segid);
		}

		/* Correct tuple count by ignoring deletes when splitting tuples. */
		ExecDelete(tupleid,
				   NULL, /* GPDB_91_MERGE_FIXME: oldTuple? */
				   node->cleanedUpSlot,
				   NULL /* DestReceiver */,
				   node->ps.state,
				   !isUpdate, /* GPDB_91_MERGE_FIXME: where to get canSetTag? */
				   PLANGEN_OPTIMIZER /* Plan origin */,
				   isUpdate);
	}

	return slot;
}

/**
 * Init nodeDML, which initializes the insert TupleTableSlot.
 * */
DMLState*
ExecInitDML(DML *node, EState *estate, int eflags)
{
	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK | EXEC_FLAG_REWIND)));

	DMLState *dmlstate = makeNode(DMLState);
	dmlstate->ps.plan = (Plan *)node;
	dmlstate->ps.state = estate;
	/*
	 * Initialize es_result_relation_info, just like ModifyTable.
	 * GPDB_90_MERGE_FIXME: do we need to consolidate the ModifyTable and DML
	 * logic?
	 */
	estate->es_result_relation_info = estate->es_result_relations;

	CmdType operation = estate->es_plannedstmt->commandType;
	ResultRelInfo *resultRelInfo = estate->es_result_relation_info;

	ExecInitResultTupleSlot(estate, &dmlstate->ps);

	dmlstate->ps.targetlist = (List *)
						ExecInitExpr((Expr *) node->plan.targetlist,
						(PlanState *) dmlstate);

	Plan *outerPlan  = outerPlan(node);
	outerPlanState(dmlstate) = ExecInitNode(outerPlan, estate, eflags);

	/*
	 * ORCA Plan does not seem to set junk attribute for "gp_segment_id", else we
	 * could call the simple code below.
	 * resultRelInfo->ri_segid_attno = ExecFindJunkAttributeInTlist(outerPlanState(dmlstate)->plan->targetlist, "gp_segment_id");
	 */
	ListCell   *t;
	dmlstate->segid_attno = InvalidAttrNumber;
	foreach(t, outerPlanState(dmlstate)->plan->targetlist)
	{
		TargetEntry *tle = lfirst(t);

		if (tle->resname && (strcmp(tle->resname, "gp_segment_id") == 0))
		{
			dmlstate->segid_attno = tle->resno;
			break;
		}
	}

	ExecAssignResultTypeFromTL(&dmlstate->ps);

	/* Create expression evaluation context. This will be used for projections */
	ExecAssignExprContext(estate, &dmlstate->ps);

	/*
	 * Create projection info from the child tuple descriptor and our target list
	 * Projection will be placed in the ResultSlot
	 */
	TupleTableSlot *childResultSlot = outerPlanState(dmlstate)->ps_ResultTupleSlot;
	ExecAssignProjectionInfo(&dmlstate->ps, childResultSlot->tts_tupleDescriptor);

	/*
	 * Initialize slot to insert/delete using output relation descriptor.
	 */
	dmlstate->cleanedUpSlot = ExecInitExtraTupleSlot(estate);

	/*
	 * Both input and output of the junk filter include dropped attributes, so
	 * the junk filter doesn't need to do anything special there about them
	 */

	dmlstate->junkfilter = ExecInitJunkFilter(node->plan.targetlist,
			dmlstate->ps.state->es_result_relation_info->ri_RelationDesc->rd_att->tdhasoid,
			dmlstate->cleanedUpSlot);

	/*
	 * We don't maintain typmod in the targetlist, so we should fixup the
	 * junkfilter to use the same tuple descriptor as the result relation.
	 * Otherwise the mismatch of tuple descriptor will cause a break in
	 * ExecInsert()->reconstructMatchingTupleSlot().
	 */
	TupleDesc cleanTupType = CreateTupleDescCopy(dmlstate->ps.state->es_result_relation_info->ri_RelationDesc->rd_att);

	ExecSetSlotDescriptor(dmlstate->junkfilter->jf_resultSlot, cleanTupType);

	ReleaseTupleDesc(dmlstate->junkfilter->jf_cleanTupType);
	dmlstate->junkfilter->jf_cleanTupType = cleanTupType;

	if (estate->es_instrument && (estate->es_instrument & INSTRUMENT_CDB))
	{
	        dmlstate->ps.cdbexplainbuf = makeStringInfo();

	        /* Request a callback at end of query. */
	        dmlstate->ps.cdbexplainfun = ExecDMLExplainEnd;
	}

	/*
	 * If there are indices on the result relation, open them and save
	 * descriptors in the result relation info, so that we can add new index
	 * entries for the tuples we add/update.  We need not do this for a
	 * DELETE, however, since deletion doesn't affect indexes.
	 */
	if (Gp_role != GP_ROLE_EXECUTE || Gp_is_writer) /* only needed by the root slice who will do the actual updating */
	{
		if (resultRelInfo->ri_RelationDesc->rd_rel->relhasindex  &&
			operation != CMD_DELETE)
		{
			ExecOpenIndices(resultRelInfo);
		}
	}

	return dmlstate;
}

/* Release Resources Requested by nodeDML. */
void
ExecEndDML(DMLState *node)
{
	/* Release explicitly the TupleDesc for result relation */
	ReleaseTupleDesc(node->junkfilter->jf_cleanTupType);

	ExecFreeExprContext(&node->ps);
	ExecClearTuple(node->ps.ps_ResultTupleSlot);
	ExecClearTuple(node->cleanedUpSlot);
	ExecEndNode(outerPlanState(node));
	EndPlanStateGpmonPkt(&node->ps);
}
/* EOF */
