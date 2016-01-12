/*-------------------------------------------------------------------------
 *
 * nodeResilientJoin.c
 *	  A resilient joiner that supports switching between multiple join types and recursive sub-joins
 *
 * Portions Copyright (c) 2015, Pivotal Inc.
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "inttypes.h"
#include "executor/execResilientJoin.h"
#include "executor/nodeResilientJoin.h"
#include "executor/execAbstractReader.h"
#include "executor/execTupleHashComputer.h"
#include "utils/workfile_mgr.h"
#include "utils/memutils.h"
#include "utils/murmurhash3.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbexplain.h"
#include "inttypes.h"

/* Constant to indicate that we should spill largest bucket */
#define SPILL_LARGEST_BUCKETS 0
/* Constant to indicate that we should spill largest memory context */
#define SPILL_LARGEST_MEMORY_CONTEXT 1
/* Failure to reduce hash table size by at least this factor may trigger a nested loop join */
#define NLJ_SWITCHING_FACTOR 0.8

/* Guc to control the current spill strategy */
int resilientjoin_spill_strategy = SPILL_LARGEST_BUCKETS;

static TupleTableSlot* DelegateToNextStage(ResilientJoiner *resilientJoiner);
static TupleTableSlot* DelegateGetNextToParent(ResilientJoiner *joiner, bool releaseSimpleJoin);
static TupleTableSlot* DelegateGetNextToChild(ResilientJoiner *childJoiner);
static TupleTableSlot* DelegateGetNextToProbe(ResilientJoiner *joiner);
static TupleTableSlot* DelegateGetNextToSpillProcessor(ResilientJoiner *joiner);
static TupleTableSlot* DelegateGetNextToNestedLoop(ResilientJoiner *resilientJoiner);

static ResilientJoiner* BuildResilientJoiner(ResilientJoinState *joinerState, ResilientJoiner *parentJoiner,
		JoinAlgorithm joinAlgorithm, AbstractTupleReader *innerReader,
		AbstractTupleReader *outerReader, double expectedTupCount, int expectedTupWidth,
		Size memoryQuota, bool roleReversed);
static TupleTableSlot* ExecuteHashJoinBuildStage(ResilientJoiner *hashJoiner);
static TupleTableSlot* ExecuteJoin(ResilientJoiner *hashJoiner);
static TupleTableSlot* ExecuteHashJoinSpillProcessor(ResilientJoiner *resilientJoiner);
static void CreateWorkSet(ResilientHashJoiner* hashJoiner);
static void CreateWorkFile(workfile_set * workSet, ExecWorkFile **workFile, MemoryContext memoryContext);
static void SaveTupleToSpillFile(SpilledPartition *spilledPartition, MemTuple tuple);

static void PostBuildCleanup(ResilientHashJoiner *hashJoiner);
static void PostProbeCleanup(ResilientHashJoiner *hashJoiner);
static void CleanupWorkFiles(ResilientHashJoiner *hashJoiner);
static void PostSpillCleanup(ResilientHashJoiner *hashJoiner);
static void CleanupResilientJoiner(ResilientJoiner *joiner);
static void ChooseHashTableSize(bool isPrecise, uint64 tupCount, int tupWidth, Size memoryQuota, int *numBuckets, int *numBatches);
static void CollectResilientHashJoinerBuildStats(ResilientHashJoiner *hashJoiner);

/* Helper function to spill an outer tuple */
extern void ResilientHashJoiner_SpillOuterTuple(ResilientHashJoiner *hashJoiner, int bucketNo, MemTuple tuple, HashType hashValue);

/* A marker to indicate that hash bucket has spilled */
static HashBucketTuple spillMarker;
/* We mark a bucket as spilled by pointing the bucket head to the spillMarkerAddress */
HashBucketTuple *spillMarkerAddress = &spillMarker;

/********************************************************************************
 *
 * ******************** PUBLIC METHODS ******************************************
 */

/*
 * Factory method to create the first joiner (i.e., root joiner).
 */
ResilientJoiner*
ResilientJoiner_GetRootJoiner(ResilientJoinState *joinerState, JoinAlgorithm algo, PlanState *innerPlanState, PlanState *outerPlanState,
		double planRows, int planWidth, Size memoryQuota)
{
	/* Make sure we have a valid ResilientJoinState pointer */
	ValidateJoinState(joinerState);

	/*
	 * We must start with HashJoin to support LASJ_NOTIN, as we only check for a
	 * null join keys during hash build phase. In the future we may extend SimpleJoin
	 * to keep track of null join key values on the inner side for a NLJ root
	 */
	AssertImply(joinerState->sjs.js.jointype == JOIN_LASJ_NOTIN, algo == JoinAlgorithm_HashJoin);

	/* The inner reader should output its tuple into ecxt_innertuple. Similarly, outer reader should do so in ecxt_outertuple */
	AbstractTupleReader *innerReader = CreateOperatorTupleReader(innerPlanState, &joinerState->sjs.js.ps.ps_ExprContext->ecxt_innertuple);
	AbstractTupleReader *outerReader = CreateOperatorTupleReader(outerPlanState, &joinerState->sjs.js.ps.ps_ExprContext->ecxt_outertuple);

	ValidateAbstractReaderPair(innerReader, outerReader);

	/* The root joiner doesn't have any parent joiner. So, set the parentJoiner to NULL */
	return BuildResilientJoiner(joinerState, NULL /* parentJoiner */, algo, innerReader,
		outerReader, planRows, planWidth, memoryQuota, false /* role is not reversed */);
}

/*
 * Spill an outer tuple.
 *
 * We assume that the bucket is already spilled
 */
void
ResilientHashJoiner_SpillOuterTuple(ResilientHashJoiner *hashJoiner, int bucketNo, MemTuple tuple, HashType hashValue)
{
	/* Verify that the corresponding bucket is already spilled */
	Assert(IS_SPILLED_BUCKET(hashJoiner->buckets[bucketNo]));
	Assert (NULL != hashJoiner->spilled);

	int spillFileIdx = bucketNo % hashJoiner->numSpillFiles;

	/*
	 * Verify that the spill files are already created. Note, we create spill file for a
	 * bucket when we first flag that bucket as spilled and the corresponding spill file
	 * is not already created
	 */
	Assert (NULL != hashJoiner->spilled[spillFileIdx]);
	Assert(NULL != hashJoiner->spilled[spillFileIdx]->inner);

	/* We need to create the outer spill file for the very first spilled outer tuple */
	if (NULL == hashJoiner->spilled[spillFileIdx]->outer)
	{
		hashJoiner->spilled[spillFileIdx]->outer = MemoryContextAllocZero(hashJoiner->workFileCxt, sizeof(SpilledPartition));
		CreateWorkFile(hashJoiner->workSet, &hashJoiner->spilled[spillFileIdx]->outer->workFile, hashJoiner->workFileCxt);
	}

	Assert(NULL != hashJoiner->spilled[spillFileIdx]->outer->workFile);
	SaveTupleToSpillFile(hashJoiner->spilled[spillFileIdx]->outer, tuple);
}

/* Clean up a given spill file (either inner or outer side) */
void
ResilientHashJoiner_CleanupSpillFile(ResilientHashJoiner *hashJoiner, int spillFileIdx, InputSide inputSide)
{
	Assert(NULL != hashJoiner->spilled);
	SpilledPartitionPair *spillPair = hashJoiner->spilled[spillFileIdx];

	if (NULL != spillPair)
	{
		SpilledPartition **spilledPartition = NULL;
		if (InputSide_Inner == inputSide)
		{
			spilledPartition = &spillPair->inner;
		}
		else if (InputSide_Outer == inputSide && NULL != spillPair->outer)
		{
			spilledPartition = &spillPair->outer;
		}

		if (NULL != *spilledPartition)
		{
			workfile_mgr_close_file(hashJoiner->workSet, (*spilledPartition)->workFile);
			pfree(*spilledPartition);
			*spilledPartition = NULL;
		}

		/* If both inner and outer are freed, cleanup the spill pair data structure */
		if (NULL == spillPair->inner && NULL == spillPair->outer)
		{
			pfree(spillPair);
			hashJoiner->spilled[spillFileIdx] = NULL;
		}
	}
}

/***********************************************************************************
 *
 * ******************** PRIVATE (STATIC) METHODS ***********************************
 */

/*
 * Central state transition gateway to decide which
 * next function to call
 */
static TupleTableSlot*
DelegateToNextStage(ResilientJoiner *resilientJoiner)
{
	/* Ensure that we have an instance of one of the ResilientJoiner sub-classes */
	ValidateJoinerPointer(resilientJoiner);

	Assert(JoinStage_Uninitialized != resilientJoiner->stage);

	switch (resilientJoiner->stage)
	{
		case JoinStage_PreBuild:
			resilientJoiner->stage = JoinStage_Build;
			/*
			 * Fall through, as we don't have any state transition
			 * action for PreBuild -> Build
			 */
		case JoinStage_Build:
			resilientJoiner->stage = JoinStage_Probe;
			return ExecuteHashJoinBuildStage(resilientJoiner);
		case JoinStage_Probe:
			PostBuildCleanup((ResilientHashJoiner *) resilientJoiner);
			resilientJoiner->stage = JoinStage_ProcessSpilled;
			return DelegateGetNextToProbe(resilientJoiner);
		case JoinStage_ProcessSpilled:
			PostProbeCleanup((ResilientHashJoiner *) resilientJoiner);
			resilientJoiner->stage = JoinStage_ProcessSpillChild;
			return DelegateGetNextToSpillProcessor(resilientJoiner);
		case JoinStage_ProcessSpillChild:
			if (NULL != resilientJoiner->childJoiner)
			{
				return DelegateGetNextToChild(resilientJoiner->childJoiner);
			}
			/* All the children are done */
			resilientJoiner->stage = JoinStage_SubHashJoinDone;

			/* Fall through to JoinStage_SubHashJoinDone */
		case JoinStage_SubHashJoinDone:
			PostSpillCleanup((ResilientHashJoiner *) resilientJoiner);
			return DelegateGetNextToParent(resilientJoiner, false /* releaseSimpleJoin */);

		case JoinStage_NestedLoop:
			resilientJoiner->stage = JoinStage_SubNestedLoopJoinDone;
			return DelegateGetNextToNestedLoop(resilientJoiner);

		case JoinStage_SubNestedLoopJoinDone:
			SimpleJoin_EndJoin(&resilientJoiner->joinerState->sjs, resilientJoiner);
			return DelegateGetNextToParent(resilientJoiner, true /* releaseSimpleJoin */);

		default:
			Insist(!"Unknown joiner stage");
	}
}

/*
 * Return the next tuple from the parent join operator and
 * set the current joiner to the parent so that the next get next
 * call is redirected to the parent joiner
 */
static TupleTableSlot*
DelegateGetNextToParent(ResilientJoiner *joiner, bool releaseSimpleJoin)
{
	ValidateJoinerPointer(joiner);

	if (releaseSimpleJoin)
	{
		SimpleJoin_EndJoin(&joiner->joinerState->sjs, joiner);
	}

	/*
	 * After cleanup we won't have a valid joiner pointer. Therefore,
	 * ensure that we don't access joiner pointer anymore
	 */
	ResilientJoiner *parentJoiner = joiner->parentJoiner;

	/*
	 * Update the current joiner back to parent joiner. So, next time the call
	 * would be redirected to that joiner, which should by then have set the current
	 * joiner to another recursive sub-joiner
	 */
	joiner->joinerState->curJoiner = parentJoiner;

	CleanupResilientJoiner(joiner);

	/* We are done joining all the partitions and sub-partitions! */
	if (NULL == parentJoiner)
	{
		return NULL;
	}
	return parentJoiner->GetNextJoinedTuple(parentJoiner);
}

/*
 * Return the next tuple from the child join operator and
 * set the current joiner to the child so that the next get next
 * call is redirected to the child joiner
 */
static TupleTableSlot*
DelegateGetNextToChild(ResilientJoiner *childJoiner)
{
	ValidateJoinerPointer(childJoiner);

	/*
	 * Update the current joiner to childJoiner so that next time the call
	 * would be redirected to that joiner
	 */
	childJoiner->joinerState->curJoiner = childJoiner;
	return childJoiner->GetNextJoinedTuple(childJoiner);
}

/*
 * Set the next method to the "probe" so that any subsequent "next"
 * call at the global joiner is redirected to probe directly. Also
 * return the next successfully probed tuple
 */
static TupleTableSlot*
DelegateGetNextToProbe(ResilientJoiner *resilientJoiner)
{
	ValidateJoinerPointer(resilientJoiner);

	ResilientHashJoiner *hashJoiner = (ResilientHashJoiner *)resilientJoiner;

	if (NULL != resilientJoiner->joinerState->joinStats)
	{
		CollectResilientHashJoinerBuildStats(hashJoiner);
	}


	TupleHashComputer_Begin(hashJoiner->outerHashComputer, resilientJoiner, hashJoiner->seed);

	/*
	 * Grab ownership of the singleton SimpleJoin to execute join operation
	 * once per outer tuple
	 */
	SimpleJoin_BeginJoin(&resilientJoiner->joinerState->sjs, resilientJoiner,
			resilientJoiner->outerReader, hashJoiner->bucketReader);

	resilientJoiner->GetNextJoinedTuple = ExecuteJoin;
	return ExecuteJoin(resilientJoiner);
}

/*
 * Set the next method to the "spill processor" so that any subsequent "next"
 * call at the global joiner is redirected to the spill processor method directly.
 * Also return the next successfully processed spilled tuple. Note, "successfully
 * processed spill tuple" may come from a recursive sub-join that was able to
 * join a spilled probe tuple with a spilled build tuple.
 */
static TupleTableSlot*
DelegateGetNextToSpillProcessor(ResilientJoiner *resilientJoiner)
{
	ValidateJoinerPointer(resilientJoiner);

	TupleHashComputer_End(((ResilientHashJoiner *)resilientJoiner)->outerHashComputer, resilientJoiner);
	SimpleJoin_EndJoin(&resilientJoiner->joinerState->sjs, resilientJoiner);

	resilientJoiner->GetNextJoinedTuple = ExecuteHashJoinSpillProcessor;
	return ExecuteHashJoinSpillProcessor(resilientJoiner);
}

/*
 * Set the next method to the NLJ's ExecuteNestedLoopJoin  so that any subsequent "next"
 * call at the global joiner is redirected to it directly. Also return the next joined tuple
 */
static TupleTableSlot*
DelegateGetNextToNestedLoop(ResilientJoiner *resilientJoiner)
{
	Assert(IsA(resilientJoiner, ResilientNestedLoopJoiner));

	/*
	 * Grab ownership of the singleton SimpleJoin to execute join operation
	 * once per outer tuple
	 */
	SimpleJoin_BeginJoin(&resilientJoiner->joinerState->sjs, resilientJoiner,
			resilientJoiner->outerReader, resilientJoiner->innerReader);

	resilientJoiner->GetNextJoinedTuple = ExecuteJoin;
	return ExecuteJoin(resilientJoiner);
}

/*
 * Returns true if the roles were reversed (only if the global join config permits such role reversal).
 *
 * 		memoryQuota: Try best to fit partitionIds in the memoryQuota. If none of the partitionId
 * 		fits in the memoryQuota, we return *at least one* partition id. If memoryQuota is set to 0
 * 		we assume unlimited memory (e.g., NLJ may process all the partitions at once)
 *
 * 		roleReversalAllowed: Is role reversal allowed (e.g., not an outer join)
 *
 * 		isAlreadyReversed: Whether the role of the current joiner is already reversed. Depending on
 * 		that we may flip back to the original role order (role is always relative to the original
 * 		optimizer provided role)
 *
 *		buildTupleCount: The tuple count of the build side after partition tuning (for precise stats)
 *
 *		buildTupleWidth: The avg tuple width of the build side after partition tuning (for precise stats)
 *
 *		Returns true if the role was reversed with respect to the original ordering of the optimizer
 */
static bool
ChooseNextSetOfSpillPartitions(ResilientHashJoiner *hashJoiner, Size memoryQuota, bool roleReversalAllowed,
		bool isAlreadyReversed, double *buildTupleCount, int *buildTupleWidth)
{
	list_free(hashJoiner->partitionIds);
	hashJoiner->partitionIds = NULL;

	if (NULL == hashJoiner->spilled || hashJoiner->nextPartitionIdx >= hashJoiner->numSpillFiles)
	{
		*buildTupleCount = 0;
		*buildTupleWidth = 0;
		return false;
	}

	int partIdx = 0;
	Size innerSize = 0;
	Size innerCount = 0;

	Size outerSize = 0;
	Size outerCount = 0;

	List *partitionIds = NULL;

	/* Do partition tuning and return a list of partition ids */
	for (partIdx = hashJoiner->nextPartitionIdx; partIdx < hashJoiner->numSpillFiles; partIdx++)
	{
		if (NULL != hashJoiner->spilled[partIdx] &&
				NULL != hashJoiner->spilled[partIdx]->outer &&
				(IS_SIMPLE_OUTER_JOIN(hashJoiner->joiner.joinerState->sjs.js.jointype) ||
						NULL != hashJoiner->spilled[partIdx]->inner))
		{
			/* Calculate the new size of the build and probe if we are to include the next partition pair */
			Size newInnerSize = innerSize + hashJoiner->spilled[partIdx]->inner->memorySize;
			Size newInnerCount = innerCount + hashJoiner->spilled[partIdx]->inner->tupleCount;

			Size newOuterSize = outerSize + hashJoiner->spilled[partIdx]->outer->memorySize;
			Size newOuterCount = outerCount + hashJoiner->spilled[partIdx]->outer->tupleCount;

			Size minCount = newInnerCount;
			Size sizeOfSmallerSide = newInnerSize;

			if (roleReversalAllowed && newOuterCount < newInnerCount)
			{
				minCount = newOuterCount;
				sizeOfSmallerSide = newOuterSize;
			}

			/*
			 * Fit as many partitions in memory as possible, after role reversal, if permitted
			 * (0 memoryQuota means unlimited)
			 */
			if (memoryQuota != 0 && sizeOfSmallerSide > memoryQuota && (innerCount > 0 || outerCount > 0))
			{
				/*
				 * We are excluding this partIdx from current tuning. So, the next tuning should
				 * begin from this partIdx
				 */
				--partIdx;
				break;
			}

			partitionIds = lcons_int(partIdx, partitionIds);

			innerSize = newInnerSize;
			innerCount = newInnerCount;

			outerSize = newOuterSize;
			outerCount = newOuterCount;

			/* Disable partition tuning */
			if (!enable_resilient_join_partition_tuning)
			{
				break;
			}
		}
	}

	bool roleReversed = false;
	double buildCount = innerCount;
	double buildSize = innerSize;

	if (roleReversalAllowed && innerCount > outerCount)
	{
		buildCount = outerCount;
		buildSize = outerSize;
		/*
		 * Role was reversed for only current case. If the parent is also reversed,
		 * we may need to return false for final role reversal status (with respect
		 * to the original optimizer ordering)
		 */
		roleReversed = true;
	}

	*buildTupleCount = buildCount;
	/* tuple width calculation should not include hash bucket tuple overhead */
	*buildTupleWidth = ceil(((double)(buildSize - buildCount * HASH_BUCKET_MEMORY_INFLATION)) /
			((double)buildCount));

	hashJoiner->partitionIds = partitionIds;
	hashJoiner->nextPartitionIdx = partIdx + 1;

	/* A null set of partitions means we exhausted our options */
	AssertImply(NULL == hashJoiner->partitionIds, hashJoiner->nextPartitionIdx == hashJoiner->numSpillFiles + 1);

	/*
	 * Role reversal is relative to the global config. That means if the parent is already
	 * reversed and we reverse again, we are no longer reversed. Moreover, if parent was
	 * already reversed, and we didn't reverse, we need to preserve parent's reversal
	 */
	return isAlreadyReversed ^ roleReversed;
}

/*
 * Swap inner and outer output slot (i.e., ecxt_innertuple and ecxt_outertuple) depending
 * on the role reversal status
 */
static inline TupleTableSlot**
GetRoleReversedOutputSlot(ResilientJoinState *joinerState, bool roleReversed, InputSide inputSide)
{
	TupleTableSlot **nextinnerOutputSlot = &joinerState->sjs.js.ps.ps_ExprContext->ecxt_innertuple;
	TupleTableSlot **nextOuterOutputSlot = &joinerState->sjs.js.ps.ps_ExprContext->ecxt_outertuple;

	if (inputSide == InputSide_Inner)
	{
		return roleReversed ? nextOuterOutputSlot : nextinnerOutputSlot;
	}
	else
	{
		return roleReversed ? nextinnerOutputSlot : nextOuterOutputSlot;
	}
}

/*
 * Swap inner and outer staging slot (tuple slot to wrap a MemTuple to a TupleTableSlot) depending
 * on the role reversal status
 */
static inline TupleTableSlot*
GetRoleReversedStagingSlot(ResilientJoinState *joinerState, bool roleReversed, InputSide inputSide)
{
	TupleTableSlot *innerStagingSlot = joinerState->innerTupleSlot;
	TupleTableSlot *outerStagingSlot = joinerState->outerTupleSlot;

	if (inputSide == InputSide_Inner)
	{
		return roleReversed ? outerStagingSlot : innerStagingSlot;
	}
	else
	{
		return roleReversed ? innerStagingSlot : outerStagingSlot;
	}
}

/* Adjust the spill file side based on role reversal status of the parent and child sub-joiner */
static InputSide
GetRoleReversedSpillReadingSide(bool childRoleReversed, bool parentRoleReversed, InputSide inputSide)
{
	if (childRoleReversed == parentRoleReversed)
	{
		return inputSide;
	}

	return inputSide == InputSide_Inner ? InputSide_Outer : InputSide_Inner;
}

/*
 * Find the next partition tuned SpilledTupleReader for both inner and outer.
 *
 * 0 memory quota means unlimited memory (i.e., return all remaining partitions in one bundle)
 */
static bool
GetNextSpilledTupleReader(ResilientHashJoiner *hashJoiner,
		Size memoryQuota, AbstractTupleReader **innerReader, AbstractTupleReader **outerReader, double *innerTupleCount, int *innerTupleWidth)
{
	ResilientJoinState *joinerState = hashJoiner->joiner.joinerState;

	bool roleReversed = ChooseNextSetOfSpillPartitions(hashJoiner, memoryQuota, joinerState->roleReversalAllowed,
			hashJoiner->joiner.roleReversed, innerTupleCount, innerTupleWidth);

	if (NULL == hashJoiner->partitionIds)
	{
		*innerReader = NULL;
		*outerReader = NULL;

		return false;
	}

	/* We only create tuple readers that can read from spill file. However, once these readers are passed on
	 * to the sub-joiner, the sub-joiner may need respilling support for the probe side and therefore, it may
	 * wrap our passed probe tuple reader in a SpillableProbeTupleReader
	 */
	*innerReader = CreateSpilledTupleReader(hashJoiner, GetRoleReversedOutputSlot(joinerState, roleReversed, InputSide_Inner),
			GetRoleReversedStagingSlot(joinerState, roleReversed, InputSide_Inner),
			GetRoleReversedSpillReadingSide(roleReversed, hashJoiner->joiner.roleReversed, InputSide_Inner), hashJoiner->partitionIds);

	*outerReader = CreateSpilledTupleReader(hashJoiner, GetRoleReversedOutputSlot(joinerState, roleReversed, InputSide_Outer),
			GetRoleReversedStagingSlot(joinerState, roleReversed, InputSide_Outer),
			GetRoleReversedSpillReadingSide(roleReversed, hashJoiner->joiner.roleReversed, InputSide_Outer), hashJoiner->partitionIds);

	return roleReversed;
}

/*
 * Initializes the common properties (i.e., base-class) of different joiners
 */
static void
InitializeResilientJoiner(ResilientJoiner *joiner, ResilientJoinState *joinerState,
		JoinAlgorithm joinAlgorithm, JoinStage joinStage, ResilientJoiner *parentJoiner, AbstractTupleReader *innerReader,
		AbstractTupleReader *outerReader, double expectedTupCount,
		int expectedTupWidth, Size memoryQuota, bool roleReversed)
{
	ValidateJoinerPointer(joiner);
	ValidateJoinState(joinerState);
	ValidateAbstractReaderPair(innerReader, outerReader);

	joiner->joinerId = joinerState->numSubJoin++;
	joiner->parentJoiner = parentJoiner;
	joiner->GetNextJoinedTuple = DelegateToNextStage;
	joiner->joinerState = joinerState;
	joiner->stage = joinStage;
	joiner->innerReader = innerReader;
	joiner->outerReader = outerReader;
	joiner->expectedTupCount = expectedTupCount;
	joiner->expectedTupWidth = expectedTupWidth;
	joiner->memoryQuota = memoryQuota;
	joiner->roleReversed = roleReversed;

	if (NULL != joinerState->joinStats)
	{
		/* Ensure that the sub join stat array has a new entry reserved for this sub-join stats */
		InitStatArrayEntry(joiner, joinerState, joinAlgorithm);
	}
}

/* Builds a hash joiner using the inner and outer reader */
static ResilientJoiner*
BuildResilientHashJoiner(ResilientJoinState *joinerState, ResilientJoiner *parentJoiner,
		AbstractTupleReader *innerReader, AbstractTupleReader *outerReader, double expectedTupCount,
		int expectedTupWidth, Size memoryQuota, bool roleReversed)
{
	ResilientHashJoiner *hashJoiner = makeNode(ResilientHashJoiner);

	/* Hash join seed starts at 0 */
	int seed = 0;

	/* For each recursive level we add 1 to the seed */
	if (NULL != parentJoiner && IsA(parentJoiner, ResilientHashJoiner))
	{
		ResilientHashJoiner *parentHashJoiner = (ResilientHashJoiner *) parentJoiner;
		seed = parentHashJoiner->seed + 1;
	}

	/* Initialize base class properties */
	InitializeResilientJoiner(&hashJoiner->joiner, joinerState, JoinAlgorithm_HashJoin, JoinStage_PreBuild, parentJoiner,
			innerReader, outerReader, expectedTupCount, expectedTupWidth, memoryQuota, roleReversed);

	/* Swap the tuple hash computer (with respect to optimizer generated input side) if role reversal is active */
	if (roleReversed)
	{
		hashJoiner->innerHashComputer = joinerState->outerHashComputer;
		hashJoiner->outerHashComputer = joinerState->innerHashComputer;
	}
	else
	{
		hashJoiner->innerHashComputer = joinerState->innerHashComputer;
		hashJoiner->outerHashComputer = joinerState->outerHashComputer;
	}

	/*
	 * Probe reader must be able to support spilling for probe tuples whose buckets
	 * were spilled during build phase. The SpillableProbeTupleReader equips
	 * an abstract reader with spilling functionality
	 *
	 * TODO: Bypass SpillableProbeTupleReader (optimization) if no spilling
	 */
	AbstractTupleReader *probeReader = CreateSpillableProbeTupleReader(hashJoiner, outerReader);
	/* Swap out the outerReader with the decorator probeReader so that we can support spilling */
	hashJoiner->joiner.outerReader = probeReader;

	/*
	 * Create temporary memory contexts in which to keep the hashtable working
	 * storage.  See notes in executor/hashjoin.h.
	 */
	hashJoiner->hashCxt = AllocSetContextCreate(CurrentMemoryContext,
											   "HashTableContext",
											   ALLOCSET_DEFAULT_MINSIZE,
											   ALLOCSET_DEFAULT_INITSIZE,
											   ALLOCSET_DEFAULT_MAXSIZE);

	/* A memory context to allocate all the spill file metadata */
	hashJoiner->workFileCxt = AllocSetContextCreate(CurrentMemoryContext,
											   "WorkFileContext",
											   ALLOCSET_DEFAULT_MINSIZE,
											   ALLOCSET_DEFAULT_INITSIZE,
											   ALLOCSET_DEFAULT_MAXSIZE);

	hashJoiner->seed = seed;

	int numBuckets = 0;
	int numSpillFiles = 0;
	/* Calculate buckets and number of spill files */
	ChooseHashTableSize(seed > 0 /* precise stat? */, expectedTupCount, expectedTupWidth, memoryQuota, &numBuckets, &numSpillFiles);

	hashJoiner->numBuckets = numBuckets;
	hashJoiner->numSpillFiles = numSpillFiles;

	hashJoiner->buckets = MemoryContextAllocZero(hashJoiner->hashCxt, sizeof(HashBucketTuple *) * numBuckets);
	hashJoiner->bucketSizes = MemoryContextAllocZero(hashJoiner->hashCxt, sizeof(Size) * numBuckets);
	hashJoiner->foundHashedColumnsAreNull = false;
	hashJoiner->spilled = NULL;

	/*
	 * For now 200 fixed memory context. Ideally, it should be granular enough
	 * to find a set of contexts to reset to free up certain percentage of memory
	 *
	 * TODO: Configurable number of memory contexts either through guc or
	 * derived from stats and memory quota
	 */
	hashJoiner->numBucketCxt = 200;

	hashJoiner->bucketCxt = MemoryContextAlloc(hashJoiner->hashCxt, sizeof(MemoryContext) * hashJoiner->numBucketCxt);

	/*
	 * Allocate an array of memory conexts to bulk free (i.e., bulk spill) a bunch of
	 * hash buckets. This can be thought of a logically connected set of buckets, where
	 * resetting the largest memory context would spill the logical groups of buckets
	 * that share that memory context
	 */
	for (int i = 0; i < hashJoiner->numBucketCxt; i++)
	{
		hashJoiner->bucketCxt[i] = AllocSetContextCreate(hashJoiner->hashCxt,
				   "BucketContext",
				   ALLOCSET_DEFAULT_MINSIZE,
				   ALLOCSET_DEFAULT_INITSIZE,
				   ALLOCSET_DEFAULT_MAXSIZE);
	}

	/* Create a bucket reader that can filter unjoinable inner tuples for the current outer tuple */
	hashJoiner->bucketReader = CreateHashBucketTupleReader(hashJoiner, GetRoleReversedOutputSlot(joinerState, roleReversed, InputSide_Inner),
			GetRoleReversedStagingSlot(joinerState, roleReversed, InputSide_Inner));

	return (ResilientJoiner *)hashJoiner;
}

/* Builds a nested loop joiner using the inner and outer reader */
static ResilientJoiner*
BuildResilientNestedLoopJoiner(ResilientJoinState *joinerState, ResilientJoiner *parentJoiner,
		AbstractTupleReader *innerReader, AbstractTupleReader *outerReader, double expectedTupCount,
		int expectedTupWidth, Size memoryQuota, bool roleReversed)

{
	ResilientNestedLoopJoiner *nlJoiner = makeNode(ResilientNestedLoopJoiner);

	InitializeResilientJoiner(&nlJoiner->joiner, joinerState, JoinAlgorithm_NLJ, JoinStage_NestedLoop, parentJoiner,
			innerReader, outerReader, expectedTupCount, expectedTupWidth, memoryQuota, roleReversed);

	return (ResilientJoiner *) nlJoiner;
}

/*
 * A generic builder that can build both hash join and nested loop join instance
 * using a inner and an outer reader
 */
static ResilientJoiner*
BuildResilientJoiner(ResilientJoinState *joinerState, ResilientJoiner *parentJoiner,
		JoinAlgorithm joinAlgorithm, AbstractTupleReader *innerReader,
		AbstractTupleReader *outerReader, double expectedTupCount, int expectedTupWidth,
		Size memoryQuota, bool roleReversed)
{
	ValidateJoinState(joinerState);
	ValidateAbstractReaderPair(innerReader, outerReader);

	switch (joinAlgorithm)
	{
		case JoinAlgorithm_HashJoin:
			return BuildResilientHashJoiner(joinerState, parentJoiner, innerReader, outerReader,
					expectedTupCount, expectedTupWidth, memoryQuota, roleReversed);
		case JoinAlgorithm_NLJ:
			return BuildResilientNestedLoopJoiner(joinerState, parentJoiner, innerReader, outerReader,
					expectedTupCount, expectedTupWidth, memoryQuota, roleReversed);
		default:
			Insist(!"Unsupported join algorithm");
	}

	return NULL;
}

/*
 * Returns the maximum size of the spilled partitions (useful for determining
 * NLJ switching
 */
static Size
GetMaxSpilledPartitionSize(ResilientJoiner *joiner)
{
	Assert(!"Not yet implemented");

	if (!IsA(joiner, ResilientHashJoiner))
	{
		return 0;
	}

	/* TODO: find the maximum size of the spilled partitions */
	return 0;
}

/* Determines the join algorithm suitable for spill file processing */
static JoinAlgorithm
DetermineJoinAlgorithm(ResilientHashJoiner *hashJoiner)
{
	ResilientJoiner *joiner = (ResilientJoiner*)hashJoiner;

	if (NULL == joiner->parentJoiner)
	{
		return JoinAlgorithm_HashJoin;
	}

	/* TODO: Enable NLJ switching */
	if (false /* Disable NLJ switching */ &&
			NLJ_SWITCHING_FACTOR * GetMaxSpilledPartitionSize(joiner->parentJoiner) < GetMaxSpilledPartitionSize(joiner))
	{
		return JoinAlgorithm_NLJ;
	}

	return JoinAlgorithm_HashJoin;
}

/*
 * Prepares a nested join instance on maximum number of spilled partitions that can fit
 * in the memory. If sub-partitioning attempt was unsuccessful in previous iteration
 * it may decide to switch to nested loop join
 */
static ResilientJoiner*
GetNextNestedJoiner(ResilientHashJoiner *hashJoiner)
{
	JoinAlgorithm algo = DetermineJoinAlgorithm(hashJoiner);

	ResilientJoiner *joiner = (ResilientJoiner *)hashJoiner;

	Size partitionTunerMemoryQuota = joiner->joinerState->memoryQuota;

	if (algo == JoinAlgorithm_NLJ)
	{
		/* Unlimited memory for partition tuning (i.e., choose all partition at once) */
		partitionTunerMemoryQuota = 0;
	}

	AbstractTupleReader *innerReader = NULL;
	AbstractTupleReader *outerReader = NULL;

	double innerTupleCount = 0.0;
	int innerTupleWidth = 0;

	bool roleReversed = GetNextSpilledTupleReader(hashJoiner, partitionTunerMemoryQuota, &innerReader,
			&outerReader, &innerTupleCount, &innerTupleWidth);

	/* We exhausted all the spill partitions */
	if (NULL == innerReader || NULL == outerReader)
	{
		AssertImply(NULL == innerReader, NULL == outerReader);
		return NULL;
	}
	else
	{
		return BuildResilientJoiner(hashJoiner->joiner.joinerState, joiner, algo, innerReader,
			outerReader, innerTupleCount, innerTupleWidth, hashJoiner->joiner.joinerState->memoryQuota, roleReversed);
	}
}

/* Creates a work set to contain all the work files for handling spilling */
static void
CreateWorkSet(ResilientHashJoiner* hashJoiner)
{
	Assert(NULL == hashJoiner->workSet);
	MemoryContext oldcxt = MemoryContextSwitchTo(hashJoiner->workFileCxt);
	hashJoiner->spilled = MemoryContextAllocZero(hashJoiner->workFileCxt,
			sizeof(SpilledPartitionPair *) * hashJoiner->numSpillFiles);
	hashJoiner->workSet = workfile_mgr_create_set(gp_workfile_type_hashjoin,
			false /* can_be_reused */, &hashJoiner->joiner.joinerState->sjs.js.ps,
			NULL_SNAPSHOT /* workfile_set_snapshot */);
	MemoryContextSwitchTo(oldcxt);
}

/* Create a work file for a spilled partition */
static void
CreateWorkFile(workfile_set * workSet, ExecWorkFile **workFile, MemoryContext memoryContext)
{
	MemoryContext oldcxt = MemoryContextSwitchTo(memoryContext);
	*workFile = workfile_mgr_create_file(workSet);
	MemoryContextSwitchTo(oldcxt);
	elog(gp_workfile_caching_loglevel, "create work files %s with gp_workfile_compress_algorithm=%d",
			ExecWorkFile_GetFileName(*workFile), workSet->metadata.bfz_compress_type);
}

/* Save a tuple to spill file */
static void
SaveTupleToSpillFile(SpilledPartition *spilledPartition, MemTuple tuple)
{
	Size tupleSize = memtuple_get_size(tuple, NULL);
	if (!ExecWorkFile_Write(spilledPartition->workFile, (void*) tuple, tupleSize))
	{
		workfile_mgr_report_error();
	}

	spilledPartition->tupleCount++;
	spilledPartition->memorySize += INFLATED_HASH_BUCKET_TUPLE_SIZE(tupleSize);
}

/*
 * Spill an inner tuple.
 *
 * We assume that the corresponding bucket is already spilled (i.e., spill files are already created)
 */
static void
SpillInnerTuple(ResilientHashJoiner *hashJoiner, int bucketNo, MemTuple tuple)
{
	int spillFileIdx = bucketNo % hashJoiner->numSpillFiles;

	/* Verify that spill file exists for the corresponding bucket */
	Assert(IS_SPILLED_BUCKET(hashJoiner->buckets[bucketNo]));
	Assert (NULL != hashJoiner->spilled);
	Assert (NULL != hashJoiner->spilled[spillFileIdx]);
	Assert (NULL != hashJoiner->spilled[spillFileIdx]->inner->workFile);

	SaveTupleToSpillFile(hashJoiner->spilled[spillFileIdx]->inner, tuple);
}

/*
 * Spill a hash bucket
 *
 * Returns the number of bytes freed
 */
static Size
SpillBucket(ResilientHashJoiner *hashJoiner, int bucketNo)
{
	HashBucketTuple *curPos = hashJoiner->buckets[bucketNo];
	/* We shouldn't attempt to spill an empty bucket or an already spilled bucket */
	Assert (NULL != curPos);
	Assert(0 != hashJoiner->bucketSizes[bucketNo]);
	Assert(!IS_SPILLED_BUCKET(hashJoiner->buckets[bucketNo]));

	if (NULL == hashJoiner->spilled)
	{
		CreateWorkSet(hashJoiner);
		Assert(NULL != hashJoiner->spilled);
	}

	int spillFileIdx = bucketNo % hashJoiner->numSpillFiles;

	/* If the destination spill file metadata is not already created, create it */
	if (NULL == hashJoiner->spilled[spillFileIdx])
	{
		/* First write to this batch file, so create it */
		Assert(hashJoiner->workSet != NULL);
		hashJoiner->spilled[spillFileIdx] = MemoryContextAllocZero(hashJoiner->workFileCxt, sizeof(SpilledPartitionPair));
		hashJoiner->spilled[spillFileIdx]->inner = MemoryContextAllocZero(hashJoiner->workFileCxt, sizeof(SpilledPartition));
	}

	/* If the destination spill file doesn't exist, create it */
	if (NULL == hashJoiner->spilled[spillFileIdx]->inner->workFile)
	{
		CreateWorkFile(hashJoiner->workSet, &hashJoiner->spilled[spillFileIdx]->inner->workFile, hashJoiner->workFileCxt);
		Assert(NULL != hashJoiner->spilled[spillFileIdx]->inner->workFile);
	}

	/* Spill all tuples in this bucket */
	HashBucketTuple *prevPos = NULL;
	while (NULL != curPos)
	{
		MemTuple tuple = HASH_BUCKET_MINTUPLE(curPos);
		SaveTupleToSpillFile(hashJoiner->spilled[spillFileIdx]->inner, tuple);
		prevPos = curPos;
		curPos = curPos->next;
		pfree(prevPos);
	}

	Size freed = hashJoiner->bucketSizes[bucketNo];
	/* Record the reduction of in memory tuple size */
	hashJoiner->inMemoryTupleSize -= freed;
	/* Mark the bucket as spilled */
	hashJoiner->buckets[bucketNo] = spillMarkerAddress;

	/* Update the bucket size */
	hashJoiner->bucketSizes[bucketNo] = 0;
	Assert(0 <= hashJoiner->inMemoryTupleSize);

	return freed;
}


/* Find the largest bucket */
static int
FindLargestBucketNo(ResilientHashJoiner *hashJoiner)
{
	int largestBucketNo = -1;
	Size largestBucketSize = 0;

	for (int bucketNo = 0; bucketNo < hashJoiner->numBuckets; bucketNo++)
	{
		if (largestBucketSize < hashJoiner->bucketSizes[bucketNo])
		{
			largestBucketNo = bucketNo;
			largestBucketSize = hashJoiner->bucketSizes[bucketNo];
		}
	}

	AssertImply(largestBucketNo >= 0, largestBucketSize > 0);

	return largestBucketNo;
}

/* Spill the largest bucket */
static void
SpillLargestBuckets(ResilientHashJoiner *hashJoiner)
{
	ResilientJoiner *joiner = (ResilientJoiner *) hashJoiner;

	while (hashJoiner->inMemoryTupleSize >= joiner->memoryQuota)
	{
		int largestBucketNo = FindLargestBucketNo(hashJoiner);

		if (largestBucketNo < 0)
		{
			/* No bucket to spill */
			elog(ERROR, "Could not find a bucket to spill");
		}

		/* There should be at least one in-memory bucket that can be spilled */
		Assert(largestBucketNo >= 0);

		SpillBucket(hashJoiner, largestBucketNo);
	}
}

/* Spills top k largest buckets using a heap */
static Size
SpillTopKBuckets(ResilientHashJoiner *hashJoiner)
{
	/*
	 * TODO: introduce guc to control "k" (i.e., number of largest
	 * buckets to find
	 */
	int HEAP_SIZE = 64;

	/* 1 based index; 0 is discarded for quick arithmetic */
	int *minHeap = palloc((HEAP_SIZE + 1) * sizeof(int));

	int nextHeapIndex = 1;
	int bucketNo = 0;

	for (bucketNo = 0; (nextHeapIndex <= HEAP_SIZE) && (bucketNo < hashJoiner->numBuckets); bucketNo++)
	{
		if (hashJoiner->bucketSizes[bucketNo] > 0)
		{
			minHeap[nextHeapIndex] = bucketNo;

			int swimPos = nextHeapIndex;

			/* Swim up until we reach root */
			while (swimPos != 1)
			{
				/* Divide by 2 */
				int parentPos = swimPos >> 1;

				if (hashJoiner->bucketSizes[minHeap[parentPos]] > hashJoiner->bucketSizes[minHeap[swimPos]])
				{
					int swapVal = minHeap[parentPos];
					minHeap[parentPos] = minHeap[swimPos];
					minHeap[swimPos] = swapVal;

					swimPos = parentPos;
				}
				else
				{
					/* Heap order is restored */
					break;
				}
			}

			++nextHeapIndex;
		}
	}

	int heapElemCount = nextHeapIndex - 1;

	/*
	 * If we have more buckets to process in the following for loop, then we should
	 * already have the heap filled
	 */
	AssertImply(bucketNo < hashJoiner->numSpillFiles, heapElemCount == HEAP_SIZE);

	for (; bucketNo < hashJoiner->numBuckets; bucketNo++)
	{
		if (hashJoiner->bucketSizes[minHeap[1]] < hashJoiner->bucketSizes[bucketNo])
		{
			/* Replace the root as we found an larger */
			minHeap[1] = bucketNo;

			/* First element may cause a heap disorder */
			int disorderedIndex = 1;
			int chosenChild = 2 * disorderedIndex;

			while (chosenChild <= heapElemCount)
			{
				if (chosenChild < heapElemCount && hashJoiner->bucketSizes[minHeap[chosenChild]] >
					hashJoiner->bucketSizes[minHeap[chosenChild + 1]])
				{
					++chosenChild;
				}

				if (hashJoiner->bucketSizes[minHeap[disorderedIndex]] > hashJoiner->bucketSizes[minHeap[chosenChild]])
				{
					int swapVal = minHeap[disorderedIndex];
					minHeap[disorderedIndex] = minHeap[chosenChild];
					minHeap[chosenChild] = swapVal;

					disorderedIndex = chosenChild;
					chosenChild = 2 * disorderedIndex;
				}
				else
				{
					/* Heap order is restored */
					break;
				}
			}
		}
	}

	Size freed = 0;

	/*
	 * Try to free 10% memory.
	 * TODO: introduce guc to control percentage of memory to free
	 */
	Size target = hashJoiner->inMemoryTupleSize * 0.1;

	for (int i = heapElemCount; i >= 1; i--)
	{
		int freeIndex = minHeap[i];

		freed += SpillBucket(hashJoiner, freeIndex);

		if (freed >= target)
		{
			break;
		}
	}

	pfree(minHeap);
	return freed;
}

/*
 * Finds the group of buckets that share the largest memory context and
 * spills all of them
 */
static Size
SpillBucketsInLargestMemoryContext(ResilientHashJoiner *hashJoiner)
{
	Size largestMemCxtSize = 0;
	int memCxtIdx = -1;

	for (int i = 0; i < hashJoiner->numBucketCxt; i++)
	{
		MemoryContext cxt = hashJoiner->bucketCxt[i];

		Size allocSize = cxt->allBytesAlloc - cxt->allBytesFreed;
		if (allocSize > largestMemCxtSize)
		{
			largestMemCxtSize = allocSize;
			memCxtIdx = i;
		}
	}

	Assert(memCxtIdx >= 0);

	Size freed = 0;

	for (int bucketNo = memCxtIdx; bucketNo < hashJoiner->numBuckets; bucketNo += hashJoiner->numBucketCxt)
	{
		HashBucketTuple *curPos = hashJoiner->buckets[bucketNo];
		/* We shouldn't attempt to spill an empty bucket or an already spilled bucket */
		if (NULL != curPos && !IS_SPILLED_BUCKET(hashJoiner->buckets[bucketNo]))
		{
			Assert(0 != hashJoiner->bucketSizes[bucketNo]);

			if (NULL == hashJoiner->spilled)
			{
				CreateWorkSet(hashJoiner);
				Assert(NULL != hashJoiner->spilled);
			}

			int spillFileIdx = bucketNo % hashJoiner->numSpillFiles;

			if (NULL == hashJoiner->spilled[spillFileIdx])
			{
				/* First write to this batch file, so create it */
				Assert(hashJoiner->workSet != NULL);
				hashJoiner->spilled[spillFileIdx] = MemoryContextAllocZero(hashJoiner->workFileCxt, sizeof(SpilledPartitionPair));
				hashJoiner->spilled[spillFileIdx]->inner = MemoryContextAllocZero(hashJoiner->workFileCxt, sizeof(SpilledPartition));
			}

			if (NULL == hashJoiner->spilled[spillFileIdx]->inner->workFile)
			{
				CreateWorkFile(hashJoiner->workSet, &hashJoiner->spilled[spillFileIdx]->inner->workFile, hashJoiner->workFileCxt);
				Assert(NULL != hashJoiner->spilled[spillFileIdx]->inner->workFile);
			}

			while (NULL != curPos)
			{
				MemTuple tuple = HASH_BUCKET_MINTUPLE(curPos);
				SaveTupleToSpillFile(hashJoiner->spilled[spillFileIdx]->inner, tuple);
				curPos = curPos->next;
			}

			hashJoiner->buckets[bucketNo] = spillMarkerAddress;

			freed += hashJoiner->bucketSizes[bucketNo];
			hashJoiner->bucketSizes[bucketNo] = 0;
			Assert(0 <= hashJoiner->inMemoryTupleSize);
		}
	}

	hashJoiner->inMemoryTupleSize -= freed;

	MemoryContextReset(hashJoiner->bucketCxt[memCxtIdx]);

	return freed;
}

/* Inserts a tuple in an in-memory hash bucket */
static void
InsertIntoInMemoryBucket(ResilientHashJoiner* hashJoiner, HashType bucketNo, MemTuple innerTuple, HashType hashValue)
{
	Assert(!IS_SPILLED_BUCKET(hashJoiner->buckets[bucketNo]));
	Size hashTupleSize = INFLATED_HASH_BUCKET_TUPLE_SIZE(memtuple_get_size(innerTuple, NULL));
	hashJoiner->bucketSizes[bucketNo] += hashTupleSize;
	hashJoiner->inMemoryTupleSize += hashTupleSize;

//	Assert(ActiveMemoryAccount == hashJoiner->joiner.joinerState->sjs.js.ps.plan->memoryAccount);
//	Size curBal = ActiveMemoryAccount->allocated - ActiveMemoryAccount->freed;

	HashBucketTuple *hashTuple = (HashBucketTuple *) MemoryContextAlloc(hashJoiner->bucketCxt[bucketNo % hashJoiner->numBucketCxt], hashTupleSize);
	hashTuple->hashValue = hashValue;
	memcpy(HASH_BUCKET_MINTUPLE(hashTuple), innerTuple, memtuple_get_size(innerTuple, NULL));
	hashTuple->next = hashJoiner->buckets[bucketNo];
	hashJoiner->buckets[bucketNo] = hashTuple;

//	Assert(curBal + MAXALIGN(hashTupleSize) + ALLOC_CHUNKHDRSZ == ActiveMemoryAccount->allocated - ActiveMemoryAccount->freed);
}

/*
 * Inserts an inner tuple in the hash table. If the destination bucket
 * is spilled, it saves the tuple in the corresponding spill file.
 *
 * The inner tuple should already be in ecxt_innertuple
 */
static void
InsertInnerTupleIntoHashTable(ResilientJoiner *resilientJoiner)
{
	ResilientHashJoiner *hashJoiner = (ResilientHashJoiner *)resilientJoiner;

	/*
	 * We check for memory overflow before inserting the new tuple. Ideally,
	 * including new tuple size in the calculation could be better. However,
	 * excluding it simplifies the calculation as the tuple's destination bucket
	 * is either in-memory or spilled by the time we insert the tuple and the
	 * inMemoryTupleSize can accurately track the already inserted tuples
	 */
	if (hashJoiner->inMemoryTupleSize >= resilientJoiner->memoryQuota)
	{
		/* Spilling strategy to use to free up memory */
		if (SPILL_LARGEST_BUCKETS == resilientjoin_spill_strategy)
		{
			SpillTopKBuckets(hashJoiner);
		}
		else if (SPILL_LARGEST_MEMORY_CONTEXT == resilientjoin_spill_strategy)
		{
			SpillBucketsInLargestMemoryContext(hashJoiner);
		}
		else
		{
			SpillLargestBuckets(hashJoiner);
		}

		Assert(hashJoiner->inMemoryTupleSize <= resilientJoiner->memoryQuota);
	}

	/* The current output of the inner abstract reader */
	TupleTableSlot *innerTupleSlot = *resilientJoiner->innerReader->outputSlot;

	bool allHashValsAreNull = true;
	HashType hashValue = TupleHashComputer_ComputeHash(hashJoiner->innerHashComputer, &allHashValsAreNull);
	hashJoiner->foundHashedColumnsAreNull |= allHashValsAreNull;

	HashType bucketNo = hashValue % hashJoiner->numBuckets;
	MemTuple innerTuple = ExecFetchSlotMemTuple(innerTupleSlot, false);

	if (IS_SPILLED_BUCKET(hashJoiner->buckets[bucketNo]))
	{
		/* The bucket is already spilled. Save the tuple in the corresponding spill file */
		SpillInnerTuple(hashJoiner, bucketNo, innerTuple);
	}
	else
	{
		/* Insert into in-memory hash table bucket */
		InsertIntoInMemoryBucket(hashJoiner, bucketNo, innerTuple, hashValue);
	}
}

/* Build the entire hash table and spill all the build side tuples */
static TupleTableSlot*
ExecuteHashJoinBuildStage(ResilientJoiner *resilientJoiner)
{
	Assert(IsA(resilientJoiner, ResilientHashJoiner));

	ResilientHashJoiner *hashJoiner = (ResilientHashJoiner *) resilientJoiner;

	/* Prepare the shared tuple hash computer with the current seed */
	TupleHashComputer_Begin(hashJoiner->innerHashComputer, resilientJoiner, hashJoiner->seed);

	TupleTableSlot *innerTuple = resilientJoiner->innerReader->ReadNextTuple(resilientJoiner->innerReader);
	JoinType joinType = hashJoiner->joiner.joinerState->sjs.js.jointype;
	while (!TupIsNull(innerTuple))
	{
		InsertInnerTupleIntoHashTable(resilientJoiner);

		if (joinType == JOIN_LASJ_NOTIN && hashJoiner->foundHashedColumnsAreNull)
		{
			/* Found at least one null inner tuple. So, terminate the whole thing without any joined tuple */
			resilientJoiner->joinerState->sjs.joinDone = true;
			break;
		}

		innerTuple = resilientJoiner->innerReader->ReadNextTuple(resilientJoiner->innerReader);
	}

	TupleHashComputer_End(hashJoiner->innerHashComputer, resilientJoiner);

	return DelegateToNextStage(resilientJoiner);
}

/*
 * Execute probe stage by probing next probe side tuple in the already
 * built hash table. If probe is done, this would delegate to the spill
 * file processing stage for the next result tuple.
 */
static TupleTableSlot*
ExecuteJoin(ResilientJoiner *resilientJoiner)
{
	SimpleJoinState *sjs = &resilientJoiner->joinerState->sjs;

	TupleTableSlot *joinedTuple = ExecuteSimpleJoin(sjs);
	if (!TupIsNull(joinedTuple))
	{
		return joinedTuple;
	}

	return DelegateToNextStage(resilientJoiner);
}

/*
 * Execute the spill file processing stage and return the next
 * joined tuple. The next joined tuple would come from a nested
 * sub-joiner
 */
static TupleTableSlot*
ExecuteHashJoinSpillProcessor(ResilientJoiner *resilientJoiner)
{
	/* Handle next spill partition */
	resilientJoiner->childJoiner = GetNextNestedJoiner((ResilientHashJoiner *)resilientJoiner);
	return DelegateToNextStage(resilientJoiner);
}

/* Collects the basic hash table stats for EXPLAIN ANALYZE */
static void
CollectResilientHashJoinerBuildStats(ResilientHashJoiner *hashJoiner)
{
	Assert(NULL != hashJoiner->joiner.joinerState->joinStats);
	Assert(hashJoiner->joiner.joinerState->joinStats->numSubJoin > hashJoiner->joiner.joinerId);

	int64 _maxBucketSize = 0;
	int64 _minBucketSize = INT64_MAX;
	int64 _totalBucketSize = 0;

	int _emptyBucketCount = 0;

	SubJoinStats *joinStat = &hashJoiner->joiner.joinerState->joinStats->subJoinStat[hashJoiner->joiner.joinerId];
	CdbExplain_Agg *chainLength = &joinStat->hashJoinStats.chainLength;

	for (int bucketNo = 0; bucketNo < hashJoiner->numBuckets; bucketNo++)
	{
		int64 curBucketSize = 0;
		if (!IS_SPILLED_BUCKET(hashJoiner->buckets[bucketNo]))
		{
			curBucketSize = hashJoiner->bucketSizes[bucketNo];

			/* Skip over empty bucket */
			if (curBucketSize > 0)
			{
				_maxBucketSize = Max(_maxBucketSize, curBucketSize);
				_minBucketSize = Min(_minBucketSize, curBucketSize);
				_totalBucketSize += curBucketSize;
			}

            HashBucketTuple *hashTuple = hashJoiner->buckets[bucketNo];
            int bucketLength = 0;
			for (; hashTuple; hashTuple = hashTuple->next)
			{
				bucketLength++;
			}

			cdbexplain_agg_upd(chainLength, bucketLength, bucketNo);
		}

		if (curBucketSize == 0)
		{
			++_emptyBucketCount;
		}
	}

	int64 _maxSpilledBucketSize = 0;
	int64 _minSpilledBucketSize = INT64_MAX;
	int _spilledBucketCount = 0;
	int64 _totalSpilledBucketSize = 0;

	if (hashJoiner->spilled)
	{
		for (int spillFileNo = 0; spillFileNo < hashJoiner->numSpillFiles; spillFileNo++)
		{
			if (NULL != hashJoiner->spilled[spillFileNo] &&
					NULL != hashJoiner->spilled[spillFileNo]->inner)
			{
				++_spilledBucketCount;
				Size curSpillSize = hashJoiner->spilled[spillFileNo]->inner->memorySize;

				_maxSpilledBucketSize = Max(_maxSpilledBucketSize, curSpillSize);
				_minSpilledBucketSize = Min(_minSpilledBucketSize, curSpillSize);
				_totalSpilledBucketSize += curSpillSize;
			}
		}
	}

	Assert(hashJoiner->inMemoryTupleSize == _totalBucketSize);

	joinStat->hashJoinStats.maxBucketSize = _maxBucketSize;
	joinStat->hashJoinStats.minBucketSize = _minBucketSize;
	joinStat->hashJoinStats.maxSpilledBucketSize = _maxSpilledBucketSize;
	joinStat->hashJoinStats.minSpilledBucketSize = _minSpilledBucketSize;
	joinStat->hashJoinStats.totalBucketSize = _totalBucketSize;
	joinStat->hashJoinStats.totalSpilledBucketSize = _totalSpilledBucketSize;
	joinStat->hashJoinStats.emptyBucketCount = _emptyBucketCount;
	joinStat->hashJoinStats.spilledBucketCount = _spilledBucketCount;
	joinStat->hashJoinStats.totalBucketCount = hashJoiner->numBuckets;
}

/* Cleans up the build side abstract reader once the hash table is built */
static void
PostBuildCleanup(ResilientHashJoiner *hashJoiner)
{
	ResilientJoiner *joiner = (ResilientJoiner *)hashJoiner;

	if (NULL != joiner->innerReader)
	{
		joiner->innerReader->FreeReader(&joiner->innerReader);
		Assert(NULL == joiner->innerReader);
	}
}

/* Drops the hash table and any associated metadata once the probe phase is done */
static void
PostProbeCleanup(ResilientHashJoiner *hashJoiner)
{
	ResilientJoiner *joiner = (ResilientJoiner *)hashJoiner;

	if (NULL != joiner->outerReader)
	{
		joiner->outerReader->FreeReader(&joiner->outerReader);
		Assert(NULL == joiner->outerReader);
	}

	if (NULL != hashJoiner->bucketReader)
	{
		/* Free up the hash bucket reader */
		hashJoiner->bucketReader->FreeReader(&hashJoiner->bucketReader);
		Assert(NULL == hashJoiner->bucketReader);
	}

	if (NULL != hashJoiner->hashCxt)
	{
		/* Free all memory of the hash table */
		MemoryContextDelete(hashJoiner->hashCxt);
		hashJoiner->hashCxt = NULL;
	}
}

/* Cleanup all work files */
static void
CleanupWorkFiles(ResilientHashJoiner *hashJoiner)
{
	if (NULL != hashJoiner->spilled)
	{
		/* Free up any sub-joiner partition id list */
		list_free(hashJoiner->partitionIds);
		hashJoiner->partitionIds = NULL;

		for (int spillFileIdx = 0; spillFileIdx < hashJoiner->numSpillFiles; spillFileIdx++)
		{
			ResilientHashJoiner_CleanupSpillFile(hashJoiner, spillFileIdx, InputSide_Inner);
			ResilientHashJoiner_CleanupSpillFile(hashJoiner, spillFileIdx, InputSide_Outer);

			/* Once both spill files are cleaned, the CleanupSpillFile should also release SpillPartitionPair */
			Assert(NULL == hashJoiner->spilled[spillFileIdx]);
		}

		pfree(hashJoiner->spilled);
		hashJoiner->spilled = NULL;

		Assert(NULL != hashJoiner->workSet);
		workfile_mgr_close_set(hashJoiner->workSet);
		hashJoiner->workSet = NULL;
	}
}

/*
 * Drops all spill files and related metadata once the spill handling
 * is done
 */
static void
PostSpillCleanup(ResilientHashJoiner *hashJoiner)
{
	CleanupWorkFiles(hashJoiner);

	if (NULL != hashJoiner->workFileCxt)
	{
		/* Free memory of the spill file data structures */
		MemoryContextDelete(hashJoiner->workFileCxt);
		hashJoiner->workFileCxt = NULL;
	}
}

/*
 * Cleans up a resilient hash join
 */
static void
CleanupResilientHashJoin(ResilientHashJoiner *hashJoiner)
{
	/* Free parent ResilientJoiner */
	PostBuildCleanup(hashJoiner);
	PostProbeCleanup(hashJoiner);

	/* Cleanup all spill files */
	PostSpillCleanup(hashJoiner);
}

/* Cleans up a nested loop join */
static void
CleanupResilientNestedLoopJoiner(ResilientNestedLoopJoiner *nlJoiner)
{
	Insist(!"Not yet implemented");
}

/* Cleans up a sub-join */
static void
CleanupResilientJoiner(ResilientJoiner *joiner)
{
	ValidateJoinerPointer(joiner);

	if (IsA(joiner, ResilientHashJoiner))
	{
		CleanupResilientHashJoin((ResilientHashJoiner *)joiner);
	}
	else if (IsA(joiner, ResilientNestedLoopJoiner))
	{
		CleanupResilientNestedLoopJoiner((ResilientNestedLoopJoiner *) joiner);
	}
	else
	{
		Insist(!"Unknown joiner");
	}

	pfree(joiner);
}

/*
 * We want numBuckets to be prime so as to avoid having bucket
 * numbers depend on only some bits of the hash code.  Choose the next
 * larger prime from the list in bucketsPrimes[].  This also enforces that
 * numBuckets is not very small, by the simple expedient of not putting any
 * very small entries in bucketsPrimes[]
 */
static int
ExecChoosePrimeNBuckets(int numBuckets)
{
	/* Prime numbers that we like to  use as numBuckets values */
	static const int bucketsPrimes[] = {
		1033, 2063, 4111, 8219, 16417, 32779, 65539, 131111,
		262151, 524341, 1048589, 2097211, 4194329, 8388619, 16777289, 33554473,
		67108913, 134217773, 268435463, 536870951, 1073741831
	};

	int primeCount = sizeof(bucketsPrimes) / sizeof(bucketsPrimes[0]);
	if (numBuckets > bucketsPrimes[primeCount - 1])
	{
		numBuckets = bucketsPrimes[primeCount - 1];
		return numBuckets;
	}

 	for (int i = 0; i < (int) lengthof(bucketsPrimes); i++)
	{
		if (bucketsPrimes[i] >= numBuckets)
		{
			numBuckets = bucketsPrimes[i];
			break;
		}
	}

 	return numBuckets;
}

/*
 * Calculates the number of buckets and spill files for the hash table assuming that the stats
 * are not precise (e.g., for a first level sub-join)
 */
static void
ChooseHashTableSizeForImpreciseStat(uint64 tupCount, int tupWidth, Size memoryQuota, int *numBuckets, int *numSpillFiles)
{
	uint64 bucketCount = 0;
	uint64 spillFileCount = 0;

	if (Gp_role == GP_ROLE_EXECUTE)
	{
		/*
		 * On the segment we are expected to get a fraction of the total tuples,
		 * assuming tuples are uniformly distributed
		 */
		tupCount = tupCount / getgpsegmentCount();
	}

	/* Force a plausible relation size if no info */
	if (tupCount < 0)
	{
		tupCount = 1000;
	}

	int tupSizeWithOverhead = INFLATED_HASH_BUCKET_TUPLE_SIZE(tupWidth);
	Size allTupSizeWithOverhead = tupCount * tupSizeWithOverhead;

	if (allTupSizeWithOverhead > memoryQuota)
	{
		/* We'll need to spill */
		uint64 inMemTupleCount = (memoryQuota / tupSizeWithOverhead);
		/*
		 * TODO: maximize memory utilization by increasing bucket
		 * count if we have free memory, ignoring gp_hashjoin_tuples_per_bucket
		 */
		bucketCount = inMemTupleCount / gp_hashjoin_tuples_per_bucket;
		bucketCount = Min(bucketCount, INT_MAX / 32);

		spillFileCount = ceil(((double)allTupSizeWithOverhead) / memoryQuota);
		spillFileCount = Min(spillFileCount, INT_MAX / 32);

		/* Check to see if we're capping the number of workfiles we allow per query */
		if (gp_workfile_limit_files_per_query > 0)
		{
			spillFileCount = Min(gp_workfile_limit_files_per_query, spillFileCount);
		}
	}
	else
	{
		/* We expect to execute in memory */

		/*
		 * Divide our tuple row-count estimate by our the number of
		 * tuples we'd like in a bucket: this produces a small bucket
		 * count independent of our work_mem setting
		 */
		uint64 bucketCountFromTupCount = tupCount / gp_hashjoin_tuples_per_bucket;

		/*
		 * If we have memory to spare, we'd like to use it. So,
		 * divide up our memory evenly
		 */
		uint64 bucketCountFromMemQuota = memoryQuota / (tupSizeWithOverhead * gp_hashjoin_tuples_per_bucket);

		/*
		 * We'll use our "lower" memory independent guess as a lower
		 * limit; but if we've got memory to spare we'll take the mean
		 * of the lower-limit and the upper-limit
		 */
		if (bucketCountFromMemQuota > bucketCountFromTupCount)
		{
			bucketCount = ceil(((double)(bucketCountFromTupCount + bucketCountFromMemQuota)) / 2.0);
		}
		else
		{
			bucketCount = bucketCountFromTupCount;
		}

		bucketCount = Min(bucketCount, INT_MAX);
		spillFileCount = 1;
	}

	int64 metadataMemQuota = memoryQuota * (((double)gp_hashjoin_metadata_memory_percent)/100.0);
	int64 metaDataMemRequested = HASH_TABLE_METADATA_OVERHEAD(bucketCount);

	if (metaDataMemRequested > metadataMemQuota)
	{
		bucketCount = metadataMemQuota / HASH_TABLE_METADATA_OVERHEAD(1);
	}
	else
	{
		bucketCount = Max(bucketCount, metadataMemQuota / HASH_TABLE_METADATA_OVERHEAD(1));
	}

	/* Pick the closest prime number for the number of buckets */
	int primeBucketCount = ExecChoosePrimeNBuckets((int) bucketCount);
	*numBuckets = primeBucketCount;

	/*
	 * TODO: Introduce minimum number of spill files guc to guard against
	 * wrong stat
	 */
	spillFileCount = Max(spillFileCount, 100);
	*numSpillFiles = (int) spillFileCount;
}

/*
 * Calculates the number of buckets and spill files for the hash table assuming that the stats
 * are precise (e.g., for a second level sub-join)
 */
static void
ChooseHashTableSizeForPreciseStat(uint64 tupCount, int tupWidth, Size memoryQuota, int *numBuckets, int *numSpillFiles)
{
	uint64 bucketCount = 0;
	uint64 spillFileCount = 0;

	bucketCount = tupCount / gp_hashjoin_tuples_per_bucket;

	/* 10% overhead for metadata */
	int64 metadataMemQuota = memoryQuota * (((double)gp_hashjoin_metadata_memory_percent)/100.0);
	int64 metaDataMemRequested = HASH_TABLE_METADATA_OVERHEAD(bucketCount);

	if (metaDataMemRequested > metadataMemQuota)
	{
		bucketCount = metadataMemQuota / HASH_TABLE_METADATA_OVERHEAD(1);
	}
	else
	{
		bucketCount = Max(bucketCount, metadataMemQuota / HASH_TABLE_METADATA_OVERHEAD(1));
	}

	/* Pick the closest prime number for the number of buckets */
	int primeBucketCount = ExecChoosePrimeNBuckets((int) bucketCount);
	*numBuckets = primeBucketCount;

	/* At least 4 buckets should share a spill file, but no more than 1024 spill files */
	spillFileCount = bucketCount / 4;
	spillFileCount = Max(spillFileCount, 100);
	*numSpillFiles = (int) spillFileCount;
}

/* Calculates the number of buckets and spill files for the hash table */
static void
ChooseHashTableSize(bool isPrecise, uint64 tupCount, int tupWidth, Size memoryQuota, int *numBuckets, int *numBatches)
{
	if (isPrecise)
	{
		ChooseHashTableSizeForPreciseStat(tupCount, tupWidth, memoryQuota, numBuckets, numBatches);
	}
	else
	{
		ChooseHashTableSizeForImpreciseStat(tupCount, tupWidth, memoryQuota, numBuckets, numBatches);
	}
}
