/*-------------------------------------------------------------------------
 *
 * cdbappendonlystoragewrite.h
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbappendonlystoragewrite.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBAPPENDONLYSTORAGEWRITE_H
#define CDBAPPENDONLYSTORAGEWRITE_H

#include "catalog/pg_appendonly.h"
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
	bool		isActive;

	/*
	 * The memory context to use for buffers and other memory needs.
	 */
	MemoryContext memoryContext;

	/*
	 * The maximum Append-Only Storage Block length including all storage
	 * headers.
	 */
	int32		maxBufferLen;

	/*
	 * The large write length given to the BufferedAppend module.
	 * The buffer in BufferedAppend will be flushed to disk when
	 * maxLargeWriteLen is reached.
	 */
	int32		maxLargeWriteLen;

	/*
	 * Version number indicating the AO table format version to write in.
	 */
	AORelationVersion formatVersion;

	/*
	 * Name of the relation to use in system logging and error messages.
	 */
	char	   *relationName;

	/*
	 * A phrase that better describes the purpose of the this open.
	 *
	 * The caller manages the storage for this.
	 */
	char	   *title;

	/*
	 * The Append-Only Storage Attributes from relation creation.
	 */
	AppendOnlyStorageAttributes storageAttributes;

	/*
	 * Fixed header length as determined by the checksum flag in
	 * storageAttributes.
	 */
	int32		regularHeaderLen;

	/*
	 * The BufferedAppend module's object that holds write session data.
	 */
	BufferedAppend bufferedAppend;

	/*
	 * Name of the current segment file name to use in system logging and
	 * error messages.
	 */
	char	   *segmentFileName;

	/*
	 * The handle to the current open segment file.
	 */
	File		file;

	/*
	 * The committed EOF at the beginning of the write open.
	 */
	int64		startEof;

	RelFileNodeBackend relFileNode;
	int32		segmentFileNum;

	/*
	 * The number of blocks written since the beginning of the segment file.
	 */
	int64		bufferCount;

	/*
	 * When true, the first row number for the next block has been explicitly
	 * set and will be stored in the Append-Only Storage Header.
	 */
	bool		isFirstRowNumSet;

	/* Explicitly set first row number for the next block. */
	int64		firstRowNum;

	/*
	 * The beginning of the logical block which will be used to record in
	 * block-directory. So, incase of datum broken down into multiple blocks
	 * this points to first / starting block, which would be large content
	 * block for such a case.
	 */
	int64		logicalBlockStartOffset;

	/* The kind of header specifed to ~_GetBuffer. */
	AoHeaderKind getBufferAoHeaderKind;

	/*
	 * Complete header length of the current block.  Varies depending on
	 * whether the first row number was set.
	 */
	int32		currentCompleteHeaderLen;

	/*
	 * Current block pointer within the BufferedAppend buffer.
	 */
	uint8	   *currentBuffer;

	/*
	 * A temporary buffer that is given back from *_GetBuffer when compression
	 * is being done.
	 */
	uint8	   *uncompressedBuffer;

	/*
	 * Number of bytes of extra buffer must have beyond the output compression
	 * buffer needed for spillover for some compression libraries.
	 */
	int32		compressionOverrunLen;

	/* The maximum buffer plus compression overrun byte length */
	int32		maxBufferWithCompressionOverrrunLen;

	/*
	 * When non-null, we are doing VerifyBlock and this is the buffer to
	 * decompress the just compressed block into so we can memory comprare it
	 * with the input.
	 */
	uint8	   *verifyWriteBuffer;

	/*
	 * Add these two byte lengths to get the length of the
	 * qlzScratchDecompress buffer.
	 */

	/* Storage attributes */
	CompressionState *compressionState;
	CompressionState *verifyWriteCompressionState;		/* This is only valid if
														 * the
														 * gp_appendonly_verify_w
														 * rite_block GUC is
														 * set. */
	int			blocksize;		/* For AO or CO uncompresed block size		   */
	PGFunction *compression_functions;	/* For AO or CO compression.                   */
	/* The array index corresponds to COMP_FUNC_*  */

	bool needsWAL;

} AppendOnlyStorageWrite;

extern void AppendOnlyStorageWrite_Init(AppendOnlyStorageWrite *storageWrite,
										MemoryContext memoryContext,
										int32 maxBufferLen,
										char *relationName,
										char *title,
										AppendOnlyStorageAttributes *storageAttributes,
										bool needsWAL);
extern void AppendOnlyStorageWrite_FinishSession(AppendOnlyStorageWrite *storageWrite);

extern void AppendOnlyStorageWrite_TransactionCreateFile(AppendOnlyStorageWrite *storageWrite,
											 RelFileNodeBackend *relFileNode,
											 int32 segmentFileNum);
extern void AppendOnlyStorageWrite_OpenFile(AppendOnlyStorageWrite *storageWrite,
								char *filePathName,
								int version,
								int64 logicalEof,
								int64 fileLen_uncompressed,
								RelFileNodeBackend *relFileNode,
								int32 segmentFileNum);
extern void AppendOnlyStorageWrite_FlushAndCloseFile(AppendOnlyStorageWrite *storageWrite,
											 int64 *newLogicalEof,
											 int64 *fileLen_uncompressed);
extern void AppendOnlyStorageWrite_TransactionFlushAndCloseFile(AppendOnlyStorageWrite *storageWrite,
													int64 *newLogicalEof,
												int64 *fileLen_uncompressed);

extern int32 AppendOnlyStorageWrite_CompleteHeaderLen(AppendOnlyStorageWrite *storageWrite,
										 AoHeaderKind aoHeaderKind);
extern uint8 *AppendOnlyStorageWrite_GetBuffer(AppendOnlyStorageWrite *storageWrite,
								 int aoHeaderKind);
extern bool AppendOnlyStorageWrite_IsBufferAllocated(
									   AppendOnlyStorageWrite *storageWrite);
extern int64 AppendOnlyStorageWrite_LogicalBlockStartOffset(
									   AppendOnlyStorageWrite *storageWrite);
extern int64 AppendOnlyStorageWrite_CurrentPosition(
									   AppendOnlyStorageWrite *storageWrite);
extern uint8 *AppendOnlyStorageWrite_GetCurrentInternalBuffer(
									   AppendOnlyStorageWrite *storageWrite);

extern void AppendOnlyStorageWrite_FinishBuffer(
									AppendOnlyStorageWrite *storageWrite,
									int32 contentLen,
									int executorBlockKind,
									int rowCount);

extern void AppendOnlyStorageWrite_CancelLastBuffer(AppendOnlyStorageWrite *storageWrite);

extern void AppendOnlyStorageWrite_Content(AppendOnlyStorageWrite *storageWrite,
							   uint8 *content,
							   int32 contentLen,
							   int executorBlockKind,
							   int rowCount);
extern void AppendOnlyStorageWrite_SetFirstRowNum(AppendOnlyStorageWrite *storageWrite,
									  int64 firstRowNum);

extern char *AppendOnlyStorageWrite_ContextStr(AppendOnlyStorageWrite *storageWrite);

#endif   /* CDBAPPENDONLYSTORAGEWRITE_H */
