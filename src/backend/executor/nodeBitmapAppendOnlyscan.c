/*-------------------------------------------------------------------------
 *
 * nodeBitmapAppendOnlyscan.c
 *	  Routines to support bitmapped scan from Append-Only relations
 *
 * This is a modified copy of nodeBitmapHeapscan.c converted to Append-Only.
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 * Portions Copyright (c) 2008-2009, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeBitmapAppendOnlyscan.c
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		ExecBitmapAppendOnlyScan		scan from an AO relation using bitmap info
 *		ExecBitmapAppendOnlyNext		workhorse for above
 *		ExecInitBitmapAppendOnlyScan	creates and initializes state info.
 *		ExecBitmapAppendOnlyReScan	prepares to rescan the plan.
 *		ExecEndBitmapAppendOnlyScan	releases all storage.
 */
#include "postgres.h"

#include "access/heapam.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbvars.h" /* gp_select_invisible */
#include "executor/execdebug.h"
#include "executor/nodeBitmapAppendOnlyscan.h"
#include "miscadmin.h"
#include "nodes/tidbitmap.h"
#include "parser/parsetree.h"
#include "pgstat.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"

static TupleTableSlot *BitmapAppendOnlyScanNext(BitmapAppendOnlyScanState *node);

/*
 * Initialize the fetch descriptor for the BitmapAppendOnlyScanState if
 * it is not initialized.
 */
static void
initFetchDesc(BitmapAppendOnlyScanState *scanstate)
{
	BitmapAppendOnlyScan *node = (BitmapAppendOnlyScan *)(scanstate->ss.ps.plan);
	Relation currentRelation = scanstate->ss.ss_currentRelation;
	EState *estate = scanstate->ss.ps.state;

	if (node->isAORow)
	{
		if (scanstate->baos_currentAOFetchDesc == NULL)
		{
			Snapshot appendOnlyMetaDataSnapshot = estate->es_snapshot;
			if (appendOnlyMetaDataSnapshot == SnapshotAny)
			{
				/* 
				 * the append-only meta data should never be fetched with
				 * SnapshotAny as bogus results are returned.
				 */
				appendOnlyMetaDataSnapshot = GetTransactionSnapshot();
			}
			scanstate->baos_currentAOFetchDesc = 
				appendonly_fetch_init(currentRelation,
									  estate->es_snapshot,
									  appendOnlyMetaDataSnapshot);
			scanstate->baos_currentAOCSFetchDesc = NULL;
			scanstate->baos_currentAOCSLossyFetchDesc = NULL;
		}
	}
	
	else
	{
		if (scanstate->baos_currentAOCSFetchDesc == NULL)
		{
			bool *proj;
			bool *projLossy;
			int colno;
			Snapshot appendOnlyMetaDataSnapshot = estate->es_snapshot;
			if (appendOnlyMetaDataSnapshot == SnapshotAny)
			{
				/* 
				 * the append-only meta data should never be fetched with
				 * SnapshotAny as bogus results are returned.
				 */
				appendOnlyMetaDataSnapshot = GetTransactionSnapshot();
			}

			scanstate->baos_currentAOFetchDesc = NULL;

			/*
			 * Obtain the projection.
			 */
			Assert(currentRelation->rd_att != NULL);
		
			proj = (bool *)palloc0(sizeof(bool) * currentRelation->rd_att->natts);
			projLossy = (bool *)palloc0(sizeof(bool) * currentRelation->rd_att->natts);

			GetNeededColumnsForScan((Node *) node->scan.plan.targetlist, proj, currentRelation->rd_att->natts);
			GetNeededColumnsForScan((Node *) node->scan.plan.qual, proj, currentRelation->rd_att->natts);

            memcpy(projLossy,proj,sizeof(bool) * currentRelation->rd_att->natts);

			GetNeededColumnsForScan((Node *) node->bitmapqualorig, projLossy, currentRelation->rd_att->natts);
                
			for(colno = 0; colno < currentRelation->rd_att->natts; colno++)
			{
				if(proj[colno])
					break;
			}
			
			/*
			 * At least project one column. Since the tids stored in the index may not have
			 * a correponding tuple any more (because of previous crashes, for example), we
			 * need to read the tuple to make sure.
			 */
			if(colno == currentRelation->rd_att->natts)
				proj[0] = true;
			
			scanstate->baos_currentAOCSFetchDesc =
				aocs_fetch_init(currentRelation, estate->es_snapshot, appendOnlyMetaDataSnapshot, proj);

			for(colno = 0; colno < currentRelation->rd_att->natts; colno++)
			{
				if(projLossy[colno])
					break;
			}
			
			/*
			 * At least project one column. Since the tids stored in the index may not have
			 * a correponding tuple any more (because of previous crashes, for example), we
			 * need to read the tuple to make sure.
			 */
			if(colno == currentRelation->rd_att->natts)
				projLossy[0] = true;
			
			scanstate->baos_currentAOCSLossyFetchDesc =
				aocs_fetch_init(currentRelation, estate->es_snapshot, appendOnlyMetaDataSnapshot, projLossy);
			
		}
	}
}

/*
 * Free fetch descriptor.
 */
static inline void
freeFetchDesc(BitmapAppendOnlyScanState *scanstate)
{
	if (scanstate->baos_currentAOFetchDesc != NULL)
	{
		Assert(((BitmapAppendOnlyScan *)(scanstate->ss.ps.plan))->isAORow);
		appendonly_fetch_finish(scanstate->baos_currentAOFetchDesc);
		pfree(scanstate->baos_currentAOFetchDesc);
		scanstate->baos_currentAOFetchDesc = NULL;
	}

	if (scanstate->baos_currentAOCSFetchDesc != NULL)
	{
		Assert(!((BitmapAppendOnlyScan *)(scanstate->ss.ps.plan))->isAORow);
		aocs_fetch_finish(scanstate->baos_currentAOCSFetchDesc);
		pfree(scanstate->baos_currentAOCSFetchDesc);
		scanstate->baos_currentAOCSFetchDesc = NULL;
	}

	if (scanstate->baos_currentAOCSLossyFetchDesc != NULL)
	{
		Assert(!((BitmapAppendOnlyScan *)(scanstate->ss.ps.plan))->isAORow);
		aocs_fetch_finish(scanstate->baos_currentAOCSLossyFetchDesc);
		pfree(scanstate->baos_currentAOCSLossyFetchDesc);
		scanstate->baos_currentAOCSLossyFetchDesc = NULL;
	}
}

/*
 * Free the state relevant to bitmaps
 */
static inline void
freeBitmapState(BitmapAppendOnlyScanState *scanstate)
{
	if (scanstate->baos_iterator)
	{
		tbm_generic_end_iterate(scanstate->baos_iterator);
		scanstate->baos_iterator = NULL;
		/* baos_tbmres is owned by the iterator and freed during end_iterate. */
		scanstate->baos_tbmres = NULL;
	}
}

/* ----------------------------------------------------------------
 *		BitmapAppendOnlyNext
 *
 *		Retrieve next tuple from the BitmapAppendOnlyScan node's currentRelation
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
BitmapAppendOnlyScanNext(BitmapAppendOnlyScanState *node)
{
	EState	   *estate;
	ExprContext *econtext;
	AppendOnlyFetchDesc aoFetchDesc;
	AOCSFetchDesc aocsFetchDesc;
	AOCSFetchDesc aocsLossyFetchDesc;
	Index		scanrelid;
	GenericBMIterator *iterator;
	OffsetNumber psuedoHeapOffset;
	ItemPointerData psudeoHeapTid;
	AOTupleId aoTid;
	TupleTableSlot *slot;

	/*
	 * extract necessary information from index scan node
	 */
	estate = node->ss.ps.state;
	econtext = node->ss.ps.ps_ExprContext;
	slot = node->ss.ss_ScanTupleSlot;

	initFetchDesc(node);

	aoFetchDesc = node->baos_currentAOFetchDesc;
	aocsFetchDesc = node->baos_currentAOCSFetchDesc;
	aocsLossyFetchDesc = node->baos_currentAOCSLossyFetchDesc;
	scanrelid = ((BitmapAppendOnlyScan *) node->ss.ps.plan)->scan.scanrelid;
	iterator = node->baos_iterator;

	/*
	 * Check if we are evaluating PlanQual for tuple of this relation.
	 * Additional checking is not good, but no other way for now. We could
	 * introduce new nodes for this case and handle IndexScan --> NewNode
	 * switching in Init/ReScan plan...
	 */
	if (estate->es_evTuple != NULL &&
		estate->es_evTuple[scanrelid - 1] != NULL)
	{
		if (estate->es_evTupleNull[scanrelid - 1])
		{
			freeFetchDesc(node);
			freeBitmapState(node);
			
			return ExecClearTuple(slot);
		}

		ExecStoreHeapTuple(estate->es_evTuple[scanrelid - 1],
						   slot, InvalidBuffer, false);

		/* Does the tuple meet the original qual conditions? */
		econtext->ecxt_scantuple = slot;

		ResetExprContext(econtext);

		if (!ExecQual(node->baos_bitmapqualorig, econtext, false))
		{
			ExecEagerFreeBitmapAppendOnlyScan(node);

			ExecClearTuple(slot);		/* would not be returned by scan */
		}

		/* Flag for the next call that no more tuples */
		estate->es_evTupleNull[scanrelid - 1] = true;

		return slot;
	}

	/*
	 * If we haven't yet performed the underlying index scan, or
	 * we have used up the bitmaps from the previous scan, do the next scan,
	 * and prepare the bitmap to be iterated over.
 	 */
	if (iterator == NULL)
	{
		Node *tbm = (Node *) MultiExecProcNode(outerPlanState(node));

		if (!tbm || !(IsA(tbm, TIDBitmap) || IsA(tbm, StreamBitmap)))
			elog(ERROR, "unrecognized result from subplan");

		if (tbm == NULL)
		{
			ExecEagerFreeBitmapAppendOnlyScan(node);

			return ExecClearTuple(slot);
		}

		/*
		 * BitmapIndexScan is the owner of the bitmap memory. We don't take
		 * ownership here; just begin iteration.
		 */
		node->baos_iterator = iterator = tbm_generic_begin_iterate(tbm);
	}

	Assert(iterator != NULL);

	for (;;)
	{
		/* GPDB_84_MERGE_FIXME: can the baos_tbmres state be removed from
		 * BitmapAppendOnlyScanState, or is it possible for it to be carried
		 * through multiple calls to BitmapAppendOnlyScanNext()? */
		TBMIterateResult *tbmres = node->baos_tbmres;

		CHECK_FOR_INTERRUPTS();

		if (QueryFinishPending)
			return NULL;

		if (!node->baos_gotpage)
		{
			/*
			 * Obtain the next psuedo-heap-page-info with item bit-map.  Later, we'll
			 * convert the (psuedo) heap block number and item number to an
			 * Append-Only TID.
			 */
			tbmres = tbm_generic_iterate(iterator);
			if (!tbmres)
			{
				/* no more entries in the bitmap */
				break;
			}

			node->baos_tbmres = tbmres;

			/* If tbmres contains no tuples, continue. */
			if (tbmres->ntuples == 0)
				continue;

			/* Make sure we never cross 15-bit offset number [MPP-24326] */
			Assert(tbmres->ntuples <= INT16_MAX + 1);
			CheckSendPlanStateGpmonPkt(&node->ss.ps);

		 	node->baos_gotpage = true;

			/*
		 	* Set cindex to first slot to examine
		 	*/
			node->baos_cindex = 0;

			node->baos_lossy = (tbmres->ntuples < 0);
			if (!node->baos_lossy)
			{
				node->baos_ntuples = tbmres->ntuples;
			}
			else
			{
				/* Iterate over the first 2^15 tuples [MPP-24326] */
				node->baos_ntuples = INT16_MAX + 1;
			}
		}
		else
		{
			/*
			 * Continuing in previously obtained page; advance cindex
			 */
			Assert(tbmres);
			node->baos_cindex++;
		}

		/*
		 * Out of range?  If so, nothing more to look at on this page
		 */
		if (node->baos_cindex < 0 || node->baos_cindex >= node->baos_ntuples)
		{
		 	node->baos_gotpage = false;
			continue;
		}

		/*
		 * Must account for lossy page info...
		 */
		if (node->baos_lossy)
		{
			psuedoHeapOffset = node->baos_cindex;	// We are iterating through all items.
		}
		else
		{
			Assert(node->baos_cindex <= tbmres->ntuples);
			psuedoHeapOffset = tbmres->offsets[node->baos_cindex];

			/*
			 * Ensure that the reserved 16-th bit is always ON for offsets from
			 * lossless bitmap pages [MPP-24326].
			 */
			Assert(((uint16)(psuedoHeapOffset & 0x8000)) > 0);
		}

		/*
		 * Okay to fetch the tuple
		 */
		ItemPointerSet(
				&psudeoHeapTid, 
				tbmres->blockno, 
				psuedoHeapOffset);

		tbm_convert_appendonly_tid_out(&psudeoHeapTid, &aoTid);

		if (aoFetchDesc != NULL)
		{
			appendonly_fetch(aoFetchDesc, &aoTid, slot);
		}
		
		else
		{
			if (node->baos_lossy) 
			{
				Assert(aocsLossyFetchDesc != NULL);
				aocs_fetch(aocsLossyFetchDesc, &aoTid, slot);
			}
			else
			{
				Assert(aocsFetchDesc != NULL);
				aocs_fetch(aocsFetchDesc, &aoTid, slot);
			}
		}
		
      	if (TupIsNull(slot))
			continue;

		pgstat_count_heap_fetch(node->ss.ss_currentRelation);

		/*
		 * If we are using lossy info, we have to recheck the qual
		 * conditions at every tuple.
		 */
		if (node->baos_lossy)
		{
			econtext->ecxt_scantuple = slot;
			ResetExprContext(econtext);

			if (!ExecQual(node->baos_bitmapqualorig, econtext, false))
			{
				/* Fails recheck, so drop it and loop back for another */
				ExecClearTuple(slot);
				continue;
			}
		}

		/* OK to return this tuple */
		return slot;
	}

	/*
	 * if we get here it means we are at the end of the scan..
	 */
	ExecEagerFreeBitmapAppendOnlyScan(node);

	return ExecClearTuple(slot);
}

/* ----------------------------------------------------------------
 *		ExecBitmapAppendOnlyScan(node)
 * ----------------------------------------------------------------
 */
TupleTableSlot *
ExecBitmapAppendOnlyScan(BitmapAppendOnlyScanState *node)
{
	/*
	 * use BitmapAppendOnlyNext as access method
	 */
	return ExecScan(&node->ss, (ExecScanAccessMtd) BitmapAppendOnlyScanNext);
}

/* ----------------------------------------------------------------
 *		ExecBitmapAppendOnlyReScan(node)
 * ----------------------------------------------------------------
 */
void
ExecBitmapAppendOnlyReScan(BitmapAppendOnlyScanState *node, ExprContext *exprCtxt)
{
	EState	   *estate;
	Index		scanrelid;

	estate = node->ss.ps.state;
	scanrelid = ((BitmapAppendOnlyScan *) node->ss.ps.plan)->scan.scanrelid;

	/* node->aofs.ps.ps_TupFromTlist = false; */

	/*
	 * If we are being passed an outer tuple, link it into the "regular"
	 * per-tuple econtext for possible qual eval.
	 */
	if (exprCtxt != NULL)
	{
		ExprContext *stdecontext;

		stdecontext = node->ss.ps.ps_ExprContext;
		stdecontext->ecxt_outertuple = exprCtxt->ecxt_outertuple;
	}

	/* If this is re-scanning of PlanQual ... */
	if (estate->es_evTuple != NULL &&
		estate->es_evTuple[scanrelid - 1] != NULL)
	{
		estate->es_evTupleNull[scanrelid - 1] = false;
	}

	/*
	 * NOTE: The appendonly_fetch routine can fetch randomly, so no need to reset it.
	 */

	freeBitmapState(node);

	/*
	 * Always rescan the input immediately, to ensure we can pass down any
	 * outer tuple that might be used in index quals.
	 */
	ExecReScan(outerPlanState(node), exprCtxt);
}

/* ----------------------------------------------------------------
 *		ExecEndBitmapAppendOnlyScan
 * ----------------------------------------------------------------
 */
void
ExecEndBitmapAppendOnlyScan(BitmapAppendOnlyScanState *node)
{
	Relation	relation;

	/*
	 * extract information from the node
	 */
	relation = node->ss.ss_currentRelation;

	/*
	 * Free the exprcontext
	 */
	ExecFreeExprContext(&node->ss.ps);

	/*
	 * clear out tuple table slots
	 */
	ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
	ExecClearTuple(node->ss.ss_ScanTupleSlot);

	/*
	 * close down subplans
	 */
	ExecEndNode(outerPlanState(node));

	ExecEagerFreeBitmapAppendOnlyScan(node);
	
	/*
	 * close the heap relation.
	 */
	ExecCloseScanRelation(relation);

	node->baos_gotpage = false;
	node->baos_lossy = false;
	node->baos_cindex = 0;
	node->baos_ntuples = 0;

	EndPlanStateGpmonPkt(&node->ss.ps);
}

/* ----------------------------------------------------------------
 *		ExecInitBitmapAppendOnlyScan
 *
 *		Initializes the scan's state information.
 * ----------------------------------------------------------------
 */
BitmapAppendOnlyScanState *
ExecInitBitmapAppendOnlyScan(BitmapAppendOnlyScan *node, EState *estate, int eflags)
{
	BitmapAppendOnlyScanState *scanstate;
	Relation	currentRelation;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	Assert(IsA(node, BitmapAppendOnlyScan));

	/*
	 * Assert caller didn't ask for an unsafe snapshot --- see comments at
	 * head of file.
	 *
	 * MPP-4703: the MVCC-snapshot restriction is required for correct results.
	 * our test-mode may deliberately return incorrect results, but that's OK.
	 */
	Assert(IsMVCCSnapshot(estate->es_snapshot) || gp_select_invisible);

	/*
	 * create state structure
	 */
	scanstate = makeNode(BitmapAppendOnlyScanState);
	scanstate->ss.ps.plan = (Plan *) node;
	scanstate->ss.ps.state = estate;

	scanstate->baos_iterator = NULL;
	scanstate->baos_tbmres = NULL;
	scanstate->baos_gotpage = false;
	scanstate->baos_lossy = false;
	scanstate->baos_cindex = 0;
	scanstate->baos_ntuples = 0;

	/*
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &scanstate->ss.ps);

	/* scanstate->aofs.ps.ps_TupFromTlist = false;*/

	/*
	 * initialize child expressions
	 */
	scanstate->ss.ps.targetlist = (List *)
		ExecInitExpr((Expr *) node->scan.plan.targetlist,
					 (PlanState *) scanstate);
	scanstate->ss.ps.qual = (List *)
		ExecInitExpr((Expr *) node->scan.plan.qual,
					 (PlanState *) scanstate);
	scanstate->baos_bitmapqualorig = (List *)
		ExecInitExpr((Expr *) node->bitmapqualorig,
					 (PlanState *) scanstate);

#define BITMAPAPPENDONLYSCAN_NSLOTS 2

	/*
	 * tuple table initialization
	 */
	ExecInitResultTupleSlot(estate, &scanstate->ss.ps);
	ExecInitScanTupleSlot(estate, &scanstate->ss);

	/*
	 * open the base relation and acquire appropriate lock on it.
	 */
	currentRelation = ExecOpenScanRelation(estate, node->scan.scanrelid);

	scanstate->ss.ss_currentRelation = currentRelation;

	/*
	 * get the scan type from the relation descriptor.
	 */
	ExecAssignScanType(&scanstate->ss, RelationGetDescr(currentRelation));

	/*
	 * Initialize result tuple type and projection info.
	 */
	ExecAssignResultTypeFromTL(&scanstate->ss.ps);
	ExecAssignScanProjectionInfo(&scanstate->ss);

	scanstate->baos_currentAOFetchDesc = NULL;
	scanstate->baos_currentAOCSFetchDesc = NULL;
	scanstate->baos_currentAOCSLossyFetchDesc = NULL;
	
	/*
	 * initialize child nodes
	 *
	 * We do this last because the child nodes will open indexscans on our
	 * relation's indexes, and we want to be sure we have acquired a lock on
	 * the relation first.
	 */
	outerPlanState(scanstate) = ExecInitNode(outerPlan(node), estate, eflags);

	/*
	 * all done.
	 */
	return scanstate;
}

int
ExecCountSlotsBitmapAppendOnlyScan(BitmapAppendOnlyScan *node)
{
	return ExecCountSlotsNode(outerPlan((Plan *) node)) +
		ExecCountSlotsNode(innerPlan((Plan *) node)) + BITMAPAPPENDONLYSCAN_NSLOTS;
}

void
ExecEagerFreeBitmapAppendOnlyScan(BitmapAppendOnlyScanState *node)
{
	freeFetchDesc(node);
	freeBitmapState(node);
}
