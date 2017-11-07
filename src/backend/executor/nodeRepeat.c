/*-------------------------------------------------------------------------
 *
 * nodeRepeat.c
 *    Repeatly output each result tuple in the subplan with some defined
 *    number of counts.
 *
 * DESCRIPTION
 *
 *    Repeat nodes are used when input tuples need to be outputted several
 *    times. Different input tuple can be repeated different times.
 *
 *    For example, it is useful in grouping extension queries where the query
 *    contain duplicate grouping sets.
 *
 * Portions Copyright (c) 2008 - present, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeRepeat.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "cdb/cdbvars.h"
#include "executor/executor.h"
#include "executor/nodeRepeat.h"
#include "parser/parsetree.h"

static void init_RepeatState(RepeatState *repeatstate);

/*
 * Repeatly output each tuple received from the outer plan with some
 * defined number of times.  The number of times to output a tuple is
 * determined by the value of a given column in the received tuple.
 *
 * Note that the Repeat node also have the functionality to evaluate
 * the GroupingFunc.
 */
TupleTableSlot *
ExecRepeat(RepeatState *repeatstate)
{
	TupleTableSlot *outerslot;
	ExprContext *econtext = repeatstate->ps.ps_ExprContext;
	Repeat *node = (Repeat *)repeatstate->ps.plan;
		
	if (repeatstate->repeat_done)
		return NULL;

	/*
	 * If the previous tuple still needs to be outputted,
	 * output it here.
	 */
	if (repeatstate->slot != NULL)
	{
		if (repeatstate->repeat_count > 0)
		{
			/* Output the previous tuple */
			econtext->ecxt_outertuple = repeatstate->slot;
			econtext->ecxt_scantuple = repeatstate->slot;

			do
			{
				econtext->group_id = repeatstate->repeat_count - 1;
				econtext->grouping = node->grouping;
			
				repeatstate->repeat_count--;
				/* Check the qual until we find one output tuple. */
				if (ExecQual(repeatstate->ps.qual, econtext, false))
				{
					return ExecProject(repeatstate->ps.ps_ProjInfo, NULL);
				}
			} while (repeatstate->repeat_count > 0);
		}
		else
			repeatstate->slot = NULL;
	}

	ResetExprContext(econtext);

	while (!repeatstate->repeat_done)
	{
		MemoryContext oldcxt;
		bool isNull = false;
		
		outerslot = ExecProcNode(outerPlanState(repeatstate));
		if (TupIsNull(outerslot))
		{
			repeatstate->repeat_done = true;
			return NULL;
		}

		econtext->ecxt_outertuple = outerslot;
		econtext->ecxt_scantuple = outerslot;

		/* Compute the number of times to output this tuple. */
		oldcxt = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);
		repeatstate->repeat_count = 
			DatumGetInt32(ExecEvalExpr(repeatstate->expr_state, econtext,
									   &isNull, NULL));
		Assert(!isNull);
		MemoryContextSwitchTo(oldcxt);

		if (repeatstate->repeat_count == 0)
			continue;

		if (repeatstate->repeat_count > 1)
			repeatstate->slot = outerslot;
		
		do
		{
			econtext->group_id = repeatstate->repeat_count - 1;
			econtext->grouping = node->grouping;
			
			repeatstate->repeat_count--;

			/* Check the qual until we find one output tuple. */
			if (ExecQual(repeatstate->ps.qual, econtext, false))
			{
				return ExecProject(repeatstate->ps.ps_ProjInfo, NULL);
			}
		} while (repeatstate->repeat_count > 0);
	}

	return NULL;
}

RepeatState *
ExecInitRepeat(Repeat *node, EState *estate, int eflags)
{
	RepeatState *repeatstate;

	/* Check for unsupported flag */
	Assert(!(eflags & (EXEC_FLAG_MARK | EXEC_FLAG_BACKWARD)) ||
		   outerPlan(node) != NULL);

	/*
	 * Create state structure.
	 */
	repeatstate = makeNode(RepeatState);
	repeatstate->ps.plan = (Plan *)node;
	repeatstate->ps.state = estate;

	/* Create expression context for the node. */
	ExecAssignExprContext(estate, &repeatstate->ps);
	
	ExecInitResultTupleSlot(estate, &repeatstate->ps);
	
	/* Initialize child expressions */
	repeatstate->ps.targetlist = (List *)
		ExecInitExpr((Expr *)node->plan.targetlist,
					 (PlanState *)repeatstate);
	repeatstate->ps.qual = (List *)
		ExecInitExpr((Expr *)node->plan.qual,
					 (PlanState *)repeatstate);
	repeatstate->expr_state =
		ExecInitExpr(node->repeatCountExpr,
					 (PlanState *)repeatstate);

	/* Initialize child nodes */
	outerPlanState(repeatstate) = ExecInitNode(outerPlan(node), estate, eflags);

	/* Inner plan is not used. */
	Assert(innerPlan(node) == NULL);
	
	/* Initialize tuple type and projection info */
	ExecAssignResultTypeFromTL(&repeatstate->ps);
	ExecAssignProjectionInfo(&repeatstate->ps, NULL);

	init_RepeatState(repeatstate);

	return repeatstate;
}

int
ExecCountSlotsRepeat(Repeat *node)
{
	return ExecCountSlotsNode(outerPlan(node)) + 1;
}

void
ExecEndRepeat(RepeatState *node)
{
	/* Free the ExprContext */
	ExecFreeExprContext(&node->ps);
	
	/* Clean out the tuple table */
	ExecClearTuple(node->ps.ps_ResultTupleSlot);
	
	/* End the subplans */
	ExecEndNode(outerPlanState(node));
	
	EndPlanStateGpmonPkt(&node->ps);
}

void
ExecReScanRepeat(RepeatState *node, ExprContext *exprCtxt)
{
	/* Clean out the tuple table */
	ExecClearTuple(node->ps.ps_ResultTupleSlot);

	init_RepeatState(node);
	
	/*
	 * if chgParam of subnode is not null then plan will be re-scanned by
	 * first ExecProcNode.
	 */
	if (((PlanState *) node)->lefttree->chgParam == NULL)
		ExecReScan(((PlanState *) node)->lefttree, exprCtxt);
}

/*
 * init_RepeatState() -- initialize the RepeatState.
 */
static void
init_RepeatState(RepeatState *repeatstate)
{
	repeatstate->repeat_done = false;
	repeatstate->slot = NULL;
	repeatstate->repeat_count = 0;
}
