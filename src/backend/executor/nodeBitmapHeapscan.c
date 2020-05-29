/*-------------------------------------------------------------------------
 *
 * nodeBitmapHeapscan.c
 *	  Routines to support bitmapped scans of relations
 *
 * NOTE: it is critical that this plan type only be used with MVCC-compliant
 * snapshots (ie, regular snapshots, not SnapshotAny or one of the other
 * special snapshots).  The reason is that since index and heap scans are
 * decoupled, there can be no assurance that the index tuple prompting a
 * visit to a particular heap TID still exists when the visit is made.
 * Therefore the tuple might not exist anymore either (which is OK because
 * heap_fetch will cope) --- but worse, the tuple slot could have been
 * re-used for a newer tuple.  With an MVCC snapshot the newer tuple is
 * certain to fail the time qual and so it will not be mistakenly returned,
 * but with anything else we might return a tuple that doesn't meet the
 * required index qual conditions.
 *
 * In GPDB, this also deals with AppendOnly and AOCS tables. The prefetching
 * hasn't been implemented for them, though.
 *
 * This can also be used in "Dynamic" mode.
 *
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 * Portions Copyright (c) 2008-2009, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeBitmapHeapscan.c
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		ExecBitmapHeapScan			scans a relation using bitmap info
 *		ExecBitmapHeapNext			workhorse for above
 *		ExecInitBitmapHeapScan		creates and initializes state info.
 *		ExecReScanBitmapHeapScan	prepares to rescan the plan.
 *		ExecEndBitmapHeapScan		releases all storage.
 */
#include "postgres.h"

#include "access/relscan.h"
#include "access/transam.h"
#include "executor/execdebug.h"
#include "executor/nodeBitmapHeapscan.h"
#include "pgstat.h"
#include "storage/bufmgr.h"
#include "storage/predicate.h"
#include "utils/memutils.h"
#include "parser/parsetree.h"
#include "nodes/tidbitmap.h"
#include "utils/rel.h"
#include "utils/spccache.h"
#include "utils/snapmgr.h"
#include "utils/tqual.h"

#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbvars.h" /* gp_select_invisible */

static TupleTableSlot *BitmapHeapNext(BitmapHeapScanState *node);
static TupleTableSlot *BitmapAppendOnlyNext(BitmapHeapScanState *node);
static void bitgetpage(HeapScanDesc scan, TBMIterateResult *tbmres);

static void ExecEagerFreeBitmapHeapScan(BitmapHeapScanState *node);

/*
 * Initialize the fetch descriptor, if it is not initialized.
 */
static void
initFetchDesc(BitmapHeapScanState *scanstate)
{
	BitmapHeapScan *node = (BitmapHeapScan *)(scanstate->ss.ps.plan);
	Relation	currentRelation = scanstate->ss.ss_currentRelation;
	EState	   *estate = scanstate->ss.ps.state;

	if (RelationIsAoRows(currentRelation))
	{
		if (scanstate->bhs_currentAOFetchDesc == NULL)
		{
			Snapshot	appendOnlyMetaDataSnapshot;

			appendOnlyMetaDataSnapshot = estate->es_snapshot;
			if (appendOnlyMetaDataSnapshot == SnapshotAny)
			{
				/*
				 * the append-only meta data should never be fetched with
				 * SnapshotAny as bogus results are returned.
				 */
				appendOnlyMetaDataSnapshot = GetTransactionSnapshot();
			}
			scanstate->bhs_currentAOFetchDesc =
				appendonly_fetch_init(currentRelation,
									  estate->es_snapshot,
									  appendOnlyMetaDataSnapshot);
			scanstate->bhs_currentAOCSFetchDesc = NULL;
			scanstate->bhs_currentAOCSLossyFetchDesc = NULL;
		}
	}
	else if (RelationIsAoCols(currentRelation))
	{
		if (scanstate->bhs_currentAOCSFetchDesc == NULL)
		{
			bool	   *proj;
			bool	   *projLossy;
			int			colno;
			Snapshot	appendOnlyMetaDataSnapshot;

			appendOnlyMetaDataSnapshot = estate->es_snapshot;
			if (appendOnlyMetaDataSnapshot == SnapshotAny)
			{
				/*
				 * the append-only meta data should never be fetched with
				 * SnapshotAny as bogus results are returned.
				 */
				appendOnlyMetaDataSnapshot = GetTransactionSnapshot();
			}

			/*
			 * Obtain the projection.
			 */
			Assert(currentRelation->rd_att != NULL);

			proj = (bool *) palloc0(currentRelation->rd_att->natts * sizeof(bool));
			projLossy = (bool *) palloc0(currentRelation->rd_att->natts * sizeof(bool));

			GetNeededColumnsForScan((Node *) node->scan.plan.targetlist, proj, currentRelation->rd_att->natts);
			GetNeededColumnsForScan((Node *) node->scan.plan.qual, proj, currentRelation->rd_att->natts);

			memcpy(projLossy,proj, currentRelation->rd_att->natts * sizeof(bool));

			GetNeededColumnsForScan((Node *) node->bitmapqualorig, projLossy, currentRelation->rd_att->natts);

			for (colno = 0; colno < currentRelation->rd_att->natts; colno++)
			{
				if (proj[colno])
					break;
			}

			/*
			 * At least project one column. Since the tids stored in the index may not have
			 * a correponding tuple any more (because of previous crashes, for example), we
			 * need to read the tuple to make sure.
			 */
			if (colno == currentRelation->rd_att->natts)
				proj[0] = true;

			scanstate->bhs_currentAOCSFetchDesc =
				aocs_fetch_init(currentRelation, estate->es_snapshot, appendOnlyMetaDataSnapshot, proj);

			for (colno = 0; colno < currentRelation->rd_att->natts; colno++)
			{
				if (projLossy[colno])
					break;
			}

			/*
			 * At least project one column. Since the tids stored in the index may not have
			 * a correponding tuple any more (because of previous crashes, for example), we
			 * need to read the tuple to make sure.
			 */
			if (colno == currentRelation->rd_att->natts)
				projLossy[0] = true;

			scanstate->bhs_currentAOCSLossyFetchDesc =
				aocs_fetch_init(currentRelation, estate->es_snapshot, appendOnlyMetaDataSnapshot, projLossy);
		}
	}
	else
	{
		if (scanstate->bhs_currentScanDesc_heap == NULL)
		{
			/*
			 * Even though we aren't going to do a conventional seqscan, it is useful
			 * to create a HeapScanDesc --- most of the fields in it are usable.
			 */
			scanstate->bhs_currentScanDesc_heap = heap_beginscan_bm(currentRelation,
																	estate->es_snapshot,
																	0,
																	NULL);
		}
	}
}

/*
 * Free fetch descriptor.
 */
static void
freeFetchDesc(BitmapHeapScanState *scanstate)
{
	if (scanstate->bhs_currentAOFetchDesc != NULL)
	{
		appendonly_fetch_finish(scanstate->bhs_currentAOFetchDesc);
		pfree(scanstate->bhs_currentAOFetchDesc);
		scanstate->bhs_currentAOFetchDesc = NULL;
	}

	if (scanstate->bhs_currentAOCSFetchDesc != NULL)
	{
		aocs_fetch_finish(scanstate->bhs_currentAOCSFetchDesc);
		pfree(scanstate->bhs_currentAOCSFetchDesc);
		scanstate->bhs_currentAOCSFetchDesc = NULL;
	}

	if (scanstate->bhs_currentAOCSLossyFetchDesc != NULL)
	{
		aocs_fetch_finish(scanstate->bhs_currentAOCSLossyFetchDesc);
		pfree(scanstate->bhs_currentAOCSLossyFetchDesc);
		scanstate->bhs_currentAOCSLossyFetchDesc = NULL;
	}
	if (scanstate->bhs_currentScanDesc_heap != NULL)
	{
		heap_endscan(scanstate->bhs_currentScanDesc_heap);
		scanstate->bhs_currentScanDesc_heap = NULL;
	}
}

/*
 * Free the state relevant to bitmaps
 */
static void
freeBitmapState(BitmapHeapScanState *scanstate)
{
	/* BitmapIndexScan is the owner of the bitmap memory. Don't free it here */
	scanstate->tbm = NULL;
	/* Likewise, the tbmres member is owned by the iterator. It'll be freed
	 * during end_iterate. */
	scanstate->tbmres = NULL;
	if (scanstate->tbmiterator)
		tbm_generic_end_iterate(scanstate->tbmiterator);
	scanstate->tbmiterator = NULL;
	if (scanstate->prefetch_iterator)
		tbm_generic_end_iterate(scanstate->prefetch_iterator);
	scanstate->prefetch_iterator = NULL;
}

/* ----------------------------------------------------------------
 *		BitmapHeapNext
 *
 *		Retrieve next tuple from the BitmapHeapScan node's currentRelation
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
BitmapHeapNext(BitmapHeapScanState *node)
{
	ExprContext *econtext;
	HeapScanDesc scan;
	Node  		*tbm;
	GenericBMIterator *tbmiterator;
	TBMIterateResult *tbmres;
#ifdef USE_PREFETCH
	GenericBMIterator *prefetch_iterator;
#endif

	OffsetNumber targoffset;
	TupleTableSlot *slot;

	initFetchDesc(node);

	/*
	 * extract necessary information from index scan node
	 */
	econtext = node->ss.ps.ps_ExprContext;
	slot = node->ss.ss_ScanTupleSlot;
	scan = node->bhs_currentScanDesc_heap;

	tbm = node->tbm;
	tbmiterator = node->tbmiterator;
	tbmres = node->tbmres;
#ifdef USE_PREFETCH
	prefetch_iterator = node->prefetch_iterator;
#endif

	/*
	 * If we haven't yet performed the underlying index scan, do it, and begin
	 * the iteration over the bitmap.
	 *
	 * For prefetching, we use *two* iterators, one for the pages we are
	 * actually scanning and another that runs ahead of the first for
	 * prefetching.  node->prefetch_pages tracks exactly how many pages ahead
	 * the prefetch iterator is.  Also, node->prefetch_target tracks the
	 * desired prefetch distance, which starts small and increases up to the
	 * node->prefetch_maximum.  This is to avoid doing a lot of prefetching in
	 * a scan that stops after a few tuples because of a LIMIT.
	 */
	if (tbm == NULL)
	{
		tbm = (Node *) MultiExecProcNode(outerPlanState(node));

		if (!tbm || !(IsA(tbm, TIDBitmap) || IsA(tbm, StreamBitmap)))
			elog(ERROR, "unrecognized result from subplan");

		node->tbm = tbm;
		node->tbmiterator = tbmiterator = tbm_generic_begin_iterate(tbm);
		node->tbmres = tbmres = NULL;

#ifdef USE_PREFETCH
		if (node->prefetch_maximum > 0)
		{
			node->prefetch_iterator = prefetch_iterator = tbm_generic_begin_iterate(tbm);
			node->prefetch_pages = 0;
			node->prefetch_target = -1;
		}
#endif   /* USE_PREFETCH */
	}

	for (;;)
	{
		Page		dp;
		ItemId		lp;

		if (tbmres == NULL || tbmres->ntuples == 0)
		{
			CHECK_FOR_INTERRUPTS();

			if (QueryFinishPending)
				return NULL;

			node->tbmres = tbmres = tbm_generic_iterate(tbmiterator);

			if (tbmres == NULL)
			{
				/* no more entries in the bitmap */
				break;
			}

#ifdef USE_PREFETCH
			if (node->prefetch_pages > 0)
			{
				/* The main iterator has closed the distance by one page */
				node->prefetch_pages--;
			}
			else if (prefetch_iterator)
			{
				/* Do not let the prefetch iterator get behind the main one */
				TBMIterateResult *tbmpre = tbm_generic_iterate(prefetch_iterator);

				if (tbmpre == NULL || tbmpre->blockno != tbmres->blockno)
					elog(ERROR, "prefetch and main iterators are out of sync");
			}
#endif   /* USE_PREFETCH */

			/*
			 * Ignore any claimed entries past what we think is the end of
			 * the relation.  (This is probably not necessary given that we
			 * got at least AccessShareLock on the table before performing
			 * any of the indexscans, but let's be safe.)
			 */
			if (tbmres->blockno >= scan->rs_nblocks)
			{
				tbmres->ntuples = 0;
				continue;
			}

			/* If tbmres contains no tuples, continue. */
			if (tbmres->ntuples == 0)
				continue;

			/*
			 * Fetch the current heap page and identify candidate tuples.
			 */
			bitgetpage(scan, tbmres);

			if (tbmres->ntuples >= 0)
				node->exact_pages++;
			else
				node->lossy_pages++;

			/*
			 * Set rs_cindex to first slot to examine
			 */
			scan->rs_cindex = 0;

#ifdef USE_PREFETCH

			/*
			 * Increase prefetch target if it's not yet at the max.  Note that
			 * we will increase it to zero after fetching the very first
			 * page/tuple, then to one after the second tuple is fetched, then
			 * it doubles as later pages are fetched.
			 */
			if (node->prefetch_target >= node->prefetch_maximum)
				 /* don't increase any further */ ;
			else if (node->prefetch_target >= node->prefetch_maximum / 2)
				node->prefetch_target = node->prefetch_maximum;
			else if (node->prefetch_target > 0)
				node->prefetch_target *= 2;
			else
				node->prefetch_target++;
#endif   /* USE_PREFETCH */
		}
		else
		{
			/*
			 * Continuing in previously obtained page; advance rs_cindex
			 */
			scan->rs_cindex++;
			tbmres->ntuples--;

#ifdef USE_PREFETCH

			/*
			 * Try to prefetch at least a few pages even before we get to the
			 * second page if we don't stop reading after the first tuple.
			 */
			if (node->prefetch_target < node->prefetch_maximum)
				node->prefetch_target++;
#endif   /* USE_PREFETCH */
		}

		/*
		 * Out of range?  If so, nothing more to look at on this page
		 */
		if (scan->rs_cindex < 0 || scan->rs_cindex >= scan->rs_ntuples)
		{
			tbmres->ntuples = 0;
			continue;
		}

#ifdef USE_PREFETCH

		/*
		 * We issue prefetch requests *after* fetching the current page to try
		 * to avoid having prefetching interfere with the main I/O. Also, this
		 * should happen only when we have determined there is still something
		 * to do on the current page, else we may uselessly prefetch the same
		 * page we are just about to request for real.
		 */
		if (prefetch_iterator)
		{
			while (node->prefetch_pages < node->prefetch_target)
			{
				TBMIterateResult *tbmpre = tbm_generic_iterate(prefetch_iterator);

				if (tbmpre == NULL)
				{
					/* No more pages to prefetch */
					tbm_generic_end_iterate(prefetch_iterator);
					node->prefetch_iterator = prefetch_iterator = NULL;
					break;
				}
				node->prefetch_pages++;
				PrefetchBuffer(scan->rs_rd, MAIN_FORKNUM, tbmpre->blockno);
			}
		}
#endif   /* USE_PREFETCH */

		/*
		 * Okay to fetch the tuple
		 */
		targoffset = scan->rs_vistuples[scan->rs_cindex];
		dp = (Page) BufferGetPage(scan->rs_cbuf);
		lp = PageGetItemId(dp, targoffset);
		Assert(ItemIdIsNormal(lp));

		scan->rs_ctup.t_data = (HeapTupleHeader) PageGetItem((Page) dp, lp);
		scan->rs_ctup.t_len = ItemIdGetLength(lp);
#if 0
		scan->rs_ctup.t_tableOid = scan->rs_rd->rd_id;
#endif
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

		/*
		 * If we are using lossy info, we have to recheck the qual conditions
		 * at every tuple.
		 */
		if (tbmres->recheck)
		{
			econtext->ecxt_scantuple = slot;
			ResetExprContext(econtext);

			if (!ExecQual(node->bitmapqualorig, econtext, false))
			{
				/* Fails recheck, so drop it and loop back for another */
				InstrCountFiltered2(node, 1);
				ExecClearTuple(slot);
				continue;
			}
		}

		/* OK to return this tuple */
		return slot;
	}

	ExecEagerFreeBitmapHeapScan(node);

	/*
	 * if we get here it means we are at the end of the scan..
	 */
	return ExecClearTuple(slot);
}

/*
 * bitgetpage - subroutine for BitmapHeapNext()
 *
 * This routine reads and pins the specified page of the relation, then
 * builds an array indicating which tuples on the page are both potentially
 * interesting according to the bitmap, and visible according to the snapshot.
 */
static void
bitgetpage(HeapScanDesc scan, TBMIterateResult *tbmres)
{
	BlockNumber page = tbmres->blockno;
	Buffer		buffer;
	Snapshot	snapshot;
	int			ntup;

	/*
	 * Acquire pin on the target heap page, trading in any pin we held before.
	 */
	Assert(page < scan->rs_nblocks);

	scan->rs_cbuf = ReleaseAndReadBuffer(scan->rs_cbuf,
										 scan->rs_rd,
										 page);
	buffer = scan->rs_cbuf;
	snapshot = scan->rs_snapshot;

	ntup = 0;

	/*
	 * Prune and repair fragmentation for the whole page, if possible.
	 */
	heap_page_prune_opt(scan->rs_rd, buffer);

	/*
	 * We must hold share lock on the buffer content while examining tuple
	 * visibility.  Afterwards, however, the tuples we have found to be
	 * visible are guaranteed good as long as we hold the buffer pin.
	 */
	LockBuffer(buffer, BUFFER_LOCK_SHARE);

	/*
	 * We need two separate strategies for lossy and non-lossy cases.
	 */
	if (tbmres->ntuples >= 0)
	{
		/*
		 * Bitmap is non-lossy, so we just look through the offsets listed in
		 * tbmres; but we have to follow any HOT chain starting at each such
		 * offset.
		 */
		int			curslot;

		for (curslot = 0; curslot < tbmres->ntuples; curslot++)
		{
			OffsetNumber offnum = tbmres->offsets[curslot];
			ItemPointerData tid;
			HeapTupleData heapTuple;

			ItemPointerSet(&tid, page, offnum);
			if (heap_hot_search_buffer(&tid, scan->rs_rd, buffer, snapshot,
									   &heapTuple, NULL, true))
				scan->rs_vistuples[ntup++] = ItemPointerGetOffsetNumber(&tid);
		}
	}
	else
	{
		/*
		 * Bitmap is lossy, so we must examine each item pointer on the page.
		 * But we can ignore HOT chains, since we'll check each tuple anyway.
		 */
		Page		dp = (Page) BufferGetPage(buffer);
		OffsetNumber maxoff = PageGetMaxOffsetNumber(dp);
		OffsetNumber offnum;

		for (offnum = FirstOffsetNumber; offnum <= maxoff; offnum = OffsetNumberNext(offnum))
		{
			ItemId		lp;
			HeapTupleData loctup;
			bool		valid;

			lp = PageGetItemId(dp, offnum);
			if (!ItemIdIsNormal(lp))
				continue;
			loctup.t_data = (HeapTupleHeader) PageGetItem((Page) dp, lp);
			loctup.t_len = ItemIdGetLength(lp);
			ItemPointerSet(&loctup.t_self, page, offnum);
			valid = HeapTupleSatisfiesVisibility(scan->rs_rd, &loctup, snapshot, buffer);
			if (valid)
			{
				scan->rs_vistuples[ntup++] = offnum;
				PredicateLockTuple(scan->rs_rd, &loctup, snapshot);
			}
			CheckForSerializableConflictOut(valid, scan->rs_rd, &loctup,
											buffer, snapshot);
		}
	}

	LockBuffer(buffer, BUFFER_LOCK_UNLOCK);

	Assert(ntup <= MaxHeapTuplesPerPage);
	scan->rs_ntuples = ntup;
}

/* ----------------------------------------------------------------
 *		BitmapAppendOnlyNext
 *
 *		Like BitmapHeapNext(), but used when the table is an AO or AOCS table.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
BitmapAppendOnlyNext(BitmapHeapScanState *node)
{
	EState	   *estate;
	ExprContext *econtext;
	AppendOnlyFetchDesc aoFetchDesc;
	AOCSFetchDesc aocsFetchDesc;
	AOCSFetchDesc aocsLossyFetchDesc;
	Index		scanrelid;
	GenericBMIterator *tbmiterator;
	OffsetNumber psuedoHeapOffset;
	ItemPointerData psudeoHeapTid;
	AOTupleId	aoTid;
	TupleTableSlot *slot;

	/*
	 * extract necessary information from index scan node
	 */
	estate = node->ss.ps.state;
	econtext = node->ss.ps.ps_ExprContext;
	slot = node->ss.ss_ScanTupleSlot;

	initFetchDesc(node);

	aoFetchDesc = node->bhs_currentAOFetchDesc;
	aocsFetchDesc = node->bhs_currentAOCSFetchDesc;
	aocsLossyFetchDesc = node->bhs_currentAOCSLossyFetchDesc;
	scanrelid = ((BitmapHeapScan *) node->ss.ps.plan)->scan.scanrelid;
	tbmiterator = node->tbmiterator;

	/*
	 * If we haven't yet performed the underlying index scan, or
	 * we have used up the bitmaps from the previous scan, do the next scan,
	 * and prepare the bitmap to be iterated over.
	 */
	if (tbmiterator == NULL)
	{
		Node *tbm = (Node *) MultiExecProcNode(outerPlanState(node));

		if (!tbm || !(IsA(tbm, TIDBitmap) || IsA(tbm, StreamBitmap)))
			elog(ERROR, "unrecognized result from subplan");

		if (tbm == NULL)
		{
			ExecEagerFreeBitmapHeapScan(node);

			return ExecClearTuple(slot);
		}

		node->tbmiterator = tbmiterator = tbm_generic_begin_iterate(tbm);
	}

	Assert(tbmiterator != NULL);

	for (;;)
	{
		TBMIterateResult *tbmres = node->tbmres;
		bool		need_recheck = false;

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
			node->tbmres = tbmres = tbm_generic_iterate(tbmiterator);

			if (tbmres == NULL)
			{
				/* no more entries in the bitmap */
				break;
			}

			/* If tbmres contains no tuples, continue. */
			if (tbmres->ntuples == 0)
				continue;

			/* Make sure we never cross 15-bit offset number [MPP-24326] */
			Assert(tbmres->ntuples <= INT16_MAX + 1);

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

		if (node->baos_lossy || tbmres->recheck)
			need_recheck = true;

		/*
		 * Must account for lossy page info...
		 */
		if (node->baos_lossy)
		{
			/*
			 * +1 to convert index to offset, since TID offsets are not zero
			 * based.
			 */
			psuedoHeapOffset = node->baos_cindex + 1;	// We are iterating through all items.
		}
		else
		{
			Assert(node->baos_cindex <= tbmres->ntuples);
			psuedoHeapOffset = tbmres->offsets[node->baos_cindex];
		}

		/*
		 * Okay to fetch the tuple
		 */
		ItemPointerSet(&psudeoHeapTid,
					   tbmres->blockno,
					   psuedoHeapOffset);

		tbm_convert_appendonly_tid_out(&psudeoHeapTid, &aoTid);

		if (aoFetchDesc != NULL)
		{
			appendonly_fetch(aoFetchDesc, &aoTid, slot);
		}
		else
		{
			if (need_recheck)
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
		if (need_recheck)
		{
			econtext->ecxt_scantuple = slot;
			ResetExprContext(econtext);

			if (!ExecQual(node->bitmapqualorig, econtext, false))
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
	ExecEagerFreeBitmapHeapScan(node);

	return ExecClearTuple(slot);
}


/*
 * BitmapHeapRecheck -- access method routine to recheck a tuple in EvalPlanQual
 */
static bool
BitmapHeapRecheck(BitmapHeapScanState *node, TupleTableSlot *slot)
{
	ExprContext *econtext;

	/*
	 * extract necessary information from index scan node
	 */
	econtext = node->ss.ps.ps_ExprContext;

	/* Does the tuple meet the original qual conditions? */
	econtext->ecxt_scantuple = slot;

	ResetExprContext(econtext);

	return ExecQual(node->bitmapqualorig, econtext, false);
}

/* ----------------------------------------------------------------
 *		ExecBitmapHeapScan(node)
 * ----------------------------------------------------------------
 */
TupleTableSlot *
ExecBitmapHeapScan(BitmapHeapScanState *node)
{
	TupleTableSlot *slot = NULL;
	Relation	currentRelation = node->ss.ss_currentRelation;

	if (RelationIsAppendOptimized(currentRelation))
		slot = ExecScan(&node->ss,
						(ExecScanAccessMtd) BitmapAppendOnlyNext,
						(ExecScanRecheckMtd) BitmapHeapRecheck);
	else
		slot = ExecScan(&node->ss,
						(ExecScanAccessMtd) BitmapHeapNext,
						(ExecScanRecheckMtd) BitmapHeapRecheck);

	if (TupIsNull(slot))
	{
		/* next partition, if needed */
	}

	return slot;
}

/* ----------------------------------------------------------------
 *		ExecReScanBitmapHeapScan(node)
 * ----------------------------------------------------------------
 */
void
ExecReScanBitmapHeapScan(BitmapHeapScanState *node)
{
	PlanState  *outerPlan = outerPlanState(node);

	/* rescan to release any page pin */
	if (node->bhs_currentScanDesc_heap)
		heap_rescan(node->bhs_currentScanDesc_heap, NULL);

	freeBitmapState(node);

	ExecScanReScan(&node->ss);

	/*
	 * if chgParam of subnode is not null then plan will be re-scanned by
	 * first ExecProcNode.
	 */
	if (outerPlan->chgParam == NULL)
		ExecReScan(outerPlan);
}

/* ----------------------------------------------------------------
 *		ExecEndBitmapHeapScan
 * ----------------------------------------------------------------
 */
void
ExecEndBitmapHeapScan(BitmapHeapScanState *node)
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
	 * release bitmap iterator if any
	 */
	ExecEagerFreeBitmapHeapScan(node);

	/*
	 * close down subplans
	 */
	ExecEndNode(outerPlanState(node));

	/*
	 * close heap scan
	 */
	freeFetchDesc(node);

	/*
	 * close the heap relation.
	 */
	ExecCloseScanRelation(relation);
}

/* ----------------------------------------------------------------
 *		ExecInitBitmapHeapScan
 *
 *		Initializes the scan's state information.
 * ----------------------------------------------------------------
 */
BitmapHeapScanState *
ExecInitBitmapHeapScan(BitmapHeapScan *node, EState *estate, int eflags)
{
	Relation	currentRelation;
	BitmapHeapScanState *bhsState;

	/*
	 * open the base relation and acquire appropriate lock on it.
	 */
	currentRelation = ExecOpenScanRelation(estate, node->scan.scanrelid, eflags);

	bhsState =  ExecInitBitmapHeapScanForPartition(node, estate, eflags,
												   currentRelation);

	/*
	 * initialize child nodes
	 *
	 * We do this last because the child nodes will open indexscans on our
	 * relation's indexes, and we want to be sure we have acquired a lock on
	 * the relation first.
	 */
	outerPlanState(bhsState) = ExecInitNode(outerPlan(node), estate, eflags);

	return bhsState;
}

BitmapHeapScanState *
ExecInitBitmapHeapScanForPartition(BitmapHeapScan *node, EState *estate, int eflags,
								   Relation currentRelation)
{

	BitmapHeapScanState *scanstate;
	int			io_concurrency;

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

	/*
	 * create state structure
	 */
	scanstate = makeNode(BitmapHeapScanState);
	scanstate->ss.ps.plan = (Plan *) node;
	scanstate->ss.ps.state = estate;

	scanstate->tbm = NULL;
	scanstate->tbmiterator = NULL;
	scanstate->tbmres = NULL;
	scanstate->exact_pages = 0;
	scanstate->lossy_pages = 0;
	scanstate->prefetch_iterator = NULL;
	scanstate->prefetch_pages = 0;
	scanstate->prefetch_target = 0;
	/* may be updated below */
	scanstate->prefetch_maximum = target_prefetch_pages;

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

	/* scanstate->ss.ps.ps_TupFromTlist = false; */

	/*
	 * initialize child expressions
	 */
	scanstate->ss.ps.targetlist = (List *)
		ExecInitExpr((Expr *) node->scan.plan.targetlist,
					 (PlanState *) scanstate);
	scanstate->ss.ps.qual = (List *)
		ExecInitExpr((Expr *) node->scan.plan.qual,
					 (PlanState *) scanstate);
	scanstate->bitmapqualorig = (List *)
		ExecInitExpr((Expr *) node->bitmapqualorig,
					 (PlanState *) scanstate);

	/*
	 * tuple table initialization
	 */
	ExecInitResultTupleSlot(estate, &scanstate->ss.ps);
	ExecInitScanTupleSlot(estate, &scanstate->ss);

	/*
	 * Determine the maximum for prefetch_target.  If the tablespace has a
	 * specific IO concurrency set, use that to compute the corresponding
	 * maximum value; otherwise, we already initialized to the value computed
	 * by the GUC machinery.
	 */
	io_concurrency =
		get_tablespace_io_concurrency(currentRelation->rd_rel->reltablespace);
	if (io_concurrency != effective_io_concurrency)
	{
		double		maximum;

		if (ComputeIoConcurrency(io_concurrency, &maximum))
			scanstate->prefetch_maximum = rint(maximum);
	}

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

	scanstate->bhs_currentAOFetchDesc = NULL;
	scanstate->bhs_currentAOCSFetchDesc = NULL;
	scanstate->bhs_currentAOCSLossyFetchDesc = NULL;

	/*
	 * all done.
	 */
	return scanstate;
}

static void
ExecEagerFreeBitmapHeapScan(BitmapHeapScanState *node)
{
	freeFetchDesc(node);
	freeBitmapState(node);
}

void
ExecSquelchBitmapHeapScan(BitmapHeapScanState *node)
{
	ExecEagerFreeBitmapHeapScan(node);
}
