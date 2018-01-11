/*-------------------------------------------------------------------------
 *
 * cdbappendonlystorageread.c
 *
 * Portions Copyright (c) 2007-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbappendonlystorageread.c
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

#include "catalog/pg_compression.h"
#include "cdb/cdbappendonlystorage.h"
#include "cdb/cdbappendonlystoragelayer.h"
#include "cdb/cdbappendonlystorageformat.h"
#include "cdb/cdbappendonlystorageread.h"
#include "utils/guc.h"


/*----------------------------------------------------------------
 * Initialization
 *----------------------------------------------------------------
 */

/*
 * Initialize AppendOnlyStorageRead.
 *
 * The AppendOnlyStorageRead data structure is initialized once for a read
 * "session" and can be used to read Append-Only Storage Blocks from 1 or
 * more segment files.
 *
 * The current file to read to is opened with the
 * AppendOnlyStorageRead_OpenFile routine.
 *
 * storageRead		- data structure to initialize
 * memoryContext	- memory context to use for buffers and other memory
 *					  needs. When NULL, the current memory context is used.
 * maxBufferLen		- maximum Append-Only Storage Block length including all
 *					  storage headers.
 * relationName		- name of the relation to use in system logging and
 *					  error messages.
 * title			- A phrase that better describes the purpose of this open.
 *					  The caller manages the storage for this.
 * storageAttributes - Append-Only Storage Attributes from relation creation.
 */
void
AppendOnlyStorageRead_Init(AppendOnlyStorageRead *storageRead,
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

	Assert(storageRead != NULL);

	/* UNDONE: Range check maxBufferLen */

	Assert(relationName != NULL);
	Assert(storageAttributes != NULL);

	/* UNDONE: Range check fields in storageAttributes */

	MemSet(storageRead, 0, sizeof(AppendOnlyStorageRead));

	storageRead->maxBufferLen = maxBufferLen;

	if (memoryContext == NULL)
		storageRead->memoryContext = CurrentMemoryContext;
	else
		storageRead->memoryContext = memoryContext;

	oldMemoryContext = MemoryContextSwitchTo(storageRead->memoryContext);

	memcpy(&storageRead->storageAttributes,
		   storageAttributes,
		   sizeof(AppendOnlyStorageAttributes));

	relationNameLen = strlen(relationName);
	storageRead->relationName = (char *) palloc(relationNameLen + 1);
	memcpy(storageRead->relationName, relationName, relationNameLen + 1);

	storageRead->title = title;

	storageRead->minimumHeaderLen =
		AppendOnlyStorageFormat_RegularHeaderLenNeeded(
													   storageRead->storageAttributes.checksum);

	/*
	 * Initialize BufferedRead.
	 */
	storageRead->largeReadLen = 2 * storageRead->maxBufferLen;

	memoryLen = BufferedReadMemoryLen(storageRead->maxBufferLen,
									  storageRead->largeReadLen);

	Assert(CurrentMemoryContext == storageRead->memoryContext);
	memory = (uint8 *) palloc(memoryLen);

	BufferedReadInit(&storageRead->bufferedRead,
					 memory,
					 memoryLen,
					 storageRead->maxBufferLen,
					 storageRead->largeReadLen,
					 relationName);

	elogif(Debug_appendonly_print_scan || Debug_appendonly_print_read_block, LOG,
		   "Append-Only Storage Read initialize for table '%s' "
		   "(compression = %s, compression level %d, maximum buffer length %d, large read length %d)",
		   storageRead->relationName,
		   (storageRead->storageAttributes.compress ? "true" : "false"),
		   storageRead->storageAttributes.compressLevel,
		   storageRead->maxBufferLen,
		   storageRead->largeReadLen);

	storageRead->file = -1;
	storageRead->formatVersion = -1;

	MemoryContextSwitchTo(oldMemoryContext);

	storageRead->isActive = true;
}

/*
 * Return (read-only) pointer to relation name.
 */
char *
AppendOnlyStorageRead_RelationName(AppendOnlyStorageRead *storageRead)
{
	Assert(storageRead != NULL);
	Assert(storageRead->isActive);

	return storageRead->relationName;
}

/*
 * Return (read-only) pointer to relation name.
 */
char *
AppendOnlyStorageRead_SegmentFileName(AppendOnlyStorageRead *storageRead)
{
	Assert(storageRead != NULL);
	Assert(storageRead->isActive);

	return storageRead->segmentFileName;
}

/*
 * Finish using the AppendOnlyStorageRead session created with ~Init.
 */
void
AppendOnlyStorageRead_FinishSession(AppendOnlyStorageRead *storageRead)
{
	MemoryContext oldMemoryContext;

	if (!storageRead->isActive)
		return;

	oldMemoryContext = MemoryContextSwitchTo(storageRead->memoryContext);

	/*
	 * UNDONE: This expects the MemoryContext to be what was used for the
	 * 'memory' in ~Init
	 */
	BufferedReadFinish(&storageRead->bufferedRead);

	if (storageRead->relationName != NULL)
	{
		pfree(storageRead->relationName);
		storageRead->relationName = NULL;
	}

	if (storageRead->segmentFileName != NULL)
	{
		pfree(storageRead->segmentFileName);
		storageRead->segmentFileName = NULL;
	}

	if (storageRead->compression_functions != NULL)
	{
		callCompressionDestructor(storageRead->compression_functions[COMPRESSION_DESTRUCTOR], storageRead->compressionState);
		if (storageRead->compressionState != NULL)
		{
			pfree(storageRead->compressionState);
			storageRead->compressionState = NULL;
		}
		pfree(storageRead->compression_functions);
		storageRead->compression_functions = NULL;
	}

	/* Deallocation is done.      Go back to caller memory-context. */
	MemoryContextSwitchTo(oldMemoryContext);

	storageRead->isActive = false;

}

/*----------------------------------------------------------------
 * Open and Close
 *----------------------------------------------------------------
 */

/*
 * Do open the next segment file to read, but don't do error processing.
 *
 * This routine is responsible for seeking to the proper location given
 * the logical EOF.
 *
 * filePathName - name of the segment file to open.
 */
static File
AppendOnlyStorageRead_DoOpenFile(AppendOnlyStorageRead *storageRead,
								 char *filePathName)
{
	int			fileFlags = O_RDONLY | PG_BINARY;

	/* File mode is S_IRUSR 00400 user has read permission */
	int			fileMode = 0400;
	File		file;

	Assert(storageRead != NULL);
	Assert(storageRead->isActive);
	Assert(filePathName != NULL);

	elogif(Debug_appendonly_print_read_block, LOG,
		   "Append-Only storage read: opening table '%s', segment file '%s', fileFlags 0x%x, fileMode 0x%x",
		   storageRead->relationName,
		   storageRead->segmentFileName,
		   fileFlags,
		   fileMode);

	/*
	 * Open the file for read.
	 */
	file = PathNameOpenFile(filePathName, fileFlags, fileMode);

	return file;
}

/*
 * Finish the open by positioning the next read and saving information.
 *
 * file			- The open file.
 * filePathName - name of the segment file to open.
 * version		- AO table format version the file is in.
 * logicalEof	- snapshot version of the EOF value to use as the read end
 *				  of the segment file.
 */
static void
AppendOnlyStorageRead_FinishOpenFile(AppendOnlyStorageRead *storageRead,
									 File file,
									 char *filePathName,
									 int version,
									 int64 logicalEof)
{
	int64		seekResult;
	MemoryContext oldMemoryContext;
	int			segmentFileNameLen;

	AORelationVersion_CheckValid(version);

	/*
	 * Seek to the beginning of the file.
	 */
	seekResult = FileSeek(file, 0, SEEK_SET);
	if (seekResult != 0)
	{
		FileClose(file);
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				 errmsg("Append-only Storage Read error on segment file '%s' for relation '%s'.  FileSeek offset = 0.  Error code = %d (%s)",
						filePathName,
						storageRead->relationName,
						(int) seekResult,
						strerror((int) seekResult))));
	}

	storageRead->file = file;
	storageRead->formatVersion = version;

	/*
	 * When reading multiple segment files, we throw away the old segment file
	 * name strings.
	 */
	oldMemoryContext = MemoryContextSwitchTo(storageRead->memoryContext);

	if (storageRead->segmentFileName != NULL)
		pfree(storageRead->segmentFileName);

	segmentFileNameLen = strlen(filePathName);
	storageRead->segmentFileName = (char *) palloc(segmentFileNameLen + 1);
	memcpy(storageRead->segmentFileName, filePathName, segmentFileNameLen + 1);

	/* Allocation is done.  Go back to caller memory-context. */
	MemoryContextSwitchTo(oldMemoryContext);

	storageRead->logicalEof = logicalEof;

	BufferedReadSetFile(
						&storageRead->bufferedRead,
						storageRead->file,
						storageRead->segmentFileName,
						logicalEof);
}

/*
 * Open the next segment file to read.
 *
 * This routine is responsible for seeking to the proper
 * read location given the logical EOF.
 *
 * filePathName - name of the segment file to open.
 * version		- AO table format version the file is in.
 * logicalEof	- snapshot version of the EOF value to use as the read end
 *				  of the segment file.
 */
void
AppendOnlyStorageRead_OpenFile(AppendOnlyStorageRead *storageRead,
							   char *filePathName,
							   int version,
							   int64 logicalEof)
{
	File		file;

	Assert(storageRead != NULL);
	Assert(storageRead->isActive);
	Assert(filePathName != NULL);

	/*
	 * The EOF must be be greater than 0, otherwise we risk transactionally
	 * created segment files from disappearing if a concurrent write
	 * transaction aborts.
	 */
	if (logicalEof == 0)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Append-only Storage Read segment file '%s' EOF must be > 0 for relation '%s'",
						filePathName,
						storageRead->relationName)));

	file = AppendOnlyStorageRead_DoOpenFile(storageRead,
											filePathName);
	if (file < 0)
	{
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("Append-Only Storage Read could not open segment file '%s' for relation '%s'",
						filePathName,
						storageRead->relationName)));
	}

	AppendOnlyStorageRead_FinishOpenFile(storageRead,
										 file,
										 filePathName,
										 version,
										 logicalEof);
}

/*
 * Try opening the next segment file to read.
 *
 * This routine is responsible for seeking to the proper read location given
 * the logical EOF.
 *
 * filePathName - name of the segment file to open
 * version		- AO table format version the file is in.
 * logicalEof	- snapshot version of the EOF value to use as the read end of
 *				  the segment file.
 */
bool
AppendOnlyStorageRead_TryOpenFile(AppendOnlyStorageRead *storageRead,
								  char *filePathName,
								  int version,
								  int64 logicalEof)
{
	File		file;

	Assert(storageRead != NULL);
	Assert(storageRead->isActive);
	Assert(filePathName != NULL);
	/* UNDONE: Range check logicalEof */

	file = AppendOnlyStorageRead_DoOpenFile(storageRead,
											filePathName);
	if (file < 0)
		return false;

	AppendOnlyStorageRead_FinishOpenFile(storageRead,
										 file,
										 filePathName,
										 version,
										 logicalEof);

	return true;
}

/*
 * Set a temporary read range in the current open segment file.
 *
 * The beginFileOffset must be to the beginning of an Append-Only Storage block.
 *
 * The afterFileOffset serves as the temporary EOF.  It will cause
 * ~_GetBlockInfo to return false (no more blocks) when reached.  It must be
 * at the end of an Append-Only Storage block.
 *
 * When ~_GetBlockInfo returns false (no more blocks), the temporary read
 * range is forgotten.
 */
void
AppendOnlyStorageRead_SetTemporaryRange(AppendOnlyStorageRead *storageRead,
										int64 beginFileOffset,
										int64 afterFileOffset)
{
	Assert(storageRead->isActive);
	Assert(storageRead->file != -1);
	Assert(storageRead->formatVersion != -1);
	Assert(beginFileOffset >= 0);
	Assert(beginFileOffset <= storageRead->logicalEof);
	Assert(afterFileOffset >= 0);
	Assert(afterFileOffset <= storageRead->logicalEof);

	BufferedReadSetTemporaryRange(&storageRead->bufferedRead,
								  beginFileOffset,
								  afterFileOffset);
}

/*
 * Close the current segment file.
 *
 * No error if the current is already closed.
 */
void
AppendOnlyStorageRead_CloseFile(AppendOnlyStorageRead *storageRead)
{
	if (!storageRead->isActive)
		return;

	if (storageRead->file == -1)
		return;

	FileClose(storageRead->file);

	storageRead->file = -1;
	storageRead->formatVersion = -1;

	storageRead->logicalEof = INT64CONST(0);

	if (storageRead->bufferedRead.file >= 0)
		BufferedReadCompleteFile(&storageRead->bufferedRead);
}


/*----------------------------------------------------------------
 * Reading Content
 *----------------------------------------------------------------
 */

/*
 * This section describes for reading potentially long content that can be
 * up to 1 Gb long and and/or content may have been be bulk-compressed.
 *
 * The AppendOnlyStorageRead_GetBlockInfo routine is used to peek at the
 * next Append-Only Storage Block and tell the caller how to handle it.
 *
 * If the block is small and not compressed, then it may be looked at
 * directly in the read buffer.
 *
 * Otherwise, the caller must provide other buffer space to either
 * reconstruct large content and/or to decompress content into.
 */

/*
 * Skip zero padding to next page boundary, if necessary.
 *
 * This function is called when the file system block we are scanning has
 * no more valid data but instead is padded with zero's from the position
 * we are currently in until the end of the block. The function will skip
 * to the end of block if skipLen is -1 or skip skipLen bytes otherwise.
 */
static void
AppendOnlyStorageRead_DoSkipPadding(AppendOnlyStorageRead *storageRead,
									int32 skipLen)
{
	int64		nextReadPosition;
	int64		nextBoundaryPosition;
	int32		safeWriteRemainder;
	bool		doSkip;
	uint8	   *buffer;
	int32		availableLen;
	int32		safewrite = storageRead->storageAttributes.safeFSWriteSize;

	/* early exit if no pad used */
	if (safewrite == 0)
		return;

	nextReadPosition =
		BufferedReadNextBufferPosition(&storageRead->bufferedRead);
	nextBoundaryPosition =
		((nextReadPosition + safewrite - 1) / safewrite) * safewrite;
	safeWriteRemainder = (int32) (nextBoundaryPosition - nextReadPosition);

	if (safeWriteRemainder <= 0)
		doSkip = false;
	else if (skipLen == -1)
	{
		/*
		 * Skip to end of page.
		 */
		doSkip = true;
		skipLen = safeWriteRemainder;
	}
	else
		doSkip = (safeWriteRemainder < skipLen);

	if (doSkip)
	{
		/*
		 * Read through the remainder.
		 */
		buffer = BufferedReadGetNextBuffer(&storageRead->bufferedRead,
										   safeWriteRemainder,
										   &availableLen);

		/*
		 * Since our file EOF should always be a multiple of the file-system
		 * page, we do not expect a short read here.
		 */
		if (buffer == NULL)
			availableLen = 0;
		if (buffer == NULL || safeWriteRemainder != availableLen)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("Unexpected end of file.  Expected to read %d bytes after position " INT64_FORMAT " but found %d bytes (bufferCount  " INT64_FORMAT ")\n",
							safeWriteRemainder,
							nextReadPosition,
							availableLen,
							storageRead->bufferCount)));
		}

		/*
		 * UNDONE: For verification purposes, we should verify the remainder
		 * is all zeroes.
		 */

		elogif(Debug_appendonly_print_scan, LOG,
			   "Append-only scan skipping zero padded remainder for table '%s' (nextReadPosition = " INT64_FORMAT ", safeWriteRemainder = %d)",
			   storageRead->relationName,
			   nextReadPosition,
			   safeWriteRemainder);
	}
}

/*
 * Skip zero padding to next page boundary, if necessary.
 *
 * This function is called when the file system block we are scanning has
 * no more valid data but instead is padded with zero's from the position
 * we are currently in until the end of the block. The function will skip
 * to the end of block if skipLen is -1 or skip skipLen bytes otherwise.
 */
static bool
AppendOnlyStorageRead_PositionToNextBlock(AppendOnlyStorageRead *storageRead,
										  int64 *headerOffsetInFile,
										  uint8 **header,
										  int32 *blockLimitLen)
{
	int32		availableLen;
	int			i;
	int64		fileRemainderLen;

	Assert(storageRead != NULL);
	Assert(header != NULL);

	/*
	 * Peek ahead just enough so we can see the Append-Only storage header.
	 *
	 * However, we need to honor the file-system page boundaries here since we
	 * do not let the length information cross the boundary.
	 */
	AppendOnlyStorageRead_DoSkipPadding(storageRead, storageRead->minimumHeaderLen);

	*headerOffsetInFile =
		BufferedReadNextBufferPosition(&storageRead->bufferedRead);

	*header = BufferedReadGetNextBuffer(&storageRead->bufferedRead,
										storageRead->minimumHeaderLen,
										&availableLen);

	if (*header == NULL)
	{
		/* done reading the file */
		return false;
	}

	storageRead->bufferCount++;

	if (availableLen != storageRead->minimumHeaderLen)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Expected %d bytes and got %d bytes in table %s (segment file '%s', header offset in file = " INT64_FORMAT ", bufferCount " INT64_FORMAT ")\n",
						storageRead->minimumHeaderLen,
						availableLen,
						storageRead->relationName,
						storageRead->segmentFileName,
						*headerOffsetInFile,
						storageRead->bufferCount)));

	/*
	 * First check for zero padded page remainder.
	 */
	i = 0;
	while (true)
	{
		if ((*header)[i] != 0)
			break;

		i++;
		if (i >= storageRead->minimumHeaderLen)
		{
			/*
			 * Skip over zero padding caused when the append command left a
			 * partially full page.
			 */
			AppendOnlyStorageRead_DoSkipPadding(storageRead,
												-1 /* means till end of page */ );

			/*
			 * Now try to get the peek data from the new page.
			 */
			*headerOffsetInFile = BufferedReadNextBufferPosition(&storageRead->bufferedRead);

			*header = BufferedReadGetNextBuffer(&storageRead->bufferedRead,
												storageRead->minimumHeaderLen,
												&availableLen);

			if (*header == NULL)
			{
				/* done reading the file */
				return false;
			}

			if (availableLen != storageRead->minimumHeaderLen)
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("Expected %d bytes and found %d bytes in table %s (segment file '%s', header offset in file = " INT64_FORMAT ", bufferCount " INT64_FORMAT ")",
								storageRead->minimumHeaderLen,
								availableLen,
								storageRead->relationName,
								storageRead->segmentFileName,
								*headerOffsetInFile,
								storageRead->bufferCount)));
			break;
		}
	}

	/*
	 * Determine the maximum boundary of the block. UNDONE: When we have a
	 * block directory, we will tighten the limit down.
	 */
	fileRemainderLen = storageRead->bufferedRead.fileLen -
		*headerOffsetInFile;
	if (storageRead->maxBufferLen > fileRemainderLen)
		*blockLimitLen = (int32) fileRemainderLen;
	else
		*blockLimitLen = storageRead->maxBufferLen;

	return true;
}


char *
AppendOnlyStorageRead_ContextStr(AppendOnlyStorageRead *storageRead)
{
	StringInfoData buf;
	int64		headerOffsetInFile;

	headerOffsetInFile =
		BufferedReadCurrentPosition(&storageRead->bufferedRead);

	initStringInfo(&buf);
	appendStringInfo(&buf,
					 "%s. Append-Only segment file '%s', block header offset in file = " INT64_FORMAT ", bufferCount " INT64_FORMAT,
					 storageRead->title,
					 storageRead->segmentFileName,
					 headerOffsetInFile,
					 storageRead->bufferCount);

	return buf.data;
}

/*
 * errcontext_appendonly_read_storage_block
 *
 * Add an errcontext() line showing the table, segment file, offset in file,
 * block count of the storage block being read.
 */
int
errcontext_appendonly_read_storage_block(AppendOnlyStorageRead *storageRead)
{
	char	   *str;

	str = AppendOnlyStorageRead_ContextStr(storageRead);
	errcontext("%s", str);
	pfree(str);

	return 0;
}

char *
AppendOnlyStorageRead_StorageContentHeaderStr(AppendOnlyStorageRead *storageRead)
{
	uint8	   *header;

	header = BufferedReadGetCurrentBuffer(&storageRead->bufferedRead);

	return AppendOnlyStorageFormat_BlockHeaderStr(header,
												  storageRead->storageAttributes.checksum,
												  storageRead->formatVersion);
}

/*
 * errdetail_appendonly_read_storage_content_header
 *
 * Add an errdetail() line showing the Append-Only Storage header being read.
 */
int
errdetail_appendonly_read_storage_content_header(AppendOnlyStorageRead *storageRead)
{
	char	   *str;

	str = AppendOnlyStorageRead_StorageContentHeaderStr(storageRead);

	errdetail("%s", str);

	pfree(str);

	return 0;
}

static void
AppendOnlyStorageRead_LogBlockHeader(AppendOnlyStorageRead *storageRead,
									 uint8 *header)
{
	char	   *contextStr;
	char	   *blockHeaderStr;

	contextStr = AppendOnlyStorageRead_ContextStr(storageRead);

	blockHeaderStr =
		AppendOnlyStorageFormat_SmallContentHeaderStr(header,
													  storageRead->storageAttributes.checksum,
													  storageRead->formatVersion);
	ereport(LOG,
			(errmsg("%s. %s",
					contextStr,
					blockHeaderStr)));

	pfree(contextStr);
	pfree(blockHeaderStr);
}

/*
 * Get information on the next Append-Only Storage Block.
 *
 * Return true if another block was found.  Otherwise, we have reached the
 * end of the current segment file.
 */
bool
AppendOnlyStorageRead_ReadNextBlock(AppendOnlyStorageRead *storageRead)
{
	uint8	   *header;
	AOHeaderCheckError checkError;
	int32		blockLimitLen = 0;	/* Shutup compiler. */
	pg_crc32	storedChecksum;
	pg_crc32	computedChecksum;

	/*
	 * Reset current* variables.
	 */

	/* For efficiency, zero out.  Comment out lines that set fields to 0. */
	memset(&storageRead->current, 0, sizeof(AppendOnlyStorageReadCurrent));

/*	storageRead->current.headerOffsetInFile = 0; */
	storageRead->current.headerKind = AoHeaderKind_None;
/*	storageRead->current.actualHeaderLen = 0; */
/*	storageRead->current.contentLen = 0; */
/*	storageRead->current.overallBlockLen = 0; */
/*	storageRead->current.contentOffset = 0; */
/*	storageRead->current.executorBlockKind = 0; */
/*	storageRead->current.hasFirstRowNum = false; */
	storageRead->current.firstRowNum = INT64CONST(-1);
/*	storageRead->current.rowCount = 0; */
/*	storageRead->current.isLarge = false; */
/*	storageRead->current.isCompressed = false; */
/*	storageRead->current.compressedLen = 0; */

	elogif(Debug_appendonly_print_datumstream, LOG,
		   "before AppendOnlyStorageRead_PositionToNextBlock, storageRead->current.headerOffsetInFile is" INT64_FORMAT "storageRead->current.overallBlockLen is %d",
		   storageRead->current.headerOffsetInFile, storageRead->current.overallBlockLen);

	if (!AppendOnlyStorageRead_PositionToNextBlock(storageRead,
												   &storageRead->current.headerOffsetInFile,
												   &header,
												   &blockLimitLen))
	{
		/* Done reading the file */
		return false;
	}

	elogif(Debug_appendonly_print_datumstream, LOG,
		   "after AppendOnlyStorageRead_PositionToNextBlock, storageRead->current.headerOffsetInFile is" INT64_FORMAT "storageRead->current.overallBlockLen is %d",
		   storageRead->current.headerOffsetInFile, storageRead->current.overallBlockLen);

	/*----------
	 * Proceed very carefully:
	 * [ 1. Verify header checksum ]
	 *	 2. Examine (basic) header.
	 *	 3. Examine specific header.
	 * [ 4. Verify the block checksum ]
	 *----------
	 */
	if (storageRead->storageAttributes.checksum &&
		gp_appendonly_verify_block_checksums)
	{
		if (!AppendOnlyStorageFormat_VerifyHeaderChecksum(header,
														  &storedChecksum,
														  &computedChecksum))
			ereport(ERROR,
					(errcode(ERRCODE_DATA_CORRUPTED),
					 errmsg("Header checksum does not match.  Expected 0x%X and found 0x%X ",
							storedChecksum,
							computedChecksum),
					 errdetail_appendonly_read_storage_content_header(storageRead),
					 errcontext_appendonly_read_storage_block(storageRead)));
	}

	/*
	 * Check the (basic) header information.
	 */
	checkError = AppendOnlyStorageFormat_GetHeaderInfo(header,
													   storageRead->storageAttributes.checksum,
													   &storageRead->current.headerKind,
													   &storageRead->current.actualHeaderLen);
	if (checkError != AOHeaderCheckOk)
		ereport(ERROR,
				(errcode(ERRCODE_DATA_CORRUPTED),
				 errmsg("Bad append-only storage header.  Header check error %d, detail '%s'",
						(int) checkError,
						AppendOnlyStorageFormat_GetHeaderCheckErrorStr()),
				 errdetail_appendonly_read_storage_content_header(storageRead),
				 errcontext_appendonly_read_storage_block(storageRead)));

	/*
	 * Get more header since AppendOnlyStorageRead_PositionToNextBlock only
	 * gets minimum.
	 */
	if (storageRead->minimumHeaderLen < storageRead->current.actualHeaderLen)
	{
		int32		availableLen;

		header = BufferedReadGrowBuffer(&storageRead->bufferedRead,
										storageRead->current.actualHeaderLen,
										&availableLen);

		if (header == NULL ||
			availableLen != storageRead->current.actualHeaderLen)
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("Expected %d bytes and found %d bytes in table %s "
							"(segment file '%s', header offset in file = " INT64_FORMAT ", bufferCount " INT64_FORMAT ")",
							storageRead->current.actualHeaderLen,
							availableLen,
							storageRead->relationName,
							storageRead->segmentFileName,
							storageRead->current.headerOffsetInFile,
							storageRead->bufferCount)));
	}

	/*
	 * Based on the kind of header, we either have small or large content.
	 */
	switch (storageRead->current.headerKind)
	{
		case AoHeaderKind_SmallContent:

			/*
			 * Check the SmallContent header information.
			 */
			checkError = AppendOnlyStorageFormat_GetSmallContentHeaderInfo
				(header,
				 storageRead->current.actualHeaderLen,
				 storageRead->storageAttributes.checksum,
				 blockLimitLen,
				 &storageRead->current.overallBlockLen,
				 &storageRead->current.contentOffset,
				 &storageRead->current.uncompressedLen,
				 &storageRead->current.executorBlockKind,
				 &storageRead->current.hasFirstRowNum,
				 storageRead->formatVersion,
				 &storageRead->current.firstRowNum,
				 &storageRead->current.rowCount,
				 &storageRead->current.isCompressed,
				 &storageRead->current.compressedLen
				);
			if (checkError != AOHeaderCheckOk)
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("Bad append-only storage header of type small content. Header check error %d, detail '%s'",
								(int) checkError,
								AppendOnlyStorageFormat_GetHeaderCheckErrorStr()),
						 errdetail_appendonly_read_storage_content_header(storageRead),
						 errcontext_appendonly_read_storage_block(storageRead)));
			break;

		case AoHeaderKind_LargeContent:

			/*
			 * Check the LargeContent metadata header information.
			 */
			checkError = AppendOnlyStorageFormat_GetLargeContentHeaderInfo
				(header,
				 storageRead->current.actualHeaderLen,
				 storageRead->storageAttributes.checksum,
				 &storageRead->current.uncompressedLen,
				 &storageRead->current.executorBlockKind,
				 &storageRead->current.hasFirstRowNum,
				 &storageRead->current.firstRowNum,
				 &storageRead->current.rowCount);
			if (checkError != AOHeaderCheckOk)
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("Bad append-only storage header of type large content. Header check error %d, detail '%s'",
								(int) checkError,
								AppendOnlyStorageFormat_GetHeaderCheckErrorStr()),
						 errdetail_appendonly_read_storage_content_header(storageRead),
						 errcontext_appendonly_read_storage_block(storageRead)));
			storageRead->current.isLarge = true;
			break;

		case AoHeaderKind_NonBulkDenseContent:

			/*
			 * Check the NonBulkDense header information.
			 */
			checkError =
				AppendOnlyStorageFormat_GetNonBulkDenseContentHeaderInfo
				(header,
				 storageRead->current.actualHeaderLen,
				 storageRead->storageAttributes.checksum,
				 blockLimitLen,
				 &storageRead->current.overallBlockLen,
				 &storageRead->current.contentOffset,
				 &storageRead->current.uncompressedLen,
				 &storageRead->current.executorBlockKind,
				 &storageRead->current.hasFirstRowNum,
				 storageRead->formatVersion,
				 &storageRead->current.firstRowNum,
				 &storageRead->current.rowCount
				);
			if (checkError != AOHeaderCheckOk)
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("Bad append-only storage header of type non-bulk dense content. Header check error %d, detail '%s'",
								(int) checkError,
								AppendOnlyStorageFormat_GetHeaderCheckErrorStr()),
						 errdetail_appendonly_read_storage_content_header(storageRead),
						 errcontext_appendonly_read_storage_block(storageRead)));
			break;

		case AoHeaderKind_BulkDenseContent:

			/*
			 * Check the BulkDenseContent header information.
			 */
			checkError =
				AppendOnlyStorageFormat_GetBulkDenseContentHeaderInfo
				(header,
				 storageRead->current.actualHeaderLen,
				 storageRead->storageAttributes.checksum,
				 blockLimitLen,
				 &storageRead->current.overallBlockLen,
				 &storageRead->current.contentOffset,
				 &storageRead->current.uncompressedLen,
				 &storageRead->current.executorBlockKind,
				 &storageRead->current.hasFirstRowNum,
				 storageRead->formatVersion,
				 &storageRead->current.firstRowNum,
				 &storageRead->current.rowCount,
				 &storageRead->current.isCompressed,
				 &storageRead->current.compressedLen
				);
			if (checkError != AOHeaderCheckOk)
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("Bad append-only storage header of type bulk dense content. Header check error %d, detail '%s'",
								(int) checkError,
								AppendOnlyStorageFormat_GetHeaderCheckErrorStr()),
						 errdetail_appendonly_read_storage_content_header(storageRead),
						 errcontext_appendonly_read_storage_block(storageRead)));
			break;

		default:
			elog(ERROR, "Unexpected Append-Only header kind %d",
				 storageRead->current.headerKind);
			break;
	}

	if (Debug_appendonly_print_storage_headers)
	{
		AppendOnlyStorageRead_LogBlockHeader(storageRead, header);
	}

	if (storageRead->current.hasFirstRowNum)
	{
		/* UNDONE: Grow buffer and read the value into firstRowNum. */
	}

	if (storageRead->current.headerKind == AoHeaderKind_LargeContent)
	{
		/* UNDONE: Finish the read for the information only header. */
	}

	return true;
}

/*
 * Get information on the next Append-Only Storage Block.
 *
 * Return true if another block was found.  Otherwise, when we have reached
 * the end of the current segment file.
 *
 * OUTPUTS:
 *
 * contentLen	- total byte length of the content.
 * executorBlockKind - executor supplied value stored in the Append-Only
 *				  Storage Block header
 * firstRowNum	- When the first row number for this block was explicitly set,
 *				  that value is returned here.  Otherwise, INT64CONST(-1) is
 *				  returned.
 * rowCount		- number of rows in the content
 * isLarge		- When true, the content was longer than the maxBufferLen
 *				  (i.e. blocksize) minus Append-Only Storage Block header and
 *				  had to be stored in more than one storage block.
 * isCompressed - When true, the content is compressed and cannot be looked at
 *				  directly in the buffer.
 */
bool
AppendOnlyStorageRead_GetBlockInfo(AppendOnlyStorageRead *storageRead,
								   int32 *contentLen,
								   int *executorBlockKind,
								   int64 *firstRowNum,
								   int *rowCount,
								   bool *isLarge,
								   bool *isCompressed)
{
	bool		isNext;

	Assert(storageRead != NULL);
	Assert(storageRead->isActive);

	isNext = AppendOnlyStorageRead_ReadNextBlock(storageRead);

	/*
	 * The current* variables have good values even when there is no next
	 * block.
	 */
	*contentLen = storageRead->current.uncompressedLen;
	*executorBlockKind = storageRead->current.executorBlockKind;
	*firstRowNum = storageRead->current.firstRowNum;
	*rowCount = storageRead->current.rowCount;
	*isLarge = storageRead->current.isLarge;
	*isCompressed = storageRead->current.isCompressed;

	return isNext;
}

/*
 * Return the file offset of the current Append-Only Storage Block.
 */
int64
AppendOnlyStorageRead_CurrentHeaderOffsetInFile(AppendOnlyStorageRead *storageRead)
{
	Assert(storageRead != NULL);
	Assert(storageRead->isActive);

	return storageRead->current.headerOffsetInFile;
}

/*
 * Return the compressed length of the content of the current Append-Only
 * Storage Block.
 */
int64
AppendOnlyStorageRead_CurrentCompressedLen(AppendOnlyStorageRead *storageRead)
{
	Assert(storageRead != NULL);
	Assert(storageRead->isActive);

	return storageRead->current.compressedLen;
}

/*
 * Return the overall block length of the current Append-Only Storage Block.
 */
int64
AppendOnlyStorageRead_OverallBlockLen(AppendOnlyStorageRead *storageRead)
{
	Assert(storageRead != NULL);
	Assert(storageRead->isActive);

	return storageRead->current.overallBlockLen;
}

/*
 * Internal routine to grow the BufferedRead buffer to be the whole current
 * block and to get header and content pointers of current block.
 *
 * Since we are growing the BufferedRead buffer to the whole block, old
 * pointers to the header must be abandoned.
 *
 * Header to current block was read and verified by
 * AppendOnlyStorageRead_ReadNextBlock.
 */
static void
AppendOnlyStorageRead_InternalGetBuffer(AppendOnlyStorageRead *storageRead,
										uint8 **header, uint8 **content)
{
	int32		availableLen;
	pg_crc32	storedChecksum;
	pg_crc32	computedChecksum;

	/*
	 * Verify next block is type Block.
	 */
	Assert(storageRead->current.headerKind == AoHeaderKind_SmallContent ||
		   storageRead->current.headerKind == AoHeaderKind_NonBulkDenseContent ||
		   storageRead->current.headerKind == AoHeaderKind_BulkDenseContent);

	/*
	 * Grow the buffer to the full block length to avoid any unnecessary
	 * copying by BufferedRead.
	 *
	 * Since the BufferedRead module may have to copy information around, we
	 * do not save any pointers to the prior buffer call.  This why
	 * AppendOnlyStorageFormat_GetHeaderInfo passes back the offset to the
	 * data, not a pointer.
	 */
	*header = BufferedReadGrowBuffer(&storageRead->bufferedRead,
									 storageRead->current.overallBlockLen,
									 &availableLen);

	if (storageRead->current.overallBlockLen != availableLen)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Wrong buffer length.  Expected %d byte length buffer and got %d ",
						storageRead->current.overallBlockLen,
						availableLen),
				 errdetail_appendonly_read_storage_content_header(storageRead),
				 errcontext_appendonly_read_storage_block(storageRead)));

	if (storageRead->storageAttributes.checksum &&
		gp_appendonly_verify_block_checksums)
	{
		/*
		 * Now that the header has been verified, verify the block checksum in
		 * the header with the checksum of the data portion.
		 */
		if (!AppendOnlyStorageFormat_VerifyBlockChecksum(*header,
														 storageRead->current.overallBlockLen,
														 &storedChecksum,
														 &computedChecksum))
			ereport(ERROR,
					(errcode(ERRCODE_DATA_CORRUPTED),
					 errmsg("Block checksum does not match.  Expected 0x%X and found 0x%X",
							storedChecksum,
							computedChecksum),
					 errdetail_appendonly_read_storage_content_header(storageRead),
					 errcontext_appendonly_read_storage_block(storageRead)));
	}

	*content = &((*header)[storageRead->current.contentOffset]);
}

/*
 * Get a pointer to the *small* non-compressed content.
 *
 * This interface provides a pointer directly into the read buffer for
 * efficient data use.
 */
uint8 *
AppendOnlyStorageRead_GetBuffer(AppendOnlyStorageRead *storageRead)
{
	uint8	   *header;
	uint8	   *content;

	Assert(storageRead != NULL);
	Assert(storageRead->isActive);

	/*
	 * Verify next block is a "small" non-compressed block.
	 */
	Assert(storageRead->current.headerKind == AoHeaderKind_SmallContent ||
		   storageRead->current.headerKind == AoHeaderKind_NonBulkDenseContent ||
		   storageRead->current.headerKind == AoHeaderKind_BulkDenseContent);
	Assert(!storageRead->current.isLarge);
	Assert(!storageRead->current.isCompressed);

	/*
	 * Fetch pointers to content.
	 */
	AppendOnlyStorageRead_InternalGetBuffer(storageRead,
											&header,
											&content);

	return content;
}

/*
 * Copy the large and/or decompressed content out.
 *
 * The contentOutLen parameter value must match the contentLen from the
 * AppendOnlyStorageReadGetBlockInfo call.
 *
 * Note this routine will work for small non-compressed content, too.
 *
 * contentOut	- memory to receive the contiguous content.
 * contentOutLen - byte length of the contentOut buffer.
 */
void
AppendOnlyStorageRead_Content(AppendOnlyStorageRead *storageRead,
							  uint8 *contentOut,
							  int32 contentOutLen)
{
	Assert(storageRead != NULL);
	Assert(storageRead->isActive);
	Assert(contentOutLen == storageRead->current.uncompressedLen);

	if (storageRead->current.isLarge)
	{
		int64		largeContentPosition;	/* Position of the large content
											 * metadata block. */
		int32		largeContentLen;	/* Total length of the large content. */
		int32		remainingLargeContentLen;	/* The remaining number of
												 * bytes to read for the large
												 * content. */
		uint8	   *contentNext;	/* Pointer inside the contentOut buffer to
									 * put the next byte. */
		int32		regularBlockReadCount;	/* Number of regular blocks read
											 * after the metadata block. */
		int32		regularContentLen;	/* Length of the current regular
										 * block's content. */

		/*
		 * Large content.
		 *
		 * We have the LargeContent "metadata" AO block with the total length
		 * (already read) followed by N SmallContent blocks with the fragments
		 * of the large content.
		 */


		/*
		 * Save any values needed from the current* members since they will be
		 * modifed as we read the regular blocks.
		 */
		largeContentPosition = storageRead->current.headerOffsetInFile;
		largeContentLen = storageRead->current.uncompressedLen;

		/*
		 * Loop to read regular blocks.
		 */
		contentNext = contentOut;
		remainingLargeContentLen = largeContentLen;
		regularBlockReadCount = 0;
		while (true)
		{
			/*
			 * Read next regular block.
			 */
			regularBlockReadCount++;
			if (!AppendOnlyStorageRead_ReadNextBlock(storageRead))
			{
				/*
				 * Unexpected end of file.
				 */
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("Unexpected end of file trying to read block %d of large content in segment file '%s' of table '%s'.  "
								"Large content metadata block is at position " INT64_FORMAT "  "
								"Large content length %d",
								regularBlockReadCount,
								storageRead->segmentFileName,
								storageRead->relationName,
								largeContentPosition,
								largeContentLen)));
			}
			if (storageRead->current.headerKind != AoHeaderKind_SmallContent)
			{
				/*
				 * Unexpected headerKind.
				 */
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("Expected header kind 'Block' for block %d of large content in segment file '%s' of table '%s'.  "
								"Large content metadata block is at position " INT64_FORMAT "  "
								"Large content length %d",
								regularBlockReadCount,
								storageRead->segmentFileName,
								storageRead->relationName,
								largeContentPosition,
								largeContentLen)));
			}
			Assert(!storageRead->current.isLarge);

			regularContentLen = storageRead->current.uncompressedLen;
			remainingLargeContentLen -= regularContentLen;
			if (remainingLargeContentLen < 0)
			{
				/*
				 * Too much data found???
				 */
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("Too much data found after reading %d blocks for large content in segment file '%s' of table '%s'.  "
								"Large content metadata block is at position " INT64_FORMAT "  "
								"Large content length %d; extra data length %d",
								regularBlockReadCount,
								storageRead->segmentFileName,
								storageRead->relationName,
								largeContentPosition,
								largeContentLen,
								-remainingLargeContentLen)));
			}

			/*
			 * We can safely recurse one level here.
			 */
			AppendOnlyStorageRead_Content(storageRead,
										  contentNext,
										  regularContentLen);

			if (remainingLargeContentLen == 0)
				break;

			/*
			 * Advance our pointer inside the contentOut buffer to put the
			 * next bytes.
			 */
			contentNext += regularContentLen;
		}
	}
	else
	{
		uint8	   *header;
		uint8	   *content;

		/*
		 * "Small" content in one regular block.
		 */

		/*
		 * Fetch pointers to content.
		 */
		AppendOnlyStorageRead_InternalGetBuffer(storageRead,
												&header,
												&content);

		if (!storageRead->current.isCompressed)
		{
			/*
			 * Not compressed.
			 */
			memcpy(contentOut,
				   content,
				   storageRead->current.uncompressedLen);

			if (Debug_appendonly_print_scan)
				elog(LOG,
					 "Append-only Storage Read non-compressed block for table '%s' "
					 "(length = %d, segment file '%s', header offset in file = "
					 INT64_FORMAT ", block count " INT64_FORMAT ")",
					 storageRead->relationName,
					 storageRead->current.uncompressedLen,
					 storageRead->segmentFileName,
					 storageRead->current.headerOffsetInFile,
					 storageRead->bufferCount);
		}
		else
		{
			/*
			 * Compressed.
			 */
			PGFunction	decompressor;
			PGFunction *cfns = storageRead->compression_functions;

			if (cfns == NULL)
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("decompression information missing")));

			decompressor = cfns[COMPRESSION_DECOMPRESS];

			gp_decompress_new(content,	/* Compressed data in block. */
							  storageRead->current.compressedLen,
							  contentOut,
							  storageRead->current.uncompressedLen,
							  decompressor,
							  storageRead->compressionState,
							  storageRead->bufferCount);

			if (Debug_appendonly_print_scan)
				elog(LOG,
					 "Append-only Storage Read decompressed block for table '%s' "
					 "(compressed length %d, uncompressed length = %d, segment file '%s', "
					 "header offset in file = " INT64_FORMAT ", block count " INT64_FORMAT ")",
					 storageRead->relationName,
					 AppendOnlyStorageFormat_GetCompressedLen(header),
					 storageRead->current.uncompressedLen,
					 storageRead->segmentFileName,
					 storageRead->current.headerOffsetInFile,
					 storageRead->bufferCount);
		}
	}
}

/*
 * Skip the current block found with ~_GetBlockInfo.
 *
 * Do not decompress the block contents.
 *
 * Call this routine instead of calling ~_GetBuffer or ~_Contents that look at
 * contents. Useful when the desired row(s) are not within the row range of
 * the current block.
 */
void
AppendOnlyStorageRead_SkipCurrentBlock(AppendOnlyStorageRead *storageRead)
{
	Assert(storageRead != NULL);
	Assert(storageRead->isActive);

	if (storageRead->current.isLarge)
	{
		int64		largeContentPosition;	/* Position of the large content
											 * metadata block. */
		int32		largeContentLen;	/* Total length of the large content. */
		int32		remainingLargeContentLen;	/* Remaining number of bytes
												 * to read for the large
												 * content. */
		int32		regularBlockReadCount;	/* Number of regular blocks read
											 * after the metadata block. */
		int32		regularContentLen;	/* Length of the current regular
										 * block's content. */
		int32		availableLen;

		/*
		 * Large content.
		 *
		 * We have the LargeContent "metadata" AO block with the total length
		 * (already read) followed by N SmallContent blocks with the fragments
		 * of the large content.
		 */


		/*
		 * Save any values needed from the current* members since they will be
		 * modifed as we read the regular blocks.
		 */
		largeContentPosition = storageRead->current.headerOffsetInFile;
		largeContentLen = storageRead->current.uncompressedLen;

		/*
		 * Loop to read regular blocks.
		 */
		remainingLargeContentLen = largeContentLen;
		regularBlockReadCount = 0;
		while (true)
		{
			/*
			 * Read next regular block.
			 */
			regularBlockReadCount++;
			if (!AppendOnlyStorageRead_ReadNextBlock(storageRead))
			{
				/*
				 * Unexpected end of file.
				 */
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("Unexpected end of file trying to read block %d of large content in segment file '%s' of table '%s'.  "
								"Large content metadata block is at position " INT64_FORMAT "  "
								"Large content length %d",
								regularBlockReadCount,
								storageRead->segmentFileName,
								storageRead->relationName,
								largeContentPosition,
								largeContentLen)));
			}
			if (storageRead->current.headerKind != AoHeaderKind_SmallContent)
			{
				/*
				 * Unexpected headerKind.
				 */
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("Expected header kind 'Block' for block %d of large content in segment file '%s' of table '%s'.  "
								"Large content metadata block is at position " INT64_FORMAT "  "
								"Large content length %d",
								regularBlockReadCount,
								storageRead->segmentFileName,
								storageRead->relationName,
								largeContentPosition,
								largeContentLen)));
			}
			Assert(!storageRead->current.isLarge);

			BufferedReadGrowBuffer(&storageRead->bufferedRead,
								   storageRead->current.overallBlockLen,
								   &availableLen);

			if (storageRead->current.overallBlockLen != availableLen)
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("Wrong buffer length.  Expected %d byte length"
								" buffer and got %d ",
								storageRead->current.overallBlockLen,
								availableLen),
						 errdetail_appendonly_read_storage_content_header(storageRead),
						 errcontext_appendonly_read_storage_block(storageRead)));

			regularContentLen = storageRead->current.uncompressedLen;
			remainingLargeContentLen -= regularContentLen;
			if (remainingLargeContentLen < 0)
			{
				/*
				 * Too much data found???
				 */
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("Too much data found after reading %d blocks for large content in segment file '%s' of table '%s'.	"
								"Large content metadata block is at position " INT64_FORMAT "  "
								"Large content length %d; extra data length %d",
								regularBlockReadCount,
								storageRead->segmentFileName,
								storageRead->relationName,
								largeContentPosition,
								largeContentLen,
								-remainingLargeContentLen)));
			}

			/*
			 * Since we are skipping, we do not use the compressed or
			 * uncompressed content.
			 */

			if (remainingLargeContentLen == 0)
				break;
		}
	}
	else
	{
		uint8	   *header;
		uint8	   *content;

		/*
		 * "Small" content in one regular block.
		 */

		/*
		 * Fetch pointers to content.
		 *
		 * Since we are skipping, we do not look at the content.
		 */
		AppendOnlyStorageRead_InternalGetBuffer(storageRead,
												&header,
												&content);
	}
}
