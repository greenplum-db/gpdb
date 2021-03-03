/*-------------------------------------------------------------------------
 *
 * nodeAssertOp.c
 *	  Implementation of nodeAssertOp.
 *
 * Portions Copyright (c) 2012, EMC Corp.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeAssertOp.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"

#include "commands/tablecmds.h"
#include "executor/nodeAssertOp.h"
#include "executor/instrument.h"
#include "utils/memutils.h"

/*
 * Check for assert violations and error out, if any.
 */
static void
CheckForAssertViolations(AssertOpState* node, TupleTableSlot* slot)
{
	AssertOp* plannode = (AssertOp*) node->ps.plan;
	ExprContext* econtext = node->ps.ps_ExprContext;
	ResetExprContext(econtext);
	// GPDB_12_MERGE_FIXME: Is this really always a list? Even after the merge of all
	// the JIT stuff?
	List *predicates = (List *) node->ps.qual;
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
		Datum expr_value = ExecEvalExpr(clause, econtext, &isNull);

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
				 errmsg("one or more assertions failed"),
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

	return ExecProject(node->ps.ps_ProjInfo);
}

/**
 * Init AssertOp, which sets the ProjectInfo and
 * the Constraints to evaluate.
 * */
AssertOpState*
ExecInitAssertOp(AssertOp *node, EState *estate, int eflags)
{
	AssertOpState *assertOpState;

	/* Check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));
	Assert(outerPlan(node) != NULL);

	assertOpState = makeNode(AssertOpState);
	assertOpState->ps.plan = (Plan *)node;
	assertOpState->ps.state = estate;

	/* Create expression evaluation context */
	ExecAssignExprContext(estate, &assertOpState->ps);

	/*
	 * Initialize outer plan
	 */
	Plan *outerPlan = outerPlan(node);
	outerPlanState(assertOpState) = ExecInitNode(outerPlan, estate, eflags);

	/*
	 * Initialize result type and projection.
	 */
	ExecInitResultTypeTL(&assertOpState->ps);
	ExecAssignProjectionInfo(&assertOpState->ps, NULL);

	assertOpState->ps.qual =
		ExecInitQual(node->plan.qual, (PlanState *) assertOpState);

	if (estate->es_instrument && (estate->es_instrument & INSTRUMENT_CDB))
	{
		assertOpState->ps.cdbexplainbuf = makeStringInfo();
	}

	return assertOpState;
}

/* Rescan AssertOp */
void
ExecReScanAssertOp(AssertOpState *node)
{
	/*
	 * If chgParam of subnode is not null then plan will be re-scanned by
	 * first ExecProcNode.
	 */
	if (node->ps.lefttree &&
		node->ps.lefttree->chgParam == NULL)
		ExecReScan(node->ps.lefttree);
}

/* Release Resources Requested by AssertOp node. */
void
ExecEndAssertOp(AssertOpState *node)
{
	ExecFreeExprContext(&node->ps);
	ExecEndNode(outerPlanState(node));
}
