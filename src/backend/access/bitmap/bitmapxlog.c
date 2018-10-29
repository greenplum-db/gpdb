/*-------------------------------------------------------------------------
 *
 * bitmapxlog.c
 *	  WAL replay logic for the bitmap index.
 *
 * Portions Copyright (c) 2007-2010 Greenplum Inc
 * Portions Copyright (c) 2010-2012 EMC Corporation
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 2006-2008, PostgreSQL Global Development Group
 * 
 *
 * IDENTIFICATION
 *	  src/backend/access/bitmap/bitmapxlog.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <unistd.h>

#include "access/bitmap.h"
#include "access/xlogutils.h"
#include "utils/guc.h"

static void forget_incomplete_insert_bitmapwords(RelFileNode node,
									 xl_bm_bitmapwords* newWords);
/*
 * We must keep track of expected insertion of bitmap words when these
 * bitmap words are inserted into multiple bitmap pages. We need to manually
 * insert these words if they are not seen in the WAL log during replay.
 * This makes it safe for page insertion to be a multiple-WAL-action process.
 */
typedef xl_bm_bitmapwords bm_incomplete_action;

static List *incomplete_actions;

static void
log_incomplete_insert_bitmapwords(RelFileNode node,
								  xl_bm_bitmapwords* newWords)
{
	int					lastTids_size;
	int					cwords_size;
	int					hwords_size;
	int					total_size;
	bm_incomplete_action *action;

	/* Delete the previous entry */
	forget_incomplete_insert_bitmapwords(node, newWords);	

	lastTids_size = newWords->bm_num_cwords * sizeof(uint64);
	cwords_size = newWords->bm_num_cwords * sizeof(BM_HRL_WORD);
	hwords_size = (BM_CALC_H_WORDS(newWords->bm_num_cwords)) *
		sizeof(BM_HRL_WORD);
	total_size = MAXALIGN(sizeof(bm_incomplete_action)) + MAXALIGN(lastTids_size) +
		MAXALIGN(cwords_size) + MAXALIGN(hwords_size);

	action = palloc0(total_size);
	memcpy(action, newWords, total_size);

	/* Reset the following fields */
	action->bm_blkno = newWords->bm_next_blkno;
	action->bm_next_blkno = InvalidBlockNumber;
	action->bm_start_wordno =
		newWords->bm_start_wordno + newWords->bm_words_written;
	action->bm_words_written = 0;

	incomplete_actions = lappend(incomplete_actions, action);
}

static void
forget_incomplete_insert_bitmapwords(RelFileNode node,
									 xl_bm_bitmapwords* newWords)
{
	ListCell* l;

	foreach (l, incomplete_actions)
	{
		bm_incomplete_action *action = (bm_incomplete_action *) lfirst(l);

		if (RelFileNodeEquals(node, action->bm_node) &&
			(action->bm_lov_blkno == newWords->bm_lov_blkno &&
			 action->bm_lov_offset == newWords->bm_lov_offset &&
			 action->bm_last_setbit == newWords->bm_last_setbit) &&
			!action->bm_is_last)
		{
			Assert(action->bm_blkno == newWords->bm_blkno);

			incomplete_actions = list_delete_ptr(incomplete_actions, action);
			pfree(action);
			break;
		}		
	}
}

/*
 * _bitmap_xlog_insert_lovitem() -- insert a new lov item.
 */
static void
_bitmap_xlog_insert_lovitem(XLogRecPtr lsn, XLogRecord *record)
{
	xl_bm_lovitem	*xlrec = (xl_bm_lovitem *) XLogRecGetData(record);

	if (record->xl_info & XLR_BKP_BLOCK(0))
	{
		(void) RestoreBackupBlock(lsn, record, 0, false, false);
	}
	else
	{
		Buffer			lovBuffer;
		Page			lovPage;

		if (xlrec->bm_is_new_lov_blkno)
		{
			lovBuffer = XLogReadBufferExtended(xlrec->bm_node, xlrec->bm_fork,
					xlrec->bm_lov_blkno, RBM_ZERO);
			Assert(BufferIsValid(lovBuffer));
			LockBuffer(lovBuffer, BUFFER_LOCK_EXCLUSIVE);
		}
		else
		{
			lovBuffer = XLogReadBuffer(xlrec->bm_node, xlrec->bm_lov_blkno, false);
			if (!BufferIsValid(lovBuffer))
				return;
		}

		lovPage = BufferGetPage(lovBuffer);

		if (PageIsNew(lovPage))
			_bitmap_init_lovpage(NULL, lovBuffer);

		elog(DEBUG1, "In redo, processing a lovItem: (blockno, offset)=(%d,%d)",
				xlrec->bm_lov_blkno, xlrec->bm_lov_offset);

		if (PageGetLSN(lovPage) < lsn)
		{
			OffsetNumber	newOffset, itemSize;

			newOffset = OffsetNumberNext(PageGetMaxOffsetNumber(lovPage));
			if (newOffset > xlrec->bm_lov_offset)
				elog(PANIC, "_bitmap_xlog_insert_lovitem: LOV item is not inserted "
						"in pos %d (requested %d)",
						newOffset, xlrec->bm_lov_offset);

			/*
			 * The value newOffset could be smaller than xlrec->bm_lov_offset because
			 * of aborted transactions.
			 */
			if (newOffset < xlrec->bm_lov_offset)
			{
				_bitmap_relbuf(lovBuffer);
				return;
			}

			itemSize = sizeof(BMLOVItemData);
			if (itemSize > PageGetFreeSpace(lovPage))
				elog(PANIC, 
						"_bitmap_xlog_insert_lovitem: not enough space in LOV page %d",
						xlrec->bm_lov_blkno);

			if (PageAddItem(lovPage, (Item)&(xlrec->bm_lovItem), itemSize, 
						newOffset, false, false) == InvalidOffsetNumber)
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("_bitmap_xlog_insert_lovitem: failed to add LOV item")));

			PageSetLSN(lovPage, lsn);

			_bitmap_wrtbuf(lovBuffer);
		}
		else
			_bitmap_relbuf(lovBuffer);
	}

	/* Update the meta page when needed */
	if (!xlrec->bm_is_new_lov_blkno)
		return;

	if (record->xl_info & XLR_BKP_BLOCK(1))
	{
		(void) RestoreBackupBlock(lsn, record, 1, false, false);
	}
	else
	{
		BMMetaPage	metapage;
		Buffer		metabuf;

		metabuf = XLogReadBufferExtended(xlrec->bm_node, xlrec->bm_fork,
												BM_METAPAGE, RBM_ZERO);
		if (!BufferIsValid(metabuf))
 			return;
		
		LockBuffer(metabuf, BUFFER_LOCK_EXCLUSIVE);

		metapage = (BMMetaPage) PageGetContents(BufferGetPage(metabuf));
		if (PageGetLSN(BufferGetPage(metabuf)) < lsn)
		{
			metapage->bm_lov_lastpage = xlrec->bm_lov_blkno;

			PageSetLSN(BufferGetPage(metabuf), lsn);

			_bitmap_wrtbuf(metabuf);
		}
		else
		{
			_bitmap_relbuf(metabuf);
		}
	}
}

/*
 * _bitmap_xlog_insert_meta() -- update a metapage.
 */
static void
_bitmap_xlog_insert_meta(XLogRecPtr lsn, XLogRecord *record)
{
	xl_bm_metapage	*xlrec = (xl_bm_metapage *) XLogRecGetData(record);
	Buffer			metabuf;
	Page			mp;
	BMMetaPage		metapage;

	metabuf = XLogReadBufferExtended(xlrec->bm_node, xlrec->bm_fork, BM_METAPAGE, RBM_ZERO);
	Assert(BufferIsValid(metabuf));
	LockBuffer(metabuf, BUFFER_LOCK_EXCLUSIVE);

	mp = BufferGetPage(metabuf);
	if (PageIsNew(mp))
		PageInit(mp, BufferGetPageSize(metabuf), 0);

	if (PageGetLSN(mp) < lsn)
	{
		metapage = (BMMetaPage)PageGetContents(mp);

		metapage->bm_magic = BITMAP_MAGIC;
		metapage->bm_version = BITMAP_VERSION;
		metapage->bm_lov_heapId = xlrec->bm_lov_heapId;
		metapage->bm_lov_indexId = xlrec->bm_lov_indexId;
		metapage->bm_lov_lastpage = xlrec->bm_lov_lastpage;

		PageSetLSN(mp, lsn);
		_bitmap_wrtbuf(metabuf);
	}
	else
		_bitmap_relbuf(metabuf);
}

/*
 * _bitmap_xlog_insert_bitmap_lastwords() -- update the last two words
 * in a bitmap vector.
 */
static void
_bitmap_xlog_insert_bitmap_lastwords(XLogRecPtr lsn, 
									 XLogRecord *record)
{
	xl_bm_bitmap_lastwords *xlrec;

	Buffer		lovBuffer;
	Page		lovPage;
	BMLOVItem	lovItem;

	/* If we have a full-page image, restore it and we're done */
	if (record->xl_info & XLR_BKP_BLOCK(0))
	{
		(void) RestoreBackupBlock(lsn, record, 0, false, false);
		return;
	}

	xlrec = (xl_bm_bitmap_lastwords *) XLogRecGetData(record);

	lovBuffer = XLogReadBuffer(xlrec->bm_node, xlrec->bm_lov_blkno, false);
	if (BufferIsValid(lovBuffer))
	{
		lovPage = BufferGetPage(lovBuffer);

		if (PageGetLSN(lovPage) < lsn)
		{
			ItemId item = PageGetItemId(lovPage, xlrec->bm_lov_offset);

			if (ItemIdIsUsed(item))
			{
				lovItem = (BMLOVItem)PageGetItem(lovPage, item);

				lovItem->bm_last_compword = xlrec->bm_last_compword;
				lovItem->bm_last_word = xlrec->bm_last_word;
				lovItem->lov_words_header = xlrec->lov_words_header;
				lovItem->bm_last_setbit = xlrec->bm_last_setbit;
				lovItem->bm_last_tid_location = xlrec->bm_last_tid_location;
				
				PageSetLSN(lovPage, lsn);

				_bitmap_wrtbuf(lovBuffer);
			}
			else
				_bitmap_relbuf(lovBuffer);
		}
		else
			_bitmap_relbuf(lovBuffer);
	}
}

static void
_bitmap_xlog_insert_bitmapwords(XLogRecPtr lsn, XLogRecord *record)
{
	xl_bm_bitmapwords *xlrec;

	Buffer		bitmapBuffer;
	Page		bitmapPage;
	BMBitmapOpaque	bitmapPageOpaque;
	BMTIDBuffer newWords;
	uint64		words_written;

	int					lastTids_size;
	int					cwords_size;
	int					hwords_size;

	Buffer lovBuffer;
	Page lovPage;
	BMLOVItem lovItem;
	
	xlrec = (xl_bm_bitmapwords *) XLogRecGetData(record);

	bitmapBuffer = XLogReadBuffer(xlrec->bm_node, xlrec->bm_blkno, true);
	bitmapPage = BufferGetPage(bitmapBuffer);

	if (PageIsNew(bitmapPage))
		_bitmap_init_bitmappage(NULL, bitmapBuffer);

	bitmapPageOpaque =
		(BMBitmapOpaque)PageGetSpecialPointer(bitmapPage);

	if (PageGetLSN(bitmapPage) < lsn)
	{
		uint64      *last_tids;
		BM_HRL_WORD *cwords;
		BM_HRL_WORD *hwords;

		newWords.curword = xlrec->bm_num_cwords;
		newWords.start_wordno = xlrec->bm_start_wordno;

		lastTids_size = newWords.curword * sizeof(uint64);
		cwords_size = newWords.curword * sizeof(BM_HRL_WORD);
		hwords_size = (BM_CALC_H_WORDS(newWords.curword)) *
			sizeof(BM_HRL_WORD);

		newWords.last_tids = (uint64*)palloc0(lastTids_size);
		newWords.cwords = (BM_HRL_WORD*)palloc0(cwords_size);

		last_tids = 
			(uint64*)(((char*)xlrec) + MAXALIGN(sizeof(xl_bm_bitmapwords)));
		cwords =
			(BM_HRL_WORD*)(((char*)xlrec) +
						 MAXALIGN(sizeof(xl_bm_bitmapwords)) + MAXALIGN(lastTids_size));
		hwords =
			(BM_HRL_WORD*)(((char*)xlrec) +
						   MAXALIGN(sizeof(xl_bm_bitmapwords)) + MAXALIGN(lastTids_size) +
						   MAXALIGN(cwords_size));
		memcpy(newWords.last_tids, last_tids, lastTids_size);
		memcpy(newWords.cwords, cwords, cwords_size);
		newWords.num_cwords = xlrec->bm_num_cwords;
		memcpy(newWords.hwords, hwords, hwords_size);

		/*
		 * If no words are written to this bitmap page, it means
		 * this bitmap page is full.
		 */
		if (xlrec->bm_words_written == 0)
		{
			Assert(BM_NUM_OF_HRL_WORDS_PER_PAGE -
				   bitmapPageOpaque->bm_hrl_words_used == 0);
			words_written = 0;
		}
		else
			words_written =
				_bitmap_write_bitmapwords(bitmapBuffer, &newWords);

		Assert(words_written == xlrec->bm_words_written);

		bitmapPageOpaque->bm_bitmap_next = xlrec->bm_next_blkno;
		Assert(bitmapPageOpaque->bm_last_tid_location == xlrec->bm_last_tid);

		if (xlrec->bm_is_last)
		{
			forget_incomplete_insert_bitmapwords(xlrec->bm_node, xlrec);
		}

		else {

			Buffer	nextBuffer;
			Page	nextPage;

			/* create a new bitmap page */
			nextBuffer = XLogReadBuffer(xlrec->bm_node, xlrec->bm_next_blkno, true);
			nextPage = BufferGetPage(nextBuffer);

			_bitmap_init_bitmappage(NULL, nextBuffer);
			
			PageSetLSN(nextPage, lsn);

			_bitmap_wrtbuf(nextBuffer);

			log_incomplete_insert_bitmapwords(xlrec->bm_node, xlrec);
		}

		PageSetLSN(bitmapPage, lsn);

		_bitmap_wrtbuf(bitmapBuffer);

		_bitmap_free_tidbuf(&newWords);
	}

	else {
		_bitmap_relbuf(bitmapBuffer);
	}

 	/* Update lovPage when needed */
	if (record->xl_info & XLR_BKP_BLOCK(0))
	{
		(void) RestoreBackupBlock(lsn, record, 0, false, false);
		return;
	}

 	lovBuffer = XLogReadBuffer(xlrec->bm_node, xlrec->bm_lov_blkno, false);
 	if (!BufferIsValid(lovBuffer))
 		return;
 	
 	lovPage = BufferGetPage(lovBuffer);
 	lovItem = (BMLOVItem)
 		PageGetItem(lovPage, 
 					PageGetItemId(lovPage, xlrec->bm_lov_offset));
 	
 	if (xlrec->bm_is_last && PageGetLSN(lovPage) < lsn)
 	{
 		lovItem->bm_last_compword = xlrec->bm_last_compword;
 		lovItem->bm_last_word = xlrec->bm_last_word;
 		lovItem->lov_words_header = xlrec->lov_words_header;
 		lovItem->bm_last_setbit = xlrec->bm_last_setbit;
 		lovItem->bm_last_tid_location = xlrec->bm_last_setbit -
 			xlrec->bm_last_setbit % BM_HRL_WORD_SIZE;
 		lovItem->bm_lov_tail = xlrec->bm_blkno;
 		if (lovItem->bm_lov_head == InvalidBlockNumber)
 			lovItem->bm_lov_head = lovItem->bm_lov_tail;
 		
 		PageSetLSN(lovPage, lsn);
 		
 		_bitmap_wrtbuf(lovBuffer);
 		
 	}
 	else if (xlrec->bm_is_first && PageGetLSN(lovPage) < lsn)
 	{
 		lovItem->bm_lov_head = xlrec->bm_blkno;
 		lovItem->bm_lov_tail = lovItem->bm_lov_head;
 		
 		PageSetLSN(lovPage, lsn);
 		
 		_bitmap_wrtbuf(lovBuffer);
 	}
 	else
 	{
 		_bitmap_relbuf(lovBuffer);
 	}
}

static void
_bitmap_xlog_updateword(XLogRecPtr lsn, XLogRecord *record)
{
	xl_bm_updateword *xlrec;

	Buffer			bitmapBuffer;
	Page			bitmapPage;
	BMBitmapOpaque	bitmapOpaque;
	BMBitmap 		bitmap;

	xlrec = (xl_bm_updateword *) XLogRecGetData(record);

	elog(DEBUG1, "_bitmap_xlog_updateword: (blkno, word_no, cword, hword)="
		 "(%d, %d, " INT64_FORMAT ", " INT64_FORMAT ")", xlrec->bm_blkno,
		 xlrec->bm_word_no, xlrec->bm_cword,
		 xlrec->bm_hword);

	if (record->xl_info & XLR_BKP_BLOCK(0))
	{
		(void) RestoreBackupBlock(lsn, record, 0, false, false);
		return;
	}

	bitmapBuffer = XLogReadBuffer(xlrec->bm_node, xlrec->bm_blkno, false);
	if (BufferIsValid(bitmapBuffer))
	{
		bitmapPage = BufferGetPage(bitmapBuffer);
		bitmapOpaque =
			(BMBitmapOpaque)PageGetSpecialPointer(bitmapPage);
		bitmap = (BMBitmap) PageGetContentsMaxAligned(bitmapPage);

		if (PageGetLSN(bitmapPage) < lsn)
		{
			Assert(bitmapOpaque->bm_hrl_words_used > xlrec->bm_word_no);

			bitmap->cwords[xlrec->bm_word_no] = xlrec->bm_cword;
			bitmap->hwords[xlrec->bm_word_no/BM_HRL_WORD_SIZE] = xlrec->bm_hword;

			PageSetLSN(bitmapPage, lsn);
			_bitmap_wrtbuf(bitmapBuffer);
		}
		else
			_bitmap_relbuf(bitmapBuffer);
	}
}

static void
_bitmap_xlog_updatewords(XLogRecPtr lsn, XLogRecord *record)
{
	xl_bm_updatewords *xlrec;

	xlrec = (xl_bm_updatewords *) XLogRecGetData(record);
	elog(DEBUG1, "_bitmap_xlog_updatewords: (first_blkno, num_cwords, last_tid, next_blkno)="
			"(%d, " INT64_FORMAT ", " INT64_FORMAT ", %d), (second_blkno, num_cwords, last_tid, next_blkno)="
			"(%d, " INT64_FORMAT ", " INT64_FORMAT ", %d)",
			xlrec->bm_first_blkno, xlrec->bm_first_num_cwords, xlrec->bm_first_last_tid,
			xlrec->bm_two_pages ? xlrec->bm_second_blkno : xlrec->bm_next_blkno,
			xlrec->bm_second_blkno, xlrec->bm_second_num_cwords,
			xlrec->bm_second_last_tid, xlrec->bm_next_blkno);

	if (record->xl_info & XLR_BKP_BLOCK(0))
	{
		(void) RestoreBackupBlock(lsn, record, 0, false, false);
	}
	else
	{
		Buffer	firstBuffer;
		Page	firstPage;
		BMBitmapOpaque	firstOpaque;
		BMBitmap 		firstBitmap;

		firstBuffer = XLogReadBuffer(xlrec->bm_node, xlrec->bm_first_blkno, false);
		if (BufferIsValid(firstBuffer))
		{
			firstPage = BufferGetPage(firstBuffer);
			firstOpaque =
				(BMBitmapOpaque)PageGetSpecialPointer(firstPage);
			firstBitmap = (BMBitmap) PageGetContentsMaxAligned(firstPage);

			if (PageGetLSN(firstPage) < lsn)
			{
				memcpy(firstBitmap->cwords, xlrec->bm_first_cwords,
						BM_NUM_OF_HRL_WORDS_PER_PAGE * sizeof(BM_HRL_WORD));
				memcpy(firstBitmap->hwords, xlrec->bm_first_hwords,
						BM_NUM_OF_HEADER_WORDS *	sizeof(BM_HRL_WORD));
				firstOpaque->bm_hrl_words_used = xlrec->bm_first_num_cwords;
				firstOpaque->bm_last_tid_location = xlrec->bm_first_last_tid;

				if (xlrec->bm_two_pages)
					firstOpaque->bm_bitmap_next = xlrec->bm_second_blkno;
				else
					firstOpaque->bm_bitmap_next = xlrec->bm_next_blkno;

				PageSetLSN(firstPage, lsn);
				_bitmap_wrtbuf(firstBuffer);
			}
			else
				_bitmap_relbuf(firstBuffer);
		}
	}

	/* Update secondPage when needed */
	if (xlrec->bm_two_pages)
	{
		if (record->xl_info & XLR_BKP_BLOCK(1))
		{
			(void) RestoreBackupBlock(lsn, record, 1, false, false);
		}
		else
		{
			Buffer	secondBuffer = InvalidBuffer;
			Page	secondPage = NULL;
			BMBitmapOpaque	secondOpaque = NULL;
			BMBitmap		secondBitmap = NULL;

			secondBuffer = XLogReadBuffer(xlrec->bm_node, xlrec->bm_second_blkno, true);
			secondPage = BufferGetPage(secondBuffer);
			if (PageIsNew(secondPage))
				_bitmap_init_bitmappage(NULL, secondBuffer);

			secondOpaque = (BMBitmapOpaque)PageGetSpecialPointer(secondPage);
			secondBitmap = (BMBitmap) PageGetContentsMaxAligned(secondPage);

			if (PageGetLSN(secondPage) < lsn)
			{
				memcpy(secondBitmap->cwords, xlrec->bm_second_cwords,
						BM_NUM_OF_HRL_WORDS_PER_PAGE * sizeof(BM_HRL_WORD));
				memcpy(secondBitmap->hwords, xlrec->bm_second_hwords,
						BM_NUM_OF_HEADER_WORDS *	sizeof(BM_HRL_WORD));
				secondOpaque->bm_hrl_words_used = xlrec->bm_second_num_cwords;
				secondOpaque->bm_last_tid_location = xlrec->bm_second_last_tid;
				secondOpaque->bm_bitmap_next = xlrec->bm_next_blkno;

				PageSetLSN(secondPage, lsn);
				_bitmap_wrtbuf(secondBuffer);
			}

			else
			{
				_bitmap_relbuf(secondBuffer);
			}
		}
	}

	/* Update lovPage when needed */
	if (xlrec->bm_new_lastpage)
	{
		Buffer lovBuffer;
		Page lovPage;
		BMLOVItem lovItem;
		int bkpNo = xlrec->bm_two_pages ? 2 : 1;

		if (record->xl_info & XLR_BKP_BLOCK(bkpNo))
		{
			(void) RestoreBackupBlock(lsn, record, bkpNo, false, false);
		}
		else
		{
			lovBuffer = XLogReadBuffer(xlrec->bm_node, xlrec->bm_lov_blkno, false);
			if (!BufferIsValid(lovBuffer))
				return;

			lovPage = BufferGetPage(lovBuffer);

			if (PageGetLSN(lovPage) < lsn)
			{
				lovItem = (BMLOVItem)
					PageGetItem(lovPage, 
							PageGetItemId(lovPage, xlrec->bm_lov_offset));

				lovItem->bm_lov_tail = xlrec->bm_second_blkno;

				PageSetLSN(lovPage, lsn);

				_bitmap_wrtbuf(lovBuffer);
			}
			else
			{
				_bitmap_relbuf(lovBuffer);
			}
		}
	}
}

void
bitmap_redo(XLogRecPtr beginLoc, XLogRecPtr lsn, XLogRecord *record)
{
	uint8	info = record->xl_info & ~XLR_INFO_MASK;

	switch (info)
	{
		case XLOG_BITMAP_INSERT_LOVITEM:
			_bitmap_xlog_insert_lovitem(lsn, record);
			break;
		case XLOG_BITMAP_INSERT_META:
			_bitmap_xlog_insert_meta(lsn, record);
			break;
		case XLOG_BITMAP_INSERT_BITMAP_LASTWORDS:
			_bitmap_xlog_insert_bitmap_lastwords(lsn, record);
			break;
		case XLOG_BITMAP_INSERT_WORDS:
			_bitmap_xlog_insert_bitmapwords(lsn, record);
			break;
		case XLOG_BITMAP_UPDATEWORD:
			_bitmap_xlog_updateword(lsn, record);
			break;
		case XLOG_BITMAP_UPDATEWORDS:
			_bitmap_xlog_updatewords(lsn, record);
			break;
		default:
			elog(PANIC, "bitmap_redo: unknown op code %u", info);
	}
}

void
bitmap_xlog_startup(void)
{
	incomplete_actions = NIL;
	/* sleep(30); */
}

void
bitmap_xlog_cleanup(void)
{
	ListCell* l;
	foreach (l, incomplete_actions)
	{
		Buffer				lovBuffer;
		BMTIDBuffer 	    newWords;

		int					lastTids_size;
		int					cwords_size;
		int					hwords_size;
		BM_HRL_WORD        *hwords;

		bm_incomplete_action *action = (bm_incomplete_action *) lfirst(l);

		lovBuffer = XLogReadBuffer(action->bm_node, action->bm_lov_blkno, false);

		newWords.num_cwords = action->bm_num_cwords;
		newWords.start_wordno = action->bm_start_wordno;

		lastTids_size = newWords.num_cwords * sizeof(uint64);
		cwords_size = newWords.num_cwords * sizeof(BM_HRL_WORD);
		hwords_size = (BM_CALC_H_WORDS(newWords.num_cwords)) *
			sizeof(BM_HRL_WORD);

		newWords.last_tids =
			(uint64*)(((char*)action) + MAXALIGN(sizeof(xl_bm_bitmapwords)));
		newWords.cwords =
			(BM_HRL_WORD*)(((char*)action) +
						 MAXALIGN(sizeof(xl_bm_bitmapwords)) + MAXALIGN(lastTids_size));
		hwords =
			(BM_HRL_WORD*)(((char*)action) +
						   MAXALIGN(sizeof(xl_bm_bitmapwords)) + MAXALIGN(lastTids_size) +
						   MAXALIGN(cwords_size));
		memcpy(newWords.hwords, hwords, hwords_size);

		newWords.last_compword = action->bm_last_compword;
		newWords.last_word = action->bm_last_word;
		newWords.is_last_compword_fill = (action->lov_words_header == 2);
		newWords.last_tid = action->bm_last_setbit;

		/* Finish an incomplete insert */
		_bitmap_write_new_bitmapwords(RelationIdGetRelation(action->bm_node.relNode),
							  lovBuffer, action->bm_lov_offset,
							  &newWords, false);

 		elog(DEBUG1, "finish incomplete insert of bitmap words: last_tid: " INT64_FORMAT
 			 ", lov_blkno=%d, lov_offset=%d",
 			 newWords.last_tids[newWords.num_cwords-1], action->bm_lov_blkno,
 			 action->bm_lov_offset);
	}
	incomplete_actions = NIL;
}

/*
 * GPDB_94_MERGE_FIXME: PostgreSQL 9.4 got rid of the "safe_restartpoint" and
 * incomplete actions mechanism. See commits 631118fe1e and 40dae7ec53. We
 * need to do the same for bitmap indexes.
 */
bool
bitmap_safe_restartpoint(void)
{
	if (incomplete_actions)
		return false;
	return true;
}
