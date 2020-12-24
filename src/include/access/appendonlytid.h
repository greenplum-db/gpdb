/*-------------------------------------------------------------------------
 *
 * appendonlytid.h
 *
 * Portions Copyright (c) 2007-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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
 * AOTupleId is a unique tuple id, specific to AO relation tuples, of the
 * form (segfile#, row#)
 *
 * *** WARNING *** Outside the appendonly AM code, AOTIDs are treated as
 * HEAPTIDs!  Therefore, AOTupleId struct must have the same size and
 * alignment as ItemPointer, and we mustn't construct AOTupleIds that would
 * look like invalid ItemPointers.
 *
 * The 48 bits available in the struct are divided as follow:
 *
 * - the 7 most significant bits stand for which segment file the tuple is in
 *   (Limit: 128 (2^8)). This also makes up part of the "block number" when
 *   interpreted as a heap TID.
 *
 * - The next 25 bits come from the 25 most significant bits of the row
 *   number. This makes up the "lower" part of the block number when
 *   interpreted as a heap TID.
 *
 * - Next up, slightly more interesting: we take the least significant 15 bits
 *   from the row numbers, add one to it. The results take up 16 bits (range:
 *   1 - 2^15). This forms the "offset" when interpreted as a heap TID. We
 *   plus one on the LSB to accommodate the assumption in various places that
 *   heap TID's never have a zero offset.
 *
 * NOTE: Before GPDB6, we *always* set the reserved bit, not only when
 * 'bytes_4_5' would otherwise be zero.  That was simpler, but caused problems
 * of its own, because very high offset numbers, which cannot occur in heap
 * tables, have a special meaning in some places.  See TUPLE_IS_INVALID in
 * gist_private.h, for example.  In other words, the 'ip_posid' field now uses
 * values in the range 1..32768, whereas before GPDB 6, it used the range
 * (32768..65535).
 */
typedef struct AOTupleId
{
	uint16		bytes_0_1;
	uint16		bytes_2_3;
	uint16		bytes_4_5;
} AOTupleId;

#define AO_MAX_OFFSET	32768
#define AOTupleIdGet_segmentFileNum(h)        ((((h)->bytes_0_1&0xFE00)>>9)) // 7 bits

static inline uint64
AOTupleIdGet_rowNum(AOTupleId *h)
{
	uint64 rowNumber;
	Assert(h->bytes_4_5 > 0);
	Assert(h->bytes_4_5 <= 0x8000);

	/* top 25 bits */
	rowNumber = ((uint64)(h->bytes_0_1&0x01FF))<<31;
	rowNumber |= ((uint64)(h->bytes_2_3))<<15;

	/* lower 15 bits */
	/* subtract one since we always add one when initialing the bytes_4_5 */
	rowNumber |= h->bytes_4_5 - 1;

	return rowNumber;
}

static inline void
AOTupleIdInit(AOTupleId *h, uint16 segfilenum, uint64 rownum)
{
	h->bytes_0_1 = ((uint16) (0x007F & segfilenum)) << 9;
	h->bytes_0_1 |= (uint16) ((INT64CONST(0x000000FFFFFFFFFF) & rownum) >> 31);
	h->bytes_2_3 = (uint16) ((INT64CONST(0x000000007FFFFFFF) & rownum) >> 15);

	/*
	 * Add one to make sure bytes_4_5 is never zero. Since bytes_4_5 form
	 * offset part when interpreted as TID, rest of system expects offset to
	 * be greater than zero.
	 */
	h->bytes_4_5 = (0x7FFF & rownum) + 1;
}

/* like ItemPointerSetInvalid */
static inline void
AOTupleIdSetInvalid(AOTupleId *h)
{
	ItemPointerSetInvalid((ItemPointer) h);
}

#define AOTupleId_MaxRowNum            INT64CONST(1099511627775) 		// 40 bits, or 1099511627775 (1 trillion).

#define AOTupleId_MaxSegmentFileNum    			127
#define AOTupleId_MultiplierSegmentFileNum    	128	// Next up power of 2 as multiplier.

extern char *AOTupleIdToString(AOTupleId *aoTupleId);

#endif							/* APPENDONLYTID_H */
