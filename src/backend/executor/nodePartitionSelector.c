/*-------------------------------------------------------------------------
 *
 * nodePartitionSelector.c
 *	  implement the execution of PartitionSelector for selecting partition
 *	  Oids based on a given set of predicates. It works for both constant
 *	  partition elimination and join partition elimination
 *
 * Copyright (c) 2014, Pivotal Inc.
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
#include "utils/memutils.h"

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

	InitPlanNodeGpmonPkt(planNode, gpmon_pkt, estate, PMNT_PartitionSelector,
							(int64)planNode->plan_rows,
							NULL);
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
		node->acceptedLeafOid = InvalidOid;
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
			return NULL;
		}
	}

	/* partition elimination with the given input tuple */
	ResetExprContext(econtext);
	node->ps.ps_OuterTupleSlot = inputSlot;
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
		if (oids != NIL)
		{
			foreach (lc, oids)
			{
				InsertPidIntoDynamicTableScanInfo(estate, ps->scanId, lfirst_oid(lc), ps->selectorId);
			}
		}
		else
		{
			/* no partitions matched. */
			InsertPidIntoDynamicTableScanInfo(estate, ps->scanId, InvalidOid, ps->selectorId);
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

	node->acceptedLeafOid = InvalidOid;
	return candidateOutputSlot;
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
	
	Assert (InvalidOid == node->acceptedLeafOid);
	
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

static bool
partition_selector_walker(Node *node, void *context)
{
	Node *outerNode = NULL;

	Assert(IsA(node, Material));

	outerNode = (Node *)((Plan *)node)->lefttree;
	if (outerNode == NULL)
	{
		return false;
	}
	return IsA(outerNode, PartitionSelector);
}

bool
contain_partition_selector(Node *node)
{
	return partition_selector_walker(node, NULL);
}

/* EOF */

