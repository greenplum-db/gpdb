/*-------------------------------------------------------------------------
 *
 * cdbappendonlystorageread.h
 *
 * Portions Copyright (c) 2007-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbappendonlystorageread.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBAPPENDONLYSTORAGEREAD_H
#define CDBAPPENDONLYSTORAGEREAD_H

#include "catalog/pg_appendonly.h"
#include "catalog/pg_compression.h"
#include "cdb/cdbappendonlystorage.h"
#include "cdb/cdbappendonlystoragelayer.h"
#include "cdb/cdbbufferedread.h"
#include "utils/palloc.h"
#include "storage/fd.h"


/*
 * This structure contains information about the current block.
 */
typedef struct AppendOnlyStorageReadCurrent
{
	/*
	 * The file offset of the current block.
	 */
	int64		headerOffsetInFile;

	/*
	 * The Append-Only Storage block kind.
	 */
	AoHeaderKind headerKind;

	/*
	 * Actual header length that includes optional header information (e.g.
	 * firstRowNum).
	 */
	int32		actualHeaderLen;

	/*
	 * The byte length of the whole block.
	 */
	int32		overallBlockLen;

	/*
	 * The byte offset of the content (executor data) within the block. It may
	 * be compressed.
	 */
	int32		contentOffset;

	/*
	 * The original content length.
	 */
	int32		uncompressedLen;

	/*
	 * The executor block kind.  Defined externally by executor.
	 */
	int			executorBlockKind;

	/*
	 * True if the first row number was explicity specified for this block.
	 */
	bool		hasFirstRowNum;

	/*
	 * The first row number of this block, or INT64CONST(-1) if not specified
	 * for this block.
	 */
	int64		firstRowNum;

	/*
	 * The number of rows in this block.
	 */
	int32		rowCount;

	/*
	 * The block read was metadata for large content.  The actual content is
	 * in one or more following small content blocks.
	 */
	bool		isLarge;

	/*
	 * True if the small content block has compressed content. Not applicable
	 * for the large content metadata.
	 */
	bool		isCompressed;

	/*
	 * The compressed length of the content.
	 */
	int32		compressedLen;
} AppendOnlyStorageReadCurrent;

/*
 * This structure contains read session information.  Consider the fields
 * inside to be private.
 */
typedef struct AppendOnlyStorageRead
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
	 * The large read length given to the BufferedRead module.
	 */
	int32		largeReadLen;

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
	 * The BufferedRead module's object that holds read session data.
	 */
	BufferedRead bufferedRead;

	/*
	 * The handle to the current open segment file.
	 */
	File		file;

	/*
	 * The byte length of the current segment file being read.
	 */
	int64		logicalEof;

	/*
	 * Version number indicating the AO table format version to read in.
	 */
	int			formatVersion;

	/*
	 * The minimum block header length based on the checkum attribute. Does
	 * not include optional header data (e.g. firstRowNum).
	 */
	int32		minimumHeaderLen;

	/*
	 * Name of the current segment file name to use in system logging and
	 * error messages.
	 */
	char	   *segmentFileName;

	/*
	 * The number of blocks read since the beginning of the segment file.
	 */
	int64		bufferCount;

	/*
	 * Lots of information about the current block that was read.
	 */
	AppendOnlyStorageReadCurrent current;

	/* Storage attributes */
	CompressionState *compressionState;
	int			blocksize;		/* For AO or CO uncompresed block size			*/
	PGFunction *compression_functions;	/* For AO or CO compression function
										 * pointers. The array index
										 * corresponds to COMP_FUNC_*	*/

} AppendOnlyStorageRead;

extern void AppendOnlyStorageRead_Init(AppendOnlyStorageRead *storageRead,
						   MemoryContext memoryContext,
						   int32 maxBufferLen,
						   char *relationName, char *title,
						   AppendOnlyStorageAttributes *storageAttributes);

extern char *AppendOnlyStorageRead_RelationName(AppendOnlyStorageRead *storageRead);
extern char *AppendOnlyStorageRead_SegmentFileName(AppendOnlyStorageRead *storageRead);
extern void AppendOnlyStorageRead_FinishSession(AppendOnlyStorageRead *storageRead);

extern void AppendOnlyStorageRead_OpenFile(AppendOnlyStorageRead *storageRead,
							   char *filePathName, int version, int64 logicalEof);
extern bool AppendOnlyStorageRead_TryOpenFile(AppendOnlyStorageRead *storageRead,
								  char *filePathName, int version, int64 logicalEof);
extern void AppendOnlyStorageRead_SetTemporaryRange(AppendOnlyStorageRead *storageRead,
							   int64 beginFileOffset, int64 afterFileOffset);
extern void AppendOnlyStorageRead_CloseFile(AppendOnlyStorageRead *storageRead);

extern bool AppendOnlyStorageRead_GetBlockInfo(AppendOnlyStorageRead *storageRead,
								   int32 *contentLen, int *executorBlockKind,
								   int64 *firstRowNum, int *rowCount,
								   bool *isLarge, bool *isCompressed);

extern bool AppendOnlyStorageRead_ReadNextBlock(AppendOnlyStorageRead *storageRead);
extern int64 AppendOnlyStorageRead_CurrentHeaderOffsetInFile(AppendOnlyStorageRead *storageRead);
extern int64 AppendOnlyStorageRead_CurrentCompressedLen(AppendOnlyStorageRead *storageRead);
extern int64 AppendOnlyStorageRead_OverallBlockLen(AppendOnlyStorageRead *storageRead);
extern uint8 *AppendOnlyStorageRead_GetBuffer(AppendOnlyStorageRead *storageRead);
extern void AppendOnlyStorageRead_Content(AppendOnlyStorageRead *storageRead,
							  uint8 *contentOut, int32 contentLen);
extern void AppendOnlyStorageRead_SkipCurrentBlock(AppendOnlyStorageRead *storageRead);

extern char *AppendOnlyStorageRead_ContextStr(AppendOnlyStorageRead *storageRead);
extern int	errcontext_appendonly_read_storage_block(AppendOnlyStorageRead *storageRead);
extern char *AppendOnlyStorageRead_StorageContentHeaderStr(AppendOnlyStorageRead *storageRead);
extern int	errdetail_appendonly_read_storage_content_header(AppendOnlyStorageRead *storageRead);

#endif   /* CDBAPPENDONLYSTORAGEREAD_H */
