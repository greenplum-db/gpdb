/*-------------------------------------------------------------------------
 *
 * syncbitvector.h
 *		Interface for synchronized bit vectors.
 *
 * Portions Copyright (c) 2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/syncbitvector.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef SYNCBITVECTOR_H_
#define SYNCBITVECTOR_H_

#define BITVECTOR_SIZE_BINARY_EXP 	(20)
#define BITVECTOR_SIZE_BYTES 		(1L << BITVECTOR_SIZE_BINARY_EXP)	/* 2^20 Bytes = 1MB  */
#define BITVECTOR_SIZE_BITS 		(BITVECTOR_SIZE_BYTES << 3)			/* 2^23 bits */
#define BITVECTOR_OFFSET_MASK 		(BITVECTOR_SIZE_BITS - 1)			/* 0x7FFFFF */

#define SYNC_BITVECTOR_MAX_COUNT 	(100)								/* max total size = 100 MB */

/* bit vector container */
typedef struct SyncBitvectorContainer_s
{
	int64 vectorCount;		// number of vectors stored

	int64 base[0];			// base address for the first vector, 64-bit aligned

} SyncBitvectorContainer_s;

typedef struct SyncBitvectorContainer_s *SyncBitvectorContainer;

/* interface */
extern Size SyncBitVector_ShmemSize(int vectorCount);
extern SyncBitvectorContainer SyncBitVector_ShmemInit(const char *name, int vectorCount);
extern bool SyncBitVector_GetBit(SyncBitvectorContainer container, int32 vectorOffset, int64 bitOffset);
extern void SyncBitVector_SetBit(SyncBitvectorContainer container, int32 vectorOffset, int64 bitOffset);
extern bool SyncBitVector_TestTestSetBit(SyncBitvectorContainer container, int32 vectorOffset, uint64 bitOffset);

#endif /* SYNCBITVECTOR_H_ */
