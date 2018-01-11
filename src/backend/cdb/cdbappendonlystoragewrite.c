/*-------------------------------------------------------------------------
 *
 * cdbappendonlystoragewrite.c
 *
 * Portions Copyright (c) 2007-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbappendonlystoragewrite.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#ifndef WIN32
#include <sys/fcntl.h>
#else
#include <io.h>
#endif
#include <sys/file.h>
#include <unistd.h>

#include "catalog/heap.h"
#include "catalog/pg_compression.h"
#include "cdb/cdbappendonlystorageread.h"
#include "cdb/cdbappendonlystorage.h"
#include "cdb/cdbappendonlystoragelayer.h"
#include "cdb/cdbappendonlystorageformat.h"
#include "cdb/cdbappendonlystoragewrite.h"
#include "cdb/cdbmirroredfilesysobj.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "utils/guc.h"


/*----------------------------------------------------------------
 * Initialization
 *----------------------------------------------------------------
 */

/*
 * Initialize AppendOnlyStorageWrite.
 *
 * The AppendOnlyStorageWrite data structure is initialized once for an
 * append "session" and can be used to add Append-Only Storage Blocks to 1
 * or more segment files.
 *
 * The current file to write to is opened with the
 * AppendOnlyStorageWrite_OpenFile routine.
 *
 * storageWrite		- data structure to initialize
 * memoryContext	- memory context to use for buffers and other memory
 *					  needs. When NULL, the current memory context is used.
 * maxBufferLen		- maximum Append-Only Storage Block length including all
 *					  storage headers.
 * relationName		- name of the relation to use in log and error messages.
 * title			- A phrase that better describes the purpose of this open.
 *					  The caller manages the storage for this.
 * storageAttributes - Append-Only Storage Attributes from relation creation.
 */
void
AppendOnlyStorageWrite_Init(AppendOnlyStorageWrite *storageWrite,
							MemoryContext memoryContext,
							int32 maxBufferLen,
							char *relationName,
							char *title,
							AppendOnlyStorageAttributes *storageAttributes)
{
	int			relationNameLen;
	uint8	   *memory;
	int32		memoryLen;
	MemoryContext oldMemoryContext;

	Assert(storageWrite != NULL);

	/* UNDONE: Range check maxBufferLen */

	Assert(relationName != NULL);
	Assert(storageAttributes != NULL);

	/* UNDONE: Range check fields in storageAttributes */

	MemSet(storageWrite, 0, sizeof(AppendOnlyStorageWrite));

	storageWrite->maxBufferLen = maxBufferLen;

	if (memoryContext == NULL)
		storageWrite->memoryContext = CurrentMemoryContext;
	else
		storageWrite->memoryContext = memoryContext;

	oldMemoryContext = MemoryContextSwitchTo(storageWrite->memoryContext);

	memcpy(
		   &storageWrite->storageAttributes,
		   storageAttributes,
		   sizeof(AppendOnlyStorageAttributes));

	/*
	 * Determine the fixed header length based on the checksum flag.
	 */
	storageWrite->regularHeaderLen = AoHeader_RegularSize;
	if (storageWrite->storageAttributes.checksum)
		storageWrite->regularHeaderLen += 2 * sizeof(pg_crc32);

	relationNameLen = strlen(relationName);
	storageWrite->relationName = (char *) palloc(relationNameLen + 1);
	memcpy(storageWrite->relationName, relationName, relationNameLen + 1);

	storageWrite->title = title;

	/*
	 * Set up extra buffers for compression.
	 */
	storageWrite->compressionOverrunLen = storageAttributes->overflowSize;

	elog(DEBUG2, "Requested compression overflow bytes = %d.", storageAttributes->overflowSize);

	if (storageWrite->storageAttributes.compress)
	{
		storageWrite->uncompressedBuffer = (uint8 *) palloc(storageWrite->maxBufferLen * sizeof(uint8));
	}
	else
	{
		Assert(storageWrite->uncompressedBuffer == NULL);
	}

	/*
	 * Now that we have determined the compression overrun, we can initialize
	 * BufferedAppend with the correct maxBufferLen + compressionOverrunLen.
	 */
	storageWrite->maxBufferWithCompressionOverrrunLen =
		storageWrite->maxBufferLen + storageWrite->compressionOverrunLen;
	storageWrite->largeWriteLen = 2 * storageWrite->maxBufferLen;
	Assert(storageWrite->maxBufferWithCompressionOverrrunLen <= storageWrite->largeWriteLen);

	memoryLen = BufferedAppendMemoryLen
		(
		 storageWrite->maxBufferWithCompressionOverrrunLen, /* maxBufferLen */
		 storageWrite->largeWriteLen);

	memory = (uint8 *) palloc(memoryLen);

	BufferedAppendInit(&storageWrite->bufferedAppend,
					   memory,
					   memoryLen,
					    /* maxBufferLen */ storageWrite->maxBufferWithCompressionOverrrunLen,
					   storageWrite->largeWriteLen,
					   relationName);

	elogif(Debug_appendonly_print_insert || Debug_appendonly_print_append_block, LOG,
		   "Append-Only Storage Write initialize for table '%s' (compression = %s, compression level %d, maximum buffer length %d, large write length %d)",
		   storageWrite->relationName,
		   (storageWrite->storageAttributes.compress ? "true" : "false"),
		   storageWrite->storageAttributes.compressLevel,
		   storageWrite->maxBufferWithCompressionOverrrunLen,
		   storageWrite->largeWriteLen);

	/*
	 * When doing VerifyBlock, allocate the extra buffers.
	 */
	if (gp_appendonly_verify_write_block && storageWrite->storageAttributes.compress)
	{
		storageWrite->verifyWriteBuffer = (uint8 *) palloc(storageWrite->maxBufferLen * sizeof(uint8));
	}
	else
	{
		Assert(storageWrite->verifyWriteBuffer == NULL);
	}

	storageWrite->file = -1;
	storageWrite->formatVersion = -1;

	MemoryContextSwitchTo(oldMemoryContext);

	storageWrite->isActive = true;

	storageWrite->bufferedAppend.mirroredOpen.isActive = FALSE;
	storageWrite->bufferedAppend.mirroredOpen.segmentFileNum = 0;
	storageWrite->bufferedAppend.mirroredOpen.primaryFile = -1;
}

/*
 * Finish using the AppendOnlyStorageWrite session created with ~Init.
 */
void
AppendOnlyStorageWrite_FinishSession(AppendOnlyStorageWrite *storageWrite)
{
	MemoryContext oldMemoryContext;

	if (!storageWrite->isActive)
		return;

	oldMemoryContext = MemoryContextSwitchTo(storageWrite->memoryContext);

	/*
	 * UNDONE: This expects the MemoryContent to be what was used for the
	 * 'memory' in ~Init
	 */
	BufferedAppendFinish(&storageWrite->bufferedAppend);

	if (storageWrite->relationName != NULL)
	{
		pfree(storageWrite->relationName);
		storageWrite->relationName = NULL;
	}

	if (storageWrite->uncompressedBuffer != NULL)
	{
		pfree(storageWrite->uncompressedBuffer);
		storageWrite->uncompressedBuffer = NULL;
	}

	if (storageWrite->verifyWriteBuffer != NULL)
	{
		pfree(storageWrite->verifyWriteBuffer);
		storageWrite->verifyWriteBuffer = NULL;
	}

	if (storageWrite->segmentFileName != NULL)
	{
		pfree(storageWrite->segmentFileName);
		storageWrite->segmentFileName = NULL;
	}

	if (storageWrite->compression_functions != NULL)
	{
		callCompressionDestructor(storageWrite->compression_functions[COMPRESSION_DESTRUCTOR], storageWrite->compressionState);
		if (storageWrite->compressionState != NULL)
		{
			pfree(storageWrite->compressionState);
		}

		if (storageWrite->verifyWriteCompressionState != NULL)
		{
			callCompressionDestructor(storageWrite->compression_functions[COMPRESSION_DESTRUCTOR], storageWrite->verifyWriteCompressionState);
		}
	}

	/* Deallocation is done.      Go back to caller memory-context. */
	MemoryContextSwitchTo(oldMemoryContext);

	storageWrite->isActive = false;

}

/*----------------------------------------------------------------
 * Open and FlushAndClose
 *----------------------------------------------------------------
 */

/*
 * Creates an on-demand Append-Only segment file under transaction.
 *
 * filePathName - name of the segment file to open.
 * logicalEof	- The last committed write transaction's EOF value to use as
 *				  the end of the segment file. If 0, we will create the file
 *				  if necessary. Otherwise, it must already exist.
 */
void
AppendOnlyStorageWrite_TransactionCreateFile(AppendOnlyStorageWrite *storageWrite,
											 char *filePathName,
											 int64 logicalEof,
											 RelFileNode *relFileNode,
											 int32 segmentFileNum,
											 ItemPointer persistentTid,
											 int64 *persistentSerialNum)
{
	Relation	gp_relation_node;

	Assert(segmentFileNum > 0);
	Assert(logicalEof == 0);

	/*
	 * We may or may not have a gp_relation_node entry when the EOF is 0.
	 */
	if (ReadGpRelationNode(
						   (relFileNode->spcNode == MyDatabaseTableSpace) ? 0 : relFileNode->spcNode,
						   relFileNode->relNode,
						   segmentFileNum,
						   persistentTid,
						   persistentSerialNum))
	{
		/*
		 * UNDONE: Verify the gp_persistent_relation_node Append-Only EOFs are
		 * zero.
		 */
		return;
	}

	MirroredFileSysObj_TransactionCreateAppendOnlyFile(relFileNode,
													   segmentFileNum,
													   storageWrite->relationName,
													    /* doJustInTimeDirCreate */ false,
													   persistentTid,
													   persistentSerialNum);

	gp_relation_node = heap_open(GpRelationNodeRelationId, RowExclusiveLock);

	InsertGpRelationNodeTuple(gp_relation_node,
							   /* relationId */ 0,	/* UNDONE: Don't have this
													 * value here -- currently
													 * only used for
													 * tracing... */
							  storageWrite->relationName,
							  (relFileNode->spcNode == MyDatabaseTableSpace) ? 0 : relFileNode->spcNode,
							  relFileNode->relNode,
							  segmentFileNum,
							   /* updateIndex */ true,
							  persistentTid,
							  *persistentSerialNum);

	heap_close(gp_relation_node, RowExclusiveLock);
}

/*
 * Opens the next segment file to write.  The file must already exist.
 *
 * This routine is responsible for seeking to the proper write location given
 * the logical EOF.
 *
 * filePathName		- name of the segment file to open.
 * version			- AO table format version the file is in.
 * logicalEof		- last committed write transaction's EOF value to use as
 *					  the end of the segment file.
 */
void
AppendOnlyStorageWrite_OpenFile(AppendOnlyStorageWrite *storageWrite,
								char *filePathName,
								int version,
								int64 logicalEof,
								int64 fileLen_uncompressed,
								RelFileNode *relFileNode,
								int32 segmentFileNum,
								ItemPointer persistentTid,
								int64 persistentSerialNum)
{
	int			primaryError;
	File		file;
	int64		seekResult;
	MemoryContext oldMemoryContext;
	int			segmentFileNameLen;

	Assert(storageWrite != NULL);
	Assert(storageWrite->isActive);

	Assert(filePathName != NULL);

	/*
	 * Assume that we only write in the current latest format. (it's redundant
	 * to pass the version number as argument, currently)
	 */
	if (version != AORelationVersion_GetLatest())
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("cannot write append-only table version %d", version)));

	/*
	 * Open or create the file for write.
	 */
	MirroredAppendOnly_OpenReadWrite(&storageWrite->bufferedAppend.mirroredOpen,
									 relFileNode,
									 segmentFileNum,
									 storageWrite->relationName,
									 logicalEof,
									  /* traceOpenFlags */ Debug_appendonly_print_append_block,
									 persistentTid,
									 persistentSerialNum,
									 &primaryError);
	if (primaryError != 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("Append-only Storage Write could not open segment file '%s' for relation '%s': %s",
						filePathName,
						storageWrite->relationName,
						strerror(primaryError))));

	file = storageWrite->bufferedAppend.mirroredOpen.primaryFile;

	/*
	 * Seek to the logical EOF write position.
	 */
	seekResult = FileSeek(file, logicalEof, SEEK_SET);
	if (seekResult != logicalEof)
	{
		bool		mirrorDataLossOccurred;

		MirroredAppendOnly_Close(&storageWrite->bufferedAppend.mirroredOpen,
								 &mirrorDataLossOccurred);

		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				 errmsg("Append-only Storage Write error on segment file '%s' for relation '%s'.  FileSeek offset = " INT64_FORMAT ".  Error code = %d (%s)",
						filePathName,
						storageWrite->relationName,
						logicalEof,
						(int) seekResult,
						strerror((int) seekResult))));
	}

	storageWrite->file = file;
	storageWrite->formatVersion = version;
	storageWrite->startEof = logicalEof;
	storageWrite->relFileNode = *relFileNode;
	storageWrite->segmentFileNum = segmentFileNum;
	storageWrite->persistentTid = *persistentTid;
	storageWrite->persistentSerialNum = persistentSerialNum;

	/*
	 * When writing multiple segment files, we throw away the old segment file
	 * name strings.
	 */
	oldMemoryContext = MemoryContextSwitchTo(storageWrite->memoryContext);

	if (storageWrite->segmentFileName != NULL)
		pfree(storageWrite->segmentFileName);

	segmentFileNameLen = strlen(filePathName);
	storageWrite->segmentFileName = (char *) palloc(segmentFileNameLen + 1);
	memcpy(storageWrite->segmentFileName, filePathName, segmentFileNameLen + 1);

	/* Allocation is done.  Go back to caller memory-context. */
	MemoryContextSwitchTo(oldMemoryContext);

	/*
	 * Tell the BufferedAppend module about the file we just opened.
	 */
	BufferedAppendSetFile(&storageWrite->bufferedAppend,
						  storageWrite->file,
						  storageWrite->segmentFileName,
						  logicalEof,
						  fileLen_uncompressed);
}

/*
 * Optionally pad out to next page boundary.
 *
 * Since we do not do typical recovery processing of append-only file-system
 * pages, we pad out the last file-system byte with zeroes. The number of
 * bytes that are padded with zero's is determined by safefswritesize.
 * This function pads with 0's of length padLen or pads the whole remainder
 * of the safefswritesize size with 0's if padLen is -1.
 */
static void
AppendOnlyStorageWrite_DoPadOutRemainder(AppendOnlyStorageWrite *storageWrite,
										 int32 padLen)
{
	int64		nextWritePosition;
	int64		nextBoundaryPosition;
	int32		safeWrite = storageWrite->storageAttributes.safeFSWriteSize;
	int32		safeWriteRemainder;
	bool		doPad;
	uint8	   *buffer;

	/* early exit if no pad needed */
	if (safeWrite == 0)
		return;

	nextWritePosition = BufferedAppendNextBufferPosition(&storageWrite->bufferedAppend);
	nextBoundaryPosition =
		((nextWritePosition + safeWrite - 1) / safeWrite) * safeWrite;
	safeWriteRemainder = (int32) (nextBoundaryPosition - nextWritePosition);

	if (safeWriteRemainder <= 0)
		doPad = false;
	else if (padLen == -1)
	{
		/*
		 * Pad to end of page.
		 */
		doPad = true;
		padLen = safeWriteRemainder;
	}
	else
		doPad = (safeWriteRemainder < padLen);

	if (doPad)
	{
		/*
		 * Get buffer of the remainder to pad.
		 */
		buffer = BufferedAppendGetBuffer(&storageWrite->bufferedAppend,
										 safeWriteRemainder);

		if (buffer == NULL)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("We do not expect files to be have a maximum length")));
		}

		memset(buffer, 0, safeWriteRemainder);
		BufferedAppendFinishBuffer(&storageWrite->bufferedAppend,
								   safeWriteRemainder,
								   safeWriteRemainder);

		elogif(Debug_appendonly_print_insert, LOG,
			   "Append-only insert zero padded safeWriteRemainder for table '%s' (nextWritePosition = " INT64_FORMAT ", safeWriteRemainder = %d)",
			   storageWrite->relationName,
			   nextWritePosition,
			   safeWriteRemainder);
	}
}

/*
 * Flush and close the current segment file.
 *
 * No error if the current is already closed.
 *
 * The new EOF of the segment file is returend in *newLogicalEof.
 */
void
AppendOnlyStorageWrite_FlushAndCloseFile(
										 AppendOnlyStorageWrite *storageWrite,
										 int64 *newLogicalEof,
										 int64 *fileLen_uncompressed,
										 bool *mirrorDataLossOccurred,
										 bool *mirrorCatchupRequired,
										 MirrorDataLossTrackingState *originalMirrorDataLossTrackingState,
										 int64 *originalMirrorDataLossTrackingSessionNum)
{
	int			primaryError;

	Assert(storageWrite != NULL);
	Assert(storageWrite->isActive);

	*mirrorDataLossOccurred = false;	/* Assume. */
	*mirrorCatchupRequired = false; /* Assume. */

	if (storageWrite->file == -1)
	{
		*newLogicalEof = 0;
		*fileLen_uncompressed = 0;
		return;
	}

	/*
	 * We pad out append commands to the page boundary.
	 */
	AppendOnlyStorageWrite_DoPadOutRemainder(
											 storageWrite,
											  /* indicate till end of page */ -1);

	/*
	 * Have the BufferedAppend module let go, but this does not close the
	 * file.
	 */
	BufferedAppendCompleteFile(&storageWrite->bufferedAppend,
							   newLogicalEof,
							   fileLen_uncompressed);

	/*
	 * We must take care of fsynching to disk ourselves since the fd API won't
	 * do it for us.
	 */

	MirroredAppendOnly_FlushAndClose(&storageWrite->bufferedAppend.mirroredOpen,
									 &primaryError,
									 mirrorDataLossOccurred,
									 mirrorCatchupRequired,
									 originalMirrorDataLossTrackingState,
									 originalMirrorDataLossTrackingSessionNum);
	if (primaryError != 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("Could not flush (fsync) Append-Only segment file '%s' to disk for relation '%s': %s",
						storageWrite->segmentFileName,
						storageWrite->relationName,
						strerror(primaryError))));

	storageWrite->file = -1;
	storageWrite->formatVersion = -1;

	MemSet(&storageWrite->relFileNode, 0, sizeof(RelFileNode));
	storageWrite->segmentFileNum = 0;
	MemSet(&storageWrite->persistentTid, 0, sizeof(ItemPointerData));
	storageWrite->persistentSerialNum = 0;
}

/*
 * Flush and close the current segment file under a transaction.
 *
 * Handles mirror loss end transaction work.
 *
 * No error if the current is already closed.
 *
 * The new EOF of the segment file is returned in *newLogicalEof.
 */
void
AppendOnlyStorageWrite_TransactionFlushAndCloseFile(
													AppendOnlyStorageWrite *storageWrite,
													int64 *newLogicalEof,
													int64 *fileLen_uncompressed)
{
	MIRRORED_LOCK_DECLARE;

	RelFileNode relFileNode;
	int32		segmentFileNum;
	ItemPointerData persistentTid;
	int64		persistentSerialNum;

	int64		startEof;
	bool		mirrorDataLossOccurred;
	bool		mirrorCatchupRequired;

	MirrorDataLossTrackingState originalMirrorDataLossTrackingState;
	int64		originalMirrorDataLossTrackingSessionNum;

	Assert(storageWrite != NULL);
	Assert(storageWrite->isActive);

	if (storageWrite->file == -1)
	{
		*newLogicalEof = 0;
		*fileLen_uncompressed = 0;
		return;
	}

	relFileNode = storageWrite->relFileNode;
	segmentFileNum = storageWrite->segmentFileNum;
	persistentTid = storageWrite->persistentTid;
	persistentSerialNum = storageWrite->persistentSerialNum;

	startEof = storageWrite->startEof;

	/*
	 * Use the MirroredLock here to cover the flush (and close) and evaluation
	 * below whether we must catchup the mirror.
	 */
	MIRRORED_LOCK;

	AppendOnlyStorageWrite_FlushAndCloseFile(storageWrite,
											 newLogicalEof,
											 fileLen_uncompressed,
											 &mirrorDataLossOccurred,
											 &mirrorCatchupRequired,
											 &originalMirrorDataLossTrackingState,
											 &originalMirrorDataLossTrackingSessionNum);

	if (*newLogicalEof - startEof > 0)
	{
		/*
		 * This routine will handle both updating the persistent information
		 * about the new EOF and copy data to the mirror if we are now in
		 * synchronized state.
		 */
		elogif(Debug_persistent_print, Persistent_DebugPrintLevel(),
			   "AppendOnlyStorageWrite_TransactionFlushAndCloseFile: %u/%u/%u, segment file #%d, serial number " INT64_FORMAT ", TID %s, mirror catchup required %s, "
			   "mirror data loss tracking (state '%s', session num " INT64_FORMAT "), "
			   "mirror start EOF " INT64_FORMAT ", mirror new EOF " INT64_FORMAT,
			   relFileNode.spcNode,
			   relFileNode.dbNode,
			   relFileNode.relNode,
			   segmentFileNum,
			   persistentSerialNum,
			   ItemPointerToString(&persistentTid),
			   (mirrorCatchupRequired ? "true" : "false"),
			   MirrorDataLossTrackingState_Name(originalMirrorDataLossTrackingState),
			   originalMirrorDataLossTrackingSessionNum,
			   startEof,
			   *newLogicalEof);
		MirroredAppendOnly_AddMirrorResyncEofs(
											   &relFileNode,
											   segmentFileNum,
											   storageWrite->relationName,
											   &persistentTid,
											   persistentSerialNum,
											   &mirroredLockLocalVars,
											   mirrorCatchupRequired,
											   originalMirrorDataLossTrackingState,
											   originalMirrorDataLossTrackingSessionNum,
											   *newLogicalEof);

	}

	MIRRORED_UNLOCK;

}


/*----------------------------------------------------------------
 * Usable Block Length
 *----------------------------------------------------------------
 */

/*
 * When writing "short" content intended to stay within the maxBufferLen (also
 * known as blocksize), some of the buffer will be used for the Append-Only
 * Block Header.  This function returns that overhead length.
 *
 * Isn't the length of the Append-Only Storage Block constant? NO.
 *
 * Currently, there are two things that can make it longer.  When checksums
 * are configured, we add checksum data to the header.  And there is optional
 * header data (e.g. firstRowNum).
 *
 * We call the header portion with the optional checksums the fixed header
 * because we need to be able to read and evaluate the checksums before we can
 * interpret flags in the fixed header that indicate there is more header
 * information.
 *
 * The complete header length is the fixed header plus optional information.
 */

/*
 * Returns the Append-Only Storage Block complete header length in bytes.
 *
 * Call this routine after specifying all optional header information for the current block
 * begin written.
 */
int32
AppendOnlyStorageWrite_CompleteHeaderLen(AppendOnlyStorageWrite *storageWrite,
										 AoHeaderKind aoHeaderKind)
{
	int32		completeHeaderLen;

	Assert(storageWrite != NULL);
	Assert(storageWrite->isActive);

	completeHeaderLen = storageWrite->regularHeaderLen;
	if (AoHeader_IsLong(aoHeaderKind))
	{
		completeHeaderLen += (AoHeader_LongSize - AoHeader_RegularSize);
	}

	if (storageWrite->isFirstRowNumSet)
		completeHeaderLen += sizeof(int64);

	return completeHeaderLen;
}

/*
 * Returns the Append-Only Storage large content metadata header length in
 * bytes.
 *
 * Call this routine after specifying all optional header information for the
 * current block begin written.
 */
static int32
AppendOnlyStorageWrite_LargeContentHeaderLen(AppendOnlyStorageWrite *storageWrite)
{
	int32		completeHeaderLen;

	Assert(storageWrite != NULL);
	Assert(storageWrite->isActive);

	completeHeaderLen = storageWrite->regularHeaderLen;

	if (storageWrite->isFirstRowNumSet)
		completeHeaderLen += sizeof(int64);

	/* UNDONE: Right alignment? */

	return completeHeaderLen;
}

char *
AppendOnlyStorageWrite_ContextStr(AppendOnlyStorageWrite *storageWrite)
{
	int64		headerOffsetInFile =
	BufferedAppendCurrentBufferPosition(&storageWrite->bufferedAppend);

	StringInfoData buf;

	initStringInfo(&buf);
	appendStringInfo(
					 &buf,
					 "%s. Append-Only segment file '%s', header offset in file " INT64_FORMAT,
					 storageWrite->title,
					 storageWrite->segmentFileName,
					 headerOffsetInFile);

	return buf.data;
}

static char *
AppendOnlyStorageWrite_BlockHeaderStr(AppendOnlyStorageWrite *storageWrite,
									  uint8 *header)
{
	return AppendOnlyStorageFormat_BlockHeaderStr(header,
												  storageWrite->storageAttributes.checksum,
												  storageWrite->formatVersion);
}


static void
AppendOnlyStorageWrite_LogBlockHeader(AppendOnlyStorageWrite *storageWrite,
									  int64 headerOffsetInFile,
									  uint8 *header)
{
	char	   *blockHeaderStr;

	blockHeaderStr = AppendOnlyStorageWrite_BlockHeaderStr(storageWrite,
														   header);
	ereport(LOG,
			(errmsg("%s. Append-Only segment file '%s', header offset in file " INT64_FORMAT ". %s",
					storageWrite->title,
					storageWrite->segmentFileName,
					headerOffsetInFile,
					blockHeaderStr)));

	pfree(blockHeaderStr);
}

/*----------------------------------------------------------------
 * errcontext and errdetail
 *----------------------------------------------------------------
 */

/*
 * errcontext_appendonly_write_storage_block
 *
 * Add an errcontext() line showing the table, segment file, offset in file,
 * block count of the storage block being read.
 */
static int
errcontext_appendonly_write_storage_block(AppendOnlyStorageWrite *storageWrite)
{
	int64		headerOffsetInFile;

	headerOffsetInFile = BufferedAppendCurrentBufferPosition(&storageWrite->bufferedAppend);

	errcontext("Append-Only table '%s', segment file '%s', block header offset in file = " INT64_FORMAT ", bufferCount " INT64_FORMAT ")",
			   storageWrite->relationName,
			   storageWrite->segmentFileName,
			   headerOffsetInFile,
			   storageWrite->bufferCount);

	return 0;
}

/*
 * errdetail_appendonly_write_storage_block_header
 *
 * Add an errdetail() line showing the Append-Only Storage header being written.
 */
static int
errdetail_appendonly_write_storage_block_header(AppendOnlyStorageWrite *storageWrite)
{
	uint8	   *header;
	bool		checksum;
	int			version;

	header = BufferedAppendGetCurrentBuffer(&storageWrite->bufferedAppend);
	checksum = storageWrite->storageAttributes.checksum;
	version = storageWrite->formatVersion;

	return errdetail_appendonly_storage_smallcontent_header(header, checksum,
															version);
}

/*----------------------------------------------------------------
 * Writing Small Content Efficiently that is not being Bulk Compressed
 *----------------------------------------------------------------
 */

/*
 * This section describes for writing content that is less than or equal to
 * the blocksize (e.g. 32k) bytes that is not being bulk compressed by the
 * Append-Only Storage Layer.
 *
 * Actually, the content is limited to blocksize minus the Append-Only header
 * size (see the AppendOnlyStorageWrite_HeaderLen routine).
 */

/*
 * Get a pointer to next maximum length buffer space for
 * appending small content.
 *
 * You must decide whether you are supplying the optional first row number
 * BEFORE calling this routine!
 *
 * NOTE: The maximum length buffer space =
 *				maxBufferLen +
 *				AppendOnlyStorageWrite_CompleteHeaderLen(...)
 *
 * When compression is not being used, this interface provides a pointer
 * directly into the write buffer for efficient data generation.  Otherwise,
 * a pointer to a temporary buffer to collect the uncompressed contents will
 * be provided.
 *
 * Returns NULL when the current file does not have enough room for another
 * buffer.
 */
uint8 *
AppendOnlyStorageWrite_GetBuffer(AppendOnlyStorageWrite *storageWrite,
								 int aoHeaderKind)
{
	Assert(storageWrite != NULL);
	Assert(storageWrite->isActive);

	Assert(aoHeaderKind == AoHeaderKind_SmallContent ||
		   aoHeaderKind == AoHeaderKind_NonBulkDenseContent ||
		   aoHeaderKind == AoHeaderKind_BulkDenseContent);

	storageWrite->getBufferAoHeaderKind = aoHeaderKind;

	/*
	 * Both headers (Small and NonBulkDense) have the same length. BulkDense
	 * is a long header.
	 */
	storageWrite->currentCompleteHeaderLen =
		AppendOnlyStorageWrite_CompleteHeaderLen(
												 storageWrite,
												 aoHeaderKind);


	/*
	 * If compression configured, the supply the temporary buffer instead.
	 */
	if (storageWrite->storageAttributes.compress)
	{
		storageWrite->currentBuffer = NULL;

		return storageWrite->uncompressedBuffer;
	}
	else
	{
		storageWrite->currentBuffer =
			BufferedAppendGetMaxBuffer(&storageWrite->bufferedAppend);

		return &storageWrite->currentBuffer[storageWrite->currentCompleteHeaderLen];
	}
}

/*
 * Test if a buffer is currently allocated.
 */
bool
AppendOnlyStorageWrite_IsBufferAllocated(AppendOnlyStorageWrite *storageWrite)
{
	Assert(storageWrite != NULL);
	Assert(storageWrite->isActive);

	return (storageWrite->currentCompleteHeaderLen > 0);
}

/*
 * Return the beginning of the last write position of the write buffer.
 */
int64
AppendOnlyStorageWrite_LogicalBlockStartOffset(AppendOnlyStorageWrite *storageWrite)
{
	return storageWrite->logicalBlockStartOffset;
}

/*
 * Return the position of the current write buffer.
 */
int64
AppendOnlyStorageWrite_CurrentPosition(AppendOnlyStorageWrite *storageWrite)
{
	Assert(storageWrite != NULL);
	Assert(storageWrite->isActive);

	return BufferedAppendCurrentBufferPosition(
											   &storageWrite->bufferedAppend);
}

/*
 * Return the internal current write buffer that includes the header.
 * UNDONE: Fix this interface privacy violation...
 */
uint8 *
AppendOnlyStorageWrite_GetCurrentInternalBuffer(AppendOnlyStorageWrite *storageWrite)
{
	Assert(storageWrite != NULL);
	Assert(storageWrite->isActive);

	return BufferedAppendGetCurrentBuffer(&storageWrite->bufferedAppend);
}


static void
AppendOnlyStorageWrite_VerifyWriteBlock(AppendOnlyStorageWrite *storageWrite,
										int64 headerOffsetInFile,
										int32 bufferLen,
										uint8 *expectedContent,
										int32 expectedUncompressedLen,
										int expectedExecutorBlockKind,
										int expectedRowCount,
										int32 expectedCompressedLen)
{
	uint8	   *header;

	AOHeaderCheckError checkError;
	AoHeaderKind headerKind;
	int32		actualHeaderLen;
	int32		offset;
	int32		uncompressedLen;
	bool		isCompressed;
	int32		overallBlockLen;
	int32		compressedLen;
	int			executorBlockKind;
	bool		hasFirstRowNum;
	int64		firstRowNum;
	int			rowCount;
	pg_crc32	storedChecksum;
	pg_crc32	computedChecksum;

	if (storageWrite->storageAttributes.compress && storageWrite->verifyWriteBuffer == NULL)
		return;					/* GUC must have been turned on
								 * mid-transaction. */

	if (gp_appendonly_verify_write_block == false)
		elog(WARNING, "The GUC gp_appendonly_verify_write_block is false. Compressed write not checked.");

	header = BufferedAppendGetCurrentBuffer(&storageWrite->bufferedAppend);

	/*
	 * Code is similar to that in getNextStorageBlockInFile routine.
	 */

	/*----------
	 * Proceed very carefully:
	 * [ 1. Verify header checksum ]
	 *	 2. Examine (basic) header.
	 *	 3. Examine specific header.
	 * [ 4. Verify the block checksum ]
	 *----------
	 */
	if (storageWrite->storageAttributes.checksum)
	{
		if (!AppendOnlyStorageFormat_VerifyHeaderChecksum(
														  header,
														  &storedChecksum,
														  &computedChecksum))
			ereport(ERROR,
					(errmsg("Verify block during write found header checksum does not match.  Expected 0x%X and found 0x%X",
							storedChecksum,
							computedChecksum),
					 errdetail_appendonly_write_storage_block_header(storageWrite),
					 errcontext_appendonly_write_storage_block(storageWrite)));
	}

	/*
	 * Check the (basic) header information.
	 */
	checkError = AppendOnlyStorageFormat_GetHeaderInfo(
													   header,
													   storageWrite->storageAttributes.checksum,
													   &headerKind,
													   &actualHeaderLen);
	if (checkError != AOHeaderCheckOk)
		ereport(ERROR,
				(errmsg("Verify block during write found bad append-only storage header. Header check error %d, detail '%s'",
						(int) checkError,
						AppendOnlyStorageFormat_GetHeaderCheckErrorStr()),
				 errdetail_appendonly_write_storage_block_header(storageWrite),
				 errcontext_appendonly_write_storage_block(storageWrite)));

	switch (headerKind)
	{
		case AoHeaderKind_SmallContent:

			/*
			 * Check the Block header information.
			 */
			checkError =
				AppendOnlyStorageFormat_GetSmallContentHeaderInfo
				(header,
				 actualHeaderLen,
				 storageWrite->storageAttributes.checksum,
				 bufferLen,
				 &overallBlockLen,
				 &offset, //Offset to data.
				 & uncompressedLen,
				 &executorBlockKind,
				 &hasFirstRowNum,
				 storageWrite->formatVersion,
				 &firstRowNum,
				 &rowCount,
				 &isCompressed,
				 &compressedLen);
			if (checkError != AOHeaderCheckOk)
				ereport(ERROR,
						(errmsg("Verify block during write found bad append-only storage block header. "
								"Header check error %d, detail '%s'",
								(int) checkError,
								AppendOnlyStorageFormat_GetHeaderCheckErrorStr()),
						 errdetail_appendonly_write_storage_block_header(storageWrite),
						 errcontext_appendonly_write_storage_block(storageWrite)));

			if (uncompressedLen != expectedUncompressedLen)
				ereport(ERROR,
						(errmsg("Verify block during write found append-only storage block header. "
								"DataLen %d does not equal expected length %d, ",
								uncompressedLen,
								expectedUncompressedLen),
						 errdetail_appendonly_write_storage_block_header(storageWrite),
						 errcontext_appendonly_write_storage_block(storageWrite)));


			if (compressedLen != expectedCompressedLen)
				ereport(ERROR,
						(errmsg("Verify block during write found append-only storage block header. "
								"CompressedLen %d does not equal expected length %d",
								compressedLen,
								expectedCompressedLen),
						 errdetail_appendonly_write_storage_block_header(storageWrite),
						 errcontext_appendonly_write_storage_block(storageWrite)));

			/*
			 * Now verify the executor portion of the block.
			 */

			if (executorBlockKind != expectedExecutorBlockKind)
				ereport(ERROR,
						(errmsg("Verify block during write found append-only storage block header. "
								"ExecutorBlockKind %d does not equal expected value %d.",
								executorBlockKind,
								expectedExecutorBlockKind),
						 errdetail_appendonly_write_storage_block_header(storageWrite),
						 errcontext_appendonly_write_storage_block(storageWrite)));

			/* UNDONE: Check hasFirstRowNum */

			if (rowCount != expectedRowCount)
				ereport(ERROR,
						(errmsg("Verify block during write found append-only storage block header. "
								"RowCount %d does not equal expected value %d",
								rowCount,
								expectedRowCount),
						 errdetail_appendonly_write_storage_block_header(storageWrite),
						 errcontext_appendonly_write_storage_block(storageWrite)));

			if (Debug_appendonly_print_verify_write_block)
			{
				AppendOnlyStorageWrite_LogBlockHeader(storageWrite,
													  headerOffsetInFile,
													  header);
			}


			if (isCompressed)
			{
				int			test;
				PGFunction	decompressor;
				PGFunction *cfns = storageWrite->compression_functions;

				Assert(gp_appendonly_verify_write_block == true);
				Assert(storageWrite->verifyWriteCompressionState != NULL);

				if (cfns == NULL)
					ereport(ERROR,
							(errcode(ERRCODE_INTERNAL_ERROR),
							 errmsg("decompression information missing")));

				decompressor = cfns[COMPRESSION_DECOMPRESS];

				gp_decompress_new(&header[offset], //Compressed data in block.
								  compressedLen,
								  storageWrite->verifyWriteBuffer, //Temporary buffer to hold uncompressed data.
								  uncompressedLen,
								  decompressor,
								  storageWrite->verifyWriteCompressionState,
								  storageWrite->bufferCount);

				/*
				 * Compare.
				 */
				test = memcmp(expectedContent,
							  storageWrite->verifyWriteBuffer,
							  uncompressedLen);

				if (test != 0)
					ereport(ERROR,
							(errmsg("Verify block during write found decompress did not produce the exact same bits passed to compress! "
									"Memcmp result %d",
									test),
							 errdetail_appendonly_write_storage_block_header(storageWrite),
							 errcontext_appendonly_write_storage_block(storageWrite)));

			}
			else
			{
				/* UNDONE: Do comparison here */
			}
			break;

		case AoHeaderKind_LargeContent:

			/*
			 * Check the LargeContent header information.
			 */
			checkError = AppendOnlyStorageFormat_GetLargeContentHeaderInfo
				(header,
				 actualHeaderLen,
				 storageWrite->storageAttributes.checksum,
				 &uncompressedLen,
				 &executorBlockKind,
				 &hasFirstRowNum,
				 &firstRowNum,
				 &rowCount);
			if (checkError != AOHeaderCheckOk)
				ereport(ERROR,
						(errmsg("Bad append-only storage header of type LargeContent. Header check error %d, detail '%s'",
								(int) checkError,
								AppendOnlyStorageFormat_GetHeaderCheckErrorStr()),
						 errdetail_appendonly_write_storage_block_header(storageWrite),
						 errcontext_appendonly_write_storage_block(storageWrite)));

			/*
			 * UNDONE: Needs checks on uncompressedLen, executorBlockKind,
			 * hasFirstRowNum, and rowCount...
			 *
			 * UNDONE: Aren't we done here?
			 */
			break;

		default:
			elog(ERROR, "Unexpected Append-Only header kind %d",
				 headerKind);
			break;
	}

#ifdef NeedCallBack
	if (executorBlockKind == AoExecutorBlockKind_VarBlock)
	{
		VarBlockCheckError varBlockCheckError;
		VarBlockReader varBlockReader;
		int			readerItemCount;

		varBlockCheckError = VarBlockIsValid(data, uncompressedLen);
		if (varBlockCheckError != VarBlockCheckOk)
			ereport(ERROR,
					(errmsg("Verify block during write found VarBlock is not valid "
							"Valid block check error %d, detail '%s'",
							varBlockCheckError,
							VarBlockGetCheckErrorStr()),
					 errdetail_appendonly_write_storage_block_header(storageWrite),
					 errcontext_appendonly_write_storage_block(storageWrite)));

		/*
		 * Now use the VarBlock module to extract the items out.
		 */
		VarBlockReaderInit(&varBlockReader,
						   data,
						   uncompressedLen);

		readerItemCount = VarBlockReaderItemCount(&varBlockReader);

		if (rowCount != readerItemCount)
		{
			ereport(ERROR,
					(errmsg("Verify block during write found row count %d in append-only storage header does not match VarBlock item count %d",
							rowCount,
							readerItemCount),
					 errdetail_appendonly_write_storage_block_header(storageWrite),
					 errcontext_appendonly_write_storage_block(storageWrite)));
		}
	}
#endif
}

static void
AppendOnlyStorageWrite_CompressAppend(AppendOnlyStorageWrite *storageWrite,
									  uint8 *sourceData,
									  int32 sourceLen,
									  int executorBlockKind,
									  int itemCount,
									  int32 *compressedLen,
									  int32 *bufferLen)
{
	uint8	   *header;
	uint8	   *dataBuffer;
	int32		dataRoundedUpLen = 0;	/* Shutup compiler. */
	int32		dataBufferWithOverrrunLen;
	PGFunction *cfns = storageWrite->compression_functions;
	PGFunction	compressor;

	if (cfns == NULL)
		compressor = NULL;
	else
		compressor = cfns[COMPRESSION_COMPRESS];

	/* UNDONE: This can be a duplicate call... */
	storageWrite->currentCompleteHeaderLen =
		AppendOnlyStorageWrite_CompleteHeaderLen(
												 storageWrite,
												 storageWrite->getBufferAoHeaderKind);

	header = BufferedAppendGetMaxBuffer(&storageWrite->bufferedAppend);
	if (header == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("We do not expect files to be have a maximum length"),
				 errcontext_appendonly_write_storage_block(storageWrite)));

	dataBuffer = &header[storageWrite->currentCompleteHeaderLen];
	dataBufferWithOverrrunLen =
		storageWrite->maxBufferWithCompressionOverrrunLen
		- storageWrite->currentCompleteHeaderLen;

	/*
	 * Compress into the BufferedAppend buffer after the large header (and
	 * optional checksum, etc.
	 */
	(void) gp_trycompress_new(
							  sourceData,
							  sourceLen,
							  dataBuffer,
							  dataBufferWithOverrrunLen,
							  sourceLen, //Limit compression to be no more than the input size.
							  compressedLen,
							  storageWrite->storageAttributes.compressLevel,
							  compressor,
							  storageWrite->compressionState);

	/*
	 * We always store the data compressed if the compressed length is less
	 * than the uncompressed length.
	 *
	 * TODO: this is a weak assumption. It doesn't account for the fact that
	 * it's not worth paying the CPU cost of decompression for a potentially
	 * trivial saving.
	 *
	 * The best solution to this seems to be to make the threshold at which we
	 * compress data user configurable.
	 */
	if (*compressedLen < sourceLen)
	{
		/*
		 * Compression successful.
		 */
		dataRoundedUpLen = AOStorage_RoundUp(*compressedLen, storageWrite->formatVersion);

		AOStorage_ZeroPad(
						  dataBuffer,
						  *compressedLen,
						  dataRoundedUpLen);

		switch (storageWrite->getBufferAoHeaderKind)
		{
			case AoHeaderKind_SmallContent:

				/*
				 * Make the header and compute the checksum if necessary.
				 */
				AppendOnlyStorageFormat_MakeSmallContentHeader
					(header,
					 storageWrite->storageAttributes.checksum,
					 storageWrite->isFirstRowNumSet,
					 storageWrite->formatVersion,
					 storageWrite->firstRowNum,
					 executorBlockKind,
					 itemCount,
					 sourceLen,
					 *compressedLen);
				break;

			case AoHeaderKind_BulkDenseContent:

				/*
				 * Make the header and compute the checksum if necessary.
				 */
				AppendOnlyStorageFormat_MakeBulkDenseContentHeader
					(header,
					 storageWrite->storageAttributes.checksum,
					 storageWrite->isFirstRowNumSet,
					 storageWrite->formatVersion,
					 storageWrite->firstRowNum,
					 executorBlockKind,
					 itemCount,
					 sourceLen,
					 *compressedLen);
				break;

			default:
				elog(ERROR, "Unexpected Append-Only header kind %d",
					 storageWrite->getBufferAoHeaderKind);
				break;
		}

		if (Debug_appendonly_print_storage_headers)
		{
			AppendOnlyStorageWrite_LogBlockHeader(storageWrite,
												  BufferedAppendCurrentBufferPosition(&storageWrite->bufferedAppend),
												  header);
		}

		elogif(Debug_appendonly_print_insert, LOG,
			   "Append-only insert finished compressed block for table '%s' "
			   "(segment file '%s', header offset in file " INT64_FORMAT ", "
			   "length = %d, compressed length %d, item count %d, block count " INT64_FORMAT ")",
			   storageWrite->relationName,
			   storageWrite->segmentFileName,
			   BufferedAppendCurrentBufferPosition(&storageWrite->bufferedAppend),
			   sourceLen,
			   *compressedLen,
			   itemCount,
			   storageWrite->bufferCount);
	}
	else
	{
		/*
		 * Unable to compress the data to smaller the input size. Solution:
		 * Indicate in the header we are storing an non-compressed block.
		 */
		*compressedLen = 0;

		dataRoundedUpLen = AOStorage_RoundUp(sourceLen, storageWrite->formatVersion);

		/*
		 * Copy non-compressed data in after the header information.
		 */
		memcpy(dataBuffer, sourceData, sourceLen);

		AOStorage_ZeroPad(dataBuffer, sourceLen, dataRoundedUpLen);

		/*
		 * Make the header and compute the checksum if necessary.
		 */
		AppendOnlyStorageFormat_MakeSmallContentHeader
			(header,
			 storageWrite->storageAttributes.checksum,
			 storageWrite->isFirstRowNumSet,
			 storageWrite->formatVersion,
			 storageWrite->firstRowNum,
			 executorBlockKind,
			 itemCount,
			 sourceLen,
			  /* compressedLen */ 0);

		if (Debug_appendonly_print_storage_headers)
		{
			AppendOnlyStorageWrite_LogBlockHeader(storageWrite,
												  BufferedAppendCurrentBufferPosition(&storageWrite->bufferedAppend),
												  header);
		}

		elogif(Debug_appendonly_print_insert, LOG,
			   "Append-only insert could not compress block for table '%s' smaller -- non-compressed block stored "
			   "(segment file '%s', header offset in file " INT64_FORMAT ", "
			   "length = %d, item count %d, block count " INT64_FORMAT ")",
			   storageWrite->relationName,
			   storageWrite->segmentFileName,
			   BufferedAppendCurrentBufferPosition(&storageWrite->bufferedAppend),
			   sourceLen,
			   itemCount,
			   storageWrite->bufferCount);
	}

	*bufferLen = storageWrite->currentCompleteHeaderLen + dataRoundedUpLen;
}

/*
 * Mark the current buffer "small" buffer as finished.
 *
 * If compression is configured, we will try to compress the contents in
 * the temporary uncompressed buffer into the write buffer.
 *
 * The buffer can be scheduled for writing and reused.
 *
 * contentLen		- byte length of the content generated directly into the
 *					  buffer returned by AppendOnlyStorageWrite_GetBuffer.
 * executorBlockKind - A value defined externally by the executor that
 *					   describes in content stored in the Append-Only Storage
 *					   Block.
 * rowCount			-  number of rows stored in the content.
 */
void
AppendOnlyStorageWrite_FinishBuffer(AppendOnlyStorageWrite *storageWrite,
									int32 contentLen,
									int executorBlockKind,
									int rowCount)
{
	int64		headerOffsetInFile;
	int32		bufferLen;

	Assert(storageWrite != NULL);
	Assert(storageWrite->isActive);

	Assert(storageWrite->currentCompleteHeaderLen > 0);

	if (contentLen >
		storageWrite->maxBufferLen - storageWrite->currentCompleteHeaderLen)
		elog(ERROR,
			 "Append-only content too large AO storage block (table '%s', "
			 "content length = %d, maximum buffer length %d, complete header length %d, first row number is set %s)",
			 storageWrite->relationName,
			 contentLen,
			 storageWrite->maxBufferLen,
			 storageWrite->currentCompleteHeaderLen,
			 (storageWrite->isFirstRowNumSet ? "true" : "false"));


	headerOffsetInFile = BufferedAppendCurrentBufferPosition(&storageWrite->bufferedAppend);

	if (!storageWrite->storageAttributes.compress)
	{
		uint8	   *nonCompressedHeader;
		uint8	   *nonCompressedData;
		int32		dataRoundedUpLen;
		int32		uncompressedlen;

		nonCompressedHeader = storageWrite->currentBuffer;

		nonCompressedData = &nonCompressedHeader[storageWrite->currentCompleteHeaderLen];

		dataRoundedUpLen = AOStorage_RoundUp(contentLen, storageWrite->formatVersion);

		AOStorage_ZeroPad(
						  nonCompressedData,
						  contentLen,
						  dataRoundedUpLen);

		switch (storageWrite->getBufferAoHeaderKind)
		{
			case AoHeaderKind_SmallContent:

				/*
				 * Make the header and compute the checksum if necessary.
				 */
				AppendOnlyStorageFormat_MakeSmallContentHeader
					(nonCompressedHeader,
					 storageWrite->storageAttributes.checksum,
					 storageWrite->isFirstRowNumSet,
					 storageWrite->formatVersion,
					 storageWrite->firstRowNum,
					 executorBlockKind,
					 rowCount,
					 contentLen,
					  /* compressedLength */ 0);
				break;

			case AoHeaderKind_NonBulkDenseContent:

				/*
				 * Make the header and compute the checksum if necessary.
				 */
				AppendOnlyStorageFormat_MakeNonBulkDenseContentHeader
					(nonCompressedHeader,
					 storageWrite->storageAttributes.checksum,
					 storageWrite->isFirstRowNumSet,
					 storageWrite->formatVersion,
					 storageWrite->firstRowNum,
					 executorBlockKind,
					 rowCount,
					 contentLen);
				break;

			default:
				elog(ERROR, "Unexpected Append-Only header kind %d",
					 storageWrite->getBufferAoHeaderKind);
				break;
		}

		if (Debug_appendonly_print_storage_headers)
		{
			AppendOnlyStorageWrite_LogBlockHeader(storageWrite,
												  headerOffsetInFile,
												  nonCompressedHeader);
		}


		bufferLen = storageWrite->currentCompleteHeaderLen + dataRoundedUpLen;
		uncompressedlen = bufferLen;	/* since there's no compression.. */

		/*
		 * Just before finishing the AO Storage buffer with our non-compressed
		 * content, let's verify it.
		 */
		if (gp_appendonly_verify_write_block)
			AppendOnlyStorageWrite_VerifyWriteBlock(storageWrite,
													headerOffsetInFile,
													bufferLen,
													nonCompressedData,
													contentLen,
													executorBlockKind,
													rowCount,
													 /* expectedCompressedLen */ 0);

		BufferedAppendFinishBuffer(&storageWrite->bufferedAppend,
								   bufferLen,
								   uncompressedlen);

		/* Declare it finished. */
		storageWrite->currentCompleteHeaderLen = 0;

		elogif(Debug_appendonly_print_insert, LOG,
			   "Append-only insert finished uncompressed block for table '%s' "
			   "(length = %d, executor block kind %d, item count %d, block count " INT64_FORMAT ")",
			   storageWrite->relationName,
			   contentLen,
			   executorBlockKind,
			   rowCount,
			   storageWrite->bufferCount);

	}
	else
	{
		int32		compressedLen = 0;

		AppendOnlyStorageWrite_CompressAppend(storageWrite,
											  storageWrite->uncompressedBuffer,
											  contentLen,
											  executorBlockKind,
											  rowCount,
											  &compressedLen,
											  &bufferLen);

		/*
		 * Just before finishing the AO Storage buffer with our non-compressed
		 * content, let's verify it.
		 */
		if (gp_appendonly_verify_write_block)
			AppendOnlyStorageWrite_VerifyWriteBlock(storageWrite,
													headerOffsetInFile,
													bufferLen,
													storageWrite->uncompressedBuffer,
													contentLen,
													executorBlockKind,
													rowCount,
													compressedLen);

		/*
		 * Finish the current buffer by specifying the used length.
		 */
		BufferedAppendFinishBuffer(&storageWrite->bufferedAppend,
								   bufferLen,
								   storageWrite->currentCompleteHeaderLen +
								   AOStorage_RoundUp(contentLen, storageWrite->formatVersion) /* non-compressed size */ );
		/* Declare it finished. */
		storageWrite->currentCompleteHeaderLen = 0;
	}

	Assert(storageWrite->currentCompleteHeaderLen == 0);
	storageWrite->currentBuffer = NULL;
	storageWrite->isFirstRowNumSet = false;
}

/*
 * Cancel the last ~GetBuffer call.
 *
 * This will also turn off the firstRowNum flag.
 */
void
AppendOnlyStorageWrite_CancelLastBuffer(
										AppendOnlyStorageWrite *storageWrite)
{
	Assert(storageWrite != NULL);
	Assert(storageWrite->isActive);

	Assert(storageWrite->currentCompleteHeaderLen > 0);

	if (storageWrite->currentBuffer != NULL)
	{
		BufferedAppendCancelLastBuffer(&storageWrite->bufferedAppend);
		storageWrite->currentBuffer = NULL;
	}

	storageWrite->currentCompleteHeaderLen = 0;

	/*
	 * Since we don't know if AppendOnlyStorageWrite_Content will be called
	 * next or the writer is doing something else, let's turn off the
	 * firstRowNum flag.
	 */
	storageWrite->isFirstRowNumSet = false;
}

/*----------------------------------------------------------------
 * Writing "Large" Content
 *----------------------------------------------------------------
 */

/*
 * This section describes for writing long content that can be up to 1 Gb
 * long and/or content that will be bulk-compressed when configured.
 */


/*
 * Write content up to 1Gb.
 *
 * Large content will be writen in fragment blocks by the Append-Only
 * Storage Layer.
 *
 * If compression is configured, then the content will be compressed in
 * fragments.
 *
 * Returns NULL when the current file does not have enough room for another
 * buffer.
 *
 * content		- Content to store.  All contiguous.
 * contentLen	- byte length of the data to store.
 * executorBlockKind - a value defined externally by the executor that
 *					   describes in content stored in the Append-Only Storage
 *					   Block.
 * rowCount		- number of rows stored in the content.
 */
void
AppendOnlyStorageWrite_Content(AppendOnlyStorageWrite *storageWrite,
							   uint8 *content,
							   int32 contentLen,
							   int executorBlockKind,
							   int rowCount)
{
	int32		completeHeaderLen;
	int32		compressedLen;
	int32		bufferLen;
	uint8	   *data;

	Assert(storageWrite != NULL);
	Assert(storageWrite->isActive);

	completeHeaderLen =
		AppendOnlyStorageWrite_CompleteHeaderLen(storageWrite,
												 AoHeaderKind_SmallContent);
	if (contentLen <= storageWrite->maxBufferLen - completeHeaderLen)
	{
		/*
		 * This is "small" content.
		 */
		if (!storageWrite->storageAttributes.compress)
		{
			data = AppendOnlyStorageWrite_GetBuffer(storageWrite,
													AoHeaderKind_SmallContent);

			memcpy(data, content, contentLen);

			storageWrite->logicalBlockStartOffset =
				BufferedAppendNextBufferPosition(&(storageWrite->bufferedAppend));

			AppendOnlyStorageWrite_FinishBuffer(storageWrite,
												contentLen,
												executorBlockKind,
												rowCount);
			Assert(storageWrite->currentCompleteHeaderLen == 0);
		}
		else
		{
			/*
			 * Since ~_GetBuffer now takes in a specification of the header
			 * kind, we need to set the header kind so general routines like
			 * ~_CompressAppend will work correctly when writing the small
			 * "fragments
			 */
			storageWrite->getBufferAoHeaderKind = AoHeaderKind_SmallContent;
			AppendOnlyStorageWrite_CompressAppend(storageWrite,
												  content,
												  contentLen,
												  executorBlockKind,
												  rowCount,
												  &compressedLen,
												  &bufferLen);

			/*
			 * Just before finishing the AO Storage buffer with our
			 * non-compressed content, let's verify it.
			 */
			if (gp_appendonly_verify_write_block)
				AppendOnlyStorageWrite_VerifyWriteBlock(storageWrite,
														BufferedAppendCurrentBufferPosition(&storageWrite->bufferedAppend),
														bufferLen,
														content,
														contentLen,
														executorBlockKind,
														rowCount,
														compressedLen);

			storageWrite->logicalBlockStartOffset =
				BufferedAppendNextBufferPosition(&(storageWrite->bufferedAppend));

			/*
			 * Finish the current buffer by specifying the used length.
			 */
			BufferedAppendFinishBuffer(&storageWrite->bufferedAppend,
									   bufferLen,
									   storageWrite->currentCompleteHeaderLen +
									   AOStorage_RoundUp(contentLen, storageWrite->formatVersion) /* non-compressed size */ );

			/* Declare it finished. */
			storageWrite->currentCompleteHeaderLen = 0;
		}
	}
	else
	{
		int32		largeContentHeaderLen;
		uint8	   *largeContentHeader;
		int32		smallContentHeaderLen;
		int32		maxSmallContentLen;
		int32		countdownContentLen;
		uint8	   *contentNext;
		int32		smallContentLen;

		/*
		 * Write the "Large" content in fragments.
		 */

		storageWrite->logicalBlockStartOffset =
			BufferedAppendNextBufferPosition(&(storageWrite->bufferedAppend));

		/*
		 * First is a LargeContent header-only block that has the "large"
		 * content length and "large" row count.
		 */
		largeContentHeaderLen = AppendOnlyStorageWrite_LargeContentHeaderLen(storageWrite);

		largeContentHeader =
			BufferedAppendGetBuffer(&storageWrite->bufferedAppend,
									largeContentHeaderLen);

		AppendOnlyStorageFormat_MakeLargeContentHeader(largeContentHeader,
													   storageWrite->storageAttributes.checksum,
													   storageWrite->isFirstRowNumSet,
													   storageWrite->formatVersion,
													   storageWrite->firstRowNum,
													   executorBlockKind,
													   rowCount,
													   contentLen);

		BufferedAppendFinishBuffer(&storageWrite->bufferedAppend,
								   largeContentHeaderLen,
								   largeContentHeaderLen);

		/* Declare it finished. */
		storageWrite->currentCompleteHeaderLen = 0;

		/*
		 * Now write the fragments as type Block.
		 */
		storageWrite->isFirstRowNumSet = false; /* Not written with fragments. */

		smallContentHeaderLen =
			AppendOnlyStorageWrite_CompleteHeaderLen(storageWrite,
													 AoHeaderKind_SmallContent);
		maxSmallContentLen = storageWrite->maxBufferLen - smallContentHeaderLen;
		countdownContentLen = contentLen;
		contentNext = content;
		while (true)
		{
			if (countdownContentLen <= maxSmallContentLen)
				smallContentLen = countdownContentLen;
			else
				smallContentLen = maxSmallContentLen;

			if (!storageWrite->storageAttributes.compress)
			{
				data = AppendOnlyStorageWrite_GetBuffer(storageWrite,
														AoHeaderKind_SmallContent);

				memcpy(data, contentNext, smallContentLen);

				AppendOnlyStorageWrite_FinishBuffer(storageWrite,
													smallContentLen,
													executorBlockKind,
													 /* rowCount */ 0);
			}
			else
			{
				/*
				 * Since ~_GetBuffer now takes in a specification of the
				 * header kind, we need to set the header kind so general
				 * routines like ~_CompressAppend will work correctly when
				 * writing the small "fragments
				 */
				storageWrite->getBufferAoHeaderKind = AoHeaderKind_SmallContent;

				AppendOnlyStorageWrite_CompressAppend(storageWrite,
													  contentNext,
													  smallContentLen,
													  executorBlockKind,
													   /* rowCount */ 0,
													  &compressedLen,
													  &bufferLen);

				/*
				 * Just before finishing the AO Storage buffer with our
				 * non-compressed content, let's verify it.
				 */
				if (gp_appendonly_verify_write_block)
					AppendOnlyStorageWrite_VerifyWriteBlock(storageWrite,
															BufferedAppendCurrentBufferPosition(&storageWrite->bufferedAppend),
															bufferLen,
															contentNext,
															smallContentLen,
															executorBlockKind,
															 /* rowCount */ 0,
															compressedLen);

				/*
				 * Finish the current buffer by specifying the used length.
				 */
				BufferedAppendFinishBuffer(&storageWrite->bufferedAppend,
										   bufferLen,
										   smallContentHeaderLen +
										   AOStorage_RoundUp(smallContentLen, storageWrite->formatVersion) /* non-compressed size */ );

				/* Declare it finished. */
				storageWrite->currentCompleteHeaderLen = 0;
			}

			countdownContentLen -= smallContentLen;
			if (countdownContentLen == 0)
				break;

			contentNext += smallContentLen;
		}
	}

	storageWrite->isFirstRowNumSet = false;

	/* Verify we have no buffer allocated. */
	Assert(storageWrite->currentCompleteHeaderLen == 0);
}

/*----------------------------------------------------------------
 * Optional: Set First Row Number
 *----------------------------------------------------------------
 */


/*
 * Normally, the first row of an Append-Only Storage Block is implicit. It
 * is the last row number of the previous block + 1. However, to support
 * BTree indexes that stored TIDs in shared-memory/disk before the
 * transaction commits, we may need to not reuse row numbers of aborted
 * transactions.  So, this routine tells the Append-Only Storage Layer to
 * explicitly keep the first row number. This will take up more header
 * overhead, so the AppendOnlyStorageWrite_HeaderLen routine should be
 * called afterwards to get the new overhead length.
 */


/*
 * Set the first row value for the next Append-Only Storage Block to be
 * written.  Only applies to the next block.
 */
void
AppendOnlyStorageWrite_SetFirstRowNum(AppendOnlyStorageWrite *storageWrite,
									  int64 firstRowNum)
{
	Assert(storageWrite != NULL);
	Assert(storageWrite->isActive);

	/* UNDONE: Range check firstRowNum */

	storageWrite->isFirstRowNumSet = true;
	storageWrite->firstRowNum = firstRowNum;
}
