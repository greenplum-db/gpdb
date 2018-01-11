/*-------------------------------------------------------------------------
 *
 * execDynamicScan.c
 *	  Support routines for iterating through dynamically chosen partitions of a relation
 *
 * Portions Copyright (c) 2014-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/execDynamicScan.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "cdb/cdbpartition.h"
#include "cdb/partitionselection.h"
#include "executor/executor.h"
#include "executor/execDynamicScan.h"
#include "executor/nodeIndexscan.h"
#include "executor/instrument.h"
#include "parser/parsetree.h"
#include "utils/memutils.h"

static Oid
DynamicScan_GetOriginalRelationOid(ScanState *scanState);

static ScanStatus
DynamicScan_Controller(ScanState *scanState, ScanStatus desiredState, PartitionInitMethod *partitionInitMethod,
		PartitionEndMethod *partitionEndMethod, PartitionReScanMethod *partitionReScanMethod);

static bool
DynamicScan_InitNextRelation(ScanState *scanState, PartitionInitMethod *partitionInitMethod, PartitionEndMethod *partitionEndMethod, PartitionReScanMethod *partitionReScanMethod);

static DynamicPartitionIterator*
DynamicScan_GetIterator(ScanState *scanState);

static Oid
DynamicScan_AdvanceIterator(ScanState *scanState, int32 numSelectors);

static void
DynamicScan_InitExpr(ScanState* scanState);

static void
DynamicScan_ObtainRelations(ScanState *scanState, Oid newOid, Relation *outOldRelation, Relation *outNewRelation);

static AttrNumber*
DynamicScan_MapRelationColumns(ScanState *scanState, Relation oldRelation, Relation newRelation);

static void
DynamicScan_UpdateScanStateForNewPart(ScanState *scanState, Relation newRelation);

static bool
DynamicScan_InitNextPartition(ScanState *scanState, PartitionInitMethod *partitionInitMethod,
		PartitionEndMethod *partitionEndMethod, PartitionReScanMethod *partitionReScanMethod);

static bool
DynamicScan_InitSingleRelation(ScanState *scanState, PartitionInitMethod *partitionInitMethod, PartitionReScanMethod *partitionReScanMethod);

static void
DynamicScan_CreateIterator(ScanState *scanState, Scan *scan);

static void
DynamicScan_EndIterator(ScanState *scanState);

static inline void
DynamicScan_CleanupOneRelation(ScanState *scanState, Relation relation, PartitionEndMethod *partitionEndMethod);

/*
 * -------------------------------------
 * Static functions
 * -------------------------------------
 */

/*
 * DynamicScan_AdvanceIterator
 * 		Returns the oid of the next partition.
 */
static Oid
DynamicScan_AdvanceIterator(ScanState *scanState, int32 numSelectors)
{
	DynamicPartitionIterator *iterator = DynamicScan_GetIterator(scanState);

	Assert(NULL != iterator);

	Oid pid = InvalidOid;
	while (InvalidOid == pid)
	{
		PartOidEntry *partOidEntry = hash_seq_search(iterator->partitionIterator);

		if (NULL == partOidEntry)
		{
			iterator->shouldCallHashSeqTerm = false;
			iterator->curRelOid = InvalidOid;
			return InvalidOid;
		}

		if (list_length(partOidEntry->selectorList) == numSelectors)
		{
			pid = partOidEntry->partOid;
			iterator->curRelOid = pid;
		}
	}

	return pid;
}

/*
 * DynamicScan_InitExpr
 * 		Initialize ExprState for a new partition from the plan's expressions
 */
static void
DynamicScan_InitExpr(ScanState* scanState)
{
	MemoryContext oldCxt = NULL;
	MemoryContext partCxt = DynamicScan_GetPartitionMemoryContext(scanState);
	if (NULL != partCxt)
	{
		MemoryContextReset(partCxt);
		/*
		 * Switch to partition memory context to prevent memory leak for
		 * per-partition data structures.
		 */
		oldCxt = MemoryContextSwitchTo(partCxt);
	}

	/*
	 * We might have reset the memory context. Set these dangling
	 * pointers to NULL so that we don't try to pfree them later
	 */
	scanState->ps.ps_ProjInfo = NULL;
	scanState->ps.qual = NULL;
	scanState->ps.targetlist = NULL;
	/* Initialize child expressions */
	scanState->ps.qual = (List*) ExecInitExpr((Expr*) scanState->ps.plan->qual,
			(PlanState*) scanState);
	scanState->ps.targetlist = (List*) ExecInitExpr(
			(Expr*) scanState->ps.plan->targetlist, (PlanState*) scanState);
	ExecAssignScanProjectionInfo(scanState);

	if (NULL != oldCxt)
	{
		MemoryContextSwitchTo(oldCxt);
	}
}

/*
 * DynamicScan_ObtainRelations
 * 		Returns old and new relations for a new part oid
 */
static void
DynamicScan_ObtainRelations(ScanState *scanState, Oid newOid, Relation *outOldRelation, Relation *outNewRelation)
{
	Relation oldRelation = NULL;
	Relation newRelation = NULL;
	Oid oldOid = InvalidOid;
	if (NULL != scanState->ss_currentRelation)
	{
		oldOid = RelationGetRelid(scanState->ss_currentRelation);
		oldRelation = scanState->ss_currentRelation;
	}
	else
	{
		/* Must be the initial state */
		Assert(SCAN_INIT == scanState->scan_state);
		oldOid = DynamicScan_GetOriginalRelationOid(scanState);
		Assert(OidIsValid(oldOid));
		oldRelation = OpenScanRelationByOid(oldOid);
	}

	if (oldOid == newOid)
	{
		newRelation = oldRelation;
	}
	else
	{
		newRelation = OpenScanRelationByOid(newOid);
	}

	Assert(NULL != oldRelation);
	Assert(NULL != newRelation);

	*outOldRelation = oldRelation;
	*outNewRelation = newRelation;
}

/*
 * DynamicScan_MapRelationColumns
 * 		Returns dropped column mapping between two relations. Returns NULL if
 * 		no mapping is necessary.
 */
static AttrNumber*
DynamicScan_MapRelationColumns(ScanState *scanState, Relation oldRelation, Relation newRelation)
{
	AttrNumber *attMap = NULL;

	if (isDynamicScan((Scan *)scanState->ps.plan) && (oldRelation != newRelation))
	{
		TupleDesc oldTupDesc = RelationGetDescr(oldRelation);
		TupleDesc newTupDesc = RelationGetDescr(newRelation);

		attMap = varattnos_map(oldTupDesc, newTupDesc);
	}

	return attMap;
}

/*
 * DynamicScan_UpdateScanStateForNewPart
 * 		Updates ScanState properties for a new part
 */
static void
DynamicScan_UpdateScanStateForNewPart(ScanState *scanState, Relation newRelation)
{
	scanState->ss_currentRelation = newRelation;
	ExecAssignScanType(scanState, RelationGetDescr(newRelation));
	Oid newOid = RelationGetRelid(newRelation);

	/*
	 * Inside ExecInitScanTupleSlot() we set the tuple table slot's oid
	 * to range table entry's relid, which for partitioned table always set
	 * to parent table's oid. In queries where we need to read table oids
	 * (MPP-20736) we use the tuple table slot's saved oid (refer to slot_getsysattr()).
	 * This wrongly returns parent oid, instead of partition oid. Therefore,
	 * to return correct partition oid, we need to update
	 * our tuple table slot's oid to reflect the partition oid.
	 */
	scanState->ss_ScanTupleSlot->tts_tableOid = newOid;
	scanState->tableType = getTableType(scanState->ss_currentRelation);
}

/*
 * DynamicScan_InitNextPartition
 *		Prepares the next partition for scanning by calling various
 *		helper methods to open relation, map dropped attributes,
 *		initialize expressions etc.
 */
static bool
DynamicScan_InitNextPartition(ScanState *scanState, PartitionInitMethod *partitionInitMethod, PartitionEndMethod *partitionEndMethod, PartitionReScanMethod *partitionReScanMethod)
{
	Assert(isDynamicScan((Scan *)scanState->ps.plan));
	AssertImply(scanState->scan_state != SCAN_INIT, NULL != scanState->ss_currentRelation);

	Scan *scan = (Scan *)scanState->ps.plan;
	DynamicTableScanInfo *partitionInfo = scanState->ps.state->dynamicTableScanInfo;
	Assert(partitionInfo->numScans >= scan->partIndex);
	int32 numSelectors = list_nth_int(partitionInfo->numSelectorsPerScanId, scan->partIndex);

	Oid newOid = DynamicScan_AdvanceIterator(scanState, numSelectors);

	if (!OidIsValid(newOid))
	{
		return false;
	}

	Relation oldRelation = NULL;
	Relation newRelation = NULL;

	DynamicScan_ObtainRelations(scanState, newOid, &oldRelation, &newRelation);
	/* Either we have a new relation or this is the first relation */
	if (oldRelation != newRelation || NULL == scanState->ss_currentRelation)
	{
		AttrNumber *attMap = DynamicScan_MapRelationColumns(scanState, oldRelation, newRelation);

		DynamicScan_RemapExpression(scanState, attMap, (Node*)scanState->ps.plan->qual);
		DynamicScan_RemapExpression(scanState, attMap, (Node*)scanState->ps.plan->targetlist);

		/*
		 * We only initialize expression if this is the first partition
		 * or if the column mapping changes between two partitions.
		 * Otherwise, we reuse the previously initialized expression.
		 */
		bool initExpressions = (NULL != attMap || SCAN_INIT == scanState->scan_state);

		if (newRelation != oldRelation)
		{
			/* Close the old relation */
			DynamicScan_CleanupOneRelation(scanState, oldRelation, partitionEndMethod);
		}

		DynamicScan_UpdateScanStateForNewPart(scanState, newRelation);

		if (initExpressions)
		{
			DynamicScan_InitExpr(scanState);
		}

		partitionInitMethod(scanState, attMap);

		if (NULL != attMap)
		{
			pfree(attMap);
			attMap = NULL;
		}
	}
	else
	{
		/* Rescan of the same part */
		partitionReScanMethod(scanState);
	}

	/* Collect number of partitions scanned in EXPLAIN ANALYZE */
	if(NULL != scanState->ps.instrument)
	{
		Instrumentation *instr = scanState->ps.instrument;
		instr->numPartScanned ++;
	}

	return true;
}

/*
 * DynamicScan_InitSingleRelation
 *		Prepares a single relation for scanning by calling various
 *		helper methods to open relation, initialize expressions etc.
 *
 *		Note: this is for the non-partitioned relations.
 *
 *		Returns true upon success.
 */
static bool
DynamicScan_InitSingleRelation(ScanState *scanState, PartitionInitMethod *partitionInitMethod, PartitionReScanMethod *partitionReScanMethod)
{
	Assert(!isDynamicScan((Scan *)scanState->ps.plan));

	if (NULL == scanState->ss_currentRelation)
	{
		/* Open the relation and initalize the expressions (targetlist, qual etc.) */
		InitScanStateRelationDetails(scanState, scanState->ps.plan, scanState->ps.state);
		partitionInitMethod(scanState, NULL /* No dropped column mapping */);
	}
	else
	{
		Insist(scanState->scan_state == SCAN_RESCAN);
		partitionReScanMethod(scanState);
	}

	return true;
}

/*
 * DynamicScan_GetOriginalRelationOid
 *		Returns the original relation as stored in the estate range table
 *		by the optimizer
 */
static Oid
DynamicScan_GetOriginalRelationOid(ScanState *scanState)
{
	EState *estate = scanState->ps.state;
	Scan *scan = (Scan *) scanState->ps.plan;

	Oid reloid = getrelid(scan->scanrelid, estate->es_range_table);
	Assert(OidIsValid(reloid));

	return reloid;
}

/*
 * DynamicScan_CreateIterator
 * 		Creates an iterator state (i.e., DynamicPartitionIterator)
 * 		and saves it to the estate's DynamicTableScanInfo.
 */
static void
DynamicScan_CreateIterator(ScanState *scanState, Scan *scan)
{
	EState *estate = scanState->ps.state;
	Assert(NULL != estate);
	DynamicTableScanInfo *partitionInfo = estate->dynamicTableScanInfo;

	/*
	 * Ensure that the dynahash exists even if the partition selector
	 * didn't choose any partition for current scan node [MPP-24169].
	 */
	InsertPidIntoDynamicTableScanInfo(estate, scan->partIndex, InvalidOid, InvalidPartitionSelectorId);

	Assert(NULL != partitionInfo && NULL != partitionInfo->pidIndexes);
	Assert(partitionInfo->numScans >= scan->partIndex);
	Assert(NULL == partitionInfo->iterators[scan->partIndex - 1]);

	DynamicPartitionIterator *iterator = palloc(sizeof(DynamicPartitionIterator));
	iterator->curRelOid = InvalidOid;
	iterator->partitionMemoryContext = AllocSetContextCreate(CurrentMemoryContext,
			 "DynamicTableScanPerPartition",
			 ALLOCSET_DEFAULT_MINSIZE,
			 ALLOCSET_DEFAULT_INITSIZE,
			 ALLOCSET_DEFAULT_MAXSIZE);
	iterator->partitionOids = partitionInfo->pidIndexes[scan->partIndex - 1];
	Assert(iterator->partitionOids != NULL);
	iterator->shouldCallHashSeqTerm = true;

	HASH_SEQ_STATUS *partitionIterator = palloc(sizeof(HASH_SEQ_STATUS));
	hash_seq_init(partitionIterator, iterator->partitionOids);

	iterator->partitionIterator = partitionIterator;

	partitionInfo->iterators[scan->partIndex - 1] = iterator;
}

/*
 * DynamicScan_EndIterator
 * 		Frees the partition iterator for a scanState.
 */
static void
DynamicScan_EndIterator(ScanState *scanState)
{
	Assert(NULL != scanState);

	/*
	 * For EXPLAIN of a plan, we may never finish the initialization,
	 * and end up calling the End method directly.In such cases, we
	 * don't have any iterator to end.
	 */
	if (SCAN_INIT == scanState->scan_state)
	{
		return;
	}

	Scan *scan = (Scan *)scanState->ps.plan;

	DynamicTableScanInfo *partitionInfo = scanState->ps.state->dynamicTableScanInfo;

	Assert(partitionInfo->numScans >= scan->partIndex);
	DynamicPartitionIterator *iterator = partitionInfo->iterators[scan->partIndex - 1];

	Assert(NULL != iterator);

	if (iterator->shouldCallHashSeqTerm)
	{
		hash_seq_term(iterator->partitionIterator);
	}

	pfree(iterator->partitionIterator);

	MemoryContextDelete(iterator->partitionMemoryContext);

	pfree(iterator);

	partitionInfo->iterators[scan->partIndex - 1] = NULL;
}

/*
 * DynamicScan_CleanupOneRelation
 *		Cleans up a relation and releases all locks.
 */
static inline void
DynamicScan_CleanupOneRelation(ScanState *scanState, Relation relation, PartitionEndMethod *partitionEndMethod)
{
	Assert(NULL != scanState);
	Assert(scanState->ss_currentRelation == relation || NULL == scanState->ss_currentRelation);

	if (NULL == relation)
	{
		return;
	}

	if (NULL != scanState->ss_currentRelation)
	{
		partitionEndMethod(scanState);
		scanState->ss_currentRelation = NULL;
	}

	ExecCloseScanRelation(relation);
}

/*
 * DynamicScan_RemapExpression
 * 		Re-maps the expression using the provided attMap.
 */
bool
DynamicScan_RemapExpression(ScanState *scanState, AttrNumber *attMap, Node *expr)
{
	if (!isDynamicScan((Scan *)scanState->ps.plan))
	{
		return false;
	}

	if (NULL != attMap)
	{
		change_varattnos_of_a_varno((Node*)expr, attMap, ((Scan *)scanState->ps.plan)->scanrelid);
		return true;
	}

	return false;
}

/*
 * DynamicScan_GetIterator
 * 		Returns the current iterator for the scanState's partIndex.
 */
DynamicPartitionIterator*
DynamicScan_GetIterator(ScanState *scanState)
{
	Assert(isDynamicScan((Scan *)scanState->ps.plan));
	Assert(NULL != scanState->ps.state->dynamicTableScanInfo);

	int partIndex = ((Scan*)scanState->ps.plan)->partIndex;
	Assert(partIndex <= scanState->ps.state->dynamicTableScanInfo->numScans);

	DynamicPartitionIterator *iterator = scanState->ps.state->dynamicTableScanInfo->iterators[partIndex - 1];

	Assert(NULL != iterator);

	return iterator;
}

/*
 * DynamicScan_Begin
 *		Prepares for the iteration of one or more relations (partitions).
 */
void
DynamicScan_Begin(ScanState *scanState, Plan *plan, EState *estate, int eflags)
{
	/* Currently only BitmapTableScanState supports this unified iteration of partitions */
	Assert(T_BitmapTableScanState == scanState->ps.type);

	Assert(SCAN_INIT == scanState->scan_state);

	InitScanStateInternal(scanState, plan, estate, eflags, false /* Do not open the relation. We open it later, per-partition */);
}

/*
 * DynamicScan_ReScan
 *		Prepares for the rescanning of one or more relations (partitions).
 */
void
DynamicScan_ReScan(ScanState *scanState, ExprContext *exprCtxt)
{
	/* Only BitmapTableScanState supports this unified iteration of partitions */
	Assert(T_BitmapTableScanState == scanState->ps.type);

	Assert(scanState->tableType >= 0 && scanState->tableType < TableTypeInvalid);

	/*
	 * If we are being passed an outer tuple, link it into the "regular"
	 * per-tuple econtext for possible qual eval.
	 */
	if (exprCtxt != NULL)
	{
		ExprContext *stdecontext = scanState->ps.ps_ExprContext;
		stdecontext->ecxt_outertuple = exprCtxt->ecxt_outertuple;
	}

	EState *estate = scanState->ps.state;
	Index scanrelid = ((Scan *)(scanState->ps.plan))->scanrelid;

	/* If this is re-scanning of PlanQual ... */
	if (estate->es_evTuple != NULL &&
		estate->es_evTuple[scanrelid - 1] != NULL)
	{
		estate->es_evTupleNull[scanrelid - 1] = false;
	}

	/* Notify controller about the request for rescan */
	DynamicScan_Controller(scanState, SCAN_RESCAN, NULL /* PartitionInitMethod */,
			NULL /* PartitionEndMethod */, NULL /* PartitionReScanMethod */);
}

/*
 * DynamicScan_End
 *		Ends partition/relation iteration/scanning and cleans up everything.
 */
void
DynamicScan_End(ScanState *scanState, PartitionEndMethod *partitionEndMethod)
{
	Assert(NULL != scanState);
	Assert(T_BitmapTableScanState == scanState->ps.type);

	if (SCAN_END == scanState->scan_state)
	{
		return;
	}

	Scan *scan = (Scan *)scanState->ps.plan;

	DynamicScan_CleanupOneRelation(scanState, scanState->ss_currentRelation, partitionEndMethod);

	if (isDynamicScan(scan))
	{
		DynamicScan_EndIterator(scanState);
	}

	FreeScanRelationInternal(scanState, false /* Do not close the relation. We closed it in DynamicScan_CleanupOneRelation */);

	scanState->scan_state = SCAN_END;
}

/*
 * DynamicScan_InitNextRelation
 *		Initializes states to process the next relation (if any).
 *		For partitioned scan we advance the iterator and prepare
 *		the next partition to scan. For non-partitioned case, we
 *		just start with the *only* relation, if we haven't
 *		already processed that relation.
 *
 *		Returns false if we don't have any more partition to iterate.
 */
static bool
DynamicScan_InitNextRelation(ScanState *scanState, PartitionInitMethod *partitionInitMethod, PartitionEndMethod *partitionEndMethod, PartitionReScanMethod *partitionReScanMethod)
{
	Assert(T_BitmapTableScanState == scanState->ps.type);

	Scan *scan = (Scan *)scanState->ps.plan;

	if (isDynamicScan(scan))
	{
		return DynamicScan_InitNextPartition(scanState, partitionInitMethod, partitionEndMethod, partitionReScanMethod);
	}

	return DynamicScan_InitSingleRelation(scanState, partitionInitMethod, partitionReScanMethod);
}

/*
 * DynamicScan_StateToString
 *		Returns string representation of an ScanStatus enum
 */
static char*
DynamicScan_StateToString(ScanStatus status)
{
	switch (status)
	{
		case SCAN_INIT:
			return "Init";
		case SCAN_FIRST:
			return "First";
		case SCAN_SCAN:
			return "Scan";
		case SCAN_MARKPOS:
			return "MarkPos";
		case SCAN_NEXT:
			return "Next";
		case SCAN_DONE:
			return "Done";
		case SCAN_RESCAN:
			return "ReScan";
		case SCAN_END:
			return "End";
		default:
			return "Unknown";
	}
}

/*
 * DynamicScan_RewindIterator
 *		Rewinds the iterator for a new scan of all the parts
 */
static void
DynamicScan_RewindIterator(ScanState *scanState)
{
	if (!isDynamicScan((Scan *)scanState->ps.plan))
	{
		return;
	}

	/*
	 * For EXPLAIN of a plan, we may never finish the initialization,
	 * and end up calling the End method directly.In such cases, we
	 * don't have any iterator to end.
	 */
	if (SCAN_INIT == scanState->scan_state)
	{
		DynamicScan_CreateIterator(scanState, (Scan *)scanState->ps.plan);
		return;
	}

	Scan *scan = (Scan *)scanState->ps.plan;

	DynamicTableScanInfo *partitionInfo = scanState->ps.state->dynamicTableScanInfo;

	Assert(partitionInfo->numScans >= scan->partIndex);
	DynamicPartitionIterator *iterator = partitionInfo->iterators[scan->partIndex - 1];

	Assert(NULL != iterator);

	if (iterator->shouldCallHashSeqTerm)
	{
		hash_seq_term(iterator->partitionIterator);
	}

	pfree(iterator->partitionIterator);

	iterator->partitionOids = partitionInfo->pidIndexes[scan->partIndex - 1];
	Assert(iterator->partitionOids != NULL);
	iterator->shouldCallHashSeqTerm = true;

	HASH_SEQ_STATUS *partitionIterator = palloc(sizeof(HASH_SEQ_STATUS));
	hash_seq_init(partitionIterator, iterator->partitionOids);

	iterator->partitionIterator = partitionIterator;

	Assert(iterator == partitionInfo->iterators[scan->partIndex - 1]);
}

/*
 * DynamicScan_StateTransitionError
 *		Errors out for an invalid state transition
 */
static void
DynamicScan_StateTransitionError(ScanStatus startState, ScanStatus endState)
{
	elog(ERROR, "Unknown state transition: %s to %s", \
	DynamicScan_StateToString(startState), DynamicScan_StateToString(endState));

}

/*
 * DynamicScan_Controller
 *		Controller function to transition from one scan status to another.
 *
 *		This function does not handle SCAN_END, which is exclusively controlled
 *		by the DynamicScan_End method
 */
static ScanStatus
DynamicScan_Controller(ScanState *scanState, ScanStatus desiredState, PartitionInitMethod *partitionInitMethod,
		PartitionEndMethod *partitionEndMethod, PartitionReScanMethod *partitionReScanMethod)
{
	ScanStatus startState = scanState->scan_state;

	/* Controller doesn't handle any state transition related to SCAN_END */
	if (SCAN_END == desiredState || SCAN_END == startState)
	{
		DynamicScan_StateTransitionError(startState, desiredState);
	}

	if (SCAN_SCAN == desiredState && (SCAN_INIT == startState || SCAN_NEXT == startState || SCAN_RESCAN == startState))
	{
		Assert(SCAN_DONE != startState || SCAN_END != startState);
		if (SCAN_INIT == startState || SCAN_RESCAN == startState)
		{
			DynamicScan_RewindIterator(scanState);
		}

		if (DynamicScan_InitNextRelation(scanState, partitionInitMethod, partitionEndMethod, partitionReScanMethod))
		{
			scanState->scan_state = SCAN_SCAN;
		}
		else
		{
			scanState->scan_state = SCAN_DONE;
		}
	}
	else if (SCAN_RESCAN == desiredState && SCAN_END != startState)
	{
		Assert(SCAN_END != startState);
		/* RESCAN shouldn't affect INIT, as nothing to rewind yet */
		if (SCAN_INIT != startState)
		{
			scanState->scan_state = SCAN_RESCAN;
		}
	}
	else if (SCAN_NEXT == desiredState && SCAN_SCAN == startState)
	{
		scanState->scan_state = isDynamicScan((Scan *)scanState->ps.plan) ? SCAN_NEXT : SCAN_DONE;
	}
	else
	{
		DynamicScan_StateTransitionError(scanState->scan_state, desiredState);
	}

	return scanState->scan_state;
}

/*
 * DynamicScan_GetNextTuple
 *		Gets the next tuple. If it needs to open a new relation,
 *		it takes care of that by asking for the next relation
 *		using DynamicScan_GetNextRelation.
 *
 *		Returns the tuple fetched, or a NULL tuple
 *		if it exhausts all the relations/partitions.
 */
TupleTableSlot *
DynamicScan_GetNextTuple(ScanState *scanState, PartitionInitMethod *partitionInitMethod,
		PartitionEndMethod *partitionEndMethod, PartitionReScanMethod *partitionReScanMethod,
		PartitionScanTupleMethod *partitionScanTupleMethod)
{
	TupleTableSlot *slot = NULL;

	while (TupIsNull(slot) && (SCAN_SCAN == scanState->scan_state ||
			SCAN_SCAN == DynamicScan_Controller(scanState, SCAN_SCAN, partitionInitMethod,
					partitionEndMethod, partitionReScanMethod)))
	{
		slot = partitionScanTupleMethod(scanState);

		if (TupIsNull(slot))
		{
			/* The underlying scanner should not change the scan status */
			Assert(SCAN_SCAN == scanState->scan_state);

			if (SCAN_DONE == DynamicScan_Controller(scanState, SCAN_NEXT, partitionInitMethod, partitionEndMethod, partitionReScanMethod) ||
					SCAN_SCAN != DynamicScan_Controller(scanState, SCAN_SCAN, partitionInitMethod, partitionEndMethod, partitionReScanMethod))
			{
				break;
			}

			Assert(SCAN_SCAN == scanState->scan_state);
		}
	}

	return slot;
}

/*
 * DynamicScan_GetPartitionMemoryContext
 * 		Returns the current partition's (if any) memory context.
 */
MemoryContext
DynamicScan_GetPartitionMemoryContext(ScanState *scanState)
{
	Assert(T_BitmapTableScanState == scanState->ps.type);

	Scan *scan = (Scan *)scanState->ps.plan;

	/*
	 * TODO rahmaf2 05/08/2014 [JIRA: MPP-23513] We currently
	 * return NULL memory context during initialization. So,
	 * for partitioned case, we will leak memory for all the
	 * expression initialization allocations during ExecInit.
	 */
	if (!isDynamicScan(scan) || SCAN_INIT == scanState->scan_state)
	{
		return NULL;
	}

	DynamicPartitionIterator *iterator = DynamicScan_GetIterator(scanState);

	Assert(NULL != iterator);
	return iterator->partitionMemoryContext;
}


/*
 * isDynamicScan
 * 		Returns true if the scan node is dynamic (i.e., determining
 * 		relations to scan at runtime).
 */
bool
isDynamicScan(const Scan *scan)
{
	return (scan->partIndex != INVALID_PART_INDEX);
}

/*
 * DynamicScan_GetTableOid
 * 		Returns the Oid of the table/partition to scan.
 *
 *		For partitioned case this method returns InvalidOid
 *		if the partition iterator hasn't been initialized yet.
 */
Oid
DynamicScan_GetTableOid(ScanState *scanState)
{
	/* For non-partitioned scan, just lookup the RTE */
	if (!isDynamicScan((Scan *)scanState->ps.plan))
	{
		return getrelid(((Scan *)scanState->ps.plan)->scanrelid, scanState->ps.state->es_range_table);
	}

	/* We are yet to initialize the iterator, so return InvalidOid */
	if (SCAN_INIT == scanState->scan_state)
	{
		return InvalidOid;
	}

	/* Get the iterator and look up the oid of the current relation */
	DynamicPartitionIterator *iterator = DynamicScan_GetIterator(scanState);
	Assert(NULL != iterator);
	Assert(OidIsValid(iterator->curRelOid));

	return iterator->curRelOid;
}
