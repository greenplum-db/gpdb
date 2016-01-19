/*-------------------------------------------------------------------------
 *
 * execAbstractReader.h
 *	  prototypes for execAbstractReader.c
 *
 * Portions Copyright (c) 2015- Pivotal Inc.
 *-------------------------------------------------------------------------
 */

#ifndef EXECABSTRACTREADER_H
#define EXECABSTRACTREADER_H

#include "postgres.h"
#include "executor/executor.h"

struct AbstractTupleReader;
struct ResilientHashJoiner;

/* The function prototype for reading next tuple from an AbstractTupleReader */
typedef TupleTableSlot* (AbstractTupleReaderReadTupleFunc)(struct AbstractTupleReader *baseReader);
typedef void (AbstractTupleReaderReScanFunc)(struct AbstractTupleReader *baseReader, ExprContext *exprCtxt);
typedef void (AbstractTupleReaderEndFunc)(struct AbstractTupleReader *baseReader);
typedef void (AbstractTupleReaderDesctructorFunc)(struct AbstractTupleReader **baseReader);

/* Abstraction to read tuples from different sources */
typedef struct AbstractTupleReader
{
	NodeTag type;

	/* The slot that points to the last returned tuple */
	TupleTableSlot **outputSlot;
	/*
	 * Function pointer to read the next tuple from a collection of tuples
	 * (may be spilled tuples or from another operator)
	 */
	AbstractTupleReaderReadTupleFunc *ReadNextTuple;
	/* Function pointer to initialize for a rescan */
	AbstractTupleReaderReScanFunc *PrepareForReScan;
	/* Function pointer to end the reader */
	AbstractTupleReaderEndFunc *EndReader;
	/* Function pointer to destroy and free all allocations of a reader */
	AbstractTupleReaderDesctructorFunc *FreeReader;
} AbstractTupleReader;

/* Validates if a pointer is pointing to one of the concrete classes of the AbstractTupleReader */
#define ValidateAbstractReader(reader) Assert(NULL != reader && \
	(IsA(reader, SpilledTupleReader) || IsA(reader, OperatorTupleReader) || \
	IsA(reader, HashBucketTupleReader) || IsA(reader, SpillableProbeTupleReader)));

/* Validates a pair of pointers are both pointing to some concrete classes of the AbstractTupleReader */
#define ValidateAbstractReaderPair(reader1, reader2) ValidateAbstractReader(reader1); ValidateAbstractReader(reader2);

extern AbstractTupleReader* CreateOperatorTupleReader(PlanState *planState, TupleTableSlot **outputSlot);
extern AbstractTupleReader* CreateHashBucketTupleReader(struct ResilientHashJoiner *hashJoiner, TupleTableSlot **outputSlot, TupleTableSlot *innerSlot);
extern AbstractTupleReader* CreateSpilledTupleReader(struct ResilientHashJoiner *hashJoiner, TupleTableSlot **outputSlot, TupleTableSlot *memTupleContainerSlot,
		InputSide inputSide, List *partitionIds);
extern AbstractTupleReader* CreateSpillableProbeTupleReader(struct ResilientHashJoiner *hashJoiner, AbstractTupleReader *sourceReader);

#endif /* EXECABSTRACTREADER_H */
