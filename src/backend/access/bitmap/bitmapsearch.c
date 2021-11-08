/*-------------------------------------------------------------------------
 *
 * bitmapsearch.c
 *	  Search routines for on-disk bitmap index access method.
 *
 * Portions Copyright (c) 2007-2010 Greenplum Inc
 * Portions Copyright (c) 2010-2012 EMC Corporation
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Porions Copyright (c) 2006-2008, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/bitmap/bitmapsearch.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/tupdesc.h"
#include "access/bitmap.h"
#include "access/bitmap_private.h"
#include "access/relscan.h"
#include "storage/lmgr.h"
#include "parser/parse_oper.h"
#include "utils/lsyscache.h"
#include "utils/snapmgr.h"
#include "utils/faultinjector.h"


typedef struct ItemPos
{
	BlockNumber		blockNo;
	OffsetNumber	offset;
} ItemPos;

static void next_batch_words(IndexScanDesc scan);
static void read_words(Relation rel, Buffer lovBuffer,
					   OffsetNumber lovOffset,
					   bool lockLovBuffer,
					   BMBatchWords *bachWords /* out */,
					   BlockNumber *nextBlockNoP /* out */,
					   bool *readLastWords /* out */);
static void init_scanpos(IndexScanDesc scan, BMVector bmScanPos,
					 BlockNumber lovBlock, OffsetNumber lovOffset);

/*
 * _bitmap_first() -- find the first tuple that satisfies a given scan.
 */
bool
_bitmap_first(IndexScanDesc scan, ScanDirection dir)
{
	BMScanOpaque so;
	BMScanPosition scanpos;

	_bitmap_findbitmaps(scan, dir);
	so = (BMScanOpaque) scan->opaque;
	scanpos = (BMScanPosition) so->bm_currPos;
	if (scanpos->done)
		return false;

	/*
	 * Bitmap indexes don't currently support Index Only Scans.
	 * It would be pretty straightforward to return the index tuples from the
	 * LOV index, but we haven't implemented it.
	 *
	 * However, even though the 'amcanreturn' function is not implemented,
	 * the planner still chooses an Index Only Scan for some queries where
	 * no attribute from the index are needed. Be prepared for that, by
	 * filling xs_itup with a dummy IndexTuple with all NULL values.
	 */
	if (scan->xs_want_itup && !scan->xs_itup)
	{
		TupleDesc idesc = RelationGetDescr(scan->indexRelation);
		Datum	   *nulldatums;
		bool	   *isnulls;

		nulldatums = palloc(idesc->natts * sizeof(Datum));
		isnulls = palloc(idesc->natts * sizeof(bool));

		for (int i = 0; i < idesc->natts; i++)
		{
			nulldatums[i] = (Datum) 0;
			isnulls[i] = true;
		}
		scan->xs_itup = index_form_tuple(idesc, nulldatums, isnulls);
		pfree(nulldatums);
		pfree(isnulls);
	}

	return _bitmap_next(scan, dir);
}

/*
 * _bitmap_next() -- return the next tuple that satisfies a given scan.
 */
bool
_bitmap_next(IndexScanDesc scan, ScanDirection dir  pg_attribute_unused())
{
	BMScanOpaque	so;
	BMScanPosition	scanPos;
	uint64			nextTid;

	so = (BMScanOpaque) scan->opaque;
	scanPos = so->bm_currPos;

	if (scanPos->done)
		return false;

	for (;;)
	{
		/*
		 * If there are no more words left from the previous scan, we
		 * try to compute the next batch of words.
		 */
		if (scanPos->bm_batchWords->nwords == 0 &&
			scanPos->bm_result.nextTidLoc >= scanPos->bm_result.numOfTids)
		{
			_bitmap_reset_batchwords(scanPos->bm_batchWords);
			scanPos->bm_batchWords->firstTid = scanPos->bm_result.nextTid;

			elog(DEBUG2, "IndexScan next batch words start Tid: " INT64_FORMAT,
				 scanPos->bm_batchWords->firstTid);

			next_batch_words(scan);

			_bitmap_begin_iterate(scanPos->bm_batchWords, &(scanPos->bm_result));
		}

		/* If we can not find more words, then this scan is over. */
		if (scanPos->done)
			return false;

		nextTid = _bitmap_findnexttid(scanPos->bm_batchWords,
									  &(scanPos->bm_result));
		if (nextTid == 0)
			continue;
		else
			break;
	}

	Assert((nextTid % BM_MAX_TUPLES_PER_PAGE) + 1 > 0);

	ItemPointerSet(&(scan->xs_heaptid), BM_INT_GET_BLOCKNO(nextTid),
				   BM_INT_GET_OFFSET(nextTid));
	so->cur_pos_valid = true;

	return true;
}

/*
 * _bitmap_firstbatchwords() -- find the first batch of bitmap words
 *  in a bitmap vector for a given scan.
 */
bool
_bitmap_firstbatchwords(IndexScanDesc scan,
						ScanDirection dir)
{
	_bitmap_findbitmaps(scan, dir);

	return _bitmap_nextbatchwords(scan, dir);
}

/*
 * _bitmap_nextbatchwords() -- find the next batch of bitmap words
 *  in a bitmap vector for a given scan.
 */
bool
_bitmap_nextbatchwords(IndexScanDesc scan,
					   ScanDirection dir  pg_attribute_unused())
{
	BMScanOpaque	so;

	so = (BMScanOpaque) scan->opaque;

	/* check if this scan if over */
	if (so->bm_currPos->done)
		return false;

	/*
	 * Set firstTid to retrun for the remain batch words. tid < nextTid should
	 * already scanned. So move firstTid to nextTid.
	 * The firstTid may get updated when read new batch words if there only one
	 * bitmap vector matched, see read_words.
	 */
	so->bm_currPos->bm_batchWords->firstTid = so->bm_currPos->bm_result.nextTid;
	elog(DEBUG2, "BitmapIndexScan next batch words start Tid: " INT64_FORMAT,
		 so->bm_currPos->bm_batchWords->firstTid);

	/*
	 * If there are some leftover words from the previous scan, simply
	 * return them.
	 */
	if (so->bm_currPos->bm_batchWords->nwords > 0)
		return true;

	/*
	 * Compute the next list of batch words. Before that,
	 * reset the previous list of batch words, especially the
	 * content and header bitmap words.
	 */
	_bitmap_reset_batchwords(so->bm_currPos->bm_batchWords);
	next_batch_words(scan);

	return true;
}

/*
 * next_batch_words() -- compute the next batch of bitmap words
 * 	from a given scan position.
 */
static void
next_batch_words(IndexScanDesc scan)
{
	BMScanPosition			scanPos;
	BMVector	bmScanPos;
	int						i;
	BMBatchWords		  **batches;
	int						numBatches;

	scanPos = ((BMScanOpaque) scan->opaque)->bm_currPos;
	bmScanPos = scanPos->posvecs;

	/*
	 * Since we read a new batch of words, we need to reset 
	 * lastScanWordNo in result words. Otherwise, we may miss
	 * some words in this new batch.
	 */
	scanPos->bm_result.lastScanWordNo = 0;

	batches = (BMBatchWords **)
		palloc0(scanPos->nvec * sizeof(BMBatchWords *));

	numBatches = 0;
	/*
	 * Obtains the next batch of words for each bitmap vector.
	 * Ignores those bitmap vectors that contain no new words.
	 */
	for (i = 0; i < scanPos->nvec; i++)
	{
		BMBatchWords	*batchWords;
		batchWords = bmScanPos[i].bm_batchWords;

		/*
		 * If there are no words left from previous scan, read the next
		 * batch of words.
		 */
		if (bmScanPos[i].bm_batchWords->nwords == 0 &&
			!(bmScanPos[i].bm_readLastWords))
		{

			_bitmap_reset_batchwords(batchWords);
			read_words(scan->indexRelation,
					   bmScanPos[i].bm_lovBuffer,
					   bmScanPos[i].bm_lovOffset,
					   true /* lockLocBuffer */,
					   batchWords,
					   &(bmScanPos[i].bm_nextBlockNo),
					   &(bmScanPos[i].bm_readLastWords));
		}

		if (bmScanPos[i].bm_batchWords->nwords > 0)
		{
			batches[numBatches] = batchWords;
			numBatches++;
		}
	}

	/*
	 * We handle the case where only one bitmap vector contributes to
	 * the scan separately with other cases. This is because 
	 * bmScanPos->bm_batchWords and scanPos->bm_batchWords
	 * are the same.
	 */
	if (scanPos->nvec == 1)
	{
		if (bmScanPos->bm_batchWords->nwords == 0)
			scanPos->done = true;
		pfree(batches);
		scanPos->bm_batchWords = scanPos->posvecs->bm_batchWords;

		return;
	}

	/*
	 * At least two bitmap vectors contribute to this scan, we
	 * ORed these bitmap vectors.
	 */
	if (numBatches == 0)
	{
		scanPos->done = true;
		pfree(batches);
		return;
	}

	_bitmap_union(batches, numBatches, scanPos->bm_batchWords);
	pfree(batches);
}

/*
 * read_words() -- read one-block of bitmap words from
 *	the bitmap page.
 *
 * If nextBlockNo is an invalid block number, then the two last words
 * are stored in lovItem. Otherwise, read words from nextBlockNo.
 */
static void
read_words(Relation rel, Buffer lovBuffer, OffsetNumber lovOffset,
		   bool lockLovBuffer, BMBatchWords *batchWords /* out */,
		   BlockNumber *nextBlockNoP /* out */, bool *readLastWords /* out */)
{
	if (BlockNumberIsValid(*nextBlockNoP))
	{
		Buffer			bitmapBuffer;
		Page			bitmapPage;
		BMBitmap		bitmap;
		BMBitmapOpaque	bo;
		uint64			totalTidsInPage;
		bool			readLOV = false;

		if (lockLovBuffer)
			LockBuffer(lovBuffer, BM_READ);

		bitmapBuffer = _bitmap_getbuf(rel, *nextBlockNoP, BM_READ);
		bitmapPage = BufferGetPage(bitmapBuffer);

		bitmap = (BMBitmap) PageGetContentsMaxAligned(bitmapPage);
		bo = (BMBitmapOpaque)PageGetSpecialPointer(bitmapPage);

		*nextBlockNoP = bo->bm_bitmap_next;
		batchWords->nwords = bo->bm_hrl_words_used;

		/*
		 * If this is the last bitmap page and the total number of words
		 * in this page is less than or equal to
		 * BM_NUM_OF_HRL_WORDS_PER_PAGE - 2, we read the last two words from LOV
		 * and append them into 'batchWords->hwords' and 'batchWords->cwords'.
		 * This requires hold lock on the lovBuffer to avoid concurrent changes
		 * on it. Otherwise, release the lock ASAP.
		 */
		if ((!BlockNumberIsValid(*nextBlockNoP)) &&
			(batchWords->nwords <= BM_NUM_OF_HRL_WORDS_PER_PAGE - 2))
			readLOV = true;
		else
		{
			if (lockLovBuffer)
				LockBuffer(lovBuffer, BUFFER_LOCK_UNLOCK);
		}

		/*
		 * Get real next tid and nwordsread in uncompressed order for a
		 * bitmap index scan on a bitmap page.
		 * If current bitmap page get rearranged words from previous page
		 * after release the previous bitmap page and before acquire lock
		 * on it for read. The expected next tid for current bitmap scan
		 * will not equal to the current page's start tid. So set to correct
		 * value.
		 * The rearrange happens when doing insert on the table and it will
		 * update a full bitmap pages(except the last page) and generate
		 * new words.
		 * Since the page is full, so it'll rearrange the words and move
		 * the unfit words to next bitmap page.
		 * This related to issue: https://github.com/greenplum-db/gpdb/issues/11308.
		 */
		totalTidsInPage = GET_NUM_BITS(bitmap->cwords, bitmap->hwords,
									   bo->bm_hrl_words_used);
		batchWords->firstTid = bo->bm_last_tid_location - totalTidsInPage + 1;
		batchWords->nwordsread = batchWords->firstTid / BM_HRL_WORD_SIZE;

		memcpy(batchWords->hwords, bitmap->hwords,
			   BM_NUM_OF_HEADER_WORDS * sizeof(BM_HRL_WORD));
		memcpy(batchWords->cwords, bitmap->cwords,
			   sizeof(BM_HRL_WORD) * batchWords->nwords);

		_bitmap_relbuf(bitmapBuffer);
		SIMPLE_FAULT_INJECTOR("after_read_one_bitmap_idx_page");

		*readLastWords = false;

		if (readLOV)
		{
			BMBatchWords	tempBWord;
			BM_HRL_WORD		cwords[2];
			BM_HRL_WORD		hword;
			BM_HRL_WORD		tmp;
			int				offs;

			MemSet(&tempBWord, 0, sizeof(BMBatchWords));
			tempBWord.cwords = cwords;
			tempBWord.hwords = &hword;

			read_words(rel, lovBuffer, lovOffset, false /* lockLovBuffer */,
					   &tempBWord, nextBlockNoP, readLastWords);
			Assert(tempBWord.nwords > 0 && tempBWord.nwords <= 2);

			// release lock on lovBuffer once we read words from it.
			if (lockLovBuffer)
				LockBuffer(lovBuffer, BUFFER_LOCK_UNLOCK);

			memcpy(batchWords->cwords + batchWords->nwords, cwords,
				   tempBWord.nwords * sizeof(BM_HRL_WORD));

			offs = batchWords->nwords / BM_HRL_WORD_SIZE;
			tmp = hword >> batchWords->nwords % BM_HRL_WORD_SIZE;
			batchWords->hwords[offs] |= tmp;

			if (batchWords->nwords % BM_HRL_WORD_SIZE == BM_HRL_WORD_SIZE - 1)
			{
				offs = (batchWords->nwords + 1)/BM_HRL_WORD_SIZE;
				batchWords->hwords[offs] |= hword << 1;
			}
			batchWords->nwords += tempBWord.nwords;
		}
	}
	else
	{
		BMLOVItem	lovItem;
		Page		lovPage;

		if (lockLovBuffer)
			LockBuffer(lovBuffer, BM_READ);

		lovPage = BufferGetPage(lovBuffer);
		lovItem = (BMLOVItem) PageGetItem(lovPage, 
										  PageGetItemId(lovPage, lovOffset));

		if (lovItem->bm_last_compword != LITERAL_ALL_ONE)
		{
			batchWords->nwords = 2;
			batchWords->hwords[0] = (((BM_HRL_WORD)lovItem->lov_words_header) <<
							  (BM_HRL_WORD_SIZE-2));
			batchWords->cwords[0] = lovItem->bm_last_compword;
			batchWords->cwords[1] = lovItem->bm_last_word;
		}
		else
		{
			batchWords->nwords = 1;
			batchWords->hwords[0] = (((BM_HRL_WORD)lovItem->lov_words_header) <<
									(BM_HRL_WORD_SIZE-1));
			batchWords->cwords[0] = lovItem->bm_last_word;
		}

		if (lockLovBuffer)
			LockBuffer(lovBuffer, BUFFER_LOCK_UNLOCK);

		*readLastWords = true;
	}
}

/*
 * _bitmap_findbitmaps() -- find the bitmap vectors that satisfy the
 * index predicate.
 */
void
_bitmap_findbitmaps(IndexScanDesc scan, ScanDirection dir  pg_attribute_unused())
{
	BMScanOpaque			so;
	BMScanPosition			scanPos;
	Buffer					metabuf;
	BMMetaPage				metapage;
	BlockNumber				lovBlock;
	OffsetNumber			lovOffset;
	bool					blockNull, offsetNull;
	int						vectorNo, keyNo;

	so = (BMScanOpaque) scan->opaque;

	/* allocate space and initialize values for so->bm_currPos */
	if (so->bm_currPos == NULL)
		so->bm_currPos = (BMScanPosition) palloc0(sizeof(BMScanPositionData));

	scanPos = so->bm_currPos;
	scanPos->nvec = 0;
	scanPos->done = false;
	MemSet(&scanPos->bm_result, 0, sizeof(BMIterateResult));

	/*
	 * The tid to return always start from 1 which is the first tid of
	 * first uncompressed word.
	 */
	scanPos->bm_result.nextTid = 1;

	for (keyNo = 0; keyNo < scan->numberOfKeys; keyNo++)
	{
		if (scan->keyData[keyNo].sk_flags & SK_ISNULL)
		{
			/* Null key passed to bitmap index scan. Return empty result */
			Assert(scanPos->nvec == 0);
			scanPos->done = true;
			return;
		}
	}

	metabuf = _bitmap_getbuf(scan->indexRelation, BM_METAPAGE, BM_READ);
	metapage = _bitmap_get_metapage_data(scan->indexRelation, metabuf);

	{
		Relation		lovHeap, lovIndex;
		ScanKey			scanKeys;
		IndexScanDesc	scanDesc;
		List*			lovItemPoss = NIL;
		ListCell		*cell;

		/*
		 * We haven't locked the metapage but that's okay... if these
		 * values change underneath us there's something much more
		 * fundamentally wrong. This could change when we have VACUUM
		 * support, of course.
		 */
		_bitmap_open_lov_heapandindex(scan->indexRelation, metapage, 
				 &lovHeap, &lovIndex, AccessShareLock);

		scanKeys = palloc0(scan->numberOfKeys * sizeof(ScanKeyData));
		for (keyNo = 0; keyNo < scan->numberOfKeys; keyNo++)
		{
			ScanKey	scanKey = (ScanKey)(((char *)scanKeys) + 
										 keyNo * sizeof(ScanKeyData));

			ScanKeyEntryInitialize(scanKey,
								   scan->keyData[keyNo].sk_flags,
								   scan->keyData[keyNo].sk_attno,
								   scan->keyData[keyNo].sk_strategy,
								   scan->keyData[keyNo].sk_subtype,
								   scan->keyData[keyNo].sk_collation,
								   scan->keyData[keyNo].sk_func.fn_oid,
								   scan->keyData[keyNo].sk_argument);
		}

		/* When there are no scan keys, all bitmap vectors are included,
		 * including the one for the NULL value.
		 * This can happen when the index is created with predicates.
		 */
		if (scan->numberOfKeys == 0)
		{
			ItemPos *itemPos;
			itemPos = (ItemPos*)palloc0(sizeof(ItemPos));
			itemPos->blockNo = BM_LOV_STARTPAGE;;
			itemPos->offset = 1;
			lovItemPoss = lappend(lovItemPoss, itemPos);

			scanPos->nvec++;
		}

		scanDesc = index_beginscan(lovHeap, lovIndex, GetActiveSnapshot(),
								   scan->numberOfKeys, 0);
		index_rescan(scanDesc, scanKeys, scan->numberOfKeys, NULL, 0);

		/*
		 * finds all lov items for this scan through lovHeap and lovIndex.
		 */
		while (true)
		{
			ItemPos			*itemPos;

			bool res = _bitmap_findvalue(lovHeap, lovIndex, scanKeys, scanDesc,
							  	         &lovBlock, &blockNull, &lovOffset, 
										 &offsetNull);

			if(!res)
				break;

			/*
			 * We find the position for one LOV item. Append it into
			 * the list.
			 */
			itemPos = (ItemPos*)palloc0(sizeof(ItemPos));
			itemPos->blockNo = lovBlock;
			itemPos->offset = lovOffset;
			lovItemPoss = lappend(lovItemPoss, itemPos);

			scanPos->nvec++;
		}

		if (scanPos->nvec)
		{
			scanPos->posvecs =
				(BMVector)palloc0(sizeof(BMVectorData) * scanPos->nvec);
		}

		vectorNo = 0;
		foreach(cell, lovItemPoss)
		{
			ItemPos		*itemPos = (ItemPos*)lfirst(cell);

			BMVector bmScanPos = &(scanPos->posvecs[vectorNo]);
			init_scanpos(scan, bmScanPos, itemPos->blockNo, itemPos->offset);
			vectorNo++;
		}

		list_free_deep(lovItemPoss);

		index_endscan(scanDesc);
		_bitmap_close_lov_heapandindex(lovHeap, lovIndex, AccessShareLock);
		pfree(scanKeys);
	}

	_bitmap_relbuf(metabuf);

	if (scanPos->nvec == 0)
	{
		scanPos->done = true;
		return;
	}

	/*
	 * Since there is only one related bitmap vector, we have
	 * the scan position's batch words structure point directly to
	 * the vector's batch words.
	 */
	if (scanPos->nvec == 1)
		scanPos->bm_batchWords = scanPos->posvecs->bm_batchWords;
	else
	{
		scanPos->bm_batchWords = (BMBatchWords *) palloc0(sizeof(BMBatchWords));
		_bitmap_init_batchwords(scanPos->bm_batchWords,
								BM_NUM_OF_HRL_WORDS_PER_PAGE,
								CurrentMemoryContext);	
	}
}

/*
 * init_scanpos() -- initialize a BMScanPosition for a given
 *	bitmap vector.
 */
static void
init_scanpos(IndexScanDesc scan, BMVector bmScanPos, BlockNumber lovBlock,
			 OffsetNumber lovOffset)
{
	Page 					lovPage;
	BMLOVItem				lovItem;

	bmScanPos->bm_lovOffset = lovOffset;
	
	bmScanPos->bm_lovBuffer = _bitmap_getbuf(scan->indexRelation, lovBlock, 
										     BM_READ);

	lovPage	= BufferGetPage(bmScanPos->bm_lovBuffer);
	lovItem = (BMLOVItem) PageGetItem(lovPage, 
					PageGetItemId(lovPage, bmScanPos->bm_lovOffset));
			
	bmScanPos->bm_nextBlockNo = lovItem->bm_lov_head;
	bmScanPos->bm_readLastWords = false;
	bmScanPos->bm_batchWords = (BMBatchWords *) palloc0(sizeof(BMBatchWords));
	_bitmap_init_batchwords(bmScanPos->bm_batchWords,
							BM_NUM_OF_HRL_WORDS_PER_PAGE,
							CurrentMemoryContext);	

	LockBuffer(bmScanPos->bm_lovBuffer, BUFFER_LOCK_UNLOCK);
}
