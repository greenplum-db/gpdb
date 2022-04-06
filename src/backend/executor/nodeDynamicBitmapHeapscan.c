/*-------------------------------------------------------------------------
 *
 * nodeDynamicBitmapHeapscan.c
 *	  Routines to support bitmapped scans of relations
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 * Portions Copyright (c) 2008-2009, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeDynamicBitmapHeapscan.c
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		ExecDynamicBitmapHeapScan	scans a relation using bitmap info
 *		ExecDynamicBitmapHeapNext	workhorse for above
 *		ExecInitDynamicBitmapHeapScan		creates and initializes state info.
 *		ExecReScanDynamicBitmapHeapScan	prepares to rescan the plan.
 *		ExecEndDynamicBitmapHeapScan		releases all storage.
 */
#include "postgres.h"

#include "executor/executor.h"
#include "executor/instrument.h"
#include "executor/execDynamicScan.h"
#include "executor/nodeBitmapHeapscan.h"
#include "executor/nodeDynamicBitmapHeapscan.h"
#include "nodes/execnodes.h"
#include "utils/hsearch.h"
#include "parser/parsetree.h"
#include "utils/memutils.h"
#include "cdb/cdbvars.h"
#include "cdb/partitionselection.h"

static void CleanupOnePartition(DynamicBitmapHeapScanState *node);

DynamicBitmapHeapScanState *
ExecInitDynamicBitmapHeapScan(DynamicBitmapHeapScan *node, EState *estate, int eflags)
{
	DynamicBitmapHeapScanState *state;
	Oid			reloid;

	Assert((eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)) == 0);

	state = makeNode(DynamicBitmapHeapScanState);
	state->eflags = eflags;
	state->ss.ps.plan = (Plan *) node;
	state->ss.ps.state = estate;

	state->scan_state = SCAN_INIT;

	/* Initialize result tuple type. */
	ExecInitResultTupleSlot(estate, &state->ss.ps);
	ExecAssignResultTypeFromTL(&state->ss.ps);

	reloid = getrelid(node->bitmapheapscan.scan.scanrelid, estate->es_range_table);
	Assert(OidIsValid(reloid));

	state->firstPartition = true;

	/* lastRelOid is used to remap varattno for heterogeneous partitions */
	state->lastRelOid = reloid;

	state->scanrelid = node->bitmapheapscan.scan.scanrelid;

	/*
	 * This context will be reset per-partition to free up per-partition
	 * qual and targetlist allocations
	 */
	state->partitionMemoryContext = AllocSetContextCreate(CurrentMemoryContext,
									 "DynamicBitmapHeapScanPerPartition",
									 ALLOCSET_DEFAULT_MINSIZE,
									 ALLOCSET_DEFAULT_INITSIZE,
									 ALLOCSET_DEFAULT_MAXSIZE);

	/*
	 * initialize child nodes.
	 *
	 * We will initialize the "sidecar" BitmapHeapScan for each partition, but
	 * the child nodes, (i.e. Dynamic Bitmap Index Scan or a BitmapAnd/Or node)
	 * we only rescan.
	 */
	outerPlanState(state) = ExecInitNode(outerPlan(node), estate, eflags);

	return state;
}

/*
 * initNextTableToScan
 *   Find the next table to scan and initiate the scan if the previous table
 * is finished.
 *
 * If scanning on the current table is not finished, or a new table is found,
 * this function returns true.
 * If no more table is found, this function returns false.
 */
static bool
initNextTableToScan(DynamicBitmapHeapScanState *node)
{
	ScanState  *scanState = (ScanState *)node;
	DynamicBitmapHeapScan *plan = (DynamicBitmapHeapScan *) scanState->ps.plan;
	EState	   *estate = scanState->ps.state;
	Relation	lastScannedRel;
	TupleDesc	partTupDesc;
	TupleDesc	lastTupDesc;
	AttrNumber *attMap;
	Oid			currentPartitionOid;
	Oid		   *pid;
	Relation	currentRelation;

	pid = hash_seq_search(&node->pidStatus);
	if (pid == NULL)
	{
		node->shouldCallHashSeqTerm = false;
		return false;
	}
	currentPartitionOid = *pid;

	/* Collect number of partitions scanned in EXPLAIN ANALYZE */
	if (NULL != scanState->ps.instrument)
	{
		Instrumentation *instr = scanState->ps.instrument;
		instr->numPartScanned++;
	}

	CleanupOnePartition(node);

	currentRelation = scanState->ss_currentRelation = heap_open(currentPartitionOid, AccessShareLock);
	lastScannedRel = heap_open(node->lastRelOid, AccessShareLock);
	lastTupDesc = RelationGetDescr(lastScannedRel);
	partTupDesc = RelationGetDescr(scanState->ss_currentRelation);
	attMap = varattnos_map(lastTupDesc, partTupDesc);
	heap_close(lastScannedRel, AccessShareLock);

	/* If attribute remapping is not necessary, then do not change the varattno */
	if (attMap)
	{
		change_varattnos_of_a_varno((Node*)scanState->ps.plan->qual, attMap, node->scanrelid);
		change_varattnos_of_a_varno((Node*)scanState->ps.plan->targetlist, attMap, node->scanrelid);

		/*
		 * Now that the varattno mapping has been changed, change the relation that
		 * the new varnos correspond to
		 */
		node->lastRelOid = currentPartitionOid;
	}

	/*
	 * For the very first partition, the targetlist of planstate is set to null. So, we must
	 * initialize quals and targetlist, regardless of remapping requirements. For later
	 * partitions, we only initialize quals and targetlist if a column re-mapping is necessary.
	 */
	if (attMap || node->firstPartition)
	{
		node->firstPartition = false;
		MemoryContextReset(node->partitionMemoryContext);
		MemoryContext oldCxt = MemoryContextSwitchTo(node->partitionMemoryContext);

		/* Initialize child expressions */
		scanState->ps.qual = (List *) ExecInitExpr((Expr *) scanState->ps.plan->qual,
												   (PlanState *) scanState);
		scanState->ps.targetlist = (List *) ExecInitExpr((Expr *) scanState->ps.plan->targetlist,
														 (PlanState *) scanState);

		MemoryContextSwitchTo(oldCxt);
	}

	if (attMap)
		pfree(attMap);

	DynamicScan_SetTableOid(&node->ss, currentPartitionOid);
	node->bhsState = ExecInitBitmapHeapScanForPartition(&plan->bitmapheapscan, estate, node->eflags,
													 currentRelation);

	/*
	 * Rescan the child node, and attach it to the sidecar BitmapHeapScan.
	 */
	ExecReScan(outerPlanState(node));
	outerPlanState(node->bhsState) = outerPlanState(node);

	return true;
}

/*
 * setPidIndex
 *   Set the pid index for the given dynamic table.
 */
static void
setPidIndex(DynamicBitmapHeapScanState *node)
{
	Assert(node->pidIndex == NULL);

	ScanState *scanState = (ScanState *)node;
	EState *estate = scanState->ps.state;
	DynamicBitmapHeapScan *plan = (DynamicBitmapHeapScan *) scanState->ps.plan;

	Assert(estate->dynamicTableScanInfo != NULL);

	/*
	 * Ensure that the dynahash exists even if the partition selector
	 * didn't choose any partition for current scan node [MPP-24169].
	 */
	InsertPidIntoDynamicTableScanInfo(estate, plan->partIndex,
									  InvalidOid, InvalidPartitionSelectorId);

	Assert(NULL != estate->dynamicTableScanInfo->pidIndexes);
	Assert(estate->dynamicTableScanInfo->numScans >= plan->partIndex);
	node->pidIndex = estate->dynamicTableScanInfo->pidIndexes[plan->partIndex - 1];
	Assert(node->pidIndex != NULL);
}

TupleTableSlot *
ExecDynamicBitmapHeapScan(DynamicBitmapHeapScanState *node)
{
	TupleTableSlot *slot = NULL;

	/*
	 * If this is called the first time, find the pid index that contains all unique
	 * partition pids for this node to scan.
	 */
	if (node->pidIndex == NULL)
	{
		setPidIndex(node);
		Assert(node->pidIndex != NULL);

		hash_seq_init(&node->pidStatus, node->pidIndex);
		node->shouldCallHashSeqTerm = true;
	}

	/*
	 * Scan the table to find next tuple to return. If the current table
	 * is finished, close it and open the next table for scan.
	 */
	for (;;)
	{
		if (!node->bhsState)
		{
			/* No partition open. Open the next one, if any. */
			if (!initNextTableToScan(node))
				break;
		}

		slot = ExecBitmapHeapScan(node->bhsState);

		if (!TupIsNull(slot))
			break;

		/* No more tuples from this partition. Move to next one. */
		CleanupOnePartition(node);
	}

	return slot;
}

/*
 * CleanupOnePartition
 *		Cleans up a partition's relation and releases all locks.
 */
static void
CleanupOnePartition(DynamicBitmapHeapScanState *scanState)
{
	Assert(NULL != scanState);

	if (scanState->bhsState)
	{
		/*
		 * Detach the child node, so that we end just the bitmap heap scan,
		 * not the children.
		 */
		outerPlanState(scanState->bhsState) = NULL;
		ExecEndBitmapHeapScan(scanState->bhsState);
		scanState->bhsState = NULL;
	}
	if ((scanState->scan_state & SCAN_SCAN) != 0)
	{
		Assert(scanState->ss.ss_currentRelation != NULL);
		ExecCloseScanRelation(scanState->ss.ss_currentRelation);
		scanState->ss.ss_currentRelation = NULL;
	}
}

/*
 * DynamicBitmapHeapScanEndCurrentScan
 *		Cleans up any ongoing scan.
 */
static void
DynamicBitmapHeapScanEndCurrentScan(DynamicBitmapHeapScanState *node)
{
	CleanupOnePartition(node);

	if (node->shouldCallHashSeqTerm)
	{
		hash_seq_term(&node->pidStatus);
		node->shouldCallHashSeqTerm = false;
	}
}

/*
 * ExecEndDynamicBitmapHeapScan
 *		Ends the scanning of this DynamicBitmapHeapScanNode and frees
 *		up all the resources.
 */
void
ExecEndDynamicBitmapHeapScan(DynamicBitmapHeapScanState *node)
{
	DynamicBitmapHeapScanEndCurrentScan(node);

	ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);

	/*
	 * close down subplans
	 */
	ExecEndNode(outerPlanState(node));
}

/*
 * ExecReScanDynamicBitmapHeapScan
 *		Prepares the internal states for a rescan.
 */
void
ExecReScanDynamicBitmapHeapScan(DynamicBitmapHeapScanState *node)
{
	DynamicBitmapHeapScanEndCurrentScan(node);

	/* Force reloading the partition hash table */
	node->pidIndex = NULL;
}
