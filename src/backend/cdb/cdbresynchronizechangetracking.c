/*-------------------------------------------------------------------------
 *
 * cdbresynchronizechangetracking.c
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbresynchronizechangetracking.c
 *
 *-------------------------------------------------------------------------
 */
#include <fcntl.h>
#include <sys/stat.h>
#include "postgres.h"
#include "miscadmin.h"
#include "cdb/cdbutil.h"
#include <unistd.h>

#include "access/xact.h"
#include "access/xlog_internal.h"
#include "access/bitmap.h"
#include "access/htup.h"
#include "access/nbtree.h"
#include "catalog/pg_authid.h"
#include "commands/sequence.h"
#include "executor/spi.h"
#include "postmaster/primary_mirror_mode.h"
#include "postmaster/fts.h"
#include "storage/fd.h"
#include "storage/lock.h"
#include "storage/relfilenode.h"
#include "storage/shmem.h"
#include "utils/builtins.h"
#include "utils/faultinjector.h"
#include "utils/memutils.h"
#include "utils/palloc.h"
#include "utils/faultinjector.h"
#include "utils/pg_crc.h"
#include "utils/snapmgr.h"

#include "cdb/cdbfilerep.h"
#include "cdb/cdbfilerepresyncmanager.h"
#include "cdb/cdbresynchronizechangetracking.h"
#include "cdb/cdbpersistentrelation.h"
#include "cdb/cdbpersistentstore.h"

#define CHANGETRACKING_FILE_EOF INT64CONST(-1)

/*
 * Global Variables
 */
char	   *changeTrackingMainBuffer;	/* buffer for writing into main file
										 * (full) */
char	   *changeTrackingCompactingBuffer; /* buffer for using when
											 * compacting files   */
char	   *changeTrackingXlogDataBuffer;
char		metabuf[CHANGETRACKING_METABUFLEN];
ChangeTrackingBufStatusData *CTMainWriteBufStatus;	/* describes state of
													 * changeTrackingMainBuffer */
ChangeTrackingBufStatusData *CTCompactWriteBufStatus;	/* describes state of
														 * changeTrackingCompactingBuffer */
ChangeTrackingResyncMetaData *changeTrackingResyncMeta;
ChangeTrackingLogCompactingStateData *changeTrackingCompState;	/* state of data
																 * compacting in log
																 * files */

extern bool enable_groupagg;	/* from guc.h */

/*
 * Local functions
 */
static int	ChangeTracking_WriteBuffer(File file, CTFType ftype);
static void ChangeTracking_AddBufferPoolChange(CTFType ctype,
								   XLogRecPtr *xlogLocation,
								   RelFileNode *relFileNode,
								   BlockNumber blockNum,
								   ItemPointerData persistentTid,
								   int64 persistentSerialNum);

/* static IncrementalChangeList* ChangeTracking_InitIncrementalChangeList(int count); */
static ChangeTrackingResult *ChangeTracking_FormResult(int count);
static void ChangeTracking_AddResultEntry(ChangeTrackingResult *result,
							  Oid space,
							  Oid db,
							  Oid rel,
							  BlockNumber blocknum,
							  XLogRecPtr *lsn_end);
static void ChangeTracking_MarkFullResyncLockAcquired(void);
static void ChangeTracking_HandleWriteError(CTFType ft);
static void ChangeTracking_CreateTransientLogIfNeeded(void);
static void ChangeTracking_ResetBufStatus(ChangeTrackingBufStatusData *bufstat);
static void ChangeTracking_ResetCompactingStatus(ChangeTrackingLogCompactingStateData *compstat);


/*
 * Return the required shared-memory size for this module.
 */
extern Size
ChangeTrackingShmemSize(void)
{
	Size		size = 0;

	size = add_size(size, 2 * CHANGETRACKING_BLCKSZ);	/* two 32kB shmem
														 * buffers */
	size = add_size(size, CHANGETRACKING_XLOGDATASZ);
	size = add_size(size, 2 * sizeof(ChangeTrackingBufStatusData)); /* the 2 buffer status */
	size = add_size(size, sizeof(ChangeTrackingResyncMetaData));	/* the resync metadata */
	size = add_size(size, sizeof(ChangeTrackingLogCompactingStateData));

	return size;
}

/*
 * Initialize the shared-memory for this module.
 */
extern void
ChangeTrackingShmemInit(void)
{
	bool		foundBuffer1,
				foundBuffer2,
				foundStatus1,
				foundStatus2,
				foundMeta,
				foundXlogData,
				foundCompState;
	size_t		bufsize1 = CHANGETRACKING_BLCKSZ;
	size_t		bufsize2 = CHANGETRACKING_XLOGDATASZ;

	changeTrackingMainBuffer = (char *)
		ShmemInitStruct("Change Tracking Main (Full) Log Buffer",
						bufsize1,
						&foundBuffer1);

	changeTrackingCompactingBuffer = (char *)
		ShmemInitStruct("Change Tracking Log Buffer for compacting operations",
						bufsize1,
						&foundBuffer2);

	CTMainWriteBufStatus = (ChangeTrackingBufStatusData *)
		ShmemInitStruct("Change Tracking Full Log Buffer Status",
						sizeof(ChangeTrackingBufStatusData),
						&foundStatus1);

	CTCompactWriteBufStatus = (ChangeTrackingBufStatusData *)
		ShmemInitStruct("Change Tracking Compact Log Buffer Status",
						sizeof(ChangeTrackingBufStatusData),
						&foundStatus2);

	changeTrackingResyncMeta = (ChangeTrackingResyncMetaData *)
		ShmemInitStruct("Change Tracking Resync Meta Data",
						sizeof(ChangeTrackingResyncMetaData),
						&foundMeta);

	changeTrackingXlogDataBuffer = (char *)
		ShmemInitStruct("Change Tracking Xlog Data Buffer",
						bufsize2,
						&foundXlogData);

	changeTrackingCompState = (ChangeTrackingLogCompactingStateData *)
		ShmemInitStruct("Change Tracking Compacting state",
						sizeof(ChangeTrackingLogCompactingStateData),
						&foundCompState);

	/* See if we are already initialized */
	if (foundBuffer1 || foundBuffer2 || foundStatus1 ||
		foundStatus2 || foundMeta || foundXlogData || foundCompState)
	{
		/* all should be present or neither */
		Assert(foundBuffer1 && foundBuffer2 && foundBuffer1 &&
			   foundBuffer2 && foundMeta && foundXlogData && foundCompState);
		return;
	}

	/* init buffers */
	memset(changeTrackingMainBuffer, 0, bufsize1);
	memset(changeTrackingCompactingBuffer, 0, bufsize1);
	memset(changeTrackingXlogDataBuffer, 0, bufsize2);

	/* init buffer status */
	CTMainWriteBufStatus->maxbufsize = bufsize1;
	ChangeTracking_ResetBufStatus(CTMainWriteBufStatus);

	CTCompactWriteBufStatus->maxbufsize = bufsize1;
	ChangeTracking_ResetBufStatus(CTCompactWriteBufStatus);

	/* init meta data */
	changeTrackingResyncMeta->resync_mode_full = false;
	changeTrackingResyncMeta->resync_lsn_end.xlogid = 0;
	changeTrackingResyncMeta->resync_lsn_end.xrecoff = 0;
	changeTrackingResyncMeta->resync_transition_completed = false;
	changeTrackingResyncMeta->insync_transition_completed = false;

	ChangeTracking_ResetCompactingStatus(changeTrackingCompState);

	ereport(DEBUG1, (errmsg("initialized changetracking shared memory structures")));

	return;
}

/*
 * Reset shmem variables to zero.
 */
extern void
ChangeTrackingShmemReset(void)
{
	ChangeTracking_ResetBufStatus(CTMainWriteBufStatus);
	ChangeTracking_ResetCompactingStatus(changeTrackingCompState);

	return;
}

/*
 * This procedure will be called when mirror loss has been detected AND
 * the master has told the primary it is carrying on.
 * It scans the xlog starting from the most recent checkpoint and collects
 * all the interesting changes for the changelog. add them to the changelog.
 */
void
ChangeTracking_CreateInitialFromPreviousCheckpoint(
												   XLogRecPtr *lastChangeTrackingEndLoc)
{
	if (gp_change_tracking)
	{
		int			count = XLogAddRecordsToChangeTracking(lastChangeTrackingEndLoc);

		elog(LOG, "scanned through %d initial xlog records since last checkpoint "
			 "for writing into the resynchronize change log", count);
	}
	else
	{
		elog(WARNING, "Change logging is disabled. This should only occur after "
			 "a manual intervention of an administrator, and only with "
			 "guidance from greenplum support.");
	}
}


/*
 * Add a buffer pool change into the change log (changes go into any of the
 * change log files, depending on the ftype that is passed in).
 * xlogLocation - The XLOG LSN of the record that describes the page change.
 * relFileNode  - The tablespace, database, and relation OIDs for the changed relation.
 * blockNum     - the block that was changed.
 *
 */
static void
ChangeTracking_AddBufferPoolChange(CTFType ftype,
								   XLogRecPtr *xlogLocation,
								   RelFileNode *relFileNode,
								   BlockNumber blockNum,
								   ItemPointerData persistentTid,
								   int64 persistentSerialNum)
{
	ChangeTrackingRecord rec;
	ChangeTrackingBufStatusData *bufstat;
	char	   *buf;
	int			freespace = 0;

	/* gp_persistent relation change? we shouldn't log it. exit early */
	if (GpPersistent_SkipXLogInfo(relFileNode->relNode))
		return;

	Assert(ftype != CTF_META);
	Assert(ftype != CTF_LOG_TRANSIENT);

	if (ftype == CTF_LOG_FULL)
	{
		/* this is a regular write from xlog */
		bufstat = CTMainWriteBufStatus;
		buf = changeTrackingMainBuffer;
	}
	else
	{
		/* this is a write as a part of a compacting operation */
		bufstat = CTCompactWriteBufStatus;
		buf = changeTrackingCompactingBuffer;
	}


	/* populate a new change log record */
	rec.xlogLocation = *xlogLocation;
	rec.relFileNode = *relFileNode;
	rec.bufferPoolBlockNum = blockNum;
	rec.persistentTid = persistentTid;
	rec.persistentSerialNum = persistentSerialNum;

	LWLockAcquire(ChangeTrackingWriteLock, LW_EXCLUSIVE);

	if (bufstat->bufsize == 0)
		bufstat->bufsize = sizeof(ChangeTrackingPageHeader);	/* leave room for header
																 * (first time around
																 * only) */

	/* copy to our shared memory buffer */
	memcpy(buf + bufstat->bufsize, &rec, sizeof(rec));

	/* update state in shared memory */
	bufstat->recordcount++;
	bufstat->bufsize += sizeof(rec);

	/*
	 * check if buffer is full. if it is pad it with zeros, add a header and
	 * write it to the change log file. We don't flush it yet, it will be done
	 * during checkpoint.
	 */
	freespace = bufstat->maxbufsize - bufstat->bufsize;
	if (freespace < sizeof(ChangeTrackingRecord))
	{
		/*
		 * NOTE: We open the file, write it, and close it each time a buffer
		 * gets written. Why? The 'File' reference used to be kept in shmem
		 * but when the background writer comes in with a checkpoint the fd.c
		 * cache of the bgwriter process didn't know about this file. so, for
		 * now we keep it in the local fd.c cache and open and close each time
		 * until a better solution is found.
		 */
		File		file = ChangeTracking_OpenFile(ftype);

		if (ChangeTracking_WriteBuffer(file, ftype) < 0)
			ChangeTracking_HandleWriteError(ftype);
		ChangeTracking_CloseFile(file);
	}

	LWLockRelease(ChangeTrackingWriteLock);

	if (Debug_filerep_print)
	{
		elog(LOG,
			 "ChangeTracking_AddBufferPoolChange() write buffer status "
			 "record count '%u' buffer size '%u' max buffer size '%u' "
			 " next offset " INT64_FORMAT " file seg '%u' ",
			 bufstat->recordcount,
			 bufstat->bufsize,
			 bufstat->maxbufsize,
			 bufstat->nextwritepos,
			 bufstat->fileseg);
	}
}

/*
 * Given an rdata structure from XLogInsert() we extract
 * all that we care about from it for changetracking, and
 * return it in a buffer. The data is stored in a buffer
 * in a similar structure to how an xlog record data section
 * looks like.
 *
 * Normally the data is stored in a shared memory buffer
 * 'changeTrackingXlogDataBuffer'. however, there are special
 * cases (currently gist is the only one) which may require
 * a much larger buffer to store the data. In that case we
 * dynamically allocate a buffer and populate it, while
 * marking 'iscopy' to true to let the caller know they
 * need to pfree it themselves.
 */
char *
ChangeTracking_CopyRdataBuffers(XLogRecData *rdata,
								RmgrId rmid,
								uint8 info,
								bool *iscopy)
{
	XLogRecData *ptr = rdata;	/* don't want to change rdata, use another ptr */
	char	   *gist_data_buf = NULL;
	int			pos = 0;
	bool		gist_split_page = (rmid == RM_GIST_ID &&
								   (info & ~XLR_INFO_MASK) == XLOG_GIST_PAGE_SPLIT);

	while (ptr->data == NULL && ptr->next != NULL)
		ptr = ptr->next;

	if (ptr->data == NULL)
	{
		*iscopy = false;
		return NULL;
	}

	/* Copy the main (first) rdata data block */
	Assert(ptr->len <= CHANGETRACKING_XLOGDATASZ);
	memcpy(changeTrackingXlogDataBuffer, ptr->data, ptr->len);
	pos += ptr->len;

	/* ok, we're done! ... unless there's a special case to handle */

	/* special case: gist split has data we need in the next rdata blocks */
	if (gist_split_page)
	{
		XLogRecData *ptr_save_loc = ptr;
		int			gist_data_len = ptr->len;	/* previous data */

		/* pre-calculate buf size we will need */
		while (ptr->next != NULL)
		{
			ptr = ptr->next;

			if (ptr->data != NULL)
				gist_data_len += ptr->len;
		}

		/* allocate a buffer. copy all previously copied data */
		gist_data_buf = (char *) palloc(gist_data_len * sizeof(char));
		memcpy(gist_data_buf, changeTrackingXlogDataBuffer, pos);

		/* now copy the rest of the gist data */
		ptr = ptr_save_loc;
		while (ptr->next != NULL)
		{
			ptr = ptr->next;

			if (ptr->data != NULL)
			{
				memcpy(gist_data_buf + pos, ptr->data, ptr->len);
				pos += ptr->len;
			}
		}

		*iscopy = true;
		return gist_data_buf;
	}

	*iscopy = false;
	return changeTrackingXlogDataBuffer;
}


/*
 * When a new xlog record is created and we're in changetracking mode this
 * function gets called in order to create a changetracking record as well.
 * If the passed in xlog record is uninteresting to us, the function will
 * not log it and will return normally.
 *
 * We pass in the actual RM data *separately* from the XLogRecord. We normally
 * wouldn't need to do that, because the data follows the XLogRecord header,
 * however it turns out that XLogInsert() will break apart an xlog record if in
 * buffer boundaries and load some of it in the end of current buffer and the
 * rest, therefore leaving it no longer contigious in memory.
 */
void
ChangeTracking_AddRecordFromXlog(RmgrId rmid,
								 uint8 info,
								 void *data,
								 XLogRecPtr *loc)
{
	int			relationChangeInfoArrayCount;
	int			i;
	int			arrlen = ChangeTracking_GetInfoArrayDesiredMaxLength(rmid, info);
	RelationChangeInfo relationChangeInfoArray[arrlen];

	Assert(gp_change_tracking);

	ChangeTracking_GetRelationChangeInfoFromXlog(
												 rmid,
												 info,
												 data,
												 relationChangeInfoArray,
												 &relationChangeInfoArrayCount,
												 arrlen);

	for (i = 0; i < relationChangeInfoArrayCount; i++)
		ChangeTracking_AddBufferPoolChange(CTF_LOG_FULL,
										   loc,
										   &relationChangeInfoArray[i].relFileNode,
										   relationChangeInfoArray[i].blockNumber,
										   relationChangeInfoArray[i].persistentTid,
										   relationChangeInfoArray[i].persistentSerialNum);
}

bool
ChangeTracking_PrintRelationChangeInfo(
									   RmgrId xl_rmid,
									   uint8 xl_info,
									   void *data,
									   XLogRecPtr *loc,
									   bool weAreGeneratingXLogNow,
									   bool printSkipIssuesOnly)
{
	bool		atLeastOneSkipIssue = false;
	int			relationChangeInfoArrayCount;
	int			i;
	int			arrlen = ChangeTracking_GetInfoArrayDesiredMaxLength(xl_rmid, xl_info);
	RelationChangeInfo relationChangeInfoArray[arrlen];

	ChangeTracking_GetRelationChangeInfoFromXlog(
												 xl_rmid,
												 xl_info,
												 data,
												 relationChangeInfoArray,
												 &relationChangeInfoArrayCount,
												 arrlen);

	for (i = 0; i < relationChangeInfoArrayCount; i++)
	{
		RelationChangeInfo *relationChangeInfo;
		int64		maxPersistentSerialNum;
		bool		skip;
		bool		zeroTid = false;
		bool		invalidTid = false;
		bool		zeroSerialNum = false;
		bool		invalidSerialNum = false;
		bool		skipIssue = false;

		relationChangeInfo = &relationChangeInfoArray[i];

		if (weAreGeneratingXLogNow)
			maxPersistentSerialNum = PersistentRelation_MyHighestSerialNum();
		else
			maxPersistentSerialNum = PersistentRelation_CurrentMaxSerialNum();

		skip = GpPersistent_SkipXLogInfo(relationChangeInfo->relFileNode.relNode);
		if (!skip)
		{
			zeroTid = PersistentStore_IsZeroTid(&relationChangeInfo->persistentTid);
			if (!zeroTid)
				invalidTid = !ItemPointerIsValid(&relationChangeInfo->persistentTid);
			zeroSerialNum = (relationChangeInfo->persistentSerialNum == 0);
			if (!zeroSerialNum)
			{
				invalidSerialNum = (relationChangeInfo->persistentSerialNum < 0);

				/*
				 * If we have'nt done the scan yet... do not do upper range
				 * check.
				 */
				if (maxPersistentSerialNum != 0 &&
					relationChangeInfo->persistentSerialNum > maxPersistentSerialNum)
					invalidSerialNum = true;
			}
			skipIssue = (zeroTid || invalidTid || zeroSerialNum || invalidSerialNum);
		}

		if (!printSkipIssuesOnly || skipIssue)
			elog(LOG,
				 "ChangeTracking_PrintRelationChangeInfo: [%d] xl_rmid %d, xl_info 0x%X, %u/%u/%u, block number %u, LSN %s, persistent serial num " INT64_FORMAT ", TID %s, maxPersistentSerialNum " INT64_FORMAT ", skip %s, zeroTid %s, invalidTid %s, zeroSerialNum %s, invalidSerialNum %s, skipIssue %s",
				 i,
				 xl_rmid,
				 xl_info,
				 relationChangeInfo->relFileNode.spcNode,
				 relationChangeInfo->relFileNode.dbNode,
				 relationChangeInfo->relFileNode.relNode,
				 relationChangeInfo->blockNumber,
				 XLogLocationToString(loc),
				 relationChangeInfo->persistentSerialNum,
				 ItemPointerToString(&relationChangeInfo->persistentTid),
				 maxPersistentSerialNum,
				 (skip ? "true" : "false"),
				 (zeroTid ? "true" : "false"),
				 (invalidTid ? "true" : "false"),
				 (zeroSerialNum ? "true" : "false"),
				 (invalidSerialNum ? "true" : "false"),
				 (skipIssue ? "true" : "false"));

		if (skipIssue)
			atLeastOneSkipIssue = true;
	}

	return atLeastOneSkipIssue;
}


static void
ChangeTracking_AddRelationChangeInfo(
									 RelationChangeInfo *relationChangeInfoArray,
									 int *relationChangeInfoArrayCount,
									 int relationChangeInfoMaxSize,
									 RelFileNode *relFileNode,
									 BlockNumber blockNumber,
									 ItemPointer persistentTid,
									 int64 persistentSerialNum)
{
	RelationChangeInfo *relationChangeInfo;

	Assert(*relationChangeInfoArrayCount < relationChangeInfoMaxSize);

	relationChangeInfo = &relationChangeInfoArray[*relationChangeInfoArrayCount];

	relationChangeInfo->relFileNode = *relFileNode;
	relationChangeInfo->blockNumber = blockNumber;
	relationChangeInfo->persistentTid = *persistentTid;
	relationChangeInfo->persistentSerialNum = persistentSerialNum;

	(*relationChangeInfoArrayCount)++;
}

void
ChangeTracking_GetRelationChangeInfoFromXlog(
											 RmgrId xl_rmid,
											 uint8 xl_info,
											 void *data,
											 RelationChangeInfo *relationChangeInfoArray,
											 int *relationChangeInfoArrayCount,
											 int relationChangeInfoMaxSize)
{

	uint8		info = xl_info & ~XLR_INFO_MASK;
	uint8		op = 0;

	MemSet(relationChangeInfoArray, 0, sizeof(RelationChangeInfo) * relationChangeInfoMaxSize);
	*relationChangeInfoArrayCount = 0;

	/*
	 * Find the RM for this xlog record and see whether we are interested in
	 * logging it as a buffer pool change or not.
	 */
	switch (xl_rmid)
	{

			/*
			 * The following changes aren't interesting to the change log
			 */
		case RM_CLOG_ID:
		case RM_MULTIXACT_ID:
		case RM_XACT_ID:
		case RM_SMGR_ID:
		case RM_DISTRIBUTEDLOG_ID:
		case RM_DBASE_ID:
		case RM_TBLSPC_ID:
		case RM_MMXLOG_ID:

#ifdef USE_SEGWALREP
		case RM_APPEND_ONLY_ID:
#endif							/* USE_SEGWALREP */

			break;

			/*
			 * These aren't supported in GPDB
			 */
		case RM_HASH_ID:
			elog(ERROR, "internal error: unsupported RM ID (%d) in ChangeTracking_GetRelationChangeInfoFromXlog", xl_rmid);
			break;
		case RM_GIN_ID:

			/*
			 * keep LOG severity till crash recovery or GIN is implemented in
			 * order to avoid double failures during cdbfast
			 */
			elog(LOG, "internal error: unsupported RM ID (%d) in ChangeTracking_GetRelationChangeInfoFromXlog", xl_rmid);
			break;

			/*
			 * The following changes must be logged in the change log.
			 */
		case RM_XLOG_ID:
			if (info == XLOG_HINT)
			{
				BkpBlockWithPT bkpbwithpt;

				memcpy(&bkpbwithpt, data, sizeof(BkpBlockWithPT));
				ChangeTracking_AddRelationChangeInfo(
													 relationChangeInfoArray,
													 relationChangeInfoArrayCount,
													 relationChangeInfoMaxSize,
													 &(bkpbwithpt.bkpb.node),
													 bkpbwithpt.bkpb.block,
													 &bkpbwithpt.persistentTid,
													 bkpbwithpt.persistentSerialNum);
			}
			break;

		case RM_HEAP2_ID:
			op = info & XLOG_HEAP_OPMASK;
			switch (op)
			{
				case XLOG_HEAP2_FREEZE:
					{
						xl_heap_freeze *xlrec = (xl_heap_freeze *) data;

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->heapnode.node),
															 xlrec->block,
															 &xlrec->heapnode.persistentTid,
															 xlrec->heapnode.persistentSerialNum);
						break;
					}
				case XLOG_HEAP2_CLEAN:
				case XLOG_HEAP2_CLEAN_MOVE:
					{
						xl_heap_clean *xlrec = (xl_heap_clean *) data;

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->heapnode.node),
															 xlrec->block,
															 &xlrec->heapnode.persistentTid,
															 xlrec->heapnode.persistentSerialNum);
						break;
					}
				default:
					elog(ERROR, "internal error: unsupported RM_HEAP2_ID op (%u) in ChangeTracking_GetRelationChangeInfoFromXlog", info);
			}
			break;
		case RM_HEAP_ID:
			op = info & XLOG_HEAP_OPMASK;
			switch (op)
			{
				case XLOG_HEAP_INSERT:
					{
						xl_heap_insert *xlrec = (xl_heap_insert *) data;

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->target.node),
															 ItemPointerGetBlockNumber(&(xlrec->target.tid)),
															 &xlrec->target.persistentTid,
															 xlrec->target.persistentSerialNum);
						break;
					}
				case XLOG_HEAP_DELETE:
					{
						xl_heap_delete *xlrec = (xl_heap_delete *) data;

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->target.node),
															 ItemPointerGetBlockNumber(&(xlrec->target.tid)),
															 &xlrec->target.persistentTid,
															 xlrec->target.persistentSerialNum);
						break;
					}
				case XLOG_HEAP_HOT_UPDATE:
				case XLOG_HEAP_UPDATE:
				case XLOG_HEAP_MOVE:
					{
						xl_heap_update *xlrec = (xl_heap_update *) data;

						BlockNumber oldblock = ItemPointerGetBlockNumber(&(xlrec->target.tid));
						BlockNumber newblock = ItemPointerGetBlockNumber(&(xlrec->newtid));
						bool		samepage = (oldblock == newblock);


						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->target.node),
															 newblock,
															 &xlrec->target.persistentTid,
															 xlrec->target.persistentSerialNum);
						if (!samepage)
							ChangeTracking_AddRelationChangeInfo(
																 relationChangeInfoArray,
																 relationChangeInfoArrayCount,
																 relationChangeInfoMaxSize,
																 &(xlrec->target.node),
																 oldblock,
																 &xlrec->target.persistentTid,
																 xlrec->target.persistentSerialNum);

						break;
					}
				case XLOG_HEAP_NEWPAGE:
					{
						xl_heap_newpage *xlrec = (xl_heap_newpage *) data;

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->heapnode.node),
															 xlrec->blkno,
															 &xlrec->heapnode.persistentTid,
															 xlrec->heapnode.persistentSerialNum);
						break;
					}
				case XLOG_HEAP_LOCK:
					{
						xl_heap_lock *xlrec = (xl_heap_lock *) data;
						BlockNumber block = ItemPointerGetBlockNumber(&(xlrec->target.tid));

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->target.node),
															 block,
															 &xlrec->target.persistentTid,
															 xlrec->target.persistentSerialNum);
						break;
					}
				case XLOG_HEAP_INPLACE:
					{
						xl_heap_inplace *xlrec = (xl_heap_inplace *) data;
						BlockNumber block = ItemPointerGetBlockNumber(&(xlrec->target.tid));

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->target.node),
															 block,
															 &xlrec->target.persistentTid,
															 xlrec->target.persistentSerialNum);
						break;
					}

				default:
					elog(ERROR, "internal error: unsupported RM_HEAP_ID op (%u) in ChangeTracking_GetRelationChangeInfoFromXlog", op);
			}
			break;

		case RM_BTREE_ID:
			switch (info)
			{
				case XLOG_BTREE_INSERT_LEAF:
				case XLOG_BTREE_INSERT_UPPER:
				case XLOG_BTREE_INSERT_META:
					{
						xl_btree_insert *xlrec = (xl_btree_insert *) data;
						BlockIdData blkid = xlrec->target.tid.ip_blkid;

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->target.node),
															 BlockIdGetBlockNumber(&blkid),
															 &xlrec->target.persistentTid,
															 xlrec->target.persistentSerialNum);

						if (info == XLOG_BTREE_INSERT_META)
							ChangeTracking_AddRelationChangeInfo(
																 relationChangeInfoArray,
																 relationChangeInfoArrayCount,
																 relationChangeInfoMaxSize,
																 &(xlrec->target.node),
																 BTREE_METAPAGE,
																 &xlrec->target.persistentTid,
																 xlrec->target.persistentSerialNum);

						break;
					}
				case XLOG_BTREE_SPLIT_L:
				case XLOG_BTREE_SPLIT_L_ROOT:
				case XLOG_BTREE_SPLIT_R:
				case XLOG_BTREE_SPLIT_R_ROOT:
					{
						xl_btree_split *xlrec = (xl_btree_split *) data;

						/* orig page / new left page */
						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize, &(xlrec->node),
															 xlrec->leftsib,
															 &xlrec->persistentTid,
															 xlrec->persistentSerialNum);

						/* new right page */
						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize, &(xlrec->node),
															 xlrec->rightsib,
															 &xlrec->persistentTid,
															 xlrec->persistentSerialNum);

						/* next block (orig page's rightlink) */
						if (xlrec->rnext != P_NONE)
						{
							ChangeTracking_AddRelationChangeInfo(
																 relationChangeInfoArray,
																 relationChangeInfoArrayCount,
																 relationChangeInfoMaxSize, &(xlrec->node),
																 xlrec->rnext,
																 &xlrec->persistentTid,
																 xlrec->persistentSerialNum);
						}
						break;
					}
				case XLOG_BTREE_DELETE:
					{
						xl_btree_delete *xlrec = (xl_btree_delete *) data;

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->btreenode.node),
															 xlrec->block,
															 &xlrec->btreenode.persistentTid,
															 xlrec->btreenode.persistentSerialNum);
						break;
					}
				case XLOG_BTREE_DELETE_PAGE:
				case XLOG_BTREE_DELETE_PAGE_HALF:
				case XLOG_BTREE_DELETE_PAGE_META:
					{
						xl_btree_delete_page *xlrec = (xl_btree_delete_page *) data;
						BlockIdData blkid = xlrec->target.tid.ip_blkid;
						BlockNumber block = BlockIdGetBlockNumber(&blkid);

						if (block != P_NONE)
							ChangeTracking_AddRelationChangeInfo(
																 relationChangeInfoArray,
																 relationChangeInfoArrayCount,
																 relationChangeInfoMaxSize,
																 &(xlrec->target.node),
																 block,
																 &xlrec->target.persistentTid,
																 xlrec->target.persistentSerialNum);

						if (xlrec->rightblk != P_NONE)
							ChangeTracking_AddRelationChangeInfo(
																 relationChangeInfoArray,
																 relationChangeInfoArrayCount,
																 relationChangeInfoMaxSize,
																 &(xlrec->target.node),
																 xlrec->rightblk,
																 &xlrec->target.persistentTid,
																 xlrec->target.persistentSerialNum);

						if (xlrec->leftblk != P_NONE)
							ChangeTracking_AddRelationChangeInfo(
																 relationChangeInfoArray,
																 relationChangeInfoArrayCount,
																 relationChangeInfoMaxSize,
																 &(xlrec->target.node),
																 xlrec->leftblk,
																 &xlrec->target.persistentTid,
																 xlrec->target.persistentSerialNum);

						if (xlrec->deadblk != P_NONE)
							ChangeTracking_AddRelationChangeInfo(
																 relationChangeInfoArray,
																 relationChangeInfoArrayCount,
																 relationChangeInfoMaxSize,
																 &(xlrec->target.node),
																 xlrec->deadblk,
																 &xlrec->target.persistentTid,
																 xlrec->target.persistentSerialNum);

						if (info == XLOG_BTREE_DELETE_PAGE_META)
							ChangeTracking_AddRelationChangeInfo(
																 relationChangeInfoArray,
																 relationChangeInfoArrayCount,
																 relationChangeInfoMaxSize,
																 &(xlrec->target.node),
																 BTREE_METAPAGE,
																 &xlrec->target.persistentTid,
																 xlrec->target.persistentSerialNum);
						break;
					}
				case XLOG_BTREE_NEWROOT:
					{
						xl_btree_newroot *xlrec = (xl_btree_newroot *) data;

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->btreenode.node),
															 xlrec->rootblk,
															 &xlrec->btreenode.persistentTid,
															 xlrec->btreenode.persistentSerialNum);

						/* newroot always updates the meta page */
						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->btreenode.node),
															 BTREE_METAPAGE,
															 &xlrec->btreenode.persistentTid,
															 xlrec->btreenode.persistentSerialNum);

						break;
					}

				default:
					elog(ERROR, "internal error: unsupported RM_BTREE_ID op (%u) in ChangeTracking_GetRelationChangeInfoFromXlog", info);
			}
			break;
		case RM_BITMAP_ID:
			switch (info)
			{
				case XLOG_BITMAP_INSERT_NEWLOV:
					{
						xl_bm_newpage *xlrec = (xl_bm_newpage *) data;

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->bm_node),
															 xlrec->bm_new_blkno,
															 &xlrec->bm_persistentTid,
															 xlrec->bm_persistentSerialNum);
						break;
					}
				case XLOG_BITMAP_INSERT_LOVITEM:
					{
						xl_bm_lovitem *xlrec = (xl_bm_lovitem *) data;

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->bm_node),
															 xlrec->bm_lov_blkno,
															 &xlrec->bm_persistentTid,
															 xlrec->bm_persistentSerialNum);

						if (xlrec->bm_is_new_lov_blkno)
							ChangeTracking_AddRelationChangeInfo(
																 relationChangeInfoArray,
																 relationChangeInfoArrayCount,
																 relationChangeInfoMaxSize,
																 &(xlrec->bm_node),
																 BM_METAPAGE,
																 &xlrec->bm_persistentTid,
																 xlrec->bm_persistentSerialNum);
						break;
					}
				case XLOG_BITMAP_INSERT_META:
					{
						xl_bm_metapage *xlrec = (xl_bm_metapage *) data;

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->bm_node),
															 BM_METAPAGE,
															 &xlrec->bm_persistentTid,
															 xlrec->bm_persistentSerialNum);
						break;
					}
				case XLOG_BITMAP_INSERT_BITMAP_LASTWORDS:
					{
						xl_bm_bitmap_lastwords *xlrec = (xl_bm_bitmap_lastwords *) data;

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->bm_node),
															 xlrec->bm_lov_blkno,
															 &xlrec->bm_persistentTid,
															 xlrec->bm_persistentSerialNum);
						break;
					}
				case XLOG_BITMAP_INSERT_WORDS:
					{
						xl_bm_bitmapwords *xlrec = (xl_bm_bitmapwords *) data;

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->bm_node),
															 xlrec->bm_lov_blkno,
															 &xlrec->bm_persistentTid,
															 xlrec->bm_persistentSerialNum);

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->bm_node),
															 xlrec->bm_blkno,
															 &xlrec->bm_persistentTid,
															 xlrec->bm_persistentSerialNum);

						if (!xlrec->bm_is_last)
							ChangeTracking_AddRelationChangeInfo(
																 relationChangeInfoArray,
																 relationChangeInfoArrayCount,
																 relationChangeInfoMaxSize,
																 &(xlrec->bm_node),
																 xlrec->bm_next_blkno,
																 &xlrec->bm_persistentTid,
																 xlrec->bm_persistentSerialNum);
						break;
					}
				case XLOG_BITMAP_UPDATEWORD:
					{
						xl_bm_updateword *xlrec = (xl_bm_updateword *) data;

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->bm_node),
															 xlrec->bm_blkno,
															 &xlrec->bm_persistentTid,
															 xlrec->bm_persistentSerialNum);
						break;
					}
				case XLOG_BITMAP_UPDATEWORDS:
					{
						xl_bm_updatewords *xlrec = (xl_bm_updatewords *) data;

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->bm_node),
															 xlrec->bm_first_blkno,
															 &xlrec->bm_persistentTid,
															 xlrec->bm_persistentSerialNum);

						if (xlrec->bm_two_pages)
							ChangeTracking_AddRelationChangeInfo(
																 relationChangeInfoArray,
																 relationChangeInfoArrayCount,
																 relationChangeInfoMaxSize,
																 &(xlrec->bm_node),
																 xlrec->bm_second_blkno,
																 &xlrec->bm_persistentTid,
																 xlrec->bm_persistentSerialNum);

						if (xlrec->bm_new_lastpage)
							ChangeTracking_AddRelationChangeInfo(
																 relationChangeInfoArray,
																 relationChangeInfoArrayCount,
																 relationChangeInfoMaxSize,
																 &(xlrec->bm_node),
																 xlrec->bm_lov_blkno,
																 &xlrec->bm_persistentTid,
																 xlrec->bm_persistentSerialNum);

						break;
					}
				default:
					elog(ERROR, "internal error: unsupported RM_BITMAP_ID op (%u) in ChangeTracking_GetRelationChangeInfoFromXlog", info);
			}
			break;
		case RM_SEQ_ID:
			switch (info)
			{
				case XLOG_SEQ_LOG:
					{
						xl_seq_rec *xlrec = (xl_seq_rec *) data;

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xlrec->node),
															 0, /* seq_redo touches
																 * block 0 only */
															 &xlrec->persistentTid,
															 xlrec->persistentSerialNum);

						break;
					}
				default:
					elog(ERROR, "internal error: unsupported RM_SEQ_ID op (%u) in ChangeTracking_GetRelationChangeInfoFromXlog", info);
			}
			break;

		case RM_GIST_ID:
			switch (info)
			{
				case XLOG_GIST_PAGE_UPDATE:
				case XLOG_GIST_NEW_ROOT:
					{
						gistxlogPageUpdate *xldata = (gistxlogPageUpdate *) data;

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xldata->node),
															 xldata->blkno,
															 &xldata->persistentTid,
															 xldata->persistentSerialNum);
						break;
					}
				case XLOG_GIST_PAGE_DELETE:
					{
						gistxlogPageDelete *xldata = (gistxlogPageDelete *) data;

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xldata->node),
															 xldata->blkno,
															 &xldata->persistentTid,
															 xldata->persistentSerialNum);
						break;
					}
				case XLOG_GIST_PAGE_SPLIT:
					{
						gistxlogPageSplit *xldata = (gistxlogPageSplit *) data;
						char	   *ptr;
						int			j,
									i = 0;

						/* first, log the splitted page */
						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xldata->node),
															 xldata->origblkno,
															 &xldata->persistentTid,
															 xldata->persistentSerialNum);

						/* now log all the pages that we split into */
						ptr = (char *) data + sizeof(gistxlogPageSplit);

						for (i = 0; i < xldata->npage; i++)
						{
							gistxlogPage *gistp;

							gistp = (gistxlogPage *) ptr;
							ptr += sizeof(gistxlogPage);

							/*
							 * elog(LOG, "CHANGETRACKING GIST SPLIT: block
							 * [%d/%d]:%d", i+1,xldata->npage, gistp->blkno);
							 */
							ChangeTracking_AddRelationChangeInfo(
																 relationChangeInfoArray,
																 relationChangeInfoArrayCount,
																 relationChangeInfoMaxSize,
																 &(xldata->node),
																 gistp->blkno,
																 &xldata->persistentTid,
																 xldata->persistentSerialNum);

							/*
							 * skip over all index tuples. we only care about
							 * block numbers
							 */
							j = 0;
							while (j < gistp->num)
							{
								ptr += IndexTupleSize((IndexTuple) ptr);
								j++;
							}
						}

						break;
					}
				case XLOG_GIST_CREATE_INDEX:
					{
						gistxlogCreateIndex *xldata = (gistxlogCreateIndex *) data;

						ChangeTracking_AddRelationChangeInfo(
															 relationChangeInfoArray,
															 relationChangeInfoArrayCount,
															 relationChangeInfoMaxSize,
															 &(xldata->node),
															 GIST_ROOT_BLKNO,
															 &xldata->persistentTid,
															 xldata->persistentSerialNum);
						break;
					}
				case XLOG_GIST_INSERT_COMPLETE:
					{
						/* nothing to be done here */
						break;
					}
				default:
					elog(ERROR, "internal error: unsupported RM_GIST_ID op (%u) in ChangeTracking_GetRelationChangeInfoFromXlog", info);
			}

			break;
		default:
			elog(ERROR, "internal error: unsupported resource manager type (%d) in ChangeTracking_GetRelationChangeInfoFromXlog", xl_rmid);
	}

}


void
ChangeTracking_FsyncDataIntoLog(CTFType ftype)
{
	File		file;
	ChangeTrackingBufStatusData *bufstat;

	Assert(ftype != CTF_META);
	Assert(ftype != CTF_LOG_TRANSIENT);

	if (ftype == CTF_LOG_FULL)
		bufstat = CTMainWriteBufStatus;
	else
		bufstat = CTCompactWriteBufStatus;

	LWLockAcquire(ChangeTrackingWriteLock, LW_EXCLUSIVE);

	file = ChangeTracking_OpenFile(ftype);

	/* write the existing (non-full) shmem buffer */
	if (bufstat->recordcount > 0)
	{
		if (ChangeTracking_WriteBuffer(file, ftype) < 0)
			ChangeTracking_HandleWriteError(ftype);
	}

#ifdef FAULT_INJECTOR
	if (FaultInjector_InjectFaultIfSet(
									   ChangeTrackingDisable,
									   DDLNotSpecified,
									   "" /* databaseName */ ,
									   "" /* tableName */ ) == FaultInjectorTypeSkip)
		ChangeTracking_HandleWriteError(ftype);
	/* disable Change Tracking */
#endif

	errno = 0;


	/* time to fsync change log to disk */
	if (FileSync(file) < 0)
		ChangeTracking_HandleWriteError(ftype);

	ChangeTracking_CloseFile(file);
	LWLockRelease(ChangeTrackingWriteLock);
}

/*
 * CheckPointChangeTracking
 *
 * When the system wide checkpoint is performed, this function is called.
 * We write and fsync the current page we have (partially full) into disk.
 * Also, we save this partially full buffer in shared memory, so that after
 * this checkpoint we will continue filling it up, and when it is full use
 * it to overwrite this partial page with a full one.
 *
 * In addition, we check if this full log file is ready to be compacted.
 * if it is, we create a transient log file out of it for the filerep
 * process that will do the compacting.
 */
void
CheckPointChangeTracking(void)
{

	/* do nothing if not in proper operating mode */
	if (!ChangeTracking_ShouldTrackChanges())
		return;

	/* force data into disk */
	ChangeTracking_FsyncDataIntoLog(CTF_LOG_FULL);

	ChangeTracking_CreateTransientLogIfNeeded();
}

/************************************************************************
 * API for getting incremental change entries for gp_persistent relation
 ************************************************************************/

IncrementalChangeList *
ChangeTracking_InitIncrementalChangeList(int count)
{
	IncrementalChangeList *result;

	result = (IncrementalChangeList *) palloc0(sizeof(IncrementalChangeList));
	result->count = count;
	result->entries = (IncrementalChangeEntry *) palloc(sizeof(IncrementalChangeEntry) * count);

	return result;
}

/*
 * Get an ordered list of [persistent_tid,persistent_serialnum, numblocks]
 * from the change tracking log, with unique tid values, each paired with
 * the newest serial num found for it, and number of blocks changed for that
 * serial number.
 */
IncrementalChangeList *
ChangeTracking_GetIncrementalChangeList(void)
{
	IncrementalChangeList *result = NULL;
	StringInfoData sqlstmt;
	int			ret;
	int			proc;
	volatile bool connected = false;
	ResourceOwner save = CurrentResourceOwner;
	MemoryContext oldcontext = CurrentMemoryContext;
	CTFType		ftype = CTF_LOG_COMPACT;	/* always read from the compact
											 * file only */

	Assert(dataState == DataStateInResync);
	Assert(gp_change_tracking);

	/* assemble our query string */
	initStringInfo(&sqlstmt);

	/* TODO: there must be a more efficient query to use here */
	appendStringInfo(&sqlstmt, "SELECT t1.persistent_tid, t1.persistent_sn, t1.numblocks "
					 "FROM (SELECT persistent_tid, persistent_sn, count(distinct blocknum) as numblocks "
					 "      FROM gp_changetracking_log(%d) "
					 "      GROUP BY persistent_tid, persistent_sn) as t1, "
					 "     (SELECT persistent_tid, max(persistent_sn) as persistent_sn "
					 "	   FROM gp_changetracking_log(%d) "
					 "      GROUP BY persistent_tid) as t2 "
					 "WHERE t1.persistent_tid = t2.persistent_tid "
					 "AND   t1.persistent_sn = t2.persistent_sn",
					 ftype, ftype);

	/*
	 * NOTE: here's a cleaner version of the same query. compare which runs
	 * more efficiently. minimal testing shows it's the one above, but by
	 * small margin
	 */
/* 	appendStringInfo(&sqlstmt,  "SELECT persistent_tid, persistent_sn, count(distinct blocknum) " */
/* 								"FROM gp_changetracking_log(%d) " */
/* 								"GROUP BY persistent_tid, persistent_sn " */
/* 								"HAVING (persistent_tid, persistent_sn) " */
/* 								"IN (SELECT persistent_tid, max(persistent_sn) " */
/* 									"FROM gp_changetracking_log(%d) " */
/* 									"GROUP BY persistent_tid", ftype, ftype); */

	PG_TRY();
	{

		/* must be in a transaction in order to use SPI */
		StartTransactionCommand();
		PushActiveSnapshot(GetTransactionSnapshot());
		SetSessionUserId(BOOTSTRAP_SUPERUSERID, true);

		if (SPI_OK_CONNECT != SPI_connect())
		{
			ereport(ERROR, (errcode(ERRCODE_CDB_INTERNAL_ERROR),
							errmsg("Unable to obtain change tracking information from segment database."),
							errdetail("SPI_connect failed in ChangeTracking_GetIncrementalChangeList()")));
		}
		connected = true;

		/* Do the query. */
		ret = SPI_execute(sqlstmt.data, true, 0);
		proc = SPI_processed;


		if (ret > 0 && SPI_tuptable != NULL)
		{
			TupleDesc	tupdesc = SPI_tuptable->tupdesc;
			SPITupleTable *tuptable = SPI_tuptable;
			MemoryContext cxt_save;
			int			i;

			/*
			 * Iterate through each result tuple
			 */
			for (i = 0; i < proc; i++)
			{
				HeapTuple	tuple = tuptable->vals[i];

				IncrementalChangeEntry *entry;
				ItemPointer persistentTid;
				int64		persistentSerialNum;
				int64		numblocks;
				char	   *str_tid;
				char	   *str_sn;
				char	   *str_numb;

				/* get result columns from SPI (as strings) */
				str_tid = SPI_getvalue(tuple, tupdesc, 1);
				str_sn = SPI_getvalue(tuple, tupdesc, 2);
				str_numb = SPI_getvalue(tuple, tupdesc, 3);

				/*
				 * elog(LOG,"tuple %d: tid %s sn %s numb %s", i, str_tid,
				 * str_sn, str_numb);
				 */

				/* use our own context so that SPI won't free our stuff later */
				cxt_save = MemoryContextSwitchTo(oldcontext);

				/* init the result memory on first pass */
				if (i == 0)
					result = ChangeTracking_InitIncrementalChangeList(proc);

				/* convert to desired data type */
				persistentTid = (ItemPointer) DatumGetPointer(
															  DirectFunctionCall1(tidin, CStringGetDatum(str_tid)));
				persistentSerialNum = DatumGetInt64(DirectFunctionCall1(int8in, CStringGetDatum(str_sn)));
				numblocks = DatumGetInt64(DirectFunctionCall1(int8in, CStringGetDatum(str_numb)));

				/* populate this entry */
				entry = &(result->entries[i]);
				entry->persistentTid = *persistentTid;
				entry->persistentSerialNum = persistentSerialNum;
				entry->numblocks = numblocks;

				MemoryContextSwitchTo(cxt_save);
			}
		}
		else
		{
			/* no results for this given request */
			result = NULL;
		}

		connected = false;
		SPI_finish();

		PopActiveSnapshot();
		CommitTransactionCommand();
	}

	/* Clean up in case of error. */
	PG_CATCH();
	{
		if (connected)
			SPI_finish();

		PopActiveSnapshot();
		AbortCurrentTransaction();

		/* Carry on with error handling. */
		PG_RE_THROW();
	}
	PG_END_TRY();

	pfree(sqlstmt.data);

	MemoryContextSwitchTo(oldcontext);
	CurrentResourceOwner = save;

	return result;
}

void
ChangeTracking_FreeIncrementalChangeList(IncrementalChangeList *iclist)
{
	Assert(iclist);

	pfree(iclist->entries);
	pfree(iclist);
}

/*************************************
 * API for the synchronizer process
 *************************************/

/*
 * Allocate memory for a request that includes information about
 * the objects of interest.
 */
ChangeTrackingRequest *
ChangeTracking_FormRequest(int max_count)
{
	ChangeTrackingRequest *request;

	request = (ChangeTrackingRequest *) palloc0(sizeof(ChangeTrackingRequest));
	request->count = 0;
	request->max_count = max_count;
	request->entries = (ChangeTrackingRequestEntry *) palloc(sizeof(ChangeTrackingRequestEntry) * max_count);

	return request;
}

/*
 * Add an entry to CT request.  An entry is a relfilenode of a relation whose
 * changed blocks recorded in CT log need to be resynchronized with mirror.
 * The block number last_fetched indicates the last block number of this
 * relation fetched and sent to mirror by a resync worker.
 */
void
ChangeTracking_AddRequestEntry(ChangeTrackingRequest *request,
							   RelFileNode relFileNode)
{
	ChangeTrackingRequestEntry *entry;

	if (request->count + 1 > request->max_count)
		elog(ERROR, "ChangeTracking: trying to add more request entries than originally requested");

	entry = &(request->entries[request->count]);
	entry->relFileNode.relNode = relFileNode.relNode;
	entry->relFileNode.spcNode = relFileNode.spcNode;
	entry->relFileNode.dbNode = relFileNode.dbNode;
	entry->last_fetched = 0;
	request->count++;
}

/*
 * Deallocate memory associated with the request.
 */
void
ChangeTracking_FreeRequest(ChangeTrackingRequest *request)
{
	pfree(request->entries);
	pfree(request);
}

/*
 * Allocate memory for assembling the result. This will be used internally
 * by the resync changetracking after the synchronizer requested information,
 * the result of the request will be stored here and passed back to the caller.
 */
static ChangeTrackingResult *
ChangeTracking_FormResult(int max_count)
{
	ChangeTrackingResult *result;

	result = (ChangeTrackingResult *) palloc0(sizeof(ChangeTrackingResult));
	result->count = 0;
	result->max_count = max_count;
	result->ask_for_more = false;
	result->entries = (ChangeTrackingResultEntry *) palloc(sizeof(ChangeTrackingResultEntry) * max_count);

	return result;
}

static void
ChangeTracking_AddResultEntry(ChangeTrackingResult *result,
							  Oid space,
							  Oid db,
							  Oid rel,
							  BlockNumber blocknum,
							  XLogRecPtr *lsn_end)
{
	ChangeTrackingResultEntry *entry;

	if (result->count + 1 > result->max_count)
		elog(ERROR, "ChangeTracking: trying to add more result entries than originally requested");

	entry = &(result->entries[result->count]);
	entry->relFileNode.spcNode = space;
	entry->relFileNode.dbNode = db;
	entry->relFileNode.relNode = rel;
	entry->block_num = blocknum;
	entry->lsn_end = *lsn_end;

	result->count++;
}

/*
 * We are in resync mode and the synchronizer module is asking
 * us for the information we have gathered.
 *
 * The synchronizer passes in a list of relfilenodes, each with
 * a start and end LSN. For each of those relations that are
 * found in the change tracking log file this routine will return
 * the list of block numbers and the end LSN of each.
 *
 * We restrict the total number of changes that this routine returns to
 * gp_filerep_ct_batch_size, in order to not overflow memory.  If a specific
 * relation is expected to have more than this number of changes, this routine
 * will return the first gp_filerep_ct_batch_size change, along with setting
 * the 'ask_for_more' flag in the result to indicate to the caller that a
 * request with the same relation should be issued when ready.  When this
 * happens the caller should set last_fetched in the relation's request to the
 * highest block number seen so far.
 */
ChangeTrackingResult *
ChangeTracking_GetChanges(ChangeTrackingRequest *request)
{
	ChangeTrackingResult *result = NULL;
	StringInfoData sqlstmt;
	int			ret;
	int			proc;
	int			i;
	volatile bool connected = false;	/* needs to survive PG_TRY()/CATCH() */
	ResourceOwner save = CurrentResourceOwner;
	MemoryContext oldcontext = CurrentMemoryContext;
	CTFType		ftype = CTF_LOG_COMPACT;	/* always read from the compact
											 * log only */

	Assert(dataState == DataStateInResync);
	Assert(gp_change_tracking);

	/* assemble our query string */
	initStringInfo(&sqlstmt);
	appendStringInfo(&sqlstmt, "SELECT space, db, rel, blocknum, max(xlogloc) "
					 "FROM gp_changetracking_log(%d) "
					 "WHERE ", ftype);

	for (i = 0; i < request->count; i++)
	{
		Oid			space = request->entries[i].relFileNode.spcNode;
		Oid			db = request->entries[i].relFileNode.dbNode;
		Oid			rel = request->entries[i].relFileNode.relNode;
		BlockNumber last_fetched = request->entries[i].last_fetched;

		if (i != 0)
			appendStringInfo(&sqlstmt, "OR ");

		appendStringInfo(&sqlstmt, "(space = %u AND "
						 "db = %u AND "
						 "rel = %u",
						 space, db, rel);
		if (last_fetched > 0)
			appendStringInfo(&sqlstmt, " AND blocknum > %u) ", last_fetched);
		else
			appendStringInfo(&sqlstmt, ") ");

	}
	appendStringInfo(&sqlstmt, "GROUP BY space, db, rel, blocknum "
					 "ORDER BY space, db, rel, blocknum ");

	/*
	 * We limit the result to our max value so that we don't use too much
	 * memory both in the query result and in the actual result returned to
	 * the caller.
	 *
	 * The +1 is there in order to "peek" if there's more data to be returned.
	 * Therefore if MAX+1 records were returned from the query we return MAX
	 * records to the caller and indicate that there are more records to
	 * return in the next call with the same request (we don't return the last
	 * record found. we'll return it next time).
	 */
	appendStringInfo(&sqlstmt, "LIMIT %d", gp_filerep_ct_batch_size + 1);
	bool		old_enable_groupagg = enable_groupagg;

	enable_groupagg = false;	/* disable sort group agg -- our query works
								 * better with hash agg */

	PG_TRY();
	{
		/* must be in a transaction in order to use SPI */
		StartTransactionCommand();
		PushActiveSnapshot(GetTransactionSnapshot());
		SetSessionUserId(BOOTSTRAP_SUPERUSERID, true);

		if (SPI_OK_CONNECT != SPI_connect())
		{
			ereport(ERROR, (errcode(ERRCODE_CDB_INTERNAL_ERROR),
							errmsg("Unable to obtain change tracking information from segment database."),
							errdetail("SPI_connect failed in ChangeTracking_GetChanges()")));
		}
		connected = true;

		/* Do the query. */
		ret = SPI_execute(sqlstmt.data, true, 0);
		proc = SPI_processed;


		if (ret > 0 && SPI_tuptable != NULL)
		{
			TupleDesc	tupdesc = SPI_tuptable->tupdesc;
			SPITupleTable *tuptable = SPI_tuptable;
			MemoryContext cxt_save;
			int			i;

			/*
			 * if got gp_filerep_ct_batch_size changes or less, it means we
			 * satisfied all the requests. If not, it means there are still
			 * more results to return in the next calls.
			 */
			bool		satisfied_request = (proc <= gp_filerep_ct_batch_size);

			/*
			 * Iterate through each result tuple
			 */
			for (i = 0; i < proc; i++)
			{
				HeapTuple	tuple = tuptable->vals[i];

				BlockNumber blocknum;
				XLogRecPtr *endlsn;
				Oid			space;
				Oid			db;
				Oid			rel;
				char	   *str_space;
				char	   *str_db;
				char	   *str_rel;
				char	   *str_blocknum;
				char	   *str_endlsn;


				/* get result columns from SPI (as strings) */
				str_space = SPI_getvalue(tuple, tupdesc, 1);
				str_db = SPI_getvalue(tuple, tupdesc, 2);
				str_rel = SPI_getvalue(tuple, tupdesc, 3);
				str_blocknum = SPI_getvalue(tuple, tupdesc, 4);
				str_endlsn = SPI_getvalue(tuple, tupdesc, 5);

				/*
				 * elog(NOTICE,"tuple %d: %s %s %s block %s lsn %s", i,
				 * str_space, str_db, str_rel, str_blocknum, str_endlsn);
				 */

				/* use our own context so that SPI won't free our stuff later */
				cxt_save = MemoryContextSwitchTo(oldcontext);

				/* init the result memory on first pass */
				if (i == 0)
				{
					if (satisfied_request)
					{
						/* prepare memory for 'proc' entries (full result) */
						result = ChangeTracking_FormResult(proc);
					}
					else
					{
						/* prepare memory for partial result */
						if (request->count != 1)
							elog(ERROR, "internal error in ChangeTracking_GetChanges(): caller "
								 "passed in an invalid request (expecting more than %d "
								 "result entries for more than a single relation)",
								 gp_filerep_ct_batch_size);

						result = ChangeTracking_FormResult(gp_filerep_ct_batch_size);

						/*
						 * tell caller to call us again with the same relation
						 * (but different start lsn)
						 */
						result->ask_for_more = true;
					}
				}


				/* convert to desired data type */
				space = DatumGetObjectId(DirectFunctionCall1(oidin, CStringGetDatum(str_space)));
				db = DatumGetObjectId(DirectFunctionCall1(oidin, CStringGetDatum(str_db)));
				rel = DatumGetObjectId(DirectFunctionCall1(oidin, CStringGetDatum(str_rel)));
				blocknum = DatumGetUInt32(DirectFunctionCall1(int4in, CStringGetDatum(str_blocknum)));
				endlsn = (XLogRecPtr *) DatumGetPointer(DirectFunctionCall1(gpxloglocin, CStringGetDatum(str_endlsn)));

				/*
				 * TODO: in the above should use DatumGetXLogLoc instead, but
				 * it's not public
				 */

				/*
				 * skip the last "extra" entry if satisfied_request is false
				 */
				if (i == gp_filerep_ct_batch_size)
				{
					Assert(!satisfied_request);
					Assert(result->ask_for_more);
					MemoryContextSwitchTo(cxt_save);
					break;
				}

				/* add our entry to the result */
				ChangeTracking_AddResultEntry(result,
											  space,
											  db,
											  rel,
											  blocknum,
											  endlsn);

				MemoryContextSwitchTo(cxt_save);
			}
		}
		else
		{
			/* no results for this given request */
			result = NULL;
		}

		connected = false;
		SPI_finish();

		PopActiveSnapshot();
		CommitTransactionCommand();
		enable_groupagg = old_enable_groupagg;
	}

	/* Clean up in case of error. */
	PG_CATCH();
	{
		if (connected)
			SPI_finish();

		PopActiveSnapshot();
		AbortCurrentTransaction();

		enable_groupagg = old_enable_groupagg;

		/* Carry on with error handling. */
		PG_RE_THROW();
	}
	PG_END_TRY();

	pfree(sqlstmt.data);

	MemoryContextSwitchTo(oldcontext);
	CurrentResourceOwner = save;

	return result;
}

/*
 * Free all memory associated with the result, after using it.
 */
void
ChangeTracking_FreeResult(ChangeTrackingResult *result)
{
	if (result)
	{
		pfree(result->entries);
		pfree(result);
	}
}

/*************************************
 * File I/O routines
 *************************************/

static void
ChangeTracking_SetPathByType(CTFType ftype, char *path)
{
	if (ftype == CTF_LOG_FULL)
		ChangeTrackingFullLogFilePath(path);
	else if (ftype == CTF_LOG_COMPACT)
		ChangeTrackingCompactLogFilePath(path);
	else if (ftype == CTF_LOG_TRANSIENT)
		ChangeTrackingTransientLogFilePath(path);
	else
		ChangeTrackingMetaFilePath(path);
}

/*
 * Open change tracking log file for write, and seek if needed
 */
File
ChangeTracking_OpenFile(CTFType ftype)
{
	File		file;
	char		path[MAXPGPATH];
	struct stat st;

	if (stat(CHANGETRACKINGDIR, &st) < 0)
	{
		errno = 0;
		if (mkdir(CHANGETRACKINGDIR, 0700) < 0)
		{
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("change tracking failure, "
							"could not create '%s' directory : %m",
							CHANGETRACKINGDIR),
					 errhint("create directory and re-start the cluster")));
		}
	}

	ChangeTracking_SetPathByType(ftype, path);

	switch (ftype)
	{
		case CTF_META:

			/* open it (and create if doesn't exist) */
			file = PathNameOpenFile(path,
									O_RDWR | O_CREAT | PG_BINARY,
									S_IRUSR | S_IWUSR);

			if (file == -1)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not open file \"%s\": %m", path)));

			/*
			 * seek to beginning of file. The meta file only has a single
			 * block. we will overwrite it each time with new meta data.
			 */
			FileSeek(file, 0, SEEK_SET);
			break;

		case CTF_LOG_FULL:
		case CTF_LOG_COMPACT:
		case CTF_LOG_TRANSIENT:

			/*
			 * open it (create if doesn't exist). seek to eof for appending.
			 * (can't use O_APPEND because we may like to reposition later
			 * on).
			 */
			file = PathNameOpenFile(path,
									O_RDWR | O_CREAT | PG_BINARY,
									S_IRUSR | S_IWUSR);

			if (file == -1)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not open file \"%s\": %m", path)));

			FileSeek(file, 0, SEEK_END);
			break;

		default:
			file = -1;
			elog(ERROR, "internal error in ChangeTracking_OpenFile. type %d", ftype);
	}

	return file;
}

void
ChangeTracking_CloseFile(File file)
{
	FileClose(file);
}

bool
ChangeTracking_DoesFileExist(CTFType ftype)
{
	File		file;
	char		path[MAXPGPATH];

	/* set up correct path */
	ChangeTracking_SetPathByType(ftype, path);

	/* open it (don't create if doesn't exist) */
	file = PathNameOpenFile(path,
							O_RDONLY | PG_BINARY,
							S_IRUSR | S_IWUSR);

	if (file < 0)
		return false;
	else
		ChangeTracking_CloseFile(file);

	return true;
}

/*
 * Rename one log file to another
 */
static void
ChangeTracking_RenameLogFile(CTFType source, CTFType dest)
{
	char		fn1[MAXPGPATH];
	char		fn2[MAXPGPATH];

	ChangeTracking_SetPathByType(source, fn1);
	ChangeTracking_SetPathByType(dest, fn2);

	/* Rename the FULL log file to TRANSIENT log */
	if (rename(fn1, fn2))
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not rename file \"%s\" to \"%s\": %m",
						fn1, fn2)));

	Assert(ChangeTracking_DoesFileExist(source) == false);
}

static void
ChangeTracking_DropLogFile(CTFType ftype)
{
	char		path[MAXPGPATH];

	Assert(ftype != CTF_META);

	ChangeTracking_SetPathByType(ftype, path);

	if (unlink(path))
		elog(LOG, "could not unlink file \"%s\": %m", path);
}

static void
ChangeTracking_DropLogFiles(void)
{
	ChangeTracking_DropLogFile(CTF_LOG_FULL);
	ChangeTracking_DropLogFile(CTF_LOG_TRANSIENT);
	ChangeTracking_DropLogFile(CTF_LOG_COMPACT);

	ChangeTracking_ResetBufStatus(CTMainWriteBufStatus);
	ChangeTracking_ResetBufStatus(CTCompactWriteBufStatus);
	ChangeTracking_ResetCompactingStatus(changeTrackingCompState);
}


static void
ChangeTracking_DropMetaFile(void)
{
	char		path[MAXPGPATH];

	changeTrackingResyncMeta->resync_mode_full = false;
	setFullResync(changeTrackingResyncMeta->resync_mode_full);
	changeTrackingResyncMeta->resync_lsn_end.xlogid = 0;
	changeTrackingResyncMeta->resync_lsn_end.xrecoff = 0;
	changeTrackingResyncMeta->resync_transition_completed = false;
	changeTrackingResyncMeta->insync_transition_completed = false;

	/* Delete the change tracking meta file */
	ChangeTracking_SetPathByType(CTF_META, path);

	if (unlink(path))
		elog(WARNING, "could not unlink file \"%s\": %m", path);
}

/*
 * Drop all change tracking files (log and meta).
 */
void
ChangeTracking_DropAll(void)
{
	LWLockAcquire(ChangeTrackingWriteLock, LW_EXCLUSIVE);

	ChangeTracking_DropLogFiles();
	ChangeTracking_DropMetaFile();

	LWLockRelease(ChangeTrackingWriteLock);
}

static void
ChangeTracking_ResetBufStatus(ChangeTrackingBufStatusData *bufstat)
{
	bufstat->bufsize = 0;
	bufstat->recordcount = 0;
	bufstat->nextwritepos = CHANGETRACKING_FILE_EOF;
	bufstat->fileseg = 0;
}

static void
ChangeTracking_ResetCompactingStatus(ChangeTrackingLogCompactingStateData *compstat)
{
	compstat->ctcompact_bufs_added = 0;
	compstat->ctfull_bufs_added = 0;
	compstat->in_progress = false;
}


/*
 * Write our shared memory buffer to the log file. This will be
 * called if we have a buffer full of change log records or
 * during a checkpoint (a checkpoint will flush it too).
 *
 * NOTE: caller must hold a LW lock before calling this function.
 * NOTE: caller must also verify that the buffer isn't empty.
 */
static int
ChangeTracking_WriteBuffer(File file, CTFType ftype)
{
	ChangeTrackingBufStatusData *bufstat;
	ChangeTrackingPageHeader *headerptr;
	char	   *buf;
	int			freespace;
	int			wrote = 0;
	int64		restartpos;
	int64		actualpos = 0;
	pg_crc32	crc;

	Assert(ftype != CTF_META);
	Assert(ftype != CTF_LOG_TRANSIENT);

	if (ftype == CTF_LOG_FULL)
	{
		/* this is for a regular write from xlog */
		bufstat = CTMainWriteBufStatus;
		buf = changeTrackingMainBuffer;
	}
	else
	{
		/* this is a write as a part of a compacting operation */
		bufstat = CTCompactWriteBufStatus;
		buf = changeTrackingCompactingBuffer;
	}

	freespace = bufstat->maxbufsize - bufstat->bufsize;

	if (bufstat->recordcount == 0)
		elog(ERROR, "ChangeTracking_WriteBuffer called with empty buffer");

	/* Set the checksum to 0 to include header also in the crc calculation */
	headerptr = (ChangeTrackingPageHeader *) buf;
	headerptr->blockversion = CHANGETRACKING_STORAGE_VERSION;
	headerptr->numrecords = bufstat->recordcount;
	headerptr->checksum = 0;

	/* pad end of page with zeros */
	MemSet(buf + bufstat->bufsize, 0, freespace);

	/* Calculate checksum for the whole buffer, including the header. */
	Assert(bufstat->maxbufsize == CHANGETRACKING_BLCKSZ);
	INIT_CRC32C(crc);
	COMP_CRC32C(crc, buf, bufstat->maxbufsize);
	FIN_CRC32C(crc);

	headerptr->checksum = crc;

	/*
	 * 1) set the file write position with FileSeek. 2) save the position in
	 * 'restartpos' for the next round (if the page we'll write soon isn't
	 * full). see nextwritepos at the bottom of this function for more
	 * information.
	 */
	if (bufstat->nextwritepos != CHANGETRACKING_FILE_EOF)
		restartpos = FileSeek(file, bufstat->nextwritepos, SEEK_SET);
	else
		restartpos = FileSeek(file, 0, SEEK_END);


	/* write the complete buffer to file */
	errno = 0;
	wrote = FileWrite(file, buf, bufstat->maxbufsize);

	/* Verify if the number of bytes written is what we expected */
	if (wrote != bufstat->maxbufsize)
	{
		/*
		 * The callers of this module should handle the error return value.
		 * But still adding an extra elog message for debug purposes.
		 */
		elog(WARNING, "Unable to write %d bytes to change tracking %s. Actual bytes written = %d."
			 ,bufstat->maxbufsize
			 ,ChangeTracking_FtypeToString(ftype)
			 ,wrote);
		return -1;
	}

	/*
	 * check if there's room for more records in this buffer.
	 */
	if (freespace < sizeof(ChangeTrackingRecord))
	{
		/* block written and is full. reset it to zero */
		ChangeTracking_ResetBufStatus(bufstat);

		/* update count of bufs added since last compact operation */
		if (ftype == CTF_LOG_FULL)
			changeTrackingCompState->ctfull_bufs_added++;
		else
			changeTrackingCompState->ctcompact_bufs_added++;

		/*
		 * This is an extra check to see if we can correctly seek to the end
		 * of file. If the seek to the end of the file is broken, there is a
		 * chance that in the next block writing cycle the system may start
		 * writing from a stale EOF causing corruption.
		 */
		actualpos = FileSeek(file, 0, SEEK_END);
		if (restartpos + bufstat->maxbufsize != actualpos)
		{
			ereport(WARNING, (errmsg("FileSeek to the end looks broken."
									 " Expected EOF = " INT64_FORMAT
									 " but after FileSeek(SEEK_END) we get EOF = " INT64_FORMAT
									 ,(restartpos + bufstat->maxbufsize)
									 ,actualpos)));
		}
	}
	else
	{
		/*
		 * written block is partially full (probably due to a checkpoint).
		 * Don't reset the buffer in shared memory, as we'd like to fill it up
		 * and later overwrite the block we just wrote. Set the next write
		 * position to the beginning of this page we just wrote, to use in the
		 * next round.
		 */
		bufstat->nextwritepos = restartpos;
	}


	return 0;
}

/*
 * Write a meta record to the meta file.
 */
static int
ChangeTracking_WriteMeta(File file)
{

	ChangeTrackingMetaRecord rec;
	int			wrote = 0;

	Assert(CHANGETRACKING_METABUFLEN >= sizeof(rec));

	/* populate a new meta record */
	rec.resync_lsn_end = changeTrackingResyncMeta->resync_lsn_end;
	rec.resync_mode_full = changeTrackingResyncMeta->resync_mode_full;
	rec.resync_transition_completed = changeTrackingResyncMeta->resync_transition_completed;
	rec.insync_transition_completed = changeTrackingResyncMeta->insync_transition_completed;

	/* init buffer and copy rec into it */
	MemSet(metabuf, 0, CHANGETRACKING_METABUFLEN);
	memcpy(metabuf, &rec, sizeof(rec));

	/* write the complete buffer to meta file */
	errno = 0;
	wrote = FileWrite(file, metabuf, CHANGETRACKING_METABUFLEN);

	/* Verify if the number of bytes written are what we expected. */
	if (wrote != CHANGETRACKING_METABUFLEN)
		return -1;

	return 0;
}

/*
 * Read a meta record from the Meta file.
 */
static ChangeTrackingMetaRecord *
ChangeTracking_ReadMeta(File file, ChangeTrackingMetaRecord *rec)
{
	char		path[MAXPGPATH];
	int			nbytes = 0;

	Assert(CHANGETRACKING_METABUFLEN >= sizeof(ChangeTrackingMetaRecord));

	/* init buffer */
	MemSet(metabuf, 0, CHANGETRACKING_METABUFLEN);

	/* read the record from the meta file, if any */
	errno = 0;
	nbytes = FileRead(file, metabuf, sizeof(ChangeTrackingMetaRecord));

	if (nbytes == 0)
	{
		/* no record */
		return NULL;
	}
	else if (nbytes < 0)
	{
		ChangeTrackingMetaFilePath(path);
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("unable to read change tracking meta file \"%s\", "
						"change tracking disabled : %m",
						path),
				 errSendAlert(true)));
		return NULL;
	}

	/* populate a the meta record with data from the file */
	rec->resync_lsn_end = ((ChangeTrackingMetaRecord *) metabuf)->resync_lsn_end;
	rec->resync_mode_full = ((ChangeTrackingMetaRecord *) metabuf)->resync_mode_full;
	rec->resync_transition_completed = ((ChangeTrackingMetaRecord *) metabuf)->resync_transition_completed;
	rec->insync_transition_completed = ((ChangeTrackingMetaRecord *) metabuf)->insync_transition_completed;

	return rec;
}

bool
ChangeTracking_ShouldTrackChanges(void)
{
	return (FileRep_IsInChangeTracking() && gp_change_tracking);
}

/*************************************
 * Resync related routines.
 *************************************/

/*
 * The routine mark in shared memory and persistently in Change Tracking log file
 * that full resync is required and disable change tracking.
 *
 * The routine is called
 *		a) if failure is detected during Change Tracking
 *		b) if user requested gprecoverseg with --FULL option (i.e. mirror node replacement)
 *		c) if user performed gpaddmirrors
 *
 * The process should have an exclusive lock on ChangeTrackingWriteLock before calling this function.
 */

/*
 * Drop all change tracking files (log and meta).
 */
void
ChangeTracking_MarkFullResync(void)
{
	LWLockAcquire(ChangeTrackingWriteLock, LW_EXCLUSIVE);

	ChangeTracking_MarkFullResyncLockAcquired();	/* no need to check return
													 * code here */

	LWLockRelease(ChangeTrackingWriteLock);
}

static void
ChangeTracking_MarkFullResyncLockAcquired(void)
{
	File		file;
	CTFType		ftype = CTF_META;
	bool		emit_error = false;

	/*
	 * Insert an entry into the meta file that mark FullResync needed, and
	 * fsync.
	 */
	FileRep_InsertConfigLogEntry("marking full resync ");

	changeTrackingResyncMeta->resync_mode_full = true;
	setFullResync(changeTrackingResyncMeta->resync_mode_full);
	changeTrackingResyncMeta->resync_lsn_end.xlogid = 0;
	changeTrackingResyncMeta->resync_lsn_end.xrecoff = 0;
	changeTrackingResyncMeta->resync_transition_completed = false;
	changeTrackingResyncMeta->insync_transition_completed = false;

	file = ChangeTracking_OpenFile(ftype);

	/*
	 * Write and fsync the file. If we fail here we can't recover from the
	 * error. Must call FileRep_SetPostmasterReset()
	 */
	if (ChangeTracking_WriteMeta(file) < 0)
	{
		emit_error = true;
		FileRep_SetPostmasterReset();
	}

	if (FileSync(file) < 0)
	{
		emit_error = true;
		FileRep_SetPostmasterReset();
	}

	if (emit_error)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("write error for change tracking meta file in "
						"ChangeTracking_MarkFullResyncLockAcquired. "
						"Change Tracking disabled : %m"),
				 errSendAlert(true)));
	}

	ChangeTracking_CloseFile(file);

	/* Delete Change Tracking log file (if exists) */
	ChangeTracking_DropLogFiles();

	/* set full resync flag in configuration shared memory */
	setFullResync(changeTrackingResyncMeta->resync_mode_full);

	getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, NULL, NULL);

	FileRep_SetSegmentState(SegmentStateChangeTrackingDisabled, FaultTypeNotInitialized);

	getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, NULL, NULL);
	Assert(segmentState == SegmentStateChangeTrackingDisabled);
}

/*
 * Mark incremental resync, reset metadata and don't drop change tracking log files
 */
void
ChangeTracking_MarkIncrResync(void)
{
	File		file;
	CTFType		ftype = CTF_META;

	FileRep_InsertConfigLogEntry("marking incremental resync ");

	LWLockAcquire(ChangeTrackingWriteLock, LW_EXCLUSIVE);

	changeTrackingResyncMeta->resync_mode_full = false;
	setFullResync(changeTrackingResyncMeta->resync_mode_full);
	changeTrackingResyncMeta->resync_lsn_end.xlogid = 0;
	changeTrackingResyncMeta->resync_lsn_end.xrecoff = 0;
	changeTrackingResyncMeta->resync_transition_completed = false;
	changeTrackingResyncMeta->insync_transition_completed = false;

	file = ChangeTracking_OpenFile(ftype);

	if (ChangeTracking_WriteMeta(file) < 0)
		ChangeTracking_HandleWriteError(ftype);

	errno = 0;

	/* fsync meta file to disk */
	if (FileSync(file) < 0)
		ChangeTracking_HandleWriteError(ftype);

	ChangeTracking_CloseFile(file);

	LWLockRelease(ChangeTrackingWriteLock);
}

void
ChangeTracking_MarkTransitionToResyncCompleted(void)
{
	File		file;
	CTFType		ftype = CTF_META;

	LWLockAcquire(ChangeTrackingWriteLock, LW_EXCLUSIVE);

	FileRep_InsertConfigLogEntry("setting resync_transition_completed to true ");

	changeTrackingResyncMeta->resync_mode_full = false;
	changeTrackingResyncMeta->resync_transition_completed = true;
	file = ChangeTracking_OpenFile(ftype);

	if (ChangeTracking_WriteMeta(file) < 0)
		ChangeTracking_HandleWriteError(ftype);

	errno = 0;

	/* fsync meta file to disk */
	if (FileSync(file) < 0)
		ChangeTracking_HandleWriteError(ftype);

	ChangeTracking_CloseFile(file);

	LWLockRelease(ChangeTrackingWriteLock);
}

void
ChangeTracking_MarkTransitionToInsyncCompleted(void)
{
	File		file;
	CTFType		ftype = CTF_META;

	LWLockAcquire(ChangeTrackingWriteLock, LW_EXCLUSIVE);

	FileRep_InsertConfigLogEntry("setting insync_transition_completed to true ");

	changeTrackingResyncMeta->insync_transition_completed = true;
	file = ChangeTracking_OpenFile(ftype);

	if (ChangeTracking_WriteMeta(file) < 0)
		ChangeTracking_HandleWriteError(ftype);

	errno = 0;

	/* fsync meta file to disk */
	if (FileSync(file) < 0)
		ChangeTracking_HandleWriteError(ftype);

	ChangeTracking_CloseFile(file);

	LWLockRelease(ChangeTrackingWriteLock);
}

/*
 *	Store last change tracked LSN into shared memory and
 *	in Change Tracking meta file (LSN has to be fsync-ed to persistent media).
 *	Also make sure to write any remaining log file data and fsync it.
 */
void
ChangeTracking_RecordLastChangeTrackedLoc(void)
{
	File		file;
	CTFType		ftype = CTF_META;
	XLogRecPtr	endResyncLSN;

	LWLockAcquire(ChangeTrackingWriteLock, LW_EXCLUSIVE);

	if (!isFullResync())
	{
		setFullResync(changeTrackingResyncMeta->resync_mode_full);
	}

	endResyncLSN = XLogLastChangeTrackedLoc();

	/* the routine stores last LSN in shared memory */
	FileRepResyncManager_SetEndResyncLSN(endResyncLSN);

	/*
	 * append "endResyncLSN" (last LSN recorded in Change Tracking log files)
	 * in Change Tracking meta file. "endResyncLSN" also marks the last entry
	 * in Change Tracking before resync takes place. Later If transition from
	 * Resync to Change tracking occurs then new changes will be appended.
	 */
	FileRep_InsertConfigLogEntry("setting resync lsn ");

	changeTrackingResyncMeta->resync_lsn_end = endResyncLSN;
	file = ChangeTracking_OpenFile(ftype);

	if (ChangeTracking_WriteMeta(file) < 0)
		ChangeTracking_HandleWriteError(ftype);

	errno = 0;

	/* fsync meta file to disk */
	if (FileSync(file) < 0)
		ChangeTracking_HandleWriteError(ftype);

	ChangeTracking_CloseFile(file);

	/*
	 * log file: write the existing (non-full) buffer and fsync it
	 */
	ftype = CTF_LOG_FULL;
	file = ChangeTracking_OpenFile(ftype);

	if (CTMainWriteBufStatus->recordcount > 0)
	{
		if (ChangeTracking_WriteBuffer(file, ftype) < 0)
			ChangeTracking_HandleWriteError(ftype);
	}

	{
		char		tmpBuf[FILEREP_MAX_LOG_DESCRIPTION_LEN];

		snprintf(tmpBuf, sizeof(tmpBuf),
				 "number of ct records to flush count '%u' size '%u' max '%u' offset " INT64_FORMAT " fileseg '%u' ",
				 CTMainWriteBufStatus->recordcount,
				 CTMainWriteBufStatus->bufsize,
				 CTMainWriteBufStatus->maxbufsize,
				 CTMainWriteBufStatus->nextwritepos,
				 CTMainWriteBufStatus->fileseg);

		FileRep_InsertConfigLogEntry(tmpBuf);
	}

	if (FileSync(file) < 0)
		ChangeTracking_HandleWriteError(ftype);

	ChangeTracking_CloseFile(file);

	LWLockRelease(ChangeTrackingWriteLock);
}

/*
 * The routine is called during gpstart if dataState == DataStateInResync.
 * Responsibilities:
 *		*) determines when crash recovery will be performed by checking
 *		   if lastChangeTrackedLoc is recorded
 *			*) if lastChangeTrackedLoc is recorded then
 *					1) resync recovers to ready state
 *					2) WAL replays to primary and mirror
 *			*) if lastChangeTrackedLoc is NOT recorded then
 *					1) WAL replays to primary with Change Tracking ON
 *					2) transition to Resync
 *		*) check/set if fullResync is recorded
 */
bool
ChangeTracking_RetrieveLastChangeTrackedLoc(void)
{
	ChangeTrackingMetaRecord rec;
	ChangeTrackingMetaRecord *recptr = NULL;
	XLogRecPtr	DummyRecPtr = {0, 0};
	File		file;

	file = ChangeTracking_OpenFile(CTF_META);

	recptr = ChangeTracking_ReadMeta(file, &rec);

	/* is there a meta record in the meta file? */
	if (recptr)
	{
		elog(LOG, "CHANGETRACKING: found an MD record. is full resync %d, last lsn (%d/%d) "
			 "is transition to resync completed %d, is transition to insync completed %d",
			 recptr->resync_mode_full,
			 recptr->resync_lsn_end.xlogid,
			 recptr->resync_lsn_end.xrecoff,
			 recptr->resync_transition_completed,
			 recptr->insync_transition_completed);

		setFullResync(recptr->resync_mode_full);
		changeTrackingResyncMeta->resync_mode_full = recptr->resync_mode_full;

		/*
		 * if resync_lsn_end isn't {0,0} then we have a valid value that was
		 * set earlier. in that case set it in the resync manager shared
		 * memory
		 */
		if (!XLByteEQ(recptr->resync_lsn_end, DummyRecPtr))
		{
			FileRepResyncManager_SetEndResyncLSN(recptr->resync_lsn_end);
			return recptr->resync_transition_completed;
		}
	}
	else
		FileRep_InsertConfigLogEntry("pg_changetracking meta data record not found ");

	ChangeTracking_CloseFile(file);

	return false;
}

/*
 * Return the value stored in the MD file insync_transition_completed field.
 */
bool
ChangeTracking_RetrieveIsTransitionToInsync(void)
{
	ChangeTrackingMetaRecord rec;
	ChangeTrackingMetaRecord *recptr = NULL;
	File		file;
	bool		res = false;

	file = ChangeTracking_OpenFile(CTF_META);

	recptr = ChangeTracking_ReadMeta(file, &rec);

	/* is there a meta record in the meta file? */
	if (recptr)
	{
		res = recptr->insync_transition_completed;

		/* set full resync flag in configuration shared memory */
		setFullResync(recptr->resync_mode_full);


		elog(LOG, "CHANGETRACKING: ChangeTracking_RetrieveIsTransitionToInsync() found "
			 "insync_transition_completed:'%s' full resync:'%s' ",
			 (res == TRUE) ? "true" : "false",
			 (recptr->resync_mode_full == TRUE) ? "true" : "false");

	}
	else
		FileRep_InsertConfigLogEntry("pg_changetracking meta data record not found ");

	ChangeTracking_CloseFile(file);

	return res;
}

/*
 * Return the value stored in the MD file resync_transition_completed field.
 */
bool
ChangeTracking_RetrieveIsTransitionToResync(void)
{
	ChangeTrackingMetaRecord rec;
	ChangeTrackingMetaRecord *recptr = NULL;
	File		file;
	bool		res = false;

	file = ChangeTracking_OpenFile(CTF_META);

	recptr = ChangeTracking_ReadMeta(file, &rec);

	/* is there a meta record in the meta file? */
	if (recptr)
	{
		res = recptr->resync_transition_completed;

		/* set full resync flag in configuration shared memory */
		setFullResync(recptr->resync_mode_full);

		elog(LOG,
			 "CHANGETRACKING: ChangeTracking_RetrieveIsTransitionToResync() found "
			 "resync_transition_completed:'%s' full resync:'%s' ",
			 (res == TRUE) ? "true" : "false",
			 (recptr->resync_mode_full == TRUE) ? "true" : "false");
	}
	else
		FileRep_InsertConfigLogEntry("pg_changetracking meta data record not found ");

	ChangeTracking_CloseFile(file);

	return res;
}

/**
 * Get the total amount of space, in bytes, used by the changetracking information.
 */
int64
ChangeTracking_GetTotalSpaceUsedOnDisk(void)
{
	return db_dir_size(CHANGETRACKINGDIR);
}

/*************************************
 * Compacting related routines.
 *************************************/

/*
 * ChangeTracking_doesFileNeedCompacting
 *
 * If a log file was appended more than CHANGETRACKING_COMPACT_THRESHOLD bytes (currently
 * 1GB) since the last time it was compacted, return true. otherwise return false.
 */
bool
ChangeTracking_doesFileNeedCompacting(CTFType ftype)
{
	bool		needs_compacting = false;

	Assert(ftype != CTF_LOG_TRANSIENT);
	Assert(ftype != CTF_META);

	switch (ftype)
	{
		case CTF_LOG_FULL:
			needs_compacting = (changeTrackingCompState->ctfull_bufs_added * CHANGETRACKING_BLCKSZ >
								CHANGETRACKING_COMPACT_THRESHOLD);
			break;
		case CTF_LOG_COMPACT:
			needs_compacting = (changeTrackingCompState->ctcompact_bufs_added * CHANGETRACKING_BLCKSZ >
								CHANGETRACKING_COMPACT_THRESHOLD);
			break;
		default:
			elog(ERROR, "internal error in ChangeTracking_doesFileNeedCompacting (used %d)", ftype);
	}

	return needs_compacting;
}

/*
 * Create a transient log file for the filerep process
 * to use for compacting. Do that if all of the following
 * conditions exist:
 *
 * 1) The full log file is larger than the threshold for
 * compacting (currently 1GB).
 *
 * 2) Filerep compacting operation isn't currently in progress
 *
 * 3) There isn't already a transient file we previously created,
 * since this means that the filerep process didn't get to
 * compacting it yet, and we don't want to run it over.
 *
 */
static void
ChangeTracking_CreateTransientLogIfNeeded(void)
{
	LWLockAcquire(ChangeTrackingCompactLock, LW_EXCLUSIVE);
	LWLockAcquire(ChangeTrackingWriteLock, LW_EXCLUSIVE);

	if (ChangeTracking_doesFileNeedCompacting(CTF_LOG_FULL) &&	/* condition (1) */
		changeTrackingCompState->in_progress == false &&	/* condition (2) */
		ChangeTracking_DoesFileExist(CTF_LOG_TRANSIENT) == false)	/* condition (3) */
	{
		ChangeTracking_RenameLogFile(CTF_LOG_FULL, CTF_LOG_TRANSIENT);

		/* we must now reset our full log write as we'll start a new file */
		ChangeTracking_ResetBufStatus(CTMainWriteBufStatus);

		changeTrackingCompState->ctfull_bufs_added = 0;
	}

	LWLockRelease(ChangeTrackingWriteLock);
	LWLockRelease(ChangeTrackingCompactLock);
}

/*
 * During crash recovery append records from CT_LOG_FULL to CT_LOG_TRANSIENT in order to run compacting
 * that will discard records that have higher lsn than the highest lsn in xlog.
 */
void
ChangeTracking_CreateTransientLog(void)
{
	LWLockAcquire(ChangeTrackingCompactLock, LW_EXCLUSIVE);
	LWLockAcquire(ChangeTrackingWriteLock, LW_EXCLUSIVE);

	if (ChangeTracking_DoesFileExist(CTF_LOG_TRANSIENT) == false)
	{
		FileRep_InsertConfigLogEntry("rename full to transient change tracking log file");
		ChangeTracking_RenameLogFile(CTF_LOG_FULL, CTF_LOG_TRANSIENT);

		/* we must now reset our full log write as we'll start a new file */
		ChangeTracking_ResetBufStatus(CTMainWriteBufStatus);

		changeTrackingCompState->ctfull_bufs_added = 0;
	}
	else
	{
		File		fileTransient = 0;
		File		fileFull = 0;

		CTFType		ftype = CTF_LOG_FULL;
		int64		position = 0;
		int64		positionFull = 0;
		int64		positionFullEnd = 0;

		int			nbytes = 0;
		char	   *buf = NULL;

		FileRep_InsertConfigLogEntry("append records from full to transient change tracking log file");

		while (1)
		{
			errno = 0;
			fileFull = ChangeTracking_OpenFile(ftype);

			if (fileFull > 0)
			{
				positionFullEnd = FileSeek(fileFull, 0, SEEK_END);
				positionFull = FileSeek(fileFull, 0, SEEK_SET);

				if (positionFullEnd < 0 || positionFull < 0)
				{
					ereport(WARNING,
							(errcode_for_file_access(),
							 errmsg("unable to seek to begin " INT64_FORMAT " or end " INT64_FORMAT " in change tracking '%s' file : %m",
									positionFull,
									positionFullEnd,
									ChangeTracking_FtypeToString(ftype))));
					break;
				}
			}
			else
			{
				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("unable to open change tracking '%s' file : %m",
								ChangeTracking_FtypeToString(ftype))));
				break;
			}

			ftype = CTF_LOG_TRANSIENT;
			errno = 0;
			fileTransient = ChangeTracking_OpenFile(ftype);

			if (fileTransient > 0)
			{
				position = FileSeek(fileTransient, 0, SEEK_END);

				if (position < 0)
				{
					ereport(WARNING,
							(errcode_for_file_access(),
							 errmsg("unable to seek to end in change tracking '%s' file : %m",
									ChangeTracking_FtypeToString(ftype))));
					break;
				}
			}
			else
			{
				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("unable to open change tracking '%s' file : %m",
								ChangeTracking_FtypeToString(ftype))));
				break;
			}

			buf = MemoryContextAlloc(TopMemoryContext, CHANGETRACKING_BLCKSZ);
			if (buf == NULL)
			{
				ChangeTracking_CloseFile(fileTransient);
				ChangeTracking_CloseFile(fileFull);

				LWLockRelease(ChangeTrackingWriteLock);
				LWLockRelease(ChangeTrackingCompactLock);

				ereport(ERROR,
						(errcode(ERRCODE_OUT_OF_MEMORY),
						 (errmsg("could not allocate memory for change tracking log buffer"))));
			}

			MemSet(buf, 0, CHANGETRACKING_BLCKSZ);

			while (positionFull < positionFullEnd)
			{
				errno = 0;
				nbytes = FileRead(fileFull, buf, CHANGETRACKING_BLCKSZ);

				if (nbytes == CHANGETRACKING_BLCKSZ)
				{
					nbytes = FileWrite(fileTransient, buf, CHANGETRACKING_BLCKSZ);

					if (nbytes < CHANGETRACKING_BLCKSZ)
					{
						ChangeTracking_HandleWriteError(ftype);
						break;
					}
				}
				else
				{
					ChangeTracking_HandleWriteError(CTF_LOG_FULL);
					break;
				}
				positionFull += CHANGETRACKING_BLCKSZ;
			}

			if (positionFull < positionFullEnd)
			{
				break;
			}

			errno = 0;
			if (FileSync(fileTransient) < 0)
			{
				ChangeTracking_HandleWriteError(ftype);
			}

			ChangeTracking_DropLogFile(CTF_LOG_FULL);

			/* we must now reset our full log write as we'll start a new file */
			ChangeTracking_ResetBufStatus(CTMainWriteBufStatus);

			changeTrackingCompState->ctfull_bufs_added = 0;

			break;
		} //while (1)

			if (fileTransient)
			{
				ChangeTracking_CloseFile(fileTransient);
			}

		if (fileFull)
		{
			ChangeTracking_CloseFile(fileFull);
		}

		if (buf)
		{
			pfree(buf);
		}
	}

	LWLockRelease(ChangeTrackingWriteLock);
	LWLockRelease(ChangeTrackingCompactLock);
}

/*
 * This routine will normally be called by an external process.
 *
 * It will do the following:
 *
 * 1) if a transient file exists (therefore waiting to be compacted),
 *    compact it into the compact log file and remove the transient
 *    file when done.
 *
 * 2) if a compact file is ready to be compacted further, rename it to
 *    transient, compact transient into compact log and remove transient
 *    when done.
 *
 * note that we take the LW compacting lock only very briefly just to
 * set "in progress" flag. Don't want to hold it for the duration of
 * compacting operation since that will slow down changetracking.
 */
void
ChangeTracking_CompactLogsIfPossible(void)
{
	bool		compact_transient = false;	/* should compact transient log
											 * file? */
	bool		compact_compact = false;	/* should compact compact log
											 * file? */

	/* -- transient log -- */

	LWLockAcquire(ChangeTrackingCompactLock, LW_EXCLUSIVE);

	if (ChangeTracking_DoesFileExist(CTF_LOG_TRANSIENT))
	{
		compact_transient = true;
		changeTrackingCompState->in_progress = true;
	}

	LWLockRelease(ChangeTrackingCompactLock);

	if (compact_transient)
	{
		/* Now do the actual compacting. Remove transient log file when done */
		FileRep_InsertConfigLogEntry("compacting the transient log file ");

		ChangeTracking_ResetBufStatus(CTCompactWriteBufStatus);
		ChangeTracking_CompactLogFile(CTF_LOG_TRANSIENT, CTF_LOG_COMPACT, NULL);

		ChangeTracking_DropLogFile(CTF_LOG_TRANSIENT);

	}

	/* -- compact log -- */

	LWLockAcquire(ChangeTrackingCompactLock, LW_EXCLUSIVE);

	if (ChangeTracking_doesFileNeedCompacting(CTF_LOG_COMPACT))
	{
		compact_compact = true;
		changeTrackingCompState->in_progress = true;
	}

	LWLockRelease(ChangeTrackingCompactLock);

	if (compact_compact)
	{
		/* Now do the actual compacting. Remove transient log file when done */
		FileRep_InsertConfigLogEntry("compacting the compact log file ");

		ChangeTracking_ResetBufStatus(CTCompactWriteBufStatus);
		ChangeTracking_RenameLogFile(CTF_LOG_COMPACT, CTF_LOG_TRANSIENT);
		ChangeTracking_CompactLogFile(CTF_LOG_TRANSIENT, CTF_LOG_COMPACT, NULL);
		changeTrackingCompState->ctcompact_bufs_added = 0;	/* reset for next round */
	}

	changeTrackingCompState->in_progress = false;

}

bool
ChangeTrackingIsCompactingInProgress(void)
{
	/*
	 * no lock is required since that information is required only for status
	 * report for test automation
	 */
	return changeTrackingCompState->in_progress;
}

void
ChangeTrackingSetXLogEndLocation(XLogRecPtr upto_lsn)
{
	LWLockAcquire(ChangeTrackingCompactLock, LW_EXCLUSIVE);

	changeTrackingCompState->xlog_end_location = upto_lsn;

	LWLockRelease(ChangeTrackingCompactLock);
}


void
ChangeTracking_DoFullCompactingRoundIfNeeded(void)
{
	if (!(changeTrackingCompState->xlog_end_location.xlogid == 0 &&
		  changeTrackingCompState->xlog_end_location.xrecoff == 0))
	{
		ChangeTracking_DoFullCompactingRound(&changeTrackingCompState->xlog_end_location);
	}
}

/*
 * This routine must be called before resync is started
 * and after the last call to ChangeTracking_CompactLogsIfPossible(),
 * or during recovery start.
 *
 * It will take care of compacting all the left over pieces.
 * (for example, an outstanding transient file, or some file that
 * didn't make it past the 1GB threshold).
 *
 * if the passed in 'uptolsn' is something other than {0,0}, then we
 * tell the compacting routines to ignore any changetracking records
 * with lsn > uptolsn when doing the compacting logic (therefore these
 * records will be gone forever).
 *
 * The logic of this routine is done in the following order:
 *
 * (1) if transient file exists, compact it into compact file. remove
 * transient file.
 *
 * (2) rename the full log file into transient and compact it into the
 * compact file. note that it is possible for the full log file to not
 * exist, in the case where it was just renamed to transient (in
 * ChangeTracking_CompactLogsIfPossible) and no more writes were made
 * into it. Even though it should be a rare case we must check for it
 * here and skip this stage if needed.
 *
 * (3) compact the compact file itself.
 *
 * NOTE: we don't care about taking any locks or setting the in_progress
 * flag. this is because this routine should be run after change tracking
 * mode is complete, so we don't expect change tracking module to do any
 * more writes.
 */
void
ChangeTracking_DoFullCompactingRound(XLogRecPtr *upto_lsn)
{
	/*
	 * we should normally have in_progress == false now, but if the filerep
	 * process that did compacting in the background was killed while
	 * compacting we issue a warning, mainly for tracking purposes. this
	 * should be harmless though - resulting in few duplicate entries.
	 * compacting state is reset few lines down.
	 */
	if (changeTrackingCompState->in_progress)
		elog(LOG, "ChangeTracking: warning - routine compacting was shut off abnormally");

	FileRep_InsertConfigLogEntry("running a full round of compacting the logs ");

	if (upto_lsn != NULL)
		elog(LOG, "ChangeTracking: discarding records with LSN higher than %s",
			 XLogLocationToString(upto_lsn));

	/* compacting state is no longer needed. reset it to be safe */
	ChangeTracking_ResetCompactingStatus(changeTrackingCompState);



	/* step (1) */
	if (ChangeTracking_DoesFileExist(CTF_LOG_TRANSIENT))
	{
		/* do the actual compacting. Remove transient log file when done */
		ChangeTracking_ResetBufStatus(CTCompactWriteBufStatus);
		ChangeTracking_CompactLogFile(CTF_LOG_TRANSIENT, CTF_LOG_COMPACT, upto_lsn);
		ChangeTracking_DropLogFile(CTF_LOG_TRANSIENT);

		upto_lsn = NULL;
		changeTrackingCompState->xlog_end_location.xlogid = 0;
		changeTrackingCompState->xlog_end_location.xrecoff = 0;
		if (segmentState == SegmentStateChangeTrackingDisabled)
			return;
	}

	/* step (2) */
	if (ChangeTracking_DoesFileExist(CTF_LOG_FULL))
	{
		ChangeTracking_RenameLogFile(CTF_LOG_FULL, CTF_LOG_TRANSIENT);
		/* must reset buf status in case a new full file will be created later */
		ChangeTracking_ResetBufStatus(CTMainWriteBufStatus);

		ChangeTracking_ResetBufStatus(CTCompactWriteBufStatus);
		ChangeTracking_CompactLogFile(CTF_LOG_TRANSIENT, CTF_LOG_COMPACT, upto_lsn);
		ChangeTracking_DropLogFile(CTF_LOG_TRANSIENT);
		if (segmentState == SegmentStateChangeTrackingDisabled)
			return;
	}

	/* step (3) */
	ChangeTracking_ResetBufStatus(CTCompactWriteBufStatus);
	ChangeTracking_RenameLogFile(CTF_LOG_COMPACT, CTF_LOG_TRANSIENT);
	ChangeTracking_CompactLogFile(CTF_LOG_TRANSIENT, CTF_LOG_COMPACT, upto_lsn);
	ChangeTracking_DropLogFile(CTF_LOG_TRANSIENT);
	changeTrackingCompState->ctcompact_bufs_added = 0;

	FileRep_InsertConfigLogEntry("full compacting round is finished ");
}

/*
 * ChangeTracking_CompactLogFile
 *
 * compact the source CT log file into the dest CT log file.
 * if 'uptolsn' is not NULL, then discard any record with
 * lsn > uptolsn when compacting.
 */
int
ChangeTracking_CompactLogFile(CTFType source, CTFType dest, XLogRecPtr *uptolsn)
{
	StringInfoData sqlstmt;
	int			ret;
	int			proc;
	bool		connected = false;
	int64		count = 0;
	ResourceOwner save = CurrentResourceOwner;
	MemoryContext oldcontext = CurrentMemoryContext;

	/*
	 * as of right now the only compacting operation possible is
	 * transient-->compact
	 */
	Assert(source == CTF_LOG_TRANSIENT);
	Assert(dest == CTF_LOG_COMPACT);

	/* find out if full log file exists, if not return error */
	if (!ChangeTracking_DoesFileExist(source))
		return -1;

	/* assemble our query string */
	initStringInfo(&sqlstmt);

	appendStringInfo(&sqlstmt, "SELECT space, db, rel, blocknum, max(xlogloc), persistent_tid, persistent_sn "
					 "FROM gp_changetracking_log(%d) ", source);

	/* filter xlogloc higher than uptolsn if requested to do so */
	if (uptolsn != NULL)
		appendStringInfo(&sqlstmt, "WHERE xlogloc <= '(%X/%X)' ", uptolsn->xlogid, uptolsn->xrecoff);

	appendStringInfo(&sqlstmt, "GROUP BY space, db, rel, blocknum, persistent_tid, persistent_sn");

	PG_TRY();
	{

		/* must be in a transaction in order to use SPI */
		StartTransactionCommand();
		PushActiveSnapshot(GetTransactionSnapshot());
		SetSessionUserId(BOOTSTRAP_SUPERUSERID, true);

		if (SPI_OK_CONNECT != SPI_connect())
		{
			ereport(ERROR, (errcode(ERRCODE_CDB_INTERNAL_ERROR),
							errmsg("Unable to obtain change tracking information from segment database."),
							errdetail("SPI_connect failed in ChangeTracking_CreateCompactLogFromFull()")));
		}
		connected = true;

		/* Do the query. */
		ret = SPI_execute(sqlstmt.data, true, 0);
		proc = SPI_processed;

		if ((segmentState != SegmentStateChangeTrackingDisabled) &&
			ret > 0 && SPI_tuptable != NULL)
		{
			TupleDesc	tupdesc = SPI_tuptable->tupdesc;
			SPITupleTable *tuptable = SPI_tuptable;
			MemoryContext cxt_save;
			int			i;

			for (i = 0; i < proc; i++)
			{
				HeapTuple	tuple = tuptable->vals[i];
				RelFileNode relfile;

				BlockNumber blocknum;
				XLogRecPtr *endlsn;
				ItemPointer persistentTid;
				int64		persistentSerialNum;
				char	   *str_space;
				char	   *str_db;
				char	   *str_rel;
				char	   *str_blocknum;
				char	   *str_endlsn;
				char	   *str_tid;
				char	   *str_sn;

				/* get result columns from SPI (as strings) */
				str_space = SPI_getvalue(tuple, tupdesc, 1);
				str_db = SPI_getvalue(tuple, tupdesc, 2);
				str_rel = SPI_getvalue(tuple, tupdesc, 3);
				str_blocknum = SPI_getvalue(tuple, tupdesc, 4);
				str_endlsn = SPI_getvalue(tuple, tupdesc, 5);
				str_tid = SPI_getvalue(tuple, tupdesc, 6);
				str_sn = SPI_getvalue(tuple, tupdesc, 7);

				/*
				 * elog(NOTICE,"tuple %d: %s %s %s block %s lsn %s", i,
				 * str_space, str_db, str_rel, str_blocknum, str_endlsn);
				 */

				/* use our own context so that SPI won't free our stuff later */
				cxt_save = MemoryContextSwitchTo(oldcontext);

				/* convert to desired data type */
				relfile.spcNode = DatumGetObjectId(DirectFunctionCall1(oidin, CStringGetDatum(str_space)));
				relfile.dbNode = DatumGetObjectId(DirectFunctionCall1(oidin, CStringGetDatum(str_db)));
				relfile.relNode = DatumGetObjectId(DirectFunctionCall1(oidin, CStringGetDatum(str_rel)));
				blocknum = DatumGetUInt32(DirectFunctionCall1(int4in, CStringGetDatum(str_blocknum)));
				endlsn = (XLogRecPtr *) DatumGetPointer(DirectFunctionCall1(gpxloglocin, CStringGetDatum(str_endlsn)));
				persistentTid = (ItemPointer) DatumGetPointer(
															  DirectFunctionCall1(tidin, CStringGetDatum(str_tid)));
				persistentSerialNum = DatumGetInt64(DirectFunctionCall1(int8in, CStringGetDatum(str_sn)));

				SIMPLE_FAULT_INJECTOR(FileRepChangeTrackingCompacting);

				/* write this record to the compact file */
				ChangeTracking_AddBufferPoolChange(dest,
												   endlsn,
												   &relfile,
												   blocknum,
												   *persistentTid,
												   persistentSerialNum);

				count++;

				MemoryContextSwitchTo(cxt_save);
			}
		}

		connected = false;
		SPI_finish();

		PopActiveSnapshot();
		CommitTransactionCommand();
	}

	/* Clean up in case of error. */
	PG_CATCH();
	{
		if (connected)
			SPI_finish();

		PopActiveSnapshot();
		AbortCurrentTransaction();

		/* Carry on with error handling. */
		PG_RE_THROW();
	}
	PG_END_TRY();

	/* done writing to the compact file. must fsync now */
	ChangeTracking_FsyncDataIntoLog(dest);

	elog(LOG, "ChangeTracking done creating the compact version. reduced to " INT64_FORMAT " records", count);

	MemoryContextSwitchTo(oldcontext);
	CurrentResourceOwner = save;

	pfree(sqlstmt.data);

	return 0;
}

/*
 * find last LSN recorded in Change Tracking Full Log file
 */
bool
ChangeTracking_GetLastChangeTrackingLogEndLoc(XLogRecPtr *lastChangeTrackingLogEndLoc)
{
	File		file;
	CTFType		ftype = CTF_LOG_FULL;
	int64		position = 0;
	int64		numBlocks = 0;
	int			nbytes = 0;
	char	   *buf = NULL;
	bool		retval = true;

	LWLockAcquire(ChangeTrackingWriteLock, LW_EXCLUSIVE);

	while (1)
	{
		errno = 0;
		file = ChangeTracking_OpenFile(ftype);

		if (file > 0)
		{
			position = FileSeek(file, 0, SEEK_END);

			if (position < 0)
			{
				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("unable to seek to end in change tracking '%s' file : %m",
								ChangeTracking_FtypeToString(ftype))));
				break;
			}

			/*
			 * If file didn't exist and was created by call
			 * ChangeTracking_OpenFile above or empty.
			 */
			if (position == 0)
				break;

			/*
			 * CT files are always written in term of CHANGETRACKING_BLCKSZ,
			 * so while reading must have it aligned to same. If not something
			 * went missing or is extra in file and hence treat it as
			 * corruption and act accordingly.
			 */
			if (position % CHANGETRACKING_BLCKSZ)
			{
				ereport(WARNING,
						(errcode(ERRCODE_DATA_CORRUPTED),
						 errmsg("changetracking log (CTF_LOG_FULL) not CHANGETRACKING_BLCKSZ aligned, disabling changetracking"),
						 errdetail("CTF_LOG_FULL file size:%lu",
								   position)));
				ChangeTracking_CloseFile(file);
				file = 0;

				/*
				 * Marks segment state as CT disabled and deletes all the CT
				 * files
				 */
				ChangeTracking_MarkFullResyncLockAcquired();
				retval = false;
				break;
			}

			numBlocks = position / CHANGETRACKING_BLCKSZ;
			position = (numBlocks - 1) * CHANGETRACKING_BLCKSZ;
			FileSeek(file, position, SEEK_SET);
		}
		else
		{
			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("unable to open change tracking '%s' file : %m",
							ChangeTracking_FtypeToString(ftype))));
			break;
		}

		buf = MemoryContextAlloc(TopMemoryContext, CHANGETRACKING_BLCKSZ);
		if (buf == NULL)
		{
			ChangeTracking_CloseFile(file);
			LWLockRelease(ChangeTrackingWriteLock);

			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 (errmsg("could not allocate memory for change tracking log buffer"))));
		}

		MemSet(buf, 0, CHANGETRACKING_BLCKSZ);

		errno = 0;
		nbytes = FileRead(file, buf, CHANGETRACKING_BLCKSZ);

		if (nbytes == CHANGETRACKING_BLCKSZ)
		{
			ChangeTrackingPageHeader *header;
			ChangeTrackingRecord *record;
			char	   *bufTemp = buf;
			pg_crc32	read_checksum;
			pg_crc32	calc_checksum;

			header = (ChangeTrackingPageHeader *) bufTemp;
			read_checksum = header->checksum;
			header->checksum = 0;
			INIT_CRC32C(calc_checksum);
			COMP_CRC32C(calc_checksum, buf, CHANGETRACKING_BLCKSZ);
			FIN_CRC32C(calc_checksum);
			if (read_checksum != calc_checksum)
			{
				ereport(WARNING,
						(errcode(ERRCODE_DATA_CORRUPTED),
						 errmsg("changetracking log (CTF_LOG_FULL) corrupted, disabling changetracking"),
						 errdetail("checksum mismatch read:0x%08X compute:0x%08X",
								   read_checksum, calc_checksum)));
				ChangeTracking_CloseFile(file);
				file = 0;

				/*
				 * Marks segment state as CT disabled and deletes all the CT
				 * files
				 */
				ChangeTracking_MarkFullResyncLockAcquired();
				retval = false;
				break;
			}

			bufTemp += sizeof(ChangeTrackingPageHeader) + sizeof(ChangeTrackingRecord) * (header->numrecords - 1);
			record = (ChangeTrackingRecord *) bufTemp;
			*lastChangeTrackingLogEndLoc = record->xlogLocation;
		}
		else
		{
			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("unable to read change tracking '%s' file (read only %d bytes) : %m",
							ChangeTracking_FtypeToString(ftype), nbytes)));
			ChangeTracking_CloseFile(file);
			file = 0;
			/* Marks segment state as CT disabled and deletes all the CT files */
			ChangeTracking_MarkFullResyncLockAcquired();
			retval = false;
		}
		break;
	}

	if (file)
	{
		ChangeTracking_CloseFile(file);
	}

	LWLockRelease(ChangeTrackingWriteLock);

	if (buf)
	{
		pfree(buf);
	}

	return retval;
}

/*
 * Any RM will not change more than 5 blocks per xlog record. The only exception
 * is a GIST RM SPLIT xlog record, which has an undefined number of blocks to change.
 * This function will return the maximum number of change infos that could occur, so
 * that we could set the array size accordingly.
 */
int
ChangeTracking_GetInfoArrayDesiredMaxLength(RmgrId rmid, uint8 info)
{
	int			MaxRelChangeInfoReturns = 5;

	/* TODO:this is some sort of a very large guess.check if realistic */
	int			MaxRelChangeInfoReturns_GistSplit = 1024;

	bool		gist_split = ((rmid == RM_GIST_ID && (info & ~XLR_INFO_MASK) == XLOG_GIST_PAGE_SPLIT));
	int			arrLen = (!gist_split ? MaxRelChangeInfoReturns : MaxRelChangeInfoReturns_GistSplit);

	return arrLen;
}

static void
ChangeTracking_HandleWriteError(CTFType ft)
{
	ChangeTracking_MarkFullResyncLockAcquired();
}

char *
ChangeTracking_FtypeToString(CTFType ftype)
{
	switch (ftype)
	{
		case CTF_LOG_FULL:
			return "full log";
		case CTF_LOG_COMPACT:
			return "compact log";
		case CTF_LOG_TRANSIENT:
			return "transient log";
		case CTF_META:
			return "meta";
		default:
			elog(ERROR, "internal error in ChangeTracking_FtypeToString. invalid arg value (%d)", ftype);
			return NULL;
	}
}
