/*-------------------------------------------------------------------------
 *
 * syncbitvector.c
 * 		Implementation of interface for synchronized bit vectors.
 *
 * Synchronized bit vectors are stored in shared memory, so they can be
 * accessed by all backends. Synchronization is achieved by using the
 * test-and-test-and-set technique so the overhead is minimal.
 *
 * The size of each bit vector is set to BITVECTOR_SIZE_BYTES. The postmaster
 * allocates a shared memory segment that can hold up to
 * SYNC_BITVECTOR_MAX_COUNT bit vectors. This shared memory segment is passed
 * to all backends during their initialization. At shutdown the postmaster
 * releases the shared memory segment.  In the occurrence of a severe failure,
 * the postmaster resets shared memory; at this point the bit vector
 * information is lost.
 *
 * Portions Copyright (c) 2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/misc/syncbitvector.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "port/atomics.h"
#include "storage/shmem.h"
#include "utils/syncbitvector.h"

/*
 * bit vector access descriptor.
 *
 * The bit vector is accessed using uint32 data.
 * Each datum has 32 bits. The bit is assumed to be
 * located in the (datum_offset)-th datum.
 */
typedef struct datumIdx
{
	int64 datum_offset;		/* offset of the datum inside the bit vector */

	uint32 bit_mask;		/* bit mask for the uint32 datum that contains the bit */

} datumIdx;

/* static functions */
static inline void *getVector(SyncBitvectorContainer container, int32 vectorOffset);
static inline datumIdx *getDatumIdx(uint64 offset, datumIdx *idx);

/*
 * Returns the size of the shared memory segment
 * required for holding (vectorCount) bit vectors.
 */
Size SyncBitVector_ShmemSize(int vectorCount)
{
	Assert(vectorCount <= SYNC_BITVECTOR_MAX_COUNT && "vector count exceeds maximum value");

	Size size = sizeof(SyncBitvectorContainer_s) + vectorCount * BITVECTOR_SIZE_BYTES;

	return size;
}

/*
 * Initializes the shared memory segment and casts it to a bit vector container.
 */
SyncBitvectorContainer SyncBitVector_ShmemInit(const char *name, int vectorCount)
{
	bool found = false; 	/* indicates if the shmem segment has already been initialized */
	const Size size = SyncBitVector_ShmemSize(vectorCount);

	SyncBitvectorContainer container = (SyncBitvectorContainer) ShmemInitStruct(name, size, &found);

	if (container == NULL) {
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 (errmsg("Not enough shared memory for SyncBitVector."))));
	}

	if (!found)
	{
		/* only the postmaster resets the bit vector container */
		MemSet(container, 0, size);
		container->vectorCount = vectorCount;
	}

	return container;
}

/*
 * Returns the bit value for the given offset inside the given vector.
 */
bool SyncBitVector_GetBit(SyncBitvectorContainer container, int32 vectorOffset, int64 bitOffset)
{
	void *vector = getVector(container, vectorOffset);

	/* use uint32 as datum type to access bit vector */
	datumIdx idx;
	getDatumIdx(bitOffset, &idx);

	uint32 *pdatum = ((uint32 *) vector) + idx.datum_offset;
	uint32 bit = (*pdatum) & idx.bit_mask;

	return (bit != 0);
}

/*
 * Sets the bit value for the given offset inside the given vector.
 * Non-synchronized operation.
 */
void SyncBitVector_SetBit(SyncBitvectorContainer container, int32 vectorOffset, int64 bitOffset)
{
	void *vector = getVector(container, vectorOffset);

	/* use uint32 as datum type to access bit vector */
	datumIdx idx;
	getDatumIdx(bitOffset, &idx);

	uint32 *pdatum = ((uint32 *) vector) + idx.datum_offset;
	*pdatum |= idx.bit_mask;
}

/*
 * Checks the bit value for the given offset inside the given vector.
 * Returns false if the bit is set, otherwise sets it and returns true.
 * Synchronized operation (through atomic test and set).
 */
bool SyncBitVector_TestTestSetBit(SyncBitvectorContainer container, int32 vectorOffset, uint64 bitOffset)
{
	void *vector = getVector(container, vectorOffset);

	/* use uint32 as datum type to access bit vector */
	datumIdx idx;
	getDatumIdx(bitOffset, &idx);

	/*
	 * We use TestTestSet on a 32-bit datum. This fails if any of the datum
	 * bits are changed, i.e. if a concurrent update changes a different bit
	 * than the one we want to set. We therefore need to recheck the bit and,
	 * if it is not set, call compare_and_swap again.
	 */
	int32 i = 0;
	for (i = 0; i < 32; i++)	/* 32-bit datum */
	{
		bool swapped = false;
		uint32 *pdatum = ((uint32 *) vector) + idx.datum_offset;
		uint32 bit = (*pdatum) & idx.bit_mask;
		uint32 datumOld = *pdatum;
		uint32 datumNew = datumOld | idx.bit_mask;

		/* test */
		if (bit)
		{
			return false;
		}

		/* test and set */
		swapped = pg_atomic_compare_exchange_u32((pg_atomic_uint32 *)pdatum, &datumOld, datumNew);
		if (swapped)
		{
			return true;
		}
	}

	return false;
}

/*
 * Returns the base pointer for a bit vector given its offset.
 */
static inline
void *getVector(SyncBitvectorContainer container, int32 vectorOffset)
{
	Assert(container);
	Assert(container->base);
	Assert(vectorOffset >= 0 && "vector ID is negative");
	Assert(vectorOffset < container->vectorCount && "vector ID is greater than vector count");

	/* use byte arithmetics */
	int8 *base = (int8 *) container->base;
	int8 *vectorBase = base + vectorOffset * BITVECTOR_SIZE_BYTES;

	return (void *) vectorBase;
}

/*
 * Updates and returns an access descriptor for the given bit offset.
 * The offset is normalized to the bit vector size. The access descriptor
 * is allocated by the caller and passed as an argument.
 */
static inline
datumIdx *getDatumIdx(uint64 offset, datumIdx *idx)
{
	Assert(idx &&  "idx is NULL");

	/* datum size (uint32) is 32 bits */
	const int32 LogBitsPerDatum = 5; 					/* = log2(32) */
	const uint64 MaxBitValue = 31UL;

	/* offset = offset % BITVECTOR_SIZE_BITS - prevents overflow */
	offset = offset & BITVECTOR_OFFSET_MASK;

	idx->datum_offset = offset >> LogBitsPerDatum;		/* = (offset / 32) */
	idx->bit_mask = 1UL << (offset & MaxBitValue);		/* = 1 << (offset % 32) */

	return idx;
}

// EOF
