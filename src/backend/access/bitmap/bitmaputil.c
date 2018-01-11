/*-------------------------------------------------------------------------
 *
 * bitmaputil.c
 *	  Utility routines for on-disk bitmap index access method.
 *
 * Portions Copyright (c) 2007-2010 Greenplum Inc
 * Portions Copyright (c) 2010-2012 EMC Corporation
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 2006-2008, PostgreSQL Global Development Group
 * 
 *
 * IDENTIFICATION
 *	  src/backend/access/bitmap/bitmaputil.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"

#include "access/genam.h"
#include "access/bitmap.h"
#include "access/heapam.h"
#include "access/reloptions.h"

static void _bitmap_findnextword(BMBatchWords* words, uint64 nextReadNo);
static void _bitmap_resetWord(BMBatchWords *words, uint32 prevStartNo);
static uint8 _bitmap_find_bitset(BM_HRL_WORD word, uint8 lastPos);

/*
 * _bitmap_formitem() -- construct a LOV entry.
 *
 * If the given tid number is greater than BM_HRL_WORD_SIZE, we
 * construct the first fill word for this bitmap vector.
 */
BMLOVItem
_bitmap_formitem(uint64 currTidNumber)
{
	BMLOVItem	bmitem;

	bmitem = (BMLOVItem) palloc(sizeof(BMLOVItemData));

	bmitem->bm_lov_head = bmitem->bm_lov_tail = InvalidBlockNumber;
	bmitem->bm_last_setbit = 0;
	bmitem->bm_last_compword = LITERAL_ALL_ONE;
	bmitem->bm_last_word = LITERAL_ALL_ZERO;
	bmitem->lov_words_header = 0;
	bmitem->bm_last_tid_location = 0;

	/* fill up all existing bits with 0. */
	if (currTidNumber <= BM_HRL_WORD_SIZE)
	{
		bmitem->bm_last_compword = LITERAL_ALL_ONE;
		bmitem->bm_last_word = LITERAL_ALL_ZERO;
		bmitem->lov_words_header = 0;
		bmitem->bm_last_tid_location = 0;
	}
	else
	{
		uint64		numOfTotalFillWords;
		BM_HRL_WORD	numOfFillWords;

		numOfTotalFillWords = (currTidNumber-1)/BM_HRL_WORD_SIZE;

		numOfFillWords = (numOfTotalFillWords >= MAX_FILL_LENGTH) ? 
			MAX_FILL_LENGTH : numOfTotalFillWords;

		bmitem->bm_last_compword = BM_MAKE_FILL_WORD(0, numOfFillWords);
		bmitem->bm_last_word = LITERAL_ALL_ZERO;
		bmitem->lov_words_header = 2;
		bmitem->bm_last_tid_location = numOfFillWords * BM_HRL_WORD_SIZE;

		bmitem->bm_last_setbit = numOfFillWords*BM_HRL_WORD_SIZE;
	}

	return bmitem;
}

/*
 * _bitmap_get_metapage_data() -- return the metadata info stored
 * in the given metapage buffer.
 */
BMMetaPage
_bitmap_get_metapage_data(Relation rel, Buffer metabuf)
{
	Page page;
	BMMetaPage metapage;
	
	page = BufferGetPage(metabuf);
	metapage = (BMMetaPage)PageGetContents(page);

	/*
	 * If this metapage is from the pre 3.4 version of the bitmap
	 * index, we print "require to reindex" message, and error
	 * out.
	 */
	if (metapage->bm_version != BITMAP_VERSION)
	{
		ereport(ERROR,
				(0,
				 errmsg("The disk format for %s is not valid for this version of "
						"Greenplum Database. Use REINDEX %s to update this index",
						RelationGetRelationName(rel), RelationGetRelationName(rel))));
	}

	return metapage;
}


/*
 * _bitmap_init_batchwords() -- initialize a BMBatchWords in a given
 * memory context.
 *
 * Allocate spaces for bitmap header words and bitmap content words.
 */
void
_bitmap_init_batchwords(BMBatchWords* words,
						uint32 maxNumOfWords,
						MemoryContext mcxt)
{
	uint32	numOfHeaderWords;
	MemoryContext oldcxt;

	words->nwordsread = 0;
	words->nextread = 1;
	words->startNo = 0;
	words->nwords = 0;

	numOfHeaderWords = BM_CALC_H_WORDS(maxNumOfWords);

	words->maxNumOfWords = maxNumOfWords;

	/* Make sure that we have at least one page of words */
	Assert(words->maxNumOfWords >= BM_NUM_OF_HRL_WORDS_PER_PAGE);

	oldcxt = MemoryContextSwitchTo(mcxt);
	words->hwords = palloc0(sizeof(BM_HRL_WORD)*numOfHeaderWords);
	words->cwords = palloc0(sizeof(BM_HRL_WORD)*words->maxNumOfWords);
	MemoryContextSwitchTo(oldcxt);
}

/*
 * _bitmap_copy_batchwords() -- copy a given BMBatchWords to another
 *	BMBatchWords.
 */
void
_bitmap_copy_batchwords(BMBatchWords* words, BMBatchWords* copyWords)
{
	uint32	numOfHeaderWords;

	copyWords->maxNumOfWords = words->maxNumOfWords;
	copyWords->nwordsread = words->nwordsread;
	copyWords->nextread = words->nextread;
	copyWords->firstTid = words->firstTid;
	copyWords->startNo = words->startNo;
	copyWords->nwords = words->nwords;

	numOfHeaderWords = BM_CALC_H_WORDS(copyWords->maxNumOfWords);

	memcpy(copyWords->hwords, words->hwords,
			sizeof(BM_HRL_WORD)*numOfHeaderWords);
	memcpy(copyWords->cwords, words->cwords,
			sizeof(BM_HRL_WORD)*copyWords->maxNumOfWords);
}

/*
 * _bitmap_reset_batchwords() -- reset the BMBatchWords for re-use.
 */
void
_bitmap_reset_batchwords(BMBatchWords *words)
{
	words->startNo = 0;
	words->nwords = 0;
	MemSet(words->hwords, 0,
		   sizeof(BM_HRL_WORD) * BM_CALC_H_WORDS(words->maxNumOfWords));
}

/*
 * _bitmap_cleanup_batchwords() -- release spaces allocated for the BMBatchWords.
 */
void _bitmap_cleanup_batchwords(BMBatchWords* words)
{
	if (words == NULL)
		return;

	if (words->hwords)
		pfree(words->hwords);
	if (words->cwords)
		pfree(words->cwords);
}

/*
 * _bitmap_cleanup_scanpos() -- release space allocated for
 * 	BMVector.
 */
void
_bitmap_cleanup_scanpos(BMVector bmScanPos, uint32 numBitmapVectors)
{
	uint32 keyNo;

	if (numBitmapVectors == 0)
	{
		return;
	}
		
	for (keyNo=0; keyNo<numBitmapVectors; keyNo++)
	{
		if (BufferIsValid((bmScanPos[keyNo]).bm_lovBuffer))
			ReleaseBuffer((bmScanPos[keyNo]).bm_lovBuffer);

		_bitmap_cleanup_batchwords((bmScanPos[keyNo]).bm_batchWords);
		if (bmScanPos[keyNo].bm_batchWords != NULL)
			pfree((bmScanPos[keyNo]).bm_batchWords);
	}

	pfree(bmScanPos);
}

/*
 * _bitmap_findnexttid() -- find the next tid location in a given batch
 *  of bitmap words.
 */
uint64
_bitmap_findnexttid(BMBatchWords *words, BMIterateResult *result)
{
	/*
	 * If there is not tids from previous computation, then we
	 * try to find next set of tids.
	 */

	if (result->nextTidLoc >= result->numOfTids)
		_bitmap_findnexttids(words, result, BM_BATCH_TIDS);

	/* if find more tids, then return the first one */
	if (result->nextTidLoc < result->numOfTids)
	{
		result->nextTidLoc++;
		return (result->nextTids[result->nextTidLoc-1]);
	}

	/* no more tids */
	return 0;
}

/*
 * _bitmap_findnexttids() -- find the next set of tids from a given
 *  batch of bitmap words.
 *
 * The maximum number of tids to be found is defined in 'maxTids'.
 */
void
_bitmap_findnexttids(BMBatchWords *words, BMIterateResult *result,
					 uint32 maxTids)
{
	bool done = false;

	result->nextTidLoc = result->numOfTids = 0;
	while (words->nwords > 0 && result->numOfTids < maxTids && !done)
	{
		uint8 oldScanPos = result->lastScanPos;
		BM_HRL_WORD word = words->cwords[result->lastScanWordNo];

		/* new word, zero filled */
		if (oldScanPos == 0 &&
			((IS_FILL_WORD(words->hwords, result->lastScanWordNo) && 
			  GET_FILL_BIT(word) == 0) || word == 0))
		{
			BM_HRL_WORD	fillLength;
			if (word == 0)
				fillLength = 1;
			else
				fillLength = FILL_LENGTH(word);

			/* skip over non-matches */
			result->nextTid += fillLength * BM_HRL_WORD_SIZE;
			result->lastScanWordNo++;
			words->nwords--;
			result->lastScanPos = 0;
			continue;
		}
		else if (IS_FILL_WORD(words->hwords, result->lastScanWordNo)
				 && GET_FILL_BIT(word) == 1)
		{
			uint64	nfillwords = FILL_LENGTH(word);
			uint8 	bitNo;

			while (result->numOfTids + BM_HRL_WORD_SIZE <= maxTids &&
				   nfillwords > 0)
			{
				/* explain the fill word */
				for (bitNo = 0; bitNo < BM_HRL_WORD_SIZE; bitNo++)
					result->nextTids[result->numOfTids++] = ++result->nextTid;

				nfillwords--;
				/* update fill word to reflect expansion */
				words->cwords[result->lastScanWordNo]--;
			}

			if (nfillwords == 0)
			{
				result->lastScanWordNo++;
				words->nwords--;
				result->lastScanPos = 0;
				continue;
			}
			else
			{
				done = true;
				break;
			}
		}
		else
		{
			if(oldScanPos == 0)
				oldScanPos = BM_HRL_WORD_SIZE + 1;

			while (oldScanPos != 0 && result->numOfTids < maxTids)
			{
				BM_HRL_WORD		w;

				if (oldScanPos == BM_HRL_WORD_SIZE + 1)
					oldScanPos = 0;

				w = words->cwords[result->lastScanWordNo];
				result->lastScanPos = _bitmap_find_bitset(w, oldScanPos);

				/* did we fine a bit set in this word? */
				if (result->lastScanPos != 0)
				{
					result->nextTid += (result->lastScanPos - oldScanPos);
					result->nextTids[result->numOfTids++] = result->nextTid;
				}
				else
				{
					result->nextTid += BM_HRL_WORD_SIZE - oldScanPos;
					/* start scanning a new word */
					words->nwords--;
					result->lastScanWordNo++;
					result->lastScanPos = 0;
				}
				oldScanPos = result->lastScanPos;
			}
		}
	}
}

/*
 * _bitmap_intesect() is dead code because streaming intersects
 * PagetableEntry structures, not raw batch words. It's possible we may
 * want to intersect batches later though -- it would definately improve
 * streaming of intersections.
 */

#ifdef NOT_USED

/*
 * _bitmap_intersect() -- intersect 'numBatches' bitmap words.
 *
 * All 'numBatches' bitmap words are HRL compressed. The result
 * bitmap words HRL compressed, except that fill set words(1s) may
 * be lossily compressed.
 */
void
_bitmap_intersect(BMBatchWords **batches, uint32 numBatches,
				  BMBatchWords *result)
{
	bool done = false;
	uint32	*prevStartNos;
	uint64	nextReadNo;
	uint64	batchNo;

	Assert(numBatches > 0);

	prevStartNos = (uint32 *)palloc0(numBatches * sizeof(uint32));
	nextReadNo = batches[0]->nextread;

	while (!done &&	result->nwords < result->maxNumOfWords)
	{
		BM_HRL_WORD andWord = LITERAL_ALL_ONE;
		BM_HRL_WORD	word;

		bool		andWordIsLiteral = true;

		/*
    	 * We walk through the bitmap word in each list one by one
         * without de-compress the bitmap words. 'nextReadNo' defines
         * the position of the next word that should be read in an
         * uncompressed format.
         */
		for (batchNo = 0; batchNo < numBatches; batchNo++)
		{
			uint32 offs;
			BMBatchWords *bch = batches[batchNo];

			/* skip nextReadNo - nwordsread - 1 words */
			_bitmap_findnextword(bch, nextReadNo);

			if (bch->nwords == 0)
			{
				done = true;
				break;
			}

			Assert(bch->nwordsread == nextReadNo - 1);

			/* Here, startNo should point to the word to be read. */
			offs = bch->startNo;
			word = bch->cwords[offs];

			if (CUR_WORD_IS_FILL(bch) && (GET_FILL_BIT(word) == 0))
			{
				uint32		n;
				
				bch->nwordsread += FILL_LENGTH(word);

				n = bch->nwordsread - nextReadNo + 1;
				andWord = BM_MAKE_FILL_WORD(0, n);
				andWordIsLiteral = false;

				nextReadNo = bch->nwordsread + 1;
				bch->startNo++;
				bch->nwords--;
				break;
			}
			else if (CUR_WORD_IS_FILL(bch) && (GET_FILL_BIT(word) == 1))
			{
				bch->nwordsread++;

				prevStartNos[batchNo] = bch->startNo;

				if (FILL_LENGTH(word) == 1)
				{
					bch->startNo++;
					bch->nwords--;
				}
				else
				{
					uint32 s = bch->startNo;
					bch->cwords[s]--;
				}
				andWordIsLiteral = true;
			}
			else if (!CUR_WORD_IS_FILL(bch))
			{
				prevStartNos[batchNo] = bch->startNo;

				andWord &= word;
				bch->nwordsread++;
				bch->startNo++;
				bch->nwords--;
				andWordIsLiteral = true;
			}
		}

		/* Since there are not enough words in this attribute break this loop */
		if (done)
		{
			uint32 preBatchNo;

			/* reset the attributes before batchNo */
			for (preBatchNo = 0; preBatchNo < batchNo; preBatchNo++)
			{
				_bitmap_resetWord(batches[preBatchNo], prevStartNos[preBatchNo]);
			}
			break;
		}
		else
		{
			if (!andWordIsLiteral)
			{
				uint32	off = result->nwords/BM_HRL_WORD_SIZE;
				uint32	w = result->nwords;

				result->hwords[off] |= WORDNO_GET_HEADER_BIT(w);
			}
			result->cwords[result->nwords] = andWord;
			result->nwords++;
		}

		if (andWordIsLiteral)
			nextReadNo++;

		if (batchNo == 1 && bch->nwords == 0)
			done = true;
	}

	/* set the nextReadNo */
	for (batchNo = 0; batchNo < numBatches; batchNo++)
		batches[batchNo]->nextread = nextReadNo;

	pfree(prevStartNos);
}

#endif /* NOT_USED */

/*
 * Fast forward to the next read position by skipping the common compressed
 * zeros that appear in all batches. These skipped zeros are also copied into
 * the result words.
 */
static uint64
fast_forward(uint32 nbatches, BMBatchWords **batches, BMBatchWords *result)
{
	int i;
	uint64 min_tid = ~0;
	uint64 fast_forward_words = 0;

	Assert(result != NULL);
	Assert(nbatches == 0 || batches != NULL);

	for (i = 0; i < nbatches; i++)
	{
		BM_HRL_WORD word = batches[i]->cwords[batches[i]->startNo];

		/* if we find a matching tid in one of the batches, nothing to do */
		if (CUR_WORD_IS_FILL(batches[i]) && GET_FILL_BIT(word) == 1)
			return batches[0]->nextread;
		else if (!CUR_WORD_IS_FILL(batches[i]))
			return batches[0]->nextread;
		else if (CUR_WORD_IS_FILL(batches[i]) && GET_FILL_BIT(word) == 0)
		{
			uint64 batch_tid = batches[i]->firstTid +
				(FILL_LENGTH(word) * BM_HRL_WORD_SIZE);

			/* adjust down */
			if (batch_tid < min_tid)
			{
				min_tid = batch_tid;
				fast_forward_words = FILL_LENGTH(word);
			}
		}
	}
	if (fast_forward_words)
	{
		uint32 offset;
		
		for (i = 0; i < nbatches; i++)
			batches[i]->nextread = batches[i]->nwordsread + 
				fast_forward_words + 1;

		/*
		 * Copy these fast-forwarded words to
		 * the result.
		 */
		offset = result->nwords/BM_HRL_WORD_SIZE;
		result->hwords[offset] |= WORDNO_GET_HEADER_BIT(result->nwords);
		result->cwords[result->nwords] = fast_forward_words;
		result->nwords++;
	}

	return batches[0]->nextread;
}

/*
 * _bitmap_union() -- union 'numBatches' bitmaps
 *
 * All bitmap words are HRL compressed. The result bitmap words are also
 * HRL compressed, except that fill unset words may be lossily compressed.
 */
void
_bitmap_union(BMBatchWords **batches, uint32 numBatches, BMBatchWords *result)
{
	bool 		done = false;
	uint32 	   *prevstarts;
	uint64		nextReadNo;
	uint64		batchNo;

	Assert ((int)numBatches >= 0);

	if (numBatches == 0)
		return;

	/* save batch->startNo for each input bitmap vector */
	prevstarts = (uint32 *)palloc0(numBatches * sizeof(uint32));

	/* 
	 * Compute the next read offset. We fast forward compressed
	 * zero words when possible.
	 */
	nextReadNo = fast_forward(numBatches, batches, result);

	while (!done &&	result->nwords < result->maxNumOfWords)
	{
		BM_HRL_WORD orWord = LITERAL_ALL_ZERO;
		BM_HRL_WORD	word;
		bool		orWordIsLiteral = true;

		for (batchNo = 0; batchNo < numBatches; batchNo++)
		{
			BMBatchWords *bch = batches[batchNo];

			/* skip nextReadNo - nwordsread - 1 words */
			_bitmap_findnextword(bch, nextReadNo);

			if (bch->nwords == 0)
			{
				done = true;
				break;
			}

			Assert(bch->nwordsread == nextReadNo - 1);

			/* Here, startNo should point to the word to be read. */
			word = bch->cwords[bch->startNo];

			if (CUR_WORD_IS_FILL(bch) && GET_FILL_BIT(word) == 1)
			{
				/* Fill word represents matches */
				bch->nwordsread += FILL_LENGTH(word);
				orWord = BM_MAKE_FILL_WORD(1, bch->nwordsread - nextReadNo + 1);
				orWordIsLiteral = false;

				nextReadNo = bch->nwordsread + 1;
				bch->startNo++;
				bch->nwords--;
				break;
			}
			else if (CUR_WORD_IS_FILL(bch) && GET_FILL_BIT(word) == 0)
			{
				/* Fill word represents no matches */

				bch->nwordsread++;
				prevstarts[batchNo] = bch->startNo;
				if (FILL_LENGTH(word) == 1)
				{
					bch->startNo++;
					bch->nwords--;
				}
				else
					bch->cwords[bch->startNo]--;
				orWordIsLiteral = true;
			}
			else if (!CUR_WORD_IS_FILL(bch))
			{
				/* word is literal */
				prevstarts[batchNo] = bch->startNo;
				orWord |= word;
				bch->nwordsread++;
				bch->startNo++;
				bch->nwords--;
				orWordIsLiteral = true;
			}
		}

		if (done)
		{
			uint32 i;

			/* reset the attributes before batchNo */
			for (i = 0; i < batchNo; i++)
				_bitmap_resetWord(batches[i], prevstarts[i]);
			break;
		}
		else
		{
			if (!orWordIsLiteral)
			{
				 /* Word is not literal, update the result header */
				uint32 	offs = result->nwords/BM_HRL_WORD_SIZE;
				uint32	n = result->nwords;
				result->hwords[offs] |= WORDNO_GET_HEADER_BIT(n);
			}
			result->cwords[result->nwords] = orWord;
			result->nwords++;
		}

		if (orWordIsLiteral)
			nextReadNo++;

		/* we just processed the last batch and it was empty */
		if (batchNo == numBatches - 1 && batches[batchNo]->nwords == 0)
			done = true;
	}

	/* set the next word to read for all input vectors */
	for (batchNo = 0; batchNo < numBatches; batchNo++)
		batches[batchNo]->nextread = nextReadNo;

	pfree(prevstarts);
}

/*
 * _bitmap_findnextword() -- Find the next word whose position is
 *        	                'nextReadNo' in an uncompressed format.
 */
static void
_bitmap_findnextword(BMBatchWords *words, uint64 nextReadNo)
{
	/* 
     * 'words->nwordsread' defines how many un-compressed words
     * have been read in this bitmap. We read from
     * position 'startNo', and increment 'words->nwordsread'
     * differently based on the type of words that are read, until
     * 'words->nwordsread' is equal to 'nextReadNo'.
     */
	while (words->nwords > 0 && words->nwordsread < nextReadNo - 1)
	{
		/* Get the current word */
		BM_HRL_WORD word = words->cwords[words->startNo];

		if (CUR_WORD_IS_FILL(words))
		{
			if(FILL_LENGTH(word) <= (nextReadNo - words->nwordsread - 1))
			{
				words->nwordsread += FILL_LENGTH(word);
				words->startNo++;
				words->nwords--;
			}
			else
			{
				words->cwords[words->startNo] -= (nextReadNo - words->nwordsread - 1);
				words->nwordsread = nextReadNo - 1;
			}
		}
		else
		{
			words->nwordsread++;
			words->startNo++;
			words->nwords--;
		}
	}
}

/*
 * _bitmap_resetWord() -- Reset the read position in an BMBatchWords
 *       	              to its previous value.
 *
 * Reset the read position in an BMBatchWords to its previous value,
 * which is given in 'prevStartNo'. Based on different type of words read,
 * the actual bitmap word may need to be changed.
 */
static void
_bitmap_resetWord(BMBatchWords *words, uint32 prevStartNo)
{
	if (words->startNo > prevStartNo)
	{
		Assert(words->startNo == prevStartNo + 1);
		words->startNo = prevStartNo;
		words->nwords++;
	}
	else
	{
		Assert(words->startNo == prevStartNo);
		Assert(CUR_WORD_IS_FILL(words));
		words->cwords[words->startNo]++;
	}
	words->nwordsread--;
}


/*
 * _bitmap_find_bitset() -- find the rightmost set bit (bit=1) in the 
 * 		given word since 'lastPos', not including 'lastPos'.
 *
 * The rightmost bit in the given word is considered the position 1, and
 * the leftmost bit is considered the position BM_HRL_WORD_SIZE.
 *
 * If such set bit does not exist in this word, 0 is returned.
 */
static uint8
_bitmap_find_bitset(BM_HRL_WORD word, uint8 lastPos)
{
	uint8 pos = lastPos + 1;
	BM_HRL_WORD	rightmostBitWord;

	if (pos > BM_HRL_WORD_SIZE)
	  return 0;

	rightmostBitWord = (((BM_HRL_WORD)1) << (pos-1));

	while (pos <= BM_HRL_WORD_SIZE && (word & rightmostBitWord) == 0)
	{
		rightmostBitWord <<= 1;
		pos++;
	}

	if (pos > BM_HRL_WORD_SIZE)
		pos = 0;

	return pos;
}

/*
 * _bitmap_begin_iterate() -- initialize the given BMIterateResult instance.
 */
void
_bitmap_begin_iterate(BMBatchWords *words, BMIterateResult *result)
{
	result->nextTid = words->firstTid;
	result->lastScanPos = 0;
	result->lastScanWordNo = words->startNo;
	result->numOfTids = 0;
	result->nextTidLoc = 0;
}


/*
 * _bitmap_log_newpage() -- log a new page.
 *
 * This function is called before writing a new buffer.
 */
void
_bitmap_log_newpage(Relation rel, uint8 info, Buffer buf)
{
	Page page;

	xl_bm_newpage		xlNewPage;
	XLogRecPtr			recptr;
	XLogRecData			rdata[1];

	page = BufferGetPage(buf);

	// Fetch gp_persistent_relation_node information that will be added to XLOG record.
	RelationFetchGpRelationNodeForXLog(rel);

	xlNewPage.bm_node = rel->rd_node;
	RelationGetPTInfo(rel, &xlNewPage.bm_persistentTid, &xlNewPage.bm_persistentSerialNum);
	xlNewPage.bm_new_blkno = BufferGetBlockNumber(buf);

	elog(DEBUG1, "_bitmap_log_newpage: blkno=%d", xlNewPage.bm_new_blkno);

	rdata[0].buffer = InvalidBuffer;
	rdata[0].data = (char *)&xlNewPage;
	rdata[0].len = sizeof(xl_bm_newpage);
	rdata[0].next = NULL;
			
	recptr = XLogInsert(RM_BITMAP_ID, info, rdata);

	PageSetLSN(page, recptr);
}

/*
 * _bitmap_log_metapage() -- log the changes to the metapage
 */
void
_bitmap_log_metapage(Relation rel, Page page)
{
	BMMetaPage metapage = (BMMetaPage) PageGetContents(page);

	xl_bm_metapage*		xlMeta;
	XLogRecPtr			recptr;
	XLogRecData			rdata[1];

	// Fetch gp_persistent_relation_node information that will be added to XLOG record.
	RelationFetchGpRelationNodeForXLog(rel);

	xlMeta = (xl_bm_metapage *)
		palloc(MAXALIGN(sizeof(xl_bm_metapage)));
	xlMeta->bm_node = rel->rd_node;
	RelationGetPTInfo(rel, &xlMeta->bm_persistentTid, &xlMeta->bm_persistentSerialNum);
	xlMeta->bm_lov_heapId = metapage->bm_lov_heapId;
	xlMeta->bm_lov_indexId = metapage->bm_lov_indexId;
	xlMeta->bm_lov_lastpage = metapage->bm_lov_lastpage;

	rdata[0].buffer = InvalidBuffer;
	rdata[0].data = (char*)xlMeta;
	rdata[0].len = MAXALIGN(sizeof(xl_bm_metapage));
	rdata[0].next = NULL;
			
	recptr = XLogInsert(RM_BITMAP_ID, XLOG_BITMAP_INSERT_META, rdata);

	PageSetLSN(page, recptr);
	pfree(xlMeta);
}

/*
 * _bitmap_log_bitmap_lastwords() -- log the last two words in a bitmap.
 */
void
_bitmap_log_bitmap_lastwords(Relation rel, Buffer lovBuffer, 
							 OffsetNumber lovOffset, BMLOVItem lovItem)
{
	xl_bm_bitmap_lastwords	xlLastwords;
	XLogRecPtr				recptr;
	XLogRecData				rdata[1];

	// Fetch gp_persistent_relation_node information that will be added to XLOG record.
	RelationFetchGpRelationNodeForXLog(rel);

	xlLastwords.bm_node = rel->rd_node;
	RelationGetPTInfo(rel, &xlLastwords.bm_persistentTid, &xlLastwords.bm_persistentSerialNum);
	xlLastwords.bm_last_compword = lovItem->bm_last_compword;
	xlLastwords.bm_last_word = lovItem->bm_last_word;
	xlLastwords.lov_words_header = lovItem->lov_words_header;
	xlLastwords.bm_last_setbit = lovItem->bm_last_setbit;
	xlLastwords.bm_last_tid_location = lovItem->bm_last_tid_location;
	xlLastwords.bm_lov_blkno = BufferGetBlockNumber(lovBuffer);
	xlLastwords.bm_lov_offset = lovOffset;

	rdata[0].buffer = InvalidBuffer;
	rdata[0].data = (char*)&xlLastwords;
	rdata[0].len = sizeof(xl_bm_bitmap_lastwords);
	rdata[0].next = NULL;

	recptr = XLogInsert(RM_BITMAP_ID, XLOG_BITMAP_INSERT_BITMAP_LASTWORDS, 
						rdata);

	PageSetLSN(BufferGetPage(lovBuffer), recptr);
}

/*
 * _bitmap_log_lovitem() -- log adding a new lov item to a lov page.
 */
void
_bitmap_log_lovitem(Relation rel, Buffer lovBuffer, OffsetNumber offset,
					BMLOVItem lovItem, Buffer metabuf, bool is_new_lov_blkno)
{
	Page lovPage = BufferGetPage(lovBuffer);

	xl_bm_lovitem	xlLovItem;
	XLogRecPtr		recptr;
	XLogRecData		rdata[1];

	// Fetch gp_persistent_relation_node information that will be added to XLOG record.
	RelationFetchGpRelationNodeForXLog(rel);

	Assert(BufferGetBlockNumber(lovBuffer) > 0);

	xlLovItem.bm_node = rel->rd_node;
	RelationGetPTInfo(rel, &xlLovItem.bm_persistentTid, &xlLovItem.bm_persistentSerialNum);
	xlLovItem.bm_lov_blkno = BufferGetBlockNumber(lovBuffer);
	xlLovItem.bm_lov_offset = offset;
	memcpy(&(xlLovItem.bm_lovItem), lovItem, sizeof(BMLOVItemData));
	xlLovItem.bm_is_new_lov_blkno = is_new_lov_blkno;

	rdata[0].buffer = InvalidBuffer;
	rdata[0].data = (char*)&xlLovItem;
	rdata[0].len = sizeof(xl_bm_lovitem);
	rdata[0].next = NULL;

	recptr = XLogInsert(RM_BITMAP_ID, 
						XLOG_BITMAP_INSERT_LOVITEM, rdata);

	if (is_new_lov_blkno)
	{
		Page metapage = BufferGetPage(metabuf);

		PageSetLSN(metapage, recptr);
	}

	PageSetLSN(lovPage, recptr);

	elog(DEBUG1, "Insert a new lovItem at (blockno, offset): (%d,%d)",
		 BufferGetBlockNumber(lovBuffer), offset);
}

/*
 * _bitmap_log_bitmapwords() -- log new bitmap words to be inserted.
 */
void
_bitmap_log_bitmapwords(Relation rel, Buffer bitmapBuffer, Buffer lovBuffer,
						OffsetNumber lovOffset, BMTIDBuffer* buf,
						uint64 words_written, uint64 tidnum, BlockNumber nextBlkno,
						bool isLast, bool isFirst)
{
	Page				bitmapPage;
	BMBitmapOpaque		bitmapPageOpaque;
	xl_bm_bitmapwords  *xlBitmapWords;
	XLogRecPtr			recptr;
	XLogRecData			rdata[1];
	uint64*				lastTids;
	BM_HRL_WORD*		cwords;
	BM_HRL_WORD*		hwords;
	int					lastTids_size;
	int					cwords_size;
	int					hwords_size;
	Page lovPage = BufferGetPage(lovBuffer);

	// Fetch gp_persistent_relation_node information that will be added to XLOG record.
	RelationFetchGpRelationNodeForXLog(rel);

	lastTids_size = buf->curword * sizeof(uint64);
	cwords_size = buf->curword * sizeof(BM_HRL_WORD);
	hwords_size = (BM_CALC_H_WORDS(buf->curword)) *
		sizeof(BM_HRL_WORD);

	bitmapPage = BufferGetPage(bitmapBuffer);
	bitmapPageOpaque =
		(BMBitmapOpaque)PageGetSpecialPointer(bitmapPage);

	xlBitmapWords = (xl_bm_bitmapwords *)
		palloc0(MAXALIGN(sizeof(xl_bm_bitmapwords)) + MAXALIGN(lastTids_size) +
				MAXALIGN(cwords_size) + MAXALIGN(hwords_size));

	xlBitmapWords->bm_node = rel->rd_node;
	RelationGetPTInfo(rel, &xlBitmapWords->bm_persistentTid, &xlBitmapWords->bm_persistentSerialNum);
	xlBitmapWords->bm_blkno = BufferGetBlockNumber(bitmapBuffer);
	xlBitmapWords->bm_next_blkno = nextBlkno;
	xlBitmapWords->bm_last_tid = bitmapPageOpaque->bm_last_tid_location;
	xlBitmapWords->bm_lov_blkno = BufferGetBlockNumber(lovBuffer);
	xlBitmapWords->bm_lov_offset = lovOffset;
	xlBitmapWords->bm_last_compword = buf->last_compword;
	xlBitmapWords->bm_last_word = buf->last_word;
	xlBitmapWords->lov_words_header =
		(buf->is_last_compword_fill) ? 2 : 0;
	xlBitmapWords->bm_last_setbit = tidnum;
	xlBitmapWords->bm_is_last = isLast;
	xlBitmapWords->bm_is_first = isFirst;

	xlBitmapWords->bm_start_wordno = buf->start_wordno;
	xlBitmapWords->bm_words_written = words_written;
	xlBitmapWords->bm_num_cwords = buf->curword;
	lastTids = (uint64*)(((char*)xlBitmapWords) +
						 MAXALIGN(sizeof(xl_bm_bitmapwords)));
	memcpy(lastTids, buf->last_tids,
		   buf->curword * sizeof(uint64));

	cwords = (BM_HRL_WORD*)(((char*)xlBitmapWords) +
							MAXALIGN(sizeof(xl_bm_bitmapwords)) + MAXALIGN(lastTids_size));
	memcpy(cwords, buf->cwords, cwords_size);
	hwords = (BM_HRL_WORD*)(((char*)xlBitmapWords) +
						 MAXALIGN(sizeof(xl_bm_bitmapwords)) + MAXALIGN(lastTids_size) +
						 MAXALIGN(cwords_size));
	memcpy(hwords, buf->hwords, hwords_size);

	rdata[0].buffer = InvalidBuffer;
	rdata[0].data = (char*)xlBitmapWords;
	rdata[0].len = MAXALIGN(sizeof(xl_bm_bitmapwords)) + MAXALIGN(lastTids_size) +
					MAXALIGN(cwords_size) + MAXALIGN(hwords_size);
	rdata[0].next = NULL;

	recptr = XLogInsert(RM_BITMAP_ID, XLOG_BITMAP_INSERT_WORDS, rdata);

	PageSetLSN(bitmapPage, recptr);

	PageSetLSN(lovPage, recptr);

	pfree(xlBitmapWords);
}

/*
 * _bitmap_log_updateword() -- log updating a single word in a given
 * 	bitmap page.
 */
void
_bitmap_log_updateword(Relation rel, Buffer bitmapBuffer, int word_no)
{
	Page				bitmapPage;
	BMBitmap			bitmap;
	xl_bm_updateword	xlBitmapWord;
	XLogRecPtr			recptr;
	XLogRecData			rdata[1];

	// Fetch gp_persistent_relation_node information that will be added to XLOG record.
	RelationFetchGpRelationNodeForXLog(rel);

	bitmapPage = BufferGetPage(bitmapBuffer);
	bitmap = (BMBitmap) PageGetContentsMaxAligned(bitmapPage);

	xlBitmapWord.bm_node = rel->rd_node;
	RelationGetPTInfo(rel, &xlBitmapWord.bm_persistentTid, &xlBitmapWord.bm_persistentSerialNum);
	xlBitmapWord.bm_blkno = BufferGetBlockNumber(bitmapBuffer);
	xlBitmapWord.bm_word_no = word_no;
	xlBitmapWord.bm_cword = bitmap->cwords[word_no];
	xlBitmapWord.bm_hword = bitmap->hwords[word_no/BM_HRL_WORD_SIZE];

	elog(DEBUG1, "_bitmap_log_updateword: (blkno, word_no, cword, hword)="
		 "(%d, %d, " INT64_FORMAT ", " INT64_FORMAT ")", xlBitmapWord.bm_blkno,
		 xlBitmapWord.bm_word_no, xlBitmapWord.bm_cword,
		 xlBitmapWord.bm_hword);

	rdata[0].buffer = InvalidBuffer;
	rdata[0].data = (char*)&xlBitmapWord;
	rdata[0].len = sizeof(xl_bm_updateword);
	rdata[0].next = NULL;

	recptr = XLogInsert(RM_BITMAP_ID, XLOG_BITMAP_UPDATEWORD, rdata);

	PageSetLSN(bitmapPage, recptr);
}
						

/*
 * _bitmap_log_updatewords() -- log updating bitmap words in one or
 * 	two bitmap pages.
 *
 * If nextBuffer is Invalid, we only update one page.
 *
 */
void
_bitmap_log_updatewords(Relation rel,
						Buffer lovBuffer, OffsetNumber lovOffset,
						Buffer firstBuffer, Buffer secondBuffer,
						bool new_lastpage)
{
	Page				firstPage = NULL;
	Page				secondPage = NULL;
	BMBitmap			firstBitmap;
	BMBitmap			secondBitmap;
	BMBitmapOpaque		firstOpaque;
	BMBitmapOpaque		secondOpaque;

	xl_bm_updatewords	xlBitmapWords;
	XLogRecPtr			recptr;
	XLogRecData			rdata[1];


	firstPage = BufferGetPage(firstBuffer);
	firstBitmap = (BMBitmap) PageGetContentsMaxAligned(firstPage);
	firstOpaque = (BMBitmapOpaque)PageGetSpecialPointer(firstPage);
	xlBitmapWords.bm_two_pages = false;
	xlBitmapWords.bm_first_blkno = BufferGetBlockNumber(firstBuffer);
	memcpy(&xlBitmapWords.bm_first_cwords,
			firstBitmap->cwords,
			BM_NUM_OF_HRL_WORDS_PER_PAGE * sizeof(BM_HRL_WORD));
	memcpy(&xlBitmapWords.bm_first_hwords,
			firstBitmap->hwords,
			BM_NUM_OF_HEADER_WORDS * sizeof(BM_HRL_WORD));
	xlBitmapWords.bm_first_last_tid = firstOpaque->bm_last_tid_location;
	xlBitmapWords.bm_first_num_cwords =
		firstOpaque->bm_hrl_words_used;
	xlBitmapWords.bm_next_blkno = firstOpaque->bm_bitmap_next;

	if (BufferIsValid(secondBuffer))
	{
		secondPage = BufferGetPage(secondBuffer);
		secondBitmap = (BMBitmap) PageGetContentsMaxAligned(secondPage);
		secondOpaque = (BMBitmapOpaque)PageGetSpecialPointer(secondPage);

		xlBitmapWords.bm_two_pages = true;
		xlBitmapWords.bm_second_blkno = BufferGetBlockNumber(secondBuffer);

		memcpy(&xlBitmapWords.bm_second_cwords,
				secondBitmap->cwords,
				BM_NUM_OF_HRL_WORDS_PER_PAGE * sizeof(BM_HRL_WORD));
		memcpy(&xlBitmapWords.bm_second_hwords,
				secondBitmap->hwords,
				BM_NUM_OF_HEADER_WORDS * sizeof(BM_HRL_WORD));
		xlBitmapWords.bm_second_last_tid = secondOpaque->bm_last_tid_location;
		xlBitmapWords.bm_second_num_cwords =
			secondOpaque->bm_hrl_words_used;
		xlBitmapWords.bm_next_blkno = secondOpaque->bm_bitmap_next;
	}

	// Fetch gp_persistent_relation_node information that will be added to XLOG record.
	RelationFetchGpRelationNodeForXLog(rel);

	xlBitmapWords.bm_node = rel->rd_node;
	RelationGetPTInfo(rel, &xlBitmapWords.bm_persistentTid, &xlBitmapWords.bm_persistentSerialNum);
	xlBitmapWords.bm_lov_blkno = BufferGetBlockNumber(lovBuffer);
	xlBitmapWords.bm_lov_offset = lovOffset;
	xlBitmapWords.bm_new_lastpage = new_lastpage;

	rdata[0].buffer = InvalidBuffer;
	rdata[0].data = (char*)&xlBitmapWords;
	rdata[0].len = sizeof(xl_bm_updatewords);
	rdata[0].next = NULL;

	recptr = XLogInsert(RM_BITMAP_ID, XLOG_BITMAP_UPDATEWORDS, rdata);

	PageSetLSN(firstPage, recptr);

	if (BufferIsValid(secondBuffer))
	{
		PageSetLSN(secondPage, recptr);
	}

	if (new_lastpage)
	{
		Page lovPage = BufferGetPage(lovBuffer);

		PageSetLSN(lovPage, recptr);
	}
}

Datum
bmoptions(PG_FUNCTION_ARGS)
{
	Datum		reloptions = PG_GETARG_DATUM(0);
	bool		validate = PG_GETARG_BOOL(1);
	bytea	   *result;

	result = default_reloptions(reloptions, validate,
								RELOPT_KIND_BITMAP);
	if (result)
		PG_RETURN_BYTEA_P(result);
	PG_RETURN_NULL();
}
