/*-------------------------------------------------------------------------
 *
 * nodeHashDummy.c
 *	  A dummy hash node that doesn't do any hashing
 *
 * Portions Copyright (c) 2015, Pivotal, Inc.
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		ExecInitHashDummy		creates and initializes state info.
 *		ExecHashDummy			scans a relation using bitmap info
 *		ExecHashDummyReScan		prepares to rescan the plan.
 *		ExecEndHashDummy		releases all storage.
 */
#include "postgres.h"

#include "access/heapam.h"
#include "access/relscan.h"
#include "access/transam.h"
#include "executor/execdebug.h"
#include "executor/nodeHashDummy.h"
#include "executor/nodeHash.h"
#include "pgstat.h"
#include "storage/bufmgr.h"
#include "utils/memutils.h"
#include "miscadmin.h"
#include "parser/parsetree.h"
#include "cdb/cdbvars.h" /* gp_select_invisible */
#include "cdb/cdbfilerepprimary.h"
#include "nodes/tidbitmap.h"
#include "cdb/cdbpartition.h"

/* For result tuple */
#define HASHDUMMY_NSLOTS 1

/*
 * Initializes the HushDummy executor state
 */
HashDummyState *
ExecInitHashDummy(HashDummy *node, EState *estate, int eflags)
{
	HashDummyState *state = makeNode(HashDummyState);

	state->ps.plan = (Plan *) node;
	state->ps.state = estate;

	/*
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &state->ps);

	/*
	 * initialize our result slot
	 */
	ExecInitResultTupleSlot(estate, &state->ps);

	/*
	 * initialize child expressions
	 */
	state->ps.targetlist = (List *)
		ExecInitExpr((Expr *) node->plan.targetlist,
					 (PlanState *) state);
	state->ps.qual = (List *)
		ExecInitExpr((Expr *) node->plan.qual,
					 (PlanState *) state);

	/*
	 * initialize child nodes
	 */
	outerPlanState(state) = ExecInitNode(outerPlan(node), estate, eflags);

	/*
	 * initialize tuple type. no need to initialize projection info because
	 * this node doesn't do projections
	 */
	ExecAssignResultTypeFromTL(&state->ps);
	state->ps.ps_ProjInfo = NULL;

	initGpmonPktForHash((Plan *)node, &state->ps.gpmon_pkt, estate);

	return state;
}

/*
 * Retrieves the next tuple from the child node.
 */
TupleTableSlot *
ExecHashDummy(HashDummyState *node)
{
	PlanState *outerNode = outerPlanState(node);
	TupleTableSlot *slot = ExecProcNode(outerNode);

	return slot;
}

/*
 * Prepares the HashDummy for a re-scan.
 */
void
ExecReScanHashDummy(HashDummyState *node, ExprContext *exprCtxt)
{
	/*
	 * if chgParam of subnode is not null then plan will be re-scanned by
	 * first ExecProcNode.
	 */

	if (((PlanState *) node)->lefttree->chgParam == NULL)
		ExecReScan(((PlanState *) node)->lefttree, exprCtxt);
}

/* Cleans up once finished */
void
ExecEndHashDummy(HashDummyState *node)
{
	PlanState  *outerPlan;

	/*
	 * shut down the subplan
	 */
	outerPlan = outerPlanState(node);
	ExecEndNode(outerPlan);
}

/* Returns the number of slots needed for this operator */
int
ExecCountSlotsHashDummy(HashDummy *node)
{
	return ExecCountSlotsNode(outerPlan((Plan *) node)) +
		ExecCountSlotsNode(innerPlan((Plan *) node)) + HASHDUMMY_NSLOTS;
}
