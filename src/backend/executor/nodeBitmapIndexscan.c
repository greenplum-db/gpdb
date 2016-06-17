/*-------------------------------------------------------------------------
 *
 * nodeBitmapIndexscan.c
 *	  Routines to support bitmapped index scans of relations
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/executor/nodeBitmapIndexscan.c,v 1.25 2008/01/01 19:45:49 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		MultiExecBitmapIndexScan	scans a relation using index.
 *		ExecInitBitmapIndexScan		creates and initializes state info.
 *		ExecBitmapIndexReScan		prepares to rescan the plan.
 *		ExecEndBitmapIndexScan		releases all storage.
 */
#include "postgres.h"

#include "access/genam.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbpartition.h"
#include "executor/execdebug.h"
#include "executor/execDynamicIndexScan.h"
#include "executor/instrument.h"
#include "executor/nodeBitmapIndexscan.h"
#include "executor/nodeIndexscan.h"
#include "miscadmin.h"
#include "utils/memutils.h"
#include "utils/lsyscache.h"
#include "nodes/tidbitmap.h"

#define BITMAPINDEXSCAN_NSLOTS 0

/* ----------------------------------------------------------------
 *		MultiExecBitmapIndexScan(node)
 * ----------------------------------------------------------------
 */
Node *
MultiExecBitmapIndexScan(BitmapIndexScanState *node)
{
	IndexScanState *scanState = (IndexScanState*)node;

	Node 		*bitmap = NULL;

	/* Make sure we are not leaking a previous bitmap */
	AssertImply(node->indexScanState.ss.ps.state->es_plannedstmt->planGen == PLANGEN_OPTIMIZER, NULL == node->bitmap);

	/* must provide our own instrumentation support */
	if (scanState->ss.ps.instrument)
	{
		InstrStartNode(scanState->ss.ps.instrument);
	}
	bool partitionIsReady = IndexScan_BeginIndexPartition(scanState, node->partitionMemoryContext, false /* initQual */,
			false /* initTargetList */, true /* supportsArrayKeys */,
			true /* isMultiScan */);

	Assert(partitionIsReady);

	if (partitionIsReady)
	{
		bool doscan = node->indexScanState.iss_RuntimeKeysReady;

		IndexScanDesc scandesc = scanState->iss_ScanDesc;

		/* Get bitmap from index */
		while (doscan)
		{
			bitmap = index_getmulti(scandesc, node->bitmap);

			if ((NULL != bitmap) &&
				!(IsA(bitmap, HashBitmap) || IsA(bitmap, StreamBitmap)))
			{
				elog(ERROR, "unrecognized result from bitmap index scan");
			}

			CHECK_FOR_INTERRUPTS();

			if (QueryFinishPending)
				break;

	        /* CDB: If EXPLAIN ANALYZE, let bitmap share our Instrumentation. */
	        if (scanState->ss.ps.instrument)
	        {
	            tbm_bitmap_set_instrument(bitmap, scanState->ss.ps.instrument);
	        }

			if(node->bitmap == NULL)
			{
				node->bitmap = (Node *)bitmap;
			}

			doscan = ExecIndexAdvanceArrayKeys(scanState->iss_ArrayKeys,
												   scanState->iss_NumArrayKeys);
			if (doscan)
			{
				/* reset index scan */
				index_rescan(scanState->iss_ScanDesc, scanState->iss_ScanKeys);
			}
		}
	}

	IndexScan_EndIndexPartition(scanState);

	/* must provide our own instrumentation support */
	if (scanState->ss.ps.instrument)
	{
		InstrStopNode(scanState->ss.ps.instrument, 1 /* nTuples */);
	}

	return (Node *) bitmap;
}

/* ----------------------------------------------------------------
 *		ExecBitmapIndexReScan(node)
 *
 *		Recalculates the value of the scan keys whose value depends on
 *		information known at runtime and rescans the indexed relation.
 * ----------------------------------------------------------------
 */
void
ExecBitmapIndexReScan(BitmapIndexScanState *node, ExprContext *exprCtxt)
{
	IndexScanState *scanState = (IndexScanState*)node;

	IndexScan_RescanIndex(scanState, exprCtxt, false, false, true);

	/* Sanity check */
	if ((node->bitmap) && (!IsA(node->bitmap, HashBitmap) && !IsA(node->bitmap, StreamBitmap)))
	{
		ereport(ERROR,
				(errcode(ERRCODE_GP_INTERNAL_ERROR),
				 errmsg("the returning bitmap in nodeBitmapIndexScan is invalid.")));
	}

	if(NULL != node->bitmap)
	{
		/* Only for optimizer BitmapIndexScan is in charge to free the bitmap */
		if (PLANGEN_OPTIMIZER == node->indexScanState.ss.ps.state->es_plannedstmt->planGen)
		{
			tbm_bitmap_free(node->bitmap);
		}
		else
		{
			/* reset hashBitmap */
			if(node->bitmap && IsA(node->bitmap, HashBitmap))
			{
				tbm_bitmap_free(node->bitmap);
			}
			else
			{
				/* XXX: we leak here */
				/* XXX: put in own memory context? */
			}
		}
		node->bitmap = NULL;
	}

	Gpmon_M_Incr(GpmonPktFromBitmapIndexScanState(node), GPMON_BITMAPINDEXSCAN_RESCAN);
	CheckSendPlanStateGpmonPkt(&scanState->ss.ps);
}

/* ----------------------------------------------------------------
 *		ExecEndBitmapIndexScan
 * ----------------------------------------------------------------
 */
void
ExecEndBitmapIndexScan(BitmapIndexScanState *node)
{
	IndexScanState *scanState = (IndexScanState*)node;

	IndexScan_EndIndexScan(scanState);
	Assert(SCAN_END == scanState->ss.scan_state);

	/* Only for optimizer BitmapIndexScan is in charge to free the bitmap */
	if(PLANGEN_OPTIMIZER == node->indexScanState.ss.ps.state->es_plannedstmt->planGen)
	{
		tbm_bitmap_free(node->bitmap);
	}
	node->bitmap = NULL;

	MemoryContextDelete(node->partitionMemoryContext);
	node->partitionMemoryContext = NULL;

	EndPlanStateGpmonPkt(&scanState->ss.ps);
}

/* ----------------------------------------------------------------
 *		ExecInitBitmapIndexScan
 *
 *		Initializes the index scan's state information.
 * ----------------------------------------------------------------
 */
BitmapIndexScanState *
ExecInitBitmapIndexScan(BitmapIndexScan *node, EState *estate, int eflags)
{
	BitmapIndexScanState *indexstate;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	/*
	 * create state structure
	 */
	indexstate = makeNode(BitmapIndexScanState);
	indexstate->partitionMemoryContext = AllocSetContextCreate(CurrentMemoryContext,
			 "BitmapIndexScanPerPartition",
			 ALLOCSET_DEFAULT_MINSIZE,
			 ALLOCSET_DEFAULT_INITSIZE,
			 ALLOCSET_DEFAULT_MAXSIZE);

	IndexScanState *scanState = (IndexScanState*)indexstate;

	scanState->ss.ps.plan = (Plan *) node;
	scanState->ss.ps.state = estate;

	/*
	 * If we are just doing EXPLAIN (ie, aren't going to run the plan), stop
	 * here.  This allows an index-advisor plugin to EXPLAIN a plan containing
	 * references to nonexistent indexes.
	 */
	if (eflags & EXEC_FLAG_EXPLAIN_ONLY)
		return indexstate;

	IndexScan_BeginIndexScan(scanState, indexstate->partitionMemoryContext, false, false, true);

	/*
	 * We do not open or lock the base relation here.  We assume that an
	 * ancestor BitmapHeapScan node is holding AccessShareLock (or better) on
	 * the heap relation throughout the execution of the plan tree.
	 */
	Assert(NULL == scanState->ss.ss_currentRelation);

	initGpmonPktForBitmapIndexScan((Plan *)node, &scanState->ss.ps.gpmon_pkt, estate);

	return indexstate;
}

int
ExecCountSlotsBitmapIndexScan(BitmapIndexScan *node)
{
	return ExecCountSlotsNode(outerPlan((Plan *) node)) +
		ExecCountSlotsNode(innerPlan((Plan *) node)) + BITMAPINDEXSCAN_NSLOTS;
}


void
initGpmonPktForBitmapIndexScan(Plan *planNode, gpmon_packet_t *gpmon_pkt, EState *estate)
{
	Assert(NULL != planNode && NULL != gpmon_pkt && IsA(planNode, BitmapIndexScan));

	{
		char *relname = get_rel_name(((BitmapIndexScan *)planNode)->indexid);
		
		Assert(GPMON_BITMAPINDEXSCAN_TOTAL <= (int)GPMON_QEXEC_M_COUNT);
		InitPlanNodeGpmonPkt(planNode, gpmon_pkt, estate, PMNT_BitmapIndexScan,
							 (int64)planNode->plan_rows, 
							 relname);
		if (NULL != relname)
		{
			pfree(relname);
		}
	}
	
}
