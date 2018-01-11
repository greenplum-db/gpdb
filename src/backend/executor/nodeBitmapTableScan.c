/*-------------------------------------------------------------------------
 *
 * nodeBitmapTableScan.c
 *	  Routines to support bitmapped scans of relations
 *
 * NOTE: it is critical that this plan type only be used with MVCC-compliant
 * snapshots (ie, regular snapshots, not SnapshotNow or one of the other
 * special snapshots).	The reason is that since index and heap scans are
 * decoupled, there can be no assurance that the index tuple prompting a
 * visit to a particular heap TID still exists when the visit is made.
 * Therefore the tuple might not exist anymore either (which is OK because
 * heap_fetch will cope) --- but worse, the tuple slot could have been
 * re-used for a newer tuple.  With an MVCC snapshot the newer tuple is
 * certain to fail the time qual and so it will not be mistakenly returned.
 * With SnapshotNow we might return a tuple that doesn't meet the required
 * index qual conditions.
 *
 * Portions Copyright (c) 2014-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeBitmapTableScan.c
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		ExecInitBitmapTableScan		creates and initializes state info.
 *		ExecBitmapTableScan			scans a relation using bitmap info
 *		ExecBitmapTableReScan		prepares to rescan the plan.
 *		ExecEndBitmapTableScan		releases all storage.
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
#include "cdb/cdbpartition.h"

#define BITMAPTABLESCAN_NSLOTS 2

/*
 * Initializes the BitmapTableScanState, including creation of the
 * scan description and the bitmapqualorig.
 */
BitmapTableScanState *
ExecInitBitmapTableScan(BitmapTableScan *node, EState *estate, int eflags)
{
	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));
	/*
	 * Assert caller didn't ask for an unsafe snapshot --- see comments at
	 * head of file.
	 *
	 * MPP-4703: the MVCC-snapshot restriction is required for correct results.
	 * our test-mode may deliberately return incorrect results, but that's OK.
	 */
	Assert(IsMVCCSnapshot(estate->es_snapshot) || gp_select_invisible);

	BitmapTableScanState *state = makeNode(BitmapTableScanState);

	BitmapTableScanBegin(state, (Plan *) node, estate, eflags);

	/*
	 * initialize child nodes
	 *
	 * We do this last because the child nodes will open indexscans on our
	 * relation's indexes, and we want to be sure we have acquired a lock on
	 * the relation first.
	 */
	outerPlanState(state) = ExecInitNode(outerPlan(node), estate, eflags);

	return state;
}

/*
 * Retrieves the next tuple from the BitmapTableScan's underlying relation.
 */
TupleTableSlot *
ExecBitmapTableScan(BitmapTableScanState *node)
{
	ScanState *scanState = (ScanState *)node;

	TupleTableSlot *slot = DynamicScan_GetNextTuple(scanState, BitmapTableScanBeginPartition,
			BitmapTableScanEndPartition, BitmapTableScanReScanPartition, BitmapTableScanFetchNext);

	if (TupIsNull(slot) && !scanState->ps.delayEagerFree)
	{
		ExecEagerFreeBitmapTableScan(node);
	}

	return slot;
}

/*
 * Prepares the BitmapTableScanState for a re-scan.
 */
void
ExecBitmapTableReScan(BitmapTableScanState *node, ExprContext *exprCtxt)
{
	BitmapTableScanReScan(node, exprCtxt);
	/*
	 * Always rescan the input immediately, to ensure we can pass down any
	 * outer tuple that might be used in index quals.
	 */
	ExecReScan(outerPlanState(node), exprCtxt);
}

/* Cleans up once scanning is finished */
void
ExecEndBitmapTableScan(BitmapTableScanState *node)
{
	BitmapTableScanEnd(node);

	/*
	 * close down subplans
	 */
	ExecEndNode(outerPlanState(node));

	EndPlanStateGpmonPkt(&node->ss.ps);
}

/* Returns the number of slots needed for this operator */
int
ExecCountSlotsBitmapTableScan(BitmapTableScan *node)
{
	return ExecCountSlotsNode(outerPlan((Plan *) node)) +
		ExecCountSlotsNode(innerPlan((Plan *) node)) + BITMAPTABLESCAN_NSLOTS;
}

/* Eagerly free memory held for scanning */
void
ExecEagerFreeBitmapTableScan(BitmapTableScanState *node)
{
	DynamicScan_End((ScanState *)node, BitmapTableScanEndPartition);
}
