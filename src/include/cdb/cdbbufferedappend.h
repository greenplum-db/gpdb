/*-------------------------------------------------------------------------
 *
 * cdbbufferedappend.h
 *	  Write buffers to the end of a file efficiently.
 *
 * The client is given direct access to the write buffer for appending
 * buffers efficiency.
 *        
 * Portions Copyright (c) 2007, greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbbufferedappend.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBBUFFEREDAPPEND_H
#define CDBBUFFEREDAPPEND_H

#include "storage/fd.h"
#include "cdb/cdbmirroredappendonly.h"

typedef struct BufferedAppend
{
	/*
	 * Init level.
	 */
	char				*relationName;

	/*
	 * Large-write memory level members.
	 */
    int32      			 maxBufferLen;
    int32      			 maxLargeWriteLen;

	uint8                *memory;
    int32                memoryLen;

    uint8                *largeWriteMemory;

    uint8                *afterBufferMemory;
							/*
							 * We allocate maxBufferLen bytes after largeWriteMemory
							 * to support buffers that cross large write boundaries.
							 */
	
	int64				 largeWritePosition;
    int32                largeWriteLen;
							/*
							 * The position within the current file for the next write
							 * and the number of bytes buffered up in largeWriteMemory
							 * for the next write.
							 *
							 * NOTE: The currently allocated buffer (see bufferLen)
							 * may spill across into afterBufferMemory.
							 */
	
	/*
	 * Buffer level members.
	 */	
    int32                bufferLen;
							/*
							 * The buffer within largeWriteMemory given to the caller to
							 * fill in.  It starts after largeWriteLen bytes (i.e. the
							 * offset) and is bufferLen bytes long.
							 */

	/*
	 * File level members.
	 */
	File 				 file;
    char				 *filePathName;
    int64                fileLen;
    int64				 fileLen_uncompressed; /* for calculating compress ratio */

	int64				initialSetFilePosition;

	MirroredAppendOnlyOpen		mirroredOpen;

} BufferedAppend;

/*
 * Determines the amount of memory to supply for
 * BufferedAppend given the desired buffer and
 * large write lengths.
 */
extern int32 BufferedAppendMemoryLen(
    int32                maxBufferLen,
    int32                maxLargeWriteLen);

/*
 * Initialize BufferedAppend.
 *
 * Use the BufferedAppendMemoryLen procedure to
 * determine the amount of memory to supply.
 */
extern void BufferedAppendInit(
    BufferedAppend       *bufferedAppend,
    uint8                *memory,
    int32                memoryLen,
    int32                maxBufferLen,
    int32                maxLargeWriteLen,
    char				 *relationName);

/*
 * Takes an open file handle for the next file.
 */
extern void BufferedAppendSetFile(
    BufferedAppend       *bufferedAppend,
    File 				 file,
    char				 *filePathName,
    int64				 eof,
    int64				 eof_uncompressed);

/*
 * Return the position of the current write buffer in bytes.
 */
extern int64 BufferedAppendCurrentBufferPosition(
    BufferedAppend     *bufferedAppend);

/*
 * Return the position of the next write buffer in bytes.
 */
extern int64 BufferedAppendNextBufferPosition(
    BufferedAppend     *bufferedAppend);

/*
 * Get the next buffer space for appending with a specified length.
 *
 * Returns NULL when the current file does not have enough
 * room for another buffer.
 */
extern uint8 *BufferedAppendGetBuffer(
    BufferedAppend       *bufferedAppend,
    int32				 bufferLen);

/*
 * Get the address of the current buffer space being used appending.
 */
extern uint8 *BufferedAppendGetCurrentBuffer(
    BufferedAppend       *bufferedAppend);

/*
 * Get the next maximum length buffer space for appending.
 *
 * Returns NULL when the current file does not have enough
 * room for another buffer.
 */
extern uint8 *BufferedAppendGetMaxBuffer(
    BufferedAppend       *bufferedAppend);

/*
 * Cancel the last GetBuffer or GetMaxBuffer.
 */
extern void BufferedAppendCancelLastBuffer(
    BufferedAppend       *bufferedAppend);

/*
 * Indicate the current buffer is finished.
 */
extern void BufferedAppendFinishBuffer(
    BufferedAppend       *bufferedAppend,
    int32                usedLen,
    int32				 usedLen_uncompressed);

/*
 * Returns the length of the current file.
 */
extern int64 BufferedAppendFileLen(
    BufferedAppend *bufferedAppend);

/*
 * Flushes the current file for append.  Caller is responsible for closing
 * the file afterwards.
 */
extern void BufferedAppendCompleteFile(
    BufferedAppend	*bufferedAppend,
    int64 			*fileLen,
    int64 			*fileLen_uncompressed);


/*
 * Finish with writing all together.
 */
extern void BufferedAppendFinish(
    BufferedAppend *bufferedAppend);

#endif   /* CDBBUFFEREDAPPEND_H */
