/*-------------------------------------------------------------------------
 *
 * execAmi.c
 *	  miscellaneous executor access method routines
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *	$PostgreSQL: pgsql/src/backend/executor/execAmi.c,v 1.97 2008/08/05 21:28:29 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "executor/execdebug.h"
#include "executor/instrument.h"
#include "executor/nodeAgg.h"
#include "executor/nodeAppend.h"
#include "executor/nodeBitmapAnd.h"
#include "executor/nodeBitmapHeapscan.h"
#include "executor/nodeBitmapIndexscan.h"
#include "executor/nodeDynamicBitmapIndexscan.h"
#include "executor/nodeBitmapOr.h"
#include "executor/nodeFunctionscan.h"
#include "executor/nodeHash.h"
#include "executor/nodeHashjoin.h"
#include "executor/nodeIndexscan.h"
#include "executor/nodeLimit.h"
#include "executor/nodeMaterial.h"
#include "executor/nodeMergejoin.h"
#include "executor/nodeNestloop.h"
#include "executor/nodeRecursiveunion.h"
#include "executor/nodeResult.h"
#include "executor/nodeSetOp.h"
#include "executor/nodeSort.h"
#include "executor/nodeSubplan.h"
#include "executor/nodeSubqueryscan.h"
#include "executor/nodeTidscan.h"
#include "executor/nodeUnique.h"
#include "executor/nodeValuesscan.h"
#include "executor/nodeCtescan.h"
#include "executor/nodeWorktablescan.h"
#include "executor/nodeAssertOp.h"
#include "executor/nodeTableScan.h"
#include "executor/nodeDynamicTableScan.h"
#include "executor/nodeDynamicIndexscan.h"
#include "executor/nodeExternalscan.h"
#include "executor/nodeBitmapTableScan.h"
#include "executor/nodeMotion.h"
#include "executor/nodeSequence.h"
#include "executor/nodeTableFunction.h"
#include "executor/nodePartitionSelector.h"
#include "executor/nodeBitmapAppendOnlyscan.h"
#include "executor/nodeWindow.h"
#include "executor/nodeShareInputScan.h"

/*
 * ExecReScan
 *		Reset a plan node so that its output can be re-scanned.
 *
 * Note that if the plan node has parameters that have changed value,
 * the output might be different from last time.
 *
 * The second parameter is currently only used to pass a NestLoop plan's
 * econtext down to its inner child plan, in case that is an indexscan that
 * needs access to variables of the current outer tuple.  (The handling of
 * this parameter is currently pretty inconsistent: some callers pass NULL
 * and some pass down their parent's value; so don't rely on it in other
 * situations.	It'd probably be better to remove the whole thing and use
 * the generalized parameter mechanism instead.)
 */
void
ExecReScan(PlanState *node, ExprContext *exprCtxt)
{
	/* If collecting timing stats, update them */
	if (node->instrument)
		InstrEndLoop(node->instrument);

	/*
	 * If we have changed parameters, propagate that info.
	 *
	 * Note: ExecReScanSetParamPlan() can add bits to node->chgParam,
	 * corresponding to the output param(s) that the InitPlan will update.
	 * Since we make only one pass over the list, that means that an InitPlan
	 * can depend on the output param(s) of a sibling InitPlan only if that
	 * sibling appears earlier in the list.  This is workable for now given
	 * the limited ways in which one InitPlan could depend on another, but
	 * eventually we might need to work harder (or else make the planner
	 * enlarge the extParam/allParam sets to include the params of depended-on
	 * InitPlans).
	 */
	if (node->chgParam != NULL)
	{
		ListCell   *l;

		foreach(l, node->initPlan)
		{
			SubPlanState *sstate = (SubPlanState *) lfirst(l);
			PlanState  *splan = sstate->planstate;

			if (splan->plan->extParam != NULL)	/* don't care about child
												 * local Params */
				UpdateChangedParamSet(splan, node->chgParam);
			if (splan->chgParam != NULL)
				ExecReScanSetParamPlan(sstate, node);
		}
		foreach(l, node->subPlan)
		{
			SubPlanState *sstate = (SubPlanState *) lfirst(l);
			PlanState  *splan = sstate->planstate;

			if (splan->plan->extParam != NULL)
				UpdateChangedParamSet(splan, node->chgParam);
		}
		/* Well. Now set chgParam for left/right trees. */
		if (node->lefttree != NULL)
			UpdateChangedParamSet(node->lefttree, node->chgParam);
		if (node->righttree != NULL)
			UpdateChangedParamSet(node->righttree, node->chgParam);
	}

	/* Shut down any SRFs in the plan node's targetlist */
	if (node->ps_ExprContext)
		ReScanExprContext(node->ps_ExprContext);

	/* And do node-type-specific processing */
	switch (nodeTag(node))
	{
		case T_ResultState:
			ExecReScanResult((ResultState *) node, exprCtxt);
			break;

		case T_AppendState:
			ExecReScanAppend((AppendState *) node, exprCtxt);
			break;

		case T_RecursiveUnionState:
			ExecRecursiveUnionReScan((RecursiveUnionState *) node, exprCtxt);
			break;

		case T_AssertOpState:
			ExecReScanAssertOp((AssertOpState *) node, exprCtxt);
			break;

		case T_BitmapAndState:
			ExecReScanBitmapAnd((BitmapAndState *) node, exprCtxt);
			break;

		case T_BitmapOrState:
			ExecReScanBitmapOr((BitmapOrState *) node, exprCtxt);
			break;

		case T_SeqScanState:
		case T_AppendOnlyScanState:
		case T_AOCSScanState:
			insist_log(false, "SeqScan/AppendOnlyScan/AOCSScan are defunct");
			break;

		case T_IndexScanState:
			ExecIndexReScan((IndexScanState *) node, exprCtxt);
			break;

		case T_ExternalScanState:
			ExecExternalReScan((ExternalScanState *) node, exprCtxt);
			break;			

		case T_TableScanState:
			ExecTableReScan((TableScanState *) node, exprCtxt);
			break;

		case T_DynamicTableScanState:
			ExecDynamicTableReScan((DynamicTableScanState *) node, exprCtxt);
			break;

		case T_BitmapTableScanState:
			ExecBitmapTableReScan((BitmapTableScanState *) node, exprCtxt);
			break;

		case T_DynamicIndexScanState:
			ExecDynamicIndexReScan((DynamicIndexScanState *) node, exprCtxt);
			break;

		case T_BitmapIndexScanState:
			ExecBitmapIndexReScan((BitmapIndexScanState *) node, exprCtxt);
			break;

		case T_DynamicBitmapIndexScanState:
			ExecDynamicBitmapIndexReScan((DynamicBitmapIndexScanState *) node, exprCtxt);
			break;

		case T_BitmapHeapScanState:
			ExecBitmapHeapReScan((BitmapHeapScanState *) node, exprCtxt);
			break;

		case T_TidScanState:
			ExecTidReScan((TidScanState *) node, exprCtxt);
			break;

		case T_SubqueryScanState:
			ExecSubqueryReScan((SubqueryScanState *) node, exprCtxt);
			break;

		case T_SequenceState:
			ExecReScanSequence((SequenceState *) node, exprCtxt);
			break;

		case T_FunctionScanState:
			ExecFunctionReScan((FunctionScanState *) node, exprCtxt);
			break;

		case T_ValuesScanState:
			ExecValuesReScan((ValuesScanState *) node, exprCtxt);
			break;

		case T_CteScanState:
			ExecCteScanReScan((CteScanState *) node, exprCtxt);
			break;

		case T_WorkTableScanState:
			ExecWorkTableScanReScan((WorkTableScanState *) node, exprCtxt);
			break;

		case T_BitmapAppendOnlyScanState:
			ExecBitmapAppendOnlyReScan((BitmapAppendOnlyScanState *) node, exprCtxt);
			break;

		case T_NestLoopState:
			ExecReScanNestLoop((NestLoopState *) node, exprCtxt);
			break;

		case T_MergeJoinState:
			ExecReScanMergeJoin((MergeJoinState *) node, exprCtxt);
			break;

		case T_HashJoinState:
			ExecReScanHashJoin((HashJoinState *) node, exprCtxt);
			break;

		case T_MaterialState:
			ExecMaterialReScan((MaterialState *) node, exprCtxt);
			break;

		case T_SortState:
			ExecReScanSort((SortState *) node, exprCtxt);
			break;

		case T_AggState:
			ExecReScanAgg((AggState *) node, exprCtxt);
			break;

		case T_UniqueState:
			ExecReScanUnique((UniqueState *) node, exprCtxt);
			break;

		case T_HashState:
			ExecReScanHash((HashState *) node, exprCtxt);
			break;

		case T_SetOpState:
			ExecReScanSetOp((SetOpState *) node, exprCtxt);
			break;

		case T_LimitState:
			ExecReScanLimit((LimitState *) node, exprCtxt);
			break;

		case T_MotionState:
			ExecReScanMotion((MotionState *) node, exprCtxt);
			break;

		case T_TableFunctionScan:
			ExecReScanTableFunction((TableFunctionState *) node, exprCtxt);
			break;

		case T_WindowState:
			ExecReScanWindow((WindowState *) node, exprCtxt);
			break;

		case T_ShareInputScanState:
			ExecShareInputScanReScan((ShareInputScanState *) node, exprCtxt);
			break;
		case T_PartitionSelectorState:
			ExecReScanPartitionSelector((PartitionSelectorState *) node, exprCtxt);
			break;
			
		default:
			elog(ERROR, "unrecognized node type: %d", (int) nodeTag(node));
			break;
	}

	if (node->chgParam != NULL)
	{
		bms_free(node->chgParam);
		node->chgParam = NULL;
	}

	/* Now would be a good time to also send an update to gpmon */
	CheckSendPlanStateGpmonPkt(node);
}

/*
 * ExecMarkPos
 *
 * Marks the current scan position.
 */
void
ExecMarkPos(PlanState *node)
{
	switch (nodeTag(node))
	{
		case T_TableScanState:
			ExecTableMarkPos((TableScanState *) node);
			break;

		case T_DynamicTableScanState:
			ExecDynamicTableMarkPos((DynamicTableScanState *) node);
			break;

		case T_SeqScanState:
		case T_AppendOnlyScanState:
		case T_AOCSScanState:
			insist_log(false, "SeqScan/AppendOnlyScan/AOCSScan are defunct");
			break;

		case T_IndexScanState:
			ExecIndexMarkPos((IndexScanState *) node);
			break;

		case T_ExternalScanState:
			elog(ERROR, "Marking scan position for external relation is not supported");
			break;			

		case T_TidScanState:
			ExecTidMarkPos((TidScanState *) node);
			break;

		case T_ValuesScanState:
			ExecValuesMarkPos((ValuesScanState *) node);
			break;

		case T_MaterialState:
			ExecMaterialMarkPos((MaterialState *) node);
			break;

		case T_SortState:
			ExecSortMarkPos((SortState *) node);
			break;

		case T_ResultState:
			ExecResultMarkPos((ResultState *) node);
			break;

		case T_MotionState:
			ereport(ERROR, (
				errcode(ERRCODE_CDB_INTERNAL_ERROR),
				errmsg("unsupported call to mark position of Motion operator")
				));
			break;

		default:
			/* don't make hard error unless caller asks to restore... */
			elog(DEBUG2, "unrecognized node type: %d", (int) nodeTag(node));
			break;
	}
}

/*
 * ExecRestrPos
 *
 * restores the scan position previously saved with ExecMarkPos()
 *
 * NOTE: the semantics of this are that the first ExecProcNode following
 * the restore operation will yield the same tuple as the first one following
 * the mark operation.	It is unspecified what happens to the plan node's
 * result TupleTableSlot.  (In most cases the result slot is unchanged by
 * a restore, but the node may choose to clear it or to load it with the
 * restored-to tuple.)	Hence the caller should discard any previously
 * returned TupleTableSlot after doing a restore.
 */
void
ExecRestrPos(PlanState *node)
{
	switch (nodeTag(node))
	{
		case T_TableScanState:
			ExecTableRestrPos((TableScanState *) node);
			break;

		case T_DynamicTableScanState:
			ExecDynamicTableRestrPos((DynamicTableScanState *) node);
			break;

		case T_SeqScanState:
		case T_AppendOnlyScanState:
		case T_AOCSScanState:
			insist_log(false, "SeqScan/AppendOnlyScan/AOCSScan are defunct");
			break;

		case T_IndexScanState:
			ExecIndexRestrPos((IndexScanState *) node);
			break;

		case T_ExternalScanState:
			elog(ERROR, "Restoring scan position is not yet supported for external relation scan");
			break;			

		case T_TidScanState:
			ExecTidRestrPos((TidScanState *) node);
			break;

		case T_ValuesScanState:
			ExecValuesRestrPos((ValuesScanState *) node);
			break;

		case T_MaterialState:
			ExecMaterialRestrPos((MaterialState *) node);
			break;

		case T_SortState:
			ExecSortRestrPos((SortState *) node);
			break;

		case T_ResultState:
			ExecResultRestrPos((ResultState *) node);
			break;

		case T_MotionState:
			ereport(ERROR, (
				errcode(ERRCODE_CDB_INTERNAL_ERROR),
				errmsg("unsupported call to restore position of Motion operator")
				));
			break;

		default:
			elog(ERROR, "unrecognized node type: %d", (int) nodeTag(node));
			break;
	}

	/* Now would be a good time to also send an update to gpmon */
	CheckSendPlanStateGpmonPkt(node);
}

/*
 * ExecSupportsMarkRestore - does a plan type support mark/restore?
 *
 * XXX Ideally, all plan node types would support mark/restore, and this
 * wouldn't be needed.  For now, this had better match the routines above.
 * But note the test is on Plan nodetype, not PlanState nodetype.
 *
 * (However, since the only present use of mark/restore is in mergejoin,
 * there is no need to support mark/restore in any plan type that is not
 * capable of generating ordered output.  So the seqscan, tidscan,
 * and valuesscan support is actually useless code at present.)
 */
bool
ExecSupportsMarkRestore(NodeTag plantype)
{
	switch (plantype)
	{
		case T_SeqScan:
		case T_IndexScan:
		case T_TidScan:
		case T_ValuesScan:
		case T_Material:
		case T_Sort:
		case T_ShareInputScan:
			return true;

		case T_Result:

			/*
			 * T_Result only supports mark/restore if it has a child plan that
			 * does, so we do not have enough information to give a really
			 * correct answer.	However, for current uses it's enough to
			 * always say "false", because this routine is not asked about
			 * gating Result plans, only base-case Results.
			 */
			return false;

		default:
			break;
	}

	return false;
}

/*
 * ExecSupportsBackwardScan - does a plan type support backwards scanning?
 *
 * Ideally, all plan types would support backwards scan, but that seems
 * unlikely to happen soon.  In some cases, a plan node passes the backwards
 * scan down to its children, and so supports backwards scan only if its
 * children do.  Therefore, this routine must be passed a complete plan tree.
 */
bool
ExecSupportsBackwardScan(Plan *node)
{
	if (node == NULL)
		return false;

	switch (nodeTag(node))
	{
		case T_Result:
			if (outerPlan(node) != NULL)
				return ExecSupportsBackwardScan(outerPlan(node));
			else
				return false;

		case T_Append:
			{
				ListCell   *l;

				foreach(l, ((Append *) node)->appendplans)
				{
					if (!ExecSupportsBackwardScan((Plan *) lfirst(l)))
						return false;
				}
				return true;
			}

		case T_SeqScan:
		case T_IndexScan:
		case T_TidScan:
		case T_FunctionScan:
		case T_ValuesScan:
		case T_CteScan:
		case T_WorkTableScan:
			return true;

		case T_SubqueryScan:
			return ExecSupportsBackwardScan(((SubqueryScan *) node)->subplan);

		case T_ShareInputScan:
			return true;

		case T_Material:
		case T_Sort:
			return true;

		case T_Limit:
			return ExecSupportsBackwardScan(outerPlan(node));

		default:
			return false;
	}
}

/*
 * ExecEagerFree
 *    Eager free the memory that is used by the given node.
 */
void
ExecEagerFree(PlanState *node)
{
	switch (nodeTag(node))
	{
		/* No need for eager free */
		case T_AppendState:
		case T_AssertOpState:
		case T_BitmapAndState:
		case T_BitmapOrState:
		case T_BitmapIndexScanState:
		case T_DynamicBitmapIndexScanState:
		case T_LimitState:
		case T_MotionState:
		case T_NestLoopState:
		case T_RepeatState:
		case T_ResultState:
		case T_SetOpState:
		case T_SubqueryScanState:
		case T_TidScanState:
		case T_UniqueState:
		case T_HashState:
		case T_ValuesScanState:
		case T_TableFunctionState:
		case T_DynamicTableScanState:
		case T_DynamicIndexScanState:
		case T_SequenceState:
		case T_PartitionSelectorState:
		case T_WorkTableScanState:
			break;

		case T_TableScanState:
			ExecEagerFreeTableScan((TableScanState *)node);
			break;
			
		case T_SeqScanState:
		case T_AppendOnlyScanState:
		case T_AOCSScanState:
			insist_log(false, "SeqScan/AppendOnlyScan/AOCSScan are defunct");
			break;
			
		case T_ExternalScanState:
			ExecEagerFreeExternalScan((ExternalScanState *)node);
			break;
			
		case T_IndexScanState:
			ExecEagerFreeIndexScan((IndexScanState *)node);
			break;
			
		case T_BitmapHeapScanState:
			ExecEagerFreeBitmapHeapScan((BitmapHeapScanState *)node);
			break;
			
		case T_BitmapAppendOnlyScanState:
			ExecEagerFreeBitmapAppendOnlyScan((BitmapAppendOnlyScanState *)node);
			break;
			
		case T_BitmapTableScanState:
			ExecEagerFreeBitmapTableScan((BitmapTableScanState *)node);
			break;

		case T_FunctionScanState:
			ExecEagerFreeFunctionScan((FunctionScanState *)node);
			break;
			
		case T_MergeJoinState:
			ExecEagerFreeMergeJoin((MergeJoinState *)node);
			break;
			
		case T_HashJoinState:
			ExecEagerFreeHashJoin((HashJoinState *)node);
			break;
			
		case T_MaterialState:
			ExecEagerFreeMaterial((MaterialState*)node);
			break;
			
		case T_SortState:
			ExecEagerFreeSort((SortState *)node);
			break;
			
		case T_AggState:
			ExecEagerFreeAgg((AggState*)node);
			break;

		case T_WindowState:
			ExecEagerFreeWindow((WindowState *)node);
			break;

		case T_ShareInputScanState:
			ExecEagerFreeShareInputScan((ShareInputScanState *)node);
			break;

		case T_RecursiveUnionState:
			ExecEagerFreeRecursiveUnion((RecursiveUnionState *)node);
			break;

		default:
			Insist(false);
			break;
	}
}

/*
 * EagerFreeChildNodesContext
 *    Store the context info for eager freeing child nodes.
 */
typedef struct EagerFreeChildNodesContext
{
	/*
	 * Indicate whether the eager free is called when a subplan
	 * is finished. This is used to indicate whether we should
	 * free the Material node under the Result (to support
	 * correlated subqueries (CSQ)).
	 */
	bool subplanDone;
} EagerFreeChildNodesContext;

/*
 * EagerFreeWalker
 *    Walk the tree, and eager free the memory.
 */
static CdbVisitOpt
EagerFreeWalker(PlanState *node, void *context)
{
	EagerFreeChildNodesContext *ctx = (EagerFreeChildNodesContext *)context;

	if (node == NULL)
	{
		return CdbVisit_Walk;
	}
	
	if (IsA(node, MotionState))
	{
		/* Skip the subtree */
		return CdbVisit_Skip;
	}

	if (IsA(node, ResultState))
	{
		ResultState *resultState = (ResultState *)node;
		PlanState *lefttree = resultState->ps.lefttree;

		/*
		 * If the child node for the Result node is a Material, and the child node for
		 * the Material is a Broadcast Motion, we can't eagerly free the memory for
		 * the Material node until the subplan is done.
		 */
		if (!ctx->subplanDone && lefttree != NULL && IsA(lefttree, MaterialState))
		{
			PlanState *matLefttree = lefttree->lefttree;
			Assert(matLefttree != NULL);
			
			if (IsA(matLefttree, MotionState) &&
				((Motion*)matLefttree->plan)->motionType == MOTIONTYPE_FIXED)
			{
				ExecEagerFree(node);

				/* Skip the subtree */
				return CdbVisit_Skip;
			}
		}
	}

	ExecEagerFree(node);
	
	return CdbVisit_Walk;
}

/*
 * ExecEagerFreeChildNodes
 *    Eager free the memory for the child nodes.
 *
 * If this function is called when a subplan is finished, this function eagerly frees
 * the memory for all child nodes. Otherwise, it stops when it sees a Result node on top of
 * a Material and a Broadcast Motion. The reason that the Material node below the
 * Result can not be freed until the parent node of the subplan is finished.
 */
void
ExecEagerFreeChildNodes(PlanState *node, bool subplanDone)
{
	EagerFreeChildNodesContext ctx;
	ctx.subplanDone = subplanDone;
	
	switch(nodeTag(node))
	{
		case T_AssertOpState:
		case T_BitmapIndexScanState:
		case T_LimitState:
		case T_RepeatState:
		case T_ResultState:
		case T_SetOpState:
		case T_ShareInputScanState:
		case T_SubqueryScanState:
		case T_TidScanState:
		case T_UniqueState:
		case T_HashState:
		case T_ValuesScanState:
		case T_TableScanState:
		case T_DynamicTableScanState:
		case T_DynamicIndexScanState:
		case T_ExternalScanState:
		case T_IndexScanState:
		case T_BitmapHeapScanState:
		case T_BitmapAppendOnlyScanState:
		case T_FunctionScanState:
		case T_MaterialState:
		case T_SortState:
		case T_AggState:
		case T_WindowState:
		{
			planstate_walk_node(outerPlanState(node), EagerFreeWalker, &ctx);
			break;
		}

		case T_SeqScanState:
		case T_AppendOnlyScanState:
		case T_AOCSScanState:
			insist_log(false, "SeqScan/AppendOnlyScan/AOCSScan are defunct");
			break;

		case T_NestLoopState:
		case T_MergeJoinState:
		case T_BitmapAndState:
		case T_BitmapOrState:
		case T_HashJoinState:
		case T_RecursiveUnionState:
		{
			planstate_walk_node(innerPlanState(node), EagerFreeWalker, &ctx);
			planstate_walk_node(outerPlanState(node), EagerFreeWalker, &ctx);
			break;
		}
		
		case T_AppendState:
		{
			AppendState *appendState = (AppendState *)node;
			for (int planNo = 0; planNo < appendState->as_nplans; planNo++)
			{
				planstate_walk_node(appendState->appendplans[planNo], EagerFreeWalker, &ctx);
			}
			
			break;
		}
			
		case T_MotionState:
		{
			/* do nothing */
			break;
		}
			
		default:
		{
			Insist(false);
			break;
		}
	}
}
