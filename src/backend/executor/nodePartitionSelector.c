/*-------------------------------------------------------------------------
 *
 * nodePartitionSelector.c
 *	  implement the execution of PartitionSelector for pruning partitions
 *	  based on rows seen by the inner side of a join.
 *
 * For example:
 *
 * explain (costs off, timing off, analyze)
 *   select * from t, pt where tid = ptid;
 *
 *                                    QUERY PLAN                                    
 * ---------------------------------------------------------------------------------
 *  Gather Motion 3:1  (slice1; segments: 3) (actual rows=5 loops=1)
 *    ->  Hash Join (actual rows=2 loops=1)
 *          Hash Cond: ((pt1.ptid = t.tid) AND (pt1.dist = t.dist))
 *          ->  Append (actual rows=36 loops=1)
 *                Partition Selectors: $0
 *                ->  Seq Scan on pt1 (actual rows=1 loops=1)
 *                ->  Seq Scan on pt2 (actual rows=1 loops=1)
 *                ->  Seq Scan on pt3 (never executed)
 *                ->  Seq Scan on pt4 (never executed)
 *                ->  Seq Scan on pt5 (actual rows=1 loops=1)
 *                ->  Seq Scan on ptdefault (actual rows=35 loops=1)
 *          ->  Hash (actual rows=37 loops=1)
 *                Buckets: 524288  Batches: 1  Memory Usage: 4098kB
 *                ->  Partition Selector (selector id: $0) (actual rows=37 loops=1)
 *                      ->  Seq Scan on t (actual rows=37 loops=1)
 * (15 rows)
 *
 * In this example, the 't' table is scanned first, and the Hash table is
 * built. All the rows from 't' are also passed through the Partition Selector
 * node. For each row, the Partition Selector computes the corresponding
 * partition in the 'pt' table. Based on the rows seen, the Append node can
 * skip partitions that cannot contain any matching rows.
 *
 * The Partition Selector has a PartitionPruneInfo struct that contains
 * the logic used to compute the matching partition for each input row.
 * In PostgreSQL, the partition pruning is performed entirely based on
 * constants (at planning time) or on constants and Params (run-time
 * pruning). The pruning steps used in Partition Selectors can also
 * contain Vars referring to the columns of the outer side of the join
 * that are available at the Partition Selector.
 *
 * The Partition Selector performs the pruning and stores the result in a
 * special executor Param to make it available to the Append node. When doing
 * join pruning using a Partition Selector, the Append node doesn't perform
 * the pruning steps, but uses the pre-computed result Bitmapset. (A mix of
 * upstream-style pruning based on Params, an join pruning using a Partition
 * Seletor, is possible however).
 *
 * Copyright (c) 2014-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodePartitionSelector.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "executor/executor.h"
#include "executor/execPartition.h"
#include "executor/nodePartitionSelector.h"

static TupleTableSlot *ExecPartitionSelector(PlanState *pstate);

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
	PartitionSelectorState *psstate;

	/* check for unsupported flags */
	Assert (!(eflags & (EXEC_FLAG_MARK | EXEC_FLAG_BACKWARD)));

	psstate = makeNode(PartitionSelectorState);
	psstate->ps.plan = (Plan *) node;
	psstate->ps.state = estate;

	/* ExprContext initialization */
	ExecAssignExprContext(estate, &psstate->ps);

	psstate->ps.ExecProcNode = ExecPartitionSelector;

	/* tuple table initialization */
	ExecInitResultTypeTL(&psstate->ps);
	ExecAssignProjectionInfo(&psstate->ps, NULL);

	/*
	 * initialize outer plan
	 */
	outerPlanState(psstate) = ExecInitNode(outerPlan(node), estate, eflags);

	/* Create the working data structure for pruning. */
	psstate->prune_state = ExecCreatePartitionPruneState(&psstate->ps,
														 node->part_prune_info);

	return psstate;
}

/* ----------------------------------------------------------------
 *		ExecPartitionSelector(node)
 *
 * Pass-through rows, but for each row, compute which partitions
 * on other side of a join above can contain rows that match
 * the join quals.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecPartitionSelector(PlanState *pstate)
{
	PartitionSelectorState *node = (PartitionSelectorState *) pstate;
	PartitionSelector *ps = (PartitionSelector *) node->ps.plan;
	EState	   *estate = node->ps.state;
	ExprContext *econtext = node->ps.ps_ExprContext;
	PlanState *outerPlan = outerPlanState(node);
	TupleTableSlot *inputSlot;

	/* get tuple from child */
	Assert(outerPlan);
	inputSlot = ExecProcNode(outerPlan);

	if (TupIsNull(inputSlot))
	{
		/*
		 * No more tuples from outerPlan. The set of surviving partitions
		 * is now complete. Make the result available to the Append node
		 * by storing it in the special executor Param.
		 */
		ParamExecData *param;

		param = &(estate->es_param_exec_vals[ps->paramid]);
		Assert(param->execPlan == NULL);
		Assert(!param->isnull);
		param->value = PointerGetDatum(node);

		return NULL;
	}

	/*
	 * Run the partition pruning logic with this tuple, and accumulate
	 * the partitions matching this tuple to the result.
	 */
	ResetExprContext(econtext);
	econtext->ecxt_outertuple = inputSlot;
	node->part_prune_result = ExecAddMatchingSubPlans(node->prune_state,
													  node->part_prune_result);

	return inputSlot;
}

/* ----------------------------------------------------------------
 *		ExecReScanPartitionSelector(node)
 *
 *		ExecReScan routine for PartitionSelector.
 * ----------------------------------------------------------------
 */
void
ExecReScanPartitionSelector(PartitionSelectorState *node)
{
}

/* ----------------------------------------------------------------
 *		ExecEndPartitionSelector(node)
 *
 *		ExecEnd routine for PartitionSelector. Free resources
 *		and clear tuple.
 * ----------------------------------------------------------------
 */
void
ExecEndPartitionSelector(PartitionSelectorState *node)
{
	ExecFreeExprContext(&node->ps);

	/* clean child node */
	ExecEndNode(outerPlanState(node));
}

/* EOF */
