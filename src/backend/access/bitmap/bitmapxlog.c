/*-------------------------------------------------------------------------
 *
 * bitmapxlog.c
 *	  WAL replay logic for the bitmap index.
 *
 * Portions Copyright (c) 2007-2010 Greenplum Inc
 * Portions Copyright (c) 2010-2012 EMC Corporation
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 2006-2008, PostgreSQL Global Development Group
 * 
 *
 * IDENTIFICATION
 *	  src/backend/access/bitmap/bitmapxlog.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/bitmap_private.h"
#include "access/bitmap_xlog.h"
#include "access/xlogutils.h"

/*
 * _bitmap_xlog_insert_lovitem() -- insert a new lov item.
 */
static void
_bitmap_xlog_insert_lovitem(XLogRecPtr lsn, XLogReaderState *record)
{
	Buffer			lovBuffer;
	xl_bm_lovitem	*xlrec = (xl_bm_lovitem *) XLogRecGetData(record);

	if (XLogReadBufferForRedo(record, 0, &lovBuffer) == BLK_NEEDS_REDO)
	{
		Page			lovPage;

		lovPage = BufferGetPage(lovBuffer);

		if (PageIsNew(lovPage))
			_bitmap_init_lovpage(NULL, lovBuffer);

		elog(DEBUG1, "In redo, processing a lovItem: (blockno, offset)=(%d,%d)",
				xlrec->bm_lov_blkno, xlrec->bm_lov_offset);

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
			UnlockReleaseBuffer(lovBuffer);
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

		MarkBufferDirty(lovBuffer);
	}
	if (BufferIsValid(lovBuffer))
		UnlockReleaseBuffer(lovBuffer);

	/* Update the meta page when needed */
	if (!xlrec->bm_is_new_lov_blkno)
		return;

	Buffer		metabuf;
	if (XLogReadBufferForRedo(record, 1, &metabuf) == BLK_NEEDS_REDO)
	{
		BMMetaPage	metapage;

		if (!BufferIsValid(metabuf))
 			return;
		
		metapage = (BMMetaPage) PageGetContents(BufferGetPage(metabuf));
		metapage->bm_lov_lastpage = xlrec->bm_lov_blkno;

		PageSetLSN(BufferGetPage(metabuf), lsn);

		MarkBufferDirty(metabuf);
	}
	if (BufferIsValid(metabuf))
		UnlockReleaseBuffer(metabuf);
}

/*
 * _bitmap_xlog_insert_meta() -- update a metapage.
 */
static void
_bitmap_xlog_insert_meta(XLogRecPtr lsn, XLogReaderState *record)
{
	xl_bm_metapage	*xlrec = (xl_bm_metapage *) XLogRecGetData(record);
	Buffer			metabuf;
	Page			mp;
	BMMetaPage		metapage;

	metabuf = XLogReadBufferExtended(xlrec->bm_node, xlrec->bm_fork, BM_METAPAGE, RBM_ZERO_AND_LOCK);
	Assert(BufferIsValid(metabuf));

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
									 XLogReaderState *record)
{
	xl_bm_bitmap_lastwords *xlrec;

	xlrec = (xl_bm_bitmap_lastwords *) XLogRecGetData(record);
	Buffer		lovBuffer;
	Page		lovPage;
	BMLOVItem	lovItem;

	/* If we have a full-page image, restore it and we're done */
	if (XLogReadBufferForRedo(record, 0, &lovBuffer) == BLK_NEEDS_REDO)
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

				MarkBufferDirty(lovBuffer);
			}
		}
#ifdef DUMP_BITMAPAM_INSERT_RECORDS
		_dump_page("redo", lsn, &xlrec->bm_node, lovBuffer);
#endif
	}
	if (BufferIsValid(lovBuffer))
		UnlockReleaseBuffer(lovBuffer);
}

static void
_bitmap_xlog_insert_bitmapwords(XLogRecPtr lsn, XLogReaderState *record)
{
	xl_bm_bitmapwords *xlrec;
	Buffer		lovBuffer;
	Buffer	   *bitmapBuffers;
	int			bmpageno;
	char	   *xlrecptr;
	BlockNumber first_blkno = InvalidBlockNumber;
	BlockNumber last_blkno = InvalidBlockNumber;

	/*
	 * Decode the WAL record.
	 *
	 * First comes the xl_bm_bitmapword_pages part
	 */
	xlrecptr = XLogRecGetData(record);
	xlrec = (xl_bm_bitmapwords *) xlrecptr;
	xlrecptr += sizeof(xl_bm_bitmapwords);

	Assert(xlrec->bm_num_pages > 0);

	bitmapBuffers = (Buffer *) palloc0(xlrec->bm_num_pages * sizeof(Buffer));

	/* Process the per-bitmap page data */
	for (bmpageno = 0; bmpageno < xlrec->bm_num_pages; bmpageno++)
	{
		xl_bm_bitmapwords_perpage *xlrec_perpage;
		BM_HRL_WORD *cwords;
		BM_HRL_WORD *hwords;
		Buffer		bitmapBuffer;
		Page		bitmapPage;
		BMBitmap	bitmap;
		BMBitmapOpaque	bitmapPageOpaque;
		XLogRedoAction	action;

		action = XLogReadBufferForRedo(record, bmpageno + 1, &bitmapBuffer);

		bitmapBuffers[bmpageno] = bitmapBuffer;

		if (bmpageno == 0)
			first_blkno = last_blkno = BufferGetBlockNumber(bitmapBuffer);

		if (action == BLK_NEEDS_REDO)
		{
			/*
			 * Decode the xl_bm_bitmapwords_perpage struct, and the hwords and cwords,
			 * for this bitmap page.
			 */
			xlrecptr = XLogRecGetBlockData(record, bmpageno + 1, NULL);
			xlrec_perpage = (xl_bm_bitmapwords_perpage *) xlrecptr;
			last_blkno = xlrec_perpage->bmp_blkno;
			xlrecptr += sizeof(xl_bm_bitmapwords_perpage);
			hwords = (BM_HRL_WORD *) xlrecptr;
			xlrecptr += xlrec_perpage->bmp_num_hwords * sizeof(BM_HRL_WORD);
			cwords = (BM_HRL_WORD *) xlrecptr;
			xlrecptr += xlrec_perpage->bmp_num_cwords * sizeof(BM_HRL_WORD);

			bitmapPage = BufferGetPage(bitmapBuffer);
			if (PageGetLSN(bitmapPage) >= lsn)
				continue;

			bitmap = (BMBitmap) PageGetContentsMaxAligned(bitmapPage);

			/*
			 * Copy the header and content words. Note: we WAL-log whole words only.
			 * If the insertion only set some bits of the last word, the whole
			 * word is included in the WAL record, anyway.
			 */
			memcpy(bitmap->hwords + xlrec_perpage->bmp_start_hword_no,
				   hwords,
				   xlrec_perpage->bmp_num_hwords * sizeof(BM_HRL_WORD));
			memcpy(bitmap->cwords + xlrec_perpage->bmp_start_cword_no,
				   cwords,
				   xlrec_perpage->bmp_num_cwords * sizeof(BM_HRL_WORD));

			/* Update next pointer. Peek into the next struct to get its block number */
			bitmapPageOpaque =
				(BMBitmapOpaque) PageGetSpecialPointer(bitmapPage);
			if (bmpageno + 1 < xlrec->bm_num_pages)
			{
				xl_bm_bitmapwords_perpage *next_xlrec_perpage;

				next_xlrec_perpage = (xl_bm_bitmapwords_perpage *) xlrecptr;

				bitmapPageOpaque->bm_bitmap_next = next_xlrec_perpage->bmp_blkno;
			}
			bitmapPageOpaque->bm_last_tid_location = xlrec_perpage->bmp_last_tid;
			bitmapPageOpaque->bm_hrl_words_used = xlrec_perpage->bmp_start_cword_no + xlrec_perpage->bmp_num_cwords;

			PageSetLSN(bitmapPage, lsn);
			MarkBufferDirty(bitmapBuffer);
		}
		else if (action == BLK_RESTORED)
		{
			/* has been restored in XLogReadBufferForRedo */
		}
	}

	/* The LOV page is in backup block # 0 */
	if (XLogReadBufferForRedo(record, 0, &lovBuffer) == BLK_NEEDS_REDO)
	{
		Page		lovPage = BufferGetPage(lovBuffer);

		BMLOVItem	lovItem;

		lovItem = (BMLOVItem)
			PageGetItem(lovPage,
						PageGetItemId(lovPage, xlrec->bm_lov_offset));
		lovItem->bm_last_compword = xlrec->bm_last_compword;
		lovItem->bm_last_word = xlrec->bm_last_word;
		lovItem->lov_words_header = xlrec->lov_words_header;
		lovItem->bm_last_setbit = xlrec->bm_last_setbit;
		lovItem->bm_last_tid_location = xlrec->bm_last_setbit -
			xlrec->bm_last_setbit % BM_HRL_WORD_SIZE;

		lovItem->bm_lov_tail = last_blkno;

		if (xlrec->bm_init_first_page)
			lovItem->bm_lov_head = first_blkno;

		PageSetLSN(lovPage, lsn);
		MarkBufferDirty(lovBuffer);
	}

	/*
	 * WAL consistency checking
	 */
#ifdef DUMP_BITMAPAM_INSERT_RECORDS
	_dump_page("redo", lsn, &xlrec->bm_node, lovBuffer);
	for (bmpageno = 0; bmpageno < xlrec->bm_num_pages; bmpageno++)
	{
		if (BufferIsValid(bitmapBuffers[bmpageno]))
			_dump_page("redo", lsn, &xlrec->bm_node, bitmapBuffers[bmpageno]);
	}
#endif

	/* Release buffers */
	if (BufferIsValid(lovBuffer))
		UnlockReleaseBuffer(lovBuffer);
	for (bmpageno = 0; bmpageno < xlrec->bm_num_pages; bmpageno++)
	{
		if (BufferIsValid(bitmapBuffers[bmpageno]))
			UnlockReleaseBuffer(bitmapBuffers[bmpageno]);
	}
}

static void
_bitmap_xlog_updateword(XLogRecPtr lsn, XLogReaderState *record)
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

	if (XLogReadBufferForRedo(record, 0, &bitmapBuffer) == BLK_NEEDS_REDO)
	{
		bitmapPage = BufferGetPage(bitmapBuffer);
		bitmapOpaque =
			(BMBitmapOpaque)PageGetSpecialPointer(bitmapPage);
		bitmap = (BMBitmap) PageGetContentsMaxAligned(bitmapPage);

		Assert(bitmapOpaque->bm_hrl_words_used > xlrec->bm_word_no);

		bitmap->cwords[xlrec->bm_word_no] = xlrec->bm_cword;
		bitmap->hwords[xlrec->bm_word_no/BM_HRL_WORD_SIZE] = xlrec->bm_hword;

		PageSetLSN(bitmapPage, lsn);
		MarkBufferDirty(bitmapBuffer);
	}
	if (BufferIsValid(bitmapBuffer))
		UnlockReleaseBuffer(bitmapBuffer);
}

static void
_bitmap_xlog_updatewords(XLogRecPtr lsn, XLogReaderState *record)
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

	Buffer	firstBuffer;
	Page	firstPage;
	if (XLogReadBufferForRedo(record, 0, &firstBuffer) == BLK_NEEDS_REDO)
	{
		BMBitmapOpaque	firstOpaque;
		BMBitmap 		firstBitmap;

		firstPage = BufferGetPage(firstBuffer);
		firstOpaque =
			(BMBitmapOpaque)PageGetSpecialPointer(firstPage);
		firstBitmap = (BMBitmap) PageGetContentsMaxAligned(firstPage);

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
		MarkBufferDirty(firstBuffer); 
	}
	if (BufferIsValid(firstBuffer))
		UnlockReleaseBuffer(firstBuffer);

	/* Update secondPage when needed */
	if (xlrec->bm_two_pages)
	{
		Buffer	secondBuffer = InvalidBuffer;
		if (XLogReadBufferForRedo(record, 1, &secondBuffer) == BLK_NEEDS_REDO)
		{
			Page	secondPage = NULL;
			BMBitmapOpaque	secondOpaque = NULL;
			BMBitmap		secondBitmap = NULL;

			secondPage = BufferGetPage(secondBuffer);
			if (PageIsNew(secondPage))
				_bitmap_init_bitmappage(secondPage);

			secondOpaque = (BMBitmapOpaque)PageGetSpecialPointer(secondPage);
			secondBitmap = (BMBitmap) PageGetContentsMaxAligned(secondPage);

			memcpy(secondBitmap->cwords, xlrec->bm_second_cwords,
					BM_NUM_OF_HRL_WORDS_PER_PAGE * sizeof(BM_HRL_WORD));
			memcpy(secondBitmap->hwords, xlrec->bm_second_hwords,
					BM_NUM_OF_HEADER_WORDS *	sizeof(BM_HRL_WORD));
			secondOpaque->bm_hrl_words_used = xlrec->bm_second_num_cwords;
			secondOpaque->bm_last_tid_location = xlrec->bm_second_last_tid;
			secondOpaque->bm_bitmap_next = xlrec->bm_next_blkno;

			PageSetLSN(secondPage, lsn);
			MarkBufferDirty(secondBuffer);
		}
		if (BufferIsValid(secondBuffer))
			UnlockReleaseBuffer(secondBuffer);
	}

	/* Update lovPage when needed */
	if (xlrec->bm_new_lastpage)
	{
		Buffer lovBuffer;
		Page lovPage;
		BMLOVItem lovItem;
		int bkpNo = xlrec->bm_two_pages ? 2 : 1;

		if (XLogReadBufferForRedo(record, bkpNo, &lovBuffer) == BLK_NEEDS_REDO)
		{
			lovPage = BufferGetPage(lovBuffer);

			lovItem = (BMLOVItem)
				PageGetItem(lovPage,
						PageGetItemId(lovPage, xlrec->bm_lov_offset));

			lovItem->bm_lov_tail = xlrec->bm_second_blkno;

			PageSetLSN(lovPage, lsn);

			MarkBufferDirty(lovBuffer);
		}
		if (BufferIsValid(lovBuffer))
			UnlockReleaseBuffer(lovBuffer);
	}
}

void
bitmap_redo(XLogReaderState *record)
{
	uint8	info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;
	XLogRecPtr	lsn = record->EndRecPtr;

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
