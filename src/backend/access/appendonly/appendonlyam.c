/*-------------------------------------------------------------------------
 *
 * appendonlyam.c
 *	  append-only relation access method code
 *
 * Portions Copyright (c) 1996-2006, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 * Portions Copyright (c) 2008-2009, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/appendonly/appendonlyam.c
 *
 *
 * INTERFACE ROUTINES
 *		appendonly_beginscan		- begin relation scan
 *		appendonly_rescan			- restart a relation scan
 *		appendonly_endscan			- end relation scan
 *		appendonly_getnext			- retrieve next tuple in scan
 *		appendonly_insert_init		- initialize an insert operation
 *		appendonly_insert			- insert tuple into a relation
 *		appendonly_insert_finish	- finish an insert operation
 *
 * NOTES
 *	  This file contains the appendonly_ routines which implement
 *	  the access methods used for all append-only relations.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/aosegfiles.h"
#include "access/appendonlytid.h"
#include "access/appendonlywriter.h"
#include "access/aomd.h"
#include "access/transam.h"
#include "access/tupdesc.h"
#include "access/tuptoaster.h"
#include "access/valid.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/gp_fastsequence.h"
#include "catalog/namespace.h"
#include "catalog/pg_appendonly.h"
#include "catalog/pg_appendonly_fn.h"
#include "catalog/pg_attribute_encoding.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbappendonlystorage.h"
#include "cdb/cdbappendonlystorageformat.h"
#include "cdb/cdbappendonlystoragelayer.h"
#include "cdb/cdbvars.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "utils/datum.h"
#include "utils/faultinjector.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"

#define SCANNED_SEGNO  \
	(&scan->aos_segfile_arr[ \
		(scan->aos_segfiles_processed == 0 ? 0 : scan->aos_segfiles_processed - 1) \
		])->segno

/*
 * AppendOnlyDeleteDescData is used for delete data from append-only
 * relations. It serves an equivalent purpose as AppendOnlyScanDescData
 * (relscan.h) only that the later is used for scanning append-only
 * relations.
 */
typedef struct AppendOnlyDeleteDescData
{
	/*
	 * Relation to delete from
	 */
	Relation	aod_rel;

	/*
	 * Snapshot to use for meta data operations
	 */
	Snapshot	appendOnlyMetaDataSnapshot;

	/*
	 * visibility map
	 */
	AppendOnlyVisimap visibilityMap;

	/*
	 * Visimap delete support structure. Used to handle out-of-order deletes
	 */
	AppendOnlyVisimapDelete visiMapDelete;

}			AppendOnlyDeleteDescData;

/*
 * AppendOnlyUpdateDescData is used to update data from append-only
 * relations. It serves an equivalent purpose as AppendOnlyScanDescData
 * (relscan.h) only that the later is used for scanning append-only
 * relations.
 */
typedef struct AppendOnlyUpdateDescData
{
	AppendOnlyInsertDesc aoInsertDesc;

	AppendOnlyVisimap visibilityMap;

	/*
	 * Visimap delete support structure. Used to handle out-of-order deletes
	 */
	AppendOnlyVisimapDelete visiMapDelete;

}			AppendOnlyUpdateDescData;

typedef enum AoExecutorBlockKind
{
	AoExecutorBlockKind_None = 0,
	AoExecutorBlockKind_VarBlock,
	AoExecutorBlockKind_SingleRow,
	MaxAoExecutorBlockKind		/* must always be last */
}			AoExecutorBlockKind;

static void AppendOnlyExecutionReadBlock_SetSegmentFileNum(
											   AppendOnlyExecutorReadBlock *executorReadBlock,
											   int segmentFileNum);

static void AppendOnlyExecutionReadBlock_SetPositionInfo(
											 AppendOnlyExecutorReadBlock *executorReadBlock,
											 int64 blockFirstRowNum);

static void AppendOnlyExecutorReadBlock_Init(
								 AppendOnlyExecutorReadBlock *executorReadBlock,
								 Relation relation,
								 MemoryContext memoryContext,
								 AppendOnlyStorageRead *storageRead,
								 int32 usableBlockSize);

static void AppendOnlyExecutorReadBlock_Finish(
								   AppendOnlyExecutorReadBlock *executorReadBlock);

static void AppendOnlyExecutorReadBlock_ResetCounts(
										AppendOnlyExecutorReadBlock *executorReadBlock);

/* ----------------
 *		initscan - scan code common to appendonly_beginscan and appendonly_rescan
 * ----------------
 */
static void
initscan(AppendOnlyScanDesc scan, ScanKey key)
{
	/*
	 * copy the scan key, if appropriate
	 */
	if (key != NULL)
		memcpy(scan->aos_key, key, scan->aos_nkeys * sizeof(ScanKeyData));

	scan->aos_filenamepath[0] = '\0';
	scan->aos_segfiles_processed = 0;
	scan->aos_need_new_segfile = true;	/* need to assign a file to be scanned */
	scan->aos_done_all_segfiles = false;
	scan->bufferDone = true;

	if (scan->initedStorageRoutines)
		AppendOnlyExecutorReadBlock_ResetCounts(
												&scan->executorReadBlock);

	pgstat_count_heap_scan(scan->aos_rd);
}

/*
 * Open the next file segment to scan and allocate all resources needed for it.
 */
static bool
SetNextFileSegForRead(AppendOnlyScanDesc scan)
{
	Relation	reln = scan->aos_rd;
	int			segno = -1;
	int64		eof = 0;
	int			formatversion = -1;
	bool		finished_all_files = true;	/* assume */
	int32		fileSegNo;

	Assert(scan->aos_need_new_segfile); /* only call me when last segfile
										 * completed */
	Assert(!scan->aos_done_all_segfiles);	/* don't call me if I told you to
											 * stop */


	if (!scan->initedStorageRoutines)
	{
		PGFunction *fns = NULL;

		AppendOnlyStorageRead_Init(
								   &scan->storageRead,
								   scan->aoScanInitContext,
								   scan->usableBlockSize,
								   NameStr(scan->aos_rd->rd_rel->relname),
								   scan->title,
								   &scan->storageAttributes);

		/*
		 * There is no guarantee that the current memory context will be
		 * preserved between calls, so switch to a safe memory context for
		 * retrieving compression information.
		 */
		MemoryContext oldMemoryContext = MemoryContextSwitchTo(scan->aoScanInitContext);

		/* Get the relation specific compression functions */

		fns = get_funcs_for_compression(NameStr(reln->rd_appendonly->compresstype));
		scan->storageRead.compression_functions = fns;

		if (scan->storageRead.compression_functions != NULL)
		{
			PGFunction	cons = fns[COMPRESSION_CONSTRUCTOR];
			CompressionState *cs;
			StorageAttributes sa;

			sa.comptype = scan->storageAttributes.compressType;
			sa.complevel = scan->storageAttributes.compressLevel;
			sa.blocksize = scan->usableBlockSize;

			/*
			 * The relation's tuple descriptor allows the compression
			 * constructor to make decisions about how to compress or
			 * decompress the relation given it's structure.
			 */
			cs = callCompressionConstructor(cons,
											RelationGetDescr(reln),
											&sa,
											false /* decompress */ );
			scan->storageRead.compressionState = cs;
		}

		/* Switch back to caller's memory context. */
		MemoryContextSwitchTo(oldMemoryContext);

		AppendOnlyExecutorReadBlock_Init(
										 &scan->executorReadBlock,
										 scan->aos_rd,
										 scan->aoScanInitContext,
										 &scan->storageRead,
										 scan->usableBlockSize);

		scan->bufferDone = true;	/* so we read a new buffer right away */

		scan->initedStorageRoutines = true;
	}

	/*
	 * Do we have more segment files to read or are we done?
	 */
	while (scan->aos_segfiles_processed < scan->aos_total_segfiles)
	{
		/* still have more segment files to read. get info of the next one */
		FileSegInfo *fsinfo = scan->aos_segfile_arr[scan->aos_segfiles_processed];

		segno = fsinfo->segno;
		formatversion = fsinfo->formatversion;
		eof = (int64) fsinfo->eof;

		scan->aos_segfiles_processed++;

		/*
		 * special case: we are the QD reading from an AO table in utility
		 * mode. We see entries in the aoseg table but no files or
		 * data actually exist. If we try to open this file we'll get an
		 * error, so we must skip to the next. For now, we can test if the
		 * file exists by looking at the eof value - it's always 0 on the QD.
		 */
		if (eof > 0 && fsinfo->state != AOSEG_STATE_AWAITING_DROP)
		{
			/* Initialize the block directory for inserts if needed. */
			if (scan->blockDirectory)
			{
				/*
				 * if building the block directory, we need to make sure the
				 * sequence starts higher than our highest tuple's rownum.  In
				 * the case of upgraded blocks, the highest tuple will have
				 * tupCount as its row num for non-upgrade cases, which use
				 * the sequence, it will be enough to start off the end of the
				 * sequence; note that this is not ideal -- if we are at least
				 * curSegInfo->tupcount + 1 then we don't even need to update
				 * the sequence value.
				 */
				int64		firstSequence =
				GetFastSequences(reln->rd_appendonly->segrelid,
								 segno,
								 fsinfo->total_tupcount + 1,
								 NUM_FAST_SEQUENCES);

				AppendOnlyBlockDirectory_Init_forInsert(scan->blockDirectory,
														scan->appendOnlyMetaDataSnapshot,
														fsinfo,
														0,	/* lastSequence */
														scan->aos_rd,
														segno,	/* segno */
														1,	/* columnGroupNo */
														false);

				InsertFastSequenceEntry(reln->rd_appendonly->segrelid,
										segno,
										firstSequence);
			}

			finished_all_files = false;
			break;
		}
	}

	if (finished_all_files)
	{
		/* finished reading all segment files */
		scan->aos_need_new_segfile = false;
		scan->aos_done_all_segfiles = true;

		return false;
	}

	MakeAOSegmentFileName(reln, segno, -1, &fileSegNo, scan->aos_filenamepath);
	Assert(strlen(scan->aos_filenamepath) + 1 <= scan->aos_filenamepath_maxlen);

	Assert(scan->initedStorageRoutines);

	AppendOnlyStorageRead_OpenFile(
								   &scan->storageRead,
								   scan->aos_filenamepath,
								   formatversion,
								   eof);

	AppendOnlyExecutionReadBlock_SetSegmentFileNum(
												   &scan->executorReadBlock,
												   segno);

	AppendOnlyExecutionReadBlock_SetPositionInfo(
												 &scan->executorReadBlock,
												  /* blockFirstRowNum */ 1);

	/* ready to go! */
	scan->aos_need_new_segfile = false;


	elogif(Debug_appendonly_print_scan, LOG,
		   "Append-only scan initialize for table '%s', %u/%u/%u, segment file %u, EOF " INT64_FORMAT ", "
		   "(compression = %s, usable blocksize %d)",
		   NameStr(scan->aos_rd->rd_rel->relname),
		   scan->aos_rd->rd_node.spcNode,
		   scan->aos_rd->rd_node.dbNode,
		   scan->aos_rd->rd_node.relNode,
		   segno,
		   eof,
		   (scan->storageAttributes.compress ? "true" : "false"),
		   scan->usableBlockSize);


	return true;
}

/*
 * errcontext_appendonly_insert_block_user_limit
 *
 * Add an errcontext() line showing the table name but little else because this is a user
 * caused error.
 */
static int
errcontext_appendonly_insert_block_user_limit(AppendOnlyInsertDesc aoInsertDesc)
{
	char	   *relationName = NameStr(aoInsertDesc->aoi_rel->rd_rel->relname);

	errcontext("Append-Only table '%s'", relationName);

	return 0;
}


/*
 * errcontext_appendonly_insert_block
 *
 * Add an errcontext() line showing the table, segment file, offset in file, block count of the block being inserted.
 */
static int
errcontext_appendonly_insert_block(AppendOnlyInsertDesc aoInsertDesc)
{
	char	   *relationName = NameStr(aoInsertDesc->aoi_rel->rd_rel->relname);
	int			segmentFileNum = aoInsertDesc->cur_segno;
	int64		headerOffsetInFile = AppendOnlyStorageWrite_CurrentPosition(&aoInsertDesc->storageWrite);
	int64		blockFirstRowNum = aoInsertDesc->blockFirstRowNum;
	int64		bufferCount = aoInsertDesc->bufferCount;

	errcontext("Append-Only table '%s', segment file #%d, block header offset in file = " INT64_FORMAT ", "
			   "block first row number " INT64_FORMAT ", bufferCount " INT64_FORMAT ")",
			   relationName,
			   segmentFileNum,
			   headerOffsetInFile,
			   blockFirstRowNum,
			   bufferCount);

	return 0;
}

/*
 * errdetail_appendonly_insert_block_header
 *
 * Add an errdetail() line showing the Append-Only Storage block header for the block being inserted.
 */
static int
errdetail_appendonly_insert_block_header(AppendOnlyInsertDesc aoInsertDesc)
{
	uint8	   *header;

	bool		usingChecksum;

	header = AppendOnlyStorageWrite_GetCurrentInternalBuffer(&aoInsertDesc->storageWrite);

	usingChecksum = aoInsertDesc->usingChecksum;

	return errdetail_appendonly_storage_content_header(header, usingChecksum, aoInsertDesc->storageWrite.formatVersion);
}

/*
 * Open the next file segment for write.
 */
static void
SetCurrentFileSegForWrite(AppendOnlyInsertDesc aoInsertDesc)
{
	FileSegInfo *fsinfo;
	int64		eof;
	int64		eof_uncompressed;
	int64		varblockcount;
	int32		fileSegNo;

	/* Make the 'segment' file name */
	MakeAOSegmentFileName(aoInsertDesc->aoi_rel,
						  aoInsertDesc->cur_segno, -1,
						  &fileSegNo,
						  aoInsertDesc->appendFilePathName);
	Assert(strlen(aoInsertDesc->appendFilePathName) + 1 <= aoInsertDesc->appendFilePathNameMaxLen);

	/*
	 * In order to append to this file segment entry we must first acquire the
	 * relation Append-Only segment file (transaction-scope) lock (tag
	 * LOCKTAG_RELATION_APPENDONLY_SEGMENT_FILE) in order to guarantee
	 * stability of the pg_aoseg information on this segment file and
	 * exclusive right to append data to the segment file.
	 *
	 * NOTE: This is a transaction scope lock that must be held until commit /
	 * abort.
	 */
	LockRelationAppendOnlySegmentFile(&aoInsertDesc->aoi_rel->rd_node,
									  aoInsertDesc->cur_segno,
									  AccessExclusiveLock,
									   /* dontWait */ false);

	/*
	 * Now, get the information for the file segment we are going to append
	 * to.
	 */
	aoInsertDesc->fsInfo = GetFileSegInfo(aoInsertDesc->aoi_rel,
										  aoInsertDesc->appendOnlyMetaDataSnapshot,
										  aoInsertDesc->cur_segno);

	if (aoInsertDesc->fsInfo == NULL)
	{
		InsertInitialSegnoEntry(aoInsertDesc->aoi_rel, aoInsertDesc->cur_segno);
		aoInsertDesc->fsInfo = NewFileSegInfo(aoInsertDesc->cur_segno);
	}

	/* Never insert into a segment that is awaiting a drop */
	Assert(aoInsertDesc->fsInfo->state != AOSEG_STATE_AWAITING_DROP);

	fsinfo = aoInsertDesc->fsInfo;
	Assert(fsinfo);
	eof = (int64) fsinfo->eof;
	eof_uncompressed = (int64) fsinfo->eof_uncompressed;
	varblockcount = (int64) fsinfo->varblockcount;
	aoInsertDesc->rowCount = fsinfo->total_tupcount;

	/*
	 * Segment file #0 is created when the Append-Only table is created.
	 *
	 * Other segment files are created on-demand under transaction.
	 */
	if (aoInsertDesc->cur_segno > 0 && eof == 0)
	{
		AppendOnlyStorageWrite_TransactionCreateFile(&aoInsertDesc->storageWrite,
													 aoInsertDesc->appendFilePathName,
													 &aoInsertDesc->aoi_rel->rd_node,
													 aoInsertDesc->cur_segno);
	}

	/*
	 * Open the existing file for write.
	 */
	AppendOnlyStorageWrite_OpenFile(&aoInsertDesc->storageWrite,
									aoInsertDesc->appendFilePathName,
									aoInsertDesc->fsInfo->formatversion,
									eof,
									eof_uncompressed,
									&aoInsertDesc->aoi_rel->rd_node,
									aoInsertDesc->cur_segno);

	/* reset counts */
	aoInsertDesc->insertCount = 0;
	aoInsertDesc->varblockCount = 0;

	/*
	 * Use the current block count from the segfile info so our system log
	 * error messages are accurate.
	 */
	aoInsertDesc->bufferCount = varblockcount;
}

/*
 * Finished scanning this file segment. Close it.
 */
static void
CloseScannedFileSeg(AppendOnlyScanDesc scan)
{
	AppendOnlyStorageRead_CloseFile(&scan->storageRead);

	scan->aos_need_new_segfile = true;
}

/*
 * Finished writing to this file segment. Update catalog and close file.
 */
static void
CloseWritableFileSeg(AppendOnlyInsertDesc aoInsertDesc)
{
	int64		fileLen;
	int64		fileLen_uncompressed;

	AppendOnlyStorageWrite_TransactionFlushAndCloseFile(&aoInsertDesc->storageWrite,
														&fileLen,
														&fileLen_uncompressed);

	/*
	 * Update the AO segment info table with our new eof
	 */
	UpdateFileSegInfo(aoInsertDesc->aoi_rel,
					  aoInsertDesc->cur_segno,
					  fileLen,
					  fileLen_uncompressed,
					  aoInsertDesc->insertCount,
					  aoInsertDesc->varblockCount,
					  1,
					  AOSEG_STATE_USECURRENT);

	pfree(aoInsertDesc->fsInfo);
	aoInsertDesc->fsInfo = NULL;

	elogif(Debug_appendonly_print_insert, LOG,
		   "Append-only scan closed write file segment #%d for table %s "
		   "(file length " INT64_FORMAT ", insert count %f, VarBlock count %f",
		   aoInsertDesc->cur_segno,
		   NameStr(aoInsertDesc->aoi_rel->rd_rel->relname),
		   fileLen,
		   aoInsertDesc->insertCount,
		   aoInsertDesc->varblockCount);

}

/* ------------------------------------------------------------------------------ */

static void
AppendOnlyExecutorReadBlock_GetContents(AppendOnlyExecutorReadBlock *executorReadBlock)
{
	VarBlockCheckError varBlockCheckError;

	if (!executorReadBlock->isCompressed)
	{
		if (!executorReadBlock->isLarge)
		{
			/*
			 * Small content.
			 */
			executorReadBlock->dataBuffer =
				AppendOnlyStorageRead_GetBuffer(executorReadBlock->storageRead);

			elogif(Debug_appendonly_print_scan, LOG,
				   "Append-only scan read small non-compressed block for table '%s' "
				   "(length = %d, segment file '%s', block offset in file = " INT64_FORMAT ")",
				   AppendOnlyStorageRead_RelationName(executorReadBlock->storageRead),
				   executorReadBlock->dataLen,
				   AppendOnlyStorageRead_SegmentFileName(executorReadBlock->storageRead),
				   executorReadBlock->headerOffsetInFile);
		}
		else
		{
			/*
			 * Large row.
			 */

			/* UNDONE: Error out if NOTOAST isn't ON. */

			/* UNDONE: Error out if it is not a single row */
			Assert(executorReadBlock->executorBlockKind == AoExecutorBlockKind_SingleRow);

			/*
			 * Enough room in our private buffer? UNDONE: Is there a way to
			 * avoid the 2nd copy later doProcessTuple?
			 */
			if (executorReadBlock->largeContentBufferLen < executorReadBlock->dataLen)
			{
				MemoryContext oldMemoryContext;

				/*
				 * Buffer too small.
				 */
				oldMemoryContext =
					MemoryContextSwitchTo(executorReadBlock->memoryContext);

				if (executorReadBlock->largeContentBuffer != NULL)
				{
					/*
					 * Make sure we set the our pointer to NULL here in case
					 * the subsequent allocation fails.  Otherwise cleanup
					 * will get confused.
					 */
					pfree(executorReadBlock->largeContentBuffer);
					executorReadBlock->largeContentBuffer = NULL;
				}

				executorReadBlock->largeContentBuffer = (uint8 *) palloc(executorReadBlock->dataLen);
				executorReadBlock->largeContentBufferLen = executorReadBlock->dataLen;

				/*
				 * Deallocation and allocation done.  Go back to caller
				 * memory-context.
				 */
				MemoryContextSwitchTo(oldMemoryContext);
			}

			executorReadBlock->dataBuffer = executorReadBlock->largeContentBuffer;

			AppendOnlyStorageRead_Content(executorReadBlock->storageRead,
										  executorReadBlock->dataBuffer,
										  executorReadBlock->dataLen);

			elogif(Debug_appendonly_print_scan, LOG,
				   "Append-only scan read large row for table '%s' "
				   "(length = %d, segment file '%s', "
				   "block offset in file = " INT64_FORMAT ")",
				   AppendOnlyStorageRead_RelationName(executorReadBlock->storageRead),
				   executorReadBlock->dataLen,
				   AppendOnlyStorageRead_SegmentFileName(executorReadBlock->storageRead),
				   executorReadBlock->headerOffsetInFile);
		}
	}
	else
	{
		int32		compressedLen =
		AppendOnlyStorageRead_CurrentCompressedLen(executorReadBlock->storageRead);

		/*
		 * AppendOnlyStorageWrite does not report compressed for large content
		 * metadata.
		 */
		Assert(!executorReadBlock->isLarge);

		/*
		 * Decompress into our temporary buffer.
		 */
		executorReadBlock->dataBuffer = executorReadBlock->uncompressedBuffer;

		AppendOnlyStorageRead_Content(executorReadBlock->storageRead,
									  executorReadBlock->dataBuffer,
									  executorReadBlock->dataLen);

		elogif(Debug_appendonly_print_scan, LOG,
			   "Append-only scan read decompressed block for table '%s' "
			   "(compressed length %d, length = %d, segment file '%s', "
			   "block offset in file = " INT64_FORMAT ")",
			   AppendOnlyStorageRead_RelationName(executorReadBlock->storageRead),
			   compressedLen,
			   executorReadBlock->dataLen,
			   AppendOnlyStorageRead_SegmentFileName(executorReadBlock->storageRead),
			   executorReadBlock->headerOffsetInFile);
	}

	/*
	 * The executorBlockKind value is what the executor -- i.e. the upper part
	 * of this appendonlyam module! -- has stored in the Append-Only Storage
	 * header.  We interpret it here.
	 */

	switch (executorReadBlock->executorBlockKind)
	{
		case AoExecutorBlockKind_VarBlock:
			varBlockCheckError = VarBlockIsValid(executorReadBlock->dataBuffer, executorReadBlock->dataLen);
			if (varBlockCheckError != VarBlockCheckOk)
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("VarBlock  is not valid. "
								"Valid block check error %d, detail '%s'",
								varBlockCheckError,
								VarBlockGetCheckErrorStr()),
						 errdetail_appendonly_read_storage_content_header(executorReadBlock->storageRead),
						 errcontext_appendonly_read_storage_block(executorReadBlock->storageRead)));

			/*
			 * Now use the VarBlock module to extract the items out.
			 */
			VarBlockReaderInit(&executorReadBlock->varBlockReader,
							   executorReadBlock->dataBuffer,
							   executorReadBlock->dataLen);

			executorReadBlock->readerItemCount = VarBlockReaderItemCount(&executorReadBlock->varBlockReader);

			executorReadBlock->currentItemCount = 0;

			if (executorReadBlock->rowCount != executorReadBlock->readerItemCount)
			{
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("Row count %d in append-only storage header does not match VarBlock item count %d",
								executorReadBlock->rowCount,
								executorReadBlock->readerItemCount),
						 errdetail_appendonly_read_storage_content_header(executorReadBlock->storageRead),
						 errcontext_appendonly_read_storage_block(executorReadBlock->storageRead)));
			}

			elogif(Debug_appendonly_print_scan, LOG,
				   "Append-only scan read VarBlock for table '%s' with %d items (block offset in file = " INT64_FORMAT ")",
				   AppendOnlyStorageRead_RelationName(executorReadBlock->storageRead),
				   executorReadBlock->readerItemCount,
				   executorReadBlock->headerOffsetInFile);
			break;

		case AoExecutorBlockKind_SingleRow:
			if (executorReadBlock->rowCount != 1)
			{
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("Row count %d in append-only storage header is not 1 for single row",
								executorReadBlock->rowCount),
						 errdetail_appendonly_read_storage_content_header(executorReadBlock->storageRead),
						 errcontext_appendonly_read_storage_block(executorReadBlock->storageRead)));
			}
			executorReadBlock->singleRow = executorReadBlock->dataBuffer;
			executorReadBlock->singleRowLen = executorReadBlock->dataLen;

			elogif(Debug_appendonly_print_scan, LOG, "Append-only scan read single row for table '%s' with length %d (block offset in file = " INT64_FORMAT ")",
				   AppendOnlyStorageRead_RelationName(executorReadBlock->storageRead),
				   executorReadBlock->singleRowLen,
				   executorReadBlock->headerOffsetInFile);

			break;

		default:
			elog(ERROR, "Unrecognized append-only executor block kind: %d",
				 executorReadBlock->executorBlockKind);
			break;
	}
}

static bool
AppendOnlyExecutorReadBlock_GetBlockInfo(AppendOnlyStorageRead *storageRead,
										 AppendOnlyExecutorReadBlock *executorReadBlock)
{
	int64		blockFirstRowNum = executorReadBlock->blockFirstRowNum;

	if (!AppendOnlyStorageRead_GetBlockInfo(storageRead,
											&executorReadBlock->dataLen,
											&executorReadBlock->executorBlockKind,
											&executorReadBlock->blockFirstRowNum,
											&executorReadBlock->rowCount,
											&executorReadBlock->isLarge,
											&executorReadBlock->isCompressed))
	{
		return false;
	}

	/*
	 * If the firstRowNum is not stored in the AOBlock,
	 * executorReadBlock->blockFirstRowNum is set to -1. Since this is
	 * properly updated by calling functions
	 * AppendOnlyExecutionReadBlock_SetPositionInfo and
	 * AppendOnlyExecutionReadBlock_FinishedScanBlock, we restore the last
	 * value when the block does not contain firstRowNum.
	 */
	if (executorReadBlock->blockFirstRowNum < 0)
	{
		executorReadBlock->blockFirstRowNum = blockFirstRowNum;
	}

	executorReadBlock->headerOffsetInFile =
		AppendOnlyStorageRead_CurrentHeaderOffsetInFile(storageRead);

	/* UNDONE: Check blockFirstRowNum */

	return true;
}

static void
AppendOnlyExecutionReadBlock_SetSegmentFileNum(AppendOnlyExecutorReadBlock *executorReadBlock,
											   int segmentFileNum)
{
	executorReadBlock->segmentFileNum = segmentFileNum;
}

static void
AppendOnlyExecutionReadBlock_SetPositionInfo(AppendOnlyExecutorReadBlock *executorReadBlock,
											 int64 blockFirstRowNum)
{
	executorReadBlock->blockFirstRowNum = blockFirstRowNum;
}

static void
AppendOnlyExecutionReadBlock_FinishedScanBlock(AppendOnlyExecutorReadBlock *executorReadBlock)
{
	executorReadBlock->blockFirstRowNum += executorReadBlock->rowCount;
}

/*
 * Initialize the ExecutorReadBlock once.  Assumed to be zeroed out before the call.
 */
static void
AppendOnlyExecutorReadBlock_Init(AppendOnlyExecutorReadBlock *executorReadBlock,
								 Relation relation,
								 MemoryContext memoryContext,
								 AppendOnlyStorageRead *storageRead,
								 int32 usableBlockSize)
{
	MemoryContext oldcontext;

	oldcontext = MemoryContextSwitchTo(memoryContext);
	executorReadBlock->uncompressedBuffer = (uint8 *) palloc(usableBlockSize * sizeof(uint8));

	executorReadBlock->storageRead = storageRead;

	MemoryContextSwitchTo(oldcontext);
}

/*
 * Free the space allocated inside ExexcutorReadBlock.
 */
static void
AppendOnlyExecutorReadBlock_Finish(AppendOnlyExecutorReadBlock *executorReadBlock)
{
	if (executorReadBlock->uncompressedBuffer)
	{
		pfree(executorReadBlock->uncompressedBuffer);
		executorReadBlock->uncompressedBuffer = NULL;
	}

	if (executorReadBlock->numericAtts)
	{
		pfree(executorReadBlock->numericAtts);
		executorReadBlock->numericAtts = NULL;
	}
}

static void
AppendOnlyExecutorReadBlock_ResetCounts(AppendOnlyExecutorReadBlock *executorReadBlock)
{
	executorReadBlock->totalRowsScannned = 0;
}

/*
 * Given a tuple in 'formatversion', convert it to a format that is
 * understood by the rest of the system.
 */
static MemTuple
upgrade_tuple(AppendOnlyExecutorReadBlock *executorReadBlock,
			  MemTuple mtup, MemTupleBinding *pbind, int formatversion, bool *shouldFree)
{
	TupleDesc	tupdesc = pbind->tupdesc;
	const int	natts = tupdesc->natts;
	MemTuple	newtuple;
	int			i;

	static Datum *values = NULL;
	static bool *isnull = NULL;
	static int	nallocated = 0;

	bool		convert_alignment = false;
	bool		convert_numerics = false;

	/*
	 * MPP-7372: If the AO table was created before the fix for this issue, it
	 * may contain tuples with misaligned bindings. Here we check if the
	 * stored memtuple is problematic and then create a clone of the tuple
	 * with properly aligned bindings to be used by the executor.
	 */
	if (formatversion < AORelationVersion_Aligned64bit &&
		memtuple_has_misaligned_attribute(mtup, pbind))
		convert_alignment = true;

	if (PG82NumericConversionNeeded(formatversion))
	{
		/*
		 * On first call, figure out which columns are numerics, or domains
		 * over numerics.
		 */
		if (executorReadBlock->numericAtts == NULL)
		{
			int			n;

			executorReadBlock->numericAtts = (int *) palloc(natts * sizeof(int));

			n = 0;
			for (i = 0; i < natts; i++)
			{
				Oid			typeoid;

				typeoid = getBaseType(tupdesc->attrs[i]->atttypid);
				if (typeoid == NUMERICOID)
					executorReadBlock->numericAtts[n++] = i;
			}
			executorReadBlock->numNumericAtts = n;
		}

		/* If there were any numeric columns, we need to conver them. */
		if (executorReadBlock->numNumericAtts > 0)
			convert_numerics = true;
	}

	if (!convert_alignment && !convert_numerics)
	{
		/* No conversion required. Return the original tuple unmodified. */
		*shouldFree = false;
		return mtup;
	}

	/* Conversion is needed. */

	/* enlarge the arrays if needed */
	if (natts > nallocated)
	{
		if (values)
			pfree(values);
		if (isnull)
			pfree(values);
		values = (Datum *) MemoryContextAlloc(TopMemoryContext, natts * sizeof(Datum));
		isnull = (bool *) MemoryContextAlloc(TopMemoryContext, natts * sizeof(bool));
		nallocated = natts;
	}

	if (convert_alignment)
	{
		/* get attribute values form mis-aligned tuple */
		memtuple_deform_misaligned(mtup, pbind, values, isnull);
		/* Form a new, properly-aligned, tuple */
		newtuple = memtuple_form_to(pbind, values, isnull, NULL, NULL, true);
	}
	else
	{
		/*
		 * make a modifiable copy
		 */
		newtuple = memtuple_copy_to(mtup, NULL, NULL);
	}

	/*
	 * NOTE: we do this *after* creating the new tuple, so that we can modify
	 * the new, copied, tuple in-place.
	 */
	if (convert_numerics)
	{
		int			i;

		/*
		 * Get pointers to the datums within the tuple
		 */
		memtuple_deform(newtuple, pbind, values, isnull);

		for (i = 0; i < executorReadBlock->numNumericAtts; i++)
		{
			/*
			 * Before PostgreSQL 8.3, the n_weight and n_sign_dscale fields
			 * were the other way 'round. Swap them.
			 */
			Datum		datum;
			char	   *numericdata;
			uint16		tmp;

			if (isnull[executorReadBlock->numericAtts[i]])
				continue;

			datum = values[executorReadBlock->numericAtts[i]];
			numericdata = VARDATA_ANY(DatumGetPointer(datum));

			memcpy(&tmp, &numericdata[0], 2);
			memcpy(&numericdata[0], &numericdata[2], 2);
			memcpy(&numericdata[2], &tmp, 2);
		}
	}

	*shouldFree = true;
	return newtuple;
}

static bool
AppendOnlyExecutorReadBlock_ProcessTuple(AppendOnlyExecutorReadBlock *executorReadBlock,
										 int64 rowNum,
										 MemTuple tuple,
										 int32 tupleLen,
										 int nkeys,
										 ScanKey key,
										 TupleTableSlot *slot)
{
	bool		valid = true;

	/* Assume for HeapKeyTestUsingSlot define. */
	ItemPointerData fake_ctid;
	AOTupleId  *aoTupleId = (AOTupleId *) &fake_ctid;
	int			formatVersion = executorReadBlock->storageRead->formatVersion;

	AORelationVersion_CheckValid(formatVersion);

	AOTupleIdInit_Init(aoTupleId);
	AOTupleIdInit_segmentFileNum(aoTupleId, executorReadBlock->segmentFileNum);
	AOTupleIdInit_rowNum(aoTupleId, rowNum);

	if (slot)
	{
		bool		shouldFree = false;

		/* If the tuple is not in the latest format, convert it */
		if (formatVersion < AORelationVersion_GetLatest())
			tuple = upgrade_tuple(executorReadBlock, tuple, slot->tts_mt_bind, formatVersion, &shouldFree);
		ExecStoreMinimalTuple(tuple, slot, shouldFree);
		slot_set_ctid(slot, &fake_ctid);
	}


	/* skip visibility test, all tuples are visible */

	if (key != NULL)
		HeapKeyTestUsingSlot(slot, nkeys, key, valid);

	elogif(Debug_appendonly_print_scan_tuple && valid, LOG,
		   "Append-only scan tuple for table '%s' "
		   "(AOTupleId %s, tuple length %d, memtuple length %d, block offset in file " INT64_FORMAT ")",
		   AppendOnlyStorageRead_RelationName(executorReadBlock->storageRead),
		   AOTupleIdToString(aoTupleId),
		   tupleLen,
		   memtuple_get_size(tuple),
		   executorReadBlock->headerOffsetInFile);

	return valid;
}

static MemTuple
AppendOnlyExecutorReadBlock_ScanNextTuple(AppendOnlyExecutorReadBlock *executorReadBlock,
										  int nkeys,
										  ScanKey key,
										  TupleTableSlot *slot)
{
	MemTuple	tuple;

	Assert(slot);

	switch (executorReadBlock->executorBlockKind)
	{
		case AoExecutorBlockKind_VarBlock:

			/*
			 * get the next item (tuple) from the varblock
			 */
			while (true)
			{
				int			itemLen = 0;
				uint8	   *itemPtr;
				int64		rowNum;

				itemPtr = VarBlockReaderGetNextItemPtr(
													   &executorReadBlock->varBlockReader,
													   &itemLen);

				if (itemPtr == NULL)
				{
					/* no more items in the varblock, get new buffer */
					AppendOnlyExecutionReadBlock_FinishedScanBlock(
																   executorReadBlock);
					return NULL;
				}

				executorReadBlock->currentItemCount++;

				executorReadBlock->totalRowsScannned++;

				if (itemLen > 0)
				{
					tuple = (MemTuple) itemPtr;

					rowNum = executorReadBlock->blockFirstRowNum +
						executorReadBlock->currentItemCount - INT64CONST(1);

					if (AppendOnlyExecutorReadBlock_ProcessTuple(
																 executorReadBlock,
																 rowNum,
																 tuple,
																 itemLen,
																 nkeys,
																 key,
																 slot))
						return TupGetMemTuple(slot);
				}

			}

			/* varblock sanity check */
			if (executorReadBlock->readerItemCount !=
				executorReadBlock->currentItemCount)
				elog(NOTICE, "Varblock mismatch: Reader count %d, found %d items\n",
					 executorReadBlock->readerItemCount,
					 executorReadBlock->currentItemCount);
			break;

		case AoExecutorBlockKind_SingleRow:
			{
				int32		singleRowLen;

				if (executorReadBlock->singleRow == NULL)
				{
					AppendOnlyExecutionReadBlock_FinishedScanBlock(
																   executorReadBlock);
					return NULL;
					/* Force fetching new block. */
				}

				Assert(executorReadBlock->singleRowLen != 0);

				tuple = (MemTuple) executorReadBlock->singleRow;
				singleRowLen = executorReadBlock->singleRowLen;

				/*
				 * Indicated used up for scan.
				 */
				executorReadBlock->singleRow = NULL;
				executorReadBlock->singleRowLen = 0;

				executorReadBlock->totalRowsScannned++;

				if (AppendOnlyExecutorReadBlock_ProcessTuple(
															 executorReadBlock,
															 executorReadBlock->blockFirstRowNum,
															 tuple,
															 singleRowLen,
															 nkeys,
															 key,
															 slot))
					return TupGetMemTuple(slot);
			}
			break;

		default:
			elog(ERROR, "Unrecognized append-only executor block kind: %d",
				 executorReadBlock->executorBlockKind);
			break;
	}

	AppendOnlyExecutionReadBlock_FinishedScanBlock(
												   executorReadBlock);
	return NULL;
	/* No match. */
}

static bool
AppendOnlyExecutorReadBlock_FetchTuple(AppendOnlyExecutorReadBlock *executorReadBlock,
									   int64 rowNum,
									   int nkeys,
									   ScanKey key,
									   TupleTableSlot *slot)
{
	MemTuple	tuple;
	int			itemNum;

	Assert(rowNum >= executorReadBlock->blockFirstRowNum);
	Assert(rowNum <=
		   executorReadBlock->blockFirstRowNum +
		   executorReadBlock->rowCount - 1);

	/*
	 * Get 0-based index to tuple.
	 */
	itemNum =
		(int) (rowNum - executorReadBlock->blockFirstRowNum);

	switch (executorReadBlock->executorBlockKind)
	{
		case AoExecutorBlockKind_VarBlock:
			{
				uint8	   *itemPtr;
				int			itemLen;

				itemPtr = VarBlockReaderGetItemPtr(
												   &executorReadBlock->varBlockReader,
												   itemNum,
												   &itemLen);
				Assert(itemPtr != NULL);

				tuple = (MemTuple) itemPtr;

				if (AppendOnlyExecutorReadBlock_ProcessTuple(
															 executorReadBlock,
															 rowNum,
															 tuple,
															 itemLen,
															 nkeys,
															 key,
															 slot))
					return true;
			}
			break;

		case AoExecutorBlockKind_SingleRow:
			{
				Assert(itemNum == 0);
				Assert(executorReadBlock->singleRow != NULL);
				Assert(executorReadBlock->singleRowLen != 0);

				tuple = (MemTuple) executorReadBlock->singleRow;

				if (AppendOnlyExecutorReadBlock_ProcessTuple(
															 executorReadBlock,
															 rowNum,
															 tuple,
															 executorReadBlock->singleRowLen,
															 nkeys,
															 key,
															 slot))
					return true;
			}
			break;

		default:
			elog(ERROR, "Unrecognized append-only executor block kind: %d",
				 executorReadBlock->executorBlockKind);
			break;
	}

	return false;
	/* No match. */
}

/* ------------------------------------------------------------------------------ */

/*
 * You can think of this scan routine as get next "executor" AO block.
 */
static bool
getNextBlock(AppendOnlyScanDesc scan)
{
	if (scan->aos_need_new_segfile)
	{
		/*
		 * Need to open a new segment file.
		 */
		if (!SetNextFileSegForRead(scan))
			return false;
	}

	if (!AppendOnlyExecutorReadBlock_GetBlockInfo(
												  &scan->storageRead,
												  &scan->executorReadBlock))
	{
		if (scan->blockDirectory)
		{
			AppendOnlyBlockDirectory_End_forInsert(scan->blockDirectory);
		}

		/* done reading the file */
		CloseScannedFileSeg(scan);

		return false;
	}

	if (scan->blockDirectory)
	{
		AppendOnlyBlockDirectory_InsertEntry(
											 scan->blockDirectory, 0,
											 scan->executorReadBlock.blockFirstRowNum,
											 scan->executorReadBlock.headerOffsetInFile,
											 scan->executorReadBlock.rowCount,
											 false);
	}

	AppendOnlyExecutorReadBlock_GetContents(
											&scan->executorReadBlock);

	return true;
}

/* ----------------
 *		appendonlygettup - fetch next appendonly tuple
 *
 *		Initialize the scan if not already done; then advance to the next
 *		tuple in forward direction; return the next tuple in scan->aos_ctup,
 *		or set scan->aos_ctup.t_data = NULL if no more tuples.
 *
 * Note: the reason nkeys/key are passed separately, even though they are
 * kept in the scan descriptor, is that the caller may not want us to check
 * the scankeys.
 * ----------------
 */
static MemTuple
appendonlygettup(AppendOnlyScanDesc scan,
				 ScanDirection dir __attribute__((unused)),
				 int nkeys,
				 ScanKey key,
				 TupleTableSlot *slot)
{
	MemTuple	tuple;

	Assert(ScanDirectionIsForward(dir));
	Assert(scan->usableBlockSize > 0);

	bool		isSnapshotAny = (scan->snapshot == SnapshotAny);

	for (;;)
	{
		if (scan->bufferDone)
		{
			/*
			 * Get the next block. We call this function until we successfully
			 * get a block to process, or finished reading all the data (all
			 * 'segment' files) for this relation.
			 */
			while (!getNextBlock(scan))
			{
				/* have we read all this relation's data. done! */
				if (scan->aos_done_all_segfiles)
					return NULL;
			}

			scan->bufferDone = false;
		}

		tuple = AppendOnlyExecutorReadBlock_ScanNextTuple(
														  &scan->executorReadBlock,
														  nkeys,
														  key,
														  slot);
		if (tuple != NULL)
		{

			/*
			 * Need to get the Block Directory entry that covers the TID.
			 */
			AOTupleId  *aoTupleId = (AOTupleId *) slot_get_ctid(slot);

			if (!isSnapshotAny && !AppendOnlyVisimap_IsVisible(&scan->visibilityMap, aoTupleId))
			{
				/*
				 * The tuple is invisible.
				 */
			}
			else
			{
				/* The tuple is visible */
				return tuple;
			}
		}
		else
		{
			/* no more items in the varblock, get new buffer */
			scan->bufferDone = true;
		}

	}

}

static void
cancelLastBuffer(AppendOnlyInsertDesc aoInsertDesc)
{
	if (aoInsertDesc->nonCompressedData != NULL)
	{
		Assert(AppendOnlyStorageWrite_IsBufferAllocated(&aoInsertDesc->storageWrite));
		AppendOnlyStorageWrite_CancelLastBuffer(&aoInsertDesc->storageWrite);
		aoInsertDesc->nonCompressedData = NULL;
	}
	else
		Assert(!AppendOnlyStorageWrite_IsBufferAllocated(&aoInsertDesc->storageWrite));
}

static void
setupNextWriteBlock(AppendOnlyInsertDesc aoInsertDesc)
{
	Assert(aoInsertDesc->nonCompressedData == NULL);
	Assert(!AppendOnlyStorageWrite_IsBufferAllocated(&aoInsertDesc->storageWrite));

	/* Set the firstRowNum for the block */
	aoInsertDesc->blockFirstRowNum = aoInsertDesc->lastSequence + 1;
	AppendOnlyStorageWrite_SetFirstRowNum(&aoInsertDesc->storageWrite,
										  aoInsertDesc->blockFirstRowNum);

	if (!aoInsertDesc->shouldCompress)
	{
		aoInsertDesc->nonCompressedData =
			AppendOnlyStorageWrite_GetBuffer(
											 &aoInsertDesc->storageWrite,
											 AoHeaderKind_SmallContent);

		/*
		 * Prepare our VarBlock for items.  Leave room for the Append-Only
		 * Storage header.
		 */
		VarBlockMakerInit(&aoInsertDesc->varBlockMaker,
						  aoInsertDesc->nonCompressedData,
						  aoInsertDesc->maxDataLen,
						  aoInsertDesc->tempSpace,
						  aoInsertDesc->tempSpaceLen);

	}
	else
	{
		/*
		 * Block oriented compression.  We also restrict the size of the
		 * buffer to leave room for the Append-Only Storage header in case the
		 * block cannot be compressed by the compress library.
		 */
		VarBlockMakerInit(&aoInsertDesc->varBlockMaker,
						  aoInsertDesc->uncompressedBuffer,
						  aoInsertDesc->maxDataLen,
						  aoInsertDesc->tempSpace,
						  aoInsertDesc->tempSpaceLen);
	}

	aoInsertDesc->bufferCount++;
}

static void
finishWriteBlock(AppendOnlyInsertDesc aoInsertDesc)
{
	int			executorBlockKind;
	int			itemCount;
	int32		dataLen;

	executorBlockKind = AoExecutorBlockKind_VarBlock;
	/* Assume. */

	itemCount = VarBlockMakerItemCount(&aoInsertDesc->varBlockMaker);
	if (itemCount == 0)
	{
		/*
		 * "Cancel" the last block allocation, if one.
		 */
		cancelLastBuffer(aoInsertDesc);
		return;
	}

	dataLen = VarBlockMakerFinish(&aoInsertDesc->varBlockMaker);

	aoInsertDesc->varblockCount++;

	if (!aoInsertDesc->shouldCompress)
	{
		if (itemCount == 1)
		{
			dataLen = VarBlockCollapseToSingleItem(
												    /* target */ aoInsertDesc->nonCompressedData,
												    /* source */ aoInsertDesc->nonCompressedData,
												    /* sourceLen */ dataLen);
			executorBlockKind = AoExecutorBlockKind_SingleRow;
		}

		aoInsertDesc->storageWrite.logicalBlockStartOffset =
			BufferedAppendNextBufferPosition(&(aoInsertDesc->storageWrite.bufferedAppend));

		AppendOnlyStorageWrite_FinishBuffer(
											&aoInsertDesc->storageWrite,
											dataLen,
											executorBlockKind,
											itemCount);
		aoInsertDesc->nonCompressedData = NULL;
		Assert(!AppendOnlyStorageWrite_IsBufferAllocated(&aoInsertDesc->storageWrite));

		elogif(Debug_appendonly_print_insert, LOG,
			   "Append-only insert finished uncompressed block for table '%s' "
			   "(length = %d, application specific %d, item count %d, block count " INT64_FORMAT ")",
			   NameStr(aoInsertDesc->aoi_rel->rd_rel->relname),
			   dataLen,
			   executorBlockKind,
			   itemCount,
			   aoInsertDesc->bufferCount);
	}
	else
	{
		if (itemCount == 1)
		{
			dataLen = VarBlockCollapseToSingleItem(
												    /* target */ aoInsertDesc->uncompressedBuffer,
												    /* source */ aoInsertDesc->uncompressedBuffer,
												    /* sourceLen */ dataLen);
			executorBlockKind = AoExecutorBlockKind_SingleRow;
		}
		else
		{
			Assert(executorBlockKind == AoExecutorBlockKind_VarBlock);

			/*
			 * Just before finishing the attempting to compress the VarBlock,
			 * let's verify the VarBlock has integrity, honor, etc.
			 */
			if (gp_appendonly_verify_write_block)
			{
				VarBlockCheckError varBlockCheckError;

				varBlockCheckError = VarBlockIsValid(aoInsertDesc->uncompressedBuffer, dataLen);
				if (varBlockCheckError != VarBlockCheckOk)
					ereport(ERROR,
							(errcode(ERRCODE_INTERNAL_ERROR),
							 errmsg("Verify block during write found VarBlock is not valid. "
									"Valid block check error %d, detail '%s'",
									varBlockCheckError,
									VarBlockGetCheckErrorStr()),
							 errdetail_appendonly_insert_block_header(aoInsertDesc),
							 errcontext_appendonly_insert_block(aoInsertDesc)));
			}
		}

		AppendOnlyStorageWrite_Content(
									   &aoInsertDesc->storageWrite,
									   aoInsertDesc->uncompressedBuffer,
									   dataLen,
									   executorBlockKind,
									   itemCount);
	}

	/* Insert an entry to the block directory */
	AppendOnlyBlockDirectory_InsertEntry(
										 &aoInsertDesc->blockDirectory,
										 0,
										 aoInsertDesc->blockFirstRowNum,
										 AppendOnlyStorageWrite_LogicalBlockStartOffset(&aoInsertDesc->storageWrite),
										 itemCount,
										 false);

	Assert(aoInsertDesc->nonCompressedData == NULL);
	Assert(!AppendOnlyStorageWrite_IsBufferAllocated(&aoInsertDesc->storageWrite));
}

/* ----------------------------------------------------------------
 *					 append-only access method interface
 * ----------------------------------------------------------------
 */

/*
 * appendonly_beginrangescan_internal()
 *
 * Begins a scan over a subset of segment info files.
 *
 * Should only be called with valid seginfos for the given relation.
 * Should only be called with an aoentry based on the same snapshot.
 *
 * The ownership of the seginfos and aoentry are transferred to the scan descriptor.
 */
static AppendOnlyScanDesc
appendonly_beginrangescan_internal(Relation relation,
								   Snapshot snapshot,
								   Snapshot appendOnlyMetaDataSnapshot,
								   FileSegInfo **seginfo,
								   int segfile_count,
								   int nkeys,
								   ScanKey key)
{
	AppendOnlyScanDesc scan;

	AppendOnlyStorageAttributes *attr;

	StringInfoData titleBuf;

	/*
	 * increment relation ref count while scanning relation
	 *
	 * This is just to make really sure the relcache entry won't go away while
	 * the scan has a pointer to it.  Caller should be holding the rel open
	 * anyway, so this is redundant in all normal scenarios...
	 */
	RelationIncrementReferenceCount(relation);

	/*
	 * allocate scan descriptor
	 */
	scan = (AppendOnlyScanDesc) palloc0(sizeof(AppendOnlyScanDescData));

	/*
	 * initialize the scan descriptor
	 */
	scan->aos_filenamepath_maxlen = AOSegmentFilePathNameLen(relation) + 1;
	scan->aos_filenamepath = (char *) palloc(scan->aos_filenamepath_maxlen);
	scan->aos_filenamepath[0] = '\0';
	scan->usableBlockSize = AppendOnlyStorage_GetUsableBlockSize(relation->rd_appendonly->blocksize);
	scan->aos_rd = relation;
	scan->appendOnlyMetaDataSnapshot = appendOnlyMetaDataSnapshot;
	scan->snapshot = snapshot;
	scan->aos_nkeys = nkeys;
	scan->aoScanInitContext = CurrentMemoryContext;

	initStringInfo(&titleBuf);
	appendStringInfo(&titleBuf, "Scan of Append-Only Row-Oriented relation '%s'",
					 RelationGetRelationName(relation));
	scan->title = titleBuf.data;


	/*
	 * Fill in Append-Only Storage layer attributes.
	 */
	attr = &scan->storageAttributes;

	/*
	 * These attributes describe the AppendOnly format to be scanned.
	 */
	if (strcmp(NameStr(relation->rd_appendonly->compresstype), "") == 0 ||
		pg_strcasecmp(NameStr(relation->rd_appendonly->compresstype), "none") == 0)
	{
		attr->compress = false;
		attr->compressType = "none";
	}
	else
	{
		attr->compress = true;
		attr->compressType = pstrdup(NameStr(relation->rd_appendonly->compresstype));
	}
	attr->compressLevel = relation->rd_appendonly->compresslevel;
	attr->checksum = relation->rd_appendonly->checksum;
	attr->safeFSWriteSize = relation->rd_appendonly->safefswritesize;

	/* UNDONE: We are calling the static header length routine here. */
	scan->maxDataLen =
		scan->usableBlockSize -
		AppendOnlyStorageFormat_RegularHeaderLenNeeded(scan->storageAttributes.checksum);


	/*
	 * Get information about all the file segments we need to scan
	 */
	scan->aos_segfile_arr = seginfo;

	scan->aos_total_segfiles = segfile_count;

	/*
	 * we do this here instead of in initscan() because appendonly_rescan also
	 * calls initscan() and we don't want to allocate memory again
	 */
	if (nkeys > 0)
		scan->aos_key = (ScanKey) palloc(sizeof(ScanKeyData) * nkeys);
	else
		scan->aos_key = NULL;

	/* pgstat_initstats(relation); */
	initscan(scan, key);

	scan->blockDirectory = NULL;

	AppendOnlyVisimap_Init(&scan->visibilityMap,
						   relation->rd_appendonly->visimaprelid,
						   relation->rd_appendonly->visimapidxid,
						   AccessShareLock,
						   appendOnlyMetaDataSnapshot);

	return scan;
}

/*
 * appendonly_beginrangescan
 *
 * begins range-limited relation scan
 */
AppendOnlyScanDesc
appendonly_beginrangescan(Relation relation,
						  Snapshot snapshot,
						  Snapshot appendOnlyMetaDataSnapshot,
						  int *segfile_no_arr, int segfile_count,
						  int nkeys, ScanKey keys)
{
	/*
	 * Get the pg_appendonly information for this table
	 */

	FileSegInfo **seginfo = palloc0(sizeof(FileSegInfo *) * segfile_count);
	int			i;

	for (i = 0; i < segfile_count; i++)
	{
		seginfo[	i] = GetFileSegInfo(relation, appendOnlyMetaDataSnapshot,
										segfile_no_arr[i]);
	}
	return appendonly_beginrangescan_internal(relation,
											  snapshot,
											  appendOnlyMetaDataSnapshot,
											  seginfo,
											  segfile_count,
											  nkeys,
											  keys);
}

/* ----------------
 *		appendonly_beginscan	- begin relation scan
 * ----------------
 */
AppendOnlyScanDesc
appendonly_beginscan(Relation relation,
					 Snapshot snapshot,
					 Snapshot appendOnlyMetaDataSnapshot,
					 int nkeys, ScanKey keys)
{
	/*
	 * Get the pg_appendonly information for this table
	 */
	int			segfile_count;
	FileSegInfo **seginfo;

	seginfo = GetAllFileSegInfo(relation,
								appendOnlyMetaDataSnapshot, &segfile_count);

	return appendonly_beginrangescan_internal(relation,
											  snapshot,
											  appendOnlyMetaDataSnapshot,
											  seginfo,
											  segfile_count,
											  nkeys,
											  keys);
}

/* ----------------
 *		appendonly_rescan		- restart a relation scan
 *
 *
 * TODO: instead of freeing resources here and reallocating them in initscan
 * over and over see which of them can be refactored into appendonly_beginscan
 * and persist there until endscan is finally reached. For now this will do.
 * ----------------
 */
void
appendonly_rescan(AppendOnlyScanDesc scan,
				  ScanKey key)
{
	CloseScannedFileSeg(scan);

	AppendOnlyStorageRead_FinishSession(&scan->storageRead);

	scan->initedStorageRoutines = false;

	AppendOnlyExecutorReadBlock_Finish(&scan->executorReadBlock);

	scan->aos_need_new_segfile = true;

	/*
	 * reinitialize scan descriptor
	 */
	initscan(scan, key);
}

/* ----------------
 *		appendonly_endscan	- end relation scan
 * ----------------
 */
void
appendonly_endscan(AppendOnlyScanDesc scan)
{
	RelationDecrementReferenceCount(scan->aos_rd);

	if (scan->aos_key)
		pfree(scan->aos_key);

	if (scan->aos_segfile_arr)
	{
		for (int seginfo_no = 0; seginfo_no < scan->aos_total_segfiles; seginfo_no++)
		{
			pfree(scan->aos_segfile_arr[seginfo_no]);
		}

		pfree(scan->aos_segfile_arr);
	}

	CloseScannedFileSeg(scan);

	AppendOnlyStorageRead_FinishSession(&scan->storageRead);

	scan->initedStorageRoutines = false;

	AppendOnlyExecutorReadBlock_Finish(&scan->executorReadBlock);

	AppendOnlyVisimap_Finish(&scan->visibilityMap, AccessShareLock);
	pfree(scan->aos_filenamepath);

	pfree(scan->title);

	pfree(scan);
}

/* ----------------
 *		appendonly_getnext	- retrieve next tuple in scan
 * ----------------
 */
MemTuple
appendonly_getnext(AppendOnlyScanDesc scan, ScanDirection direction, TupleTableSlot *slot)
{
	MemTuple	tup = appendonlygettup(scan, direction, scan->aos_nkeys, scan->aos_key, slot);

	if (tup == NULL)
	{
		if (slot)
			ExecClearTuple(slot);

		return NULL;
	}

	pgstat_count_heap_getnext(scan->aos_rd);

	return tup;
}

static void
closeFetchSegmentFile(AppendOnlyFetchDesc aoFetchDesc)
{
	Assert(aoFetchDesc->currentSegmentFile.isOpen);

	AppendOnlyStorageRead_CloseFile(&aoFetchDesc->storageRead);

	aoFetchDesc->currentSegmentFile.isOpen = false;
}

static bool
openFetchSegmentFile(AppendOnlyFetchDesc aoFetchDesc,
					 int openSegmentFileNum)
{
	int			i;

	FileSegInfo *fsInfo;
	int			segmentFileNum;
	int64		logicalEof;
	int32		fileSegNo;

	Assert(!aoFetchDesc->currentSegmentFile.isOpen);

	i = 0;
	while (true)
	{
		if (i >= aoFetchDesc->totalSegfiles)
			return false;
		/* Segment file not visible in catalog information. */

		fsInfo = aoFetchDesc->segmentFileInfo[i];
		segmentFileNum = fsInfo->segno;
		if (openSegmentFileNum == segmentFileNum)
		{
			if (fsInfo->state == AOSEG_STATE_AWAITING_DROP)
			{
				/*
				 * File compacted, but not dropped. All information are
				 * declared invisible
				 */
				return false;
			}
			logicalEof = (int64) fsInfo->eof;
			break;
		}
		i++;
	}

	/*
	 * Don't try to open a segment file when its EOF is 0, since the file may
	 * not exist. See MPP-8280.
	 */
	if (logicalEof == 0)
		return false;

	MakeAOSegmentFileName(
						  aoFetchDesc->relation,
						  openSegmentFileNum, -1,
						  &fileSegNo,
						  aoFetchDesc->segmentFileName);
	Assert(strlen(aoFetchDesc->segmentFileName) + 1 <=
		   aoFetchDesc->segmentFileNameMaxLen);

	/* UNDONE: Appropriate to use Try here? */
	if (!AppendOnlyStorageRead_TryOpenFile(
										   &aoFetchDesc->storageRead,
										   aoFetchDesc->segmentFileName,
										   fsInfo->formatversion,
										   logicalEof))
		return false;

	aoFetchDesc->currentSegmentFile.num = openSegmentFileNum;
	aoFetchDesc->currentSegmentFile.logicalEof = logicalEof;

	aoFetchDesc->currentSegmentFile.isOpen = true;

	return true;
}

static bool
fetchNextBlock(AppendOnlyFetchDesc aoFetchDesc)
{
	AppendOnlyExecutorReadBlock *executorReadBlock =
	&aoFetchDesc->executorReadBlock;

	/*
	 * Try to read next block.
	 */
	if (!AppendOnlyExecutorReadBlock_GetBlockInfo(
												  &aoFetchDesc->storageRead,
												  &aoFetchDesc->executorReadBlock))
		return false;
	/* Hit end of range. */

	/*
	 * Unpack information into member variables.
	 */
	aoFetchDesc->currentBlock.have = true;
	aoFetchDesc->currentBlock.fileOffset =
		executorReadBlock->headerOffsetInFile;
	aoFetchDesc->currentBlock.overallBlockLen =
		AppendOnlyStorageRead_OverallBlockLen(
											  &aoFetchDesc->storageRead);
	aoFetchDesc->currentBlock.firstRowNum =
		executorReadBlock->blockFirstRowNum;
	aoFetchDesc->currentBlock.lastRowNum =
		executorReadBlock->blockFirstRowNum +
		executorReadBlock->rowCount - 1;

	aoFetchDesc->currentBlock.isCompressed =
		executorReadBlock->isCompressed;
	aoFetchDesc->currentBlock.isLargeContent =
		executorReadBlock->isLarge;

	aoFetchDesc->currentBlock.gotContents = false;

	return true;
}

static bool
fetchFromCurrentBlock(AppendOnlyFetchDesc aoFetchDesc,
					  int64 rowNum,
					  TupleTableSlot *slot)
{
	Assert(aoFetchDesc->currentBlock.have);
	Assert(rowNum >= aoFetchDesc->currentBlock.firstRowNum);
	Assert(rowNum <= aoFetchDesc->currentBlock.lastRowNum);

	if (!aoFetchDesc->currentBlock.gotContents)
	{
		/*
		 * Do decompression if necessary and get contents.
		 */
		AppendOnlyExecutorReadBlock_GetContents(&aoFetchDesc->executorReadBlock);

		aoFetchDesc->currentBlock.gotContents = true;
	}

	return AppendOnlyExecutorReadBlock_FetchTuple(&aoFetchDesc->executorReadBlock,
												  rowNum,
												   /* nkeys */ 0,
												   /* key */ NULL,
												  slot);
}

static void
positionFirstBlockOfRange(AppendOnlyFetchDesc aoFetchDesc)
{
	AppendOnlyBlockDirectoryEntry_GetBeginRange(&aoFetchDesc->currentBlock.blockDirectoryEntry,
												&aoFetchDesc->scanNextFileOffset,
												&aoFetchDesc->scanNextRowNum);
}

static void
positionLimitToEndOfRange(AppendOnlyFetchDesc aoFetchDesc)
{
	AppendOnlyBlockDirectoryEntry_GetEndRange(&aoFetchDesc->currentBlock.blockDirectoryEntry,
											  &aoFetchDesc->scanAfterFileOffset,
											  &aoFetchDesc->scanLastRowNum);
}


static void
positionSkipCurrentBlock(AppendOnlyFetchDesc aoFetchDesc)
{
	aoFetchDesc->scanNextFileOffset =
		aoFetchDesc->currentBlock.fileOffset +
		aoFetchDesc->currentBlock.overallBlockLen;

	aoFetchDesc->scanNextRowNum = aoFetchDesc->currentBlock.lastRowNum + 1;
}

/*
 * Scan through blocks to find row.
 *
 * If row is not represented in any of the blocks covered by the Block Directory, then the row
 * falls into a row gap.  The row must have been aborted or deleted and reclaimed.
 */
static bool
scanToFetchTuple(AppendOnlyFetchDesc aoFetchDesc,
				 int64 rowNum,
				 TupleTableSlot *slot)
{
	if (aoFetchDesc->scanNextFileOffset >=
		aoFetchDesc->scanAfterFileOffset)
		return false;
	/* No more blocks requested for range. */

	if (aoFetchDesc->currentSegmentFile.logicalEof ==
		aoFetchDesc->scanNextFileOffset)
		return false;
	/* No more blocks in this file. */

	if (aoFetchDesc->currentSegmentFile.logicalEof <
		aoFetchDesc->scanNextFileOffset)
		return false;
/* UNDONE:Why does our next scan position go beyond logical EOF ? */

	/*
	 * Temporarily restrict our reading to just the range.
	 */
	AppendOnlyStorageRead_SetTemporaryRange(
											&aoFetchDesc->storageRead,
											aoFetchDesc->scanNextFileOffset,
											aoFetchDesc->scanAfterFileOffset);
	AppendOnlyExecutionReadBlock_SetSegmentFileNum(
												   &aoFetchDesc->executorReadBlock,
												   aoFetchDesc->currentSegmentFile.num);
	AppendOnlyExecutionReadBlock_SetPositionInfo(
												 &aoFetchDesc->executorReadBlock,
												 aoFetchDesc->scanNextRowNum);

	aoFetchDesc->skipBlockCount = 0;
	while (true)
	{
		/*
		 * Fetch block starting at scanNextFileOffset.
		 */
		if (!fetchNextBlock(aoFetchDesc))
			return false;
		/* No more blocks. */

		/*
		 * Examine new current block header information.
		 */
		if (rowNum < aoFetchDesc->currentBlock.firstRowNum)
		{
			/*
			 * Since we have read a new block, the temporary range for the
			 * read needs to be adjusted accordingly. Otherwise, the
			 * underlying bufferedRead may stop reading more data because of
			 * the previously-set smaller temporary range.
			 */
			int64		beginFileOffset = aoFetchDesc->currentBlock.fileOffset;
			int64		afterFileOffset = aoFetchDesc->currentBlock.fileOffset +
			aoFetchDesc->currentBlock.overallBlockLen;

			AppendOnlyStorageRead_SetTemporaryRange(
													&aoFetchDesc->storageRead,
													beginFileOffset,
													afterFileOffset);

			return false;
			/* Row fell in gap between blocks. */
		}

		if (rowNum <= aoFetchDesc->currentBlock.lastRowNum)
			return fetchFromCurrentBlock(aoFetchDesc, rowNum, slot);

		/*
		 * Update information to get next block.
		 */
		Assert(!aoFetchDesc->currentBlock.gotContents);

		/* MPP-17061: reach the end of range covered by block directory entry */
		if ((aoFetchDesc->currentBlock.fileOffset +
			 aoFetchDesc->currentBlock.overallBlockLen) >=
			aoFetchDesc->scanAfterFileOffset)
		{
			return false;
		}

		AppendOnlyExecutionReadBlock_FinishedScanBlock(
													   &aoFetchDesc->executorReadBlock);

		AppendOnlyStorageRead_SkipCurrentBlock(
											   &aoFetchDesc->storageRead);
		aoFetchDesc->skipBlockCount++;
	}
}


AppendOnlyFetchDesc
appendonly_fetch_init(Relation relation,
					  Snapshot snapshot,
					  Snapshot appendOnlyMetaDataSnapshot)
{
	AppendOnlyFetchDesc aoFetchDesc;

	AppendOnlyStorageAttributes *attr;

	PGFunction *fns;

	StringInfoData titleBuf;

	/*
	 * increment relation ref count while scanning relation
	 *
	 * This is just to make really sure the relcache entry won't go away while
	 * the scan has a pointer to it.  Caller should be holding the rel open
	 * anyway, so this is redundant in all normal scenarios...
	 */
	RelationIncrementReferenceCount(relation);

	/*
	 * allocate scan descriptor
	 */
	aoFetchDesc = (AppendOnlyFetchDesc) palloc0(sizeof(AppendOnlyFetchDescData));

	aoFetchDesc->relation = relation;
	aoFetchDesc->appendOnlyMetaDataSnapshot = appendOnlyMetaDataSnapshot;
	aoFetchDesc->snapshot = snapshot;

	aoFetchDesc->initContext = CurrentMemoryContext;

	aoFetchDesc->segmentFileNameMaxLen = AOSegmentFilePathNameLen(relation) + 1;
	aoFetchDesc->segmentFileName =
		(char *) palloc(aoFetchDesc->segmentFileNameMaxLen);
	aoFetchDesc->segmentFileName[0] = '\0';

	initStringInfo(&titleBuf);
	appendStringInfo(&titleBuf, "Fetch of Append-Only Row-Oriented relation '%s'",
					 RelationGetRelationName(relation));
	aoFetchDesc->title = titleBuf.data;

	/*
	 * Fill in Append-Only Storage layer attributes.
	 */
	attr = &aoFetchDesc->storageAttributes;

	/*
	 * These attributes describe the AppendOnly format to be scanned.
	 */
	if (strcmp(NameStr(relation->rd_appendonly->compresstype), "") == 0 ||
		pg_strcasecmp(NameStr(relation->rd_appendonly->compresstype), "none") == 0)
	{
		attr->compress = false;
		attr->compressType = "none";
	}
	else
	{
		attr->compress = true;
		attr->compressType = NameStr(relation->rd_appendonly->compresstype);
	}
	attr->compressLevel = relation->rd_appendonly->compresslevel;
	attr->checksum = relation->rd_appendonly->checksum;
	attr->safeFSWriteSize = relation->rd_appendonly->safefswritesize;

	aoFetchDesc->usableBlockSize =
		AppendOnlyStorage_GetUsableBlockSize(relation->rd_appendonly->blocksize);

	/*
	 * Get information about all the file segments we need to scan
	 */
	aoFetchDesc->segmentFileInfo =
		GetAllFileSegInfo(
						  relation,
						  appendOnlyMetaDataSnapshot,
						  &aoFetchDesc->totalSegfiles);

	AppendOnlyStorageRead_Init(
							   &aoFetchDesc->storageRead,
							   aoFetchDesc->initContext,
							   aoFetchDesc->usableBlockSize,
							   NameStr(aoFetchDesc->relation->rd_rel->relname),
							   aoFetchDesc->title,
							   &aoFetchDesc->storageAttributes);


	fns = get_funcs_for_compression(NameStr(relation->rd_appendonly->compresstype));
	aoFetchDesc->storageRead.compression_functions = fns;

	if (fns)
	{
		PGFunction	cons = fns[COMPRESSION_CONSTRUCTOR];
		CompressionState *cs;
		StorageAttributes sa;

		sa.comptype = NameStr(relation->rd_appendonly->compresstype);
		sa.complevel = relation->rd_appendonly->compresslevel;
		sa.blocksize = relation->rd_appendonly->blocksize;


		cs = callCompressionConstructor(cons, RelationGetDescr(relation),
										&sa,
										false /* decompress */ );
		aoFetchDesc->storageRead.compressionState = cs;
	}

	AppendOnlyExecutorReadBlock_Init(
									 &aoFetchDesc->executorReadBlock,
									 aoFetchDesc->relation,
									 aoFetchDesc->initContext,
									 &aoFetchDesc->storageRead,
									 aoFetchDesc->usableBlockSize);

	AppendOnlyBlockDirectory_Init_forSearch(
											&aoFetchDesc->blockDirectory,
											appendOnlyMetaDataSnapshot,
											aoFetchDesc->segmentFileInfo,
											aoFetchDesc->totalSegfiles,
											aoFetchDesc->relation,
											1,
											false,
											NULL);

	AppendOnlyVisimap_Init(&aoFetchDesc->visibilityMap,
						   relation->rd_appendonly->visimaprelid,
						   relation->rd_appendonly->visimapidxid,
						   AccessShareLock,
						   appendOnlyMetaDataSnapshot);

	return aoFetchDesc;

}

/*
 * appendonly_fetch -- fetch the tuple for a given tid.
 *
 * If the 'slot' is not NULL, the fetched tuple will be assigned to the slot.
 *
 * Return true if such a tuple is found. Otherwise, return false.
 */
bool
appendonly_fetch(AppendOnlyFetchDesc aoFetchDesc,
				 AOTupleId *aoTupleId,
				 TupleTableSlot *slot)
{
	int			segmentFileNum = AOTupleIdGet_segmentFileNum(aoTupleId);
	int64		rowNum = AOTupleIdGet_rowNum(aoTupleId);
	bool		isSnapshotAny = (aoFetchDesc->snapshot == SnapshotAny);

	/*
	 * Do we have a current block?  If it has the requested tuple, that would
	 * be a great performance optimization.
	 */
	if (aoFetchDesc->currentBlock.have)
	{
		if (segmentFileNum == aoFetchDesc->currentSegmentFile.num &&
			segmentFileNum == aoFetchDesc->blockDirectory.currentSegmentFileNum &&
			segmentFileNum == aoFetchDesc->executorReadBlock.segmentFileNum)
		{
			if (rowNum >= aoFetchDesc->currentBlock.firstRowNum &&
				rowNum <= aoFetchDesc->currentBlock.lastRowNum &&
				AppendOnlyBlockDirectoryEntry_RangeHasRow(
														  &aoFetchDesc->currentBlock.blockDirectoryEntry,
														  rowNum))
			{
				if (!isSnapshotAny && !AppendOnlyVisimap_IsVisible(&aoFetchDesc->visibilityMap, aoTupleId))
				{
					if (slot != NULL)
					{
						ExecClearTuple(slot);
					}
					return false;	/* row has been deleted or updated. */
				}
				return fetchFromCurrentBlock(aoFetchDesc, rowNum, slot);
			}

			/*
			 * Otherwize, if the current Block Directory entry covers the
			 * request tuples, lets use its information as another performance
			 * optimization.
			 */
			if (AppendOnlyBlockDirectoryEntry_RangeHasRow(
														  &aoFetchDesc->currentBlock.blockDirectoryEntry,
														  rowNum))
			{
				/*
				 * The tuple is covered by the current Block Directory entry,
				 * but is it before or after our current block?
				 */
				if (rowNum < aoFetchDesc->currentBlock.firstRowNum)
				{
					/*
					 * XXX This could happen when an insert is cancelled. In
					 * that case, we fetched the next block that has a higher
					 * firstRowNum when we try to find the first cancelled
					 * row. So for the second or any cancelled row, we enter
					 * here, and re-read the previous block. This seems
					 * inefficient.
					 *
					 * We may be able to fix this by adding an entry to the
					 * block directory for those cancelled inserts.
					 */

					/*
					 * Set scan range to prior blocks.
					 */
					positionFirstBlockOfRange(aoFetchDesc);

					/* Set limit to before current block. */
					aoFetchDesc->scanAfterFileOffset =
						aoFetchDesc->currentBlock.fileOffset;

					aoFetchDesc->scanLastRowNum =
						aoFetchDesc->currentBlock.firstRowNum - 1;
				}
				else
				{
					/*
					 * Set scan range to following blocks.
					 */
					positionSkipCurrentBlock(aoFetchDesc);

					positionLimitToEndOfRange(aoFetchDesc);
				}

				if (!isSnapshotAny && !AppendOnlyVisimap_IsVisible(&aoFetchDesc->visibilityMap, aoTupleId))
				{
					if (slot != NULL)
					{
						ExecClearTuple(slot);
					}
					return false;	/* row has been deleted or updated. */
				}

				if (scanToFetchTuple(aoFetchDesc, rowNum, slot))
					return true;

				if (slot != NULL)
					ExecClearTuple(slot);
				return false;
				/* Segment file not in aoseg table.. */
			}
		}
	}

/* 	resetCurrentBlockInfo(aoFetchDesc); */

	/*
	 * Open or switch open, if necessary.
	 */
	if (aoFetchDesc->currentSegmentFile.isOpen &&
		segmentFileNum != aoFetchDesc->currentSegmentFile.num)
	{
#ifdef USE_ASSERT_CHECKING
		if (segmentFileNum < aoFetchDesc->currentSegmentFile.num)
			ereport(WARNING,
					(errmsg("Append-only fetch requires scan prior segment file: "
							"segmentFileNum %d, rowNum " INT64_FORMAT
							", currentSegmentFileNum %d",
							segmentFileNum, rowNum, aoFetchDesc->currentSegmentFile.num)));
#endif
		closeFetchSegmentFile(aoFetchDesc);

		Assert(!aoFetchDesc->currentSegmentFile.isOpen);
	}

	if (!aoFetchDesc->currentSegmentFile.isOpen)
	{
		if (!openFetchSegmentFile(
								  aoFetchDesc,
								  segmentFileNum))
		{
			if (slot != NULL)
				ExecClearTuple(slot);
			return false;
			/* Segment file not in aoseg table.. */
			/* Must be aborted or deleted and reclaimed. */
		}
	}

	/*
	 * Need to get the Block Directory entry that covers the TID.
	 */
	if (!AppendOnlyBlockDirectory_GetEntry(
										   &aoFetchDesc->blockDirectory,
										   aoTupleId,
										   0,
										   &aoFetchDesc->currentBlock.blockDirectoryEntry))
	{
		if (slot != NULL)
		{
			ExecClearTuple(slot);
		}
		return false;			/* Row not represented in Block Directory. */
		/* Must be aborted or deleted and reclaimed. */
	}

	if (!isSnapshotAny && !AppendOnlyVisimap_IsVisible(&aoFetchDesc->visibilityMap, aoTupleId))
	{
		if (slot != NULL)
		{
			ExecClearTuple(slot);
		}
		return false;			/* row has been deleted or updated. */
	}


	/*
	 * Set scan range covered by new Block Directory entry.
	 */
	positionFirstBlockOfRange(aoFetchDesc);

	positionLimitToEndOfRange(aoFetchDesc);

	if (scanToFetchTuple(aoFetchDesc, rowNum, slot))
		return true;

	if (slot != NULL)
		ExecClearTuple(slot);
	return false;
	/* Segment file not in aoseg table.. */
}

void
appendonly_fetch_finish(AppendOnlyFetchDesc aoFetchDesc)
{
	RelationDecrementReferenceCount(aoFetchDesc->relation);

	AppendOnlyStorageRead_CloseFile(&aoFetchDesc->storageRead);

	AppendOnlyStorageRead_FinishSession(&aoFetchDesc->storageRead);

	AppendOnlyExecutorReadBlock_Finish(&aoFetchDesc->executorReadBlock);

	AppendOnlyBlockDirectory_End_forSearch(&aoFetchDesc->blockDirectory);

	if (aoFetchDesc->segmentFileInfo)
	{
		FreeAllSegFileInfo(aoFetchDesc->segmentFileInfo, aoFetchDesc->totalSegfiles);
		pfree(aoFetchDesc->segmentFileInfo);
		aoFetchDesc->segmentFileInfo = NULL;
	}

	AppendOnlyVisimap_Finish(&aoFetchDesc->visibilityMap, AccessShareLock);

	pfree(aoFetchDesc->segmentFileName);
	aoFetchDesc->segmentFileName = NULL;

	pfree(aoFetchDesc->title);
}

/*
 * appendonly_delete_init
 *
 * before using appendonly_delete() to delete tuples from append-only segment
 * files, we need to call this function to initialize the delete desc
 * data structured.
 */
AppendOnlyDeleteDesc
appendonly_delete_init(Relation rel, Snapshot appendOnlyMetaDataSnapshot)
{
	Assert(RelationIsAoRows(rel));
	Assert(!IsXactIsoLevelSerializable);

	AppendOnlyDeleteDesc aoDeleteDesc = palloc0(sizeof(AppendOnlyDeleteDescData));

	aoDeleteDesc->aod_rel = rel;
	aoDeleteDesc->appendOnlyMetaDataSnapshot = appendOnlyMetaDataSnapshot;

	AppendOnlyVisimap_Init(&aoDeleteDesc->visibilityMap,
						   rel->rd_appendonly->visimaprelid,
						   rel->rd_appendonly->visimapidxid,
						   RowExclusiveLock,
						   aoDeleteDesc->appendOnlyMetaDataSnapshot);

	AppendOnlyVisimapDelete_Init(&aoDeleteDesc->visiMapDelete,
								 &aoDeleteDesc->visibilityMap);

	return aoDeleteDesc;
}

void
appendonly_delete_finish(AppendOnlyDeleteDesc aoDeleteDesc)
{
	Assert(aoDeleteDesc);

	AppendOnlyVisimapDelete_Finish(&aoDeleteDesc->visiMapDelete);

	AppendOnlyVisimap_Finish(&aoDeleteDesc->visibilityMap, NoLock);

	pfree(aoDeleteDesc);
}

HTSU_Result
appendonly_delete(AppendOnlyDeleteDesc aoDeleteDesc,
				  AOTupleId *aoTupleId)
{
	Assert(aoDeleteDesc);
	Assert(aoTupleId);

	elogif(Debug_appendonly_print_delete, LOG,
		   "Append-only delete tuple from table '%s' (AOTupleId %s)",
		   NameStr(aoDeleteDesc->aod_rel->rd_rel->relname),
		   AOTupleIdToString(aoTupleId));

#ifdef FAULT_INJECTOR
	FaultInjector_InjectFaultIfSet(
								   AppendOnlyDelete,
								   DDLNotSpecified,
								   "", //databaseName
								   RelationGetRelationName(aoDeleteDesc->aod_rel));
	/* tableName */
#endif

	return AppendOnlyVisimapDelete_Hide(&aoDeleteDesc->visiMapDelete, aoTupleId);
}

/*
 * appendonly_update_init
 *
 * before using appendonly_update() to update tuples from append-only segment
 * files, we need to call this function to initialize the update desc
 * data structured.
 */
AppendOnlyUpdateDesc
appendonly_update_init(Relation rel, Snapshot appendOnlyMetaDataSnapshot, int segno)
{
	Assert(RelationIsAoRows(rel));
	Assert(!IsXactIsoLevelSerializable);

	/*
	 * allocate and initialize the insert descriptor
	 */
	AppendOnlyUpdateDesc aoUpdateDesc = (AppendOnlyUpdateDesc) palloc0(sizeof(AppendOnlyUpdateDescData));

	aoUpdateDesc->aoInsertDesc = appendonly_insert_init(rel, segno, true);

	AppendOnlyVisimap_Init(&aoUpdateDesc->visibilityMap,
						   aoUpdateDesc->aoInsertDesc->aoi_rel->rd_appendonly->visimaprelid,
						   aoUpdateDesc->aoInsertDesc->aoi_rel->rd_appendonly->visimapidxid,
						   RowExclusiveLock,
						   appendOnlyMetaDataSnapshot);

	AppendOnlyVisimapDelete_Init(&aoUpdateDesc->visiMapDelete,
								 &aoUpdateDesc->visibilityMap);

	return aoUpdateDesc;
}

void
appendonly_update_finish(AppendOnlyUpdateDesc aoUpdateDesc)
{
	Assert(aoUpdateDesc);

	AppendOnlyVisimapDelete_Finish(&aoUpdateDesc->visiMapDelete);

	appendonly_insert_finish(aoUpdateDesc->aoInsertDesc);
	aoUpdateDesc->aoInsertDesc = NULL;

	/* Keep lock until the end of the transaction */
	AppendOnlyVisimap_Finish(&aoUpdateDesc->visibilityMap, NoLock);

	pfree(aoUpdateDesc);
}

HTSU_Result
appendonly_update(AppendOnlyUpdateDesc aoUpdateDesc,
				  MemTuple memTuple,
				  AOTupleId *aoTupleId,
				  AOTupleId *newAoTupleId)
{
	HTSU_Result result;

	Assert(aoUpdateDesc);
	Assert(aoTupleId);

#ifdef FAULT_INJECTOR
	FaultInjector_InjectFaultIfSet(
								   AppendOnlyUpdate,
								   DDLNotSpecified,
								   "", //databaseName
								   RelationGetRelationName(aoUpdateDesc->aoInsertDesc->aoi_rel));
	/* tableName */
#endif

	result = AppendOnlyVisimapDelete_Hide(&aoUpdateDesc->visiMapDelete, aoTupleId);
	if (result != HeapTupleMayBeUpdated)
		return result;

	appendonly_insert(aoUpdateDesc->aoInsertDesc,
					  memTuple,
					  InvalidOid,	/* new oid should be old oid */
					  newAoTupleId);

	return result;
}

/*
 * appendonly_insert_init
 *
 * before using appendonly_insert() to insert tuples we need to call
 * this function to initialize our varblock and bufferedAppend structures
 * and memory for appending data into the relation file.
 *
 * see appendonly_insert() for more specifics about inserting tuples into
 * append only tables.
 */
AppendOnlyInsertDesc
appendonly_insert_init(Relation rel, int segno, bool update_mode)
{
	AppendOnlyInsertDesc aoInsertDesc;
	int			maxtupsize;
	int64		firstSequence = 0;
	PGFunction *fns;
	int			desiredOverflowBytes = 0;
	size_t		(*desiredCompressionSize) (size_t input);

	AppendOnlyStorageAttributes *attr;

	StringInfoData titleBuf;

	/*
	 * Get the pg_appendonly information for this table
	 */

	/*
	 * allocate and initialize the insert descriptor
	 */
	aoInsertDesc = (AppendOnlyInsertDesc) palloc0(sizeof(AppendOnlyInsertDescData));

	aoInsertDesc->aoi_rel = rel;

	/*
	 * Writers uses this since they have exclusive access to the lock acquired
	 * with LockRelationAppendOnlySegmentFile for the segment-file.
	 */
	aoInsertDesc->appendOnlyMetaDataSnapshot = SnapshotNow;

	aoInsertDesc->mt_bind = create_memtuple_binding(RelationGetDescr(rel));

	aoInsertDesc->appendFile = -1;
	aoInsertDesc->appendFilePathNameMaxLen = AOSegmentFilePathNameLen(rel) + 1;
	aoInsertDesc->appendFilePathName = (char *) palloc(aoInsertDesc->appendFilePathNameMaxLen);
	aoInsertDesc->appendFilePathName[0] = '\0';

	aoInsertDesc->bufferCount = 0;
	aoInsertDesc->blockFirstRowNum = 0;
	aoInsertDesc->insertCount = 0;
	aoInsertDesc->varblockCount = 0;
	aoInsertDesc->rowCount = 0;

	Assert(segno >= 0);
	aoInsertDesc->cur_segno = segno;
	aoInsertDesc->update_mode = update_mode;

	/*
	 * Adding a NOTOAST table attribute in 3.3.3 would require a catalog
	 * change, so in the interim we will test this with a GUC.
	 *
	 * This GUC must have the same value on write and read.
	 */
/* 	aoInsertDesc->useNoToast = aoentry->notoast; */
	aoInsertDesc->useNoToast = Debug_appendonly_use_no_toast;

	aoInsertDesc->usableBlockSize = AppendOnlyStorage_GetUsableBlockSize(rel->rd_appendonly->blocksize);

	attr = &aoInsertDesc->storageAttributes;

	/*
	 * These attributes describe the AppendOnly format to be scanned.
	 */
	if (strcmp(NameStr(rel->rd_appendonly->compresstype), "") == 0 ||
		pg_strcasecmp(NameStr(rel->rd_appendonly->compresstype), "none") == 0)
	{
		attr->compress = false;
		attr->compressType = "none";
	}
	else
	{
		attr->compress = true;
		attr->compressType = NameStr(rel->rd_appendonly->compresstype);
	}
	attr->compressLevel = rel->rd_appendonly->compresslevel;
	attr->checksum = rel->rd_appendonly->checksum;
	attr->safeFSWriteSize = rel->rd_appendonly->safefswritesize;

	fns = get_funcs_for_compression(NameStr(rel->rd_appendonly->compresstype));

	CompressionState *cs = NULL;
	CompressionState *verifyCs = NULL;

	if (fns)
	{
		PGFunction	cons = fns[COMPRESSION_CONSTRUCTOR];
		StorageAttributes sa;

		sa.comptype = NameStr(rel->rd_appendonly->compresstype);
		sa.complevel = rel->rd_appendonly->compresslevel;
		sa.blocksize = rel->rd_appendonly->blocksize;

		cs = callCompressionConstructor(cons, RelationGetDescr(rel),
										&sa,
										true /* compress */ );
		if (gp_appendonly_verify_write_block == true)
		{
			verifyCs = callCompressionConstructor(cons, RelationGetDescr(rel),
												  &sa,
												  false /* decompress */ );
		}

		desiredCompressionSize = cs->desired_sz;
		if (desiredCompressionSize != NULL)
		{
			/*
			 * Call the compression's desired size function to find out what
			 * additional space it requires for our block size.
			 */
			desiredOverflowBytes =
				(int) (desiredCompressionSize) (aoInsertDesc->usableBlockSize)
				- aoInsertDesc->usableBlockSize;
			Assert(desiredOverflowBytes >= 0);
		}
	}

	aoInsertDesc->storageAttributes.overflowSize = desiredOverflowBytes;

	initStringInfo(&titleBuf);
	appendStringInfo(&titleBuf, "Write of Append-Only Row-Oriented relation '%s'",
					 RelationGetRelationName(aoInsertDesc->aoi_rel));
	aoInsertDesc->title = titleBuf.data;

	AppendOnlyStorageWrite_Init(
								&aoInsertDesc->storageWrite,
								NULL,
								aoInsertDesc->usableBlockSize,
								RelationGetRelationName(aoInsertDesc->aoi_rel),
								aoInsertDesc->title,
								&aoInsertDesc->storageAttributes);

	aoInsertDesc->storageWrite.compression_functions = fns;
	aoInsertDesc->storageWrite.compressionState = cs;
	aoInsertDesc->storageWrite.verifyWriteCompressionState = verifyCs;

	elogif(Debug_appendonly_print_insert, LOG,
		   "Append-only insert initialize for table '%s' segment file %u "
		   "(compression = %s, compression type %s, compression level %d)",
		   NameStr(aoInsertDesc->aoi_rel->rd_rel->relname),
		   aoInsertDesc->cur_segno,
		   (attr->compress ? "true" : "false"),
		   NameStr(rel->rd_appendonly->compresstype),
		   attr->compressLevel);

	/*
	 * Temporarily set the firstRowNum for the block so that we can calculate
	 * the correct header length.
	 */
	AppendOnlyStorageWrite_SetFirstRowNum(&aoInsertDesc->storageWrite,
										  1);

	aoInsertDesc->completeHeaderLen =
		AppendOnlyStorageWrite_CompleteHeaderLen(
												 &aoInsertDesc->storageWrite,
												 AoHeaderKind_SmallContent);

	aoInsertDesc->maxDataLen =
		aoInsertDesc->usableBlockSize -
		aoInsertDesc->completeHeaderLen;

	aoInsertDesc->tempSpaceLen = aoInsertDesc->usableBlockSize / 8; /* TODO - come up with a
																	 * more efficient
																	 * calculation */
	aoInsertDesc->tempSpace = (uint8 *) palloc(aoInsertDesc->tempSpaceLen * sizeof(uint8));
	maxtupsize = aoInsertDesc->maxDataLen - VARBLOCK_HEADER_LEN - 4;
	aoInsertDesc->toast_tuple_threshold = maxtupsize / 4;	/* see tuptoaster.h for
															 * more information */
	aoInsertDesc->toast_tuple_target = maxtupsize / 4;

	/* open our current relation file segment for write */
	SetCurrentFileSegForWrite(aoInsertDesc);

	Assert(aoInsertDesc->tempSpaceLen > 0);

	/*
	 * Obtain the next list of fast sequences for this relation.
	 *
	 * Even in the case of no indexes, we need to update the fast sequences,
	 * since the table may contain indexes at some point of time.
	 */
	Assert(aoInsertDesc->fsInfo->segno == segno);

	firstSequence =
		GetFastSequences(aoInsertDesc->aoi_rel->rd_appendonly->segrelid,
						 segno,
						 aoInsertDesc->rowCount + 1,
						 NUM_FAST_SEQUENCES);
	aoInsertDesc->numSequences = NUM_FAST_SEQUENCES;

	/* Set last_sequence value */
	Assert(firstSequence > aoInsertDesc->rowCount);
	aoInsertDesc->lastSequence = firstSequence - 1;

	setupNextWriteBlock(aoInsertDesc);

	/* Initialize the block directory. */
	AppendOnlyBlockDirectory_Init_forInsert(
											&(aoInsertDesc->blockDirectory),
aoInsertDesc->appendOnlyMetaDataSnapshot, //CONCERN:Safe to assume all block directory entries for segment are "covered" by same exclusive lock.
											aoInsertDesc->fsInfo, aoInsertDesc->lastSequence,
											rel, segno, 1, false);

	return aoInsertDesc;
}


/*
 *	appendonly_insert		- insert tuple into a varblock
 *
 * Note the following major differences from heap_insert
 *
 * - wal is always bypassed here.
 * - transaction information is of no interest.
 * - tuples inserted into varblocks, not via the postgresql buf/page manager.
 * - no need to pin buffers.
 *
  * The output parameter tupleOid is the OID assigned to the tuple (either here or by the
  * caller), or InvalidOid if no OID.  The header fields of *tup are updated
  * to match the stored tuple;
  *
  * Unlike heap_insert(), this function doesn't scribble on the input tuple.
  */
Oid
appendonly_insert(AppendOnlyInsertDesc aoInsertDesc,
				  MemTuple instup,
				  Oid tupleOid,
				  AOTupleId *aoTupleId)
{
	Relation	relation = aoInsertDesc->aoi_rel;
	VarBlockByteLen itemLen;
	uint8	   *itemPtr;
	MemTuple	tup = NULL;
	bool		need_toast;
	bool		isLargeContent;

	Assert(aoInsertDesc->usableBlockSize > 0 && aoInsertDesc->tempSpaceLen > 0);
	Assert(aoInsertDesc->toast_tuple_threshold > 0 && aoInsertDesc->toast_tuple_target > 0);

#ifdef FAULT_INJECTOR
	FaultInjector_InjectFaultIfSet(
								   AppendOnlyInsert,
								   DDLNotSpecified,
								   "", //databaseName
								   RelationGetRelationName(aoInsertDesc->aoi_rel));
	/* tableName */
#endif

	Insist(RelationIsAoRows(relation));

	if (aoInsertDesc->useNoToast)
		need_toast = false;
	else
		need_toast = (MemTupleHasExternal(instup, aoInsertDesc->mt_bind) ||
					  memtuple_get_size(instup) > aoInsertDesc->toast_tuple_threshold);

	/*
	 * If the new tuple is too big for storage or contains already toasted
	 * out-of-line attributes from some other relation, invoke the toaster.
	 *
	 * Note: below this point, tup is the data we actually intend to store
	 * into the relation; instup is the caller's original untoasted data.
	 */
	if (need_toast)
		tup = toast_insert_or_update_memtup(relation, instup,
											NULL, aoInsertDesc->mt_bind,
											aoInsertDesc->toast_tuple_target,
											false,	/* errtbl is never AO */
											0);
	else
		tup = instup;

	if (relation->rd_rel->relhasoids)
	{
		/*
		 * Don't modify the input tuple, so make a copy unless we already
		 * made one. I'm not sure if the input tuple can point to any
		 * permanent storage, so modifying it might be harmless, but better
		 * safe than sorry. An AO table with OIDs is a weird beast anyway,
		 * so performance of this case isn't important.
		 */
		if (tup == instup)
			tup = memtuple_copy_to(instup, NULL, NULL);

		/*
		 * If the object id of this tuple has already been assigned, trust the
		 * caller.	There are a couple of ways this can happen.  At initial db
		 * creation, the backend program sets oids for tuples. When we define
		 * an index, we set the oid.  Finally, in the future, we may allow
		 * users to set their own object ids in order to support a persistent
		 * object store (objects need to contain pointers to one another).
		 */
		if (!OidIsValid(tupleOid))
			tupleOid = GetNewOid(relation);

		MemTupleSetOid(tup, aoInsertDesc->mt_bind, tupleOid);
	}

	/*
	 * get space to insert our next item (tuple)
	 */
	itemLen = memtuple_get_size(tup);
	isLargeContent = false;

	/*
	 * If we are at the limit for append-only storage header's row count,
	 * force this VarBlock to finish.
	 */
	if (VarBlockMakerItemCount(&aoInsertDesc->varBlockMaker) >= AOSmallContentHeader_MaxRowCount)
		itemPtr = NULL;
	else
		itemPtr = VarBlockMakerGetNextItemPtr(&aoInsertDesc->varBlockMaker, itemLen);

	/*
	 * If no more room to place items in the current varblock finish it and
	 * start inserting into the next one.
	 */
	if (itemPtr == NULL)
	{
		if (VarBlockMakerItemCount(&aoInsertDesc->varBlockMaker) == 0)
		{
			/*
			 * Case #1.  The entire tuple cannot fit within a VarBlock.  It is
			 * too large.
			 */
			if (aoInsertDesc->useNoToast)
			{
				/*
				 * Indicate we need to write the large tuple as a large
				 * content multiple-block set.
				 */
				isLargeContent = true;
			}
			else
			{
				/*
				 * Use a different errcontext when user input (tuple contents)
				 * cause the error.
				 */
				ereport(ERROR,
						(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
						 errmsg("Item too long (check #1): length %d, maxBufferLen %d",
								itemLen, aoInsertDesc->varBlockMaker.maxBufferLen),
						 errcontext_appendonly_insert_block_user_limit(aoInsertDesc)));
			}
		}
		else
		{
			/*
			 * Write out the current VarBlock to make room.
			 */
			finishWriteBlock(aoInsertDesc);
			Assert(aoInsertDesc->nonCompressedData == NULL);
			Assert(!AppendOnlyStorageWrite_IsBufferAllocated(&aoInsertDesc->storageWrite));

			/*
			 * Setup a new VarBlock.
			 */
			setupNextWriteBlock(aoInsertDesc);

			itemPtr = VarBlockMakerGetNextItemPtr(&aoInsertDesc->varBlockMaker, itemLen);

			if (itemPtr == NULL)
			{
				/*
				 * Case #2.  The entire tuple cannot fit within a VarBlock. It
				 * is too large.
				 */
				if (aoInsertDesc->useNoToast)
				{
					/*
					 * Indicate we need to write the large tuple as a large
					 * content multiple-block set.
					 */
					isLargeContent = true;
				}
				else
				{
					/*
					 * Use a different errcontext when user input (tuple
					 * contents) cause the error.
					 */
					ereport(ERROR,
							(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
							 errmsg("Item too long (check #2): length %d, maxBufferLen %d",
									itemLen, aoInsertDesc->varBlockMaker.maxBufferLen),
							 errcontext_appendonly_insert_block_user_limit(aoInsertDesc)));
				}
			}
		}
	}

	if (!isLargeContent)
	{
		/*
		 * We have room in the current VarBlock for the new tuple.
		 */
		Assert(itemPtr != NULL);

		if (itemLen > 0)
			memcpy(itemPtr, tup, itemLen);
	}
	else
	{
		/*
		 * Write the large tuple as a large content multiple-block set.
		 */
		Assert(itemPtr == NULL);
		Assert(!need_toast);
		Assert(instup == tup);

		/*
		 * "Cancel" the last block allocation, if one.
		 */
		cancelLastBuffer(aoInsertDesc);
		Assert(aoInsertDesc->nonCompressedData == NULL);
		Assert(!AppendOnlyStorageWrite_IsBufferAllocated(&aoInsertDesc->storageWrite));

		/*
		 * Write large content.
		 */
		AppendOnlyStorageWrite_Content(
									   &aoInsertDesc->storageWrite,
									   (uint8 *) tup,
									   itemLen,
									   AoExecutorBlockKind_SingleRow,
									    /* rowCount */ 1);
		Assert(aoInsertDesc->nonCompressedData == NULL);
		Assert(!AppendOnlyStorageWrite_IsBufferAllocated(&aoInsertDesc->storageWrite));

		setupNextWriteBlock(aoInsertDesc);
	}

	aoInsertDesc->insertCount++;
	if (!aoInsertDesc->update_mode)
		pgstat_count_heap_insert(relation);
	else
		pgstat_count_heap_update(relation, false);
	aoInsertDesc->lastSequence++;
	if (aoInsertDesc->numSequences > 0)
		(aoInsertDesc->numSequences)--;

	Assert(aoInsertDesc->numSequences >= 0);

	tupleOid = MemTupleGetOid(tup, aoInsertDesc->mt_bind);

	AOTupleIdInit_Init(aoTupleId);
	AOTupleIdInit_segmentFileNum(aoTupleId, aoInsertDesc->cur_segno);
	AOTupleIdInit_rowNum(aoTupleId, aoInsertDesc->lastSequence);

	/*
	 * If the allocated fast sequence numbers are used up, we request for a
	 * next list of fast sequence numbers.
	 */
	if (aoInsertDesc->numSequences == 0)
	{
		int64		firstSequence;

		firstSequence =
			GetFastSequences(aoInsertDesc->aoi_rel->rd_appendonly->segrelid,
							 aoInsertDesc->cur_segno,
							 aoInsertDesc->lastSequence + 1,
							 NUM_FAST_SEQUENCES);

		Assert(firstSequence == aoInsertDesc->lastSequence + 1);
		aoInsertDesc->numSequences = NUM_FAST_SEQUENCES;
	}

	elogif(Debug_appendonly_print_insert_tuple, LOG,
		   "Append-only insert tuple for table '%s' "
		   "(AOTupleId %s, memtuple length %d, isLargeRow %s, block count " INT64_FORMAT ")",
		   NameStr(aoInsertDesc->aoi_rel->rd_rel->relname),
		   AOTupleIdToString(aoTupleId),
		   itemLen,
		   (isLargeContent ? "true" : "false"),
		   aoInsertDesc->bufferCount);

	if (tup != instup)
		pfree(tup);

	return tupleOid;
}

/*
 * appendonly_insert_finish
 *
 * when done inserting all the data via appendonly_insert() we need to call
 * this function to flush all remaining data in the buffer into the file.
 */
void
appendonly_insert_finish(AppendOnlyInsertDesc aoInsertDesc)
{
	/*
	 * Finish up that last varblock.
	 */
	finishWriteBlock(aoInsertDesc);

	CloseWritableFileSeg(aoInsertDesc);

	AppendOnlyBlockDirectory_End_forInsert(&(aoInsertDesc->blockDirectory));

	AppendOnlyStorageWrite_FinishSession(&aoInsertDesc->storageWrite);

	pfree(aoInsertDesc->title);
	pfree(aoInsertDesc);
}

/*
 * RelationGuessNumberOfBlocks
 *
 * Has the same meaning as RelationGetNumberOfBlocks for heap relations
 * however uses an estimation since AO relations use variable len blocks
 * which are meaningless to the optimizer.
 *
 * This function, in other words, answers the following question - "If
 * I were a heap relation, about how many blocks would I have had?"
 */
BlockNumber
RelationGuessNumberOfBlocks(double totalbytes)
{
	/* for now it's very simple */
	return (BlockNumber) (totalbytes / BLCKSZ) + 1;
}
