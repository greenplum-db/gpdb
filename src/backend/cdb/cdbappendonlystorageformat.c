/*-------------------------------------------------------------------------
 *
 * cdbappendonlystorageformat.c
 *
 * Portions Copyright (c) 2007-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbappendonlystorageformat.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "storage/gp_compress.h"
#include "cdb/cdbappendonlystorage_int.h"
#include "cdb/cdbappendonlystorage.h"
#include "cdb/cdbappendonlystorageformat.h"
#include "port/pg_crc32c.h"
#include "utils/guc.h"

static pg_crc32
AppendOnlyStorageFormat_ComputeHeaderChecksum(
											  uint8 *headerPtr,
											  int32 headerLen)
{
	pg_crc32	crc;

	Assert(headerPtr != NULL);

	/*
	 * Compute CRC of the header. The header length does not include the
	 * header checksum.
	 */
	INIT_CRC32C(crc);
	COMP_CRC32C(crc, headerPtr, headerLen);

	/*
	 * By historical accident, the checksum calculated for append-only blocks
	 * is *not* inverted, like CRC-32C checksums usually are.
	 */
	/* FIN_CRC32C(crc); */

	return crc;
}

static pg_crc32
AppendOnlyStorageFormat_ComputeBlockChecksum(
											 uint8 *headerPtr,
											 int32 headerLen,
											 int32 overallBlockLen)
{
	int32		dataOffset;
	pg_crc32	crc;

	Assert(headerPtr != NULL);

	/*
	 * The block checksum covers right after the header checksum through the
	 * end of the whole block (including the optional firstRowNum).
	 */
	dataOffset = headerLen + sizeof(pg_crc32);

	/* Compute CRC of the header. */
	INIT_CRC32C(crc);
	COMP_CRC32C(crc, headerPtr + dataOffset, overallBlockLen - dataOffset);

	/*
	 * By historical accident, the checksum calculated for append-only blocks
	 * is *not* inverted, like CRC-32C checksums usually are.
	 */
	/* FIN_CRC32C(crc); */

	return crc;
}

int32
AppendOnlyStorageFormat_RegularHeaderLenNeeded(
											   bool usingChecksum)
{
	return AoHeader_RegularSize +
		(usingChecksum ? 2 * sizeof(pg_crc32) : 0);
}

static void
AppendOnlyStorageFormat_AddFirstRowNum(
									   uint8 *headerPtr,
									   bool usingChecksums,
									   int64 firstRowNum)
{
	AOSmallContentHeader *blockHeader = (AOSmallContentHeader *) headerPtr;

	int32		offsetToFirstRowNum;
	int64	   *firstRowNumPtr;

	if (!AoHeader_IsLong(AOSmallContentHeaderGet_headerKind(blockHeader)))
	{
		offsetToFirstRowNum = AoHeader_RegularSize;
	}
	else
	{
		offsetToFirstRowNum = AoHeader_LongSize;
	}

	if (usingChecksums)
		offsetToFirstRowNum += 2 * sizeof(pg_crc32);
	/* Header and Block checksums. */

	firstRowNumPtr = (int64 *) &headerPtr[offsetToFirstRowNum];

	*firstRowNumPtr = firstRowNum;

	elogif(Debug_appendonly_print_storage_headers, LOG,
		   "Append-Only storage first row number header result: block_bytes_0_3 0x%X, block_bytes_4_7 0x%X, "
		   "firstRowNum " INT64_FORMAT,
		   blockHeader->smallcontent_bytes_0_3,
		   blockHeader->smallcontent_bytes_4_7,
		   firstRowNum);
}

static void
AppendOnlyStorageFormat_AddBlockHeaderChecksums(
												uint8 *headerPtr,
												bool isCompressed,
												bool hasFirstRowNum,
												int version,
												int32 dataLength,
												int32 compressedLength)
{
	AOSmallContentHeader *blockHeader = (AOSmallContentHeader *) headerPtr;

	AoHeaderKind headerKind;

	int32		firstHeaderLen;
	int32		firstHeaderAndBlockChecksumLen;
	int32		offset;
	int32		extHeaderOffset;
	int32		overallBlockLen;
	pg_crc32   *blockChecksumPtr;
	pg_crc32   *headerChecksumPtr;

	headerKind = AOSmallContentHeaderGet_headerKind(blockHeader);

	firstHeaderLen = AoHeader_RegularSize;
	firstHeaderAndBlockChecksumLen = firstHeaderLen + sizeof(pg_crc32);
	/* Block checksum. */

	offset = firstHeaderAndBlockChecksumLen +
		sizeof(pg_crc32);
	/* Header checksum. */
	if (AoHeader_IsLong(headerKind))
	{
		extHeaderOffset = offset;
		offset += AoHeader_RegularSize;
	}
	else
	{
		extHeaderOffset = 0;
	}
	if (hasFirstRowNum)
	{
		offset += sizeof(int64);
	}
	overallBlockLen = offset +
		AOStorage_RoundUp(
						  (isCompressed ? compressedLength : dataLength),
						  version);

	/*
	 * Calculate Block checksum first since it is included in the header
	 * checksum.
	 */
	blockChecksumPtr = (pg_crc32 *) &headerPtr[firstHeaderLen];
	*blockChecksumPtr = AppendOnlyStorageFormat_ComputeBlockChecksum(
																	 headerPtr,
																	 firstHeaderAndBlockChecksumLen,
																	 overallBlockLen);

	/*
	 * Now the Header checksum after the header and block checksum.
	 */
	headerChecksumPtr = (pg_crc32 *) &headerPtr[firstHeaderAndBlockChecksumLen];
	*headerChecksumPtr = AppendOnlyStorageFormat_ComputeHeaderChecksum(
																	   headerPtr,
																	   firstHeaderAndBlockChecksumLen);

	if (Debug_appendonly_print_storage_headers)
	{
		switch (headerKind)
		{
			case AoHeaderKind_SmallContent:
			case AoHeaderKind_LargeContent:
			case AoHeaderKind_NonBulkDenseContent:
				elog(LOG,
					 "Append-Only storage make with checksums block header result: block_bytes_0_3 0x%X, block_bytes_4_7 0x%X, "
					 "header checksum 0x%X, block checksum 0x%X, overallBlockLen %d",
					 blockHeader->smallcontent_bytes_0_3,
					 blockHeader->smallcontent_bytes_4_7,
					 *headerChecksumPtr,
					 *blockChecksumPtr,
					 overallBlockLen);
				break;

			case AoHeaderKind_BulkDenseContent:
				{
					AOBulkDenseContentHeader *bulkDenseHeader = (AOBulkDenseContentHeader *) headerPtr;
					AOBulkDenseContentHeaderExt *bulkDenseHeaderExt;

					bulkDenseHeaderExt =
						(AOBulkDenseContentHeaderExt *)
						(headerPtr + extHeaderOffset);

					elog(LOG,
						 "Append-Only storage make with checksums Bulk Dense Content header result: "
						 "bulkdensecontent_bytes_0_3 0x%X, bulkdensecontent_bytes_4_7 0x%X "
						 "bulkdensecontent_ext_bytes_0_3 0x%X, bulkdensecontent_ext_bytes_4_7 0x%X, "
						 "header checksum 0x%X, block checksum 0x%X, overallBlockLen %d",
						 bulkDenseHeader->bulkdensecontent_bytes_0_3,
						 bulkDenseHeader->bulkdensecontent_bytes_4_7,
						 bulkDenseHeaderExt->bulkdensecontent_ext_bytes_0_3,
						 bulkDenseHeaderExt->bulkdensecontent_ext_bytes_4_7,
						 *headerChecksumPtr,
						 *blockChecksumPtr,
						 overallBlockLen);
					break;
				}

			default:
				ereport(ERROR,
						(errmsg("Unexpected Append-Only header kind %d",
								headerKind)));
				break;
		}
	}
}

void
AppendOnlyStorageFormat_MakeSmallContentHeader(
											   uint8 *headerPtr,
											   bool usingChecksums,
											   bool hasFirstRowNum,
											   int version,
											   int64 firstRowNum,
											   int executorKind,
											   int rowCount,
											   int32 dataLength,
											   int32 compressedLength)
{
	AOSmallContentHeader *blockHeader;

	bool		isCompressed;

	Assert(headerPtr != NULL);

	blockHeader = (AOSmallContentHeader *) headerPtr;

	elogif(Debug_appendonly_print_storage_headers, LOG,
		   "Append-Only storage make Small Content header parameters: wantChecksum = %s, hasFirstRowNum %s, executorKind = %d, "
		   "rowCount = %d, dataLength %d, compressedLength %d",
		   (usingChecksums ? "true" : "false"),
		   (hasFirstRowNum ? "true" : "false"),
		   executorKind,
		   rowCount,
		   dataLength,
		   compressedLength);

	/* Zero out whole header */
	AOSmallContentHeaderInit_Init(blockHeader);

	AOSmallContentHeaderInit_headerKind(blockHeader, AoHeaderKind_SmallContent);
	AOSmallContentHeaderInit_executorBlockKind(blockHeader, executorKind);
	AOSmallContentHeaderInit_rowCount(blockHeader, rowCount);
	AOSmallContentHeaderInit_dataLength(blockHeader, dataLength);
	AOSmallContentHeaderInit_hasFirstRowNum(blockHeader, hasFirstRowNum);

	isCompressed = (compressedLength > 0);
	if (isCompressed)
		AOSmallContentHeaderInit_compressedLength(blockHeader, compressedLength);

	/*
	 * Add the optional firstRowNum.
	 *
	 * NOTE: This is not part of the 8-byte (64-bit) header because it is so
	 * big. NOTE: And, it is not covered by the header checksum because in
	 * order to NOTE: determine if we should checksum more data we would need
	 * to examine NOTE: the header data not verified by checksum yet...
	 *
	 * So, the firstRowNum is extra data between the header (and checksums)
	 * and the content.  We must add it before computing the checksum.
	 */
	if (hasFirstRowNum)
	{
		AppendOnlyStorageFormat_AddFirstRowNum(
											   headerPtr,
											   usingChecksums,
											   firstRowNum);
	}

	if (usingChecksums)
	{
		AppendOnlyStorageFormat_AddBlockHeaderChecksums(
														headerPtr,
														isCompressed,
														hasFirstRowNum,
														version,
														dataLength,
														compressedLength);
	}
	else
	{
		elogif(Debug_appendonly_print_storage_headers, LOG,
			   "Append-Only storage make Small Content header result: smallcontent_bytes_0_3 0x%X, smallcontent_bytes_4_7 0x%X",
			   blockHeader->smallcontent_bytes_0_3,
			   blockHeader->smallcontent_bytes_4_7);
	}

}

char *
AppendOnlyStorageFormat_SmallContentHeaderStr(
											  uint8 *headerPtr,
											  bool usingChecksums,
											  int version)
{
	AOSmallContentHeader *blockHeader;
	AoHeaderKind aoHeaderKind;

	int32		headerLen;
	int32		wholeHeaderLen;

	int			executorBlockKind;
	int			rowCount;
	int32		dataLength;
	int32		compressedLength;
	int32		overallBlockLen;

	pg_crc32   *blockChecksumPtr = NULL;
	pg_crc32	blockChecksum;
	pg_crc32   *headerChecksumPtr = NULL;
	pg_crc32	headerChecksum;

	StringInfoData buf;

	Assert(headerPtr != NULL);

	blockHeader = (AOSmallContentHeader *) headerPtr;
	aoHeaderKind = AOSmallContentHeaderGet_headerKind(blockHeader);

	headerLen = AoHeader_RegularSize;

	executorBlockKind = AOSmallContentHeaderGet_executorBlockKind(blockHeader);
	rowCount = AOSmallContentHeaderGet_rowCount(blockHeader);

	wholeHeaderLen = headerLen +
		(usingChecksums ? 2 * sizeof(pg_crc32) : 0);

	dataLength = AOSmallContentHeaderGet_dataLength(blockHeader);

	compressedLength = AOSmallContentHeaderGet_compressedLength(blockHeader);

	overallBlockLen = wholeHeaderLen +
		AOStorage_RoundUp(
						  (compressedLength == 0 ? dataLength : compressedLength),
						  version);

	if (usingChecksums)
	{
		blockChecksumPtr = (pg_crc32 *) &headerPtr[headerLen];
		blockChecksum = *blockChecksumPtr;

		headerLen += sizeof(pg_crc32);

		headerChecksumPtr = (pg_crc32 *) &headerPtr[headerLen];
		headerChecksum = *headerChecksumPtr;
	}
	else
	{
		blockChecksum = 0;
		headerChecksum = 0;
	}

	initStringInfo(&buf);
	appendStringInfo(
					 &buf,
					 "Append-Only storage Small Content header: "
					 "smallcontent_bytes_0_3 0x%X, smallcontent_bytes_4_7 0x%X, "
					 "headerKind = %d, "
					 "executorBlockKind = %d, "
					 "rowCount = %d, usingChecksums = %s, header checksum 0x%X, block checksum 0x%X, "
					 "dataLength %d, compressedLength %d, overallBlockLen %d",
					 blockHeader->smallcontent_bytes_0_3,
					 blockHeader->smallcontent_bytes_4_7,
					 aoHeaderKind,
					 executorBlockKind,
					 rowCount,
					 (usingChecksums ? "true" : "false"),
					 headerChecksum,
					 blockChecksum,
					 dataLength,
					 compressedLength,
					 overallBlockLen);

	return buf.data;
}

char *
AppendOnlyStorageFormat_LargeContentHeaderStr(
											  uint8 *headerPtr,
											  bool usingChecksums,
											  int version)
{
	AOLargeContentHeader *blockHeader;
	AoHeaderKind aoHeaderKind;

	int32		headerLen;
	int32		wholeHeaderLen;

	int			executorBlockKind;
	int			rowCount;
	int32		largeContentLength;
	int32		overallBlockLen;

	pg_crc32   *blockChecksumPtr = NULL;
	pg_crc32	blockChecksum;
	pg_crc32   *headerChecksumPtr = NULL;
	pg_crc32	headerChecksum;

	StringInfoData buf;

	Assert(headerPtr != NULL);

	blockHeader = (AOLargeContentHeader *) headerPtr;
	aoHeaderKind = AOLargeContentHeaderGet_headerKind(blockHeader);

	headerLen = AoHeader_RegularSize;

	executorBlockKind = AOLargeContentHeaderGet_executorBlockKind(blockHeader);
	rowCount = AOLargeContentHeaderGet_largeRowCount(blockHeader);

	wholeHeaderLen = headerLen +
		(usingChecksums ? 2 * sizeof(pg_crc32) : 0);

	largeContentLength = AOLargeContentHeaderGet_largeContentLength(blockHeader);

	overallBlockLen = wholeHeaderLen;
	/* No data with this kind of header. */

	if (usingChecksums)
	{
		blockChecksumPtr = (pg_crc32 *) &headerPtr[headerLen];
		blockChecksum = *blockChecksumPtr;

		headerLen += sizeof(pg_crc32);

		headerChecksumPtr = (pg_crc32 *) &headerPtr[headerLen];
		headerChecksum = *headerChecksumPtr;
	}
	else
	{
		blockChecksum = 0;
		headerChecksum = 0;
	}

	initStringInfo(&buf);
	appendStringInfo(
					 &buf,
					 "Append-Only storage Large Content header: "
					 "largecontent_bytes_0_3 0x%X, largecontent_bytes_4_7 0x%X, "
					 "headerKind = %d, "
					 "executorBlockKind = %d, "
					 "rowCount = %d, usingChecksums = %s, header checksum 0x%X, block checksum 0x%X, "
					 "largeContentLength %d, overallBlockLen %d",
					 blockHeader->largecontent_bytes_0_3,
					 blockHeader->largecontent_bytes_4_7,
					 aoHeaderKind,
					 executorBlockKind,
					 rowCount,
					 (usingChecksums ? "true" : "false"),
					 headerChecksum,
					 blockChecksum,
					 largeContentLength,
					 overallBlockLen);

	return buf.data;
}

char *
AppendOnlyStorageFormat_NonBulkDenseContentHeaderStr(
													 uint8 *headerPtr,
													 bool usingChecksums,
													 int version)
{
	AONonBulkDenseContentHeader *blockHeader;
	AoHeaderKind aoHeaderKind;

	int32		headerLen;
	int32		wholeHeaderLen;

	int			executorBlockKind;
	int			rowCount;
	int32		dataLength;
	int32		overallBlockLen;

	pg_crc32   *blockChecksumPtr = NULL;
	pg_crc32	blockChecksum;
	pg_crc32   *headerChecksumPtr = NULL;
	pg_crc32	headerChecksum;

	StringInfoData buf;

	Assert(headerPtr != NULL);

	blockHeader = (AONonBulkDenseContentHeader *) headerPtr;
	aoHeaderKind = AONonBulkDenseContentHeaderGet_headerKind(blockHeader);

	headerLen = AoHeader_RegularSize;

	executorBlockKind = AONonBulkDenseContentHeaderGet_executorBlockKind(blockHeader);
	rowCount = AONonBulkDenseContentHeaderGet_largeRowCount(blockHeader);

	wholeHeaderLen = headerLen +
		(usingChecksums ? 2 * sizeof(pg_crc32) : 0);

	dataLength = AONonBulkDenseContentHeaderGet_dataLength(blockHeader);

	overallBlockLen = wholeHeaderLen +
		AOStorage_RoundUp(
						  dataLength,
						  version);

	if (usingChecksums)
	{
		blockChecksumPtr = (pg_crc32 *) &headerPtr[headerLen];
		blockChecksum = *blockChecksumPtr;

		headerLen += sizeof(pg_crc32);

		headerChecksumPtr = (pg_crc32 *) &headerPtr[headerLen];
		headerChecksum = *headerChecksumPtr;
	}
	else
	{
		blockChecksum = 0;
		headerChecksum = 0;
	}

	initStringInfo(&buf);
	appendStringInfo(
					 &buf,
					 "Append-Only storage Large Content header: "
					 "nonbulkdensecontent_bytes_0_3 0x%X, nonbulkdensecontent_bytes_4_7 0x%X, "
					 "headerKind = %d, "
					 "executorBlockKind = %d, "
					 "rowCount = %d, usingChecksums = %s, header checksum 0x%X, block checksum 0x%X, "
					 "dataLength %d, overallBlockLen %d",
					 blockHeader->nonbulkdensecontent_bytes_0_3,
					 blockHeader->nonbulkdensecontent_bytes_4_7,
					 aoHeaderKind,
					 executorBlockKind,
					 rowCount,
					 (usingChecksums ? "true" : "false"),
					 headerChecksum,
					 blockChecksum,
					 dataLength,
					 overallBlockLen);

	return buf.data;
}

char *
AppendOnlyStorageFormat_BulkDenseContentHeaderStr(
												  uint8 *headerPtr,
												  bool usingChecksums,
												  int version)
{
	AOBulkDenseContentHeader *blockHeader;
	AoHeaderKind aoHeaderKind;

	AOBulkDenseContentHeaderExt *extHeader;
	int32		firstHeaderLen;
	int32		firstHeaderAndChecksumsLen;
	int32		extHeaderLen;

	int			executorBlockKind;
	int			rowCount;
	int32		dataLength;
	int32		compressedLength;
	int32		overallBlockLen;

	pg_crc32   *blockChecksumPtr = NULL;
	pg_crc32	blockChecksum;
	pg_crc32   *headerChecksumPtr = NULL;
	pg_crc32	headerChecksum;

	StringInfoData buf;

	Assert(headerPtr != NULL);

	blockHeader = (AOBulkDenseContentHeader *) headerPtr;
	aoHeaderKind = AOBulkDenseContentHeaderGet_headerKind(blockHeader);

	/*
	 * The first header and extension header are both is 64-bits.
	 */
	firstHeaderLen = AoHeader_RegularSize;
	extHeaderLen = AoHeader_RegularSize;

	executorBlockKind = AOBulkDenseContentHeaderGet_executorBlockKind(blockHeader);

	firstHeaderAndChecksumsLen =
		firstHeaderLen +
		(usingChecksums ? 2 * sizeof(pg_crc32) : 0);

	/*
	 * The extension header is in the data portion with first row number.
	 */
	extHeader = (AOBulkDenseContentHeaderExt *) (headerPtr + firstHeaderAndChecksumsLen);
	rowCount = AOBulkDenseContentHeaderExtGet_largeRowCount(extHeader);

	dataLength = AOBulkDenseContentHeaderGet_dataLength(blockHeader);

	compressedLength = AOBulkDenseContentHeaderGet_compressedLength(blockHeader);

	overallBlockLen = firstHeaderAndChecksumsLen +
		extHeaderLen +
		(AOBulkDenseContentHeaderGet_hasFirstRowNum(blockHeader) ? sizeof(int64) : 0) +
		AOStorage_RoundUp(
						  (compressedLength == 0 ? dataLength : compressedLength),
						  version);

	if (usingChecksums)
	{
		blockChecksumPtr = (pg_crc32 *) &headerPtr[firstHeaderLen];
		blockChecksum = *blockChecksumPtr;

		headerChecksumPtr = (pg_crc32 *) &headerPtr[firstHeaderLen + sizeof(pg_crc32)];
		headerChecksum = *headerChecksumPtr;
	}
	else
	{
		blockChecksum = 0;
		headerChecksum = 0;
	}

	initStringInfo(&buf);
	appendStringInfo(
					 &buf,
					 "Append-Only storage Bulk Dense Content header: "
					 "bulkdensecontent_bytes_0_3 0x%X, bulkdensecontent_bytes_4_7 0x%X, "
					 "bulkdensecontent_ext_bytes_0_3 0x%X, bulkdensecontent_ext_bytes_4_7 0x%X, "
					 "headerKind = %d, "
					 "executorBlockKind = %d, "
					 "rowCount = %d, usingChecksums = %s, header checksum 0x%X, block checksum 0x%X, "
					 "dataLength %d, compressedLength %d, overallBlockLen %d",
					 blockHeader->bulkdensecontent_bytes_0_3,
					 blockHeader->bulkdensecontent_bytes_4_7,
					 extHeader->bulkdensecontent_ext_bytes_0_3,
					 extHeader->bulkdensecontent_ext_bytes_4_7,
					 aoHeaderKind,
					 executorBlockKind,
					 rowCount,
					 (usingChecksums ? "true" : "false"),
					 headerChecksum,
					 blockChecksum,
					 dataLength,
					 compressedLength,
					 overallBlockLen);

	return buf.data;
}

char *
AppendOnlyStorageFormat_BlockHeaderStr(
									   uint8 *headerPtr,
									   bool usingChecksums,
									   int version)
{
	AOSmallContentHeader *blockHeader;

	AoHeaderKind aoHeaderKind;

	char	   *str;

	blockHeader = (AOSmallContentHeader *) headerPtr;
	aoHeaderKind = AOSmallContentHeaderGet_headerKind(blockHeader);
	switch (aoHeaderKind)
	{
		case AoHeaderKind_SmallContent:
			str = AppendOnlyStorageFormat_SmallContentHeaderStr(
																headerPtr,
																usingChecksums,
																version);
			break;

		case AoHeaderKind_LargeContent:
			str = AppendOnlyStorageFormat_LargeContentHeaderStr(
																headerPtr,
																usingChecksums,
																version);
			break;

		case AoHeaderKind_NonBulkDenseContent:
			str = AppendOnlyStorageFormat_NonBulkDenseContentHeaderStr(
																	   headerPtr,
																	   usingChecksums,
																	   version);
			break;

		case AoHeaderKind_BulkDenseContent:
			str = AppendOnlyStorageFormat_BulkDenseContentHeaderStr(
																	headerPtr,
																	usingChecksums,
																	version);
			break;

		default:
			{
				StringInfoData buf;

				initStringInfo(&buf);
				appendStringInfo(
								 &buf,
								 "Append-Only storage header kind %d unknown",
								 aoHeaderKind);

				str = buf.data;
			}
			break;
	}

	return str;
}

/*
 * errdetail_appendonly_storage_smallcontent_header
 *
 * Add an errdetail() line showing the Append-Only Storage block header.
 */
int
errdetail_appendonly_storage_smallcontent_header(
												 uint8 *headerPtr,
												 bool usingChecksums,
												 int version)
{
	char	   *str;

	str = AppendOnlyStorageFormat_SmallContentHeaderStr(
														headerPtr,
														usingChecksums,
														version);

	errdetail("%s", str);

	pfree(str);

	return 0;
}

/*
 * errdetail_appendonly_storage_largecontent_header
 *
 * Add an errdetail() line showing the Append-Only Storage block header.
 */
int
errdetail_appendonly_storage_largecontent_header(
												 uint8 *headerPtr,
												 bool usingChecksums,
												 int version)
{
	char	   *str;

	str = AppendOnlyStorageFormat_LargeContentHeaderStr(
														headerPtr,
														usingChecksums,
														version);

	errdetail("%s", str);

	pfree(str);

	return 0;
}


/*
 * errdetail_appendonly_storage_nonbulkdensecontent_header
 *
 * Add an errdetail() line showing the Append-Only Storage block header.
 */
int
errdetail_appendonly_storage_nonbulkdensecontent_header(
														uint8 *headerPtr,
														bool usingChecksums,
														int version)
{
	char	   *str;

	str = AppendOnlyStorageFormat_NonBulkDenseContentHeaderStr(
															   headerPtr,
															   usingChecksums,
															   version);

	errdetail("%s", str);

	pfree(str);

	return 0;
}

/*
 * errdetail_appendonly_storage_bulkdensecontent_header
 *
 * Add an errdetail() line showing the Append-Only Storage block header.
 */
int
errdetail_appendonly_storage_bulkdensecontent_header(
													 uint8 *headerPtr,
													 bool usingChecksums,
													 int version)
{
	char	   *str;

	str = AppendOnlyStorageFormat_BulkDenseContentHeaderStr(
															headerPtr,
															usingChecksums,
															version);

	errdetail("%s", str);

	pfree(str);

	return 0;
}

/*
 * errdetail_appendonly_storage_content_header
 *
 * Add an errdetail() line showing the Append-Only Storage content (Small, Large, Dense) header.
 */
int
errdetail_appendonly_storage_content_header(
											uint8 *header,
											bool usingChecksum,
											int version)
{
	AOSmallContentHeader *blockHeader;

	AoHeaderKind aoHeaderKind;

	blockHeader = (AOSmallContentHeader *) header;
	aoHeaderKind = AOSmallContentHeaderGet_headerKind(blockHeader);
	switch (aoHeaderKind)
	{
		case AoHeaderKind_SmallContent:
			return errdetail_appendonly_storage_smallcontent_header(header, usingChecksum, version);

		case AoHeaderKind_LargeContent:
			return errdetail_appendonly_storage_largecontent_header(header, usingChecksum, version);

		case AoHeaderKind_NonBulkDenseContent:
			return errdetail_appendonly_storage_nonbulkdensecontent_header(header, usingChecksum, version);

		case AoHeaderKind_BulkDenseContent:
			return errdetail_appendonly_storage_bulkdensecontent_header(header, usingChecksum, version);

		default:
			return errdetail(
							 "Append-Only storage header kind %d unknown",
							 aoHeaderKind);
	}
}


void
AppendOnlyStorageFormat_MakeLargeContentHeader(
											   uint8 *headerPtr,
											   bool usingChecksums,
											   bool hasFirstRowNum,
											   int version,
											   int64 firstRowNum,
											   int executorKind,
											   int largeRowCount,
											   int32 largeContentLength)
{
	AOLargeContentHeader *largeContentHeader;

	Assert(headerPtr != NULL);

	largeContentHeader = (AOLargeContentHeader *) headerPtr;

	elogif(Debug_appendonly_print_storage_headers, LOG,
		   "Append-Only Storage make LargeContent header parameters: usingChecksums = %s, executorKind = %d, "
		   "largeRowCount = %d, largeContentLength %d",
		   (usingChecksums ? "true" : "false"),
		   executorKind,
		   largeRowCount,
		   largeContentLength);

	/* Zero out whole header */
	AOLargeContentHeaderInit_Init(largeContentHeader);

	AOLargeContentHeaderInit_headerKind(largeContentHeader, AoHeaderKind_LargeContent);
	AOLargeContentHeaderInit_executorBlockKind(largeContentHeader, executorKind);
	AOLargeContentHeaderInit_largeRowCount(largeContentHeader, largeRowCount);
	AOLargeContentHeaderInit_largeContentLength(largeContentHeader, largeContentLength);
	AOLargeContentHeaderInit_hasFirstRowNum(largeContentHeader, hasFirstRowNum);

	/*
	 * Add the optional firstRowNum.
	 *
	 * NOTE: This is not part of the 8-byte (64-bit) header because it is so
	 * big. NOTE: And, it is not covered by the header checksum because in
	 * order to NOTE: determine if we should checksum more data we would need
	 * to examine NOTE: the header data not verified by checksum yet...
	 *
	 * So, the firstRowNum is extra data between the header (and checksums)
	 * and the content.  We must add it before computing the checksum.
	 */
	if (hasFirstRowNum)
	{
		AppendOnlyStorageFormat_AddFirstRowNum(
											   headerPtr,
											   usingChecksums,
											   firstRowNum);
	}

	if (usingChecksums)
	{
		/* UNDONE: Set 2nd checksum to 0 when there is no content??? */
		AppendOnlyStorageFormat_AddBlockHeaderChecksums(
														headerPtr,
														 /* isCompressed */ false,
														 /* hasFirstRowNum */ false,
														version,
														 /* dataLength */ 0,
														 /* compressedLength */ 0);
	}
	else
	{
		elogif(Debug_appendonly_print_storage_headers, LOG,
			   "Append-Only storage make block header result: block_bytes_0_3 0x%X, block_bytes_4_7 0x%X",
			   largeContentHeader->largecontent_bytes_0_3,
			   largeContentHeader->largecontent_bytes_4_7);
	}
}

void
AppendOnlyStorageFormat_MakeNonBulkDenseContentHeader(
													  uint8 *headerPtr,
													  bool usingChecksums,
													  bool hasFirstRowNum,
													  int version,
													  int64 firstRowNum,
													  int executorKind,
													  int rowCount,
													  int32 dataLength)
{
	AONonBulkDenseContentHeader *blockHeader;

	Assert(headerPtr != NULL);

	blockHeader = (AONonBulkDenseContentHeader *) headerPtr;

	elogif(Debug_appendonly_print_storage_headers, LOG,
		   "Append-Only storage make Non-Bulk Dense Content header parameters: wantChecksum = %s, hasFirstRowNum %s, executorKind = %d, "
		   "rowCount = %d, dataLength %d",
		   (usingChecksums ? "true" : "false"),
		   (hasFirstRowNum ? "true" : "false"),
		   executorKind,
		   rowCount,
		   dataLength);

	/* Zero out whole header */
	AONonBulkDenseContentHeaderInit_Init(blockHeader);

	AONonBulkDenseContentHeaderInit_headerKind(blockHeader, AoHeaderKind_NonBulkDenseContent);
	AONonBulkDenseContentHeaderInit_executorBlockKind(blockHeader, executorKind);
	AONonBulkDenseContentHeaderInit_largeRowCount(blockHeader, rowCount);
	AONonBulkDenseContentHeaderInit_dataLength(blockHeader, dataLength);
	AONonBulkDenseContentHeaderInit_hasFirstRowNum(blockHeader, hasFirstRowNum);

	/*
	 * Add the optional firstRowNum.
	 *
	 * NOTE: This is not part of the 8-byte (64-bit) header because it is so
	 * big. And, it is not covered by the header checksum because in order to
	 * determine if we should checksum more data we would need to examine the
	 * header data not verified by checksum yet.
	 *
	 * So, the firstRowNum is extra data between the header (and checksums)
	 * and the content.  We must add it before computing the checksum.
	 */
	if (hasFirstRowNum)
	{
		AppendOnlyStorageFormat_AddFirstRowNum(
											   headerPtr,
											   usingChecksums,
											   firstRowNum);
	}

	if (usingChecksums)
	{
		AppendOnlyStorageFormat_AddBlockHeaderChecksums(
														headerPtr,
														 /* isCompressed */ false,
														hasFirstRowNum,
														version,
														dataLength,
														 /* compressedLength */ 0);
	}
	else
	{
		elogif(Debug_appendonly_print_storage_headers, LOG,
			   "Append-Only storage make Bulk Dense Content header result: nonbulkdensecontent_bytes_0_3 0x%X, nonbulkdensecontent_bytes_4_7 0x%X",
			   blockHeader->nonbulkdensecontent_bytes_0_3,
			   blockHeader->nonbulkdensecontent_bytes_4_7);
	}

}


void
AppendOnlyStorageFormat_MakeBulkDenseContentHeader(
												   uint8 *headerPtr,
												   bool usingChecksums,
												   bool hasFirstRowNum,
												   int version,
												   int64 firstRowNum,
												   int executorKind,
												   int rowCount,
												   int32 dataLength,
												   int32 compressedLength)
{
	AOBulkDenseContentHeader *blockHeader;
	int32		firstHeaderAndChecksumsLen;
	AOBulkDenseContentHeaderExt *extHeader;

	bool		isCompressed;

	Assert(headerPtr != NULL);

	blockHeader = (AOBulkDenseContentHeader *) headerPtr;
	firstHeaderAndChecksumsLen =
		AoHeader_RegularSize +
		(usingChecksums ? 2 * sizeof(pg_crc32) : 0);

	/*
	 * The extension header is in the data portion with first row number.
	 */
	extHeader = (AOBulkDenseContentHeaderExt *) (headerPtr + firstHeaderAndChecksumsLen);

	elogif(Debug_appendonly_print_storage_headers, LOG,
		   "Append-Only storage make Bulk Dense Content header parameters: wantChecksum = %s, hasFirstRowNum %s, executorKind = %d, "
		   "rowCount = %d, dataLength %d, compressedLength %d",
		   (usingChecksums ? "true" : "false"),
		   (hasFirstRowNum ? "true" : "false"),
		   executorKind,
		   rowCount,
		   dataLength,
		   compressedLength);

	/* Zero out whole header */
	AOBulkDenseContentHeaderInit_Init(blockHeader);

	AOBulkDenseContentHeaderInit_headerKind(blockHeader, AoHeaderKind_BulkDenseContent);
	AOBulkDenseContentHeaderInit_executorBlockKind(blockHeader, executorKind);
	AOBulkDenseContentHeaderInit_dataLength(blockHeader, dataLength);
	AOBulkDenseContentHeaderInit_hasFirstRowNum(blockHeader, hasFirstRowNum);

	isCompressed = (compressedLength > 0);
	if (isCompressed)
		AOBulkDenseContentHeaderInit_compressedLength(blockHeader, compressedLength);

	/* Zero out whole extension */
	AOBulkDenseContentHeaderExtInit_Init(extHeader);
	AOBulkDenseContentHeaderExtInit_largeRowCount(extHeader, rowCount);

	/*
	 * Add the optional firstRowNum.
	 *
	 * NOTE: This is not part of the 8-byte (64-bit) header because it is so
	 * big. And, it is not covered by the header checksum because in order to
	 * determine if we should checksum more data we would need to examine the
	 * header data not verified by checksum yet.
	 *
	 * So, the firstRowNum is extra data between the header (and checksums)
	 * and the content.  We must add it before computing the checksum.
	 */
	if (hasFirstRowNum)
	{
		AppendOnlyStorageFormat_AddFirstRowNum(
											   headerPtr,
											   usingChecksums,
											   firstRowNum);
	}

	if (usingChecksums)
	{
		AppendOnlyStorageFormat_AddBlockHeaderChecksums(
														headerPtr,
														isCompressed,
														hasFirstRowNum,
														version,
														dataLength,
														compressedLength);
	}
	else
	{
		elogif(Debug_appendonly_print_storage_headers, LOG,
			   "Append-Only storage make Bulk Dense Content header result: "
			   "bulkdensecontent_bytes_0_3 0x%X, bulkdensecontent_bytes_4_7 0x%X "
			   "bulkdensecontent_ext_bytes_0_3 0x%X, bulkdensecontent_ext_bytes_4_7 0x%X ",
			   blockHeader->bulkdensecontent_bytes_0_3,
			   blockHeader->bulkdensecontent_bytes_4_7,
			   extHeader->bulkdensecontent_ext_bytes_0_3,
			   extHeader->bulkdensecontent_ext_bytes_4_7);
	}

#ifdef USE_ASSERT_CHECKING
	{
		int			checkHeaderLen;
		int32		checkLength;
		int32		checkBlockLimitLen;
		int32		checkOverallBlockLen;
		int32		checkOffset;
		int32		checkUncompressedLen;
		int			checkExecutorBlockKind;
		bool		checkHasFirstRowNum;
		int64		checkFirstRowNum;
		int			checkRowCount;
		bool		checkIsCompressed;
		int32		checkCompressedLen;

		AOHeaderCheckError checkError;

		checkHeaderLen = firstHeaderAndChecksumsLen +
			AoHeader_RegularSize;
		if (hasFirstRowNum)
			checkHeaderLen += sizeof(int64);

		if (compressedLength == 0)
		{
			checkLength = dataLength;
		}
		else
		{
			checkLength = compressedLength;
		}
		checkBlockLimitLen = checkHeaderLen +
			AOStorage_RoundUp(checkLength, version);

		checkError =
			AppendOnlyStorageFormat_GetBulkDenseContentHeaderInfo(
																  headerPtr,
																  checkHeaderLen,
																  usingChecksums,
																  checkBlockLimitLen,
																  &checkOverallBlockLen,
																  &checkOffset,
																  &checkUncompressedLen,
																  &checkExecutorBlockKind,
																  &checkHasFirstRowNum,
																  version,
																  &checkFirstRowNum,
																  &checkRowCount,
																  &checkIsCompressed,
																  &checkCompressedLen);
		if (checkError != AOHeaderCheckOk)
			ereport(ERROR,
					(errmsg("Problem making append-only storage header of type bulk dense content. Header check error %d, detail '%s'",
							(int) checkError,
							AppendOnlyStorageFormat_GetHeaderCheckErrorStr())));

		if (checkOverallBlockLen != checkBlockLimitLen)
			ereport(ERROR,
					(errmsg("Problem making append-only storage header of type bulk dense content. Found block length %d, expected %d",
							checkOverallBlockLen,
							checkBlockLimitLen)));
		if (checkOffset != checkHeaderLen)
			ereport(ERROR,
					(errmsg("Problem making append-only storage header of type bulk dense content. Found data offset %d, expected %d",
							checkOffset,
							checkHeaderLen)));
		if (checkUncompressedLen != dataLength)
			ereport(ERROR,
					(errmsg("Problem making append-only storage header of type bulk dense content. Found uncompressed length %d, expected %d",
							checkUncompressedLen,
							dataLength)));
		if (checkExecutorBlockKind != executorKind)
			ereport(ERROR,
					(errmsg("Problem making append-only storage header of type bulk dense content. Found executor kind %d, expected %d",
							checkExecutorBlockKind,
							executorKind)));
		if (checkHasFirstRowNum != hasFirstRowNum)
			ereport(ERROR,
					(errmsg("Problem making append-only storage header of type bulk dense content. Found has first row number flag %s, expected %s",
							(checkHasFirstRowNum ? "true" : "false"),
							(hasFirstRowNum ? "true" : "false"))));
		if (hasFirstRowNum)
		{
			if (checkFirstRowNum != firstRowNum)
				ereport(ERROR,
						(errmsg("Problem making append-only storage header of type bulk dense content. "
								"Found first row number " INT64_FORMAT ", expected " INT64_FORMAT,
								checkFirstRowNum,
								firstRowNum)));
		}
		if (checkRowCount != rowCount)
			ereport(ERROR,
					(errmsg("Problem making append-only storage header of type bulk dense content. Found row count %d, expected %d",
							checkRowCount,
							rowCount)));
		if (checkIsCompressed != isCompressed)
			ereport(ERROR,
					(errmsg("Problem making append-only storage header of type bulk dense content. Found is compressed flag %s, expected %s",
							(checkIsCompressed ? "true" : "false"),
							(isCompressed ? "true" : "false"))));
		if (checkCompressedLen != compressedLength)
			ereport(ERROR,
					(errmsg("Problem making append-only storage header of type bulk dense content. Found data length %d, expected %d",
							checkCompressedLen,
							dataLength)));
	}
#endif
}

#define MAX_AOHEADER_CHECK_ERROR_STR 300
static char AoHeaderCheckErrorStr[MAX_AOHEADER_CHECK_ERROR_STR] = "\0";

/*
 * Return a string message for the last check error.
 */
char *
AppendOnlyStorageFormat_GetHeaderCheckErrorStr(void)
{
	return AoHeaderCheckErrorStr;
}

AOHeaderCheckError
AppendOnlyStorageFormat_GetHeaderInfo(
									  uint8 *headerPtr,
									  bool usingChecksums,
									  AoHeaderKind *headerKind,
									  int32 *actualHeaderLen)
{
	AOHeader   *header;
	int			snprintfResult;

	Assert(headerPtr != NULL);
	Assert(headerKind != NULL);

	header = (AOHeader *) headerPtr;

	if (header->header_bytes_0_3 == 0)
	{
		snprintfResult =
			snprintf(
					 AoHeaderCheckErrorStr,
					 MAX_AOHEADER_CHECK_ERROR_STR,
					 "Append-only storage header is invalid -- first 32 bits are all zeroes (header_bytes_0_3 0x%08x, header_bytes_4_7 0x%08x)",
					 header->header_bytes_0_3, header->header_bytes_4_7);

		Assert(snprintfResult >= 0);
		Assert(snprintfResult < MAX_AOHEADER_CHECK_ERROR_STR);

		return AOHeaderCheckFirst32BitsAllZeroes;
	}

	if (AOHeaderGet_reserved0(header) != 0)
	{
		snprintfResult =
			snprintf(
					 AoHeaderCheckErrorStr,
					 MAX_AOHEADER_CHECK_ERROR_STR,
					 "Append-only storage header is invalid -- reserved bit 0 of the header is not zero (header_bytes_0_3 0x%08x, header_bytes_4_7 0x%08x)",
					 header->header_bytes_0_3, header->header_bytes_4_7);

		Assert(snprintfResult >= 0);
		Assert(snprintfResult < MAX_AOHEADER_CHECK_ERROR_STR);

		return AOHeaderCheckReservedBit0Not0;
	}

	*headerKind = AOHeaderGet_headerKind(header);

	if (*headerKind == AoHeaderKind_None)
	{
		snprintfResult =
			snprintf(
					 AoHeaderCheckErrorStr,
					 MAX_AOHEADER_CHECK_ERROR_STR,
					 "Append-only storage header is invalid -- invalid value 0 (none) for header kind (header_bytes_0_3 0x%08x, header_bytes_4_7 0x%08x)",
					 header->header_bytes_0_3, header->header_bytes_4_7);

		Assert(snprintfResult >= 0);
		Assert(snprintfResult < MAX_AOHEADER_CHECK_ERROR_STR);

		return AOHeaderCheckInvalidHeaderKindNone;
	}

	if (*headerKind >= MaxAoHeaderKind)
	{
		snprintfResult =
			snprintf(
					 AoHeaderCheckErrorStr,
					 MAX_AOHEADER_CHECK_ERROR_STR,
					 "Append-only storage header is invalid -- invalid header kind value %d (header_bytes_0_3 0x%08x, header_bytes_4_7 0x%08x)",
					 (int) *headerKind,
					 header->header_bytes_0_3, header->header_bytes_4_7);

		Assert(snprintfResult >= 0);
		Assert(snprintfResult < MAX_AOHEADER_CHECK_ERROR_STR);

		return AOHeaderCheckInvalidHeaderKind;
	}

	switch (*headerKind)
	{
		case AoHeaderKind_SmallContent:
			{
				AOSmallContentHeader *blockHeader;

				blockHeader = (AOSmallContentHeader *) headerPtr;

				*actualHeaderLen =
					AoHeader_RegularSize +
					(usingChecksums ? 2 * sizeof(pg_crc32) : 0);
				if (AOSmallContentHeaderGet_hasFirstRowNum(blockHeader))
					(*actualHeaderLen) += sizeof(int64);
			}
			break;

		case AoHeaderKind_LargeContent:
			{
				AOLargeContentHeader *largeContentHeader;

				largeContentHeader = (AOLargeContentHeader *) headerPtr;

				*actualHeaderLen =
					AoHeader_RegularSize +
					(usingChecksums ? 2 * sizeof(pg_crc32) : 0);
				if (AOLargeContentHeaderGet_hasFirstRowNum(largeContentHeader))
					(*actualHeaderLen) += sizeof(int64);
			}
			break;

		case AoHeaderKind_NonBulkDenseContent:
			{
				AONonBulkDenseContentHeader *denseContentHeader;

				denseContentHeader = (AONonBulkDenseContentHeader *) headerPtr;

				*actualHeaderLen =
					AoHeader_RegularSize +
					(usingChecksums ? 2 * sizeof(pg_crc32) : 0);
				if (AONonBulkDenseContentHeaderGet_hasFirstRowNum(denseContentHeader))
					(*actualHeaderLen) += sizeof(int64);
			}
			break;

		case AoHeaderKind_BulkDenseContent:
			{
				AOBulkDenseContentHeader *blockHeader;

				blockHeader = (AOBulkDenseContentHeader *) headerPtr;

				*actualHeaderLen =
					AoHeader_LongSize +
					(usingChecksums ? 2 * sizeof(pg_crc32) : 0);
				if (AOBulkDenseContentHeaderGet_hasFirstRowNum(blockHeader))
					(*actualHeaderLen) += sizeof(int64);
			}
			break;

		default:
			elog(ERROR, "Unexpected Append-Only header kind %d",
				 *headerKind);
			break;
	}

	return AOHeaderCheckOk;
}

AOHeaderCheckError
AppendOnlyStorageFormat_GetSmallContentHeaderInfo(
												  uint8 *headerPtr,
												  int headerLen,
												  bool usingChecksums,
												  int32 blockLimitLen,
												  int32 *overallBlockLen,
												  int32 *offset,
												  int32 *uncompressedLen,
												  int *executorBlockKind,
												  bool *hasFirstRowNum,
												  int version,
												  int64 *firstRowNum,
												  int *rowCount,
												  bool *isCompressed,
												  int32 *compressedLen)
{
	AOSmallContentHeader *blockHeader;
	int32		length;

	Assert(headerPtr != NULL);

	blockHeader = (AOSmallContentHeader *) headerPtr;

	*executorBlockKind = AOSmallContentHeaderGet_executorBlockKind(blockHeader);
	*hasFirstRowNum = AOSmallContentHeaderGet_hasFirstRowNum(blockHeader);
	*rowCount = AOSmallContentHeaderGet_rowCount(blockHeader);

	*offset = AoHeader_RegularSize +
		(usingChecksums ? 2 * sizeof(pg_crc32) : 0);
	if (*hasFirstRowNum)
	{
		int64	   *firstRowNumPtr;

		firstRowNumPtr = (int64 *) &headerPtr[*offset];
		*firstRowNum = *firstRowNumPtr;

		(*offset) += sizeof(int64);
	}
	else
		*firstRowNum = INT64CONST(-1);

	if (*offset != headerLen)
		elog(ERROR, "Content offset %d doesn't equal header length parameter %d",
			 *offset,
			 headerLen);

	*uncompressedLen = AOSmallContentHeaderGet_dataLength(blockHeader);
	*compressedLen = AOSmallContentHeaderGet_compressedLength(blockHeader);
	if (*compressedLen == 0)
	{
		*isCompressed = false;
		length = *uncompressedLen;
	}
	else
	{
		*isCompressed = true;
		length = *compressedLen;

		/*
		 * UNDONE: Fix doCompressAppend to supply slightly less output
		 * buffer... UNDONE: so we can make this comparison >=.
		 */
		if (*compressedLen > *uncompressedLen)
		{
			sprintf(AoHeaderCheckErrorStr,
					"Append-only storage header is invalid -- compressed length %d is > uncompressed length %d "
					"(smallcontent_bytes_0_3 0x%08x, smallcontent_bytes_4_7 0x%08x)",
					*compressedLen,
					*uncompressedLen,
					blockHeader->smallcontent_bytes_0_3, blockHeader->smallcontent_bytes_4_7);
			return AOHeaderCheckInvalidCompressedLen;
		}
	}

	*overallBlockLen = *offset +
		AOStorage_RoundUp(length, version);

	if (*overallBlockLen > blockLimitLen)
	{
		sprintf(AoHeaderCheckErrorStr,
				"Append-only storage header is invalid -- overall block length %d is > block limit length %d "
				"(smallcontent_bytes_0_3 0x%08x, smallcontent_bytes_4_7 0x%08x)",
				*overallBlockLen,
				blockLimitLen,
				blockHeader->smallcontent_bytes_0_3, blockHeader->smallcontent_bytes_4_7);
		return AOHeaderCheckInvalidOverallBlockLen;
	}

	return AOHeaderCheckOk;
}

int32
AppendOnlyStorageFormat_GetCompressedLen(
										 uint8 *headerPtr)
{
	AOSmallContentHeader *blockHeader;

	Assert(headerPtr != NULL);
	blockHeader = (AOSmallContentHeader *) headerPtr;
	Assert(blockHeader->smallcontent_bytes_0_3 != 0);

	return AOSmallContentHeaderGet_compressedLength(blockHeader);
}

AOHeaderCheckError
AppendOnlyStorageFormat_GetLargeContentHeaderInfo(
												  uint8 *headerPtr,
												  int headerLen,
												  bool usingChecksums,
												  int32 *largeContentLen,
												  int *executorBlockKind,
												  bool *hasFirstRowNum,
												  int64 *firstRowNum,
												  int *largeRowCount)
{
	AOLargeContentHeader *largeContentHeader;
	int32		offset;

	Assert(headerPtr != NULL);

	largeContentHeader = (AOLargeContentHeader *) headerPtr;

	*executorBlockKind = AOLargeContentHeaderGet_executorBlockKind(largeContentHeader);
	*hasFirstRowNum = AOLargeContentHeaderGet_hasFirstRowNum(largeContentHeader);
	*largeRowCount = AOLargeContentHeaderGet_largeRowCount(largeContentHeader);
	*largeContentLen = AOLargeContentHeaderGet_largeContentLength(largeContentHeader);
	if (*largeContentLen == 0)
	{
		sprintf(AoHeaderCheckErrorStr,
				"Append-only storage header is invalid -- large content length is zero "
				"(block_bytes_0_3 0x%08x, block_bytes_4_7 0x%08x)",
				largeContentHeader->largecontent_bytes_0_3, largeContentHeader->largecontent_bytes_4_7);
		return AOHeaderCheckLargeContentLenIsZero;
	}

	offset = AoHeader_RegularSize +
		(usingChecksums ? 2 * sizeof(pg_crc32) : 0);
	if (*hasFirstRowNum)
	{
		int64	   *firstRowNumPtr;

		firstRowNumPtr = (int64 *) &headerPtr[offset];
		*firstRowNum = *firstRowNumPtr;

		offset += sizeof(int64);
	}
	else
		*firstRowNum = INT64CONST(-1);

	if (offset != headerLen)
		elog(ERROR, "Content offset %d doesn't equal header length parameter %d",
			 offset,
			 headerLen);

	return AOHeaderCheckOk;
}

AOHeaderCheckError
AppendOnlyStorageFormat_GetNonBulkDenseContentHeaderInfo(
														 uint8 *headerPtr,
														 int headerLen,
														 bool usingChecksums,
														 int32 blockLimitLen,
														 int32 *overallBlockLen,
														 int32 *offset,
														 int32 *uncompressedLen,
														 int *executorBlockKind,
														 bool *hasFirstRowNum,
														 int version,
														 int64 *firstRowNum,
														 int *rowCount)
{
	AONonBulkDenseContentHeader *blockHeader;

	Assert(headerPtr != NULL);

	blockHeader = (AONonBulkDenseContentHeader *) headerPtr;

	*executorBlockKind = AONonBulkDenseContentHeaderGet_executorBlockKind(blockHeader);
	*hasFirstRowNum = AONonBulkDenseContentHeaderGet_hasFirstRowNum(blockHeader);
	*rowCount = AONonBulkDenseContentHeaderGet_largeRowCount(blockHeader);

	*offset = AoHeader_RegularSize +
		(usingChecksums ? 2 * sizeof(pg_crc32) : 0);
	if (*hasFirstRowNum)
	{
		int64	   *firstRowNumPtr;

		firstRowNumPtr = (int64 *) &headerPtr[*offset];
		*firstRowNum = *firstRowNumPtr;

		(*offset) += sizeof(int64);
	}
	else
		*firstRowNum = INT64CONST(-1);

	if (*offset != headerLen)
		elog(ERROR, "Content offset %d doesn't equal header length parameter %d",
			 *offset,
			 headerLen);

	*uncompressedLen = AONonBulkDenseContentHeaderGet_dataLength(blockHeader);

	*overallBlockLen = *offset +
		AOStorage_RoundUp(*uncompressedLen, version);

	if (*overallBlockLen > blockLimitLen)
	{
		sprintf(AoHeaderCheckErrorStr,
				"Append-only storage header is invalid -- overall block length %d is > block limit length %d "
				"(nonbulkdensecontent_bytes_0_3 0x%08x, nonbulkdensecontent_bytes_4_7 0x%08x)",
				*overallBlockLen,
				blockLimitLen,
				blockHeader->nonbulkdensecontent_bytes_0_3, blockHeader->nonbulkdensecontent_bytes_4_7);
		return AOHeaderCheckInvalidOverallBlockLen;
	}

	return AOHeaderCheckOk;
}

AOHeaderCheckError
AppendOnlyStorageFormat_GetBulkDenseContentHeaderInfo(
													  uint8 *headerPtr,
													  int headerLen,
													  bool usingChecksums,
													  int32 blockLimitLen,
													  int32 *overallBlockLen,
													  int32 *offset,
													  int32 *uncompressedLen,
													  int *executorBlockKind,
													  bool *hasFirstRowNum,
													  int version,
													  int64 *firstRowNum,
													  int *rowCount,
													  bool *isCompressed,
													  int32 *compressedLen)
{
	AOBulkDenseContentHeader *blockHeader;
	int32		firstHeaderAndChecksumsLen;
	AOBulkDenseContentHeaderExt *extHeader;

	int32		length;

	Assert(headerPtr != NULL);

	blockHeader = (AOBulkDenseContentHeader *) headerPtr;
	firstHeaderAndChecksumsLen =
		AoHeader_RegularSize +
		(usingChecksums ? 2 * sizeof(pg_crc32) : 0);

	/*
	 * The extension header is in the data portion with first row number.
	 */
	extHeader = (AOBulkDenseContentHeaderExt *) (headerPtr + firstHeaderAndChecksumsLen);
	*offset = firstHeaderAndChecksumsLen +
		AoHeader_RegularSize;

	*executorBlockKind = AOBulkDenseContentHeaderGet_executorBlockKind(blockHeader);
	*hasFirstRowNum = AOBulkDenseContentHeaderGet_hasFirstRowNum(blockHeader);

	*rowCount = AOBulkDenseContentHeaderExtGet_largeRowCount(extHeader);

	if (*hasFirstRowNum)
	{
		int64	   *firstRowNumPtr;

		firstRowNumPtr = (int64 *) &headerPtr[*offset];
		*firstRowNum = *firstRowNumPtr;

		(*offset) += sizeof(int64);
	}
	else
		*firstRowNum = INT64CONST(-1);

	if (*offset != headerLen)
		elog(ERROR, "Content offset %d doesn't equal header length parameter %d",
			 *offset,
			 headerLen);

	*uncompressedLen = AOBulkDenseContentHeaderGet_dataLength(blockHeader);
	*compressedLen = AOBulkDenseContentHeaderGet_compressedLength(blockHeader);
	if (*compressedLen == 0)
	{
		*isCompressed = false;
		length = *uncompressedLen;
	}
	else
	{
		*isCompressed = true;
		length = *compressedLen;

		/*
		 * UNDONE: Fix doCompressAppend to supply slightly less output
		 * buffer... UNDONE: so we can make this comparison >=.
		 */
		if (*compressedLen > *uncompressedLen)
		{
			sprintf(AoHeaderCheckErrorStr,
					"Append-only storage header is invalid -- compressed length %d is > uncompressed length %d "
					"(bulkdensecontent_bytes_0_3 0x%08x, bulkdensecontent_bytes_4_7 0x%08x, "
					"bulkdensecontent_ext_bytes_0_3 0x%08x, bulkdensecontent_ext_bytes_4_7 0x%08x)",
					*compressedLen,
					*uncompressedLen,
					blockHeader->bulkdensecontent_bytes_0_3, blockHeader->bulkdensecontent_bytes_4_7,
					extHeader->bulkdensecontent_ext_bytes_0_3, extHeader->bulkdensecontent_ext_bytes_4_7);
			return AOHeaderCheckInvalidCompressedLen;
		}
	}

	*overallBlockLen = *offset +
		AOStorage_RoundUp(length, version);

	if (*overallBlockLen > blockLimitLen)
	{
		sprintf(AoHeaderCheckErrorStr,
				"Append-only storage header is invalid -- overall block length %d is > block limit length %d "
				"(bulkdensecontent_bytes_0_3 0x%08x, bulkdensecontent_bytes_4_7 0x%08x, "
				"bulkdensecontent_ext_bytes_0_3 0x%08x, bulkdensecontent_ext_bytes_4_7 0x%08x)",
				*overallBlockLen,
				blockLimitLen,
				blockHeader->bulkdensecontent_bytes_0_3, blockHeader->bulkdensecontent_bytes_4_7,
				extHeader->bulkdensecontent_ext_bytes_0_3, extHeader->bulkdensecontent_ext_bytes_4_7);
		return AOHeaderCheckInvalidOverallBlockLen;
	}

	return AOHeaderCheckOk;
}

bool
AppendOnlyStorageFormat_VerifyHeaderChecksum(
											 uint8 *headerPtr,
											 pg_crc32 *storedChecksum,
											 pg_crc32 *computedChecksum)
{
	int32		firstHeaderLen;
	int32		firstHeaderAndBlockChecksumLen;

	pg_crc32   *headerChecksumPtr;

	Assert(headerPtr != NULL);
	Assert(storedChecksum != NULL);
	Assert(computedChecksum != NULL);

	firstHeaderLen = AoHeader_RegularSize;
	firstHeaderAndBlockChecksumLen = firstHeaderLen + sizeof(pg_crc32);
	/* Block checksum. */

	/*
	 * CRC checksum is first 32 bits after the whole header.
	 */
	headerChecksumPtr = (pg_crc32 *) &headerPtr[firstHeaderAndBlockChecksumLen];
	*storedChecksum = *headerChecksumPtr;

	*computedChecksum = AppendOnlyStorageFormat_ComputeHeaderChecksum(
																	  headerPtr,
																	  firstHeaderAndBlockChecksumLen);

	return (*storedChecksum == *computedChecksum);
}

bool
AppendOnlyStorageFormat_VerifyBlockChecksum(
											uint8 *headerPtr,
											int32 overallBlockLen,
											pg_crc32 *storedChecksum,
											pg_crc32 *computedChecksum)
{
	int32		firstHeaderLen;

	pg_crc32   *blockChecksumPtr;

	Assert(headerPtr != NULL);
	Assert(storedChecksum != NULL);
	Assert(computedChecksum != NULL);

	firstHeaderLen = AoHeader_RegularSize;

	/*
	 * Block checksum is first 32 bits after header.
	 */
	blockChecksumPtr = (pg_crc32 *) &headerPtr[firstHeaderLen];
	*storedChecksum = *blockChecksumPtr;

	*computedChecksum = AppendOnlyStorageFormat_ComputeBlockChecksum(
																	 headerPtr,
																	 firstHeaderLen + sizeof(pg_crc32),
																	 overallBlockLen);

	return (*storedChecksum == *computedChecksum);
}
