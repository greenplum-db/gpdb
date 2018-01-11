/*-------------------------------------------------------------------------
 *
 * nodePartitionSelector.c
 *	  implement the execution of PartitionSelector for selecting partition
 *	  Oids based on a given set of predicates. It works for both constant
 *	  partition elimination and join partition elimination
 *
 * Copyright (c) 2014-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodePartitionSelector.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"

#include "cdb/cdbpartition.h"
#include "cdb/partitionselection.h"
#include "commands/tablecmds.h"
#include "executor/executor.h"
#include "executor/instrument.h"
#include "executor/nodePartitionSelector.h"
#include "nodes/makefuncs.h"
#include "utils/guc.h"
#include "utils/memutils.h"

static void LogPartitionSelection(EState *estate, int32 selectorId);

static void
partition_propagation(EState *estate, List *partOids, List *scanIds, int32 selectorId);

/* PartitionSelector Slots */
#define PARTITIONSELECTOR_NSLOTS 1

/* Return number of TupleTableSlots used by nodePartitionSelector.*/
int
ExecCountSlotsPartitionSelector(PartitionSelector *node)
{
	if (NULL != outerPlan(node))
	{
		return ExecCountSlotsNode(outerPlan(node)) + PARTITIONSELECTOR_NSLOTS;
	}
	return PARTITIONSELECTOR_NSLOTS;
}

void
initGpmonPktForPartitionSelector(Plan *planNode, gpmon_packet_t *gpmon_pkt, EState *estate)
{
	Assert(planNode != NULL && gpmon_pkt != NULL && IsA(planNode, PartitionSelector));

	InitPlanNodeGpmonPkt(planNode, gpmon_pkt, estate);
}

/* ----------------------------------------------------------------
 *		ExecInitPartitionSelector
 *
 *		Create the run-time state information for PartitionSelector node
 *		produced by Orca and initializes outer child if exists.
 *
 * ----------------------------------------------------------------
 */
PartitionSelectorState *
ExecInitPartitionSelector(PartitionSelector *node, EState *estate, int eflags)
{
	/* check for unsupported flags */
	Assert (!(eflags & (EXEC_FLAG_MARK | EXEC_FLAG_BACKWARD)));

	PartitionSelectorState *psstate = initPartitionSelection(node, estate);

	/* tuple table initialization */
	ExecInitResultTupleSlot(estate, &psstate->ps);
	ExecAssignResultTypeFromTL(&psstate->ps);
	ExecAssignProjectionInfo(&psstate->ps, NULL);

	/* initialize child nodes */
	/* No inner plan for PartitionSelector */
	Assert(NULL == innerPlan(node));
	if (NULL != outerPlan(node))
	{
		outerPlanState(psstate) = ExecInitNode(outerPlan(node), estate, eflags);
	}

	/*
	 * Initialize projection, to produce a tuple that has the partitioning key
	 * columns at the same positions as in the partitioned table.
	 */
	if (node->partTabTargetlist)
	{
		List	   *exprStates;

		exprStates = (List *) ExecInitExpr((Expr *) node->partTabTargetlist,
										   (PlanState *) psstate);

		psstate->partTabDesc = ExecTypeFromTL(node->partTabTargetlist, false);
		psstate->partTabSlot = MakeSingleTupleTableSlot(psstate->partTabDesc);
		psstate->partTabProj = ExecBuildProjectionInfo(exprStates,
													   psstate->ps.ps_ExprContext,
													   psstate->partTabSlot,
													   ExecGetResultType(&psstate->ps));
	}

	initGpmonPktForPartitionSelector((Plan *)node, &psstate->ps.gpmon_pkt, estate);

	return psstate;
}

/* ----------------------------------------------------------------
 *		ExecPartitionSelector(node)
 *
 *		Compute and propagate partition table Oids that will be
 *		used by Dynamic table scan. There are two ways of
 *		executing PartitionSelector.
 *
 *		1. Constant partition elimination
 *		Plan structure:
 *			Sequence
 *				|--PartitionSelector
 *				|--DynamicTableScan
 *		In this case, PartitionSelector evaluates constant partition
 *		constraints to compute and propagate partition table Oids.
 *		It only need to be called once.
 *
 *		2. Join partition elimination
 *		Plan structure:
 *			...:
 *				|--DynamicTableScan
 *				|--...
 *					|--PartitionSelector
 *						|--...
 *		In this case, PartitionSelector is in the same slice as
 *		DynamicTableScan, DynamicIndexScan or DynamicBitmapHeapScan.
 *		It is executed for each tuple coming from its child node.
 *		It evaluates partition constraints with the input tuple and
 *		propagate matched partition table Oids.
 *
 *
 * Instead of a Dynamic Table Scan, there can be other nodes that use
 * a PartSelected qual to filter rows, based on which partitions are
 * selected. Currently, ORCA uses Dynamic Table Scans, while plans
 * produced by the non-ORCA planner use gating Result nodes with
 * PartSelected quals, to exclude unwanted partitions.
 *
 * ----------------------------------------------------------------
 */
TupleTableSlot *
ExecPartitionSelector(PartitionSelectorState *node)
{
	PartitionSelector *ps = (PartitionSelector *) node->ps.plan;
	EState	   *estate = node->ps.state;
	ExprContext *econtext = node->ps.ps_ExprContext;
	TupleTableSlot *inputSlot = NULL;
	TupleTableSlot *candidateOutputSlot = NULL;

	if (ps->staticSelection)
	{
		/* propagate the part oids obtained via static partition selection */
		partition_propagation(estate, ps->staticPartOids, ps->staticScanIds, ps->selectorId);
		return NULL;
	}

	/* Retrieve PartitionNode and access method from root table.
	 * We cannot do it during node initialization as
	 * DynamicTableScanInfo is not properly initialized yet.
	 */
	if (NULL == node->rootPartitionNode)
	{
		Assert(NULL != estate->dynamicTableScanInfo);
		getPartitionNodeAndAccessMethod
									(
									ps->relid,
									estate->dynamicTableScanInfo->partsMetadata,
									estate->es_query_cxt,
									&node->rootPartitionNode,
									&node->accessMethods
									);
	}

	if (NULL != outerPlanState(node))
	{
		/* Join partition elimination */
		/* get tuple from outer children */
		PlanState *outerPlan = outerPlanState(node);
		Assert(outerPlan);
		inputSlot = ExecProcNode(outerPlan);

		if (TupIsNull(inputSlot))
		{
			/* no more tuples from outerPlan */

			/*
			 * Make sure we have an entry for this scan id in
			 * dynamicTableScanInfo. Normally, this would've been done the
			 * first time a partition is selected, but we must ensure that
			 * there is an entry even if no partitions were selected.
			 * (The traditional Postgres planner uses this method.)
			 */
			if (ps->partTabTargetlist)
				InsertPidIntoDynamicTableScanInfo(estate, ps->scanId, InvalidOid, ps->selectorId);
			else
				LogPartitionSelection(estate, ps->selectorId);

			return NULL;
		}
	}

	/* partition elimination with the given input tuple */
	ResetExprContext(econtext);
	econtext->ecxt_outertuple = inputSlot;
	econtext->ecxt_scantuple = inputSlot;

	if (NULL != inputSlot)
	{
		candidateOutputSlot = ExecProject(node->ps.ps_ProjInfo, NULL);
	}

	/*
	 * If we have a partitioning projection, project the input tuple
	 * into a tuple that looks like tuples from the partitioned table, and use
	 * selectPartitionMulti() to select the partitions. (The traditional
	 * Postgres planner uses this method.)
	 */
	if (ps->partTabTargetlist)
	{
		TupleTableSlot *slot;
		List	   *oids;
		ListCell   *lc;

		slot = ExecProject(node->partTabProj, NULL);
		slot_getallattrs(slot);

		oids = selectPartitionMulti(node->rootPartitionNode,
									slot_get_values(slot),
									slot_get_isnull(slot),
									slot->tts_tupleDescriptor,
									node->accessMethods);
		foreach (lc, oids)
		{
			InsertPidIntoDynamicTableScanInfo(estate, ps->scanId, lfirst_oid(lc), ps->selectorId);
		}
	}
	else
	{
		/*
		 * Select the partitions based on levelEqExpressions and
		 * levelExpressions. (ORCA uses this method)
		 */
		SelectedParts *selparts = processLevel(node, 0 /* level */, inputSlot);

		/* partition propagation */
		if (NULL != ps->propagationExpression)
		{
			partition_propagation(estate, selparts->partOids, selparts->scanIds, ps->selectorId);
		}
		list_free(selparts->partOids);
		list_free(selparts->scanIds);
		pfree(selparts);
	}

	return candidateOutputSlot;
}

static void LogSelectedPartitionsForScan(int32 selectorId, HTAB *pidIndex, const int32 scanId);

void LogPartitionSelection(EState *estate, int32 selectorId)
{
	if (optimizer_partition_selection_log == false)
		return;

	DynamicTableScanInfo *dynamicTableScanInfo = estate->dynamicTableScanInfo;

	Assert(dynamicTableScanInfo != NULL);

	HTAB **pidIndexes = dynamicTableScanInfo->pidIndexes;

	for (int32 scanIdMinusOne = 0; scanIdMinusOne < dynamicTableScanInfo->numScans; ++scanIdMinusOne)
	{
		HTAB *const pidIndex = pidIndexes[scanIdMinusOne];
		if (pidIndex == NULL)
			continue;
		const int32 scanId = scanIdMinusOne + 1;

		LogSelectedPartitionsForScan(selectorId, pidIndex, scanId);
	}
}

void LogSelectedPartitionsForScan(int32 selectorId, HTAB *pidIndex, const int32 scanId)
{
	int32 numPartitionsSelected = 0;
	Datum *selectedPartOids = palloc(sizeof(Datum) * hash_get_num_entries(pidIndex));

	HASH_SEQ_STATUS status;
	PartOidEntry *partOidEntry;
	hash_seq_init(&status, pidIndex);

	while ((partOidEntry = hash_seq_search(&status)) != NULL)
	{
		if (list_member_int(partOidEntry->selectorList, selectorId))
			selectedPartOids[numPartitionsSelected++] = ObjectIdGetDatum(partOidEntry->partOid);
	}

	if (numPartitionsSelected > 0)
	{
		char *debugPartitionOid = DebugPartitionOid(selectedPartOids, numPartitionsSelected);
		ereport(LOG,
				(errmsg_internal("scanId: %d, partitions: %s, selector: %d",
								 scanId,
								 debugPartitionOid,
								 selectorId)));
		pfree(debugPartitionOid);
	}

	pfree(selectedPartOids);
}

/* ----------------------------------------------------------------
 *		ExecReScanPartitionSelector(node)
 *
 *		ExecReScan routine for PartitionSelector.
 * ----------------------------------------------------------------
 */
void
ExecReScanPartitionSelector(PartitionSelectorState *node, ExprContext *exprCtxt)
{
	/* reset PartitionSelectorState */
	PartitionSelector *ps = (PartitionSelector *) node->ps.plan;

	for(int iter = 0; iter < ps->nLevels; iter++)
	{
		node->levelPartRules[iter] = NULL;
	}

	/* free result tuple slot */
	ExecClearTuple(node->ps.ps_ResultTupleSlot);

	/*
	 * If we are being passed an outer tuple, link it into the "regular"
	 * per-tuple econtext for possible qual eval.
	 */
	if (exprCtxt != NULL)
	{
		ExprContext *stdecontext = node->ps.ps_ExprContext;
		stdecontext->ecxt_outertuple = exprCtxt->ecxt_outertuple;
	}

	/* If the PartitionSelector is in the inner side of a nest loop join,
	 * it should be constant partition elimination and thus has no child node.*/
#if USE_ASSERT_CHECKING
	PlanState  *outerPlan = outerPlanState(node);
	Assert (NULL == outerPlan);
#endif

}

/* ----------------------------------------------------------------
 *		ExecEndPartitionSelector(node)
 *
 *		ExecEnd routine for PartitionSelector. Free resources
 *		and clear tuple.
 *
 * ----------------------------------------------------------------
 */
void
ExecEndPartitionSelector(PartitionSelectorState *node)
{
	ExecFreeExprContext(&node->ps);

	ExecClearTuple(node->ps.ps_ResultTupleSlot);

	/* clean child node */
	if (NULL != outerPlanState(node))
	{
		ExecEndNode(outerPlanState(node));
	}

	EndPlanStateGpmonPkt(&node->ps);
}

/* ----------------------------------------------------------------
 *		partition_propagation
 *
 *		Propagate a list of leaf part Oids to the corresponding dynamic scans
 *
 * ----------------------------------------------------------------
 */
static void
partition_propagation(EState *estate, List *partOids, List *scanIds, int32 selectorId)
{
	Assert (list_length(partOids) == list_length(scanIds));

	ListCell *lcOid = NULL;
	ListCell *lcScanId = NULL;
	forboth (lcOid, partOids, lcScanId, scanIds)
	{
		Oid partOid = lfirst_oid(lcOid);
		int scanId = lfirst_int(lcScanId);

		InsertPidIntoDynamicTableScanInfo(estate, scanId, partOid, selectorId);
	}
}

/* EOF */

