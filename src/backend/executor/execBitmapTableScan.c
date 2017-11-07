/*-------------------------------------------------------------------------
 *
 * execBitmapTableScan.c
 *	  Support routines for nodeBitmapTableScan.c
 *
 * Portions Copyright (c) 2014-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/execBitmapTableScan.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "access/relscan.h"
#include "access/transam.h"
#include "executor/execdebug.h"
#include "executor/nodeBitmapTableScan.h"
#include "pgstat.h"
#include "storage/bufmgr.h"
#include "utils/memutils.h"
#include "miscadmin.h"
#include "parser/parsetree.h"
#include "cdb/cdbvars.h" /* gp_select_invisible */
#include "cdb/cdbfilerepprimary.h"
#include "nodes/tidbitmap.h"

/*
 * Returns BitmapTableScanMethod for a given table type.
 */
static const ScanMethod *
getBitmapTableScanMethod(TableType tableType)
{
	Assert(tableType >= TableTypeHeap && tableType < TableTypeInvalid);

	/*
	 * scanMethods
	 *    Array that specifies different scan methods for various table types.
	 *
	 * The index in this array for a specific table type should match the enum value
	 * defined in TableType.
	 */
	static const ScanMethod scanMethods[] =
	{
		{
			&BitmapHeapScanNext, &BitmapHeapScanBegin, &BitmapHeapScanEnd,
			&BitmapHeapScanReScan, &MarkRestrNotAllowed, &MarkRestrNotAllowed
		},
		{
			&BitmapAOScanNext, &BitmapAOScanBegin, &BitmapAOScanEnd,
			&BitmapAOScanReScan, &MarkRestrNotAllowed, &MarkRestrNotAllowed
		},
		{
			/* The same set of methods serve both AO and AOCO scans */
			&BitmapAOScanNext, &BitmapAOScanBegin, &BitmapAOScanEnd,
			&BitmapAOScanReScan, &MarkRestrNotAllowed, &MarkRestrNotAllowed
		}
	};

	COMPILE_ASSERT(ARRAY_SIZE(scanMethods) == TableTypeInvalid);

	return &scanMethods[tableType];
}

/*
 * Initializes the state relevant to bitmaps.
 */
static inline void
initBitmapState(BitmapTableScanState *scanstate)
{
	if (scanstate->tbmres == NULL)
	{
		scanstate->tbmres =
			palloc0(sizeof(TBMIterateResult) +
					MAX_TUPLES_PER_PAGE * sizeof(OffsetNumber));
	}
}

/*
 * Frees the state relevant to bitmaps.
 */
static inline void
freeBitmapState(BitmapTableScanState *scanstate)
{
	/* BitmapIndexScan is the owner of the bitmap memory. Don't free it here */
	scanstate->tbm = NULL;

	/* BitmapTableScan created the iterator, so it is responsible to free its iterator */
	if (scanstate->tbmres != NULL)
	{
		pfree(scanstate->tbmres);
		scanstate->tbmres = NULL;
	}
}

static TupleTableSlot*
BitmapTableScanPlanQualTuple(BitmapTableScanState *node)
{
	EState	   *estate = node->ss.ps.state;
	Index		scanrelid = ((BitmapTableScan *) node->ss.ps.plan)->scan.scanrelid;
	ExprContext *econtext = node->ss.ps.ps_ExprContext;
	TupleTableSlot *slot = node->ss.ss_ScanTupleSlot;

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
			return ExecClearTuple(slot);
		}

		ExecStoreHeapTuple(estate->es_evTuple[scanrelid - 1], slot, InvalidBuffer, false);

		/* Does the tuple meet the original qual conditions? */
		econtext->ecxt_scantuple = slot;

		ResetExprContext(econtext);

		if (!ExecQual(node->bitmapqualorig, econtext, false))
		{
			ExecClearTuple(slot);		/* would not be returned by scan */
		}

		/* Flag for the next call that no more tuples */
		estate->es_evTupleNull[scanrelid - 1] = true;

		return slot;
	}

	return ExecClearTuple(slot);
}

/*
 * Reads a bitmap (with possibly many pages) from the underlying node.
 */
static void
readBitmap(BitmapTableScanState *scanState)
{
	if (scanState->tbm != NULL)
	{
		return;
	}

	Node *tbm = (Node *) MultiExecProcNode(outerPlanState(scanState));

	if (tbm != NULL && (!(IsA(tbm, HashBitmap) ||
						  IsA(tbm, StreamBitmap))))
	{
		elog(ERROR, "unrecognized result from subplan");
	}

	scanState->tbm = tbm;
	scanState->needNewBitmapPage = true;
}

/*
 * Reads the next bitmap page from the current bitmap.
 */
static bool
fetchNextBitmapPage(BitmapTableScanState *scanState)
{
	if (scanState->tbm == NULL)
	{
		return false;
	}

	TBMIterateResult *tbmres = (TBMIterateResult *)scanState->tbmres;

	Assert(scanState->needNewBitmapPage);

	bool gotBitmapPage = true;

	/* Set to 0 so that we can enter the while loop */
	tbmres->ntuples = 0;

	while (gotBitmapPage && tbmres->ntuples == 0)
	{
		gotBitmapPage = tbm_iterate(scanState->tbm, (TBMIterateResult *)scanState->tbmres);
	}

	if (gotBitmapPage)
	{
		scanState->iterator = NULL;
		scanState->needNewBitmapPage = false;

		if (tbmres->ntuples == BITMAP_IS_LOSSY)
		{
			scanState->isLossyBitmapPage = true;
		}
		else
		{
			scanState->isLossyBitmapPage = false;
		}
	}

	return gotBitmapPage;
}

/*
 * Checks eligibility of a tuple.
 *
 * Note, a tuple may fail to meet visibility requirement. Moreover,
 * for a lossy bitmap, we need to check for every tuple to make sure
 * that it satisfies the qual.
 */
bool
BitmapTableScanRecheckTuple(BitmapTableScanState *scanState, TupleTableSlot *slot)
{
	/*
	 * If we are using lossy info or we are required to recheck each tuple
	 * because of visibility or other causes, then evaluate the tuple
	 * eligibility.
	 */
	if (scanState->isLossyBitmapPage || scanState->recheckTuples)
	{
		ExprContext *econtext = scanState->ss.ps.ps_ExprContext;

		econtext->ecxt_scantuple = slot;
		ResetExprContext(econtext);

		return ExecQual(scanState->bitmapqualorig, econtext, false);
	}

	return true;
}

/*
 * Prepares for a new scan such as initializing bitmap states, preparing
 * the corresponding scan method etc.
 */
void
BitmapTableScanBegin(BitmapTableScanState *scanState, Plan *plan, EState *estate, int eflags)
{
	DynamicScan_Begin((ScanState *)scanState, plan, estate, eflags);
}

/*
 * Prepares for scanning of a new partition/relation.
 */
void
BitmapTableScanBeginPartition(ScanState *node, AttrNumber *attMap)
{
	Assert(NULL != node);
	BitmapTableScanState *scanState = (BitmapTableScanState *)node;
	BitmapTableScan *plan = (BitmapTableScan*)(node->ps.plan);

	initBitmapState(scanState);

	/* Remap the bitmapqualorig as we might have dropped column problem */
	DynamicScan_RemapExpression(node, attMap, (Node *) plan->bitmapqualorig);

	if (NULL == scanState->bitmapqualorig || NULL != attMap)
	{
		/* Always initialize new expressions in the per-partition memory context to prevent leaking */
		MemoryContext partitionContext = DynamicScan_GetPartitionMemoryContext(node);
		MemoryContext oldCxt = NULL;
		if (NULL != partitionContext)
		{
			oldCxt = MemoryContextSwitchTo(partitionContext);
		}

		scanState->bitmapqualorig = (List *)
			ExecInitExpr((Expr *) plan->bitmapqualorig,
						 (PlanState *) scanState);

		if (NULL != oldCxt)
		{
			MemoryContextSwitchTo(oldCxt);
		}
	}

	scanState->needNewBitmapPage = true;
	/*
	 * In some cases, the BitmapTableScan needs to re-evaluate the
	 * bitmap qual. This is determined by the recheckTuples and
	 * isLossyBitmapPage flags, as well as the type of table.
	 * The appropriate type of BitmapIndexScan will set this flag
	 * as follows:
	 *  Table/Index Type  Lossy    Recheck
	 *	Heap                1        1
	 * 	Ao/Lossy            1        0
	 *	Ao/Non-Lossy        0        0
	 *	Aocs/Lossy          1        0
	 *	Aocs/Non-Lossy      0        0
	 */
	scanState->recheckTuples = true;

	getBitmapTableScanMethod(node->tableType)->beginScanMethod(node);

	/*
	 * Prepare child node to produce new bitmaps for the new partition (and cleanup
	 * any leftover state from old partition).
	 */
	ExecReScan(outerPlanState(node), NULL);
}

/*
 * ReScan a partition
 */
void
BitmapTableScanReScanPartition(ScanState *node)
{
	BitmapTableScanState *scanState = (BitmapTableScanState *) node;

	freeBitmapState(scanState);
	Assert(scanState->tbm == NULL);

	initBitmapState(scanState);
	scanState->needNewBitmapPage = true;
	scanState->recheckTuples = true;

	getBitmapTableScanMethod(node->tableType)->reScanMethod(node);
}

/*
 * Cleans up once scanning of a partition/relation is done.
 */
void
BitmapTableScanEndPartition(ScanState *node)
{
	BitmapTableScanState *scanState = (BitmapTableScanState *) node;

	freeBitmapState(scanState);

	getBitmapTableScanMethod(node->tableType)->endScanMethod(node);

	Assert(scanState->tbm == NULL);
}

/*
 * Executes underlying scan method to fetch the next matching tuple.
 */
TupleTableSlot *
BitmapTableScanFetchNext(ScanState *node)
{
	BitmapTableScanState *scanState = (BitmapTableScanState *) node;
	TupleTableSlot *slot = BitmapTableScanPlanQualTuple(scanState);

	while (TupIsNull(slot))
	{
		/* If we haven't already obtained the required bitmap, do so */
		readBitmap(scanState);

		/* If we have exhausted the current bitmap page, fetch the next one */
		if (!scanState->needNewBitmapPage || fetchNextBitmapPage(scanState))
		{
			slot = ExecScan(&scanState->ss, (ExecScanAccessMtd) getBitmapTableScanMethod(scanState->ss.tableType)->accessMethod);
		}
		else
		{
			/*
			 * Needed a new bitmap page, but couldn't fetch one. Therefore,
			 * try the next partition.
			 */
			break;
		}
	}

	return slot;
}

/*
 * Cleans up after the scanning has finished.
 */
void
BitmapTableScanEnd(BitmapTableScanState *scanState)
{
	DynamicScan_End((ScanState *)scanState, BitmapTableScanEndPartition);
}

/*
 * Prepares for a rescan.
 */
void
BitmapTableScanReScan(BitmapTableScanState *node, ExprContext *exprCtxt)
{
	ScanState *scanState = &node->ss;
	DynamicScan_ReScan(scanState, exprCtxt);
	ExecReScan(outerPlanState(node), exprCtxt);
}
