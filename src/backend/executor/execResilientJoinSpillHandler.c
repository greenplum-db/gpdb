/*-------------------------------------------------------------------------
 *
 * execResilientJoinSpillHandler.c
 *	  Supporting APIs for handling spilling during ResilientHashJoiner (part of ResilientJoin)
 *
 * Portions Copyright (c) 2015, Pivotal Inc.
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "inttypes.h"
#include "executor/execResilientJoinSpillHandler.h"
#include "executor/nodeResilientJoin.h"
#include "utils/workfile_mgr.h"
#include "utils/memutils.h"
#include "cdb/cdbvars.h"

/* Constant to indicate that we should spill largest bucket */
#define SPILL_LARGEST_BUCKETS 0
/* Constant to indicate that we should spill largest memory context */
#define SPILL_LARGEST_MEMORY_CONTEXT 1

/* Guc to control the current spill strategy */
int resilientjoin_spill_strategy = SPILL_LARGEST_BUCKETS;

static void CreateWorkSet(ResilientHashJoiner* hashJoiner);
static void CreateWorkFile(workfile_set * workSet, ExecWorkFile **workFile, MemoryContext memoryContext);
static void SaveTupleToSpillFile(SpilledPartition *spilledPartition, MemTuple tuple);
static void CleanupWorkFiles(ResilientHashJoiner *hashJoiner);

/* A marker to indicate that hash bucket has spilled */
static HashBucketTuple spillMarker;
/* We mark a bucket as spilled by pointing the bucket head to the spillMarkerAddress */
HashBucketTuple *spillMarkerAddress = &spillMarker;

/********************************************************************************
 *
 * ******************** PRIVATE METHODS ******************************************
 */

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

/********************************************************************************
 *
 * ******************** PUBLIC METHODS ******************************************
 */

/*
 * Spill an inner tuple.
 *
 * We assume that the corresponding bucket is already spilled (i.e., spill files are already created)
 */
void
ResilientHashJoiner_SpillInnerTuple(ResilientHashJoiner *hashJoiner, int bucketNo, MemTuple tuple)
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

/*
 * Spill hash buckets to free up memory
 *
 * Returns the number of bytes freed
 */
Size
ResilientHashJoiner_SpillBuckets(ResilientHashJoiner *hashJoiner)
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

	Assert(hashJoiner->inMemoryTupleSize <= hashJoiner->joiner.memoryQuota);
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
bool
ResilientHashJoiner_ChooseNextSetOfSpillPartitions(ResilientHashJoiner *hashJoiner, Size memoryQuota, bool roleReversalAllowed,
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

/*
 * Drops all spill files and related metadata once the spill handling
 * is done
 */
void
ResilientHashJoiner_PostSpillCleanup(ResilientHashJoiner *hashJoiner)
{
	CleanupWorkFiles(hashJoiner);

	if (NULL != hashJoiner->workFileCxt)
	{
		/* Free memory of the spill file data structures */
		MemoryContextDelete(hashJoiner->workFileCxt);
		hashJoiner->workFileCxt = NULL;
	}
}

