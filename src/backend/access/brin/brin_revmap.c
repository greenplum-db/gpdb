/*
 * brin_revmap.c
 *		Range map for BRIN indexes
 *
 * The range map (revmap) is a translation structure for BRIN indexes: for each
 * page range there is one summary tuple, and its location is tracked by the
 * revmap.  Whenever a new tuple is inserted into a table that violates the
 * previously recorded summary values, a new tuple is inserted into the index
 * and the revmap is updated to point to it.
 *
 * The revmap is stored in the first pages of the index, immediately following
 * the metapage.  When the revmap needs to be expanded, all tuples on the
 * regular BRIN page at that block (if any) are moved out of the way.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/brin/brin_revmap.c
 */
#include "postgres.h"

#include "access/brin_page.h"
#include "access/brin_pageops.h"
#include "access/brin_revmap.h"
#include "access/brin_tuple.h"
#include "access/brin_xlog.h"
#include "access/rmgr.h"
#include "access/xloginsert.h"
#include "miscadmin.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "utils/rel.h"


struct BrinRevmap
{
	Relation    rm_irel;
	BlockNumber rm_pagesPerRange;
	BlockNumber rm_lastRevmapPage;	/* cached from the metapage */
	Buffer      rm_metaBuf;
	Buffer      rm_currBuf;
	bool 		rm_isAO;

	/* GPDB: Cached state from metapage for AO/CO tables */
	AOChainInfo 	rm_aoChainInfo[MAX_AOREL_CONCURRENCY];
	/* GPDB: Revmap iterator state for AO/CO tables */
	int         	rm_aoIterBlockSeqNum;
	BlockNumber 	rm_aoIterRevmapPage;
	LogicalPageNum 	rm_aoIterRevmapPageNum;
};

/* typedef appears in brin_revmap.h */


static BlockNumber revmap_get_blkno(BrinRevmap *revmap,
									BlockNumber heapBlk);
static Buffer revmap_get_buffer(BrinRevmap *revmap, BlockNumber heapBlk);
static BlockNumber revmap_extend_and_get_blkno_heap(BrinRevmap *revmap, BlockNumber heapBlk);
static BlockNumber revmap_extend_and_get_blkno_ao(BrinRevmap *revmap, BlockNumber heapBlk);
static BlockNumber revmap_extend_and_get_blkno(BrinRevmap *revmap,
											   BlockNumber heapBlk);
static void revmap_physical_extend(BrinRevmap *revmap, LogicalPageNum targetLogicalPageNum);
static void set_ao_revmap_chain(BrinRevmap *revmap, BrinMetaPageData *metadata, int seqnum);
/*
 * Initialize an access object for a range map.  This must be freed by
 * brinRevmapTerminate when caller is done with it.
 */
BrinRevmap *
brinRevmapInitialize(Relation idxrel, BlockNumber *pagesPerRange,
					 Snapshot snapshot)
{
	BrinRevmap *revmap;
	Buffer		meta;
	BrinMetaPageData *metadata;
	Page		page;

	meta = ReadBuffer(idxrel, BRIN_METAPAGE_BLKNO);
	LockBuffer(meta, BUFFER_LOCK_SHARE);
	page = BufferGetPage(meta);
	TestForOldSnapshot(snapshot, idxrel, page);
	metadata = (BrinMetaPageData *) PageGetContents(page);

	revmap = palloc(sizeof(BrinRevmap));
	revmap->rm_irel = idxrel;
	revmap->rm_pagesPerRange = metadata->pagesPerRange;
	revmap->rm_lastRevmapPage = metadata->lastRevmapPage;
	revmap->rm_metaBuf = meta;
	revmap->rm_currBuf = InvalidBuffer;

	/* GPDB AO/CO specific initialization (barring iterator state) */
	revmap->rm_isAO = metadata->isAO;
	memcpy(revmap->rm_aoChainInfo, metadata->aoChainInfo, sizeof(metadata->aoChainInfo));
	revmap->rm_aoIterBlockSeqNum = InvalidBlockSequenceNum;
	revmap->rm_aoIterRevmapPage = InvalidBlockNumber;
	revmap->rm_aoIterRevmapPageNum = InvalidLogicalPageNum;

	*pagesPerRange = metadata->pagesPerRange;

	LockBuffer(meta, BUFFER_LOCK_UNLOCK);

	return revmap;
}

/*
 * Release resources associated with a revmap access object.
 */
void
brinRevmapTerminate(BrinRevmap *revmap)
{
	ReleaseBuffer(revmap->rm_metaBuf);
	if (revmap->rm_currBuf != InvalidBuffer)
		ReleaseBuffer(revmap->rm_currBuf);
	pfree(revmap);
}

/*
 * Extend the revmap to cover the given heap block number.
 */
void
brinRevmapExtend(BrinRevmap *revmap, BlockNumber heapBlk)
{
	BlockNumber mapBlk PG_USED_FOR_ASSERTS_ONLY;

	mapBlk = revmap_extend_and_get_blkno(revmap, heapBlk);

	/* Ensure the buffer we got is in the expected range */
	Assert(mapBlk != InvalidBlockNumber &&
		   mapBlk != BRIN_METAPAGE_BLKNO &&
		   ((!revmap->rm_isAO && mapBlk <= revmap->rm_lastRevmapPage) ||
		   	(revmap->rm_isAO && mapBlk == revmap->rm_aoChainInfo[revmap->rm_aoIterBlockSeqNum].lastPage)));
}

/*
 * Prepare to insert an entry into the revmap; the revmap buffer in which the
 * entry is to reside is locked and returned.  Most callers should call
 * brinRevmapExtend beforehand, as this routine does not extend the revmap if
 * it's not long enough.
 *
 * The returned buffer is also recorded in the revmap struct; finishing that
 * releases the buffer, therefore the caller needn't do it explicitly.
 */
Buffer
brinLockRevmapPageForUpdate(BrinRevmap *revmap, BlockNumber heapBlk)
{
	Buffer		rmBuf;

	rmBuf = revmap_get_buffer(revmap, heapBlk);
	LockBuffer(rmBuf, BUFFER_LOCK_EXCLUSIVE);

	return rmBuf;
}

/*
 * In the given revmap buffer (locked appropriately by caller), which is used
 * in a BRIN index of pagesPerRange pages per range, set the element
 * corresponding to heap block number heapBlk to the given TID.
 *
 * Once the operation is complete, the caller must update the LSN on the
 * returned buffer.
 *
 * This is used both in regular operation and during WAL replay.
 */
void
brinSetHeapBlockItemptr(Buffer buf, BlockNumber pagesPerRange,
						BlockNumber heapBlk, ItemPointerData tid)
{
	RevmapContents *contents;
	ItemPointerData *iptr;
	Page		page;

	/* The correct page should already be pinned and locked */
	page = BufferGetPage(buf);
	contents = (RevmapContents *) PageGetContents(page);
	iptr = (ItemPointerData *) contents->rm_tids;
	iptr += HEAPBLK_TO_REVMAP_INDEX(pagesPerRange, heapBlk);

	if (ItemPointerIsValid(&tid))
		ItemPointerSet(iptr,
					   ItemPointerGetBlockNumber(&tid),
					   ItemPointerGetOffsetNumber(&tid));
	else
		ItemPointerSetInvalid(iptr);
}

/*
 * Fetch the BrinTuple for a given heap block.
 *
 * The buffer containing the tuple is locked, and returned in *buf.  The
 * returned tuple points to the shared buffer and must not be freed; if caller
 * wants to use it after releasing the buffer lock, it must create its own
 * palloc'ed copy.  As an optimization, the caller can pass a pinned buffer
 * *buf on entry, which will avoid a pin-unpin cycle when the next tuple is on
 * the same page as a previous one.
 *
 * If no tuple is found for the given heap range, returns NULL. In that case,
 * *buf might still be updated (and pin must be released by caller), but it's
 * not locked.
 *
 * The output tuple offset within the buffer is returned in *off, and its size
 * is returned in *size.
 */
BrinTuple *
brinGetTupleForHeapBlock(BrinRevmap *revmap, BlockNumber heapBlk,
						 Buffer *buf, OffsetNumber *off, Size *size, int mode,
						 Snapshot snapshot)
{
	Relation	idxRel = revmap->rm_irel;
	BlockNumber mapBlk;
	RevmapContents *contents;
	ItemPointerData *iptr;
	BlockNumber blk;
	Page		page;
	ItemId		lp;
	BrinTuple  *tup;
	ItemPointerData previptr;

	/* normalize the heap block number to be the first page in the range */
	heapBlk = brin_range_start_blk(heapBlk, revmap->rm_isAO, revmap->rm_pagesPerRange);
	/*
	 * Compute the revmap page number we need.  If Invalid is returned (i.e.,
	 * the revmap page hasn't been created yet), the requested page range is
	 * not summarized.
	 */
	mapBlk = revmap_get_blkno(revmap, heapBlk);
	if (mapBlk == InvalidBlockNumber)
	{
		*off = InvalidOffsetNumber;
		return NULL;
	}

	ItemPointerSetInvalid(&previptr);
	for (;;)
	{
		CHECK_FOR_INTERRUPTS();

		if (revmap->rm_currBuf == InvalidBuffer ||
			BufferGetBlockNumber(revmap->rm_currBuf) != mapBlk)
		{
			if (revmap->rm_currBuf != InvalidBuffer)
				ReleaseBuffer(revmap->rm_currBuf);

			Assert(mapBlk != InvalidBlockNumber);
			revmap->rm_currBuf = ReadBuffer(revmap->rm_irel, mapBlk);
			if (revmap->rm_isAO)
				revmap->rm_aoIterRevmapPageNum = BrinLogicalPageNum(BufferGetPage(revmap->rm_currBuf));
		}

		LockBuffer(revmap->rm_currBuf, BUFFER_LOCK_SHARE);

		contents = (RevmapContents *)
			PageGetContents(BufferGetPage(revmap->rm_currBuf));
		iptr = contents->rm_tids;
		iptr += HEAPBLK_TO_REVMAP_INDEX(revmap->rm_pagesPerRange, heapBlk);

		if (!ItemPointerIsValid(iptr))
		{
			LockBuffer(revmap->rm_currBuf, BUFFER_LOCK_UNLOCK);
			return NULL;
		}

		/*
		 * Check the TID we got in a previous iteration, if any, and save the
		 * current TID we got from the revmap; if we loop, we can sanity-check
		 * that the next one we get is different.  Otherwise we might be stuck
		 * looping forever if the revmap is somehow badly broken.
		 */
		if (ItemPointerIsValid(&previptr) && ItemPointerEquals(&previptr, iptr))
			ereport(ERROR,
					(errcode(ERRCODE_INDEX_CORRUPTED),
					 errmsg_internal("corrupted BRIN index: inconsistent range map")));
		previptr = *iptr;

		blk = ItemPointerGetBlockNumber(iptr);
		*off = ItemPointerGetOffsetNumber(iptr);

		LockBuffer(revmap->rm_currBuf, BUFFER_LOCK_UNLOCK);

		/* Ok, got a pointer to where the BrinTuple should be. Fetch it. */
		if (!BufferIsValid(*buf) || BufferGetBlockNumber(*buf) != blk)
		{
			if (BufferIsValid(*buf))
				ReleaseBuffer(*buf);
			*buf = ReadBuffer(idxRel, blk);
		}
		LockBuffer(*buf, mode);
		page = BufferGetPage(*buf);
		TestForOldSnapshot(snapshot, idxRel, page);

		/* If we land on a revmap page, start over */
		if (BRIN_IS_REGULAR_PAGE(page))
		{
			/*
			 * If the offset number is greater than what's in the page, it's
			 * possible that the range was desummarized concurrently. Just
			 * return NULL to handle that case.
			 */
			if (*off > PageGetMaxOffsetNumber(page))
			{
				LockBuffer(*buf, BUFFER_LOCK_UNLOCK);
				return NULL;
			}

			lp = PageGetItemId(page, *off);
			if (ItemIdIsUsed(lp))
			{
				tup = (BrinTuple *) PageGetItem(page, lp);

				if (tup->bt_blkno == heapBlk)
				{
					if (size)
						*size = ItemIdGetLength(lp);
					/* found it! */
					return tup;
				}
			}
		}

		/*
		 * No luck. Assume that the revmap was updated concurrently.
		 */
		LockBuffer(*buf, BUFFER_LOCK_UNLOCK);
	}
	/* not reached, but keep compiler quiet */
	return NULL;
}

/*
 * Delete an index tuple, marking a page range as unsummarized.
 *
 * Index must be locked in ShareUpdateExclusiveLock mode.
 *
 * Return false if caller should retry.
 */
bool
brinRevmapDesummarizeRange(Relation idxrel, BlockNumber heapBlk)
{
	BrinRevmap *revmap;
	BlockNumber pagesPerRange;
	RevmapContents *contents;
	ItemPointerData *iptr;
	ItemPointerData invalidIptr;
	BlockNumber revmapBlk;
	Buffer		revmapBuf;
	Buffer		regBuf;
	Page		revmapPg;
	Page		regPg;
	OffsetNumber revmapOffset;
	OffsetNumber regOffset;
	ItemId		lp;

	revmap = brinRevmapInitialize(idxrel, &pagesPerRange, NULL);

	/* Position the AO revmap iterator to the chain containing heapBlk */
	if (revmap->rm_isAO)
		brinRevmapAOPositionAtStart(revmap, AOSegmentGet_blockSequenceNum(heapBlk));

	revmapBlk = revmap_get_blkno(revmap, heapBlk);
	if (!BlockNumberIsValid(revmapBlk))
	{
		/* revmap page doesn't exist: range not summarized, we're done */
		brinRevmapTerminate(revmap);
		return true;
	}

	/* Lock the revmap page, obtain the index tuple pointer from it */
	revmapBuf = brinLockRevmapPageForUpdate(revmap, heapBlk);
	revmapPg = BufferGetPage(revmapBuf);
	revmapOffset = HEAPBLK_TO_REVMAP_INDEX(revmap->rm_pagesPerRange, heapBlk);

	contents = (RevmapContents *) PageGetContents(revmapPg);
	iptr = contents->rm_tids;
	iptr += revmapOffset;

	if (!ItemPointerIsValid(iptr))
	{
		/* no index tuple: range not summarized, we're done */
		LockBuffer(revmapBuf, BUFFER_LOCK_UNLOCK);
		brinRevmapTerminate(revmap);
		return true;
	}

	regBuf = ReadBuffer(idxrel, ItemPointerGetBlockNumber(iptr));
	LockBuffer(regBuf, BUFFER_LOCK_EXCLUSIVE);
	regPg = BufferGetPage(regBuf);
	/*
	 * We're only removing data, not reading it, so there's no need to
	 * TestForOldSnapshot here.
	 */

	/* if this is no longer a regular page, tell caller to start over */
	if (!BRIN_IS_REGULAR_PAGE(regPg))
	{
		LockBuffer(revmapBuf, BUFFER_LOCK_UNLOCK);
		LockBuffer(regBuf, BUFFER_LOCK_UNLOCK);
		brinRevmapTerminate(revmap);
		return false;
	}

	regOffset = ItemPointerGetOffsetNumber(iptr);
	if (regOffset > PageGetMaxOffsetNumber(regPg))
		ereport(ERROR,
				(errcode(ERRCODE_INDEX_CORRUPTED),
				 errmsg("corrupted BRIN index: inconsistent range map")));

	lp = PageGetItemId(regPg, regOffset);
	if (!ItemIdIsUsed(lp))
		ereport(ERROR,
				(errcode(ERRCODE_INDEX_CORRUPTED),
				 errmsg("corrupted BRIN index: inconsistent range map")));

	/*
	 * Placeholder tuples only appear during unfinished summarization, and we
	 * hold ShareUpdateExclusiveLock, so this function cannot run concurrently
	 * with that.  So any placeholder tuples that exist are leftovers from a
	 * crashed or aborted summarization; remove them silently.
	 */

	START_CRIT_SECTION();

	ItemPointerSetInvalid(&invalidIptr);
	brinSetHeapBlockItemptr(revmapBuf, revmap->rm_pagesPerRange, heapBlk,
							invalidIptr);
	PageIndexTupleDeleteNoCompact(regPg, regOffset);
	/* XXX record free space in FSM? */

	MarkBufferDirty(regBuf);
	MarkBufferDirty(revmapBuf);

	if (RelationNeedsWAL(idxrel))
	{
		xl_brin_desummarize xlrec;
		XLogRecPtr	recptr;

		xlrec.pagesPerRange = revmap->rm_pagesPerRange;
		xlrec.heapBlk = heapBlk;
		xlrec.regOffset = regOffset;

		XLogBeginInsert();
		XLogRegisterData((char *) &xlrec, SizeOfBrinDesummarize);
		XLogRegisterBuffer(0, revmapBuf, 0);
		XLogRegisterBuffer(1, regBuf, REGBUF_STANDARD);
		recptr = XLogInsert(RM_BRIN_ID, XLOG_BRIN_DESUMMARIZE);
		PageSetLSN(revmapPg, recptr);
		PageSetLSN(regPg, recptr);
	}

	END_CRIT_SECTION();

	UnlockReleaseBuffer(regBuf);
	LockBuffer(revmapBuf, BUFFER_LOCK_UNLOCK);
	brinRevmapTerminate(revmap);

	return true;
}

/*
 * Position the AO revmap iterator at the beginning of the revmap chain for the
 * given block sequence. This does temporarily lock the first page in the chain.
 */
void
brinRevmapAOPositionAtStart(BrinRevmap *revmap, int seqNum)
{
	Assert(seqNum != InvalidBlockSequenceNum);

	revmap->rm_aoIterBlockSeqNum = seqNum;
	revmap->rm_aoIterRevmapPage = revmap->rm_aoChainInfo[seqNum].firstPage;

	if (revmap->rm_aoChainInfo[seqNum].firstPage != InvalidBlockNumber)
	{
		/* chain exists, read the first page to get its logical page number */
		Buffer buf = ReadBuffer(revmap->rm_irel,
								revmap->rm_aoChainInfo[seqNum].firstPage);
		LockBuffer(buf, BUFFER_LOCK_SHARE);
		revmap->rm_aoIterRevmapPageNum = BrinLogicalPageNum(BufferGetPage(buf));
		UnlockReleaseBuffer(buf);
	}
	else
	{
		/* chain doesn't exist yet */
		revmap->rm_aoIterRevmapPageNum = InvalidLogicalPageNum;
	}
}

/*
 * Position the AO revmap iterator at the end of the revmap chain for the given
 * block sequence. This is a lockless operation.
 */
void
brinRevmapAOPositionAtEnd(BrinRevmap *revmap, int seqNum)
{
	Assert(seqNum != InvalidBlockSequenceNum);

	revmap->rm_aoIterBlockSeqNum = seqNum;
	revmap->rm_aoIterRevmapPage = revmap->rm_aoChainInfo[seqNum].lastPage;
	revmap->rm_aoIterRevmapPageNum = revmap->rm_aoChainInfo[seqNum].lastLogicalPageNum;
}

/*
 * Upstream version of revmap_get_blkno() for heap tables.
 */
static BlockNumber
revmap_get_blkno_heap(BrinRevmap *revmap, BlockNumber heapBlk)
{
	BlockNumber targetblk;

	/* obtain revmap block number, skip 1 for metapage block */
	targetblk = HEAPBLK_TO_REVMAP_BLK(revmap->rm_pagesPerRange, heapBlk) + 1;

	/* Normal case: the revmap page is already allocated */
	if (targetblk <= revmap->rm_lastRevmapPage)
		return targetblk;

	return InvalidBlockNumber;
}

/*
 * Similar in spirit to revmap_get_blkno_heap(), except here we traverse the
 * revmap chain maintained for the block sequence in which 'heapBlk' falls. Our
 * access struct buffer is used to read in each chain member. The iterator
 * state is always kept up-to-date with the traversal.
 */
static BlockNumber
revmap_get_blkno_ao(BrinRevmap *revmap, BlockNumber heapBlk)
{
	BlockNumber mapBlk;
	BlockNumber targetRevmapPageNum =
		HEAPBLK_TO_REVMAP_PAGENUM_AO(revmap->rm_pagesPerRange, heapBlk);

	Assert(targetRevmapPageNum >= 1);

	/* There are no revmap pages for the current block sequence */
	if (revmap->rm_aoIterRevmapPageNum == InvalidLogicalPageNum)
		return InvalidBlockNumber;

	Assert(revmap->rm_aoIterRevmapPage != InvalidBlockNumber);

	/*
	 * Traverse the revmap chain, looking for the target logical page number.
	 * Once found, the iterator will point to the required revmap page.
	 */
	mapBlk = revmap->rm_aoIterRevmapPage;
	while (revmap->rm_aoIterRevmapPageNum < targetRevmapPageNum && mapBlk != InvalidBlockNumber)
	{
		Page currPage;

		if (!BufferIsValid(revmap->rm_currBuf))
		{
			/* Read the next chain member */
			revmap->rm_currBuf = ReadBuffer(revmap->rm_irel, mapBlk);
		}
		else
		{
			/* Our access struct buffer already is what the iterator points to */
			Assert(revmap->rm_aoIterRevmapPage == BufferGetBlockNumber(revmap->rm_currBuf));
		}

		LockBuffer(revmap->rm_currBuf, BUFFER_LOCK_SHARE);

		currPage = BufferGetPage(revmap->rm_currBuf);

		/* Update the iterator position */
		revmap->rm_aoIterRevmapPage = mapBlk;
		revmap->rm_aoIterRevmapPageNum = BrinLogicalPageNum(currPage);

		/* Traverse to the next chain member */
		mapBlk = BrinNextRevmapPage(currPage);

		/* Release, so we can read in the next member */
		UnlockReleaseBuffer(revmap->rm_currBuf);
		revmap->rm_currBuf = InvalidBuffer;
	}

	if (revmap->rm_aoIterRevmapPageNum == targetRevmapPageNum)
	{
		/* Reached our destination */
		return revmap->rm_aoIterRevmapPage;
	}

	/* Destination doesn't exist yet */
	return InvalidBlockNumber;
}

/*
 * Given a heap block number, find the corresponding physical revmap block
 * number and return it.  If the revmap page hasn't been allocated yet, return
 * InvalidBlockNumber.
 */
static BlockNumber
revmap_get_blkno(BrinRevmap *revmap, BlockNumber heapBlk)
{
	if (revmap->rm_isAO)
		return revmap_get_blkno_ao(revmap, heapBlk);
	else
		return revmap_get_blkno_heap(revmap, heapBlk);
}

/*
 * Obtain and return a buffer containing the revmap page for the given heap
 * page.  The revmap must have been previously extended to cover that page.
 * The returned buffer is also recorded in the revmap struct; finishing that
 * releases the buffer, therefore the caller needn't do it explicitly.
 */
static Buffer
revmap_get_buffer(BrinRevmap *revmap, BlockNumber heapBlk)
{
	BlockNumber mapBlk;

	/* Translate the heap block number to physical index location. */
	mapBlk = revmap_get_blkno(revmap, heapBlk);

	if (mapBlk == InvalidBlockNumber)
		elog(ERROR, "revmap does not cover heap block %u", heapBlk);

	/* Ensure the buffer we got is in the expected range */
	Assert(mapBlk != BRIN_METAPAGE_BLKNO &&
		((!revmap->rm_isAO && mapBlk <= revmap->rm_lastRevmapPage) ||
		 (revmap->rm_isAO && mapBlk <= revmap->rm_aoChainInfo[revmap->rm_aoIterBlockSeqNum].lastPage)));

	/*
	 * Obtain the buffer from which we need to read.  If we already have the
	 * correct buffer in our access struct, use that; otherwise, release that,
	 * (if valid) and read the one we need.
	 */
	if (revmap->rm_currBuf == InvalidBuffer ||
		mapBlk != BufferGetBlockNumber(revmap->rm_currBuf))
	{
		if (revmap->rm_currBuf != InvalidBuffer)
			ReleaseBuffer(revmap->rm_currBuf);

		revmap->rm_currBuf = ReadBuffer(revmap->rm_irel, mapBlk);
		if (revmap->rm_isAO)
			revmap->rm_aoIterRevmapPageNum = BrinLogicalPageNum(BufferGetPage(revmap->rm_currBuf));
	}

	return revmap->rm_currBuf;
}

/*
 * Given a heap block number, find the corresponding physical revmap block
 * number and return it. If the revmap page hasn't been allocated yet, extend
 * the revmap until it is.
 */
static BlockNumber
revmap_extend_and_get_blkno(BrinRevmap *revmap, BlockNumber heapBlk)
{
	if (revmap->rm_isAO)
		return revmap_extend_and_get_blkno_ao(revmap, heapBlk);

	return revmap_extend_and_get_blkno_heap(revmap, heapBlk);
}

/*
 * GPDB: The upstream code from revmap_extend_and_get_blkno(), which applies to
 * heap tables has been moved here.
 */
static BlockNumber
revmap_extend_and_get_blkno_heap(BrinRevmap *revmap, BlockNumber heapBlk)
{
	BlockNumber targetblk;

	/* obtain revmap block number, skip 1 for metapage block */
	targetblk = HEAPBLK_TO_REVMAP_BLK(revmap->rm_pagesPerRange, heapBlk) + 1;

	/* Extend the revmap, if necessary */
	while (targetblk > revmap->rm_lastRevmapPage)
	{
		CHECK_FOR_INTERRUPTS();
		revmap_physical_extend(revmap, InvalidLogicalPageNum);
	}

	return targetblk;
}

/*
 * Similar in spirit to revmap_extend_and_get_blkno_heap(), except here we know
 * when we are done based on the positioning of the AO revmap iterator with
 * respect to the target logical page number. We can simply derive this target
 * page number based on some math.
 * The reason why we need to take this approach is that unlike for heap, revmap
 * pages don't reside in deterministic block numbers.
 */
static BlockNumber
revmap_extend_and_get_blkno_ao(BrinRevmap *revmap, BlockNumber heapBlk)
{
	int 			currSeqNum = revmap->rm_aoIterBlockSeqNum;
	LogicalPageNum 	targetLogicalPageNum;

	Assert(currSeqNum == AOSegmentGet_blockSequenceNum(heapBlk));

	/* set up the target page number state */
	targetLogicalPageNum = HEAPBLK_TO_REVMAP_PAGENUM_AO(revmap->rm_pagesPerRange,
														heapBlk);
	/*
	 * Extend the revmap, only if necessary. It is not necessary if the iterator
	 * is already positioned on the target logical page number.
	 */
	while (targetLogicalPageNum > revmap->rm_aoIterRevmapPageNum)
	{
		CHECK_FOR_INTERRUPTS();
		revmap_physical_extend(revmap, targetLogicalPageNum);
		/* Make sure the iterator is positioned at the end of the current chain */
		brinRevmapAOPositionAtEnd(revmap, currSeqNum);
	}

	return revmap->rm_aoIterRevmapPage;
}

/*
 * Try to extend the revmap by one page.  This might not happen for a number of
 * reasons; caller is expected to retry until the expected outcome is obtained.
 *
 * GPDB: For AO/CO tables, 'targetLogicalPageNum' contains the logical page
 * number of the to-be-added revmap page. (It is InvalidBlockNumber otherwise)
 */
static void
revmap_physical_extend(BrinRevmap *revmap, LogicalPageNum targetLogicalPageNum)
{
	Buffer		buf;
	Page		page;
	Page		metapage;
	BrinMetaPageData *metadata;
	BlockNumber mapBlk = InvalidBlockNumber;
	BlockNumber nblocks;
	Relation	irel = revmap->rm_irel;
	bool		needLock = !RELATION_IS_LOCAL(irel);

	/* GPDB: AO/CO specific state */
	bool		isAO = revmap->rm_isAO;
	Buffer		currLastRevmapBuf = InvalidBuffer;
	Page		currLastRevmapPage = NULL;
	bool		ao_chain_exists = false;
	int 		currSeq = revmap->rm_aoIterBlockSeqNum;

	/*
	 * Lock the metapage. This locks out concurrent extensions of the revmap,
	 * but note that we still need to grab the relation extension lock because
	 * another backend can extend the index with regular BRIN pages.
	 */
	LockBuffer(revmap->rm_metaBuf, BUFFER_LOCK_EXCLUSIVE);
	metapage = BufferGetPage(revmap->rm_metaBuf);
	metadata = (BrinMetaPageData *) PageGetContents(metapage);

	if (!isAO)
	{
	/* unindented to prevent merge conflicts */

	Assert(targetLogicalPageNum == InvalidLogicalPageNum);

	/*
	 * Check that our cached lastRevmapPage value was up-to-date; if it
	 * wasn't, update the cached copy and have caller start over.
	 */
	if (metadata->lastRevmapPage != revmap->rm_lastRevmapPage)
	{
		revmap->rm_lastRevmapPage = metadata->lastRevmapPage;
		LockBuffer(revmap->rm_metaBuf, BUFFER_LOCK_UNLOCK);
		return;
	}
	mapBlk = metadata->lastRevmapPage + 1;

	/* end if */
	}
	else
	{
		Assert(currSeq != InvalidBlockSequenceNum);
		/* assert that we have a valid target page number to assign */
		Assert(targetLogicalPageNum != InvalidLogicalPageNum);

		/*
		 * GPDB: AO/CO: Check that our cached last revmap page and logical page
		 * number values were up-to-date; if they weren't, update the cached
		 * copies and have caller start over.
		 */
		if (metadata->aoChainInfo[currSeq].lastPage != revmap->rm_aoChainInfo[currSeq].lastPage)
		{
			set_ao_revmap_chain(revmap, metadata, currSeq);
			LockBuffer(revmap->rm_metaBuf, BUFFER_LOCK_UNLOCK);
			return;
		}
	}

	nblocks = RelationGetNumberOfBlocks(irel);

	/*
	 * GPDB: For AO/CO tables, the new revmap page would always be allocated at
	 * the end of the relation.
	 */
	if (isAO)
		mapBlk = nblocks;

	if (mapBlk < nblocks)
	{
		buf = ReadBuffer(irel, mapBlk);
		LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);
		page = BufferGetPage(buf);
	}
	else
	{
		if (needLock)
			LockRelationForExtension(irel, ExclusiveLock);

		buf = ReadBuffer(irel, P_NEW);
		if (!isAO && BufferGetBlockNumber(buf) != mapBlk)
		{
			/*
			 * Very rare corner case: somebody extended the relation
			 * concurrently after we read its length.  If this happens, give
			 * up and have caller start over.  We will have to evacuate that
			 * page from under whoever is using it.
			 */
			if (needLock)
				UnlockRelationForExtension(irel, ExclusiveLock);
			LockBuffer(revmap->rm_metaBuf, BUFFER_LOCK_UNLOCK);
			ReleaseBuffer(buf);
			return;
		}
		LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);
		page = BufferGetPage(buf);

		if (needLock)
			UnlockRelationForExtension(irel, ExclusiveLock);

		if (isAO)
		{
			Assert(mapBlk == BufferGetBlockNumber(buf));

			if (metadata->aoChainInfo[currSeq].lastPage != InvalidBlockNumber)
			{
				/*
				 * We are extending the chain for the current block sequence. So,
				 * read and lock the last chain member.
				 */
				ao_chain_exists = true;

				currLastRevmapBuf = ReadBuffer(irel,
											   metadata->aoChainInfo[currSeq].lastPage);
				LockBuffer(currLastRevmapBuf, BUFFER_LOCK_EXCLUSIVE);
				currLastRevmapPage = BufferGetPage(currLastRevmapBuf);

				Assert(!PageIsNew(currLastRevmapPage));
			}
			else
			{
				/*
				 * We have no revmap pages yet for the current BlockSequence.
				 * A new chain will be started for the current block sequence
				 * below. Consequently, there is no last chain member to read.
				 */
				Assert(revmap->rm_aoChainInfo[currSeq].lastLogicalPageNum == InvalidLogicalPageNum);
			}
		}
	}

	AssertImply(isAO, PageIsNew(page));

	/* Check that it's a regular block (or an empty page) */
	if (!isAO && !PageIsNew(page) && !BRIN_IS_REGULAR_PAGE(page))
		ereport(ERROR,
				(errcode(ERRCODE_INDEX_CORRUPTED),
				 errmsg("unexpected page type 0x%04X in BRIN index \"%s\" block %u",
						BrinPageType(page),
						RelationGetRelationName(irel),
						BufferGetBlockNumber(buf))));

	/* If the page is in use, evacuate it and restart */
	/* GPDB: We don't follow the page evacuation protoocol for AO/CO tables */
	if (!isAO && brin_start_evacuating_page(irel, buf))
	{
		LockBuffer(revmap->rm_metaBuf, BUFFER_LOCK_UNLOCK);
		brin_evacuate_page(irel, revmap->rm_pagesPerRange, revmap, buf);

		/* have caller start over */
		return;
	}

	/*
	 * Ok, we have now locked the metapage and the target block. Re-initialize
	 * the target block as a revmap page, and update the metapage.
	 */
	START_CRIT_SECTION();

	/* the rm_tids array is initialized to all invalid by PageInit */
	brin_page_init(page, BRIN_PAGETYPE_REVMAP);

	/* Set the logical page number for AO/CO tables */
	if (isAO)
		BrinLogicalPageNum(page) = targetLogicalPageNum;

	MarkBufferDirty(buf);

	if (!isAO)
		metadata->lastRevmapPage = mapBlk;
	else
	{
		/* GPDB: Revmap chain bookkeeping for AO/CO tables */
		if (ao_chain_exists)
		{
			/* Extend the chain */
			BrinNextRevmapPage(currLastRevmapPage) = mapBlk;
			MarkBufferDirty(currLastRevmapBuf);
		}
		else
		{
			/* Begin a new chain */
			metadata->aoChainInfo[currSeq].firstPage = mapBlk;
		}

		metadata->aoChainInfo[currSeq].lastPage = mapBlk;
		metadata->aoChainInfo[currSeq].lastLogicalPageNum = targetLogicalPageNum;

		/* And refresh the revmap's cached state as well. */
		set_ao_revmap_chain(revmap, metadata, currSeq);
	}

	/*
	 * Set pd_lower just past the end of the metadata.  This is essential,
	 * because without doing so, metadata will be lost if xlog.c compresses
	 * the page.  (We must do this here because pre-v11 versions of PG did not
	 * set the metapage's pd_lower correctly, so a pg_upgraded index might
	 * contain the wrong value.)
	 */
	((PageHeader) metapage)->pd_lower =
		((char *) metadata + sizeof(BrinMetaPageData)) - (char *) metapage;

	MarkBufferDirty(revmap->rm_metaBuf);

	if (RelationNeedsWAL(revmap->rm_irel))
	{
		xl_brin_revmap_extend xlrec;
		XLogRecPtr	recptr;

		xlrec.targetBlk = mapBlk;
		xlrec.isAO      = isAO;

		if (isAO)
		{
			xlrec.blockSeq = currSeq;
			xlrec.targetPageNum = targetLogicalPageNum;
		}

		XLogBeginInsert();
		XLogRegisterData((char *) &xlrec, SizeOfBrinRevmapExtend);
		XLogRegisterBuffer(0, revmap->rm_metaBuf, REGBUF_STANDARD);

		XLogRegisterBuffer(1, buf, REGBUF_WILL_INIT);

		/*
		 * GPDB: Register the last chain member, so that we can link the new
		 * revmap page to it during replay. Pass empty flags as revmap pages
		 * don't follow the "standard" layout.
		 */
		if (ao_chain_exists)
			XLogRegisterBuffer(2, currLastRevmapBuf, 0);

		recptr = XLogInsert(RM_BRIN_ID, XLOG_BRIN_REVMAP_EXTEND);
		PageSetLSN(metapage, recptr);
		PageSetLSN(page, recptr);
		if (ao_chain_exists)
			PageSetLSN(currLastRevmapPage, recptr);
	}

	END_CRIT_SECTION();

	LockBuffer(revmap->rm_metaBuf, BUFFER_LOCK_UNLOCK);

	UnlockReleaseBuffer(buf);
	if (ao_chain_exists)
		UnlockReleaseBuffer(currLastRevmapBuf);
}

/*
 * Set the cache of chain metadata maintained in the revmap access struct,
 * for the chain with the given 'seqnum', using the metapage contents.
 */
static void
set_ao_revmap_chain(BrinRevmap *revmap, BrinMetaPageData *metadata, int seqnum)
{
	revmap->rm_aoChainInfo[seqnum].firstPage = metadata->aoChainInfo[seqnum].firstPage;
	revmap->rm_aoChainInfo[seqnum].lastPage = metadata->aoChainInfo[seqnum].lastPage;
	revmap->rm_aoChainInfo[seqnum].lastLogicalPageNum = metadata->aoChainInfo[seqnum].lastLogicalPageNum;
}
