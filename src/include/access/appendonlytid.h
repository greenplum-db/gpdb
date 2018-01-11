/*-------------------------------------------------------------------------
 *
 * appendonlytid.h
 *
 * Portions Copyright (c) 2007-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/access/appendonlytid.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef APPENDONLYTID_H
#define APPENDONLYTID_H

#include "c.h"

/*
 * AOTupleId is a unique tuple id, specific to AO
 * relation tuples, of the form (segfile#, row#)
 *
 * *** WARNING *** Some code interprets AOTIDs as HEAPTIDs, and would like
 * AOTIDs has the same ordering as HEAPTIDs.
 *
 * Begin mischief.  Want to make sure the last 16 bits (uint16) are non-zero so that when
 * the executor asserts the item number is non-zero our AOTID will pass the test.
 * So, we create a reserved bit that is always on.  So, in the case that the segment file
 * number is 0 (utility mode) and the lower part (15 bits) of the row number is 0, our AOTID's
 * last 16 bits will be non-zero because of the always on reserved bit.
 *
 * Out of the following 48 bits, the 7 leftmost bits stand for which segment file the
 * tuple is in (Limit: 128 (2^8)), the 16th rightmost bit is reserved and always 1,
 * the remaining 40 bits stand for the row within the  segment file
 * (Limit: 1099 trillion (2^40 - 1)).
 *
 *
 * ***WARNING*** STRUCT PACKING ISSUE.
 *
 *       Previously, we had the uint16 first and the uint32 second.  But on some compilers
 *       the second uint32 would get packed on a 32-bit boundary and the struct would end
 *       up with a hole between the fields...  Arranging the uint16 second is known to work
 *       correctly since the ItemPointerData has the same layout.  And, apparently putting
 *       small fields after big ones is a policy in system catalog struct layouts.
 */
typedef struct AOTupleId
{
	uint16		bytes_0_1;
	uint16		bytes_2_3;
	uint16		bytes_4_5;

} AOTupleId;

#define AOTUPLEID_INIT {0,0}

#define AOTupleIdGet_segmentFileNum(h)        ((((h)->bytes_0_1&0xFE00)>>9)) // 7 bits
#define AOTupleIdGet_makeHeapExecutorHappy(h) (((h)->bytes_4_5&0x8000)) // 1 bit
#define AOTupleIdGet_rowNum(h) \
	((((uint64)((h)->bytes_0_1&0x01FF))<<31)|(((uint64)((h)->bytes_2_3))<<15)|(((uint64)((h)->bytes_4_5&0x7FFF))))
 /* top most 25 bits */	/* 15 bits from bytes_4_5 */

/* ~_Init zeroes the 2 regular fields and sets the always on field to 1. */
static inline void
AOTupleIdInit_Init(AOTupleId *h)
{
	h->bytes_0_1 = 0;
	h->bytes_2_3 = 0;
	h->bytes_4_5 = 0x8000;
}
static inline void
AOTupleIdInit_segmentFileNum(AOTupleId *h, uint16 e)
{
	h->bytes_0_1 |= ((uint16) (0x007F & e)) << 9;
}
static inline void
AOTupleIdInit_rowNum(AOTupleId *h, uint64 e)
{
	h->bytes_0_1 |= (uint16) ((INT64CONST(0x000000FFFFFFFFFF) & e) >> 31);
	h->bytes_2_3 |= (uint16) ((INT64CONST(0x000000007FFFFFFF) & e) >> 15);
	h->bytes_4_5 |= 0x7FFF & e;
}

#define AOTupleId_MaxRowNum            INT64CONST(1099511627775) 		// 40 bits, or 1099511627775 (1099 trillion).
#define AOTupleId_MaxRowNum_CommaStr  "1,099,511,627,775"

#define AOTupleId_MaxSegmentFileNum    			127
#define AOTupleId_MultiplierSegmentFileNum    	128	// Next up power of 2 as multiplier.

extern char *AOTupleIdToString(AOTupleId *aoTupleId);

#endif							/* APPENDONLYTID_H */
