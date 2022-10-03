/*-------------------------------------------------------------------------
 *
 * nodeWorktablescan.c
 *	  routines to handle WorkTableScan nodes.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeWorktablescan.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "executor/execdebug.h"
#include "executor/nodeWorktablescan.h"

static TupleTableSlot *WorkTableScanNext(WorkTableScanState *node);

/* ----------------------------------------------------------------
 *		WorkTableScanNext
 *
 *		This is a workhorse for ExecWorkTableScan
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
WorkTableScanNext(WorkTableScanState *node)
{
	TupleTableSlot *slot;
	Tuplestorestate *tuplestorestate;

	/*
	 * get information from the estate and scan state
	 *
	 * Note: we intentionally do not support backward scan.  Although it would
	 * take only a couple more lines here, it would force nodeRecursiveunion.c
	 * to create the tuplestore with backward scan enabled, which has a
	 * performance cost.  In practice backward scan is never useful for a
	 * worktable plan node, since it cannot appear high enough in the plan
	 * tree of a scrollable cursor to be exposed to a backward-scan
	 * requirement.  So it's not worth expending effort to support it.
	 *
	 * Note: we are also assuming that this node is the only reader of the
	 * worktable.  Therefore, we don't need a private read pointer for the
	 * tuplestore, nor do we need to tell tuplestore_gettupleslot to copy.
	 */

	/*
	 * RECURSIVE_CTE_FIXME: Double check we don't have backward scan required by
	 * plan (both planner and ORCA).
	 */
	Assert(ScanDirectionIsForward(node->ss.ps.state->es_direction));

	tuplestorestate = node->rustate->working_table;
	tuplestore_select_read_pointer(tuplestorestate, node->readptr);

	/*
	 * Get the next tuple from tuplestore. Return NULL if no more tuples.
	 */
	slot = node->ss.ss_ScanTupleSlot;
	(void) tuplestore_gettupleslot(tuplestorestate, true, false, slot);
	return slot;
}

/*
 * WorkTableScanRecheck -- access method routine to recheck a tuple in EvalPlanQual
 */
static bool
WorkTableScanRecheck(WorkTableScanState *node, TupleTableSlot *slot)
{
	/* nothing to check */
	return true;
}

/* ----------------------------------------------------------------
 *		ExecWorkTableScan(node)
 *
 *		Scans the worktable sequentially and returns the next qualifying tuple.
 *		We call the ExecScan() routine and pass it the appropriate
 *		access method functions.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecWorkTableScan(PlanState *pstate)
{
	WorkTableScanState *node = castNode(WorkTableScanState, pstate);

	return ExecScan(&node->ss,
					(ExecScanAccessMtd) WorkTableScanNext,
					(ExecScanRecheckMtd) WorkTableScanRecheck);
}


/* ----------------------------------------------------------------
 *		ExecInitWorkTableScan
 * ----------------------------------------------------------------
 */
WorkTableScanState *
ExecInitWorkTableScan(WorkTableScan *node, EState *estate, int eflags)
{
	WorkTableScanState *scanstate;
	ParamExecData *param;

	/* check for unsupported flags */
	/*
	 * RECURSIVE_CTE_FIXME: Make sure we don't require EXEC_FLAG_BACKWARD
	 * in GPDB.
	 */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	/*
	 * WorkTableScan should not have any children.
	 */
	Assert(outerPlan(node) == NULL);
	Assert(innerPlan(node) == NULL);

	/*
	 * create new WorkTableScanState for node
	 */
	scanstate = makeNode(WorkTableScanState);
	scanstate->ss.ps.plan = (Plan *) node;
	scanstate->ss.ps.state = estate;
	scanstate->ss.ps.ExecProcNode = ExecWorkTableScan;

	/* In postgres, it can generate CTE SubPlans for WITH subqueries, if a CTE subplan have outer
	 * recursive refs with outer recursive CTE, it will exist a WorkTable in subplan. And initialize
	 * state information for subplans will be before initialize on the main query tree, so there are
	 * corner cases where we'll get the init call before the RecursiveUnion does.
	 * IN GPDB, we don't have CTE scan node, so we won't generate CTE subplan for WITH subqueries,
	 * also we won't call the ExecInitWorkTableScan func before ExecInitRecursiveUnion. Set rustate
	 * in the INIT step.
	 */
	param = &(estate->es_param_exec_vals[node->wtParam]);
	Assert(param->execPlan == NULL);
	Assert(!param->isnull);
	scanstate->rustate = castNode(RecursiveUnionState, DatumGetPointer(param->value));
	if (scanstate->rustate->refcount == 0)
		scanstate->readptr = 0;
	else
	{
		/* during node init, the work table hasn't been scanned yet, it must be at start, don't need to rescan here*/
		scanstate->readptr = tuplestore_alloc_read_pointer(scanstate->rustate->working_table, EXEC_FLAG_REWIND);
	}
	scanstate->rustate->refcount++;

	/*
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &scanstate->ss.ps);

	/*
	 * tuple table initialization
	 */
	ExecInitResultTypeTL(&scanstate->ss.ps);

	/* signal that return type is not yet known */
	scanstate->ss.ps.resultopsset = true;
	scanstate->ss.ps.resultopsfixed = false;

	/* The scan tuple type (ie, the rowtype we expect to find in the work
	 * table) is the same as the result rowtype of the ancestor
	 * RecursiveUnion node.  Note this depends on the assumption that
	 * RecursiveUnion doesn't allow projection.
	 */
	ExecInitScanTupleSlot(estate, &scanstate->ss, ExecGetResultType(&scanstate->rustate->ps), &TTSOpsMinimalTuple);
	ExecAssignScanProjectionInfo(&scanstate->ss);

	/*
	 * initialize child expressions
	 */
	scanstate->ss.ps.qual =
		ExecInitQual(node->scan.plan.qual, (PlanState *) scanstate);

	/*
	 * Do not yet initialize projection info, see ExecWorkTableScan() for
	 * details.
	 */

	return scanstate;
}

/* ----------------------------------------------------------------
 *		ExecEndWorkTableScan
 *
 *		frees any storage allocated through C routines.
 * ----------------------------------------------------------------
 */
void
ExecEndWorkTableScan(WorkTableScanState *node)
{
	/*
	 * Free exprcontext
	 */
	ExecFreeExprContext(&node->ss.ps);

	/*
	 * clean out the tuple table
	 */
	if (node->ss.ps.ps_ResultTupleSlot)
		ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
	ExecClearTuple(node->ss.ss_ScanTupleSlot);
}

/* ----------------------------------------------------------------
 *		ExecReScanWorkTableScan
 *
 *		Rescans the relation.
 * ----------------------------------------------------------------
 */
void
ExecReScanWorkTableScan(WorkTableScanState *node)
{
	if (node->ss.ps.ps_ResultTupleSlot)
		ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);

	ExecScanReScan(&node->ss);

	/* No need (or way) to rescan if ExecWorkTableScan not called yet */
	if (node->rustate)
	{
		tuplestore_select_read_pointer(node->rustate->working_table, node->readptr);
		tuplestore_rescan(node->rustate->working_table);
	}
}
