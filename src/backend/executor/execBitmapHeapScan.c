/*-------------------------------------------------------------------------
 *
 * execBitmapHeapScan.c
 *	  Support routines for scanning Heap tables using bitmaps.
 *
 * Portions Copyright (c) 2014-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/execBitmapHeapScan.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "executor/executor.h"
#include "nodes/execnodes.h"

#include "access/heapam.h"
#include "access/relscan.h"
#include "access/transam.h"
#include "executor/execdebug.h"
#include "executor/nodeBitmapHeapscan.h"
#include "pgstat.h"
#include "storage/bufmgr.h"
#include "utils/memutils.h"
#include "miscadmin.h"
#include "parser/parsetree.h"
#include "cdb/cdbvars.h" /* gp_select_invisible */
#include "cdb/cdbfilerepprimary.h"
#include "nodes/tidbitmap.h"

/*
 * Prepares for a new heap scan.
 */
void
BitmapHeapScanBegin(ScanState *scanState)
{
	BitmapTableScanState *node = (BitmapTableScanState *)scanState;
	Relation currentRelation = node->ss.ss_currentRelation;
	EState *estate = node->ss.ps.state;

	Assert(node->scanDesc == NULL);

	/*
	 * Even though we aren't going to do a conventional seqscan, it is useful
	 * to create a HeapScanDesc --- most of the fields in it are usable.
	 */
	node->scanDesc = heap_beginscan_bm(currentRelation,
									   estate->es_snapshot,
									   0,
									   NULL);

	/*
	 * Heap always needs rechecking each tuple because of potential
	 * visibility issue (we don't store MVCC info in the index).
	 */
	node->recheckTuples = true;
}

/*
 * Cleans up after the scanning is done.
 */
void
BitmapHeapScanEnd(ScanState *scanState)
{
	BitmapTableScanState *node = (BitmapTableScanState *)scanState;

	/*
	 * We might call "End" method before even calling init method,
	 * in case we had an ERROR. Ignore scanDesc cleanup in such cases
	 */
	if (NULL != node->scanDesc)
	{
		heap_endscan((HeapScanDesc)node->scanDesc);
		node->scanDesc = NULL;
	}

	if (NULL != node->iterator)
	{
		pfree(node->iterator);
		node->iterator = NULL;
	}
}

/*
 * Returns the next matching tuple.
 */
TupleTableSlot *
BitmapHeapScanNext(ScanState *scanState)
{
	BitmapTableScanState *node = (BitmapTableScanState *)scanState;
	Assert((node->ss.scan_state & SCAN_SCAN) != 0);

	/*
	 * extract necessary information from index scan node
	 */
	TupleTableSlot *slot = node->ss.ss_ScanTupleSlot;

	HeapScanDesc scan = node->scanDesc;

	TBMIterateResult *tbmres = (TBMIterateResult *)node->tbmres;
	Assert(tbmres != NULL && tbmres->ntuples != 0);
	Assert(node->needNewBitmapPage == false);

	for (;;)
	{
		CHECK_FOR_INTERRUPTS();

		if (node->iterator == NULL)
		{
			/*
			 * Fetch the current heap page and identify candidate tuples.
			 */
			bitgetpage(scan, tbmres);

			/*
		 	* Set rs_cindex to first slot to examine
		 	*/
			scan->rs_cindex = 0;
			/*
			 * The nullity of the iterator is used to check if
			 * we need a new iterator to process a new bitmap page.
			 * Note: the bitmap page is provided by BitmapTableScan.
			 * This iterator is supposed to maintain the cursor position
			 * in the heap page that it is scanning. However, for heap
			 * tables we already have such cursor state as part of ScanState,
			 * and so, we just use a dummy allocation here to indicate
			 * ourselves that we have finished initialization for processing
			 * a new bitmap page.
			 */
			node->iterator = palloc(0);
		}
		else
		{
			/*
			 * Continuing in previously obtained page; advance rs_cindex
			 */
			scan->rs_cindex++;
		}

		/*
		 * If we reach the end of the relation or if we are out of range or
		 * nothing more to look at on this page, then request a new bitmap page.
		 */
		if (tbmres->blockno >= scan->rs_nblocks || scan->rs_cindex < 0 ||
				scan->rs_cindex >= scan->rs_ntuples)
		{
			Assert(NULL != node->iterator);
			pfree(node->iterator);
			node->iterator = NULL;

			node->needNewBitmapPage = true;
			return ExecClearTuple(slot);
		}

		/*
		 * Okay to fetch the tuple
		 */
		OffsetNumber targoffset = scan->rs_vistuples[scan->rs_cindex];
		Page		dp = (Page) BufferGetPage(scan->rs_cbuf);
		ItemId		lp = PageGetItemId(dp, targoffset);
		Assert(ItemIdIsUsed(lp));

		scan->rs_ctup.t_data = (HeapTupleHeader) PageGetItem((Page) dp, lp);
		scan->rs_ctup.t_len = ItemIdGetLength(lp);
		ItemPointerSet(&scan->rs_ctup.t_self, tbmres->blockno, targoffset);

		pgstat_count_heap_fetch(scan->rs_rd);

		/*
		 * Set up the result slot to point to this tuple. Note that the slot
		 * acquires a pin on the buffer.
		 */
		ExecStoreHeapTuple(&scan->rs_ctup,
					   slot,
					   scan->rs_cbuf,
					   false);

		if (!BitmapTableScanRecheckTuple(node, slot))
		{
			ExecClearTuple(slot);
			continue;
		}

		return slot;
	}

	/*
	 * We should never reach here as the termination is handled
	 * from nodeBitmapTableScan.
	 */
	Assert(false);
	return NULL;
}

/*
 * Prepares for a re-scan.
 */
void
BitmapHeapScanReScan(ScanState *scanState)
{
	BitmapTableScanState *node = (BitmapTableScanState *)scanState;
	Assert(node->scanDesc != NULL);

	/* rescan to release any page pin */
	heap_rescan(node->scanDesc, NULL);
}
