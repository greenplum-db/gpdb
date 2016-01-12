/*-------------------------------------------------------------------------
 *
 * nodeResilientJoin.c
 *	  A resilient joiner executor node that supports dynamically switching between different join algorithms
 *	  such as nested loop join and hash join.
 *
 * Portions Copyright (c) 2015, Pivotal Inc.
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "executor/executor.h"
#include "executor/nodeResilientJoin.h"
#include "executor/execResilientJoin.h"
#include "executor/execSimpleJoin.h"
#include "executor/instrument.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbexplain.h"
#include "utils/memutils.h"
#include "miscadmin.h"

/* 2 extra slots to save inner and outer tuples from spill files (via AbstractTupleReader) */
#define RESILIENTJOIN_NSLOTS 2

static void InitGpMon(Plan *planNode, gpmon_packet_t *gpmon_pkt, EState *estate);
static bool IsNotDistinctJoin(List *qualList);
static void DeconstructHashClauses(ResilientJoinState* state);
static void InitializeExpressions(ResilientJoinState* state, ResilientJoin* node);
static void InitializeTupleSlots(EState* estate, ResilientJoinState* state, ResilientJoin* node);
static void ExtractTupleStatAndMemoryQuota(ResilientJoinState *joinerState, double *planRows, int *planWidth, Size *memoryQuota);
static void InitResilientJoinStatsCollector(ResilientJoinState *joinState);

extern bool optimizer;

/********************************************************************************
 *
 * ******************** PUBLIC METHODS ******************************************
 */

/*
 * Initializes the resilient join
 */
ResilientJoinState *
ExecInitResilientJoin(ResilientJoin *node, EState *estate, int eflags)
{
	/* The first call to GetNextJoinedTuple should go into Build phase */
	ResilientJoinState* state = makeNode(ResilientJoinState);

	/* Set up all common join properties */
	SimpleJoin_Setup(&state->sjs, state, node, estate, eflags);

	if (node->hashqualclauses != NIL)
	{
		/* CDB: This must be an IS NOT DISTINCT join!  */
		Insist(IsNotDistinctJoin(node->hashqualclauses));
		state->nonEquiJoin = true;
	}
	else
	{
		state->nonEquiJoin = false;
	}

	/* Only allow role reversal for inner join */
	state->roleReversalAllowed = (node->join.jointype == JOIN_INNER);

	/*
	 * initialize child expressions
	 */
	InitializeExpressions(state, node);

	state->prefetchInner = node->join.prefetch_inner;

	state->memoryQuota = node->join.plan.operatorMemKB * 1024L;

	/*
	 * tuple table initialization
	 */
	InitializeTupleSlots(estate, state, node);
	DeconstructHashClauses(state);

	double planRows = 0;
	int planWidth = 0;
	Size memoryQuota = 0;

	/* Extract memory quota and plan stats for the join */
	ExtractTupleStatAndMemoryQuota(state, &planRows, &planWidth, &memoryQuota);

	Assert(0 != planRows && 0 != planWidth && 0 != memoryQuota);

	state->memoryQuota = memoryQuota;

	/* Setup EXPLAIN ANALYZE data structure */
	if (estate->es_instrument)
	{
		InitResilientJoinStatsCollector(state);
	}

	state->rootJoiner = ResilientJoiner_GetRootJoiner(state, JoinAlgorithm_HashJoin /* JoinAlgorithm */,
			innerPlanState(state), outerPlanState(state), planRows, planWidth, memoryQuota);
	state->curJoiner = state->rootJoiner;

	InitGpMon((Plan *)node, &state->sjs.js.ps.gpmon_pkt, estate);
	return state;
}

/* The "get next" method for resilient join called by ExecProcNode */
TupleTableSlot*
ExecResilientJoin(ResilientJoinState *state)
{
	return state->curJoiner->GetNextJoinedTuple(state->curJoiner);
}

/* Number of tuple slot needed by resilient joiner */
int
ExecCountSlotsResilientJoin(ResilientJoin *node)
{
	return SimpleJoin_CountSlots(node) + RESILIENTJOIN_NSLOTS;
}

/* End the join process and free up all resources */
void
ExecEndResilientJoin(ResilientJoinState *node)
{
	ExecClearTuple(node->outerTupleSlot);
	ExecClearTuple(node->innerTupleSlot);

	SimpleJoin_End(&node->sjs);
}

/* Prepare the resilient joiner for a rescan */
void
ExecResilientJoinReScan(ResilientJoinState *node, ExprContext *exprCtxt)
{
	/* TODO: Implement rescan */
}

/* Free resources eagerly */
void
ExecEagerFreeResilientJoin(ResilientJoinState *node)
{
	/* TODO: Implement eager free */
}

/*
 * Ensures that the the array of sub join stats is sufficiently large. Once this
 * method is called, the joinerId can be used as an index into the sub join stats array
 */
void
InitStatArrayEntry(ResilientJoiner *joiner, ResilientJoinState *joinState, JoinAlgorithm joinAlgorithm)
{
	Assert(NULL != joinState->joinStats);
	Assert(NULL != joinState->joinStats->subJoinStat);

	int joinerId = joiner->joinerId;
	int parentJoinerId = NULL != joiner->parentJoiner ? joiner->parentJoiner->joinerId : -1;

	/* Verify that the root's joinerId starts at 0 */
	AssertImply(NULL == joiner->parentJoiner, 0 == joinerId);

	/* Only 1 entry is pre-allocated. Subsequent entries must be newly allocated */
	Assert(0 == joinerId || joinerId == joinState->joinStats->numSubJoin);

	if (joinerId >= joinState->joinStats->numSubJoin)
	{
		joinState->joinStats->numSubJoin = joinerId + 1;
		joinState->joinStats->subJoinStat = repalloc(joinState->joinStats->subJoinStat,
				sizeof(SubJoinStats) * joinState->joinStats->numSubJoin);
		memset(&joinState->joinStats->subJoinStat[joinerId], 0, sizeof(SubJoinStats));
	}

	joinState->joinStats->subJoinStat[joinerId].childId = joinerId;
	joinState->joinStats->subJoinStat[joinerId].parentId = parentJoinerId;
	joinState->joinStats->subJoinStat[joinerId].joinAlgorithm = joinAlgorithm;
	joinState->joinStats->subJoinStat[joinerId].isRoleReversed = joiner->roleReversed;

	Assert(joinState->joinStats->numSubJoin > joinerId);
}

/********************************************************************************
 *
 * ******************** PRIVATE METHODS ******************************************
 */

static void
CollectResilientJoinStats(PlanState *planState, struct StringInfoData *buf)
{
	Assert(IsA(planState, ResilientJoinState));
	ResilientJoinState *joinState = (ResilientJoinState *) planState;

	Assert(NULL != joinState->joinStats);
	Assert(0 < joinState->joinStats->numSubJoin);

	SubJoinStats *rootJoinStat = &joinState->joinStats->subJoinStat[0];

	Instrumentation *instr = planState->instrument;

	instr->execmemused = MemoryAccounting_GetPeak(planState->plan->memoryAccount);
	instr->workmemused = rootJoinStat->hashJoinStats.totalBucketSize;

	double workMemWanted = rootJoinStat->hashJoinStats.totalSpilledBucketSize + rootJoinStat->hashJoinStats.totalBucketSize;

	if (instr->workmemused != workMemWanted)
	{
		instr->workmemwanted = workMemWanted;
	}

	CdbExplain_Agg *chainLength = NULL;

    int parentId = rootJoinStat->parentId;
    int indent = 0;
    char *joinAlgo = "HJ";
    char *roleReversed = "O";

    for (int joinId = 0; joinId < joinState->joinStats->numSubJoin; joinId++)
    {
    	SubJoinStats *joinStat = &joinState->joinStats->subJoinStat[joinId];

    	if (joinStat->parentId > parentId)
    	{
    		++indent;
    	}
    	else if (joinStat->parentId < parentId)
    	{
    		--indent;
    	}

    	parentId = joinStat->parentId;

    	chainLength = &joinStat->hashJoinStats.chainLength;

		joinAlgo = joinStat->joinAlgorithm == JoinAlgorithm_HashJoin ? "HJ" : "NLJ";
		roleReversed = joinStat->isRoleReversed ? "R" : "O";

    	appendStringInfoFill(buf, 2*indent, ' ');
        appendStringInfo(buf,
                         "%s-%s: Chain length"
                         " %.1f avg, %.0f max, using %d of %d buckets.  ",
						 joinAlgo, roleReversed,
                         cdbexplain_agg_avg(chainLength),
                         chainLength->vmax,
                         chainLength->vcnt,
                         joinStat->hashJoinStats.totalBucketCount);
        appendStringInfoChar(buf, '\n');
    }
}

static void
InitResilientJoinStatsCollector(ResilientJoinState *joinState)
{
	joinState->sjs.js.ps.cdbexplainfun = CollectResilientJoinStats;
	joinState->joinStats = palloc0(sizeof(ResilientJoinStats));

	/* Allocate just 1 entry (assuming no spilling) */
	joinState->joinStats->subJoinStat = palloc0(sizeof(SubJoinStats));
	joinState->joinStats->numSubJoin = 1;
}

static void
InitGpMon(Plan *planNode, gpmon_packet_t *gpmon_pkt, EState *estate)
{
	/*
	 * TODO: Take out the HashJoin from assert once
	 * we have proper ResilientJoin node
	 */
	Assert(planNode != NULL && gpmon_pkt != NULL &&
			(IsA(planNode, ResilientJoin) || IsA(planNode, HashJoin)));

	InitPlanNodeGpmonPkt(planNode, gpmon_pkt, estate, PMNT_ResilientJoin,
							(int64)planNode->plan_rows,
							NULL);
}

/*
 * Is this an IS-NOT-DISTINCT-join qual list (as opposed the an equijoin)?
 *
 * We perform an abbreviated test based on the assumptions that these are the only
 * possibilities and that all conjuncts are alike in this regard.
 */
static bool
IsNotDistinctJoin(List *qualList)
{
	ListCell *lc = NULL;

	foreach (lc, qualList)
	{
		BoolExpr *boolExpr = (BoolExpr*)lfirst(lc);
		DistinctExpr *distinctExpr = NULL;

		if (IsA(boolExpr, BoolExpr) && boolExpr->boolop == NOT_EXPR)
		{
			distinctExpr = (DistinctExpr*)linitial(boolExpr->args);

			if (IsA(distinctExpr, DistinctExpr))
			{
				return true; /* We assume the rest follow suit! */
			}
		}
	}

	return false;
}

/*
 * Deconstruct the hash clauses into outer and inner argument values, so
 * that we can evaluate those subexpressions separately.  Also make a list
 * of the hash operator OIDs, in preparation for looking up the hash
 * functions to use.
 */
static void
DeconstructHashClauses(ResilientJoinState* state)
{
	/*
	 * Deconstruct the hash clauses into outer and inner argument values, so
	 * that we can evaluate those subexpressions separately.  Also make a list
	 * of the hash operator OIDs, in preparation for looking up the hash
	 * functions to use.
	 */
	List *outerHashKeys = NULL;
	List *innerHashKeys = NULL;
	List *hashOperators = NULL;
	ListCell *l = NULL;
	foreach(l, state->hashClauses)
	{
		FuncExprState *fstate = (FuncExprState *) lfirst(l);
		Assert(IsA(fstate, FuncExprState));

		OpExpr *hashClause = (OpExpr *) fstate->xprstate.expr;
		Assert(IsA(hashClause, OpExpr));

		outerHashKeys = lappend(outerHashKeys, linitial(fstate->args));
		innerHashKeys = lappend(innerHashKeys, lsecond(fstate->args));
		hashOperators = lappend_oid(hashOperators, hashClause->opno);
	}
	state->outerHashKeys = outerHashKeys;
	state->innerHashKeys = innerHashKeys;
	state->hashOperators = hashOperators;

	state->innerHashComputer = TupleHashComputer_Create(innerHashKeys, hashOperators,
			false /* shouldHashNull */, state->sjs.js.ps.ps_ExprContext);
	state->outerHashComputer = TupleHashComputer_Create(outerHashKeys, hashOperators,
			true /* shouldHashNull */, state->sjs.js.ps.ps_ExprContext);
}

/* Initialize all the expressions (e.g., targetlist, qual, hashclauses etc.) */
static void
InitializeExpressions(ResilientJoinState* state, ResilientJoin* node)
{
	/*
	 * initialize child expressions
	 */
	state->hashClauses = (List*) ExecInitExpr((Expr*) node->hashclauses, (PlanState*) state);
}

/* Setup all tuple slots */
static void
InitializeTupleSlots(EState* estate, ResilientJoinState* state, ResilientJoin* node)
{
	state->outerTupleSlot = ExecInitExtraTupleSlot(estate);
	ExecSetSlotDescriptor(state->outerTupleSlot,
			ExecGetResultType(outerPlanState(state)));

	state->innerTupleSlot = ExecInitExtraTupleSlot(estate);
	ExecSetSlotDescriptor(state->innerTupleSlot,
			ExecGetResultType(innerPlanState(state)));

	state->prefetchedOuterTupleSlot = NULL;
}

static void
ExtractTupleStatAndMemoryQuota(ResilientJoinState *joinerState, double *planRows, int *planWidth, Size *memoryQuota)
{
	Plan *joinPlan = joinerState->sjs.js.ps.plan;
	Plan *innerPlan = innerPlan(joinPlan);

	if (force_new_join)
	{
		/*
		 * We are forcing legacy HashJoin to ResilientJoin. So, we need to look
		 * one level down to find plan_rows and plan_width
		 */
		Assert(IsA(innerPlan, Hash));

		Plan *outerOfInnerPlan = outerPlan(innerPlan);

		*planRows = outerOfInnerPlan->plan_rows;
		*planWidth = outerOfInnerPlan->plan_width;

		*memoryQuota = innerPlan->operatorMemKB * 1024L;
	}
	else
	{
		Assert(!IsA(innerPlan, Hash));
		*planRows = innerPlan->plan_rows;
		*planWidth = innerPlan->plan_width;

		*memoryQuota = joinPlan->operatorMemKB * 1024L;
	}
	/*
	 * For copy command the whole plan may start with a 0 memory quota and therefore
	 * we would end up with 0 memory quota for ResilientJoin. Use a default value in
	 * such cases
	 */
	if (0 == *memoryQuota)
	{
		/* Give the entire statement_mem.
		 *
		 * TODO: assign proper memory quota if operatorMemKB is set to 0
		 */
		*memoryQuota = statement_mem * 1024L;
	}
}
