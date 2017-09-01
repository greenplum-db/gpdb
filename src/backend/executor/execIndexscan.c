/*-------------------------------------------------------------------------
 *
 * execIndexscan.c
 *	  Define common routines that are used by IndexScan, BitmapIndexScan,
 *	  and DynamicIndexScan nodes.
 *
 * Portions Copyright (c) 2013 - present, EMC/Greenplum
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/execIndexscan.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/nbtree.h"
#include "executor/execIndexscan.h"
#include "executor/executor.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "utils/array.h"
#include "utils/lsyscache.h"

/*
 * InitCommonIndexScanState
 *   Initialize the scan state that is common in IndexScan and DynamicIndexScan.
 */
void
InitCommonIndexScanState(IndexScanState *indexstate, IndexScan *node, EState *estate, int eflags)
{
	Assert(IsA(indexstate, IndexScanState) ||
		IsA(indexstate, DynamicIndexScanState));
	
	indexstate->ss.ps.plan = (Plan *)node;
	indexstate->ss.ps.state = estate;

	/*
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &indexstate->ss.ps);

	/*
	 * initialize child expressions
	 *
	 * Note: we don't initialize all of the indexqual expression, only the
	 * sub-parts corresponding to runtime keys (see below).  The indexqualorig
	 * expression is always initialized even though it will only be used in
	 * some uncommon cases --- would be nice to improve that.  (Problem is
	 * that any SubPlans present in the expression must be found now...)
	 */
	indexstate->ss.ps.targetlist = (List *)
		ExecInitExpr((Expr *) node->scan.plan.targetlist,
					 (PlanState *) indexstate);
	indexstate->ss.ps.qual = (List *)
		ExecInitExpr((Expr *) node->scan.plan.qual,
					 (PlanState *) indexstate);
	indexstate->indexqualorig = (List *)
		ExecInitExpr((Expr *) node->indexqualorig,
					 (PlanState *) indexstate);

	/*
	 * tuple table initialization
	 */
	ExecInitResultTupleSlot(estate, &indexstate->ss.ps);
	ExecInitScanTupleSlot(estate, &indexstate->ss);
	/* Initialize result tuple type [JIRA: MPP-24151]. */
	ExecAssignResultTypeFromTL((PlanState *)indexstate);

	/*
	 * If eflag contains EXEC_FLAG_REWIND or EXEC_FLAG_BACKWARD or EXEC_FLAG_MARK,
	 * then this node is not eager free safe.
	 */
	indexstate->ss.ps.delayEagerFree =
		((eflags & (EXEC_FLAG_REWIND | EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)) != 0);
}

/*
 * OpenIndexRelation
 *    Open the index relation of the given index oid.
 *
 * If the parent table is one of the target relations of the query, then
 * InitPlan already opened and write-locked the index, so we can avoid
 * taking another lock here.  Otherwise we need a normal reader's lock.
 *
 * The parent table is specified through the range table index.
 */ 
Relation
OpenIndexRelation(EState *estate, Oid indexOid, Index tableRtIndex)
{
	LOCKMODE lockMode = AccessShareLock;
	
	if (ExecRelationIsTargetRelation(estate, tableRtIndex))
	{
		lockMode = NoLock;
	}

	return index_open(indexOid, lockMode);
}

/*
 * InitRuntimeKeysContext
 *   Initialize the context for runtime keys.
 */
void
InitRuntimeKeysContext(IndexScanState *indexstate)
{
	EState *estate = indexstate->ss.ps.state;
	Assert(estate != NULL);

	if (indexstate->iss_RuntimeContext == NULL)
	{
		ExprContext *stdecontext = indexstate->ss.ps.ps_ExprContext;

		ExecAssignExprContext(estate, &indexstate->ss.ps);
		indexstate->iss_RuntimeContext = indexstate->ss.ps.ps_ExprContext;
		indexstate->ss.ps.ps_ExprContext = stdecontext;
	}
}

/*
 * FreeRuntimeKeysContext
 *   Frees the expression context for runtime keys.
 */
void
FreeRuntimeKeysContext(IndexScanState *indexstate)
{
	if (indexstate->iss_RuntimeContext != NULL)
	{
		FreeExprContext(indexstate->iss_RuntimeContext);
		indexstate->iss_RuntimeContext = NULL;
	}
}

/* EOF */
