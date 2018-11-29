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

#include "utils/rel.h"

/*
 * Hash Method
 * if change here, please also change pg_database.h
 */
#define INVALID_HASH_METHOD      (-1)
#define MODULO_HASH_METHOD       0
#define JUMP_HASH_METHOD         1

/*
 * reduction methods.
 */
typedef enum
{
	REDUCE_LAZYMOD = 1,
	REDUCE_BITMASK,
	REDUCE_JUMP_HASH
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

	int			natts;
	Oid			typeoids[FLEXIBLE_ARRAY_MEMBER];
} CdbHash;

/*
 * Create and initialize a CdbHash in the current memory context.
 */
extern CdbHash *makeCdbHash(int numsegs, int natts, Oid *typeoids);
extern CdbHash *makeCdbHashForRelation(Relation rel);

/*
 * Initialize CdbHash for hashing the next tuple values.
 */
extern void cdbhashinit(CdbHash *h);

/*
 * Add an attribute to the hash calculation.
 */
extern void cdbhash(CdbHash *h, int attno, Datum datum, bool isnull);

/*
 * Pick a random hash value, for a tuple in a relation with an empty
 * policy (i.e. DISTRIBUTED RANDOMLY).
 */
extern void cdbhashnokey(CdbHash *h);

/*
 * Reduce the hash to a segment number.
 */
extern unsigned int cdbhashreduce(CdbHash *h);

/*
 * Return a random segment number, for a randomly distributed policy.
 */
extern unsigned int cdbhashrandomseg(int numsegs);

/*
 * Return true if Oid is hashable internally in Greenplum Database.
 */
extern bool isGreenplumDbHashable(Oid typid);

/*
 * Return true if the operator Oid is hashable internally in Greenplum Database.
 */
extern bool isGreenplumDbOprRedistributable(Oid oprid);

#endif   /* CDBHASH_H */
