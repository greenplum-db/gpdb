/*-------------------------------------------------------------------------
 *
 * cdbbufferedread.c
 *	  Read buffers sequentially from a file efficiently.
 *
 * (See .h file for usage comments)
 *
 * Portions Copyright (c) 2007, greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbbufferedread.c
 *
 *-------------------------------------------------------------------------
 */
#include "cdb/cdbbufferedread.h"
#include <unistd.h>				/* for read() */
#include "utils/guc.h"
#include "miscadmin.h"

static void BufferedReadIo(
			   BufferedRead *bufferedRead);
static uint8 *BufferedReadUseBeforeBuffer(
							BufferedRead *bufferedRead,
							int32 maxReadAheadLen,
							int32 *nextBufferLen);


/*
 * Determines the amount of memory to supply for
 * BufferedRead given the desired buffer and
 * large write lengths.
 */
int32
BufferedReadMemoryLen(
					  int32 maxBufferLen,
					  int32 maxLargeReadLen)
{
	Assert(maxBufferLen > 0);
	Assert(maxLargeReadLen >= maxBufferLen);

	/* Adjacent before memory for 1 buffer and large read memory. */
	return (maxBufferLen + maxLargeReadLen);
}

/*
 * Initialize BufferedRead.
 *
 * Use the BufferedReadMemoryLen procedure to
 * determine the amount of memory to supply.
 */
void
BufferedReadInit(
				 BufferedRead *bufferedRead,
				 uint8 *memory,
				 int32 memoryLen,
				 int32 maxBufferLen,
				 int32 maxLargeReadLen,
				 char *relationName)
{
	int			relationNameLen;

	Assert(bufferedRead != NULL);
	Assert(memory != NULL);
	Assert(maxBufferLen > 0);
	Assert(maxLargeReadLen >= maxBufferLen);
	Assert(memoryLen >= BufferedReadMemoryLen(maxBufferLen, maxLargeReadLen));

	memset(bufferedRead, 0, sizeof(BufferedRead));

	/*
	 * Init level.
	 */
	relationNameLen = strlen(relationName);
	bufferedRead->relationName = (char *) palloc(relationNameLen + 1);
	memcpy(bufferedRead->relationName, relationName, relationNameLen + 1);

	/*
	 * Large-read memory level members.
	 */
	bufferedRead->maxBufferLen = maxBufferLen;
	bufferedRead->maxLargeReadLen = maxLargeReadLen;

	bufferedRead->memory = memory;
	bufferedRead->memoryLen = memoryLen;

	bufferedRead->beforeBufferMemory = memory;
	bufferedRead->largeReadMemory =
		&memory[maxBufferLen];

	bufferedRead->largeReadPosition = 0;
	bufferedRead->largeReadLen = 0;

	/*
	 * Buffer level members.
	 */
	bufferedRead->bufferOffset = 0;
	bufferedRead->bufferLen = 0;

	/*
	 * File level members.
	 */
	bufferedRead->file = -1;
	bufferedRead->fileLen = 0;

	/*
	 * Temporary limit support for random reading.
	 */
	bufferedRead->haveTemporaryLimitInEffect = false;
	bufferedRead->temporaryLimitFileLen = 0;
}

/*
 * Takes an open file handle for the next file.
 */
void
BufferedReadSetFile(
					BufferedRead *bufferedRead,
					File file,
					char *filePathName,
					int64 fileLen)
{
	Assert(bufferedRead != NULL);

	Assert(bufferedRead->file == -1);
	Assert(bufferedRead->fileLen == 0);

	Assert(bufferedRead->largeReadPosition == 0);
	Assert(bufferedRead->largeReadLen == 0);

	Assert(bufferedRead->bufferOffset == 0);
	Assert(bufferedRead->bufferLen == 0);

	Assert(file >= 0);
	Assert(fileLen >= 0);

	bufferedRead->file = file;
	bufferedRead->filePathName = filePathName;
	bufferedRead->fileLen = fileLen;

	bufferedRead->haveTemporaryLimitInEffect = false;
	bufferedRead->temporaryLimitFileLen = 0;

	if (fileLen > 0)
	{
		/*
		 * Do the first read.
		 */
		if (fileLen > bufferedRead->maxLargeReadLen)
			bufferedRead->largeReadLen = bufferedRead->maxLargeReadLen;
		else
			bufferedRead->largeReadLen = (int32) fileLen;
		BufferedReadIo(bufferedRead);
	}
}

/*
 * Perform a large read i/o.
 */
static void
BufferedReadIo(
			   BufferedRead *bufferedRead)
{
	int32		largeReadLen;
	uint8	   *largeReadMemory;
	int32		offset;

	largeReadLen = bufferedRead->largeReadLen;
	Assert(bufferedRead->largeReadLen > 0);
	largeReadMemory = bufferedRead->largeReadMemory;

#ifdef USE_ASSERT_CHECKING
	{
		int64		currentReadPosition;

		currentReadPosition = FileNonVirtualCurSeek(bufferedRead->file);
		if (currentReadPosition < 0)
			ereport(ERROR, (errcode_for_file_access(),
							errmsg("unable to get current position for table \"%s\" in file \"%s\": %m",
								   bufferedRead->relationName,
								   bufferedRead->filePathName)));

		if (currentReadPosition != bufferedRead->largeReadPosition)
		{
			ereport(ERROR, (errcode_for_file_access(),
							errmsg("Current position mismatch actual "
								   INT64_FORMAT ", expected " INT64_FORMAT " for table \"%s\" in file \"%s\"",
								   currentReadPosition, bufferedRead->largeReadPosition,
								   bufferedRead->relationName,
								   bufferedRead->filePathName)));
		}
	}
#endif

	offset = 0;
	while (largeReadLen > 0)
	{
		int			actualLen = FileRead(
										 bufferedRead->file,
										 (char *) largeReadMemory,
										 largeReadLen);

		if (actualLen == 0)
			ereport(ERROR, (errcode_for_file_access(),
							errmsg("read beyond eof in table \"%s\" file \"%s\", "
								   "read position " INT64_FORMAT " (small offset %d), "
								   "actual read length %d (large read length %d)",
								   bufferedRead->relationName,
								   bufferedRead->filePathName,
								   bufferedRead->largeReadPosition,
								   offset,
								   actualLen,
								   bufferedRead->largeReadLen)));
		else if (actualLen < 0)
			ereport(ERROR, (errcode_for_file_access(),
							errmsg("unable to read table \"%s\" file \"%s\", "
								   "read position " INT64_FORMAT " (small offset %d), "
								   "actual read length %d (large read length %d): %m",
								   bufferedRead->relationName,
								   bufferedRead->filePathName,
								   bufferedRead->largeReadPosition,
								   offset,
								   actualLen,
								   bufferedRead->largeReadLen)));

		elogif(Debug_appendonly_print_read_block, LOG,
			   "Append-Only storage read: table \"%s\", segment file \"%s\", read position " INT64_FORMAT " (small offset %d), "
			   "actual read length %d (equals large read length %d is %s)",
			   bufferedRead->relationName,
			   bufferedRead->filePathName,
			   bufferedRead->largeReadPosition,
			   offset,
			   actualLen,
			   bufferedRead->largeReadLen,
			   (actualLen == bufferedRead->largeReadLen ? "true" : "false"));

		largeReadLen -= actualLen;
		largeReadMemory += actualLen;
		offset += actualLen;
	}

	if (VacuumCostActive)
		VacuumCostBalance += VacuumCostPageMiss;
}

static uint8 *
BufferedReadUseBeforeBuffer(
							BufferedRead *bufferedRead,
							int32 maxReadAheadLen,
							int32 *nextBufferLen)
{
	int64		inEffectFileLen;
	int64		nextPosition;
	int64		remainingFileLen;
	int32		nextReadLen;
	int32		beforeLen;
	int32		beforeOffset;
	int32		extraLen;

	Assert(bufferedRead->largeReadLen - bufferedRead->bufferOffset > 0);

	if (bufferedRead->haveTemporaryLimitInEffect)
		inEffectFileLen = bufferedRead->temporaryLimitFileLen;
	else
		inEffectFileLen = bufferedRead->fileLen;

	/*
	 * Requesting more data than in the current read.
	 */
	nextPosition = bufferedRead->largeReadPosition + bufferedRead->largeReadLen;

	if (nextPosition == inEffectFileLen)
	{
		/*
		 * No more file to read.
		 */
		bufferedRead->bufferLen = bufferedRead->largeReadLen -
			bufferedRead->bufferOffset;
		Assert(bufferedRead->bufferLen > 0);

		*nextBufferLen = bufferedRead->bufferLen;
		return &bufferedRead->largeReadMemory[bufferedRead->bufferOffset];
	}

	remainingFileLen = inEffectFileLen -
		nextPosition;
	if (remainingFileLen > bufferedRead->maxLargeReadLen)
		nextReadLen = bufferedRead->maxLargeReadLen;
	else
		nextReadLen = (int32) remainingFileLen;

	Assert(nextReadLen >= 0);

	beforeLen = bufferedRead->maxLargeReadLen - bufferedRead->bufferOffset;
	beforeOffset = bufferedRead->maxBufferLen - beforeLen;

	/*
	 * Copy data from the current large-read buffer into the before memory.
	 */
	memcpy(&bufferedRead->beforeBufferMemory[beforeOffset],
		   &bufferedRead->largeReadMemory[bufferedRead->bufferOffset],
		   beforeLen);

	/*
	 * Do the next read.
	 */
	bufferedRead->largeReadPosition = nextPosition;
	bufferedRead->largeReadLen = nextReadLen;

	/*
	 * MPP-17061: nextReadLen should never be 0, since the code will return
	 * above if inEffectFileLen is equal to nextPosition.
	 */
	if (nextReadLen == 0)
	{
		ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
						errmsg("Unexpected internal error,"
							   " largeReadLen is set to 0 before calling BufferedReadIo."
							   " remainingFileLen is " INT64_FORMAT
							   " inEffectFileLen is " INT64_FORMAT
							   " nextPosition is " INT64_FORMAT,
							   remainingFileLen, inEffectFileLen, nextPosition)));
	}

	BufferedReadIo(bufferedRead);

	extraLen = maxReadAheadLen - beforeLen;
	Assert(extraLen > 0);
	if (extraLen > nextReadLen)
		extraLen = (int32) nextReadLen;

	/*
	 * Return a buffer using a negative offset that starts in the before
	 * memory and goes into the large-read memory.
	 */
	bufferedRead->bufferOffset = -beforeLen;
	bufferedRead->bufferLen = beforeLen + extraLen;
	Assert(bufferedRead->bufferLen > 0);

	*nextBufferLen = bufferedRead->bufferLen;

	return &bufferedRead->beforeBufferMemory[beforeOffset];
}

/*
 * Set a temporary read range in the current open segment file.
 *
 * The beginFileOffset must be to the beginning of an Append-Only Storage block.
 *
 * The afterFileOffset serves as the temporary EOF.  It will cause reads to return
 * false (no more blocks) when reached.  It must be at the end of an Append-Only Storage
 * block.
 *
 * When a read returns false (no more blocks), the temporary read range is forgotten.
 */
void
BufferedReadSetTemporaryRange(
							  BufferedRead *bufferedRead,
							  int64 beginFileOffset,
							  int64 afterFileOffset)
{
	bool		newReadNeeded;
	int64		largeReadAfterPos;

	Assert(bufferedRead != NULL);
	Assert(bufferedRead->file >= 0);

	/*
	 * Forget any current read buffer length (but not the offset!).
	 */
	bufferedRead->bufferLen = 0;

	newReadNeeded = false;
	largeReadAfterPos = bufferedRead->largeReadPosition +
		bufferedRead->largeReadLen;
	if (bufferedRead->bufferOffset < 0)
	{
		int64		virtualLargeReadBeginPos;

		virtualLargeReadBeginPos = bufferedRead->largeReadPosition +
			bufferedRead->bufferOffset;

		if (beginFileOffset >= virtualLargeReadBeginPos &&
			beginFileOffset < largeReadAfterPos)
		{
			/*
			 * Our temporary read begin position is within our before buffer
			 * and current read.
			 */
			bufferedRead->bufferOffset =
				(int32) (beginFileOffset -
						 bufferedRead->largeReadPosition);
		}
		else
			newReadNeeded = true;
	}
	else
	{
		if (beginFileOffset >= bufferedRead->largeReadPosition &&
			beginFileOffset < largeReadAfterPos)
		{
			/*
			 * Our temporary read begin position is within our current read.
			 */
			bufferedRead->bufferOffset =
				(int32) (beginFileOffset -
						 bufferedRead->largeReadPosition);
		}
		else
		{
			newReadNeeded = true;
		}
	}

	if (newReadNeeded)
	{
		int64		remainingFileLen;

		/*
		 * Seek to the requested beginning position. MPP-17061: allow seeking
		 * backward (negative offset) in file, this could happen during index
		 * scan, if we do lookup for a block directory entry at the end of the
		 * segment file, followed by a lookup for a block directory entry at
		 * the beginning of file.
		 */
		int64		seekOffset = beginFileOffset - largeReadAfterPos;
		int64		seekPos = FileSeek(bufferedRead->file, seekOffset, SEEK_CUR);

		if (seekPos != beginFileOffset)
			ereport(ERROR, (errcode_for_file_access(),
							errmsg("unable to seek to position for table \"%s\" in file \"%s\": %m",
								   bufferedRead->relationName,
								   bufferedRead->filePathName)));

		bufferedRead->bufferOffset = 0;

		remainingFileLen = afterFileOffset - beginFileOffset;
		if (remainingFileLen > bufferedRead->maxLargeReadLen)
			bufferedRead->largeReadLen = bufferedRead->maxLargeReadLen;
		else
			bufferedRead->largeReadLen = (int32) remainingFileLen;

		bufferedRead->largeReadPosition = beginFileOffset;

		if (bufferedRead->largeReadLen > 0)
			BufferedReadIo(bufferedRead);
	}

	bufferedRead->haveTemporaryLimitInEffect = true;
	bufferedRead->temporaryLimitFileLen = afterFileOffset;

}

/*
 * Return the position of the next read in bytes.
 */
int64
BufferedReadNextBufferPosition(
							   BufferedRead *bufferedRead)
{
	Assert(bufferedRead != NULL);
	Assert(bufferedRead->file >= 0);

	/*
	 * Note that the bufferOffset can be negative when we are using the before
	 * buffer, but that is ok.  We should get an accurate next position.
	 */
	return bufferedRead->largeReadPosition +
		bufferedRead->bufferOffset +
		bufferedRead->bufferLen;
}

/*
 * Get the next buffer space for reading with a specified max read-ahead
 * amount.
 *
 * Returns NULL when the current file has been completely read.
 */
uint8 *
BufferedReadGetNextBuffer(
						  BufferedRead *bufferedRead,
						  int32 maxReadAheadLen,
						  int32 *nextBufferLen)
{
	int64		inEffectFileLen;

	Assert(bufferedRead != NULL);
	Assert(bufferedRead->file >= 0);
	Assert(maxReadAheadLen > 0);
	Assert(nextBufferLen != NULL);
	if (maxReadAheadLen > bufferedRead->maxBufferLen)
	{
		Assert(maxReadAheadLen <= bufferedRead->maxBufferLen);

		elog(ERROR, "Read ahead length %d is greater than maximum buffer length %d",
			 maxReadAheadLen, bufferedRead->maxBufferLen);
	}

	if (bufferedRead->haveTemporaryLimitInEffect)
		inEffectFileLen = bufferedRead->temporaryLimitFileLen;
	else
		inEffectFileLen = bufferedRead->fileLen;

	/*
	 * Finish previous buffer.
	 */
	bufferedRead->bufferOffset += bufferedRead->bufferLen;
	bufferedRead->bufferLen = 0;

	if (bufferedRead->largeReadPosition == inEffectFileLen)
	{
		/*
		 * At end of file.
		 */
		*nextBufferLen = 0;
		bufferedRead->haveTemporaryLimitInEffect = false;
		return NULL;
	}

	/*
	 * Any more left in current read?
	 */
	if (bufferedRead->bufferOffset == bufferedRead->largeReadLen)
	{
		int64		remainingFileLen;

		/*
		 * Used exactly all of it.  Attempt read more.
		 */
		bufferedRead->largeReadPosition += bufferedRead->largeReadLen;
		bufferedRead->bufferOffset = 0;

		remainingFileLen = inEffectFileLen -
			bufferedRead->largeReadPosition;
		if (remainingFileLen > bufferedRead->maxLargeReadLen)
			bufferedRead->largeReadLen = bufferedRead->maxLargeReadLen;
		else
			bufferedRead->largeReadLen = (int32) remainingFileLen;
		if (bufferedRead->largeReadLen == 0)
		{
			/*
			 * At end of file.
			 */
			*nextBufferLen = 0;
			bufferedRead->haveTemporaryLimitInEffect = false;
			return NULL;
		}

		BufferedReadIo(bufferedRead);

		if (maxReadAheadLen > bufferedRead->largeReadLen)
			bufferedRead->bufferLen = bufferedRead->largeReadLen;
		else
			bufferedRead->bufferLen = maxReadAheadLen;

		Assert(bufferedRead->bufferOffset == 0);
		Assert(bufferedRead->bufferLen > 0);

		*nextBufferLen = bufferedRead->bufferLen;
		return bufferedRead->largeReadMemory;
	}

	if (bufferedRead->bufferOffset + maxReadAheadLen > bufferedRead->largeReadLen)
	{
		/*
		 * Odd boundary.  Use before memory to carry us over.
		 */
		Assert(bufferedRead->largeReadLen - bufferedRead->bufferOffset > 0);

		return BufferedReadUseBeforeBuffer(
										   bufferedRead,
										   maxReadAheadLen,
										   nextBufferLen);
	}

	/*
	 * We can satisify request from current read.
	 */
	bufferedRead->bufferLen = maxReadAheadLen;
	Assert(bufferedRead->bufferLen > 0);

	*nextBufferLen = bufferedRead->bufferLen;
	return &bufferedRead->largeReadMemory[bufferedRead->bufferOffset];
}

/*
 * Get the next, maximum buffer space for reading.
 *
 * Returns NULL when the current file has been completely read.
 */
uint8 *
BufferedReadGetMaxBuffer(
						 BufferedRead *bufferedRead,
						 int32 *nextBufferLen)
{
	Assert(bufferedRead != NULL);
	Assert(bufferedRead->file >= 0);
	Assert(nextBufferLen != NULL);

	return BufferedReadGetNextBuffer(
									 bufferedRead,
									 bufferedRead->maxBufferLen,
									 nextBufferLen);
}

/*
 * Grow the available length of the current buffer.
 *
 * NOTE: The buffer address returned can be different, even for previously
 * examined buffer data.  In other words, don't keep buffer pointers in
 * the buffer region.  Use offsets to re-establish pointers after this call.
 *
 * If the current file has been completely read, bufferLen will remain
 * the current value.
 */
uint8 *
BufferedReadGrowBuffer(
					   BufferedRead *bufferedRead,
					   int32 newMaxReadAheadLen,
					   int32 *growBufferLen)
{
	int32		newNextOffset;

	Assert(bufferedRead != NULL);
	Assert(bufferedRead->file >= 0);
	Assert(newMaxReadAheadLen > bufferedRead->bufferLen);
	Assert(newMaxReadAheadLen <= bufferedRead->maxBufferLen);
	Assert(growBufferLen != NULL);

	newNextOffset = bufferedRead->bufferOffset + newMaxReadAheadLen;
	if (newNextOffset > bufferedRead->largeReadLen)
	{
		/*
		 * Odd boundary.  Use before memory to carry us over.
		 */
		Assert(bufferedRead->largeReadLen - bufferedRead->bufferOffset > 0);

		return BufferedReadUseBeforeBuffer(
										   bufferedRead,
										   newMaxReadAheadLen,
										   growBufferLen);
	}

	/*
	 * There is maxReadAheadLen more left in current large-read memory.
	 */
	bufferedRead->bufferLen = newMaxReadAheadLen;
	Assert(bufferedRead->bufferLen > 0);

	*growBufferLen = bufferedRead->bufferLen;
	return &bufferedRead->largeReadMemory[bufferedRead->bufferOffset];
}

/*
 * Return the address of the current read buffer.
 */
uint8 *
BufferedReadGetCurrentBuffer(
							 BufferedRead *bufferedRead)
{
	Assert(bufferedRead != NULL);
	Assert(bufferedRead->file >= 0);

	return &bufferedRead->largeReadMemory[bufferedRead->bufferOffset];
}

/*
 * Return the current buffer's start position.
 */
int64
BufferedReadCurrentPosition(
							BufferedRead *bufferedRead)
{
	Assert(bufferedRead != NULL);
	Assert(bufferedRead->file >= 0);

	return bufferedRead->largeReadPosition + bufferedRead->bufferOffset;
}

/*
 * Flushes the current file for append.  Caller is responsible for closing
 * the file afterwards.
 */
void
BufferedReadCompleteFile(
						 BufferedRead *bufferedRead)
{
	Assert(bufferedRead != NULL);
	Assert(bufferedRead->file >= 0);

	bufferedRead->file = -1;
	bufferedRead->filePathName = NULL;
	bufferedRead->fileLen = 0;

	bufferedRead->bufferOffset = 0;
	bufferedRead->bufferLen = 0;

	bufferedRead->largeReadPosition = 0;
	bufferedRead->largeReadLen = 0;
}


/*
 * Finish with reading all together.
 */
void
BufferedReadFinish(BufferedRead *bufferedRead)
{
	Assert(bufferedRead != NULL);

	/* Assert(bufferedRead->file == -1); */
	Assert(bufferedRead->fileLen == 0);

	Assert(bufferedRead->bufferOffset == 0);
	Assert(bufferedRead->bufferLen == 0);

	if (bufferedRead->memory)
	{
		pfree(bufferedRead->memory);
		bufferedRead->memory = NULL;
		bufferedRead->memoryLen = 0;
	}

	if (bufferedRead->relationName != NULL)
	{
		pfree(bufferedRead->relationName);
		bufferedRead->relationName = NULL;
	}
}
