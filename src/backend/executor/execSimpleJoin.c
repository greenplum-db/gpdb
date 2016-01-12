/*-------------------------------------------------------------------------
 *
 *  execSimpleJoin.c
 *	  Joins a single outer tuple with a set of inner tuples by looping
 *	  through the inner tuples
 *
 *	SimpleJoin can be thought of as the very basic joiner that can join an outer
 *	tuple and an inner tuple. However, for many join types (like outer joins)
 *	we need more than one inner tuple. Therefore, SimpleJoin would work on a
 *	single outer tuple, but would loop through the inner tuples to join and
 *	produce output tuple.
 *
 *	As SimpleJoin is the one that would be using all the join properties (such
 *	as a JoinState) it is responsible for initializing all the JoinState. Therefore,
 *	its more complex sibling "ResilientJoin" would just let the SimpleJoin initialize
 *	all the JoinState details.
 *
 * Copyright (c) 2015, Pivotal Inc.
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "executor/executor.h"
#include "nodes/execnodes.h"
#include "executor/execResilientJoin.h"
#include "executor/nodeResilientJoin.h"
#include "executor/execSimpleJoin.h"
#include "executor/execAbstractReader.h"

/* 1 slot for result tuple and 1 for null tuple */
#define SIMPLEJOINER_NSLOTS 2

static void AssignNullTupleSlot(SimpleJoinState* sjs, EState* estate);
static void InitializeTupleSlots(EState* estate, SimpleJoinState* sjs);
static void InitializeExpressions(SimpleJoinState *sjs, ResilientJoin* node);
static TupleTableSlot * GenerateOuterJoinTuple(SimpleJoinState *sjs);
static bool ReadNewOuter(SimpleJoinState *sjs);

/********************************************************************************
 *
 * ******************** PUBLIC METHODS ******************************************
 */

/* Sets up a SimpleJoin state including all expression initialization for a join. */
void
SimpleJoin_Setup(SimpleJoinState *sjs, ResilientJoinState *rjs, ResilientJoin *node, EState *estate, int eflags)
{
	ValidateJoinState(rjs);

	sjs->js.ps.plan = (Plan *) node;
	sjs->js.ps.state = estate;
	sjs->outerTupleReader = NULL;
	sjs->innerTupleReader = NULL;
	sjs->owner = NULL;
	sjs->resilientJoinState = rjs;

	/* Create expression context to evaluate all the join expressions */
	ExecAssignExprContext(estate, &sjs->js.ps);

	InitializeExpressions(sjs, node);

	Plan *innerPlan = innerPlan(node);
	Plan *outerPlan = outerPlan(node);

	innerPlanState(sjs) = ExecInitNode(innerPlan, estate, eflags);
	outerPlanState(sjs) = ExecInitNode(outerPlan, estate, eflags);

	/*
	 * tuple table initialization
	 */
	InitializeTupleSlots(estate, sjs);

	sjs->antiJoin = (sjs->js.jointype == JOIN_LASJ || sjs->js.jointype == JOIN_LASJ_NOTIN);
	sjs->semiJoin = (sjs->js.jointype == JOIN_IN);
	sjs->joinDone = false;
}

/*
 * Prepares the SimpleJoin to execute a new sub-join. Sub-joins are driven by the
 * caller ResilientJoiner that iterates one outer tuple and asks the SimpleJoin
 * to join that outer tuple with multiple inner tuples, using different join
 * functions such as INNER, LASJ etc. The join function is globally defined
 * for all sub-joins by a single ResilientJoinState
 */
void
SimpleJoin_BeginJoin(SimpleJoinState *sjs, ResilientJoiner *resilientJoiner,
		AbstractTupleReader *outerTupleReader, AbstractTupleReader *innerTupleReader)
{
	Assert(sjs->js.jointype == JOIN_INNER || sjs->js.jointype == JOIN_IN ||
			sjs->js.jointype == JOIN_LEFT || sjs->js.jointype == JOIN_LASJ ||
			sjs->js.jointype == JOIN_LASJ_NOTIN);

	/* No ongoing join operation */
	if (NULL != sjs->owner)
	{
		elog(ERROR, "Cannot acquire SimpleJoin");
	}

	ValidateJoinState(sjs);
	ValidateJoinerPointer(resilientJoiner);
	ValidateAbstractReader(outerTupleReader);
	ValidateAbstractReader(innerTupleReader);

	sjs->owner = resilientJoiner;
	sjs->outerTupleReader = outerTupleReader;
	sjs->innerTupleReader = innerTupleReader;

	sjs->needNewOuter = true;
	sjs->generateOuterJoinTuple = IS_SIMPLE_OUTER_JOIN(sjs->js.jointype);
}

/* Carry out the actual join */
TupleTableSlot *
ExecuteSimpleJoin(SimpleJoinState *sjs)
{
	for (;;)
	{
		if (sjs->needNewOuter)
		{
			if (sjs->joinDone || !ReadNewOuter(sjs))
			{
				return ExecClearTuple(sjs->js.ps.ps_ResultTupleSlot);
			}
		}

		/* Begin joining a non-empty outer tuple */
		for (;;)
		{
			TupleTableSlot *innerTuple = sjs->innerTupleReader->ReadNextTuple(sjs->innerTupleReader);

			if (TupIsNull(innerTuple))
			{
				sjs->needNewOuter = true;

				if (sjs->generateOuterJoinTuple)
				{
					return GenerateOuterJoinTuple(sjs);
				}

				/* Process outer loop, to fetch another new outer tuple */
				break;
			}

			/*
			 * Quals are already evaluated in the inner side reader. The reader giving us
			 * back a non-empty tuple means we already have a match on the join conditions
			 */
			sjs->generateOuterJoinTuple = false;

			if (sjs->antiJoin)
			{
				/* Anti join does not produce tuples for matching inner */
				sjs->needNewOuter = true;
				break;
			}

			List *otherqual = sjs->js.ps.qual;
			ExprContext *econtext = sjs->js.ps.ps_ExprContext;

			if (otherqual == NIL || ExecQual(otherqual, econtext, false))
			{
				/*
				 * One output per-outer tuple, which we are about to produce.
				 * Require a new outer tuple upon next invocation
				 */
				if (sjs->semiJoin)
				{
					sjs->needNewOuter = true;
				}

				return ExecProject(sjs->js.ps.ps_ProjInfo, NULL);
			}
		}
	}

	return ExecClearTuple(sjs->js.ps.ps_ResultTupleSlot);
}

/* Cleanup "one" sub-join */
void
SimpleJoin_EndJoin(SimpleJoinState *sjs, ResilientJoiner *owner)
{
	ValidateJoinState(sjs);

	if (owner != sjs->owner)
	{
		elog(ERROR, "Cannot release SimpleJoin");
	}

	/* Mark as un-owned */
	sjs->owner = NULL;
	/* The lifespan of tuple reader is managed externally */
	sjs->outerTupleReader = NULL;
	/* The lifespan of tuple reader is managed externally */
	sjs->innerTupleReader = NULL;
}

/* We are completely done. Cleanup everything */
void
SimpleJoin_End(SimpleJoinState *sjs)
{
	ValidateJoinState(sjs);
	/*
	 * Free the exprcontext
	 */
	ExecFreeExprContext(&sjs->js.ps);

	/*
	 * clean out the tuple table
	 */
	ExecClearTuple(sjs->js.ps.ps_ResultTupleSlot);

	/*
	 * clean up subtrees
	 */
	ExecEndNode(outerPlanState(sjs));
	ExecEndNode(innerPlanState(sjs));
}

/* Number of slots that SimpleJoin needs */
int
SimpleJoin_CountSlots(ResilientJoin *node)
{
	return ExecCountSlotsNode(outerPlan(node)) +
			ExecCountSlotsNode(innerPlan(node)) +
		SIMPLEJOINER_NSLOTS;
}

/********************************************************************************
 *
 * ******************** PRIVATE METHODS ******************************************
 */

/* If we need a null tuple slot, then assign it */
static void
AssignNullTupleSlot(SimpleJoinState* sjs, EState* estate)
{
	switch (sjs->js.jointype) {
	case JOIN_INNER:
	case JOIN_IN:
		break;
	case JOIN_LEFT:
	case JOIN_LASJ:
	case JOIN_LASJ_NOTIN:
		sjs->nullInnerTupleSlot = ExecInitNullTupleSlot(estate,
				ExecGetResultType(innerPlanState(sjs)));
		break;
	default:
		elog(LOG, "unrecognized join type: %d", (int) sjs->js.jointype);
		Assert(false);
	}
}

/* Setup all tuple slots */
static void
InitializeTupleSlots(EState* estate, SimpleJoinState* sjs)
{
	/*
	 * tuple table initialization
	 */
	ExecInitResultTupleSlot(estate, &sjs->js.ps);
	AssignNullTupleSlot(sjs, estate);
	/*
	 * initialize tuple type and projection info
	 */
	ExecAssignResultTypeFromTL(&sjs->js.ps);
	ExecAssignProjectionInfo(&sjs->js.ps, NULL);
	sjs->js.ps.ps_OuterTupleSlot = NULL;
}

/* Initialize all the expressions (e.g., targetlist, qual, hashclauses etc.) */
static void
InitializeExpressions(SimpleJoinState *sjs, ResilientJoin* node)
{
	/*
	 * initialize child expressions
	 */
	sjs->js.ps.targetlist = (List*) ExecInitExpr(
			(Expr*) node->join.plan.targetlist, (PlanState*) sjs);
	sjs->js.ps.qual = (List*) ExecInitExpr((Expr*) node->join.plan.qual,
			(PlanState*) sjs);
	sjs->js.jointype = node->join.jointype;
	sjs->js.joinqual = (List*) ExecInitExpr((Expr*) node->join.joinqual,
			(PlanState*) sjs);
	if (node->hashqualclauses != NIL) {
		sjs->hashqualclauses = (List *) ExecInitExpr(
				(Expr *) node->hashqualclauses, (PlanState *) sjs);
	} else {
		sjs->hashqualclauses = (List*) ExecInitExpr((Expr*) node->hashclauses,
				(PlanState*) sjs);
	}
}

/* Returns true if an outer was fetched (i.e., outer not exhausted) */
static bool
ReadNewOuter(SimpleJoinState *sjs)
{
	Assert(sjs->needNewOuter);

	sjs->needNewOuter = false;
	TupleTableSlot *outerTuple = sjs->outerTupleReader->ReadNextTuple(sjs->outerTupleReader);

	if (TupIsNull(outerTuple))
	{
		return false;
	}

	if (IS_SIMPLE_OUTER_JOIN(sjs->js.jointype))
	{
		/*
		 * Assume we need to generate an outer tuple with null column values for
		 * the inner side, unless we find a match
		 */
		sjs->generateOuterJoinTuple = true;
	}

	sjs->innerTupleReader->PrepareForReScan(sjs->innerTupleReader, sjs->js.ps.ps_ExprContext);

	return true;
}

/*
 * Generates an outer join tuple by joining the current outer tuple
 * with a null inner tuple.
 *
 * Caution: this is destructive as it replace
 * the ecxt_innertuple with a null inner tuple. Caller should save the
 * ecxt_innertuple and replace it if the caller believes it would need
 * the inner tuple for a later operation
 */
static TupleTableSlot *
GenerateOuterJoinTuple(SimpleJoinState *sjs)
{
	Assert(sjs->generateOuterJoinTuple &&
			IS_SIMPLE_OUTER_JOIN(sjs->js.jointype));

	sjs->generateOuterJoinTuple = false;
	sjs->js.ps.ps_ExprContext->ecxt_innertuple = sjs->nullInnerTupleSlot;
	return ExecProject(sjs->js.ps.ps_ProjInfo, NULL);
}

