/*-------------------------------------------------------------------------
 *
 * execTupleHashComputer.h
 *	  prototypes for execTupleHashComputer.c
 *
 * Portions Copyright (c) 2015- Pivotal Inc.
 *-------------------------------------------------------------------------
 */
#ifndef EXECTUPLEHASHCOMPUTER_H
#define EXECTUPLEHASHCOMPUTER_H

#include "postgres.h"
#include "nodes/execnodes.h"

#ifdef HASH_32
#define MurmurHash3(keyp, len, seed, out) MurmurHash3_x86_32(keyp, len, seed, out)
#else
#define MurmurHash3(keyp, len, seed, out) MurmurHash3_x64_128(keyp, len, seed, out)
#endif

/*
 * A hash function prototype that can take a random seed (to generate new hash function)
 * a datum, and optionally (can be set to NULL) a binaryDatumFunc that can convert a
 * variable length data into binary datum to extract the bytes from it
 */
typedef HashType(BinaryHashFunc)(int seed, Datum hashKey, FmgrInfo *binaryDatumFunc);

/*
 * Each nested joiner has a partitioner that can re-partition the build
 * and probe side. Note, re-partitioning only applies for recursive HashJoin
 */
typedef struct _TupleHashComputer
{
	/*
	 * The current user of this hash computer. Note, a single
	 * hash computer may be shared across multiple users
	 */
	void *requester;

	/* The random seed to use to generate binary hash function */
	int seed;

	/* How many columns to hash */
	int numHashKeys;

	/* The array of expressions that can be evaluated for each tuple to get back the hash keys */
	ExprState **hashKeyExprs;

	/*
	 * An array of legacy hash functions, one per hashKey to hash each key. Note, we only use
	 * murmur3 hash function family. However, we save these functions to determine
	 * the type of each hash key. In the future if optimizer passes the type information
	 * directly, we can skip hash functions. It is also possible to use these legacy hash
	 * functions to hash at recursion level 0 to preserve the performance behavior or legacy
	 * hash join
	 */
	FmgrInfo *legacyHashFunctions;
	/* Is the join operator (e.g., "=") strict for a hash key? */
	bool *isStrictOperator;

	/* Array of Oid of the types for the hashable columns */
	Oid *typeOids;
	/* Array of type length of each hashable comlumn */
	int16 *typeLength;
	/*
	 * Do we compute hash even if one of the keys evaluate to null?
	 * We typically don't hash an inner tuple if one of the hash key
	 * is null. However, for outer tuple we always calculate hash value
	 */
	bool shouldHashNull;

	/* The expression context to use to evaluate hashKeyExprs */
	ExprContext *exprContext;
} TupleHashComputer;

extern TupleHashComputer* TupleHashComputer_Create(List *hashKeyExprs, List *hashOperators, bool shouldHashNull, ExprContext *exprContext);
extern void TupleHashComputer_Begin(TupleHashComputer *tupleHashComputer, void *requester, int seed);
extern void TupleHashComputer_End(TupleHashComputer *tupleHashComputer, void *requester);

extern HashType TupleHashComputer_ComputeHash(TupleHashComputer *tupleHashComputer, bool *allHashValsAreNull);

#endif   /* EXECTUPLEHASHCOMPUTER_H */
