/*-------------------------------------------------------------------------
 *
 * nbtdesc.c
 *	  rmgr descriptor routines for access/nbtree/nbtxlog.c
 *
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/rmgrdesc/nbtdesc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/nbtree.h"

static void
out_target(StringInfo buf, xl_btreetid *target)
{
	appendStringInfo(buf, "rel %u/%u/%u; tid %u/%u",
			 target->node.spcNode, target->node.dbNode, target->node.relNode,
					 ItemPointerGetBlockNumber(&(target->tid)),
					 ItemPointerGetOffsetNumber(&(target->tid)));
}

/*
 * Print additional information about an INSERT record.
 */
static void
out_insert(StringInfo buf, bool isleaf, bool ismeta, XLogRecord *record)
{
	char			*rec = XLogRecGetData(record);
	xl_btree_insert *xlrec = (xl_btree_insert *) rec;

	char	   *datapos;
	int			datalen;
	xl_btree_metadata md = { InvalidBlockNumber, 0, InvalidBlockNumber, 0 };
	BlockNumber downlink = 0;

	datapos = (char *) xlrec + SizeOfBtreeInsert;
	datalen = record->xl_len - SizeOfBtreeInsert;
	if (!isleaf)
	{
		memcpy(&downlink, datapos, sizeof(BlockNumber));
		datapos += sizeof(BlockNumber);
		datalen -= sizeof(BlockNumber);
	}
	if (ismeta)
	{
		memcpy(&md, datapos, sizeof(xl_btree_metadata));
		datapos += sizeof(xl_btree_metadata);
		datalen -= sizeof(xl_btree_metadata);
	}

	if ((record->xl_info & XLR_BKP_BLOCK(0)) != 0 && !ismeta && isleaf)
	{
		appendStringInfo(buf, "; page %u",
						 ItemPointerGetBlockNumber(&(xlrec->target.tid)));
		return;					/* nothing to do */
	}

	if ((record->xl_info & XLR_BKP_BLOCK(0)) == 0)
	{
		appendStringInfo(buf, "; add length %d item at offset %d in page %u",
						 datalen, 
						 ItemPointerGetOffsetNumber(&(xlrec->target.tid)),
						 ItemPointerGetBlockNumber(&(xlrec->target.tid)));
	}

	if (ismeta)
		appendStringInfo(buf, "; restore metadata page 0 (root page value %u, level %d, fastroot page value %u, fastlevel %d)",
						 md.root, 
						 md.level,
						 md.fastroot, 
						 md.fastlevel);

	/* Forget any split this insertion completes */
//	if (!isleaf)
//		appendStringInfo(buf, "; completes split for page %u",
//		 				 downlink);
}

/*
 * Print additional information about a DELETE record.
 */
static void
out_delete(StringInfo buf, XLogRecord *record)
{
	char			*rec = XLogRecGetData(record);
	xl_btree_delete *xlrec = (xl_btree_delete *) rec;

	if ((record->xl_info & XLR_BKP_BLOCK(0)) != 0)
		return;

	xlrec = (xl_btree_delete *) XLogRecGetData(record);

	if (record->xl_len > SizeOfBtreeDelete)
	{
		OffsetNumber *unused;
		OffsetNumber *unend;

		unused = (OffsetNumber *) ((char *) xlrec + SizeOfBtreeDelete);
		unend = (OffsetNumber *) ((char *) xlrec + record->xl_len);

		appendStringInfo(buf, "; page index (unend - unused = %u)",
						 (unsigned int)(unend - unused));
	}
}

void
btree_desc(StringInfo buf, XLogRecord *record)
{
	char	   *rec = XLogRecGetData(record);
	uint8		xl_info = record->xl_info;
	uint8		info = xl_info & ~XLR_INFO_MASK;

	switch (info)
	{
		case XLOG_BTREE_INSERT_LEAF:
			{
				xl_btree_insert *xlrec = (xl_btree_insert *) rec;

				appendStringInfoString(buf, "insert: ");
				out_target(buf, &(xlrec->target));
				out_insert(buf, /* isleaf */ true, /* ismeta */ false, record);
				break;
			}
		case XLOG_BTREE_INSERT_UPPER:
			{
				xl_btree_insert *xlrec = (xl_btree_insert *) rec;

				appendStringInfoString(buf, "insert_upper: ");
				out_target(buf, &(xlrec->target));
				out_insert(buf, /* isleaf */ false, /* ismeta */ false, record);
				break;
			}
		case XLOG_BTREE_INSERT_META:
			{
				xl_btree_insert *xlrec = (xl_btree_insert *) rec;

				appendStringInfoString(buf, "insert_meta: ");
				out_target(buf, &(xlrec->target));
				out_insert(buf, /* isleaf */ false, /* ismeta */ true, record);
				break;
			}
		case XLOG_BTREE_SPLIT_L:
			{
				xl_btree_split *xlrec = (xl_btree_split *) rec;

				appendStringInfo(buf, "split_l: rel %u/%u/%u ",
								 xlrec->node.spcNode, xlrec->node.dbNode,
								 xlrec->node.relNode);
				appendStringInfo(buf, "left %u, right %u, next %u, level %u, firstright %d",
							   xlrec->leftsib, xlrec->rightsib, xlrec->rnext,
								 xlrec->level, xlrec->firstright);
				break;
			}
		case XLOG_BTREE_SPLIT_R:
			{
				xl_btree_split *xlrec = (xl_btree_split *) rec;

				appendStringInfo(buf, "split_r: rel %u/%u/%u ",
								 xlrec->node.spcNode, xlrec->node.dbNode,
								 xlrec->node.relNode);
				appendStringInfo(buf, "left %u, right %u, next %u, level %u, firstright %d",
							   xlrec->leftsib, xlrec->rightsib, xlrec->rnext,
								 xlrec->level, xlrec->firstright);
				break;
			}
		case XLOG_BTREE_SPLIT_L_ROOT:
			{
				xl_btree_split *xlrec = (xl_btree_split *) rec;

				appendStringInfo(buf, "split_l_root: rel %u/%u/%u ",
								 xlrec->node.spcNode, xlrec->node.dbNode,
								 xlrec->node.relNode);
				appendStringInfo(buf, "left %u, right %u, next %u, level %u, firstright %d",
							   xlrec->leftsib, xlrec->rightsib, xlrec->rnext,
								 xlrec->level, xlrec->firstright);
				break;
			}
		case XLOG_BTREE_SPLIT_R_ROOT:
			{
				xl_btree_split *xlrec = (xl_btree_split *) rec;

				appendStringInfo(buf, "split_r_root: rel %u/%u/%u ",
								 xlrec->node.spcNode, xlrec->node.dbNode,
								 xlrec->node.relNode);
				appendStringInfo(buf, "left %u, right %u, next %u, level %u, firstright %d",
							   xlrec->leftsib, xlrec->rightsib, xlrec->rnext,
								 xlrec->level, xlrec->firstright);
				break;
			}
		case XLOG_BTREE_VACUUM:
			{
				xl_btree_vacuum *xlrec = (xl_btree_vacuum *) rec;

				appendStringInfo(buf, "vacuum: rel %u/%u/%u; blk %u, lastBlockVacuumed %u",
								 xlrec->node.spcNode, xlrec->node.dbNode,
								 xlrec->node.relNode, xlrec->block,
								 xlrec->lastBlockVacuumed);
				break;
			}
		case XLOG_BTREE_DELETE:
			{
				xl_btree_delete *xlrec = (xl_btree_delete *) rec;

				appendStringInfo(buf, "delete: index %u/%u/%u; iblk %u, heap %u/%u/%u;",
				xlrec->node.spcNode, xlrec->node.dbNode, xlrec->node.relNode,
								 xlrec->block,
								 xlrec->hnode.spcNode, xlrec->hnode.dbNode, xlrec->hnode.relNode);
				out_delete(buf, record);
				break;
			}
		case XLOG_BTREE_MARK_PAGE_HALFDEAD:
			{
				xl_btree_mark_page_halfdead *xlrec = (xl_btree_mark_page_halfdead *) rec;

				appendStringInfoString(buf, "mark_page_halfdead: ");
				out_target(buf, &(xlrec->target));
				appendStringInfo(buf, "; topparent %u; leaf %u; left %u; right %u",
								 xlrec->topparent, xlrec->leafblk, xlrec->leftblk, xlrec->rightblk);
				break;
			}
		case XLOG_BTREE_UNLINK_PAGE_META:
		case XLOG_BTREE_UNLINK_PAGE:
			{
				xl_btree_unlink_page *xlrec = (xl_btree_unlink_page *) rec;

				appendStringInfo(buf, "unlink_page: rel %u/%u/%u; ",
				xlrec->node.spcNode, xlrec->node.dbNode, xlrec->node.relNode);
				appendStringInfo(buf, "dead %u; left %u; right %u; btpo_xact %u; ",
								 xlrec->deadblk, xlrec->leftsib, xlrec->rightsib, xlrec->btpo_xact);
				appendStringInfo(buf, "leaf %u; leafleft %u; leafright %u; topparent %u",
								 xlrec->leafblk, xlrec->leafleftsib, xlrec->leafrightsib, xlrec->topparent);
				break;
			}
		case XLOG_BTREE_NEWROOT:
			{
				xl_btree_newroot *xlrec = (xl_btree_newroot *) rec;

				appendStringInfo(buf, "newroot: rel %u/%u/%u; root %u lev %u",
								 xlrec->node.spcNode, xlrec->node.dbNode,
								 xlrec->node.relNode,
								 xlrec->rootblk, xlrec->level);
				break;
			}
		case XLOG_BTREE_REUSE_PAGE:
			{
				xl_btree_reuse_page *xlrec = (xl_btree_reuse_page *) rec;

				appendStringInfo(buf, "reuse_page: rel %u/%u/%u; latestRemovedXid %u",
								 xlrec->node.spcNode, xlrec->node.dbNode,
							   xlrec->node.relNode, xlrec->latestRemovedXid);
				break;
			}
		default:
			appendStringInfoString(buf, "UNKNOWN");
			break;
	}
}
