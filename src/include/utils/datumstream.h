/*-------------------------------------------------------------------------
 *
 * datumstream.h
 *
 * Portions Copyright (c) 2008, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/datumstream.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef DATUMSTREAM_H
#define DATUMSTREAM_H

#include "catalog/pg_attribute.h"
#include "utils/datumstreamblock.h"

/*
 * Magic number.  Max number of datum in on block.
 * MUST fit in 15 bit, or we overflow ndatum(int16), defined
 * in DatumStreamBlock.
 *
 * It could be uint16 (64K), but the negative numbers have not
 * be carefully debugged.
 *
 * This number also needs to be not greater than
 * AOBlockHeader_MaxRowCount. Since AOBlockHeader_MaxRowCount is
 * 14-bit, we just use it here.
 */
#define MAXDATUM_PER_AOCS_ORIG_BLOCK AOSmallContentHeader_MaxRowCount

/*
 * A dense content Append-Only block allows for many more items.
 */
#define INITIALDATUM_PER_AOCS_DENSE_BLOCK AOSmallContentHeader_MaxRowCount
/*	UNDONE: For now, just do Small Content */
#define MAXDATUM_PER_AOCS_DENSE_BLOCK AONonBulkDenseContentHeader_MaxLargeRowCount

typedef struct DatumStreamWrite
{
	DatumStreamTypeInfo typeInfo;

	/*
	 * Version of datum stream block format -- original or RLE_TYPE,
	 */
	DatumStreamVersion datumStreamVersion;

	bool		rle_want_compression;
	bool		delta_want_compression;

	int32		maxAoBlockSize;
	int32		maxAoHeaderSize;

	char	   *title;

	/* AO Storage */
	bool		need_close_file;
	AppendOnlyStorageAttributes ao_attr;
	AppendOnlyStorageWrite ao_write;

	int64		blockFirstRowNum;

	DatumStreamBlockWrite blockWrite;

	/*
	 * EOFs of current segment file.
	 */
	int64		eof;
	int64		eofUncompress;
}	DatumStreamWrite;

typedef enum DatumStreamLargeObjectState
{
	DatumStreamLargeObjectState_None = 0,
	DatumStreamLargeObjectState_HaveAoContent = 1,
	DatumStreamLargeObjectState_PositionAdvanced = 2,
	DatumStreamLargeObjectState_Consumed = 3,
	DatumStreamLargeObjectState_Exhausted = 4,

	MaxDatumStreamLargeObjectState
}	DatumStreamLargeObjectState;

typedef struct DatumStreamRead
{
	/*--------------------------------------------------------------------------
	 * Information for reading blocks.
	 */

	/* Information collected and adjusted for the current block. */
	int64		blockFirstRowNum;
	int64		blockFileOffset;
	int			blockRowCount;

	AppendOnlyStorageRead ao_read;

	/*
	 * Values returned by AppendOnlyStorageRead_GetBlockInfo.
	 */
	struct getBlockInfo
	{
		int32		contentLen;
		int			execBlockKind;
		int64		firstRow;	/* is expected to be -1 for pre-4.0 blocks */
		int			rowCnt;
		bool		isLarge;
		bool		isCompressed;
	}			getBlockInfo;

	uint8	   *buffer_beginp;

	/*-------------------------------------------------------------------------
	 * For better CPU data cache locality, put commonly used variables of
	 * datumstreamread_get and datumstreamread_advance here.
	 */
	DatumStreamLargeObjectState largeObjectState;

	DatumStreamBlockRead blockRead;

	/*-------------------------------------------------------------------------
	 * Less commonly used fields.
	 */

	MemoryContext memctxt;

	uint8	   *large_object_buffer;
	int32		large_object_buffer_size;

	/*
	 * Temporary space for storing a Datum that has to be upgraded from a prior
	 * AO format version but can't be upgraded in place. Allocated only as
	 * needed, and holds only one value at a time.
	 */
	void	   *datum_upgrade_buffer;
	size_t		datum_upgrade_buffer_size;

	/* EOF of current file */
	int64		eof;
	int64		eofUncompress;

	AppendOnlyStorageAttributes ao_attr;

	/*
	 * Version of datum stream block format -- original or RLE_TYPE,
	 */
	DatumStreamVersion datumStreamVersion;

	DatumStreamTypeInfo typeInfo;

	/*
	 * Cached base type, to assist with numeric upgrades. Initialized lazily
	 * when needed.
	 */
	Oid			baseTypeOid;

	bool		rle_can_have_compression;
	bool		delta_can_have_compression;

	int32		maxAoBlockSize;
	int32		maxDataBlockSize;

	char	   *title;

	/* AO Storage */
	bool		need_close_file;

}	DatumStreamRead;

/*
 * A structure contains the state when fetching rows
 * through datum stream.
 */
typedef struct DatumStreamFetchDescData
{
	DatumStreamRead *datumStream;

	CurrentSegmentFile currentSegmentFile;

	int64		scanNextFileOffset;
	int64		scanNextRowNum;

	int64		scanAfterFileOffset;
	int64		scanLastRowNum;

	CurrentBlock currentBlock;

}	DatumStreamFetchDescData;

typedef DatumStreamFetchDescData *DatumStreamFetchDesc;

/* Stream access method */
extern void datumstreamread_getlarge(DatumStreamRead * ds, Datum *datum, bool *null);
inline static void
datumstreamread_get(DatumStreamRead * acc, Datum *datum, bool *null)
{
	if (acc->largeObjectState == DatumStreamLargeObjectState_None)
	{
		/*
		 * Small objects are handled by the DatumStreamBlockRead module.
		 */
		DatumStreamBlockRead_Get(&acc->blockRead, datum, null);
	}
	else
	{
		datumstreamread_getlarge(acc, datum, null);
	}
}

extern int	datumstreamread_advancelarge(DatumStreamRead * ds);
inline static int
datumstreamread_advance(DatumStreamRead * acc)
{
	if (acc->largeObjectState == DatumStreamLargeObjectState_None)
	{
		/*
		 * Small objects are handled by the DatumStreamBlockRead module.
		 */
		return DatumStreamBlockRead_Advance(&acc->blockRead);
	}
	else
	{
		return datumstreamread_advancelarge(acc);
	}
}

extern int	datumstreamread_nthlarge(DatumStreamRead * ds);
inline static int
datumstreamread_nth(DatumStreamRead * acc)
{
	if (acc->largeObjectState == DatumStreamLargeObjectState_None)
	{
		/*
		 * Small objects are handled by the DatumStreamBlockRead module.
		 */
		return DatumStreamBlockRead_Nth(&acc->blockRead);
	}
	else
	{
		return datumstreamread_nthlarge(acc);
	}
}

/* ------------------------------------------------------------------------------ */

extern int datumstreamwrite_put(
					 DatumStreamWrite * acc,
					 Datum d,
					 bool null,
					 void **toFree);
extern int	datumstreamwrite_nth(DatumStreamWrite * ds);

/* ctor and dtor */
extern DatumStreamWrite *create_datumstreamwrite(
						char *compName,
						int32 compLevel,
						bool checksum,
						int32 safeFSWriteSize,
						int32 maxsz,
						Form_pg_attribute attr,
						char *relname,
						char *title,
						bool needsWAL);

extern DatumStreamRead *create_datumstreamread(
					   char *compName,
					   int32 compLevel,
					   bool checksum,
					   int32 safeFSWriteSize,
					   int32 maxsz,
					   Form_pg_attribute attr,
					   char *relname,
					   char *title);

extern void datumstreamwrite_open_file(
						   DatumStreamWrite * ds,
						   char *fn,
						   int64 eof,
						   int64 eofUncompressed,
						   RelFileNodeBackend *relFileNode,
						   int32 segmentFileNum,
						   int version);

extern void datumstreamread_open_file(
						  DatumStreamRead * ds,
						  char *fn,
						  int64 eof,
						  int64 eofUncompressed,
						  RelFileNode relFileNode,
						  int32 segmentFileNum,
						  int version);

extern void datumstreamwrite_close_file(DatumStreamWrite * ds);
extern void datumstreamread_close_file(DatumStreamRead * ds);
extern void destroy_datumstreamwrite(DatumStreamWrite * ds);
extern void destroy_datumstreamread(DatumStreamRead * ds);

/* Read and Write op */
extern int64 datumstreamwrite_block(DatumStreamWrite *ds,
									AppendOnlyBlockDirectory *blockDirectory,
									int columnGroupNo,
									bool addColAction);
extern int64 datumstreamwrite_lob(DatumStreamWrite *ds,
								  Datum d,
								  AppendOnlyBlockDirectory *blockDirectory,
								  int columnGroupNo,
								  bool addColAction);
extern int	datumstreamread_block(DatumStreamRead * ds,
								  AppendOnlyBlockDirectory *blockDirectory,
								  int colGroupNo);
extern void datumstreamread_find(DatumStreamRead * datumStream,
					 int32 rowNumInBlock);
extern void datumstreamread_rewind_block(DatumStreamRead * datumStream);
extern bool datumstreamread_find_block(DatumStreamRead * datumStream,
						   DatumStreamFetchDesc datumStreamFetchDesc,
						   int64 rowNum);
extern void *datumstreamread_get_upgrade_space(DatumStreamRead *datumStream,
											   size_t len);

/*
 * MPP-17061: make sure datumstream_read_block_info was called first for the CO block
 * before calling datumstreamread_block_content.
 */
extern void datumstreamread_block_content(DatumStreamRead * acc);

#endif   /* DATUMSTREAM_H */
