/*-------------------------------------------------------------------------
 *
 * bitmapdesc.c
 *	  rmgr descriptor routines for access/bitmap/bitmapxlog.c
 *
 * Portions Copyright (c) 1996-2013, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/rmgrdesc/bitmapdesc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/bitmap.h"
#include "access/bitmap_xlog.h"
#include "access/xlogreader.h"
#include "storage/relfilenode.h"

static void
out_target(StringInfo buf, RelFileNode *node)
{
	appendStringInfo(buf, ", rel %u/%u/%u",
			node->spcNode, node->dbNode, node->relNode);
}

void
bitmap_desc(StringInfo buf, XLogReaderState *record)
{
	uint8		info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;
	char		*rec = XLogRecGetData(record);

	switch (info)
	{
		case XLOG_BITMAP_INSERT_LOVITEM:
		{
			/* insert a new LOV item */
			xl_bm_lovitem *xlrec = (xl_bm_lovitem *)rec;

			appendStringInfo(buf, "bm_lov_blkno %d, bm_lov_offset %d, "
							 "bm_lovItem: {bm_lov_head %d, bm_lov_tail %d, bm_last_compword " UINT64_FORMAT
							 ", bm_last_word " UINT64_FORMAT ", bm_last_tid_location " UINT64_FORMAT
							 ", bm_last_setbit " UINT64_FORMAT ", lov_words_header %d}, bm_is_new_lov_blkno %s",
							 xlrec->bm_lov_blkno, xlrec->bm_lov_offset, xlrec->bm_lovItem.bm_lov_head,
							 xlrec->bm_lovItem.bm_lov_tail, xlrec->bm_lovItem.bm_last_compword,
							 xlrec->bm_lovItem.bm_last_word, xlrec->bm_lovItem.bm_last_tid_location,
							 xlrec->bm_lovItem.bm_last_setbit, xlrec->bm_lovItem.lov_words_header,
							 xlrec->bm_is_new_lov_blkno ? "true" : "false");
			out_target(buf, &(xlrec->bm_node));
			break;
		}
		case XLOG_BITMAP_INSERT_META:
		{
			/* update the metapage */
			xl_bm_metapage *xlrec = (xl_bm_metapage *)rec;

			appendStringInfo(buf, "bm_lov_heapId %u, bm_lov_indexId %u, bm_lov_lastpage %d",
							 xlrec->bm_lov_heapId, xlrec->bm_lov_indexId, xlrec->bm_lov_lastpage);
			out_target(buf, &(xlrec->bm_node));
			break;
		}

		case XLOG_BITMAP_INSERT_BITMAP_LASTWORDS:
		{
			/* update the last two words in a bitmap */
			xl_bm_bitmap_lastwords *xlrec = (xl_bm_bitmap_lastwords *)rec;

			appendStringInfo(buf, "bm_last_compword " UINT64_FORMAT ", bm_last_word " UINT64_FORMAT
							 ", lov_words_header %d, bm_last_setbit " UINT64_FORMAT
							 ", bm_last_tid_location " UINT64_FORMAT ", bm_lov_blkno %d, bm_lov_offset %d",
							 xlrec->bm_last_compword, xlrec->bm_last_word, xlrec->lov_words_header,
							 xlrec->bm_last_setbit, xlrec->bm_last_tid_location, xlrec->bm_lov_blkno,
							 xlrec->bm_lov_offset);
			out_target(buf, &(xlrec->bm_node));
			break;
		}

		case XLOG_BITMAP_INSERT_WORDS:
		{
			/* insert words in a not-last bitmap page */
			xl_bm_bitmapwords *xlrec = (xl_bm_bitmapwords *)rec;

			appendStringInfo(buf, "bm_num_pages %d, bm_init_first_page %s, bm_lov_blkno %d, bm_lov_offset %d, "
							 "bm_last_compword " UINT64_FORMAT ", bm_last_word " UINT64_FORMAT
							 ", lov_words_header %d, bm_last_setbit " UINT64_FORMAT,
							 xlrec->bm_num_pages, xlrec->bm_init_first_page ? "true" : "false",
							 xlrec->bm_lov_blkno, xlrec->bm_lov_offset, xlrec->bm_last_compword,
							 xlrec->bm_last_word, xlrec->lov_words_header, xlrec->bm_last_setbit);
			out_target(buf, &(xlrec->bm_node));
			break;
		}

		case XLOG_BITMAP_UPDATEWORD:
		{
			/* update a word in a bitmap page */
			xl_bm_updateword *xlrec = (xl_bm_updateword *)rec;

			appendStringInfo(buf, "bm_blkno %d, bm_word_no %d, bm_cword " UINT64_FORMAT ", bm_hword " UINT64_FORMAT,
							 xlrec->bm_blkno, xlrec->bm_word_no, xlrec->bm_cword, xlrec->bm_hword);
			out_target(buf, &(xlrec->bm_node));
			break;
		}
		case XLOG_BITMAP_UPDATEWORDS:
		{
			/* update words in bitmap pages */
			xl_bm_updatewords *xlrec = (xl_bm_updatewords*)rec;

			appendStringInfo(buf, "bm_lov_blkno %d, bm_lov_offset %d, "
							 "bm_first_blkno %d, bm_second_blkno %d, "
							 "bm_two_pages %s, bm_next_blkno %d, bm_new_lastpage %s",
							 xlrec->bm_lov_blkno, xlrec->bm_lov_offset,
							 xlrec->bm_first_blkno, xlrec->bm_second_blkno,
							 xlrec->bm_two_pages ? "true" : "false",
							 xlrec->bm_next_blkno,
							 xlrec->bm_new_lastpage ? "true" : "false");
			out_target(buf, &(xlrec->bm_node));
			break;
		}
		default:
			appendStringInfo(buf, "UNKNOWN");
			break;
	}
}

const char *
bitmap_identify(uint8 info)
{
	const char *id = NULL;

	switch (info & ~XLR_INFO_MASK)
	{
		case XLOG_BITMAP_INSERT_LOVITEM:
			id = "BITMAP_INSERT_LOVITEM";
			break;
		case XLOG_BITMAP_INSERT_META:
			id = "BITMAP_INSERT_META";
			break;
		case XLOG_BITMAP_INSERT_BITMAP_LASTWORDS:
			id = "BITMAP_INSERT_BITMAP_LASTWORDS";
			break;
		case XLOG_BITMAP_INSERT_WORDS:
			id = "BITMAP_INSERT_WORDS";
			break;
		case XLOG_BITMAP_UPDATEWORD:
			id = "BITMAP_UPDATEWORD";
			break;
		case XLOG_BITMAP_UPDATEWORDS:
			id = "BITMAP_UPDATEWORDS";
			break;
	}

	return id;
}
