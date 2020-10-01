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
	appendStringInfo(buf, "rel %u/%u/%u",
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
			xl_bm_lovitem *xlrec = (xl_bm_lovitem *)rec;

			appendStringInfo(buf, "insert a new LOV item: ");
			out_target(buf, &(xlrec->bm_node));
			break;
		}
		case XLOG_BITMAP_INSERT_META:
		{
			xl_bm_metapage *xlrec = (xl_bm_metapage *)rec;

			appendStringInfo(buf, "update the metapage: ");
			out_target(buf, &(xlrec->bm_node));
			break;
		}

		case XLOG_BITMAP_INSERT_BITMAP_LASTWORDS:
		{
			xl_bm_bitmap_lastwords *xlrec = (xl_bm_bitmap_lastwords *)rec;

			appendStringInfo(buf, "update the last two words in a bitmap: ");
			out_target(buf, &(xlrec->bm_node));
			break;
		}

		case XLOG_BITMAP_INSERT_WORDS:
		{
			xl_bm_bitmapwords *xlrec = (xl_bm_bitmapwords *)rec;

			appendStringInfo(buf, "insert words in a not-last bitmap page: ");
			out_target(buf, &(xlrec->bm_node));
			break;
		}

		case XLOG_BITMAP_UPDATEWORD:
		{
			xl_bm_updateword *xlrec = (xl_bm_updateword *)rec;

			appendStringInfo(buf, "update a word in a bitmap page: ");
			out_target(buf, &(xlrec->bm_node));
			break;
		}
		case XLOG_BITMAP_UPDATEWORDS:
		{
			xl_bm_updatewords *xlrec = (xl_bm_updatewords*)rec;

			appendStringInfo(buf, "update words in bitmap pages: ");
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
