/*-------------------------------------------------------------------------
 *
 * cdbappendonlystorage.h
 *
 * Portions Copyright (c) 2007-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbappendonlystorage.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBAPPENDONLYSTORAGE_H
#define CDBAPPENDONLYSTORAGE_H

#include "c.h"
#include "utils/pg_crc.h"

/*
 * Small Content.
 */
#define AOSmallContentHeader_MaxRowCount 0x3FFF   
										   // 14 bits, or 16,383 (16k-1).
										   // Maximum row count for small content.
										   
#define AOSmallContentHeader_MaxLength   0x1FFFFF 
										   // 21 bits, or 2,097,151 (2 Mb-1).
										   // Maximum blocksize.  Does not include
										   // Append-Only Storage Header overhead...

/*
 * Large Content.
 */
#define AOLargeContentHeader_MaxLargeRowCount 0x1FFFFFF 
										   // 25 bits, or 33,554,431, or (2^25-1)
										   // Maximum row count for large content.

#define AOLargeContentHeader_MaxLargeContentLength   0x3FFFFFFF 
										  // 30 bits, or 1,073,741,823 (1Gb-1).

/*
 * Non-Bulk Dense Content.
 */
#define AONonBulkDenseContentHeader_MaxLength   0x1FFFFF 
											   // 21 bits, or 2,097,151 (2 Mb-1).
											   // Maximum blocksize.  Does not include
											   // Append-Only Storage Header overhead...
	
#define AONonBulkDenseContentHeader_MaxLargeRowCount 0x3FFFFFFF 
										   // 30 bits, or 1,073,741,823, or (2^30-1)
										   // Maximum row count for dense content.

/*
 * Bulk Dense Content.
 */
#define AOBulkDenseContentHeader_MaxLength   0x1FFFFF 
											   // 21 bits, or 2,097,151 (2 Mb-1).
											   // Maximum blocksize.  Does not include
											   // Append-Only Storage Header overhead...
	
#define AOBulkDenseContentHeader_MaxLargeRowCount 0x3FFFFFFF 
										   // 30 bits, or 1,073,741,823, or (2^30-1)
										   // Maximum row count for dense content.

/*
 * Rounding up and padding.
 */
#define AOStorage_RoundUp4(l) ((((l)+3)/4)*4) /* 32-bit alignment */

#define AOStorage_RoundUp8(l) ((((l)+7)/8)*8) /* 64-bit alignment */

#define AOStorage_RoundUp(l, version) \
	((IsAOBlockAndMemtupleAlignmentFixed(version)) ? (AOStorage_RoundUp8(l)) : (AOStorage_RoundUp4(l)))

#define	AOStorage_ZeroPad(\
			buf,\
			len,\
			padToLen)\
{\
	int b;\
\
	for (b = len; b < padToLen; b++)\
		buf[b] = 0;\
}

/*
 * Header kinds.
 */
typedef enum AoHeaderKind
{
	AoHeaderKind_None = 0,
	AoHeaderKind_SmallContent = 1,
	AoHeaderKind_LargeContent = 2,
	AoHeaderKind_NonBulkDenseContent = 3,
	AoHeaderKind_BulkDenseContent = 4,
	MaxAoHeaderKind /* must always be last */
} AoHeaderKind;


#define AoHeader_RegularSize 8
#define AoHeader_LongSize 16

/*
 * Test if the header is regular or long size.
 */
inline static bool AoHeader_IsLong(AoHeaderKind aoHeaderKind)
{
	switch (aoHeaderKind)
	{
	case AoHeaderKind_SmallContent:			
		return false;

	case AoHeaderKind_LargeContent:			
		return false;

	case AoHeaderKind_NonBulkDenseContent:	
		return false;

	case AoHeaderKind_BulkDenseContent:		
		return true;

	default:
		elog(ERROR, "Unexpected Append-Only header kind %d", 
			 aoHeaderKind);
		return false;	// Never reaches here.
	}
}

inline static int32 AoHeader_Size(
	bool isLong,
	bool hasChecksums,
	bool hasFirstRowNum)
{
	int32	headerLen;

	headerLen = (isLong ? AoHeader_LongSize : AoHeader_RegularSize);

	if (hasChecksums)
	{
		headerLen += 2 * sizeof(pg_crc32);
	}

	if (hasFirstRowNum)
	{
		headerLen += sizeof(int64);
	}

	return headerLen;
}

/*
 * Header check errors.
 */
typedef enum AOHeaderCheckError
{
	AOHeaderCheckOk = 0,
	AOHeaderCheckFirst32BitsAllZeroes,
	AOHeaderCheckReservedBit0Not0,
	AOHeaderCheckInvalidHeaderKindNone,
	AOHeaderCheckInvalidHeaderKind,
	AOHeaderCheck__OBSOLETE_,
	AOHeaderCheckInvalidCompressedLen,
	AOHeaderCheckInvalidOverallBlockLen,
	AOHeaderCheckLargeContentLenIsZero,
} AOHeaderCheckError;

extern int32 AppendOnlyStorage_GetUsableBlockSize(
	int32 configBlockSize);

#endif   /* CDBAPPENDONLYSTORAGE_H */
