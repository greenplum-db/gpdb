/*-------------------------------------------------------------------------
 *
 * execAbstractReader.c
 *	  Implementation of different abstract readers
 *
 * Portions Copyright (c) 2015, Pivotal Inc.
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "executor/executor.h"
#include "executor/execResilientJoin.h"
#include "executor/nodeResilientJoin.h"
#include "executor/execAbstractReader.h"
#include "executor/execWorkfile.h"
#include "executor/execTupleHashComputer.h"
#include "nodes/pg_list.h"

/* The reader would read from the ExecProcNode() directly */
typedef struct OperatorTupleReader
{
	AbstractTupleReader baseReader;

	/* The source plan where to read the tuples from (using ExecProcNode) */
	PlanState *sourcePlan;

	/* No more tuples? */
	bool sourceExhausted;
} OperatorTupleReader;

/*
 * A HashBucketReader reads from a hash bucket that contains one or more tuples.
 * The reader only returns tuples in the hash bucket that matches the current outer
 * tuple. Whenever the outer tuple changes, the caller should inform the reader of
 * the change by calling rescan on the hash bucket reader. The rescan call can choose
 * the new bucket to scan for the new incoming outer tuple
 */
typedef struct HashBucketTupleReader
{
	AbstractTupleReader baseReader;

	/* Where to retrieve the hash buckets */
	ResilientHashJoiner *sourceJoiner;

	/*
	 * The simple join instance to find the quals to verify
	 * if an inner tuple can match the current outer tuple
	 */
	SimpleJoinState *sjs;

	/* The current bucket that the reader is reading */
	HashBucketTuple *bucketHead;

	/*
	 * The outer tuple's hash value to filter out unnecessary inner tuple.
	 * Note, a hash bucket may have tuples of different hash values, due to
	 * modulus operation on the integer hash value to fit all hash values
	 * within a range of buckets
	 */
	HashType filterHash;
	/*
	 * One outer tuple can join with many inner tuples. We cache the outer tuple's
	 * current bucket number across multiple pull
	 */
	int curBucketNo;
	/*
	 * The current inner tuple in the current bucket. The next
	 * join call can just start from this tuple instead of iterating within the bucket
	 * all-over again
	 */
	HashBucketTuple *curBucketPos;

	/* The slot to use to convert memtuple to TupleTableSlot */
	TupleTableSlot *innerSlot;
} HashBucketTupleReader;

/* A reader that can read from a collection of spill files */
typedef struct SpilledTupleReader
{
	AbstractTupleReader baseReader;

	/*
	 * The hash joiner that owns the spilled tuples. The reader
	 * would use the partition ids (see below) to look up the spill
	 * files in the owner hash joiner
	 */
	ResilientHashJoiner *hashJoiner;

	/*
	 * The list (for partition tuning) of partition Ids that can be used
	 * to determine a set of spill files to read
	 */
	List *partitionIds;

	/*
	 * Marks the position in the partitionIds to identify the current spill
	 * file being read
	 */
	ListCell *curPos;

	/* For a one time initialization of curPos to list head */
	bool firstPartition;

	/*
	 * Is it outer or inner tuples? Depending on the input side the reader
	 * would either read the inner side work file or the outer side work file
	 */
	InputSide inputSide;

	/* The partition to read from */
	SpilledPartition *curPartition;

	/* Slot used to store a MemTuple that was read from a spill file */
	TupleTableSlot *memTupleContainerSlot;
} SpilledTupleReader;

/*
 * A decorator pattern where the reader reads from another abstract reader
 * but additionally provides filtering based on the spill status of the
 * target hash bucket. If a probe tuple's target hash bucket is memory resident,
 * it will return that tuple, otherwise it will save it to the corresponding
 * probe spill file
 */
typedef struct SpillableProbeTupleReader
{
	AbstractTupleReader baseReader;

	/* Where to retrieve the hash buckets */
	ResilientHashJoiner *sourceJoiner;

	/*
	 * The source reader to decorate with spilling and filtering based on target
	 * hash bucket's spilling status
	 */
	AbstractTupleReader *sourceReader;
} SpillableProbeTupleReader;

static void AbstractTupleReader_Initialize(AbstractTupleReader* reader,
		TupleTableSlot** outputSlot, AbstractTupleReaderReadTupleFunc *readNextFunc,
		AbstractTupleReaderReScanFunc *reScanFunc, AbstractTupleReaderEndFunc *endFunc,
		AbstractTupleReaderDesctructorFunc *destructor);

static TupleTableSlot* OperatorTupleReader_ReadNext(AbstractTupleReader *baseReader);
static void OperatorTupleReader_ReScan(AbstractTupleReader *baseReader, ExprContext *exprCtxt);
static void OperatorTupleReader_End(AbstractTupleReader *baseReader);

static TupleTableSlot* HashBucketTupleReader_ReadNext(AbstractTupleReader *baseReader);
static void HashBucketTupleReader_ReScan(AbstractTupleReader *baseReader, ExprContext *exprCtxt);
static void HashBucketTupleReader_End(AbstractTupleReader *baseReader);

static TupleTableSlot* SpilledTupleReader_ReadNext(AbstractTupleReader *baseReader);
static void SpilledTupleReader_ReScan(AbstractTupleReader *baseReader, ExprContext *exprCtxt);
static void SpilledTupleReader_End(AbstractTupleReader *baseReader);

static TupleTableSlot* SpillableProbeTupleReader_ReadNext(AbstractTupleReader *baseReader);
static void SpillableProbeTupleReader_ReScan(AbstractTupleReader *baseReader, ExprContext *exprCtxt);
static void SpillableProbeTupleReader_End(AbstractTupleReader *baseReader);

static void AbstractTupleReader_Destroy(AbstractTupleReader **reader);
static void OperatorTupleReader_Destroy(AbstractTupleReader **baseReader);
static void HashBucketTupleReader_Destroy(AbstractTupleReader **baseReader);
static void SpilledTupleReader_Destroy(AbstractTupleReader **reader);
static void SpillableProbeTupleReader_Destroy(AbstractTupleReader **baseReader);

extern void ResilientHashJoiner_SpillOuterTuple(ResilientHashJoiner *hashJoiner, int bucketNo, MemTuple tuple, HashType hashValue);

/********************************************************************************
 *
 * ******************** PUBLIC METHODS ******************************************
 */

/*
 * Creates an operator reader from a planState. An operator reader can call
 * ExecProcNode on a sub-plan to read tuples until that sub-plan is exhausted
 */
AbstractTupleReader*
CreateOperatorTupleReader(PlanState *planState, TupleTableSlot **outputSlot)
{
	OperatorTupleReader *operatorTupleReader = makeNode(OperatorTupleReader);
	operatorTupleReader->sourcePlan = planState;
	/* Initialize the base abstract reader */
	AbstractTupleReader_Initialize((AbstractTupleReader *) operatorTupleReader, outputSlot,
			OperatorTupleReader_ReadNext, OperatorTupleReader_ReScan, OperatorTupleReader_End, OperatorTupleReader_Destroy);
	return (AbstractTupleReader *) operatorTupleReader;
}

/*
 * Creates a HashBucketTupleReader instance
 */
AbstractTupleReader*
CreateHashBucketTupleReader(ResilientHashJoiner *hashJoiner, TupleTableSlot **outputSlot, TupleTableSlot *innerSlot)
{
	HashBucketTupleReader *bucketReader = makeNode(HashBucketTupleReader);
	AbstractTupleReader_Initialize((AbstractTupleReader *)bucketReader, outputSlot,
			HashBucketTupleReader_ReadNext, HashBucketTupleReader_ReScan, HashBucketTupleReader_End, HashBucketTupleReader_Destroy);

	/* The hash joiner that provides the bucket */
	bucketReader->sourceJoiner = hashJoiner;
	bucketReader->sjs = &hashJoiner->joiner.joinerState->sjs;
	bucketReader->innerSlot = innerSlot;
	return (AbstractTupleReader *) bucketReader;
}

/* Creates a SpilledTupleReader instance */
AbstractTupleReader*
CreateSpilledTupleReader(ResilientHashJoiner *hashJoiner, TupleTableSlot **outputSlot, TupleTableSlot *memTupleContainerSlot,
		InputSide inputSide, List *partitionIds)
{
	SpilledTupleReader *spillReader = makeNode(SpilledTupleReader);
	AbstractTupleReader_Initialize((AbstractTupleReader *)spillReader, outputSlot,
			SpilledTupleReader_ReadNext, SpilledTupleReader_ReScan,
			SpilledTupleReader_End, SpilledTupleReader_Destroy);

	spillReader->hashJoiner = hashJoiner;
	spillReader->inputSide = inputSide;
	spillReader->partitionIds = partitionIds;
	/* For a one time initialization of curPos to the list head of partitionIds */
	spillReader->firstPartition = true;
	spillReader->memTupleContainerSlot = memTupleContainerSlot;

	/*
	 * The output slot should always point to our internal tuple slot that we will use
	 * for deserializing spilled tuples
	 */
	*(spillReader->baseReader.outputSlot) = spillReader->memTupleContainerSlot;
	return (AbstractTupleReader *) spillReader;
}

/* Creates a SpillableProbeTupleReader instance */
AbstractTupleReader*
CreateSpillableProbeTupleReader(ResilientHashJoiner *hashJoiner, AbstractTupleReader *sourceReader)
{
	SpillableProbeTupleReader *probeReader = makeNode(SpillableProbeTupleReader);
	AbstractTupleReader_Initialize((AbstractTupleReader *)probeReader, sourceReader->outputSlot,
			SpillableProbeTupleReader_ReadNext, SpillableProbeTupleReader_ReScan,
			SpillableProbeTupleReader_End, SpillableProbeTupleReader_Destroy);

	probeReader->sourceJoiner = hashJoiner;
	probeReader->sourceReader = sourceReader;
	return (AbstractTupleReader *) probeReader;
}

/********************************************************************************
 *
 * ******************** PRIVATE METHODS *****************************************
 */

/* Initializes an AbstractTupleReader */
static void
AbstractTupleReader_Initialize(AbstractTupleReader* reader,
		TupleTableSlot** outputSlot, AbstractTupleReaderReadTupleFunc *readNextFunc,
		AbstractTupleReaderReScanFunc *reScanFunc, AbstractTupleReaderEndFunc *endFunc,
		AbstractTupleReaderDesctructorFunc *destructor)
{
	reader->outputSlot = outputSlot;
	reader->ReadNextTuple = readNextFunc;
	reader->PrepareForReScan = reScanFunc;
	reader->EndReader = endFunc;
	reader->FreeReader = destructor;
}

/* Destructor for an abstract reader */
static void
AbstractTupleReader_Destroy(AbstractTupleReader **reader)
{
	pfree(*reader);
	*reader = NULL;
}

/* Reads a tuple from the input operator by using ExecProcNode */
static TupleTableSlot*
OperatorTupleReader_ReadNext(AbstractTupleReader *baseReader)
{
	Assert(IsA(baseReader, OperatorTupleReader));
	OperatorTupleReader *operatorTupleReader = (OperatorTupleReader *)baseReader;
	*(baseReader->outputSlot) = ExecProcNode(operatorTupleReader->sourcePlan);
	return *(baseReader->outputSlot);
}

/* Calls ReScan on the input PlanState */
static void
OperatorTupleReader_ReScan(AbstractTupleReader *baseReader, ExprContext *exprCtxt)
{
	Assert(IsA(baseReader, OperatorTupleReader));
	OperatorTupleReader *opReaderState = (OperatorTupleReader *)baseReader;
	ExecReScan(opReaderState->sourcePlan, exprCtxt);
}

/* Calls ExecEndNode on the input PlanState to cleanup */
static void
OperatorTupleReader_End(AbstractTupleReader *baseReader)
{
	Assert(IsA(baseReader, OperatorTupleReader));
	OperatorTupleReader *opReaderState = (OperatorTupleReader *)baseReader;

	ExecEndNode(opReaderState->sourcePlan);
}

/* Destroys an OperatorTupleReader */
static void
OperatorTupleReader_Destroy(AbstractTupleReader **baseReader)
{
	/* TODO: Implement OperatorTupleReader specific cleanup */
	AbstractTupleReader_Destroy(baseReader);
}

/*
 * Matches two tuples (one inner and one outer) and returns true
 * if they match on the join conditions. Note: this is always
 * INNER join. The caller must implement different join behaviors.
 * E.g., if this function returns false, an outer join may decide
 * to return the outer tuple with null columns for the inner side.
 *
 * The inner and outer tuples should already be in ecxt_innertuple
 * and ecxt_outertuple
 */
static inline bool
IsTuplePairJoinable(SimpleJoinState *sjs)
{
	ExprContext *econtext = sjs->js.ps.ps_ExprContext;
	List *hashqualclauses = sjs->hashqualclauses;
	List *joinqual = sjs->js.joinqual;

	/* Reset temp memory each time to avoid leaks from qual expr */
	ResetExprContext(econtext);

	if (hashqualclauses == NIL || ExecQual(hashqualclauses, econtext, false))
	{
		if (joinqual == NIL || ExecQual(joinqual, econtext, false /* resultForNull */))
		{
			return true;
		}
	}

	return false;
}

/* Looks for the next joinable tuple from the hash bucket */
static TupleTableSlot*
HashBucketTupleReader_ReadNext(AbstractTupleReader *baseReader)
{
	Assert(IsA(baseReader, HashBucketTupleReader));
	HashBucketTupleReader *bucketReader = (HashBucketTupleReader *)baseReader;
	SimpleJoinState *sjs = (SimpleJoinState *) bucketReader->sjs;

	/* Hash value of the current outer tuple */
	HashType filterHash = bucketReader->filterHash;
	/*
	 * The bucket position in the current bucket (note, we may read halfway
	 * through a bucket, find a match and return, and resume reading from that position
	 * when we need another match)
	 */
	HashBucketTuple *curBucketPos = bucketReader->curBucketPos;

	while (NULL != curBucketPos)
	{
		/* Apply filter */
		if (curBucketPos->hashValue == filterHash)
		{
			 *(baseReader->outputSlot) = ExecStoreMemTuple(HASH_BUCKET_MINTUPLE(curBucketPos), bucketReader->innerSlot, false /* shouldFree */);
			if (IsTuplePairJoinable(sjs))
			{
				/* Save the bucket position so that we can resume later */
				bucketReader->curBucketPos = curBucketPos->next;
				return bucketReader->innerSlot;
			}
		}

		curBucketPos = curBucketPos->next;
	}

	bucketReader->curBucketPos = NULL;
	/*
	 * Important to set the output slot to the innerSlot as the caller is expecting
	 * the new tuple there. More importantly, don't try to optimize this assignment
	 * once in the constructor, as the same output slot (e.g., ecxt_innertuple) may
	 * point to a different slot, e.g., when reading from a different reader (such
	 * as an operator reader)
	 */
	*baseReader->outputSlot = bucketReader->innerSlot;
	return ExecClearTuple(bucketReader->innerSlot);
}

/*
 * Prepares for rescanning the hash bucket by updating the hash bucket that corresponds
 * to the new incoming outer tuple
 */
static void
HashBucketTupleReader_ReScan(AbstractTupleReader *baseReader, ExprContext *exprCtxt)
{
	Assert(IsA(baseReader, HashBucketTupleReader));
	HashBucketTupleReader *bucketReader = (HashBucketTupleReader *)baseReader;
	ResilientHashJoiner *hashJoiner = (ResilientHashJoiner *)bucketReader->sourceJoiner;

	/*
	 * Either we exhausted reading or we are yet to start reading or we are rescanning prematurely
	 * because of an anti-join or semi-join
	 */
	Assert(NULL == bucketReader->curBucketPos || bucketReader->curBucketPos == bucketReader->bucketHead ||
			bucketReader->sjs->antiJoin || bucketReader->sjs->semiJoin);

	/*
	 * The bucket that corresponds to the new outer tuple. This assumes that the caller
	 * hash joiner already updated the outerBucketNo when it found a new outer tuple
	 */
	HashType bucketNo = hashJoiner->outerBucketNo;

	bucketReader->bucketHead = hashJoiner->buckets[bucketNo];
	bucketReader->filterHash = hashJoiner->outerHashValue;
	bucketReader->curBucketNo = bucketNo;

	/* Initially we read from head */
	bucketReader->curBucketPos = bucketReader->bucketHead;
}

/* Ends the bucket reader by resetting all the parameters */
static void
HashBucketTupleReader_End(AbstractTupleReader *baseReader)
{
	Assert(IsA(baseReader, HashBucketTupleReader));
	HashBucketTupleReader *bucketReader = (HashBucketTupleReader *)baseReader;

	bucketReader->bucketHead = NULL;
	bucketReader->filterHash = -1;
	bucketReader->curBucketNo = -1;
	bucketReader->innerSlot = NULL;
}

/* Destroys a HashBucketTupleReader instance */
static void
HashBucketTupleReader_Destroy(AbstractTupleReader **baseReader)
{
	/* TODO: Implement HashBucketTupleReader specific cleanup */
	AbstractTupleReader_Destroy(baseReader);
}

/* Reads the next spilled tuple from a spill file */
static TupleTableSlot *
ReadNextSpilledTuple(ExecWorkFile *workFile, TupleTableSlot *outputSlot)
{
	static uint32 header = 0;

	/* Ensure that we are using 32-bit hash values */
	Assert(sizeof(uint32) == sizeof(HashType));

	size_t nread = ExecWorkFile_Read(workFile, &header, sizeof(header));
	if (nread != sizeof(header))				/* end of file */
	{
		return ExecClearTuple(outputSlot);
	}

	MemTuple tuple = (MemTuple) palloc(memtuple_size_from_uint32(header));
	memtuple_set_mtlen(tuple, NULL, header);

	nread = ExecWorkFile_Read(workFile,
							  (void *) ((char *) tuple + sizeof(uint32)),
							  memtuple_size_from_uint32(header) - sizeof(uint32));

	if (nread != memtuple_size_from_uint32(header) - sizeof(uint32))
	{
		ereport(ERROR, (errcode_for_file_access(), errmsg("Could not read from temporary file")));
	}

	return ExecStoreMemTuple(tuple, outputSlot, true /* shouldFree */);
}

/*
 * Prepares to read the next spill file.
 *
 * Note, in a partition tuning case a single spill reader
 * may need to read more than one spill files, one at a time
 */
static bool
AdvanceToNextSpillFile(SpilledTupleReader *spillReader)
{
	if (spillReader->firstPartition)
	{
		spillReader->curPos = list_head(spillReader->partitionIds);
		spillReader->firstPartition = false;
	}
	else
	{
		spillReader->curPos = lnext(spillReader->curPos);
	}

	if (NULL != spillReader->curPos)
	{
		int partId = lfirst_int(spillReader->curPos);
		SpilledPartitionPair *spilled = spillReader->hashJoiner->spilled[partId];

		Assert(NULL != spilled);
		if (InputSide_Inner == spillReader->inputSide)
		{
			spillReader->curPartition = spilled->inner;
		}
		else
		{
			spillReader->curPartition = spilled->outer;
		}

		/* Rewind the file for reading */
		if (!ExecWorkFile_Rewind(spillReader->curPartition->workFile))
		{
		    ereport(ERROR, (errcode_for_file_access(), errmsg("Could not access temporary file")));
		}

		return true;
	}

	return false;
}

/*
 * Reads next spilled tuple
 */
static TupleTableSlot*
SpilledTupleReader_ReadNext(AbstractTupleReader *baseReader)
{
	Assert(IsA(baseReader, SpilledTupleReader));
	SpilledTupleReader *spillReader = (SpilledTupleReader *)baseReader;

	if (spillReader->firstPartition)
	{
		if (!AdvanceToNextSpillFile(spillReader))
		{
			return ExecClearTuple(spillReader->memTupleContainerSlot);
		}
	}

	for (;;)
	{
		TupleTableSlot *slot = ReadNextSpilledTuple(spillReader->curPartition->workFile,
				spillReader->memTupleContainerSlot);

		if (!TupIsNull(slot))
		{
			Assert(slot == spillReader->memTupleContainerSlot && *baseReader->outputSlot == slot);
			return slot;
		}

		if (!AdvanceToNextSpillFile(spillReader))
		{
			/*
			 * Done with all spill files. The last read should already update the output
			 * slot with a null tuple
			 */
			Assert(TupIsNull(*spillReader->baseReader.outputSlot));
			return slot;
		}
	}

	/* We have a null (i.e., TupIsNull) tuple at this point */
	Assert(TupIsNull(*spillReader->baseReader.outputSlot));
	return *spillReader->baseReader.outputSlot;
}

/* SpilledTupleReader should not be rescanned */
static void
SpilledTupleReader_ReScan(AbstractTupleReader *baseReader, ExprContext *exprCtxt)
{
	Insist(!"Rescanning is not supported for the SpilledTupleReader");
}

/* Cleans up all the spill files */
static void
CleanupSpilledTupleReader(AbstractTupleReader* baseReader)
{
	Assert(IsA(baseReader, SpilledTupleReader));
	SpilledTupleReader* spillReader = (SpilledTupleReader*) baseReader;
	ResilientHashJoiner* hashJoiner = spillReader->hashJoiner;
	/* Free up the partitionIds list */
	ListCell *partIdCell = NULL;
	foreach (partIdCell, spillReader->partitionIds)
	{
		int partId = lfirst_int(partIdCell);
		ResilientHashJoiner_CleanupSpillFile(hashJoiner, partId, spillReader->inputSide);
	}
	/*
	 * This list is owned by the caller, and the caller should free it. Note, freeing it
	 * up here may also cause crash as the list may be shared between an inner and an
	 * outer spill file reader
	 */
	spillReader->partitionIds = NULL;
}

/* Cleans up a SpilledTupleReader */
static void
SpilledTupleReader_End(AbstractTupleReader *baseReader)
{
	CleanupSpilledTupleReader(baseReader);
}

/* Destroys a SpilledTupleReader instance */
static void
SpilledTupleReader_Destroy(AbstractTupleReader **reader)
{
	/* TODO: Implement cleanup for SpilledTupleReader */
	CleanupSpilledTupleReader(*reader);
	AbstractTupleReader_Destroy(reader);
}

/*
 * Returns a probe tuple that hashes into an in-memory bucket.
 *
 * This method may spill multiple tuples before finding and returning
 * a tuple that hashes into an in-memory bucket
 */
static TupleTableSlot*
SpillableProbeTupleReader_ReadNext(AbstractTupleReader *baseReader)
{
	Assert(IsA(baseReader, SpillableProbeTupleReader));
	SpillableProbeTupleReader *probeReader = (SpillableProbeTupleReader *)baseReader;
	AbstractTupleReader *sourceReader = probeReader->sourceReader;

	/* Reader another tuple from the underlying reader */
	TupleTableSlot *sourceTuple = sourceReader->ReadNextTuple(sourceReader);
	bool allHashValsAreNull = false;

	ResilientHashJoiner *hashJoiner = probeReader->sourceJoiner;

	while (!TupIsNull(sourceTuple))
	{
		/*
		 * Compute hash to see if the incoming tuple corresponds to an in-memory
		 * bucket or a spilled bucket
		 */
		HashType hashValue = TupleHashComputer_ComputeHash(hashJoiner->outerHashComputer, &allHashValsAreNull);
		HashType bucketNo = hashValue % hashJoiner->numBuckets;

		if (IS_SPILLED_BUCKET(hashJoiner->buckets[bucketNo]))
		{
			/* This bucket is not in memory. Therefore, we need to save this probe tuple */
			ResilientHashJoiner_SpillOuterTuple(hashJoiner, bucketNo, ExecFetchSlotMemTuple(sourceTuple, false), hashValue);
		}
		else
		{
			/* In memory bucket, so return it */

			/*
			 * Store the outer tuple's hash value so that the inner bucket reader does not
			 * need to recompute this hash value
			 */
			hashJoiner->outerHashValue = hashValue;
			hashJoiner->outerBucketNo = bucketNo;
			/*
			 * Assumes that underlying probe reader already updated the abstract reader's
			 * outputSlot. Note, our outputSlot is short-circuited with the source reader's outputSlot
			 */
			return sourceTuple;
		}

		/* The previous tuple was spilled, so keep looking for a tuple that corresponds to an in-memory bucket */
		sourceTuple = sourceReader->ReadNextTuple(sourceReader);
	}

	/* We have a null (i.e., TupIsNull) tuple at this point */
	Assert(TupIsNull(*probeReader->baseReader.outputSlot));
	/* Assumes that underlying probe reader already updated the abstract reader's outputSlot */
	return *probeReader->baseReader.outputSlot;
}

/* SpillableProbeReader should never be rescanned */
static void
SpillableProbeTupleReader_ReScan(AbstractTupleReader *baseReader, ExprContext *exprCtxt)
{
	Insist(!"Rescanning is not supported for the SpillableProbeTupleReader");
}

/* Calls ExecEndNode on the input PlanState to cleanup */
static void
SpillableProbeTupleReader_End(AbstractTupleReader *baseReader)
{
	Assert(IsA(baseReader, SpillableProbeTupleReader));
	SpillableProbeTupleReader *probeReader = (SpillableProbeTupleReader *)baseReader;
	probeReader->sourceReader->EndReader(probeReader->sourceReader);
}

/* Destroys a SpillableProbeTupleReader instance */
static void
SpillableProbeTupleReader_Destroy(AbstractTupleReader **baseReader)
{
	Assert(IsA(*baseReader, SpillableProbeTupleReader));

	SpillableProbeTupleReader *probeReader = (SpillableProbeTupleReader *) *baseReader;
	/* Destroy the source reader first */
	probeReader->sourceReader->FreeReader(&probeReader->sourceReader);
	/* Ensure that the source reader cleaned up */
	Assert(NULL == probeReader->sourceReader);

	/* TODO: Implement cleanup for SpillableProbeTupleReader */
	AbstractTupleReader_Destroy(baseReader);
}
