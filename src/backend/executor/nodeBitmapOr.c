/*-------------------------------------------------------------------------
 *
 * nodeBitmapOr.c
 *	  routines to handle BitmapOr nodes.
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/executor/nodeBitmapOr.c,v 1.9 2008/01/01 19:45:49 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
/* INTERFACE ROUTINES
 *		ExecInitBitmapOr	- initialize the BitmapOr node
 *		MultiExecBitmapOr	- retrieve the result bitmap from the node
 *		ExecEndBitmapOr		- shut down the BitmapOr node
 *		ExecReScanBitmapOr	- rescan the BitmapOr node
 *
 *	 NOTES
 *		BitmapOr nodes don't make use of their left and right
 *		subtrees, rather they maintain a list of subplans,
 *		much like Append nodes.  The logic is much simpler than
 *		Append, however, since we needn't cope with forward/backward
 *		execution.
 */

#include "postgres.h"

#include "cdb/cdbvars.h"
#include "executor/execdebug.h"
#include "executor/instrument.h"
#include "executor/nodeBitmapOr.h"
#include "miscadmin.h"
#include "nodes/tidbitmap.h"


/* ----------------------------------------------------------------
 *		ExecInitBitmapOr
 *
 *		Begin all of the subscans of the BitmapOr node.
 * ----------------------------------------------------------------
 */
BitmapOrState *
ExecInitBitmapOr(BitmapOr *node, EState *estate, int eflags)
{
	BitmapOrState *bitmaporstate;
	PlanState **bitmapplanstates;
	int			nplans;
	int			i;
	ListCell   *l;
	Plan	   *initNode;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	bitmaporstate = makeNode(BitmapOrState);

	/*
	 * Set up empty vector of subplan states
	 */
	nplans = list_length(node->bitmapplans);

	bitmapplanstates = (PlanState **) palloc0(nplans * sizeof(PlanState *));

	/*
	 * create new BitmapOrState for our BitmapOr node
	 */
	bitmaporstate->ps.plan = (Plan *) node;
	bitmaporstate->ps.state = estate;
	bitmaporstate->bitmapplans = bitmapplanstates;
	bitmaporstate->nplans = nplans;

	/*
	 * Miscellaneous initialization
	 *
	 * BitmapOr plans don't have expression contexts because they never call
	 * ExecQual or ExecProject.  They don't need any tuple slots either.
	 */

#define BITMAPOR_NSLOTS 0

	/*
	 * call ExecInitNode on each of the plans to be executed and save the
	 * results into the array "bitmapplanstates".
	 */
	i = 0;
	foreach(l, node->bitmapplans)
	{
		initNode = (Plan *) lfirst(l);
		bitmapplanstates[i] = ExecInitNode(initNode, estate, eflags);
		i++;
	}

	initGpmonPktForBitmapOr((Plan *)node, &bitmaporstate->ps.gpmon_pkt, estate);
	
	return bitmaporstate;
}

int
ExecCountSlotsBitmapOr(BitmapOr *node)
{
	ListCell   *plan;
	int			nSlots = 0;

	foreach(plan, node->bitmapplans)
		nSlots += ExecCountSlotsNode((Plan *) lfirst(plan));
	return nSlots + BITMAPOR_NSLOTS;
}

/* ----------------------------------------------------------------
 *	   MultiExecBitmapOr
 * ----------------------------------------------------------------
 */
Node *
MultiExecBitmapOr(BitmapOrState *node)
{
	PlanState **bitmapplans;
	int			nplans;
	int			i;
	HashBitmap *hbm = NULL;

	/* must provide our own instrumentation support */
	if (node->ps.instrument)
		InstrStartNode(node->ps.instrument);

	/*
	 * get information from the node
	 */
	bitmapplans = node->bitmapplans;
	nplans = node->nplans;

	/*
	 * Scan all the subplans and OR their result bitmaps
	 */
	for (i = 0; i < nplans; i++)
	{
		PlanState  *subnode = bitmapplans[i];
		Node	   *subresult = NULL;

		if (IsA(subnode, BitmapIndexScanState) &&
				(PLANGEN_PLANNER == node->ps.state->es_plannedstmt->planGen))
		{
			((BitmapIndexScanState *) subnode)->bitmap = node->bitmap;
		}

		subresult = MultiExecProcNode(subnode);

		if(subresult == NULL)
			continue;

		if (!(IsA(subresult, HashBitmap) ||
			  IsA(subresult, StreamBitmap)))
			elog(ERROR, "unrecognized result from subplan");

		if (IsA(subresult, HashBitmap))
		{
			if (hbm == NULL)
				hbm = (HashBitmap *)subresult;
			else
			{
				tbm_union(hbm, (HashBitmap *)subresult);

				/* For optimizer BitmapIndexScan would free all bitmaps */
				if (PLANGEN_PLANNER == node->ps.state->es_plannedstmt->planGen)
				{
					tbm_free((HashBitmap *)subresult);

					/* Since we release the space for subresult, we want to
					 * reset the bitmaps in subnode tree to NULL.
					 */
					tbm_reset_bitmaps(subnode);
				}
			}
		}
		else
		{
			if(node->bitmap)
			{
				if(node->bitmap != subresult)
				{
					StreamBitmap *s = (StreamBitmap *)subresult;
					stream_add_node((StreamBitmap *)node->bitmap, 
									s->streamNode, BMS_OR);

					/*
					 * Don't free subresult here, as we are still using the StreamNode inside it.
					 * For Planner, this would introduce memory leak. For optimizer, however,
					 * BitmapIndexScan would free the bitmap at the end of the scan of the part
					 */
				}
			}
			else
				node->bitmap = subresult;
		}
	}

	/* check to see if we have any hash bitmaps */
	if (hbm != NULL)
	{
		if(node->bitmap && IsA(node->bitmap, StreamBitmap))
			stream_add_node((StreamBitmap *)node->bitmap, 
						tbm_create_stream_node(hbm), BMS_OR);
		else
			node->bitmap = (Node *)hbm;
	}

	/* must provide our own instrumentation support */
	if (node->ps.instrument)
        InstrStopNode(node->ps.instrument, node->bitmap ? 1 : 0);

	return node->bitmap;
}

/* ----------------------------------------------------------------
 *		ExecEndBitmapOr
 *
 *		Shuts down the subscans of the BitmapOr node.
 *
 *		Returns nothing of interest.
 * ----------------------------------------------------------------
 */
void
ExecEndBitmapOr(BitmapOrState *node)
{
	PlanState **bitmapplans;
	int			nplans;
	int			i;

	/*
	 * get information from the node
	 */
	bitmapplans = node->bitmapplans;
	nplans = node->nplans;

	/*
	 * shut down each of the subscans (that we've initialized)
	 */
	for (i = 0; i < nplans; i++)
	{
		if (bitmapplans[i])
			ExecEndNode(bitmapplans[i]);
	}

	EndPlanStateGpmonPkt(&node->ps);
}

void
ExecReScanBitmapOr(BitmapOrState *node, ExprContext *exprCtxt)
{
	/*
	 * For optimizer a rescan call on BitmapIndexScan could free up the bitmap. So,
	 * we voluntarily set our bitmap to NULL to ensure that we don't have an out
	 * of scope pointer
	 */
	if (PLANGEN_OPTIMIZER == node->ps.state->es_plannedstmt->planGen)
	{
		node->bitmap = NULL;
	}

	int			i;

	for (i = 0; i < node->nplans; i++)
	{
		PlanState  *subnode = node->bitmapplans[i];

		/*
		 * ExecReScan doesn't know about my subplans, so I have to do
		 * changed-parameter signaling myself.
		 */
		if (node->ps.chgParam != NULL)
			UpdateChangedParamSet(subnode, node->ps.chgParam);

		/*
		 * Always rescan the inputs immediately, to ensure we can pass down
		 * any outer tuple that might be used in index quals.
		 */
		ExecReScan(subnode, exprCtxt);
	}
}

void
initGpmonPktForBitmapOr(Plan *planNode, gpmon_packet_t *gpmon_pkt, EState *estate)
{
	Assert(planNode != NULL && gpmon_pkt != NULL && IsA(planNode, BitmapOr));

	{
		InitPlanNodeGpmonPkt(planNode, gpmon_pkt, estate, PMNT_BitmapOr,
							  (int64)0,  NULL);
	}
}
