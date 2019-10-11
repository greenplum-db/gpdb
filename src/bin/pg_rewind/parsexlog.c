/*-------------------------------------------------------------------------
 *
 * parsexlog.c
 *	  Functions for reading Write-Ahead-Log
 *
 * Portions Copyright (c) 1996-2015, PostgreSQL Global Development Group
 * Portions Copyright (c) 2013-2014 VMware, Inc. All Rights Reserved.
 * Portions Copyright (c) 1996-2008, Nippon Telegraph and Telephone Corporation
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *-------------------------------------------------------------------------
 */
/*
 * GPDB_95_MERGE_FIXME: This hack was borrowed from 9.4 version of
 * pg_rewind. Since for 9.4 this file needs to include heapam_xlog.h
 * file. Starting 9.5 with new WAL format the need is eliminated and hence
 * this hack must be removed and instead postgres_fe.h included.
 */
#define FRONTEND 1
#include "c.h"
#undef FRONTEND
#include "postgres.h"

#include <unistd.h>

#include "pg_rewind.h"
#include "filemap.h"
#include "logging.h"

#include <unistd.h>

#include "access/bitmap.h"
#include "access/heapam_xlog.h"
#include "access/gin_private.h"
#include "access/gist_private.h"
#include "access/spgist_private.h"
#include "access/nbtree.h"
#include "access/xlog_internal.h"
#include "access/xlogreader.h"
#include "catalog/pg_control.h"
#include "catalog/storage_xlog.h"
#include "commands/dbcommands.h"
#include "commands/tablespace.h"
#include "commands/sequence.h"
#include "cdb/cdbappendonlyxlog.h"

static void extractPageInfo(XLogRecord *record);

static int	xlogreadfd = -1;
static XLogSegNo xlogreadsegno = -1;
static char xlogfpath[MAXPGPATH];

typedef struct XLogPageReadPrivate
{
	const char *datadir;
	TimeLineID	tli;
} XLogPageReadPrivate;

static int SimpleXLogPageRead(XLogReaderState *xlogreader,
				   XLogRecPtr targetPagePtr,
				   int reqLen, XLogRecPtr targetRecPtr, char *readBuf,
				   TimeLineID *pageTLI);

/*
 * Read WAL from the datadir/pg_xlog, starting from 'startpoint' on timeline
 * 'tli', until 'endpoint'. Make note of the data blocks touched by the WAL
 * records, and return them in a page map.
 */
void
extractPageMap(const char *datadir, XLogRecPtr startpoint, TimeLineID tli,
			   XLogRecPtr endpoint)
{
	XLogRecord *record;
	XLogReaderState *xlogreader;
	char	   *errormsg;
	XLogPageReadPrivate private;

	private.datadir = datadir;
	private.tli = tli;
	xlogreader = XLogReaderAllocate(&SimpleXLogPageRead, &private);
	if (xlogreader == NULL)
		pg_fatal("out of memory\n");

	do
	{
		record = XLogReadRecord(xlogreader, startpoint, &errormsg);

		if (record == NULL)
		{
			XLogRecPtr	errptr;

			errptr = startpoint ? startpoint : xlogreader->EndRecPtr;

			if (errormsg)
				pg_fatal("could not read WAL record at %X/%X: %s\n",
						 (uint32) (errptr >> 32), (uint32) (errptr),
						 errormsg);
			else
				pg_fatal("could not read WAL record at %X/%X",
						 (uint32) (errptr >> 32), (uint32) (errptr));
		}

		extractPageInfo(record);

		startpoint = InvalidXLogRecPtr; /* continue reading at next record */

	} while (xlogreader->ReadRecPtr != endpoint);

	XLogReaderFree(xlogreader);
	if (xlogreadfd != -1)
	{
		close(xlogreadfd);
		xlogreadfd = -1;
	}
}

/*
 * Reads one WAL record. Returns the end position of the record, without
 * doing anything with the record itself.
 */
XLogRecPtr
readOneRecord(const char *datadir, XLogRecPtr ptr, TimeLineID tli)
{
	XLogRecord *record;
	XLogReaderState *xlogreader;
	char	   *errormsg;
	XLogPageReadPrivate private;
	XLogRecPtr	endptr;

	private.datadir = datadir;
	private.tli = tli;
	xlogreader = XLogReaderAllocate(&SimpleXLogPageRead, &private);
	if (xlogreader == NULL)
		pg_fatal("out of memory\n");

	record = XLogReadRecord(xlogreader, ptr, &errormsg);
	if (record == NULL)
	{
		if (errormsg)
			pg_fatal("could not read WAL record at %X/%X: %s\n",
					 (uint32) (ptr >> 32), (uint32) (ptr), errormsg);
		else
			pg_fatal("could not read WAL record at %X/%X\n",
					 (uint32) (ptr >> 32), (uint32) (ptr));
	}
	endptr = xlogreader->EndRecPtr;

	XLogReaderFree(xlogreader);
	if (xlogreadfd != -1)
	{
		close(xlogreadfd);
		xlogreadfd = -1;
	}

	return endptr;
}

/*
 * Find the previous checkpoint preceding given WAL position.
 */
void
findLastCheckpoint(const char *datadir, XLogRecPtr forkptr, TimeLineID tli,
				   XLogRecPtr *lastchkptrec, TimeLineID *lastchkpttli,
				   XLogRecPtr *lastchkptredo)
{
	/* Walk backwards, starting from the given record */
	XLogRecord *record;
	XLogRecPtr	searchptr;
	XLogReaderState *xlogreader;
	char	   *errormsg;
	XLogPageReadPrivate private;

	/*
	 * The given fork pointer points to the end of the last common record,
	 * which is not necessarily the beginning of the next record, if the
	 * previous record happens to end at a page boundary. Skip over the page
	 * header in that case to find the next record.
	 */
	if (forkptr % XLOG_BLCKSZ == 0)
		forkptr += (forkptr % XLogSegSize == 0) ? SizeOfXLogLongPHD : SizeOfXLogShortPHD;

	private.datadir = datadir;
	private.tli = tli;
	xlogreader = XLogReaderAllocate(&SimpleXLogPageRead, &private);
	if (xlogreader == NULL)
		pg_fatal("out of memory\n");

	searchptr = forkptr;
	for (;;)
	{
		uint8		info;

		record = XLogReadRecord(xlogreader, searchptr, &errormsg);

		if (record == NULL)
		{
			if (errormsg)
				pg_fatal("could not find previous WAL record at %X/%X: %s\n",
						 (uint32) (searchptr >> 32), (uint32) (searchptr),
						 errormsg);
			else
				pg_fatal("could not find previous WAL record at %X/%X\n",
						 (uint32) (searchptr >> 32), (uint32) (searchptr));
		}

		/*
		 * Check if it is a checkpoint record. This checkpoint record
		 * needs to be the latest checkpoint before WAL forked and not
		 * the checkpoint where the master has been stopped to be
		 * rewinded.
		 */
		info = record->xl_info & ~XLR_INFO_MASK;
		if (searchptr < forkptr &&
			record->xl_rmid == RM_XLOG_ID &&
			(info == XLOG_CHECKPOINT_SHUTDOWN || info == XLOG_CHECKPOINT_ONLINE))
		{
			CheckPoint	checkPoint;

			memcpy(&checkPoint, XLogRecGetData(record), sizeof(CheckPoint));
			*lastchkptrec = searchptr;
			*lastchkpttli = checkPoint.ThisTimeLineID;
			*lastchkptredo = checkPoint.redo;
			break;
		}

		/* Walk backwards to previous record. */
		searchptr = record->xl_prev;
	}

	XLogReaderFree(xlogreader);
	if (xlogreadfd != -1)
	{
		close(xlogreadfd);
		xlogreadfd = -1;
	}
}

/* XLogreader callback function, to read a WAL page */
static int
SimpleXLogPageRead(XLogReaderState *xlogreader, XLogRecPtr targetPagePtr,
				   int reqLen, XLogRecPtr targetRecPtr, char *readBuf,
				   TimeLineID *pageTLI)
{
	XLogPageReadPrivate *private = (XLogPageReadPrivate *) xlogreader->private_data;
	uint32		targetPageOff;
	XLogSegNo targetSegNo PG_USED_FOR_ASSERTS_ONLY;

	XLByteToSeg(targetPagePtr, targetSegNo);
	targetPageOff = targetPagePtr % XLogSegSize;

	/*
	 * See if we need to switch to a new segment because the requested record
	 * is not in the currently open one.
	 */
	if (xlogreadfd >= 0 && !XLByteInSeg(targetPagePtr, xlogreadsegno))
	{
		close(xlogreadfd);
		xlogreadfd = -1;
	}

	XLByteToSeg(targetPagePtr, xlogreadsegno);

	if (xlogreadfd < 0)
	{
		char		xlogfname[MAXFNAMELEN];

		XLogFileName(xlogfname, private->tli, xlogreadsegno);

		snprintf(xlogfpath, MAXPGPATH, "%s/" XLOGDIR "/%s", private->datadir, xlogfname);

		xlogreadfd = open(xlogfpath, O_RDONLY | PG_BINARY, 0);

		if (xlogreadfd < 0)
		{
			printf(_("could not open file \"%s\": %s\n"), xlogfpath,
				   strerror(errno));
			return -1;
		}
	}

	/*
	 * At this point, we have the right segment open.
	 */
	Assert(xlogreadfd != -1);

	/* Read the requested page */
	if (lseek(xlogreadfd, (off_t) targetPageOff, SEEK_SET) < 0)
	{
		printf(_("could not seek in file \"%s\": %s\n"), xlogfpath,
			   strerror(errno));
		return -1;
	}

	if (read(xlogreadfd, readBuf, XLOG_BLCKSZ) != XLOG_BLCKSZ)
	{
		printf(_("could not read from file \"%s\": %s\n"), xlogfpath,
			   strerror(errno));
		return -1;
	}

	Assert(targetSegNo == xlogreadsegno);

	*pageTLI = private->tli;
	return XLOG_BLCKSZ;
}

/*
 * Extract information on which blocks the current record modifies.
 *
 * GPDB_95_MERGE_FIXME: This functions was modified to work with 9.4 wal
 * record format. Once WAL format code from 9.5 is merged should replace the
 * logic in this function to also match upstream 9.5.
 */
static void
extractPageInfo(XLogRecord *record)
{
#define pageinfo_add(forkno, rnode, blkno) process_block_change(forkno, rnode, blkno)
#define pageinfo_set_truncation(forkno, rnode, blkno) datapagemap_set_truncation(pagemap, forkno, rnode, blkno)

	uint8   info = record->xl_info & ~XLR_INFO_MASK;

	switch (record->xl_rmid)
	{
		/*
		 * These rm's don't modify any relation files. They do modify other
		 * files, like the clog or multixact files, but they are always copied
		 * in toto.
		 */
		case RM_XLOG_ID:
		case RM_XACT_ID:
		case RM_CLOG_ID:
		case RM_MULTIXACT_ID:
		case RM_STANDBY_ID:
		case RM_RELMAP_ID:
		case RM_DISTRIBUTEDLOG_ID:
			break;

		case RM_HEAP_ID:
			switch (info & XLOG_HEAP_OPMASK)
			{
				case XLOG_HEAP_INSERT:
				{
					xl_heap_insert *xlrec =
						(xl_heap_insert *) XLogRecGetData(record);

					pageinfo_add(MAIN_FORKNUM,
								 xlrec->target.node,
								 ItemPointerGetBlockNumber(&xlrec->target.tid));
					break;
				}
				case XLOG_HEAP_DELETE:
				{
					xl_heap_delete *xlrec =
						(xl_heap_delete *) XLogRecGetData(record);

					pageinfo_add(MAIN_FORKNUM,
								 xlrec->target.node,
								 ItemPointerGetBlockNumber(&xlrec->target.tid));
					break;
				}
				case XLOG_HEAP_UPDATE:
				case XLOG_HEAP_HOT_UPDATE:
					{
						bool samepage;
						xl_heap_update *xlrec =
							(xl_heap_update *) XLogRecGetData(record);

						samepage = ItemPointerGetBlockNumber(&xlrec->newtid) ==
							ItemPointerGetBlockNumber(&xlrec->target.tid);

						/* store page which contains updated tuple. */
						pageinfo_add(MAIN_FORKNUM, xlrec->target.node,
									 ItemPointerGetBlockNumber(&xlrec->target.tid));
						/* store another page if any. */
						if (!samepage)
							pageinfo_add(MAIN_FORKNUM, xlrec->target.node,
										 ItemPointerGetBlockNumber(&xlrec->newtid));
						break;
					}

				case XLOG_HEAP_NEWPAGE:
					{
						xl_heap_newpage *xlrec =
							(xl_heap_newpage *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, xlrec->node, xlrec->blkno);
						break;
					}
				case XLOG_HEAP_LOCK:
					{
						xl_heap_lock *xlrec =
							(xl_heap_lock *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, xlrec->target.node,
									 ItemPointerGetBlockNumber(&xlrec->target.tid));
						break;
					}
				case XLOG_HEAP_INPLACE:
					{
						xl_heap_inplace *xlrec =
							(xl_heap_inplace *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, xlrec->target.node,
									 ItemPointerGetBlockNumber(&xlrec->target.tid));
						break;
					}

				default:
					fprintf(stderr, "unrecognized heap record type %d\n", info);
					exit(1);

			}
			break;

		case RM_HEAP2_ID:
			switch (info & XLOG_HEAP_OPMASK)
			{
				case XLOG_HEAP2_FREEZE_PAGE:
					{
						xl_heap_freeze_page *xlrec =
							(xl_heap_freeze_page *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, xlrec->node, xlrec->block);
						break;
					}
				case XLOG_HEAP2_CLEAN:
					{
						xl_heap_clean *xlrec =
							(xl_heap_clean *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, xlrec->node, xlrec->block);
						break;
					}
				case XLOG_HEAP2_CLEANUP_INFO:
					/* nothing to do */
					break;

				case XLOG_HEAP2_VISIBLE:
					{
						xl_heap_visible *xlrec =
							(xl_heap_visible *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, xlrec->node, xlrec->block);
						break;
					}


				case XLOG_HEAP2_MULTI_INSERT:
					{
						xl_heap_multi_insert *xlrec =
							(xl_heap_multi_insert *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, xlrec->node, xlrec->blkno);
						break;
					}

					fprintf(stderr, "multi-insert record type %d\n", info);
					exit(1);

				case XLOG_HEAP2_LOCK_UPDATED:
					{
						xl_heap_lock_updated *xlrec =
							(xl_heap_lock_updated *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, xlrec->target.node,
									 ItemPointerGetBlockNumber(&(xlrec->target.tid)));
						break;
					}

				default:
					fprintf(stderr, "unrecognized heap2 record type %d\n", info);
					exit(1);
			}
			break;

		case RM_BTREE_ID:
			switch (info)
			{
				case XLOG_BTREE_INSERT_LEAF:
				case XLOG_BTREE_INSERT_UPPER:
				case XLOG_BTREE_INSERT_META:
					{
						xl_btree_insert *xlrec =
							(xl_btree_insert *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, xlrec->target.node,
									 BlockIdGetBlockNumber(&xlrec->target.tid.ip_blkid));
						if (info == XLOG_BTREE_INSERT_META)
							pageinfo_add(MAIN_FORKNUM, xlrec->target.node,
										 BTREE_METAPAGE);
						break;
					}
				case XLOG_BTREE_SPLIT_L:
				case XLOG_BTREE_SPLIT_L_ROOT:
				case XLOG_BTREE_SPLIT_R:
				case XLOG_BTREE_SPLIT_R_ROOT:
					{
						xl_btree_split *xlrec =
							(xl_btree_split *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, xlrec->node, xlrec->rightsib);
						pageinfo_add(MAIN_FORKNUM, xlrec->node, xlrec->leftsib);
						if (xlrec->rnext != P_NONE)
							pageinfo_add(MAIN_FORKNUM, xlrec->node, xlrec->rnext);
						break;
					}
				case XLOG_BTREE_DELETE:
					{
						xl_btree_delete *xlrec =
							(xl_btree_delete *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, xlrec->node, xlrec->block);
						break;
					}
				case XLOG_BTREE_UNLINK_PAGE:
				case XLOG_BTREE_UNLINK_PAGE_META:
					{
						xl_btree_unlink_page *xlrec =
							(xl_btree_unlink_page *) XLogRecGetData(record);

						/* page to delete */
						pageinfo_add(MAIN_FORKNUM, xlrec->node,
									 xlrec->deadblk);
						/* left sib, if exists */
						if (xlrec->leftsib != P_NONE)
							pageinfo_add(MAIN_FORKNUM, xlrec->node,
										 xlrec->leftsib);
						/* rightsib */
						pageinfo_add(MAIN_FORKNUM, xlrec->node,
									 xlrec->rightsib);
						/* leaf page */
						pageinfo_add(MAIN_FORKNUM, xlrec->node,
									 xlrec->leafblk);
						/* leaf leftsib page, if exists */
						if (xlrec->leafleftsib != P_NONE)
							pageinfo_add(MAIN_FORKNUM, xlrec->node,
										 xlrec->leafleftsib);
						/* leaf rightsib page */
						pageinfo_add(MAIN_FORKNUM, xlrec->node,
									 xlrec->leafrightsib);
						/* remaining parent page */
						if (xlrec->topparent != InvalidBlockNumber)
							pageinfo_add(MAIN_FORKNUM, xlrec->node,
										 xlrec->topparent);
						/* metapage, if exists */
						if (info == XLOG_BTREE_UNLINK_PAGE_META)
							pageinfo_add(MAIN_FORKNUM, xlrec->node,
										 BTREE_METAPAGE);
						break;
					}
				case XLOG_BTREE_MARK_PAGE_HALFDEAD:
					{
						xl_btree_mark_page_halfdead *xlrec =
							(xl_btree_mark_page_halfdead *) XLogRecGetData(record);

						/* parent page */
						pageinfo_add(MAIN_FORKNUM, xlrec->target.node,
									 ItemPointerGetBlockNumber(&(xlrec->target.tid)));
						/* leaf page */
						pageinfo_add(MAIN_FORKNUM, xlrec->target.node,
									 xlrec->leafblk);
						/* leftsib page, if exists */
						if (xlrec->leftblk != P_NONE)
							pageinfo_add(MAIN_FORKNUM, xlrec->target.node,
										 xlrec->leftblk);
						/* rightsib page */
						pageinfo_add(MAIN_FORKNUM, xlrec->target.node,
									 xlrec->rightblk);
						/* remaining parent page */
						if (xlrec->topparent != InvalidBlockNumber)
							pageinfo_add(MAIN_FORKNUM, xlrec->target.node,
										 xlrec->topparent);
						break;
					}

				case XLOG_BTREE_NEWROOT:
					{
						xl_btree_newroot *xlrec =
							(xl_btree_newroot *) XLogRecGetData(record);

						/* FPW does not exists. */
						pageinfo_add(MAIN_FORKNUM, xlrec->node, xlrec->rootblk);
						pageinfo_add(MAIN_FORKNUM, xlrec->node, BTREE_METAPAGE);
						break;
					}


				case XLOG_BTREE_VACUUM:
					{
						xl_btree_vacuum *xlrec =
							(xl_btree_vacuum *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, xlrec->node, xlrec->block);
						break;
					}

				case XLOG_BTREE_REUSE_PAGE:
					/*
					 * This record type is only emitted for hot standby's
					 * benefit. It doesn't actually change anything.
					 */
					break;

				default:
					fprintf(stderr, "unrecognized btree record type %d\n", info);
					exit(1);
			}
			break;
		case RM_HASH_ID:
			/* Nothing to do because HASH does not support WAL recovery. */
			break;
		case RM_GIN_ID:
			switch (info)
			{
				case XLOG_GIN_CREATE_INDEX:
					{
						RelFileNode *node =
							(RelFileNode *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, *node, GIN_ROOT_BLKNO);
						break;
					}
				case XLOG_GIN_CREATE_PTREE:
					{
						ginxlogCreatePostingTree *data =
							(ginxlogCreatePostingTree *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, data->node, data->blkno);
						break;
					}
				case XLOG_GIN_INSERT:
					{
						ginxlogInsert *data =
							(ginxlogInsert *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, data->node, data->blkno);
						break;
					}
				case XLOG_GIN_SPLIT:
					{
						ginxlogSplit *data =
							(ginxlogSplit *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, data->node, data->lblkno);
						pageinfo_add(MAIN_FORKNUM, data->node, data->rblkno);
						if ((data->flags & GIN_SPLIT_ROOT) != 0)
							pageinfo_add(MAIN_FORKNUM, data->node, data->rrlink);
						break;
					}
				case XLOG_GIN_VACUUM_PAGE:
					{
						ginxlogVacuumPage *data =
							(ginxlogVacuumPage *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, data->node, data->blkno);
						break;
					}

				case XLOG_GIN_DELETE_PAGE:
					{
						ginxlogDeletePage *data =
							(ginxlogDeletePage *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, data->node, data->blkno);
						pageinfo_add(MAIN_FORKNUM, data->node, data->parentBlkno);
						if (data->leftBlkno != InvalidBlockNumber)
							pageinfo_add(MAIN_FORKNUM, data->node, data->leftBlkno);
						break;
					}

				case XLOG_GIN_UPDATE_META_PAGE:
					{
						ginxlogUpdateMeta *data =
							(ginxlogUpdateMeta *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, data->node, GIN_METAPAGE_BLKNO);
						/*
						 * The logic in xlogRedoUpdateMetapage is more
						 * complicated - not all these are updated every time -
						 * but better to keep it simple here and copy a few
						 * pages unnecessarily.
						 */
						pageinfo_add(MAIN_FORKNUM, data->node, data->metadata.head);
						pageinfo_add(MAIN_FORKNUM, data->node, data->metadata.tail);
						if (BlockNumberIsValid(data->prevTail))
							pageinfo_add(MAIN_FORKNUM, data->node, data->prevTail);

						break;
					}

				case XLOG_GIN_INSERT_LISTPAGE:
					{
						ginxlogInsertListPage *data =
							(ginxlogInsertListPage *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, data->node, data->blkno);
						if (BlockNumberIsValid(data->rightlink))
							pageinfo_add(MAIN_FORKNUM, data->node, data->rightlink);
						break;
					}

				case XLOG_GIN_DELETE_LISTPAGE:
					{
						ginxlogDeleteListPages *data =
							(ginxlogDeleteListPages *) XLogRecGetData(record);
						int		i;

						pageinfo_add(MAIN_FORKNUM, data->node, GIN_METAPAGE_BLKNO);
						for (i = 0; i < data->ndeleted; i++)
							pageinfo_add(MAIN_FORKNUM, data->node, data->toDelete[i]);
						break;
					}

				default:
					fprintf(stderr, "unrecognized GIN record type %d\n", info);
					exit(1);
			}
			break;

		case RM_GIST_ID:
			switch (info)
			{
				case XLOG_GIST_PAGE_UPDATE:
					{
						gistxlogPageUpdate *xlrec =
							(gistxlogPageUpdate *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, xlrec->node, xlrec->blkno);
						break;
					}
				case XLOG_GIST_PAGE_SPLIT:
					{
						/*
						 * See decodePageSplitRecord function in backend.
						 */
						char	   *begin = XLogRecGetData(record),
								   *ptr;
						int			j,
									i = 0;
						gistxlogPageSplit *recheader;

						recheader = (gistxlogPageSplit *) begin;
						ptr = begin + sizeof(gistxlogPageSplit);
						for (i = 0; i < recheader->npage; i++)
						{
							gistxlogPage *pageheader;
							Assert(ptr - begin < record->xl_len);
							pageheader = (gistxlogPage *) ptr;
							ptr += sizeof(gistxlogPage);

							pageinfo_add(MAIN_FORKNUM, recheader->node,
										 pageheader->blkno);

							/*
							 * skip over the tuples to find next gistxlogPage
							 * struct
							 */
							j = 0;
							while (j < pageheader->num)
							{
								Assert(ptr - begin < record->xl_len);
								ptr += IndexTupleSize((IndexTuple) ptr);
								j++;
							}
						}
						break;
					}
				case XLOG_GIST_CREATE_INDEX:
					{
						RelFileNode *node =
							(RelFileNode *) XLogRecGetData(record);

						pageinfo_add(MAIN_FORKNUM, *node, GIST_ROOT_BLKNO);
						break;
					}

				default:
					fprintf(stderr, "unrecognized GiST record type %d\n", info);
					exit(1);
			}
			break;

		case RM_SPGIST_ID:
			switch (info)
			{
				case XLOG_SPGIST_CREATE_INDEX:
					{
						RelFileNode *node = (RelFileNode *) XLogRecGetData(record);
						BlockNumber blk;
						for (blk = 0; blk <= SPGIST_LAST_FIXED_BLKNO; blk++)
							pageinfo_add(MAIN_FORKNUM, *node, blk);
						break;
					}

				case XLOG_SPGIST_ADD_LEAF:
					{
						spgxlogAddLeaf *xldata = (spgxlogAddLeaf *) XLogRecGetData(record);
						pageinfo_add(MAIN_FORKNUM, xldata->node, xldata->blknoLeaf);
						if (BlockNumberIsValid(xldata->blknoParent))
							pageinfo_add(MAIN_FORKNUM, xldata->node, xldata->blknoParent);
						break;
					}

				case XLOG_SPGIST_MOVE_LEAFS:
					{
						spgxlogMoveLeafs *xldata = (spgxlogMoveLeafs *) XLogRecGetData(record);
						pageinfo_add(MAIN_FORKNUM, xldata->node, xldata->blknoSrc);
						pageinfo_add(MAIN_FORKNUM, xldata->node, xldata->blknoDst);
						pageinfo_add(MAIN_FORKNUM, xldata->node, xldata->blknoParent);
					}
					break;

				case XLOG_SPGIST_ADD_NODE:
					{
						spgxlogAddNode *xldata = (spgxlogAddNode *) XLogRecGetData(record);
						pageinfo_add(MAIN_FORKNUM, xldata->node, xldata->blkno);
						if (BlockNumberIsValid(xldata->blknoParent))
							pageinfo_add(MAIN_FORKNUM, xldata->node, xldata->blknoParent);
						if (BlockNumberIsValid(xldata->blknoNew))
							pageinfo_add(MAIN_FORKNUM, xldata->node, xldata->blknoNew);
					}
					break;

				case XLOG_SPGIST_SPLIT_TUPLE:
					{
						spgxlogSplitTuple *xldata = (spgxlogSplitTuple *) XLogRecGetData(record);
						pageinfo_add(MAIN_FORKNUM, xldata->node, xldata->blknoPrefix);
						pageinfo_add(MAIN_FORKNUM, xldata->node, xldata->blknoPostfix);
					}
					break;

				case XLOG_SPGIST_PICKSPLIT:
					{
						spgxlogPickSplit *xldata = (spgxlogPickSplit *) XLogRecGetData(record);
						pageinfo_add(MAIN_FORKNUM, xldata->node, xldata->blknoSrc);
						pageinfo_add(MAIN_FORKNUM, xldata->node, xldata->blknoDest);
						pageinfo_add(MAIN_FORKNUM, xldata->node, xldata->blknoInner);
						if (BlockNumberIsValid(xldata->blknoParent))
							pageinfo_add(MAIN_FORKNUM, xldata->node, xldata->blknoParent);
					}
					break;

				case XLOG_SPGIST_VACUUM_LEAF:
					{
						spgxlogVacuumLeaf *xldata = (spgxlogVacuumLeaf *) XLogRecGetData(record);
						pageinfo_add(MAIN_FORKNUM, xldata->node, xldata->blkno);
					}
					break;

				case XLOG_SPGIST_VACUUM_ROOT:
					{
						spgxlogVacuumRoot *xldata = (spgxlogVacuumRoot *) XLogRecGetData(record);
						pageinfo_add(MAIN_FORKNUM, xldata->node, xldata->blkno);
					}
					break;

				case XLOG_SPGIST_VACUUM_REDIRECT:
					{
						spgxlogVacuumRedirect *xldata = (spgxlogVacuumRedirect *) XLogRecGetData(record);
						pageinfo_add(MAIN_FORKNUM, xldata->node, xldata->blkno);
					}
					break;

				default:
					fprintf(stderr, "unrecognized SP-GiST record type %d\n", info);
					exit(1);
			}
			break;

		case RM_SEQ_ID:
			switch (info)
			{
				case XLOG_SEQ_LOG:
				{
					xl_seq_rec *xlrec =
						(xl_seq_rec *) XLogRecGetData(record);

					pageinfo_add(MAIN_FORKNUM, xlrec->node, 0);
					break;
				}

				default:
					fprintf(stderr, "unrecognized sequence record type %d\n", info);
					exit(1);
			}
			break;

		case RM_SMGR_ID:
			switch (info)
			{
				case XLOG_SMGR_CREATE:
					/*
					 * We can safely ignore these. The local file will be
					 * removed, if it doesn't exist in remote system. If a
					 * file with same name is created in remote system, too,
					 * there will be WAL records for all the blocks in it.
					 */
					break;

				case XLOG_SMGR_TRUNCATE:
					/*
					 * We can safely ignore these. If a file is truncated
					 * locally, we'll notice that when we compare the sizes,
					 * and will copy the missing tail from remote system.
					 *
					 * TODO: But it would be nice to do some sanity
					 * cross-checking here..
					 */
					break;

				default:
					fprintf(stderr, "unrecognized smgr record type %d\n", info);
					exit(1);
			}
			break;


		case RM_DBASE_ID:
			switch (info)
			{
				case XLOG_DBASE_CREATE:
					/*
					 * New databases can be safely ignored. It won't be
					 * present in the remote system, so it will be copied
					 * in toto. There's one corner-case, though: if a new,
					 * different, database is also created in the remote
					 * system, we'll see that the files already exist and not
					 * copy them. That's OK, though; WAL replay of creating
					 * the new database, from the remote WAL, will re-copy
					 * the new database, overwriting the database created in
					 * the local system.
					 */
					break;

				case XLOG_DBASE_DROP:
					/*
					 * An existing database was dropped. We'll see that the
					 * files don't exist in local system, and copy them in
					 * toto from the remote system. No need to do anything
					 * special here.
					 */
					break;

				default:
					fprintf(stderr, "unrecognized dbase record type %d\n", info);
					exit(1);
			}
			break;

		case RM_TBLSPC_ID:
			switch (info)
			{
				case XLOG_TBLSPC_CREATE:
				case XLOG_TBLSPC_DROP:
					/*
					 * We don't need to do anything special with tablespaces
					 * here. If there are any files in them, they will be
					 * handled as usual.
					 */
					break;

				default:
					fprintf(stderr, "unrecognized dbase record type %d\n", info);
					exit(1);
			}
			break;

		case RM_APPEND_ONLY_ID:
			switch (info)
			{
				case XLOG_APPENDONLY_INSERT:
				{
					xl_ao_insert *insert_record = (xl_ao_insert *) XLogRecGetData(record);
					process_aofile_change(insert_record->target.node,
										  insert_record->target.segment_filenum,
										  insert_record->target.offset);
					break;
				}
				case XLOG_APPENDONLY_TRUNCATE:
				{
					/*
					 * We can safely ignore these. If a file is truncated
					 * locally, we'll notice that when we compare the sizes,
					 * and will copy the missing tail from remote system.
					 */
					break;
				}
			}
			break;

		case RM_BITMAP_ID:
			switch (info)
			{
				case XLOG_BITMAP_INSERT_LOVITEM:
					{
						xl_bm_lovitem *xlrec = (xl_bm_lovitem *) XLogRecGetData(record);
						pageinfo_add(xlrec->bm_fork, xlrec->bm_node, xlrec->bm_lov_blkno);
						break;
					}
				case XLOG_BITMAP_INSERT_META:
					{
						xl_bm_metapage *xlrec = (xl_bm_metapage *) XLogRecGetData(record);
						/* bitmap always uses block 0 as meta-page */
						pageinfo_add(xlrec->bm_fork, xlrec->bm_node, 0);
						break;
					}
				case XLOG_BITMAP_INSERT_BITMAP_LASTWORDS:
					{
						xl_bm_bitmap_lastwords *xlrec = (xl_bm_bitmap_lastwords *) XLogRecGetData(record);
						pageinfo_add(MAIN_FORKNUM, xlrec->bm_node, xlrec->bm_lov_blkno);
						break;
					}
				case XLOG_BITMAP_INSERT_WORDS:
					{
						xl_bm_bitmapwords *xlrec = (xl_bm_bitmapwords *) XLogRecGetData(record);
						pageinfo_add(MAIN_FORKNUM, xlrec->bm_node, xlrec->bm_lov_blkno);
						for (int bmpageno = 0; bmpageno < xlrec->bm_num_pages; bmpageno++)
						{
							xl_bm_bitmapwords_perpage *xlrec_perpage;
							if (bmpageno == 0 && !xlrec->bm_init_first_page && record->xl_info & XLR_BKP_BLOCK(1))
								continue;
							xlrec_perpage = (xl_bm_bitmapwords_perpage *) xlrec;
							pageinfo_add(MAIN_FORKNUM, xlrec->bm_node, xlrec_perpage->bmp_blkno);
						}
						break;
					}
				case XLOG_BITMAP_UPDATEWORD:
					{
						xl_bm_updateword *xlrec = (xl_bm_updateword *) XLogRecGetData(record);
						pageinfo_add(MAIN_FORKNUM, xlrec->bm_node, xlrec->bm_blkno);
						break;
					}
				case XLOG_BITMAP_UPDATEWORDS:
					{
						xl_bm_updatewords *xlrec = (xl_bm_updatewords*) XLogRecGetData(record);
						pageinfo_add(MAIN_FORKNUM, xlrec->bm_node, xlrec->bm_first_blkno);
						if (xlrec->bm_two_pages)
							pageinfo_add(MAIN_FORKNUM, xlrec->bm_node, xlrec->bm_second_blkno);
						if (xlrec->bm_new_lastpage)
							pageinfo_add(MAIN_FORKNUM, xlrec->bm_node, xlrec->bm_lov_blkno);
						break;
					}
				default:
					fprintf(stderr, "unrecognized bitmap index wal record type %d\n", info);
					exit(1);
			}
			break;
		default:
			/*
			 * It's important that we error out, not ignore, records that we
			 * don't recognize. They might change some data pages, and if we
			 * ignore them, we don't copy them over correctly.
			 */
			fprintf(stderr, "unrecognized resource manager id %d\n", record->xl_rmid);
			exit(1);
	}
}
