/*-------------------------------------------------------------------------
 *
 * cdbbufferedappend.c
 *	  Write buffers to the end of a file efficiently.
 *
 * (See .h file for usage comments)
 *
 * Portions Copyright (c) 2007, greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbbufferedappend.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <unistd.h>				/* for write() */

#include "cdb/cdbappendonlyxlog.h"
#include "cdb/cdbbufferedappend.h"
#include "pgstat.h"
#include "utils/guc.h"

static void BufferedAppendWrite(
					BufferedAppend *bufferedAppend,
					bool needsWAL);

/*
 * Determines the amount of memory to supply for
 * BufferedAppend given the desired buffer and
 * large write lengths.
 */
int32
BufferedAppendMemoryLen(int32 maxBufferWithCompressionOverrrunLen,
						int32 maxLargeWriteLen)
{
	Assert(maxBufferWithCompressionOverrrunLen > 0);
	Assert(maxLargeWriteLen >= maxBufferWithCompressionOverrrunLen);

	/* Large write memory areas plus adjacent extra memory for 1 buffer. */
	return (maxLargeWriteLen + maxBufferWithCompressionOverrrunLen);
}

/*
 * Initialize BufferedAppend.
 *
 * Use the BufferedAppendMemoryLen procedure to
 * determine the amount of memory to supply.
 */
void
BufferedAppendInit(BufferedAppend *bufferedAppend,
				   uint8 *memory,
				   int32 memoryLen,
				   int32 maxBufferWithCompressionOverrrunLen,
				   int32 maxLargeWriteLen,
				   char *relationName)
{
	Assert(bufferedAppend != NULL);
	Assert(memory != NULL);
	Assert(maxBufferWithCompressionOverrrunLen> 0);
	Assert(maxLargeWriteLen >= maxBufferWithCompressionOverrrunLen);
	Assert(memoryLen >= BufferedAppendMemoryLen(maxBufferWithCompressionOverrrunLen, maxLargeWriteLen));

	memset(bufferedAppend, 0, sizeof(BufferedAppend));

	/*
	 * Init level.
	 */
	bufferedAppend->relationName = pstrdup(relationName);

	/*
	 * Large-read memory level members.
	 */
	bufferedAppend->maxBufferWithCompressionOverrrunLen = maxBufferWithCompressionOverrrunLen;
	bufferedAppend->maxLargeWriteLen = maxLargeWriteLen;

	bufferedAppend->memory = memory;
	bufferedAppend->memoryLen = memoryLen;

	bufferedAppend->largeWriteMemory = memory;
	bufferedAppend->afterBufferMemory =
		&memory[maxLargeWriteLen];

	bufferedAppend->largeWritePosition = 0;
	bufferedAppend->largeWriteLen = 0;

	/*
	 * Buffer level members.
	 */
	bufferedAppend->bufferLen = 0;

	/*
	 * File level members.
	 */
	bufferedAppend->file = -1;
	bufferedAppend->filePathName = NULL;
	bufferedAppend->fileLen = 0;
}

/*
 * Takes an open file handle for the next file.
 *
 * Note that eof_uncompressed is used only for storing the
 * uncompressed file size in the catalog, so that the compression
 * ratio could be calculated at the user's request.
 */
void
BufferedAppendSetFile(BufferedAppend *bufferedAppend,
					  File file,
					  RelFileNodeBackend relFileNode,
					  int32 segmentFileNum,
					  char *filePathName,
					  int64 eof,
					  int64 eof_uncompressed)
{
	Assert(bufferedAppend != NULL);
	Assert(bufferedAppend->largeWritePosition == 0);
	Assert(bufferedAppend->largeWriteLen == 0);
	Assert(bufferedAppend->bufferLen == 0);
	Assert(bufferedAppend->file == -1);
	Assert(bufferedAppend->fileLen == 0);
	Assert(bufferedAppend->fileLen_uncompressed == 0);

	Assert(file >= 0);
	Assert(eof >= 0);

	bufferedAppend->largeWritePosition = eof;
	bufferedAppend->file = file;
	bufferedAppend->relFileNode = relFileNode;
	bufferedAppend->segmentFileNum = segmentFileNum;
	bufferedAppend->filePathName = filePathName;
	bufferedAppend->fileLen = eof;
	bufferedAppend->fileLen_uncompressed = eof_uncompressed;
}


/*
 * Perform a large write i/o.
 */
static void
BufferedAppendWrite(BufferedAppend *bufferedAppend, bool needsWAL)
{
	int32		bytesleft;
	int32		bytestotal;
	uint8	   *largeWriteMemory;

	Assert(bufferedAppend->largeWriteLen > 0);
	largeWriteMemory = bufferedAppend->largeWriteMemory;

	bytestotal = 0;
	bytesleft = bufferedAppend->largeWriteLen;
	while (bytesleft > 0)
	{
		int32		byteswritten;

		byteswritten = FileWrite(bufferedAppend->file,
								 (char *) largeWriteMemory + bytestotal,
								 bytesleft,
								 bufferedAppend->largeWritePosition + bytestotal,
								 WAIT_EVENT_DATA_FILE_WRITE);
		if (byteswritten < 0)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not write in table \"%s\" to segment file \"%s\": %m",
							bufferedAppend->relationName,
							bufferedAppend->filePathName)));

		bytesleft -= byteswritten;
		bytestotal += byteswritten;

		if (file_extend_hook)
			(*file_extend_hook)(bufferedAppend->relFileNode);
	}

	elogif(Debug_appendonly_print_append_block, LOG,
		   "Append-Only storage write: table \"%s\", segment file \"%s\", write position " INT64_FORMAT ", bytes written %d",
		   bufferedAppend->relationName,
		   bufferedAppend->filePathName,
		   bufferedAppend->largeWritePosition,
		   bytestotal);

	/*
	 * Log each varblock to the XLog. Write to the file first, before
	 * writing the WAL record, to avoid trouble if you run out of disk
	 * space. If WAL record is written first, and then the FileWrite()
	 * fails, there's no way to "undo" the WAL record. If crash
	 * happens, crash recovery will also try to replay the WAL record,
	 * and will also run out of disk space, and will fail. As EOF
	 * controls the visibility of data in AO / CO files, writing xlog
	 * record after writing to file works fine.
	 */
	if (needsWAL)
		xlog_ao_insert(bufferedAppend->relFileNode.node, bufferedAppend->segmentFileNum,
					   bufferedAppend->largeWritePosition, largeWriteMemory, bytestotal);

	bufferedAppend->largeWritePosition += bufferedAppend->largeWriteLen;
	bufferedAppend->largeWriteLen = 0;
}

/*
 * Return the position of the current write buffer in bytes.
 */
int64
BufferedAppendCurrentBufferPosition(BufferedAppend *bufferedAppend)
{
	Assert(bufferedAppend != NULL);
	Assert(bufferedAppend->file >= 0);

	return bufferedAppend->largeWritePosition;
}

/*
 * Return the position of the next write buffer in bytes.
 */
int64
BufferedAppendNextBufferPosition(BufferedAppend *bufferedAppend)
{
	Assert(bufferedAppend != NULL);
	Assert(bufferedAppend->file >= 0);

	return bufferedAppend->largeWritePosition +
		bufferedAppend->largeWriteLen;
}

/*
 * Get the next buffer space for appending with a specified length.
 *
 * Returns NULL when the current file does not have enough
 * room for another buffer.
 */
uint8 *
BufferedAppendGetBuffer(BufferedAppend *bufferedAppend,
						int32 bufferLen)
{
	int32		currentLargeWriteLen;

	Assert(bufferedAppend != NULL);
	Assert(bufferedAppend->file >= 0);
	if (bufferLen > bufferedAppend->maxBufferWithCompressionOverrrunLen)
		elog(ERROR,
			 "bufferLen %d greater than maxBufferLen %d at position " INT64_FORMAT " in table \"%s\" in file \"%s\"",
			 bufferLen, bufferedAppend->maxBufferWithCompressionOverrrunLen, bufferedAppend->largeWritePosition,
			 bufferedAppend->relationName,
			 bufferedAppend->filePathName);

	/*
	 * Let next buffer carry-over into the extra buffer space after the large
	 * write buffer.
	 */
	currentLargeWriteLen = bufferedAppend->largeWriteLen;
	Assert(currentLargeWriteLen + bufferLen <=
		   bufferedAppend->maxLargeWriteLen +
		   bufferedAppend->maxBufferWithCompressionOverrrunLen);

	bufferedAppend->bufferLen = bufferLen;

	return &bufferedAppend->largeWriteMemory[currentLargeWriteLen];
}

/*
 * Get the address of the current buffer space being used appending.
 */
uint8 *
BufferedAppendGetCurrentBuffer(BufferedAppend *bufferedAppend)
{
	Assert(bufferedAppend != NULL);
	Assert(bufferedAppend->file >= 0);

	return &bufferedAppend->largeWriteMemory[bufferedAppend->largeWriteLen];
}

/*
 * Get the next maximum length buffer space for appending.
 *
 * Returns NULL when the current file does not have enough
 * room for another buffer.
 */
uint8 *
BufferedAppendGetMaxBuffer(BufferedAppend *bufferedAppend)
{
	Assert(bufferedAppend != NULL);
	Assert(bufferedAppend->file >= 0);

	return BufferedAppendGetBuffer(
								   bufferedAppend,
								   bufferedAppend->maxBufferWithCompressionOverrrunLen);
}

void
BufferedAppendCancelLastBuffer(BufferedAppend *bufferedAppend)
{
	Assert(bufferedAppend != NULL);
	Assert(bufferedAppend->file >= 0);

	bufferedAppend->bufferLen = 0;
}

/*
 * Indicate the current buffer is finished.
 */
void
BufferedAppendFinishBuffer(BufferedAppend *bufferedAppend,
						   int32 usedLen,
						   int32 usedLen_uncompressed,
						   bool needsWAL)
{
	int32		newLen;

	Assert(bufferedAppend != NULL);
	Assert(bufferedAppend->file >= 0);
	if (usedLen > bufferedAppend->bufferLen)
		elog(ERROR,
			 "Used length %d greater than bufferLen %d at position " INT64_FORMAT " in table \"%s\" in file \"%s\"",
			 usedLen, bufferedAppend->bufferLen, bufferedAppend->largeWritePosition,
			 bufferedAppend->relationName,
			 bufferedAppend->filePathName);


	newLen = bufferedAppend->largeWriteLen + usedLen;
	Assert(newLen <= bufferedAppend->maxLargeWriteLen +
		   bufferedAppend->maxBufferWithCompressionOverrrunLen);
	if (newLen >= bufferedAppend->maxLargeWriteLen)
	{
		/*
		 * Current large-write memory is full.
		 */
		bufferedAppend->largeWriteLen = bufferedAppend->maxLargeWriteLen;
		BufferedAppendWrite(bufferedAppend, needsWAL);

		if (newLen > bufferedAppend->maxLargeWriteLen)
		{
			int32		excessLen;

			/*
			 * We have carry-over in the extra buffer.  Write and then copy
			 * the extra to the front of the large write buffer.
			 */
			excessLen = newLen - bufferedAppend->maxLargeWriteLen;

			bufferedAppend->largeWriteLen = bufferedAppend->maxLargeWriteLen;

			memcpy(bufferedAppend->largeWriteMemory,
				   bufferedAppend->afterBufferMemory,
				   excessLen);

			bufferedAppend->largeWriteLen = excessLen;
		}
		else
		{
			/*
			 * Exactly fits.
			 */
			Assert(newLen == bufferedAppend->maxLargeWriteLen);
			Assert(bufferedAppend->largeWriteLen == 0);
		}
	}
	else
	{
		/*
		 * Normal case -- added more data to current buffer.
		 */
		bufferedAppend->largeWriteLen = newLen;
	}

	bufferedAppend->fileLen += usedLen;
	bufferedAppend->fileLen_uncompressed += usedLen_uncompressed;
}

/*
 * Flushes the current file for append.  Caller is resposible for closing
 * the file afterwards.  That close will flush any buffered writes for the
 * file.
 *
 * Returns the file length.
 */
void
BufferedAppendCompleteFile(BufferedAppend *bufferedAppend,
						   int64 *fileLen,
						   int64 *fileLen_uncompressed,
						   bool needsWAL)
{

	Assert(bufferedAppend != NULL);
	Assert(bufferedAppend->file >= 0);

	if (bufferedAppend->largeWriteLen > 0)
		BufferedAppendWrite(bufferedAppend, needsWAL);

	*fileLen = bufferedAppend->fileLen;
	*fileLen_uncompressed = bufferedAppend->fileLen_uncompressed;

	bufferedAppend->largeWritePosition = 0;
	bufferedAppend->largeWriteLen = 0;

	bufferedAppend->bufferLen = 0;

	bufferedAppend->fileLen = 0;
	bufferedAppend->fileLen_uncompressed = 0;
	bufferedAppend->file = -1;
	bufferedAppend->filePathName = NULL;
}

/*
 * Finish with writing all together.
 */
void
BufferedAppendFinish(BufferedAppend *bufferedAppend)
{
	Assert(bufferedAppend != NULL);

	Assert(bufferedAppend->file == -1);
	Assert(bufferedAppend->bufferLen == 0);
	Assert(bufferedAppend->fileLen == 0);
	Assert(bufferedAppend->fileLen_uncompressed == 0);

	if (bufferedAppend->relationName != NULL)
	{
		pfree(bufferedAppend->relationName);
		bufferedAppend->relationName = NULL;
	}
}
