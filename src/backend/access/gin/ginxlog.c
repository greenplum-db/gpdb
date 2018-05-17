/*-------------------------------------------------------------------------
 *
 * ginxlog.c
 *	  WAL replay logic for inverted index.
 *
 *
 * Portions Copyright (c) 1996-2011, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			 src/backend/access/gin/ginxlog.c
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/gin.h"
#include "access/gin_private.h"
#include "access/bufmask.h"
#include "access/xlogutils.h"
#include "storage/bufmgr.h"
#include "utils/memutils.h"

static MemoryContext opCtx;		/* working memory for operations */
static MemoryContext topCtx;

typedef struct ginIncompleteSplit
{
	RelFileNode node;
	BlockNumber leftBlkno;
	BlockNumber rightBlkno;
	BlockNumber rootBlkno;
} ginIncompleteSplit;

static List *incomplete_splits;

static void
pushIncompleteSplit(RelFileNode node, BlockNumber leftBlkno, BlockNumber rightBlkno, BlockNumber rootBlkno)
{
	ginIncompleteSplit *split;

	MemoryContextSwitchTo(topCtx);

	split = palloc(sizeof(ginIncompleteSplit));

	split->node = node;
	split->leftBlkno = leftBlkno;
	split->rightBlkno = rightBlkno;
	split->rootBlkno = rootBlkno;

	incomplete_splits = lappend(incomplete_splits, split);

	MemoryContextSwitchTo(opCtx);
}

static void
forgetIncompleteSplit(RelFileNode node, BlockNumber leftBlkno, BlockNumber updateBlkno)
{
	ListCell   *l;

	foreach(l, incomplete_splits)
	{
		ginIncompleteSplit *split = (ginIncompleteSplit *) lfirst(l);

		if (RelFileNodeEquals(node, split->node) && leftBlkno == split->leftBlkno && updateBlkno == split->rightBlkno)
		{
			incomplete_splits = list_delete_ptr(incomplete_splits, split);
			break;
		}
	}
}

static void
ginRedoCreateIndex(XLogRecPtr lsn, XLogRecord *record)
{
	RelFileNode *node = (RelFileNode *) XLogRecGetData(record);
	Buffer		RootBuffer,
				MetaBuffer;
	Page		page;

	MetaBuffer = XLogReadBuffer(*node, GIN_METAPAGE_BLKNO, true);
	Assert(BufferIsValid(MetaBuffer));
	page = (Page) BufferGetPage(MetaBuffer);

	GinInitMetabuffer(MetaBuffer);

	PageSetLSN(page, lsn);
	MarkBufferDirty(MetaBuffer);

	RootBuffer = XLogReadBuffer(*node, GIN_ROOT_BLKNO, true);
	Assert(BufferIsValid(RootBuffer));
	page = (Page) BufferGetPage(RootBuffer);

	GinInitBuffer(RootBuffer, GIN_LEAF);

	PageSetLSN(page, lsn);
	MarkBufferDirty(RootBuffer);

	UnlockReleaseBuffer(RootBuffer);
	UnlockReleaseBuffer(MetaBuffer);
}

static void
ginRedoCreatePTree(XLogRecPtr lsn, XLogRecord *record)
{
	ginxlogCreatePostingTree *data = (ginxlogCreatePostingTree *) XLogRecGetData(record);
	ItemPointerData *items = (ItemPointerData *) (XLogRecGetData(record) + sizeof(ginxlogCreatePostingTree));
	Buffer		buffer;
	Page		page;

	buffer = XLogReadBuffer(data->node, data->blkno, true);
	Assert(BufferIsValid(buffer));
	page = (Page) BufferGetPage(buffer);

	GinInitBuffer(buffer, GIN_DATA | GIN_LEAF);
	memcpy(GinDataPageGetData(page), items, sizeof(ItemPointerData) * data->nitem);
	GinPageGetOpaque(page)->maxoff = data->nitem;

	PageSetLSN(page, lsn);

	MarkBufferDirty(buffer);
	UnlockReleaseBuffer(buffer);
}

static void
ginRedoInsert(XLogRecPtr lsn, XLogRecord *record)
{
	ginxlogInsert *data = (ginxlogInsert *) XLogRecGetData(record);
	Buffer		buffer;
	Page		page;

	/* first, forget any incomplete split this insertion completes */
	if (data->isData)
	{
		Assert(data->isDelete == FALSE);
		if (!data->isLeaf && data->updateBlkno != InvalidBlockNumber)
		{
			PostingItem *pitem;

			pitem = (PostingItem *) (XLogRecGetData(record) + sizeof(ginxlogInsert));
			forgetIncompleteSplit(data->node,
								  PostingItemGetBlockNumber(pitem),
								  data->updateBlkno);
		}

	}
	else
	{
		if (!data->isLeaf && data->updateBlkno != InvalidBlockNumber)
		{
			IndexTuple	itup;

			itup = (IndexTuple) (XLogRecGetData(record) + sizeof(ginxlogInsert));
			forgetIncompleteSplit(data->node,
								  GinGetDownlink(itup),
								  data->updateBlkno);
		}
	}

	/* nothing else to do if page was backed up */
	if (IsBkpBlockApplied(record, 0))
		return;

	buffer = XLogReadBuffer(data->node, data->blkno, false);
	if (!BufferIsValid(buffer))
		return;					/* page was deleted, nothing to do */
	page = (Page) BufferGetPage(buffer);

	if (!XLByteLE(lsn, PageGetLSN(page)))
	{
		if (data->isData)
		{
			Assert(GinPageIsData(page));

			if (data->isLeaf)
			{
				OffsetNumber i;
				ItemPointerData *items = (ItemPointerData *) (XLogRecGetData(record) + sizeof(ginxlogInsert));

				Assert(GinPageIsLeaf(page));
				Assert(data->updateBlkno == InvalidBlockNumber);

				for (i = 0; i < data->nitem; i++)
					GinDataPageAddItem(page, items + i, data->offset + i);
			}
			else
			{
				PostingItem *pitem;

				Assert(!GinPageIsLeaf(page));

				if (data->updateBlkno != InvalidBlockNumber)
				{
					/* update link to right page after split */
					pitem = (PostingItem *) GinDataPageGetItem(page, data->offset);
					PostingItemSetBlockNumber(pitem, data->updateBlkno);
				}

				pitem = (PostingItem *) (XLogRecGetData(record) + sizeof(ginxlogInsert));

				GinDataPageAddItem(page, pitem, data->offset);
			}
		}
		else
		{
			IndexTuple	itup;

			Assert(!GinPageIsData(page));

			if (data->updateBlkno != InvalidBlockNumber)
			{
				/* update link to right page after split */
				Assert(!GinPageIsLeaf(page));
				Assert(data->offset >= FirstOffsetNumber && data->offset <= PageGetMaxOffsetNumber(page));
				itup = (IndexTuple) PageGetItem(page, PageGetItemId(page, data->offset));
				GinSetDownlink(itup, data->updateBlkno);
			}

			if (data->isDelete)
			{
				Assert(GinPageIsLeaf(page));
				Assert(data->offset >= FirstOffsetNumber && data->offset <= PageGetMaxOffsetNumber(page));
				PageIndexTupleDelete(page, data->offset);
			}

			itup = (IndexTuple) (XLogRecGetData(record) + sizeof(ginxlogInsert));

			if (PageAddItem(page, (Item) itup, IndexTupleSize(itup), data->offset, false, false) == InvalidOffsetNumber)
				elog(ERROR, "failed to add item to index page in %u/%u/%u",
					 data->node.spcNode, data->node.dbNode, data->node.relNode);
		}

		PageSetLSN(page, lsn);

		MarkBufferDirty(buffer);
	}

	UnlockReleaseBuffer(buffer);
}

static void
ginRedoSplit(XLogRecPtr lsn, XLogRecord *record)
{
	ginxlogSplit *data = (ginxlogSplit *) XLogRecGetData(record);
	Buffer		lbuffer,
				rbuffer;
	Page		lpage,
				rpage;
	uint32		flags = 0;

	if (data->isLeaf)
		flags |= GIN_LEAF;
	if (data->isData)
		flags |= GIN_DATA;

	lbuffer = XLogReadBuffer(data->node, data->lblkno, true);
	Assert(BufferIsValid(lbuffer));
	lpage = (Page) BufferGetPage(lbuffer);
	GinInitBuffer(lbuffer, flags);

	rbuffer = XLogReadBuffer(data->node, data->rblkno, true);
	Assert(BufferIsValid(rbuffer));
	rpage = (Page) BufferGetPage(rbuffer);
	GinInitBuffer(rbuffer, flags);

	GinPageGetOpaque(lpage)->rightlink = BufferGetBlockNumber(rbuffer);
	GinPageGetOpaque(rpage)->rightlink = data->rrlink;

	if (data->isData)
	{
		char	   *ptr = XLogRecGetData(record) + sizeof(ginxlogSplit);
		Size		sizeofitem = GinSizeOfDataPageItem(lpage);
		OffsetNumber i;
		ItemPointer bound;

		for (i = 0; i < data->separator; i++)
		{
			GinDataPageAddItem(lpage, ptr, InvalidOffsetNumber);
			ptr += sizeofitem;
		}

		for (i = data->separator; i < data->nitem; i++)
		{
			GinDataPageAddItem(rpage, ptr, InvalidOffsetNumber);
			ptr += sizeofitem;
		}

		/* set up right key */
		bound = GinDataPageGetRightBound(lpage);
		if (data->isLeaf)
			*bound = *(ItemPointerData *) GinDataPageGetItem(lpage, GinPageGetOpaque(lpage)->maxoff);
		else
			*bound = ((PostingItem *) GinDataPageGetItem(lpage, GinPageGetOpaque(lpage)->maxoff))->key;

		bound = GinDataPageGetRightBound(rpage);
		*bound = data->rightbound;
	}
	else
	{
		IndexTuple	itup = (IndexTuple) (XLogRecGetData(record) + sizeof(ginxlogSplit));
		OffsetNumber i;

		for (i = 0; i < data->separator; i++)
		{
			if (PageAddItem(lpage, (Item) itup, IndexTupleSize(itup), InvalidOffsetNumber, false, false) == InvalidOffsetNumber)
				elog(ERROR, "failed to add item to index page in %u/%u/%u",
				  data->node.spcNode, data->node.dbNode, data->node.relNode);
			itup = (IndexTuple) (((char *) itup) + MAXALIGN(IndexTupleSize(itup)));
		}

		for (i = data->separator; i < data->nitem; i++)
		{
			if (PageAddItem(rpage, (Item) itup, IndexTupleSize(itup), InvalidOffsetNumber, false, false) == InvalidOffsetNumber)
				elog(ERROR, "failed to add item to index page in %u/%u/%u",
				  data->node.spcNode, data->node.dbNode, data->node.relNode);
			itup = (IndexTuple) (((char *) itup) + MAXALIGN(IndexTupleSize(itup)));
		}
	}

	PageSetLSN(rpage, lsn);
	MarkBufferDirty(rbuffer);

	PageSetLSN(lpage, lsn);
	MarkBufferDirty(lbuffer);

	if (!data->isLeaf && data->updateBlkno != InvalidBlockNumber)
		forgetIncompleteSplit(data->node, data->leftChildBlkno, data->updateBlkno);

	if (data->isRootSplit)
	{
		Buffer		rootBuf = XLogReadBuffer(data->node, data->rootBlkno, true);
		Page		rootPage = BufferGetPage(rootBuf);

		GinInitBuffer(rootBuf, flags & ~GIN_LEAF);

		if (data->isData)
		{
			Assert(data->rootBlkno != GIN_ROOT_BLKNO);
			ginDataFillRoot(NULL, rootBuf, lbuffer, rbuffer);
		}
		else
		{
			Assert(data->rootBlkno == GIN_ROOT_BLKNO);
			ginEntryFillRoot(NULL, rootBuf, lbuffer, rbuffer);
		}

		PageSetLSN(rootPage, lsn);

		MarkBufferDirty(rootBuf);
		UnlockReleaseBuffer(rootBuf);
	}
	else
		pushIncompleteSplit(data->node, data->lblkno, data->rblkno, data->rootBlkno);

	UnlockReleaseBuffer(rbuffer);
	UnlockReleaseBuffer(lbuffer);
}

static void
ginRedoVacuumPage(XLogRecPtr lsn, XLogRecord *record)
{
	ginxlogVacuumPage *data = (ginxlogVacuumPage *) XLogRecGetData(record);
	Buffer		buffer;
	Page		page;

	/* nothing to do if page was backed up (and no info to do it with) */
	if (IsBkpBlockApplied(record, 0))
		return;

	buffer = XLogReadBuffer(data->node, data->blkno, false);
	if (!BufferIsValid(buffer))
		return;
	page = (Page) BufferGetPage(buffer);

	if (!XLByteLE(lsn, PageGetLSN(page)))
	{
		if (GinPageIsData(page))
		{
			memcpy(GinDataPageGetData(page),
				   XLogRecGetData(record) + sizeof(ginxlogVacuumPage),
				   data->nitem * GinSizeOfDataPageItem(page));
			GinPageGetOpaque(page)->maxoff = data->nitem;
		}
		else
		{
			OffsetNumber i,
					   *tod;
			IndexTuple	itup = (IndexTuple) (XLogRecGetData(record) + sizeof(ginxlogVacuumPage));

			tod = (OffsetNumber *) palloc(sizeof(OffsetNumber) * PageGetMaxOffsetNumber(page));
			for (i = FirstOffsetNumber; i <= PageGetMaxOffsetNumber(page); i++)
				tod[i - 1] = i;

			PageIndexMultiDelete(page, tod, PageGetMaxOffsetNumber(page));

			for (i = 0; i < data->nitem; i++)
			{
				if (PageAddItem(page, (Item) itup, IndexTupleSize(itup), InvalidOffsetNumber, false, false) == InvalidOffsetNumber)
					elog(ERROR, "failed to add item to index page in %u/%u/%u",
						 data->node.spcNode, data->node.dbNode, data->node.relNode);
				itup = (IndexTuple) (((char *) itup) + MAXALIGN(IndexTupleSize(itup)));
			}
		}

		PageSetLSN(page, lsn);
		MarkBufferDirty(buffer);
	}

	UnlockReleaseBuffer(buffer);
}

static void
ginRedoDeletePage(XLogRecPtr lsn, XLogRecord *record)
{
	ginxlogDeletePage *data = (ginxlogDeletePage *) XLogRecGetData(record);
	Buffer		buffer;
	Page		page;

	if (!(IsBkpBlockApplied(record, 0)))
	{
		buffer = XLogReadBuffer(data->node, data->blkno, false);
		if (BufferIsValid(buffer))
		{
			page = BufferGetPage(buffer);
			if (!XLByteLE(lsn, PageGetLSN(page)))
			{
				Assert(GinPageIsData(page));
				GinPageGetOpaque(page)->flags = GIN_DELETED;
				PageSetLSN(page, lsn);
				MarkBufferDirty(buffer);
			}
			UnlockReleaseBuffer(buffer);
		}
	}

	if (!(IsBkpBlockApplied(record, 1)))
	{
		buffer = XLogReadBuffer(data->node, data->parentBlkno, false);
		if (BufferIsValid(buffer))
		{
			page = BufferGetPage(buffer);
			if (!XLByteLE(lsn, PageGetLSN(page)))
			{
				Assert(GinPageIsData(page));
				Assert(!GinPageIsLeaf(page));
				GinPageDeletePostingItem(page, data->parentOffset);
				PageSetLSN(page, lsn);
				MarkBufferDirty(buffer);
			}
			UnlockReleaseBuffer(buffer);
		}
	}

	if (!(IsBkpBlockApplied(record, 2)) && data->leftBlkno != InvalidBlockNumber)
	{
		buffer = XLogReadBuffer(data->node, data->leftBlkno, false);
		if (BufferIsValid(buffer))
		{
			page = BufferGetPage(buffer);
			if (!XLByteLE(lsn, PageGetLSN(page)))
			{
				Assert(GinPageIsData(page));
				GinPageGetOpaque(page)->rightlink = data->rightLink;
				PageSetLSN(page, lsn);
				MarkBufferDirty(buffer);
			}
			UnlockReleaseBuffer(buffer);
		}
	}
}

static void
ginRedoUpdateMetapage(XLogRecPtr lsn, XLogRecord *record)
{
	ginxlogUpdateMeta *data = (ginxlogUpdateMeta *) XLogRecGetData(record);
	Buffer		metabuffer;
	Page		metapage;
	Buffer		buffer;

	metabuffer = XLogReadBuffer(data->node, GIN_METAPAGE_BLKNO, false);
	if (!BufferIsValid(metabuffer))
		elog(PANIC, "GIN metapage disappeared");
	metapage = BufferGetPage(metabuffer);

	if (!XLByteLE(lsn, PageGetLSN(metapage)))
	{
		memcpy(GinPageGetMeta(metapage), &data->metadata, sizeof(GinMetaPageData));
		PageSetLSN(metapage, lsn);
		MarkBufferDirty(metabuffer);
	}

	if (data->ntuples > 0)
	{
		/*
		 * insert into tail page
		 */
		if (!(record->xl_info & XLR_BKP_BLOCK_1))
		{
			buffer = XLogReadBuffer(data->node, data->metadata.tail, false);
			if (BufferIsValid(buffer))
			{
				Page		page = BufferGetPage(buffer);

				if (!XLByteLE(lsn, PageGetLSN(page)))
				{
					OffsetNumber l,
								off = (PageIsEmpty(page)) ? FirstOffsetNumber :
					OffsetNumberNext(PageGetMaxOffsetNumber(page));
					int			i,
								tupsize;
					IndexTuple	tuples = (IndexTuple) (XLogRecGetData(record) + sizeof(ginxlogUpdateMeta));

					for (i = 0; i < data->ntuples; i++)
					{
						tupsize = IndexTupleSize(tuples);

						l = PageAddItem(page, (Item) tuples, tupsize, off, false, false);

						if (l == InvalidOffsetNumber)
							elog(ERROR, "failed to add item to index page");

						tuples = (IndexTuple) (((char *) tuples) + tupsize);
					}

					/*
					 * Increase counter of heap tuples
					 */
					GinPageGetOpaque(page)->maxoff++;

					PageSetLSN(page, lsn);
					MarkBufferDirty(buffer);
				}
				UnlockReleaseBuffer(buffer);
			}
		}
	}
	else if (data->prevTail != InvalidBlockNumber)
	{
		/*
		 * New tail
		 */
		buffer = XLogReadBuffer(data->node, data->prevTail, false);
		if (BufferIsValid(buffer))
		{
			Page		page = BufferGetPage(buffer);

			if (!XLByteLE(lsn, PageGetLSN(page)))
			{
				GinPageGetOpaque(page)->rightlink = data->newRightlink;

				PageSetLSN(page, lsn);
				MarkBufferDirty(buffer);
			}
			UnlockReleaseBuffer(buffer);
		}
	}

	UnlockReleaseBuffer(metabuffer);
}

static void
ginRedoInsertListPage(XLogRecPtr lsn, XLogRecord *record)
{
	ginxlogInsertListPage *data = (ginxlogInsertListPage *) XLogRecGetData(record);
	Buffer		buffer;
	Page		page;
	OffsetNumber l,
				off = FirstOffsetNumber;
	int			i,
				tupsize;
	IndexTuple	tuples = (IndexTuple) (XLogRecGetData(record) + sizeof(ginxlogInsertListPage));

	if (record->xl_info & XLR_BKP_BLOCK_1)
		return;

	buffer = XLogReadBuffer(data->node, data->blkno, true);
	Assert(BufferIsValid(buffer));
	page = BufferGetPage(buffer);

	GinInitBuffer(buffer, GIN_LIST);
	GinPageGetOpaque(page)->rightlink = data->rightlink;
	if (data->rightlink == InvalidBlockNumber)
	{
		/* tail of sublist */
		GinPageSetFullRow(page);
		GinPageGetOpaque(page)->maxoff = 1;
	}
	else
	{
		GinPageGetOpaque(page)->maxoff = 0;
	}

	for (i = 0; i < data->ntuples; i++)
	{
		tupsize = IndexTupleSize(tuples);

		l = PageAddItem(page, (Item) tuples, tupsize, off, false, false);

		if (l == InvalidOffsetNumber)
			elog(ERROR, "failed to add item to index page");

		tuples = (IndexTuple) (((char *) tuples) + tupsize);
	}

	PageSetLSN(page, lsn);
	MarkBufferDirty(buffer);

	UnlockReleaseBuffer(buffer);
}

static void
ginRedoDeleteListPages(XLogRecPtr lsn, XLogRecord *record)
{
	ginxlogDeleteListPages *data = (ginxlogDeleteListPages *) XLogRecGetData(record);
	Buffer		metabuffer;
	Page		metapage;
	int			i;

	metabuffer = XLogReadBuffer(data->node, GIN_METAPAGE_BLKNO, false);
	if (!BufferIsValid(metabuffer))
		elog(PANIC, "GIN metapage disappeared");
	metapage = BufferGetPage(metabuffer);

	if (!XLByteLE(lsn, PageGetLSN(metapage)))
	{
		memcpy(GinPageGetMeta(metapage), &data->metadata, sizeof(GinMetaPageData));
		PageSetLSN(metapage, lsn);
		MarkBufferDirty(metabuffer);
	}

	for (i = 0; i < data->ndeleted; i++)
	{
		Buffer		buffer = XLogReadBuffer(data->node, data->toDelete[i], false);

		if (BufferIsValid(buffer))
		{
			Page		page = BufferGetPage(buffer);

			if (!XLByteLE(lsn, PageGetLSN(page)))
			{
				GinPageGetOpaque(page)->flags = GIN_DELETED;

				PageSetLSN(page, lsn);
				MarkBufferDirty(buffer);
			}

			UnlockReleaseBuffer(buffer);
		}
	}
	UnlockReleaseBuffer(metabuffer);
}

void
gin_redo(XLogRecPtr beginLoc, XLogRecPtr lsn, XLogRecord *record)
{
	uint8		info = record->xl_info & ~XLR_INFO_MASK;

	/*
	 * GIN indexes do not require any conflict processing.
	 */

	RestoreBkpBlocks(lsn, record, false);

	topCtx = MemoryContextSwitchTo(opCtx);
	switch (info)
	{
		case XLOG_GIN_CREATE_INDEX:
			ginRedoCreateIndex(lsn, record);
			break;
		case XLOG_GIN_CREATE_PTREE:
			ginRedoCreatePTree(lsn, record);
			break;
		case XLOG_GIN_INSERT:
			ginRedoInsert(lsn, record);
			break;
		case XLOG_GIN_SPLIT:
			ginRedoSplit(lsn, record);
			break;
		case XLOG_GIN_VACUUM_PAGE:
			ginRedoVacuumPage(lsn, record);
			break;
		case XLOG_GIN_DELETE_PAGE:
			ginRedoDeletePage(lsn, record);
			break;
		case XLOG_GIN_UPDATE_META_PAGE:
			ginRedoUpdateMetapage(lsn, record);
			break;
		case XLOG_GIN_INSERT_LISTPAGE:
			ginRedoInsertListPage(lsn, record);
			break;
		case XLOG_GIN_DELETE_LISTPAGE:
			ginRedoDeleteListPages(lsn, record);
			break;
		default:
			elog(PANIC, "gin_redo: unknown op code %u", info);
	}
	MemoryContextSwitchTo(topCtx);
	MemoryContextReset(opCtx);
}

static void
desc_node(StringInfo buf, RelFileNode node, BlockNumber blkno)
{
	appendStringInfo(buf, "node: %u/%u/%u blkno: %u",
					 node.spcNode, node.dbNode, node.relNode, blkno);
}

void
gin_desc(StringInfo buf, XLogRecPtr beginLoc, XLogRecord *record)
{
	uint8		info = record->xl_info & ~XLR_INFO_MASK;
	char		*rec = XLogRecGetData(record);

	switch (info)
	{
		case XLOG_GIN_CREATE_INDEX:
			appendStringInfo(buf, "Create index, ");
			desc_node(buf, *(RelFileNode *) rec, GIN_ROOT_BLKNO);
			break;
		case XLOG_GIN_CREATE_PTREE:
			appendStringInfo(buf, "Create posting tree, ");
			desc_node(buf, ((ginxlogCreatePostingTree *) rec)->node, ((ginxlogCreatePostingTree *) rec)->blkno);
			break;
		case XLOG_GIN_INSERT:
			appendStringInfo(buf, "Insert item, ");
			desc_node(buf, ((ginxlogInsert *) rec)->node, ((ginxlogInsert *) rec)->blkno);
			appendStringInfo(buf, " offset: %u nitem: %u isdata: %c isleaf %c isdelete %c updateBlkno:%u",
							 ((ginxlogInsert *) rec)->offset,
							 ((ginxlogInsert *) rec)->nitem,
							 (((ginxlogInsert *) rec)->isData) ? 'T' : 'F',
							 (((ginxlogInsert *) rec)->isLeaf) ? 'T' : 'F',
							 (((ginxlogInsert *) rec)->isDelete) ? 'T' : 'F',
							 ((ginxlogInsert *) rec)->updateBlkno
				);

			break;
		case XLOG_GIN_SPLIT:
			appendStringInfo(buf, "Page split, ");
			desc_node(buf, ((ginxlogSplit *) rec)->node, ((ginxlogSplit *) rec)->lblkno);
			appendStringInfo(buf, " isrootsplit: %c", (((ginxlogSplit *) rec)->isRootSplit) ? 'T' : 'F');
			break;
		case XLOG_GIN_VACUUM_PAGE:
			appendStringInfo(buf, "Vacuum page, ");
			desc_node(buf, ((ginxlogVacuumPage *) rec)->node, ((ginxlogVacuumPage *) rec)->blkno);
			break;
		case XLOG_GIN_DELETE_PAGE:
			appendStringInfo(buf, "Delete page, ");
			desc_node(buf, ((ginxlogDeletePage *) rec)->node, ((ginxlogDeletePage *) rec)->blkno);
			break;
		case XLOG_GIN_UPDATE_META_PAGE:
			appendStringInfo(buf, "Update metapage, ");
			desc_node(buf, ((ginxlogUpdateMeta *) rec)->node, ((ginxlogUpdateMeta *) rec)->metadata.tail);
			break;
		case XLOG_GIN_INSERT_LISTPAGE:
			appendStringInfo(buf, "Insert new list page, ");
			desc_node(buf, ((ginxlogInsertListPage *) rec)->node, ((ginxlogInsertListPage *) rec)->blkno);
			break;
		case XLOG_GIN_DELETE_LISTPAGE:
			appendStringInfo(buf, "Delete list pages (%d), ", ((ginxlogDeleteListPages *) rec)->ndeleted);
			desc_node(buf, ((ginxlogDeleteListPages *) rec)->node, ((ginxlogDeleteListPages *) rec)->metadata.head);
			break;
		default:
			elog(PANIC, "gin_desc: unknown op code %u", info);
	}
}

void
gin_xlog_startup(void)
{
	incomplete_splits = NIL;

	opCtx = AllocSetContextCreate(CurrentMemoryContext,
								  "GIN recovery temporary context",
								  ALLOCSET_DEFAULT_MINSIZE,
								  ALLOCSET_DEFAULT_INITSIZE,
								  ALLOCSET_DEFAULT_MAXSIZE);
}

static void
ginContinueSplit(ginIncompleteSplit *split)
{
	GinBtreeData btree;
	GinState	ginstate;
	Relation	reln;
	Buffer		buffer;
	GinBtreeStack stack;

	/*
	 * elog(NOTICE,"ginContinueSplit root:%u l:%u r:%u",  split->rootBlkno,
	 * split->leftBlkno, split->rightBlkno);
	 */

	buffer = XLogReadBuffer(split->node, split->leftBlkno, false);

	/*
	 * Failure should be impossible here, because we wrote the page earlier.
	 */
	if (!BufferIsValid(buffer))
		elog(PANIC, "ginContinueSplit: left block %u not found",
			 split->leftBlkno);

	reln = CreateFakeRelcacheEntry(split->node);

	/*
	 * Failure should be impossible here, because we wrote the page earlier.
	 */
	if (!BufferIsValid(buffer))
		elog(PANIC, "ginContinueSplit: left block %u not found",
			 split->leftBlkno);

	if (split->rootBlkno == GIN_ROOT_BLKNO)
	{
		MemSet(&ginstate, 0, sizeof(ginstate));
		ginstate.index = reln;

		ginPrepareEntryScan(&btree,
							InvalidOffsetNumber, (Datum) 0, GIN_CAT_NULL_KEY,
							&ginstate);
		btree.entry = ginPageGetLinkItup(buffer);
	}
	else
	{
		Page		page = BufferGetPage(buffer);

		ginPrepareDataScan(&btree, reln);

		PostingItemSetBlockNumber(&(btree.pitem), split->leftBlkno);
		if (GinPageIsLeaf(page))
			btree.pitem.key = *(ItemPointerData *) GinDataPageGetItem(page,
											 GinPageGetOpaque(page)->maxoff);
		else
			btree.pitem.key = ((PostingItem *) GinDataPageGetItem(page,
									   GinPageGetOpaque(page)->maxoff))->key;
	}

	btree.rightblkno = split->rightBlkno;

	stack.blkno = split->leftBlkno;
	stack.buffer = buffer;
	stack.off = InvalidOffsetNumber;
	stack.parent = NULL;

	ginFindParents(&btree, &stack, split->rootBlkno);
	ginInsertValue(&btree, stack.parent, NULL);

	FreeFakeRelcacheEntry(reln);

	UnlockReleaseBuffer(buffer);
}

void
gin_xlog_cleanup(void)
{
	ListCell   *l;
	MemoryContext topCtx;

	topCtx = MemoryContextSwitchTo(opCtx);

	foreach(l, incomplete_splits)
	{
		ginIncompleteSplit *split = (ginIncompleteSplit *) lfirst(l);

		ginContinueSplit(split);
		MemoryContextReset(opCtx);
	}

	MemoryContextSwitchTo(topCtx);
	MemoryContextDelete(opCtx);
	incomplete_splits = NIL;
}

bool
gin_safe_restartpoint(void)
{
	if (incomplete_splits)
		return false;
	return true;
}

/*
 * Mask a GIN page before running consistency checks on it.
 */
void
gin_mask(char *pagedata, BlockNumber blkno)
{
	Page		page = (Page) pagedata;
	GinPageOpaque opaque;

	mask_page_lsn_and_checksum(page);
	opaque = GinPageGetOpaque(page);

	mask_page_hint_bits(page);

	/*
	 * GIN metapage doesn't use pd_lower/pd_upper. Other page types do. Hence,
	 * we need to apply masking for those pages.
	 */
	if (opaque->flags != GIN_META)
	{
		/*
		 * For GIN_DELETED page, the page is initialized to empty. Hence, mask
		 * the page content.
		 */
		if (opaque->flags & GIN_DELETED)
			mask_page_content(page);
		else
			mask_unused_space(page);
	}
}
