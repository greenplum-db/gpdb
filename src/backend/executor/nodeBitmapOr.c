/*-------------------------------------------------------------------------
 *
 * nodeBitmapOr.c
 *	  routines to handle BitmapOr nodes.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeBitmapOr.c
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
#include "executor/nodeBitmapOr.h"
#include "miscadmin.h"


/* ----------------------------------------------------------------
 *		ExecBitmapOr
 *
 *		stub for pro forma compliance
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecBitmapOr(PlanState *pstate)
{
	elog(ERROR, "BitmapOr node does not support ExecProcNode call convention");
	return NULL;
}

/* ----------------------------------------------------------------
 *		ExecInitBitmapOr
 *
 *		Begin all of the subscans of the BitmapOr node.
 * ----------------------------------------------------------------
 */
BitmapOrState *
ExecInitBitmapOr(BitmapOr *node, EState *estate, int eflags)
{
	BitmapOrState *bitmaporstate = makeNode(BitmapOrState);
	PlanState **bitmapplanstates;
	int			nplans;
	int			i;
	ListCell   *l;
	Plan	   *initNode;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

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
	bitmaporstate->ps.ExecProcNode = ExecBitmapOr;
	bitmaporstate->bitmapplans = bitmapplanstates;
	bitmaporstate->nplans = nplans;

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

	/*
	 * Miscellaneous initialization
	 *
	 * BitmapOr plans don't have expression contexts because they never call
	 * ExecQual or ExecProject.  They don't need any tuple slots either.
	 */

	return bitmaporstate;
}

/* ----------------------------------------------------------------
 *	   MultiExecBitmapOr
 *
 *	   BitmapOr node gets the bitmaps generated from BitmapIndexScan
 *	   nodes and outputs a bitmap that ORs all input bitmaps.
 *
 *	   The first input bitmap is utilized to store the result of the
 *	   OR and returned to the caller. In addition, the output points
 *	   to a newly created OpStream node of type BMS_OR, where all
 *	   StreamNodes of input bitmaps are added as input streams.
 * ----------------------------------------------------------------
 */
Node *
MultiExecBitmapOr(BitmapOrState *node)
{
	PlanState **bitmapplans;
	int			nplans;
	int			i;
	/*
	 * Greenplum uses result for TIDBitmap result, and node->bitmap for
	 * StreamBitmap result if there is any StreamBitmap.
	 *
	 * At last it will union them and return.
	 */
	TIDBitmap  *result = NULL;

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

		/*
		 * Note for further merge iteration:
		 *     Greenplum's BitmapIndexScan returns a StreamBitmap
		 */
		subresult = MultiExecProcNode(subnode);

		if (subresult == NULL)
			continue;

		if (!(IsA(subresult, TIDBitmap) ||
			  IsA(subresult, StreamBitmap)))
			elog(ERROR, "unrecognized result from subplan");

		if (IsA(subresult, TIDBitmap))
		{
			/* if it's a TIDBitmap, union into result */
			if (result == NULL)
				result = (TIDBitmap *)subresult;
			else
			{
				tbm_union(result, (TIDBitmap *)subresult);
				tbm_generic_free(subresult);
			}
		}
		else
		{
			/* if it's a StreamBitmap, union into node->bitmap */
			if (node->bitmap)
			{
				if (node->bitmap != subresult)
				{
					StreamBitmap *s = (StreamBitmap *)subresult;
					stream_move_node((StreamBitmap *)node->bitmap, s, BMS_OR);
					tbm_generic_free(subresult);
				}
			}
			else
				node->bitmap = subresult;
		}
	}

	/* union the TIDBitmap and StreamBitmap into node->bitmap */
	if (result != NULL)
	{
		if (node->bitmap && IsA(node->bitmap, StreamBitmap))
			stream_add_node((StreamBitmap *)node->bitmap, 
						tbm_create_stream_node(result), BMS_OR);
		else
			node->bitmap = (Node *)result;
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
}

void
ExecReScanBitmapOr(BitmapOrState *node)
{
	/*
	 * For optimizer a rescan call on BitmapIndexScan could free up the bitmap. So,
	 * we voluntarily set our bitmap to NULL to ensure that we don't have an out
	 * of scope pointer
	 */
	node->bitmap = NULL;

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
		 * If chgParam of subnode is not null then plan will be re-scanned by
		 * first ExecProcNode.
		 */
		if (subnode->chgParam == NULL)
			ExecReScan(subnode);
	}
}
