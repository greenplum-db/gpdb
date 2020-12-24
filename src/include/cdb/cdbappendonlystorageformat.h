/*-------------------------------------------------------------------------
 *
 * cdbappendonlystorageformat.h
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbappendonlystorageformat.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBAPPENDONLYSTORAGEFORMAT_H
#define CDBAPPENDONLYSTORAGEFORMAT_H

extern int32 AppendOnlyStorageFormat_RegularHeaderLenNeeded(
	bool			usingChecksum);

extern void AppendOnlyStorageFormat_MakeSmallContentHeader(
	uint8			*headerPtr,
	bool			usingChecksums,
	bool			hasFirstRowNum,
	int				version,
	int64			firstRowNum,
	int				executorKind,
	int				rowCount,
	int32			dataLength,
	int32			compressedLength);

extern void AppendOnlyStorageFormat_MakeLargeContentHeader(
	uint8			*headerPtr,
	bool			usingChecksums,
	bool			hasFirstRowNum,
	int				version,
	int64			firstRowNum,
	int				executorKind,
	int				largeRowCount,
	int32			largeContentLength);

extern char *AppendOnlyStorageFormat_GetHeaderCheckErrorStr(void);

extern AOHeaderCheckError AppendOnlyStorageFormat_GetHeaderInfo(
	uint8			*headerPtr,
	bool			usingChecksums,
	AoHeaderKind	*headerKind,
	int32			*actualHeaderLen);

extern AOHeaderCheckError AppendOnlyStorageFormat_GetSmallContentHeaderInfo(
	uint8			*headerPtr,
	int				headerLen,
	bool			usingChecksums,
	int32			blockLimitLen,
	int32			*overallBlockLen,
	int32			*offset,
	int32			*uncompressedLen,
	int				*executorBlockKind,
	bool			*hasFirstRowNum,
	int				version,
	int64			*firstRowNum,
	int				*rowCount,
	bool			*isCompressed,
	int32			*compressedLen);

extern int32 AppendOnlyStorageFormat_GetCompressedLen(
	uint8			*headerPtr);

extern AOHeaderCheckError AppendOnlyStorageFormat_GetLargeContentHeaderInfo(
	uint8			*headerPtr,
	int				headerLen,
	bool			usingChecksums,
	int32			*largeContentLen,
	int				*executorBlockKind,
	bool			*hasFirstRowNum,
	int64			*firstRowNum,
	int				*largeRowCount);

extern bool AppendOnlyStorageFormat_VerifyHeaderChecksum(
	uint8			*headerPtr,
	pg_crc32		*storedChecksum,
	pg_crc32		*computedChecksum);

extern bool AppendOnlyStorageFormat_VerifyBlockChecksum(
	uint8			*headerPtr,
	int32			overallBlockLen,
	pg_crc32		*storedChecksum,
	pg_crc32		*computedChecksum);

extern void AppendOnlyStorageFormat_MakeNonBulkDenseContentHeader(
	uint8			*headerPtr,
	bool			usingChecksums,
	bool			hasFirstRowNum,
	int				version,
	int64			firstRowNum,
	int				executorKind,
	int				rowCount,
	int32			dataLength);

extern AOHeaderCheckError AppendOnlyStorageFormat_GetNonBulkDenseContentHeaderInfo(
	uint8			*headerPtr,
	int				headerLen,
	bool			usingChecksums,
	int32			blockLimitLen,
	int32			*overallBlockLen,
	int32			*offset,
	int32			*dataLength,
	int				*executorBlockKind,
	bool			*hasFirstRowNum,
	int				version,
	int64			*firstRowNum,
	int				*rowCount);

extern void AppendOnlyStorageFormat_MakeBulkDenseContentHeader(
	uint8			*headerPtr,
	bool			usingChecksums,
	bool			hasFirstRowNum,
	int				version,
	int64			firstRowNum,
	int				executorKind,
	int				rowCount,
	int32			dataLength,
	int32			compressedLength);

AOHeaderCheckError
AppendOnlyStorageFormat_GetBulkDenseContentHeaderInfo(
	uint8			*headerPtr,
	int				headerLen,
	bool			usingChecksums,
	int32			blockLimitLen,
	int32			*overallBlockLen,
	int32			*offset,
	int32			*uncompressedLen,
	int				*executorBlockKind,
	bool			*hasFirstRowNum,
	int				version,
	int64			*firstRowNum,
	int				*rowCount,
	bool			*isCompressed,
	int32			*compressedLen);

extern char *AppendOnlyStorageFormat_SmallContentHeaderStr(
	uint8			*headerPtr,
	bool			usingChecksums,
	int				version);

extern char *AppendOnlyStorageFormat_LargeContentHeaderStr(
	uint8			*headerPtr,
	bool			usingChecksums);

extern char *AppendOnlyStorageFormat_NonBulkDenseContentHeaderStr(
	uint8			*headerPtr,
	bool			usingChecksums,
	int				version);

extern char *AppendOnlyStorageFormat_BulkDenseContentHeaderStr(
	uint8			*headerPtr,
	bool			usingChecksums,
	int				version);

extern char *AppendOnlyStorageFormat_BlockHeaderStr(
	uint8			*headerPtr,
	bool			usingChecksums,
	int				version);

extern void errdetail_appendonly_storage_content_header(
	uint8	*headerPtr,
	bool	usingChecksums,
	int		version);

extern void errdetail_appendonly_storage_smallcontent_header(
	uint8	*headerPtr,
	bool	usingChecksums,
	int		version);

extern void errdetail_appendonly_storage_largecontent_header(
	uint8	*headerPtr,
	bool	usingChecksums,
	int		version);

extern void errdetail_appendonly_storage_densecontent_header(
	uint8	*headerPtr,
	bool	usingChecksums,
	int		version);

extern void errdetail_appendonly_storage_nonbulkdensecontent_header(
	uint8	*headerPtr,
	bool	usingChecksums,
	int		version);

extern void errdetail_appendonly_storage_bulkdensecontent_header(
	uint8	*headerPtr,
	bool	usingChecksums,
	int		version);

#endif   /* CDBAPPENDONLYSTORAGEFORMAT_H */
