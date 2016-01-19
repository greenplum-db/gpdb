/*-------------------------------------------------------------------------
 *
 * execResilientJoin.h
 *	  prototypes for execResilientJoin.c
 *
 * Portions Copyright (c) 2015- Pivotal Inc.
 *-------------------------------------------------------------------------
 */
#ifndef EXECRESILIENTJOIN_H
#define EXECRESILIENTJOIN_H

#include "postgres.h"
#include "executor/executor.h"
#include "executor/execTupleHashComputer.h"

/* Assert validation for a pointer to ResilientJoiner sub-class */
#define ValidateJoinerPointer(joiner) Assert(NULL != joiner && \
	(IsA(joiner, ResilientHashJoiner) || \
	IsA(joiner, ResilientNestedLoopJoiner)));

/* Assert validation for a pointer to ResilientJoinState */
#define ValidateJoinState(joinState) Assert(NULL != joinState && \
	IsA(joinState, ResilientJoinState));

struct ResilientJoiner;
struct ResilientJoinState;
struct ExecWorkFile;
struct workfile_set;
struct TupleHashComputer;

/*
 * Different stages for a join. For HashJoin we start at PreBuild, and transition
 * into build, which builds the hash table. Once the build is done, we move to
 * probe the hash table. After probing, we process the spill files by creating one
 * or more sub-joins. The "ProcessSpilled" is before the sub-join kicks in, more
 * like a setup for sub-join. After that we keep creating as many sub-joins as
 * necessary and we stay at ProcessSpillChild, until all the children are done.
 * After this we move to SubHashJoinDone to mark the join as done.
 *
 * For NLJ, we move to NestedLoop at the very beginning. Once we finish the NLJ,
 * we move to SubNestedLoopJoinDone.
 *
 * These stages are used in a central controller pattern to delegate to appropriate
 * handler for different stages and also to setup and cleanup (pre and post) for various
 * stages of a join algorithm
 */
typedef enum JoinStage
{
	JoinStage_Uninitialized,
	JoinStage_PreBuild,
	JoinStage_Build,
	JoinStage_Probe,
	JoinStage_ProcessSpilled,
	JoinStage_ProcessSpillChild,
	JoinStage_SubHashJoinDone,
	JoinStage_NestedLoop,
	JoinStage_SubNestedLoopJoinDone,
} JoinStage;

/* The function prototype for obtaining the next joined tuple from a ReslientJoiner */
typedef TupleTableSlot* (GetNextJoinedTupleFunc)(struct ResilientJoiner *joiner);

/*
 * Each hash bucket points to the head of a linked list of HashBucketTuple
 * so that we can navigate all tuples in a bucket by traversing the linked list
 */
typedef struct HashBucketTuple
{
	struct HashBucketTuple *next;
	HashType hashValue;
	/* Tuple data, in MinimalTuple format, follows on a MAXALIGN boundary */
} HashBucketTuple;

#define HASH_BUCKET_MINTUPLE(hjtup)  \
	((MemTuple) ((char *) (hjtup) + MAXALIGN(sizeof(HashBucketTuple))))

/*
 * We inflate a tuple size by adding MemTupleData overhead (to wrap
 * the tuple in MemTupleData) and also to wrap the MemTupleData in
 * HashBucketTuple
 */
#define HASH_BUCKET_MEMORY_INFLATION (MAXALIGN(sizeof(HashBucketTuple)))

/*
 * The final size of a MemTuple to insert it into hash table. This includes
 * the hash bucket memory inflation and the actual tuple width
 */
#define INFLATED_HASH_BUCKET_TUPLE_SIZE(tupWidth) (tupWidth + HASH_BUCKET_MEMORY_INFLATION)

/* The pseudo super class for ResilientHashJoin and ResilientNLJoin */
typedef struct ResilientJoiner
{
	NodeTag type;
	struct ResilientJoinState *joinerState;

	/*
	 * Unique id for this sub-join. Also used to index into the stats
	 * array for all sub-joiner during explain analyze
	 */
	int joinerId;

	/* Which join stage is executing now */
	JoinStage stage;

	/*
	 * The source of the tuples, both inner and outer. The inner
	 * side is expected to be smaller than the outer side
	 */
	struct AbstractTupleReader *innerReader;
	struct AbstractTupleReader *outerReader;

	/*
	 * Number of inner tuples to work on. For recursive sub-join this should be
	 * accurate. For first level join this is just an estimate
	 */
	double expectedTupCount;
	/*
	 * Width of individual tuples. For recursive sub-join this should be average width
	 * of the tuples. For first level join this is just an estimate
	 */
	int expectedTupWidth;

	/* Memory quota for this sub-join */
	Size memoryQuota;

	/*
	 * With respect to global config, is the role of inner and outer
	 * reversed for current innerReader and outerReader
	 */
	bool roleReversed;

	/*
	 * How to go back to parent joiner once this one is done
	 */
	struct ResilientJoiner *parentJoiner;

	/* Next child joiner to recursively invoke */
	struct ResilientJoiner *childJoiner;

	/* The function pointer to join and return the next tuple */
	GetNextJoinedTupleFunc *GetNextJoinedTuple;
} ResilientJoiner;

/*
 * Each spilled partition is abstracted by this. Each nested hash join
 * operation would store an array of spilled partitions, both for build
 * side and the probe side
 */
typedef struct SpilledPartition
{
	/* The workfile that contains the spilled tuples */
	struct ExecWorkFile *workFile;
	/* Total number of tuples in this spilled partition */
	int tupleCount;
	/* The amount of memory required to load this partition in memory */
	Size memorySize;
} SpilledPartition;

/*
 * SpilledPartitionPair
 *
 * A pair of inner and outer spilled partition that can be joined together
 */
typedef struct SpilledPartitionPair
{
    SpilledPartition   *inner;
    SpilledPartition   *outer;
} SpilledPartitionPair;

/* One hash join based sub-joiner (i.e., join one inner sub-partition with one outer sub-partition) */
typedef struct ResilientHashJoiner
{
	/* Pseudo inheritance */
	ResilientJoiner joiner;

	/* What random seed to use for hash function family */
	uint16 seed;

	/* How many buckets to create */
	uint32 numBuckets;

	/* Number of spill files to create as needed */
	uint32 numSpillFiles;

	/*
	 * An array of length numBuckets, where each entry is a pointer
	 * to a linked list of HashBucketTuple. Note, the entry can also
	 * point to a special marker address to indicate that the bucket
	 * has spilled
	 */
	HashBucketTuple **buckets;

	/* Combined tuple size of each bucket; useful for spilling decision */
	Size *bucketSizes;

	/* Combined size of all in-memory tuples; useful for deciding when to spill a bucket to free memory */
	Size inMemoryTupleSize;

	/* Found an inner tuple whose all hashed columns have null values */
	bool foundHashedColumnsAreNull;

	/* The work set to contain all the work files for spilled partitions */
	struct workfile_set * workSet;

	/*
	 * The inner and outer side's spilled partitions. This is an array of size numSpillFiles.
	 * This array would be allocated on-demand as partitions are spilled
	 */
	SpilledPartitionPair **spilled;

	/* The current inner tuple reader for the SimpleJoin */
	struct AbstractTupleReader *bucketReader;

	/*
	 * The hash value for the current outer tuple. This will be updated from the outer
	 * side reader, and read by the inner side hash bucket iterator to find the bucket
	 * to scan
	 */
	HashType outerHashValue;

	/* Which bucket to look into for joining the current outer tuple */
	HashType outerBucketNo;

	/* Context for whole-hash-join storage */
	MemoryContext hashCxt;

	/* How many contexts for buckets? */
	int numBucketCxt;

	/*
	 * An array of memory contexts. There will be numBucketCxt contexts. When
	 * we need to spill, we spill a group of buckets that belong to the largest
	 * memory context
	 */
	MemoryContext *bucketCxt;

	/* Context for allocating work file related memory */
	MemoryContext workFileCxt;
	/*
	 * Which partition to process next. Once the join is done, this index would track the
	 * next spill file to process
	 */
	HashType nextPartitionIdx;

	/* The current set of partition Ids that the child joiner is processing */
	List *partitionIds;

	/*
	 * Without role reversal this should point to the shared innerHashComputer in
	 * TupleHashComputer pointer in ResilientJoinState. However, if role reversal
	 * is active, this should point to outerHashComputer of ResilientJoinState
	 */
	TupleHashComputer *innerHashComputer;

	/*
	 * Without role reversal this should point to the shared outerHashComputer in
	 * TupleHashComputer pointer in ResilientJoinState. However, if role reversal
	 * is active, this should point to innerHashComputer of ResilientJoinState
	 */
	TupleHashComputer *outerHashComputer;

} ResilientHashJoiner;

/* A nested loop (sub)-joiner */
typedef struct ResilientNestedLoopJoiner
{
	/* Pseudo inheritance */
	ResilientJoiner joiner;
} ResilientNestedLoopJoiner;

extern struct ResilientJoiner* ResilientJoiner_GetRootJoiner(struct ResilientJoinState *joinerState, JoinAlgorithm algo,
		PlanState *innerPlanState, PlanState *outerPlanState, double planRows, int planWidth, Size memoryQuota);
extern void ResilientHashJoiner_CleanupSpillFile(ResilientHashJoiner *hashJoiner, int spillFileIdx, InputSide inputSide);

extern HashBucketTuple *spillMarkerAddress;
#define IS_SPILLED_BUCKET(bucket) (bucket == spillMarkerAddress)

#define HASH_TABLE_METADATA_OVERHEAD(bucketCount) (bucketCount * sizeof(HashBucketTuple*))

#endif   /* EXECRESILIENTJOIN_H */
