/*-------------------------------------------------------------------------
 *
 * cdbappendonlystorageformat_impl.h
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 * This file exists for the benefit of external programs that may wish to
 * check append-optimized storage header info.  They can #include this to get
 * the code referenced by cdb/cdbappendonlystorageformat.h.
 *
 * IDENTIFICATION
 *          src/include/cdb/cdbappendonlystorageformat_impl.h
 *
 *-------------------------------------------------------------------------
 */

#include "cdb/cdbappendonlystorage_int.h"
#include "cdb/cdbappendonlystorage.h"
#include "port/pg_crc32c.h"

#define MAX_AOHEADER_CHECK_ERROR_STR 300
char AoHeaderCheckErrorStr[MAX_AOHEADER_CHECK_ERROR_STR] = "\0";

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

	Assert(headerPtr != NULL);
	Assert(headerKind != NULL);

	header = (AOHeader *) headerPtr;

	if (header->header_bytes_0_3 == 0)
	{
		snprintf(AoHeaderCheckErrorStr,
				 MAX_AOHEADER_CHECK_ERROR_STR,
				 "Append-only storage header is invalid -- first 32 bits are all zeroes (header_bytes_0_3 0x%08x, header_bytes_4_7 0x%08x)",
				 header->header_bytes_0_3, header->header_bytes_4_7);

		return AOHeaderCheckFirst32BitsAllZeroes;
	}

	if (AOHeaderGet_reserved0(header) != 0)
	{
		snprintf(AoHeaderCheckErrorStr,
				 MAX_AOHEADER_CHECK_ERROR_STR,
				 "Append-only storage header is invalid -- reserved bit 0 of the header is not zero (header_bytes_0_3 0x%08x, header_bytes_4_7 0x%08x)",
				 header->header_bytes_0_3, header->header_bytes_4_7);

		return AOHeaderCheckReservedBit0Not0;
	}

	*headerKind = AOHeaderGet_headerKind(header);

	if (*headerKind == AoHeaderKind_None)
	{
		snprintf(AoHeaderCheckErrorStr,
				 MAX_AOHEADER_CHECK_ERROR_STR,
				 "Append-only storage header is invalid -- invalid value 0 (none) for header kind (header_bytes_0_3 0x%08x, header_bytes_4_7 0x%08x)",
				 header->header_bytes_0_3, header->header_bytes_4_7);

		return AOHeaderCheckInvalidHeaderKindNone;
	}

	if (*headerKind >= MaxAoHeaderKind)
	{
		snprintf(AoHeaderCheckErrorStr,
				 MAX_AOHEADER_CHECK_ERROR_STR,
				 "Append-only storage header is invalid -- invalid header kind value %d (header_bytes_0_3 0x%08x, header_bytes_4_7 0x%08x)",
				 (int) *headerKind,
				 header->header_bytes_0_3, header->header_bytes_4_7);

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
			snprintf(AoHeaderCheckErrorStr,
					 MAX_AOHEADER_CHECK_ERROR_STR,
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
		snprintf(AoHeaderCheckErrorStr,
				 MAX_AOHEADER_CHECK_ERROR_STR,
				 "Append-only storage header is invalid -- overall block length %d is > block limit length %d "
				 "(smallcontent_bytes_0_3 0x%08x, smallcontent_bytes_4_7 0x%08x)",
				 *overallBlockLen,
				 blockLimitLen,
				 blockHeader->smallcontent_bytes_0_3, blockHeader->smallcontent_bytes_4_7);
		return AOHeaderCheckInvalidOverallBlockLen;
	}

	return AOHeaderCheckOk;
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
		snprintf(AoHeaderCheckErrorStr,
				 MAX_AOHEADER_CHECK_ERROR_STR,
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

pg_crc32
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

pg_crc32
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
		snprintf(AoHeaderCheckErrorStr,
				 MAX_AOHEADER_CHECK_ERROR_STR,
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
			snprintf(AoHeaderCheckErrorStr,
					 MAX_AOHEADER_CHECK_ERROR_STR,
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
		snprintf(AoHeaderCheckErrorStr,
				 MAX_AOHEADER_CHECK_ERROR_STR,
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

