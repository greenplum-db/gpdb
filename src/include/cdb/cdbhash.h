/*--------------------------------------------------------------------------
 *
 * cdbhash.h
 *	 Definitions and API functions for cdbhash.c
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbhash.h
 *
 *--------------------------------------------------------------------------
 */
#ifndef CDBHASH_H
#define CDBHASH_H

/*
 * hashing algorithms.
 */
typedef enum
{
	HASH_FNV_1 = 1,
	HASH_FNV_1A
} CdbHashAlg;

/*
 * reduction methods.
 */
typedef enum
{
	REDUCE_LAZYMOD = 1,
	REDUCE_BITMASK
} CdbHashReduce;

/*
 * Structure that holds Greenplum Database hashing information.
 */
typedef struct CdbHash
{
	uint32		hash;			/* The result hash value							*/
	int			numsegs;		/* number of segments in Greenplum Database used for
								 * partitioning  */
	CdbHashReduce reducealg;	/* the algorithm used for reducing to buckets		*/
	uint32		rrindex;		/* round robin index for empty policy tables		*/

} CdbHash;


typedef void (*datumHashFunction)(void *clientData, void *buf, size_t len);

extern void hashDatum(Datum datum, Oid type, datumHashFunction hashFn, void *clientData);
extern void hashNullDatum(datumHashFunction hashFn, void *clientData);

/*
 * Create and initialize a CdbHash in the current memory context.
 * Parameter numsegs - number of segments in Greenplum Database.
 */
extern CdbHash *makeCdbHash(int numsegs);

/*
 * Initialize CdbHash for hashing the next tuple values.
 */
extern void cdbhashinit(CdbHash *h);

/*
 * Add an attribute to the hash calculation.
 */
extern void cdbhash(CdbHash *h, Datum val, Oid typid);

/*
 * Add a NULL attribute to the hash calculation.
 */
extern void cdbhashnull(CdbHash *h);

/*
 * Hash a tuple for a relation with an empty (no hash keys) partitioning policy.
 */
extern void cdbhashnokey(CdbHash *h);

/*
 * Reduce the hash to a segment number.
 */
extern unsigned int cdbhashreduce(CdbHash *h);

/*
 * Return true if Oid is hashable internally in Greenplum Database.
 */
extern bool isGreenplumDbHashable(Oid typid);

/*
 * Return true if the Oid is an array type.  This can be used prior
 *   to hashing the datum because array typeoids are expected to
 *   have been converted to any array oid.
 */
extern bool typeIsArrayType(Oid typeoid);

extern bool typeIsEnumType(Oid typeoid);

#endif   /* CDBHASH_H */
