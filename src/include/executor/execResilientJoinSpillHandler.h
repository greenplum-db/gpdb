/*-------------------------------------------------------------------------
 *
 * execResilientJoinSpillHandler.h
 *	  prototypes for execResilientJoinSpillHandler.c
 *
 * Portions Copyright (c) 2015- Pivotal Inc.
 *-------------------------------------------------------------------------
 */
#ifndef EXECRESILIENTJOINSPILLHANDLER_H
#define EXECRESILIENTJOINSPILLHANDLER_H

#include "postgres.h"
#include "executor/executor.h"
#include "executor/execResilientJoin.h"

extern void ResilientHashJoiner_CleanupSpillFile(ResilientHashJoiner *hashJoiner, int spillFileIdx, InputSide inputSide);
extern void ResilientHashJoiner_PostSpillCleanup(ResilientHashJoiner *hashJoiner);
extern void ResilientHashJoiner_SpillInnerTuple(ResilientHashJoiner *hashJoiner, int bucketNo, MemTuple tuple);
extern void ResilientHashJoiner_SpillOuterTuple(ResilientHashJoiner *hashJoiner, int bucketNo, MemTuple tuple, HashType hashValue);
extern Size ResilientHashJoiner_SpillBuckets(ResilientHashJoiner *hashJoiner);
extern bool ResilientHashJoiner_ChooseNextSetOfSpillPartitions(ResilientHashJoiner *hashJoiner, Size memoryQuota, bool roleReversalAllowed,
		bool isAlreadyReversed, double *buildTupleCount, int *buildTupleWidth);

extern HashBucketTuple *spillMarkerAddress;
#define IS_SPILLED_BUCKET(bucket) (bucket == spillMarkerAddress)

#endif   /* EXECRESILIENTJOINSPILLHANDLER_H */
