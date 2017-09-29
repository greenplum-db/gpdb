/*-------------------------------------------------------------------------
 *
 * cdbbufferedappend.c
 *	  Write buffers to the end of a file efficiently.
 *
 * (See .h file for usage comments)
 *
 * Portions Copyright (c) 2007, greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbbufferedappend.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <unistd.h>				/* for write() */

#include "cdb/cdbbufferedappend.h"
#include "utils/guc.h"

static void BufferedAppendWrite(
					BufferedAppend *bufferedAppend);

/*
 * Determines the amount of memory to supply for
 * BufferedAppend given the desired buffer and
 * large write lengths.
 */
int32
BufferedAppendMemoryLen(
						int32 maxBufferLen,
						int32 maxLargeWriteLen)
{
	Assert(maxBufferLen > 0);
	Assert(maxLargeWriteLen >= maxBufferLen);

	/* Large write memory areas plus adjacent extra memory for 1 buffer. */
	return (maxLargeWriteLen + maxBufferLen);
}

/*
 * Initialize BufferedAppend.
 *
 * Use the BufferedAppendMemoryLen procedure to
 * determine the amount of memory to supply.
 */
void
BufferedAppendInit(
				   BufferedAppend *bufferedAppend,
				   uint8 *memory,
				   int32 memoryLen,
				   int32 maxBufferLen,
				   int32 maxLargeWriteLen,
				   char *relationName)
{
	Assert(bufferedAppend != NULL);
	Assert(memory != NULL);
	Assert(maxBufferLen > 0);
	Assert(maxLargeWriteLen >= maxBufferLen);
	Assert(memoryLen >= BufferedAppendMemoryLen(maxBufferLen, maxLargeWriteLen));

	memset(bufferedAppend, 0, sizeof(BufferedAppend));

	/*
	 * Init level.
	 */
	bufferedAppend->relationName = pstrdup(relationName);

	/*
	 * Large-read memory level members.
	 */
	bufferedAppend->maxBufferLen = maxBufferLen;
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
BufferedAppendSetFile(
					  BufferedAppend *bufferedAppend,
					  File file,
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
	Assert(bufferedAppend->initialSetFilePosition == 0);

	Assert(file >= 0);
	Assert(eof >= 0);

	bufferedAppend->largeWritePosition = eof;
	bufferedAppend->file = file;
	bufferedAppend->filePathName = filePathName;
	bufferedAppend->fileLen = eof;
	bufferedAppend->fileLen_uncompressed = eof_uncompressed;

	bufferedAppend->initialSetFilePosition = eof;
}


/*
 * Perform a large write i/o.
 */
static void
BufferedAppendWrite(
					BufferedAppend *bufferedAppend)
{
	int32		writeLen;
	uint8	   *largeWriteMemory;
	int			actualLen;

	writeLen = bufferedAppend->largeWriteLen;
	Assert(bufferedAppend->largeWriteLen > 0);
	largeWriteMemory = bufferedAppend->largeWriteMemory;

#ifdef USE_ASSERT_CHECKING
	{
		int64		currentWritePosition;

		currentWritePosition = FileNonVirtualCurSeek(bufferedAppend->file);
		if (currentWritePosition < 0)
			ereport(ERROR, (errcode_for_file_access(),
							errmsg("unable to get current position in table \"%s\" for file \"%s\": %m",
								   bufferedAppend->relationName,
								   bufferedAppend->filePathName)));

		if (currentWritePosition != bufferedAppend->largeWritePosition)
			ereport(ERROR, (errcode_for_file_access(),
							errmsg("Current position mismatch actual "
								   INT64_FORMAT ", expected " INT64_FORMAT " in table \"%s\" for file \"%s\"",
								   currentWritePosition, bufferedAppend->largeWritePosition,
								   bufferedAppend->relationName,
								   bufferedAppend->filePathName)));
	}
#endif

	while (writeLen > 0)
	{
		int			primaryError;
		bool		mirrorDataLossOccurred;

		MirroredAppendOnly_Append(
								  &bufferedAppend->mirroredOpen,
								  (char *) largeWriteMemory,
								  writeLen,
								  &primaryError,
								  &mirrorDataLossOccurred);
		if (primaryError != 0)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("Could not write in table \"%s\" to segment file \"%s\": %m",
							bufferedAppend->relationName,
							bufferedAppend->filePathName)));

		elogif(Debug_appendonly_print_append_block, LOG,
			   "Append-Only storage write: table \"%s\", segment file \"%s\", write position " INT64_FORMAT ", "
			   "writeLen %d (equals large write length %d is %s)",
			   bufferedAppend->relationName,
			   bufferedAppend->filePathName,
			   bufferedAppend->largeWritePosition,
			   writeLen,
			   bufferedAppend->largeWriteLen,
			   (writeLen == bufferedAppend->largeWriteLen ? "true" : "false"));

		actualLen = writeLen;

		writeLen -= actualLen;
		largeWriteMemory += actualLen;
	}

	bufferedAppend->largeWritePosition += bufferedAppend->largeWriteLen;
	bufferedAppend->largeWriteLen = 0;

}

/*
 * Return the position of the current write buffer in bytes.
 */
int64
BufferedAppendCurrentBufferPosition(
									BufferedAppend *bufferedAppend)
{
	Assert(bufferedAppend != NULL);
	Assert(bufferedAppend->file >= 0);

	return bufferedAppend->largeWritePosition;
}

/*
 * Return the position of the next write buffer in bytes.
 */
int64
BufferedAppendNextBufferPosition(
								 BufferedAppend *bufferedAppend)
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
BufferedAppendGetBuffer(
						BufferedAppend *bufferedAppend,
						int32 bufferLen)
{
	int32		currentLargeWriteLen;

	Assert(bufferedAppend != NULL);
	Assert(bufferedAppend->file >= 0);
	if (bufferLen > bufferedAppend->maxBufferLen)
		elog(ERROR,
			 "bufferLen %d greater than maxBufferLen %d at position " INT64_FORMAT " in table \"%s\" in file \"%s\"",
			 bufferLen, bufferedAppend->maxBufferLen, bufferedAppend->largeWritePosition,
			 bufferedAppend->relationName,
			 bufferedAppend->filePathName);

	/*
	 * Let next buffer carry-over into the extra buffer space after the large
	 * write buffer.
	 */
	currentLargeWriteLen = bufferedAppend->largeWriteLen;
	Assert(currentLargeWriteLen + bufferLen <=
		   bufferedAppend->maxLargeWriteLen +
		   bufferedAppend->maxBufferLen);

	bufferedAppend->bufferLen = bufferLen;

	return &bufferedAppend->largeWriteMemory[currentLargeWriteLen];
}

/*
 * Get the address of the current buffer space being used appending.
 */
uint8 *
BufferedAppendGetCurrentBuffer(
							   BufferedAppend *bufferedAppend)
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
BufferedAppendGetMaxBuffer(
						   BufferedAppend *bufferedAppend)
{
	Assert(bufferedAppend != NULL);
	Assert(bufferedAppend->file >= 0);

	return BufferedAppendGetBuffer(
								   bufferedAppend,
								   bufferedAppend->maxBufferLen);
}

void
BufferedAppendCancelLastBuffer(
							   BufferedAppend *bufferedAppend)
{
	Assert(bufferedAppend != NULL);
	Assert(bufferedAppend->file >= 0);

	bufferedAppend->bufferLen = 0;
}

/*
 * Indicate the current buffer is finished.
 */
void
BufferedAppendFinishBuffer(
						   BufferedAppend *bufferedAppend,
						   int32 usedLen,
						   int32 usedLen_uncompressed)
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
		   bufferedAppend->maxBufferLen);
	if (newLen >= bufferedAppend->maxLargeWriteLen)
	{
		/*
		 * Current large-write memory is full.
		 */
		bufferedAppend->largeWriteLen = bufferedAppend->maxLargeWriteLen;
		BufferedAppendWrite(bufferedAppend);

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
 * Returns the current file length.
 */
int64
BufferedAppendFileLen(
					  BufferedAppend *bufferedAppend)
{
	Assert(bufferedAppend != NULL);
	Assert(bufferedAppend->file >= 0);

	return bufferedAppend->fileLen;
}

/*
 * Flushes the current file for append.  Caller is resposible for closing
 * the file afterwards.  That close will flush any buffered writes for the
 * file.
 *
 * Returns the file length.
 */
void
BufferedAppendCompleteFile(
						   BufferedAppend *bufferedAppend,
						   int64 *fileLen,
						   int64 *fileLen_uncompressed)
{

	Assert(bufferedAppend != NULL);
	Assert(bufferedAppend->file >= 0);

	if (bufferedAppend->largeWriteLen > 0)
		BufferedAppendWrite(bufferedAppend);

	*fileLen = bufferedAppend->fileLen;
	*fileLen_uncompressed = bufferedAppend->fileLen_uncompressed;

	bufferedAppend->largeWritePosition = 0;
	bufferedAppend->largeWriteLen = 0;

	bufferedAppend->bufferLen = 0;

	bufferedAppend->fileLen = 0;
	bufferedAppend->fileLen_uncompressed = 0;
	bufferedAppend->file = -1;
	bufferedAppend->filePathName = NULL;

	bufferedAppend->initialSetFilePosition = 0;
}

/*
 * Finish with writing all together.
 */
void
BufferedAppendFinish(
					 BufferedAppend *bufferedAppend)
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
