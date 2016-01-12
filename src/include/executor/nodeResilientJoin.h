/*-------------------------------------------------------------------------
 *
 * nodeResilientJoin.h
 *	  prototypes for nodeResilientJoin.c
 *
 *
 * Portions Copyright (c) 2015- Pivotal Inc.
 *-------------------------------------------------------------------------
 */
#ifndef NODERESILIENTJOIN_H
#define NODERESILIENTJOIN_H

#include "postgres.h"
#include "nodes/execnodes.h"
#include "executor/execSimpleJoin.h"
#include "executor/execTupleHashComputer.h"

typedef struct HashJoinStats
{
	int64 maxBucketSize;
	int64 minBucketSize;
	int64 maxSpilledBucketSize;
	int64 minSpilledBucketSize;
	int64 totalBucketSize;
	int64 totalSpilledBucketSize;
	int emptyBucketCount;
	int spilledBucketCount;
	int totalBucketCount;
	CdbExplain_Agg chainLength;
} HashJoinStats;

typedef struct NestedLoopJoinStats
{

} NestedLoopJoinStats;

typedef struct SubJoinStats
{
	/* Recursion id of the child sub-join */
	int childId;
	/* Recursion id of the parent sub-join */
	int parentId;

	/* The join algorithm for this sub-join */
	JoinAlgorithm joinAlgorithm;

	/* Was role reversed? */
	bool isRoleReversed;

	/* Memory account peak at the beginning */
	Size startMemoryPeak;
	/* Memory account peak at the end (before starting recursive sub-joins) */
	Size endMemoryPeak;
	/* Number of inner bytes read */
	Size innerBytesRead;
	/* Number of outer bytes read */
	Size outerBytesRead;
	/* Number of inner bytes spilled */
	Size innerBytesSpilled;
	/* Number of outer bytes spilled */
	Size outerBytesSpilled;

	union
	{
		HashJoinStats hashJoinStats;
		NestedLoopJoinStats nestedLoopJoinStats;
	};
} SubJoinStats;

typedef struct ResilientJoinStats
{
	/* An array of length numSubJoin */
	SubJoinStats *subJoinStat;

	/*
	 * Number of stats in the subJoinStat array. Every sub-join
	 * uses its joinId to index into the subJoinStat array. If
	 * the array is not sufficiently large, we allocate additional
	 * entries
	 */
    int numSubJoin;
} ResilientJoinStats;

/*
 * ExecNode for ResilientJoin.
 */
typedef struct ResilientJoinState
{
	/*
	 * A joiner state that joins *one* outer tuple with multiple inner tuples
	 * in a nested loop join. For hash join the hash joiner gets to choose
	 * the hash bucket per outer tuple so that the NLJ in the simple joiner
	 * doesn't iterate through all the tuples. For nested loop join the outer
	 * can be the entire outer side and inner can be rescanned once per outer
	 * tuple. The SimpleJoinState is derived from JoinState which also has
	 * PlanState and pointer to Plan burried inside PlanState
	 */
	SimpleJoinState sjs;

	/*
	 * A shared TupleHashComputer that can compute a hash value for an inner tuple. All
	 * sub-joins would share the same computer with different random seeds
	 */
	TupleHashComputer *innerHashComputer;

	/*
	 * A shared TupleHashComputer that can compute a hash value for an outer tuple. All
	 * sub-joins would share the same computer with different random seeds
	 */
	TupleHashComputer *outerHashComputer;

	/*
	 * A list of FuncExprState pointers that can be broken into outerHashKeys
	 * and innerHashKeys
	 */
	List           *hashClauses;

	/* The outer (probe) hash keys as extracted from hashClauses (see above) */
	List           *outerHashKeys;

	/* The inner (build) hash keys as extracted from hashClauses (see above) */
	List           *innerHashKeys;

    /*
     * All the operator functions' oids as extracted from hashClauses (see above).
     * This is mostly for compatibility with legacy hash join. The resilient hash
     * join should only use a set of binary hash functions
     */
    List           *hashOperators;

	/*
	 * This slot is shared between multiple AbstractTupleReader for reading outer tuples.
	 * Note, for OperatorTupleReader we don't use this slot. Instead the OperatorTupleReader
	 * returns output tuple of its PlanState
	 */
	TupleTableSlot *outerTupleSlot;

	/*
	 * This slot is shared between multiple AbstractTupleReader for reading inner tuples.
	 * Note, for OperatorTupleReader we don't use this slot. Instead the OperatorTupleReader
	 * returns output tuple of its PlanState
	 */
	TupleTableSlot *innerTupleSlot;

	/*
	 * We read one tuple from outer side to check if we have an empty outer side
	 * (i.e., skip building the hash table). We save that pre-fetched tuple in
	 * this slot
	 */
	TupleTableSlot *prefetchedOuterTupleSlot;

	/*
	 * Should we first check for emptiness of the outer side? If prefetch_inner
	 * is set to true, we never try to prefetch the outer to check for emptiness
	 */
	bool                prefetchInner;

	/* If set to true, this is a IS NOT DISTICT join. In such case we keep nulls */
	bool                nonEquiJoin;

	/* Memory quota in bytes (i.e., after converting operatorMemKB to byte */
	Size memoryQuota;

	/* Does the join type permit switching build and probe side? */
	bool roleReversalAllowed;

	/* The root joiner that operates on the entire input */
	struct ResilientJoiner *rootJoiner;
	/* The current sub-joiner that operates on one sub-partition of the input */
	struct ResilientJoiner *curJoiner;

	/* Number of sub-joins. Can be used to assign unique id to each sub-join */
	int numSubJoin;

	/* Statistics for explain analyze */
	ResilientJoinStats *joinStats;
} ResilientJoinState;

extern int	ExecCountSlotsResilientJoin(ResilientJoin *node);
extern ResilientJoinState *ExecInitResilientJoin(ResilientJoin *node, EState *estate, int eflags);
extern TupleTableSlot *ExecResilientJoin(ResilientJoinState *node);
extern void ExecEndResilientJoin(ResilientJoinState *node);
extern void ExecResilientJoinReScan(ResilientJoinState *node, ExprContext *exprCtxt);
extern void ExecEagerFreeResilientJoin(ResilientJoinState *node);
extern void InitStatArrayEntry(ResilientJoiner *joiner, ResilientJoinState *joinState, JoinAlgorithm joinAlgorithm);

#endif   /* NODERESILIENTJOIN_H */
