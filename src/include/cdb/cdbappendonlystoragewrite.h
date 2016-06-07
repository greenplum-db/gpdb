/*-------------------------------------------------------------------------
 *
 * cdbappendonlystoragewrite.h
 *
 * Copyright (c) 2007-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBAPPENDONLYSTORAGEWRITE_H
#define CDBAPPENDONLYSTORAGEWRITE_H

#include "catalog/pg_compression.h"
#include "cdb/cdbappendonlystorage.h"
#include "cdb/cdbappendonlystoragelayer.h"
#include "cdb/cdbbufferedappend.h"
#include "utils/palloc.h"
#include "storage/fd.h"



/*
 * This structure contains write session information.  Consider the fields
 * inside to be private.
 */
typedef struct AppendOnlyStorageWrite
{
	bool	isActive;

	MemoryContext memoryContext;
			/*
			 * The memory context to use for buffers and
			 * other memory needs.
			 */

    int32         maxBufferLen;
			/*
			 * The maximum Append-Only Storage Block
			 * length including all storage headers.
			 */

	int32		largeWriteLen;
			/*
			 * The large write length given to the BufferedAppend module.
			 */

	char		 *relationName;
			/*
			 * Name of the relation to use in system
			 * logging and error messages.
			 */

	char		*title;
			/*
			 * A phrase that better describes the purpose of the this open.
			 *
			 * The caller manages the storage for this.
			 */
			 
	AppendOnlyStorageAttributes		storageAttributes;
			/*
			 * The Append-Only Storage Attributes
			 * from relation creation.
			 */

	int32			regularHeaderLen;
			/*
			 * Fixed header length as determined by the checksum flag in
			 * storageAttributes.
			 */

	BufferedAppend	bufferedAppend;
			/*
			 * The BufferedAppend module's object that holds write session data.
			 */

	char		 *segmentFileName;
			/*
			 * Name of the current segment file name to use in system
			 * logging and error messages.
			 */

	File	file;
			/*
			 * The handle to the current open segment file.
			 */

	int64	startEof;
			/*
			 * The committed EOF at the beginning of the write open.
			 */

	RelFileNode	relFileNode;
	int32		segmentFileNum;

	ItemPointerData 	persistentTid;
	bool tidAllowedToBeZero;
	int64				persistentSerialNum;
			/*
			 * Persistence information for current write open.
			 */

	int64	bufferCount;
			/*
			 * The number of blocks written since the beginning of the segment file.
			 */

	bool	isFirstRowNumSet;
			/*
			 * When true, the first row number for the next block has
			 * been explicitly set and will be stored in the Append-Only
			 * Storage Header.
			 */

	int64	firstRowNum;
			/* Explicitly set first row number for the next block. */

	int64   lastWriteBeginPosition;
			/* The beginning of the write buffer for the last write. */

	AoHeaderKind	getBufferAoHeaderKind;
			/* The kind of header specifed to ~_GetBuffer. */

	int32	currentCompleteHeaderLen;
			/*
			 * Complete header length of the current block.  Varies depending on whether the
			 * first row number was set.
			 */

	uint8	*currentBuffer;
			 /*
			  * Current block pointer within the BufferedAppend buffer.
			  */

	uint8	*uncompressedBuffer;
			 /*
			  * A temporary buffer that is given back from *_GetBuffer when
			  * compression is being done.
			  */

	int32	compressionOverrunLen;
			/*
			 * Number of bytes of extra buffer must have beyond the output
			 * compression buffer needed for spillover for some compression libraries.
			 */

	int32	maxBufferWithCompressionOverrrunLen;
			/* The maximum buffer plus compression overrun byte length */

	uint8	*verifyWriteBuffer;
			/*
			 * When non-null, we are doing VerifyBlock and this
			 * is the buffer to decompress the just compressed block
			 * into so we can memory comprare it with the input.
			 */
	/*
	 * Add these two byte lengths to get the length of the qlzScratchDecompress buffer.
	 */

	/* Storage attributes */
	CompressionState *compressionState;
	CompressionState *verifyWriteCompressionState; /* This is only valid if the gp_appendonly_verify_write_block GUC is set. */
	int               blocksize;                   /* For AO or CO uncompresed block size         */
	PGFunction       *compression_functions;       /* For AO or CO compression.                   */
			/* The array index corresponds to COMP_FUNC_*  */

} AppendOnlyStorageWrite;

// -----------------------------------------------------------------------------
// Initialization
// -----------------------------------------------------------------------------

/*
 * Initialize AppendOnlyStorageWrite.
 *
 * The AppendOnlyStorageWrite data structure is initialized
 * once for a append �session� and can be used to add
 * Append-Only Storage Blocks to 1 or more segment files.
 *
 * The current file to write to is opened with the
 * AppendOnlyStorageWrite_OpenFile routine.
*/
extern void AppendOnlyStorageWrite_Init(
    AppendOnlyStorageWrite			*storageWrite,
				/* The data structure to initialize. */

	MemoryContext 					memoryContext,
				/*
				 * The memory context to use for buffers and
				 * other memory needs.  When NULL, the
				 * current memory context is used.
				 */
    int32                			maxBufferLen,
				/*
				 * The maximum Append-Only Storage Block
				 * length including all storage headers.
				 */
	char							*relationName,
				/*
				 * Name of the relation to use in system
				 * logging and error messages.
				 */

	char							*title,
				/*
				 * A phrase that better describes the purpose of the this open.
				 *
				 * The caller manages the storage for this.
				 */

	AppendOnlyStorageAttributes		*storageAttributes);
				/*
				 * The Append-Only Storage Attributes
				 * from relation creation.
				 */

/*
 * Return (read-only) pointer to relation name.
 */
extern char* AppendOnlyStorageWrite_RelationName(
	AppendOnlyStorageWrite			*storageWrite);


/*
 * Finish using the AppendOnlyStorageWrite session created with ~Init.
 */
extern void AppendOnlyStorageWrite_FinishSession(
	AppendOnlyStorageWrite			*storageWrite);
				/* The data structure to finish. */

// -----------------------------------------------------------------------------
// Open and FlushAndClose
// -----------------------------------------------------------------------------

/*
 * Creates an on-demand Append-Only segment file under transaction.
 */
void AppendOnlyStorageWrite_TransactionCreateFile(
	AppendOnlyStorageWrite		*storageWrite,

	char						*filePathName,
				/* The name of the segment file to open. */

	int64						logicalEof,
				/*
				 * The last committed write transaction's EOF
				 * value to use as the end of the segment
				 * file.
				 *
				 * If the EOF is 0, we will create the file
				 * if necessary.  Otherwise, it must already
				 * exist.
				 */
	RelFileNode 				*relFileNode,
	int32						segmentFileNum,
	ItemPointer 				persistentTid,
	int64						*persistentSerialNum);

/*
 * Opens the next segment file to write.  The file must already exist.
 *
 * This routine is responsible for seeking to the proper
 * write location given the logical EOF.
 */
extern void AppendOnlyStorageWrite_OpenFile(
	AppendOnlyStorageWrite		*storageWrite,

    char				 		*filePathName,
				/* The name of the segment file to open. */

    int64				 		logicalEof,
				/*
				 * The last committed write transaction�s EOF
				 * value to use as the end of the segment
				 * file.
				 *
				 * If the EOF is 0, we will create the file
				 * if necessary.  Otherwise, it must already
				 * exist.
				 */
	int64						fileLen_uncompressed,
	RelFileNode					*relFileNode,
	int32						segmentFileNum,
	ItemPointer 				persistentTid,
	int64						persistentSerialNum);

/*
 * Flush and close the current segment file.
 *
 * No error if the current is already closed.
 */
extern void AppendOnlyStorageWrite_FlushAndCloseFile(
	AppendOnlyStorageWrite		*storageWrite,

	int64						*newLogicalEof,
				/* The new EOF for the segment file. */

	int64						*fileLen_uncompressed,

	bool						*mirrorDataLossOccurred,

	bool 						*mirrorCatchupRequired,

	MirrorDataLossTrackingState 	*originalMirrorDataLossTrackingState,

	int64							*originalMirrorDataLossTrackingSessionNum);

/*
 * Flush and close the current segment file under a transaction.
 *
 * Handles mirror loss end transaction work.
 *
 * No error if the current is already closed.
 */
void AppendOnlyStorageWrite_TransactionFlushAndCloseFile(
	AppendOnlyStorageWrite		*storageWrite,

	int64						*newLogicalEof,
				/* The new EOF for the segment file. */

	int64						*fileLen_uncompressed);

// -----------------------------------------------------------------------------
// Usable Block Length
// -----------------------------------------------------------------------------

/*
 *    When writing �short� content intended to stay within the maxBufferLen (also known as
 *    blocksize), some of the buffer will be used for the Append-Only Block Header.  This
 *    function returns that overhead length.
 *
 *    Isn�t the length of the Append-Only Storage Block constant? NO.
 *
 *    Currently, there are two things that can make it longer.  When checksums are configured,
 *    we add checksum data to the header.  And there is optional header data
 *   (e.g. firstRowNum).
 *
 *   We call the header portion with the optional checksums the fixed header because we
 *   need to be able to read and evaluate the checksums before we can interpret flags
 *   in the fixed header that indicate there is more header information.
 *
 *  The complete header length is the fixed header plus optional information.
 */

/*
 * Returns the Append-Only Storage Block fixed header length in bytes.
 */
extern int32 AppendOnlyStorageWrite_FixedHeaderLen(
	AppendOnlyStorageWrite		*storageWrite);

/*
 * Returns the Append-Only Storage Block complete header length in bytes.
 *
 * Call this routine after adding all optional header information for the current block
 * begin written.
 */
extern int32 AppendOnlyStorageWrite_CompleteHeaderLen(
	AppendOnlyStorageWrite		*storageWrite,
	AoHeaderKind				aoHeaderKind);

// -----------------------------------------------------------------------------
// Writing �Short� Content Efficiently that is not being Bulk Compressed
// -----------------------------------------------------------------------------

/*
 *    This section describes for writing content that is less than or equal to the blocksize
 *    (e.g. 32k) bytes that is not being bulk compressed by the Append-Only Storage Layer.
 *
 *    Actually, the content is limited to blocksize minus the Append-Only header size
 *    (see the AppendOnlyStorageWrite_HeaderLen routine).
 */


/*
 * Get a pointer to next maximum length buffer space for
 * appending �small� content.
 *
 * NOTE: The maximum length buffer space =
 *				maxBufferLen �
 *				AppendOnlyStorageWrite_HeaderLen(...)
 *
 * When compression is not being used, this interface provides a pointer directly into the
 * write buffer for efficient data generation.  Otherwise, a pointer to a temporary buffer
 * will be provided.
 *
 * Returns NULL when the current file does not have enough
 * room for another buffer.
 */
extern uint8 *AppendOnlyStorageWrite_GetBuffer(
	AppendOnlyStorageWrite		*storageWrite,
	int 						aoHeaderKind);

/*
 * Test if a buffer is currently allocated.
 */
extern bool AppendOnlyStorageWrite_IsBufferAllocated(
	AppendOnlyStorageWrite		*storageWrite);

/*
 * Return the beginning of the last write position of
 * the write buffer.
 */
extern int64 AppendOnlyStorageWrite_LastWriteBeginPosition(
	AppendOnlyStorageWrite *storageWrite);

/*
 * Return the position of the current write buffer.
 */
extern int64 AppendOnlyStorageWrite_CurrentPosition(
	AppendOnlyStorageWrite		*storageWrite);

/*
 * Return the internal current write buffer that includes the header.
 * UNDONE: Fix this interface privacy violation...
 */
extern uint8 *AppendOnlyStorageWrite_GetCurrentInternalBuffer(
		AppendOnlyStorageWrite		*storageWrite);

/*
 * Mark the current buffer "small" buffer as finished.
 *
 * If compression is configured, we will try to compress the contents in
 * the temporary uncompressed buffer into the write buffer.
 *
 * The buffer can be scheduled for writing and reused.
 */
extern void AppendOnlyStorageWrite_FinishBuffer(
	AppendOnlyStorageWrite		*storageWrite,

    int32                		contentLen,
			/*
			 * The byte length of the content generated
			 * directly into the buffer returned by
			 * AppendOnlyStorageWrite_GetBuffer.
			 */
	int							executorBlockKind,
			/*
			 * A value defined externally by the executor
			 * that describes in content stored in the
			 * Append-Only Storage Block.
			 */
	int							rowCount);
			/* The number of rows stored in the content. */

/*
 * Cancel the last ~GetBuffer call.
 *
 * This will also turn off the firstRowNum flag.
 */
extern void AppendOnlyStorageWrite_CancelLastBuffer(
	AppendOnlyStorageWrite		*storageWrite);


// -----------------------------------------------------------------------------
// Writing "Large" and/or Compressed Content
// -----------------------------------------------------------------------------

/*
 *    This section describes for writing long content that can be up to 1 Gb long and/or
 *    content that will be bulk-compressed when configured.
 */


/*
 * Write content up to 1Gb.
 *
 * Large content will be writen in fragment blocks by
 * the Append-Only Storage Layer.
 *
 * If compression is configured, then the content will be
 * compressed in fragments.
 *
 * Returns NULL when the current file does not have enough
 * room for another buffer.
 */
extern void AppendOnlyStorageWrite_Content(
	AppendOnlyStorageWrite		*storageWrite,

	uint8						*content,
			/* The content to store.  All contiguous. */

	int32                		contentLen,
			/* The byte length of the data to store. */

	int							executorBlockKind,
			/*
			 * A value defined externally by the executor
			 * that describes in content stored in the
			 * Append-Only Storage Block.
			 */
	int							rowCount);
			/* The number of rows stored in the content. */


// -----------------------------------------------------------------------------
// Optional: Set First Row Number
// -----------------------------------------------------------------------------

/*
 * Normally, the first row of an Append-Only Storage Block is implicit.  It is the last row
 * number of the previous block + 1.	However, to support BTree indicies that stored TIDs
 * in shared-memory/disk before the transaction commits, we may need to not reuse row
 * numbers of aborted transactions.  So, this routine tells the Append-Only Storage Layer
 * to explicitly keep the first row number.	This will take up more header overhead, so the
 * AppendOnlyStorageWrite_HeaderLen routine should be called afterwards to get the
 * new overhead length.
 */


/*
 * Set the first row value for the next Append-Only Storage
 * Block to be written.  Only applies to the next block.
 */
extern void AppendOnlyStorageWrite_SetFirstRowNum(
	AppendOnlyStorageWrite		*storageWrite,
	int64						firstRowNum);


extern char *AppendOnlyStorageWrite_ContextStr(
	AppendOnlyStorageWrite		*storageWrite);

#endif   /* CDBAPPENDONLYSTORAGEWRITE_H */
