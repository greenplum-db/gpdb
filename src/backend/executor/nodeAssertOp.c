/*-------------------------------------------------------------------------
 *
 * nodeAssertOp.c
 *	  Implementation of nodeAssertOp.
 *
 * Portions Copyright (c) 2012, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeAssertOp.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"

#include "cdb/cdbpartition.h"
#include "commands/tablecmds.h"
#include "executor/nodeAssertOp.h"
#include "executor/instrument.h"

/* Number of slots and memory used by node.*/
#define ASSERTOP_NSLOTS 1
#define ASSERTOP_MEM 	1

/*
 * Estimated Memory Usage of AssertOp Node.
 * */
void
ExecAssertOpExplainEnd(PlanState *planstate, struct StringInfoData *buf)
{
	planstate->instrument->execmemused += ASSERTOP_MEM;
}

/*
 * Check for assert violations and error out, if any.
 */
static void
CheckForAssertViolations(AssertOpState* node, TupleTableSlot* slot)
{
	AssertOp* plannode = (AssertOp*) node->ps.plan;
	ExprContext* econtext = node->ps.ps_ExprContext;
	ResetExprContext(econtext);
	List* predicates = node->ps.qual;
	/* Arrange for econtext's scan tuple to be the tuple under test */
	econtext->ecxt_outertuple = slot;
	/*
	 * Run in short-lived per-tuple context while computing expressions.
	 */
	MemoryContext oldContext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);
	StringInfoData errorString;
	initStringInfo(&errorString);

	ListCell *l = NULL;
	Assert(list_length(predicates) == list_length(plannode->errmessage));

	int violationCount = 0;
	int listIndex = 0;
	foreach(l, predicates)
	{
		ExprState *clause = (ExprState *) lfirst(l);
		bool isNull = false;
		Datum expr_value = ExecEvalExpr(clause, econtext, &isNull, NULL);

		if (!isNull && !DatumGetBool(expr_value))
		{
			Value *valErrorMessage = (Value*) list_nth(plannode->errmessage,
					listIndex);

			Assert(NULL != valErrorMessage && IsA(valErrorMessage, String) &&
					0 < strlen(strVal(valErrorMessage)));

			appendStringInfo(&errorString, "%s\n", strVal(valErrorMessage));
			violationCount++;
		}

		listIndex++;
	}

	if (0 < violationCount)
	{
		ereport(ERROR,
				(errcode(plannode->errcode),
				 errmsg("One or more assertions failed"),
				 errdetail("%s", errorString.data)));

	}
	pfree(errorString.data);
	MemoryContextSwitchTo(oldContext);
	ResetExprContext(econtext);
}

/*
 * Evaluate Constraints (in node->ps.qual) and project output TupleTableSlot.
 * */
TupleTableSlot*
ExecAssertOp(AssertOpState *node)
{
	PlanState *outerNode = outerPlanState(node);
	TupleTableSlot *slot = ExecProcNode(outerNode);

	if (TupIsNull(slot))
	{
		return NULL;
	}

	CheckForAssertViolations(node, slot);

	return ExecProject(node->ps.ps_ProjInfo, NULL);
}

/**
 * Init AssertOp, which sets the ProjectInfo and
 * the Constraints to evaluate.
 * */
AssertOpState*
ExecInitAssertOp(AssertOp *node, EState *estate, int eflags)
{

	/* Check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));
	Assert(outerPlan(node) != NULL);

	AssertOpState *assertOpState = makeNode(AssertOpState);
	assertOpState->ps.plan = (Plan *)node;
	assertOpState->ps.state = estate;
	PlanState *planState = &assertOpState->ps;


	ExecInitResultTupleSlot(estate, &assertOpState->ps);

	/* Create expression evaluation context */
	ExecAssignExprContext(estate, planState);

	assertOpState->ps.targetlist = (List *)
		ExecInitExpr((Expr *) node->plan.targetlist,
					 (PlanState *) assertOpState);

	assertOpState->ps.qual = (List *)
			ExecInitExpr((Expr *) node->plan.qual,
						 (PlanState *) assertOpState);

	/*
	 * Initialize outer plan
	 */
	Plan *outerPlan = outerPlan(node);
	outerPlanState(assertOpState) = ExecInitNode(outerPlan, estate, eflags);

	/*
	 * Initialize projection info for this
	 * node appropriately
	 */
	ExecAssignResultTypeFromTL(&assertOpState->ps);
	TupleDesc tupDesc = ExecTypeFromTL(node->plan.targetlist, false);

	ExecAssignProjectionInfo(planState, tupDesc);

	if (estate->es_instrument && (estate->es_instrument & INSTRUMENT_CDB))
	{
	        assertOpState->ps.cdbexplainbuf = makeStringInfo();

	        /* Request a callback at end of query. */
	        assertOpState->ps.cdbexplainfun = ExecAssertOpExplainEnd;
	}

	initGpmonPktForAssertOp((Plan *)node, &assertOpState->ps.gpmon_pkt, estate);

	return assertOpState;
}

/* Rescan AssertOp */
void
ExecReScanAssertOp(AssertOpState *node, ExprContext *exprCtxt)
{
	/*
	 * If chgParam of subnode is not null then plan will be re-scanned by
	 * first ExecProcNode.  However, if caller is passing us an exprCtxt
	 * then forcibly rescan the subnode now, so that we can pass the
	 * exprCtxt down to the subnode (needed for gated indexscan).
	 */
	if (node->ps.lefttree->chgParam == NULL || exprCtxt != NULL)
		ExecReScan(node->ps.lefttree, exprCtxt);
}

/* Release Resources Requested by AssertOp node. */
void
ExecEndAssertOp(AssertOpState *node)
{
	ExecFreeExprContext(&node->ps);
	ExecEndNode(outerPlanState(node));
	EndPlanStateGpmonPkt(&node->ps);
}

/* Return number of TupleTableSlots used by AssertOp node.*/
int
ExecCountSlotsAssertOp(AssertOp *node)
{
	return ExecCountSlotsNode(outerPlan(node)) + ASSERTOP_NSLOTS;
}

/* Tracing execution for GP Monitor. */
void
initGpmonPktForAssertOp(Plan *planNode, gpmon_packet_t *gpmon_pkt, EState *estate)
{
	Assert(planNode != NULL && gpmon_pkt != NULL && IsA(planNode, AssertOp));

	InitPlanNodeGpmonPkt(planNode, gpmon_pkt, estate);
}

