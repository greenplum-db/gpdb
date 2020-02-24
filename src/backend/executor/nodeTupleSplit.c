/*-------------------------------------------------------------------------
 * nodeTupleSplit.c
 *	  Implementation of nodeTupleSplit.
 *
 * Portions Copyright (c) 2019-Present Pivotal Software, Inc.
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeTupleSplit.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "executor/executor.h"
#include "executor/nodeTupleSplit.h"
#include "optimizer/tlist.h"
#include "utils/memutils.h"

/* -----------------
 * ExecInitTupleSplit
 *
 *	Creates the run-time information for the tuple split node produced by the
 *	planner and initializes its outer subtree
 * -----------------
 */
TupleSplitState *ExecInitTupleSplit(TupleSplit *node, EState *estate, int eflags)
{
	TupleSplitState     *tup_spl_state;
	Plan                *outerPlan;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	tup_spl_state = makeNode(TupleSplitState);
	tup_spl_state->ss.ps.plan = (Plan *) node;
	tup_spl_state->ss.ps.state = estate;

	ExecAssignExprContext(estate, &tup_spl_state->ss.ps);

	/*
	 * tuple table initialization
	 */
	tup_spl_state->ss.ss_ScanTupleSlot = ExecInitExtraTupleSlot(estate);
	ExecInitResultTupleSlot(estate, &tup_spl_state->ss.ps);

	/*
	 * initialize child expressions
	 */
	tup_spl_state->ss.ps.targetlist = (List *)
		ExecInitExpr((Expr *) node->plan.targetlist,
					 (PlanState *) tup_spl_state);

	if (estate->es_instrument && (estate->es_instrument & INSTRUMENT_CDB))
	{
		tup_spl_state->ss.ps.cdbexplainbuf = makeStringInfo();
		tup_spl_state->ss.ps.cdbexplainfun = NULL;
	}

	/*
	 * initialize child nodes
	 */
	outerPlan = outerPlan(node);
	outerPlanState(tup_spl_state) = ExecInitNode(outerPlan, estate, eflags);

	/*
	 * initialize source tuple type.
	 */
	ExecAssignScanTypeFromOuterPlan(&tup_spl_state->ss);

	ExecAssignResultTypeFromTL(&tup_spl_state->ss.ps);
	ExecAssignProjectionInfo(&tup_spl_state->ss.ps, NULL);

	/*
	 * initialize group by bitmap set
	 */
	for (int keyno = 0; keyno < node->numCols; keyno++)
	{
		tup_spl_state->grpbySet = bms_add_member(tup_spl_state->grpbySet, node->grpColIdx[keyno]);
	}

	/*
	 * initialize input tuple isnull buffer
	 */
	tup_spl_state->isnull_orig = (bool *) palloc0(sizeof(bool) * list_length(outerPlan(node)->targetlist));

	/* create bitmap set for each dqa expr to store its input tuple attribute number */
	AttrNumber maxAttrNum = 0;
	tup_spl_state->dqa_args_attr_num = palloc0(sizeof(Bitmapset *) * node->numDisDQAs);
	for (int i = 0; i < node->numDisDQAs; i++)
	{
		int j = -1;
		while ((j = bms_next_member(node->dqa_args_id_bms[i], j)) >= 0)
		{
			TargetEntry *te = get_sortgroupref_tle((Index)j, node->plan.lefttree->targetlist);
			tup_spl_state->dqa_args_attr_num[i] = bms_add_member(tup_spl_state->dqa_args_attr_num[i], te->resno);

			if (maxAttrNum < te->resno)
				maxAttrNum = te->resno;
		}
	}

	tup_spl_state->maxAttrNum = maxAttrNum;
	/*
	 * add all DQA expr AttrNum into a bitmapset
	 */
	for (int i = 0; i < node->numDisDQAs; i++)
		tup_spl_state->all_dist_attr_num = bms_add_members(tup_spl_state->all_dist_attr_num, tup_spl_state->dqa_args_attr_num[i]);

	return tup_spl_state;
}

/*
 * ExecTupleSplit -
 *
 *      ExecTupleSplit receives tuples from its outer subplan. Every
 *      input tuple will generate n output tuples (n is the number of
 *      the DQAs exprs). Each output tuple only contain one DQA expr and
 *      all GROUP BY exprs.
 */
struct TupleTableSlot *ExecTupleSplit(TupleSplitState *node)
{
	TupleTableSlot  *result;
	ExprContext     *econtext;
	TupleSplit      *plan;
	ExprDoneCond     isDone;

	econtext = node->ss.ps.ps_ExprContext;
	plan = (TupleSplit *)node->ss.ps.plan;

	/* if all DQAs of the last slot were processed, get a new slot */
	if (node->currentExprId == 0)
	{
		node->outerslot = ExecProcNode(outerPlanState(node));

		if (TupIsNull(node->outerslot))
			return NULL;

		slot_getsomeattrs(node->outerslot, node->maxAttrNum);

		/* store original tupleslot isnull array */
		memcpy(node->isnull_orig, slot_get_isnull(node->outerslot),
			   node->outerslot->PRIVATE_tts_nvalid * sizeof(bool));
	}

	/* reset the isnull array to the original state */
	bool *isnull = slot_get_isnull(node->outerslot);
	memcpy(isnull, node->isnull_orig, node->outerslot->PRIVATE_tts_nvalid);

	for (AttrNumber attno = 1; attno <= node->outerslot->PRIVATE_tts_nvalid; attno++)
	{
		/* If the column is in the group by, keep it */
		if (bms_is_member(attno, node->grpbySet))
			continue;

		/* If the column is relevant to the current dqa, keep it */
		if (bms_is_member(attno, node->dqa_args_attr_num[node->currentExprId]))
			continue;

		/* If the column does not belong to any DQA but the projection needs it, keep it */
		if (!bms_is_member(attno, node->all_dist_attr_num))
			continue;

		/* otherwise, null this column out */
		isnull[attno - 1] = true;
	}

	/* project the tuple */
	econtext->ecxt_outertuple = node->outerslot;
	result = ExecProject(node->ss.ps.ps_ProjInfo, &isDone);

	/* the next DQA to process */
	node->currentExprId = (node->currentExprId + 1) % plan->numDisDQAs;
	ResetExprContext(econtext);

	return result;
}

void ExecEndTupleSplit(TupleSplitState *node)
{
	PlanState   *outerPlan;

	bms_free(node->grpbySet);
	pfree(node->isnull_orig);

	/*
	 * We don't actually free any ExprContexts here (see comment in
	 * ExecFreeExprContext), just unlink the output one from the plan node
	 * suffices.
	 */
	ExecFreeExprContext(&node->ss.ps);

	/* clean up tuple table */
	ExecClearTuple(node->ss.ss_ScanTupleSlot);

	outerPlan = outerPlanState(node);
	ExecEndNode(outerPlan);
}

void ExecReScanTupleSplit(TupleSplitState *node)
{
	node->currentExprId = 0;

	if (node->ss.ps.lefttree->chgParam == NULL)
		ExecReScan(node->ss.ps.lefttree);
}

void ExecSquelchTupleSplit(TupleSplitState *node)
{
	ExecSquelchNode(outerPlanState(node));
}
