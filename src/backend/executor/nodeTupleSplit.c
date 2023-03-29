/*-------------------------------------------------------------------------
 * nodeTupleSplit.c
 *	  Implementation of nodeTupleSplit.
 *
 * Portions Copyright (c) 2019-Present VMware, Inc. or its affiliates.
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
#include "optimizer/optimizer.h"
#include "utils/memutils.h"

static TupleTableSlot *ExecTupleSplit(PlanState *pstate);

/* -----------------
 * ExecInitTupleSplit
 *
 *	Creates the run-time information for the tuple split node produced by the
 *	planner and initializes its outer subtree
 * -----------------
 */
TupleSplitState *
ExecInitTupleSplit(TupleSplit *node, EState *estate, int eflags)
{
	TupleSplitState     *tup_spl_state;
	Plan                *outerPlan;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	tup_spl_state = makeNode(TupleSplitState);
	tup_spl_state->ss.ps.plan = (Plan *) node;
	tup_spl_state->ss.ps.state = estate;
	tup_spl_state->ss.ps.ExecProcNode = ExecTupleSplit;

	ExecAssignExprContext(estate, &tup_spl_state->ss.ps);

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
	tup_spl_state->ss.ps.outerops =
		ExecGetResultSlotOps(outerPlanState(&tup_spl_state->ss),
							 &tup_spl_state->ss.ps.outeropsfixed);
	tup_spl_state->ss.ps.outeropsset = true;

	ExecCreateScanSlotFromOuterPlan(estate, &tup_spl_state->ss,
									tup_spl_state->ss.ps.outerops);

	/* initialize result type, slot and projection. */
	ExecInitResultTupleSlotTL(&tup_spl_state->ss.ps, &TTSOpsVirtual);
	ExecAssignProjectionInfo(&tup_spl_state->ss.ps, NULL);

	/*
	 * initialize input tuple isnull buffer
	 */
	tup_spl_state->isnull_orig = (bool *) palloc0(sizeof(bool) * list_length(outerPlan(node)->targetlist));

	/* create bitmap set for each dqa expr to store its input tuple attribute number */
	AttrNumber maxAttrNum = 0;
	tup_spl_state->numDisDQAs = list_length(node->dqa_expr_lst);
	tup_spl_state->dqa_split_bms = palloc0(sizeof(Bitmapset *) * tup_spl_state->numDisDQAs);
	tup_spl_state->agg_filter_array = palloc0(sizeof(ExprState *) * tup_spl_state->numDisDQAs);
	tup_spl_state->dqa_id_array = palloc0( sizeof(int) * tup_spl_state->numDisDQAs);

	int i = 0;
	ListCell *lc;
	foreach(lc, node->dqa_expr_lst)
	{
		DQAExpr *dqaExpr = (DQAExpr *)lfirst(lc);

		int j = -1;
		while ((j = bms_next_member(dqaExpr->agg_args_id_bms, j)) >= 0)
		{
			TargetEntry *te = get_sortgroupref_tle((Index)j, node->plan.lefttree->targetlist);
			tup_spl_state->dqa_split_bms[i] = bms_add_member(tup_spl_state->dqa_split_bms[i], te->resno);

			if (maxAttrNum < te->resno)
				maxAttrNum = te->resno;
		}

		/* init filter expr */
		tup_spl_state->agg_filter_array[i] = ExecInitExpr(dqaExpr->agg_filter, (PlanState *)tup_spl_state);
		tup_spl_state->dqa_id_array[i] = dqaExpr->agg_expr_id;
		i ++;
	}

	/*
	 * fetch group by expr bitmap set
	 */
	Bitmapset *grpbySet = NULL;
	for (int keyno = 0; keyno < node->numCols; keyno++)
	{
		grpbySet = bms_add_member(grpbySet, node->grpColIdx[keyno]);
	}

	/*
	 * fetch all columns which is not referenced by all DQAs
	 */
	Bitmapset *all_input_attr_bms = NULL;
	foreach(lc, outerPlan(node)->targetlist)
	{
		TargetEntry *te = (TargetEntry *)lfirst(lc);
		all_input_attr_bms = bms_add_member(all_input_attr_bms, te->resno);
	}

	Bitmapset *dqa_not_used_bms = all_input_attr_bms;
	for (int id = 0; id < tup_spl_state->numDisDQAs; id++)
	{
		dqa_not_used_bms =
			bms_del_members(dqa_not_used_bms, tup_spl_state->dqa_split_bms[id]);
	}

	/* grpbySet + dqa_not_used_bms is common skip splitting pattern */
	Bitmapset *skip_split_bms = bms_join(dqa_not_used_bms, grpbySet);

	/*
	 * For each DQA splitting tuple, it contain DQA's expr needed column and
	 * common skip column.
	 */
	for (int id = 0; id < tup_spl_state->numDisDQAs; id++)
	{
		Bitmapset *orig_bms = tup_spl_state->dqa_split_bms[id];
		tup_spl_state->dqa_split_bms[id] =
			bms_union(orig_bms, skip_split_bms);
		bms_free(orig_bms);
	}

	/*
	 * Update maxAttrNum which is used to calculate projection number
	 * of ExecTupleSplit
	 */
	int x = bms_prev_member(skip_split_bms, -1);
	if (x > maxAttrNum)
		maxAttrNum = x;
	tup_spl_state->maxAttrNum = maxAttrNum;

	bms_free(skip_split_bms);

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
static TupleTableSlot *
ExecTupleSplit(PlanState *pstate)
{
	TupleSplitState *node = castNode(TupleSplitState, pstate);
	TupleTableSlot  *result;
	ExprContext     *econtext;
	TupleSplit      *plan;
	bool             filter_out = false;

	econtext = node->ss.ps.ps_ExprContext;
	plan = (TupleSplit *)node->ss.ps.plan;

	do {
		/* if all DQAs of the last slot were processed, get a new slot */
		if (node->currentExprId == 0)
		{
			node->outerslot = ExecProcNode(outerPlanState(node));

			if (TupIsNull(node->outerslot))
				return NULL;

			slot_getsomeattrs(node->outerslot, node->maxAttrNum);

			/* store original tupleslot isnull array */
			memcpy(node->isnull_orig, node->outerslot->tts_isnull,
		           node->outerslot->tts_nvalid * sizeof(bool));
		}

		econtext->ecxt_outertuple = node->outerslot;

		/* The filter is pushed down from relative DQA */
		ExprState * filter = node->agg_filter_array[node->currentExprId];
		if (filter)
		{
			Datum		res;
			bool		isnull;

			res = ExecEvalExprSwitchContext(filter, econtext, &isnull);

			if (isnull || !DatumGetBool(res))
			{
				/* skip tuple split once, if the tuple filter out */
				node->currentExprId = (node->currentExprId + 1) % node->numDisDQAs;
				filter_out = true;
			}
			else
				filter_out = false;
		}
		else
			filter_out = false;

	} while(filter_out);

	/* reset the isnull array to the original state */
	bool *isnull = node->outerslot->tts_isnull;
	memcpy(isnull, node->isnull_orig, node->outerslot->tts_nvalid);

	for (AttrNumber attno = 1; attno <= node->outerslot->tts_nvalid; attno++)
	{
		/* If the column is relevant to the current dqa, keep it */
		if (bms_is_member(attno, node->dqa_split_bms[node->currentExprId]))
			continue;

		/* otherwise, null this column out */
		isnull[attno - 1] = true;
	}

	/* project the tuple */
	result = ExecProject(node->ss.ps.ps_ProjInfo);

	/* the next DQA to process */
	node->currentExprId = (node->currentExprId + 1) % node->numDisDQAs;
	ResetExprContext(econtext);

	return result;
}

void ExecEndTupleSplit(TupleSplitState *node)
{
	PlanState   *outerPlan;

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
