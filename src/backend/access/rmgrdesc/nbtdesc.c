/*-------------------------------------------------------------------------
 *
 * nbtdesc.c
 *	  rmgr descriptor routines for access/nbtree/nbtxlog.c
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/rmgrdesc/nbtdesc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/nbtxlog.h"

/*
 * GPDB: Print additional information about an INSERT record.
 */
static void
out_insert(StringInfo buf, uint8 info, XLogReaderState *record)
{
	char		*rec = XLogRecGetData(record);
	char		*ptr;
	xl_btree_insert	*xlrec = (xl_btree_insert *) rec;
	xl_btree_metadata *md;
	BlockNumber	blkno;	
	bool		fullpage;
	Size		datalen;

	fullpage = XLogRecHasBlockImage(record, 0);
	XLogRecGetBlockTag(record, 0, NULL, NULL, &blkno);
	XLogRecGetBlockData(record, 0, &datalen);

	if (fullpage && info == XLOG_BTREE_INSERT_LEAF)
	{
		appendStringInfo(buf, "; page %u", blkno);
		return;					/* nothing to do */
	}

	if (!fullpage)
	{
		appendStringInfo(buf, "; add length %d item at offset %d in page %u",
						 (int) datalen, xlrec->offnum, blkno);
	}

	if (info == XLOG_BTREE_INSERT_META)
	{
		ptr = XLogRecGetBlockData(record, 2, NULL);
		md = (xl_btree_metadata *)ptr;
		appendStringInfo(buf, "; restore metadata page 0 (root page value %u, level %d, fastroot page value %u, fastlevel %d)",
						 md->root, 
						 md->level,
						 md->fastroot, 
						 md->fastlevel);
	}
}

/*
 * GPDB: Print additional information about a DELETE record.
 */
static void
out_delete(StringInfo buf, XLogReaderState *record)
{
	xl_btree_delete *xlrec = (xl_btree_delete *) XLogRecGetData(record);

	if (XLogRecHasBlockImage(record, 0))
		return;

	if (XLogRecGetDataLen(record) > SizeOfBtreeDelete)
	{
		OffsetNumber *unused;
		OffsetNumber *unend;

		unused = (OffsetNumber *) ((char *) xlrec + SizeOfBtreeDelete);
		unend = (OffsetNumber *) ((char *) xlrec + XLogRecGetDataLen(record));

		appendStringInfo(buf, "; page index (unend - unused = %u)",
						 (unsigned int)(unend - unused));
	}
}

void
btree_desc(StringInfo buf, XLogReaderState *record)
{
	char	   *rec = XLogRecGetData(record);
	uint8		info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

	switch (info)
	{
		case XLOG_BTREE_INSERT_LEAF:
		case XLOG_BTREE_INSERT_UPPER:
		case XLOG_BTREE_INSERT_META:
			{
				xl_btree_insert *xlrec = (xl_btree_insert *) rec;

				appendStringInfo(buf, "off %u", xlrec->offnum);
				out_insert(buf, info, record);
				break;
			}
		case XLOG_BTREE_SPLIT_L:
		case XLOG_BTREE_SPLIT_R:
			{
				xl_btree_split *xlrec = (xl_btree_split *) rec;

				appendStringInfo(buf, "level %u, firstright %d, newitemoff %d",
								 xlrec->level, xlrec->firstright, xlrec->newitemoff);
				break;
			}
		case XLOG_BTREE_VACUUM:
			{
				xl_btree_vacuum *xlrec = (xl_btree_vacuum *) rec;

				appendStringInfo(buf, "lastBlockVacuumed %u",
								 xlrec->lastBlockVacuumed);
				break;
			}
		case XLOG_BTREE_DELETE:
			{
				xl_btree_delete *xlrec = (xl_btree_delete *) rec;

				appendStringInfo(buf, "%d items, latest removed xid %u",
								 xlrec->nitems, xlrec->latestRemovedXid);
				out_delete(buf, record);
				break;
			}
		case XLOG_BTREE_MARK_PAGE_HALFDEAD:
			{
				xl_btree_mark_page_halfdead *xlrec = (xl_btree_mark_page_halfdead *) rec;

				appendStringInfo(buf, "topparent %u; leaf %u; left %u; right %u",
								 xlrec->topparent, xlrec->leafblk, xlrec->leftblk, xlrec->rightblk);
				break;
			}
		case XLOG_BTREE_UNLINK_PAGE_META:
		case XLOG_BTREE_UNLINK_PAGE:
			{
				xl_btree_unlink_page *xlrec = (xl_btree_unlink_page *) rec;

				appendStringInfo(buf, "left %u; right %u; btpo_xact %u; ",
								 xlrec->leftsib, xlrec->rightsib,
								 xlrec->btpo_xact);
				appendStringInfo(buf, "leafleft %u; leafright %u; topparent %u",
								 xlrec->leafleftsib, xlrec->leafrightsib,
								 xlrec->topparent);
				break;
			}
		case XLOG_BTREE_NEWROOT:
			{
				xl_btree_newroot *xlrec = (xl_btree_newroot *) rec;

				appendStringInfo(buf, "lev %u", xlrec->level);
				break;
			}
		case XLOG_BTREE_REUSE_PAGE:
			{
				xl_btree_reuse_page *xlrec = (xl_btree_reuse_page *) rec;

				appendStringInfo(buf, "rel %u/%u/%u; latestRemovedXid %u",
								 xlrec->node.spcNode, xlrec->node.dbNode,
								 xlrec->node.relNode, xlrec->latestRemovedXid);
				break;
			}
		case XLOG_BTREE_META_CLEANUP:
			{
				xl_btree_metadata *xlrec;

				xlrec = (xl_btree_metadata *) XLogRecGetBlockData(record, 0,
																  NULL);
				appendStringInfo(buf, "oldest_btpo_xact %u; last_cleanup_num_heap_tuples: %f",
								 xlrec->oldest_btpo_xact,
								 xlrec->last_cleanup_num_heap_tuples);
				break;
			}
	}
}

const char *
btree_identify(uint8 info)
{
	const char *id = NULL;

	switch (info & ~XLR_INFO_MASK)
	{
		case XLOG_BTREE_INSERT_LEAF:
			id = "INSERT_LEAF";
			break;
		case XLOG_BTREE_INSERT_UPPER:
			id = "INSERT_UPPER";
			break;
		case XLOG_BTREE_INSERT_META:
			id = "INSERT_META";
			break;
		case XLOG_BTREE_SPLIT_L:
			id = "SPLIT_L";
			break;
		case XLOG_BTREE_SPLIT_R:
			id = "SPLIT_R";
			break;
		case XLOG_BTREE_VACUUM:
			id = "VACUUM";
			break;
		case XLOG_BTREE_DELETE:
			id = "DELETE";
			break;
		case XLOG_BTREE_MARK_PAGE_HALFDEAD:
			id = "MARK_PAGE_HALFDEAD";
			break;
		case XLOG_BTREE_UNLINK_PAGE:
			id = "UNLINK_PAGE";
			break;
		case XLOG_BTREE_UNLINK_PAGE_META:
			id = "UNLINK_PAGE_META";
			break;
		case XLOG_BTREE_NEWROOT:
			id = "NEWROOT";
			break;
		case XLOG_BTREE_REUSE_PAGE:
			id = "REUSE_PAGE";
			break;
		case XLOG_BTREE_META_CLEANUP:
			id = "META_CLEANUP";
			break;
	}

	return id;
}
