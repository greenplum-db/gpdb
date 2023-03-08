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
#include "executor/nodeBitmapHeapscan.h"
#include "executor/nodeDynamicIndexscan.h"
#include "executor/nodeDynamicBitmapHeapscan.h"
#include "executor/execPartition.h"
#include "nodes/execnodes.h"
#include "utils/rel.h"
#include "utils/memutils.h"
#include "access/table.h"
#include "access/tableam.h"

static void CleanupOnePartition(DynamicBitmapHeapScanState *node);

DynamicBitmapHeapScanState *
ExecInitDynamicBitmapHeapScan(DynamicBitmapHeapScan *node, EState *estate, int eflags)
{
	DynamicBitmapHeapScanState *state;
	Oid			reloid;
	int			i;
	ListCell *lc;

	Assert((eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)) == 0);

	state = makeNode(DynamicBitmapHeapScanState);
	state->eflags = eflags;
	state->ss.ps.plan = (Plan *) node;
	state->ss.ps.state = estate;
	state->ss.ps.ExecProcNode = ExecDynamicBitmapHeapScan;
	state->did_pruning = false;
	state->scan_state = SCAN_INIT;

	state->whichPart = -1;
	state->nOids = list_length(node->partOids);
	state->partOids = palloc(sizeof(Oid) * state->nOids);
	foreach_with_count(lc, node->partOids, i)
		state->partOids[i] = lfirst_oid(lc);

	/*
	 * tuple table initialization
	 */
	Relation scanRel = ExecOpenScanRelation(estate, node->bitmapheapscan.scan.scanrelid, eflags);
	ExecInitScanTupleSlot(estate, &state->ss, RelationGetDescr(scanRel), table_slot_callbacks(scanRel));

	state->ss.ps.qual = ExecInitQual(node->bitmapheapscan.scan.plan.qual, (PlanState *) state);

	/*
	 * Initialize result tuple type and projection info.
	 */
	ExecInitResultTypeTL(&state->ss.ps);

	reloid = exec_rt_fetch(node->bitmapheapscan.scan.scanrelid, estate)->relid;
	Assert(OidIsValid(reloid));

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

	state->as_prune_state = NULL;

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
	Oid		   pid;
	Relation	currentRelation;

	if (++node->whichPart < node->nOids)
		pid = node->partOids[node->whichPart];
	else
		return false;

	estate->partitionOid = pid;

	/* Collect number of partitions scanned in EXPLAIN ANALYZE */
	if (NULL != scanState->ps.instrument)
	{
		Instrumentation *instr = scanState->ps.instrument;
		instr->numPartScanned++;
	}

	currentRelation = scanState->ss_currentRelation =
			table_open(node->partOids[node->whichPart], AccessShareLock);

	if (currentRelation->rd_rel->relkind != RELKIND_RELATION)
	{
		/* shouldn't happen */
		elog(ERROR, "unexpected relkind in Dynamic Scan: %c", currentRelation->rd_rel->relkind);
	}
	lastScannedRel = table_open(node->lastRelOid, AccessShareLock);
	lastTupDesc = RelationGetDescr(lastScannedRel);
	partTupDesc = RelationGetDescr(scanState->ss_currentRelation);
	/*
	 * FIXME: should we use execute_attr_map_tuple instead? Seems like a
	 * higher level abstraction that fits the bill
	 */
	attMap = convert_tuples_by_name_map_if_req(partTupDesc, lastTupDesc, "unused msg");
	table_close(lastScannedRel, AccessShareLock);

	/* If attribute remapping is not necessary, then do not change the varattno */
	if (attMap)
	{
		change_varattnos_of_a_varno((Node*)scanState->ps.plan->qual, attMap, node->scanrelid);
		change_varattnos_of_a_varno((Node*)scanState->ps.plan->targetlist, attMap, node->scanrelid);

		/*
		 * Now that the varattno mapping has been changed, change the relation that
		 * the new varnos correspond to
		 */
		node->lastRelOid = pid;
		pfree(attMap);
	}

	node->bhsState = ExecInitBitmapHeapScanForPartition(&plan->bitmapheapscan, estate, node->eflags,
													 currentRelation);

	/*
	 * Rescan the child node, and attach it to the sidecar BitmapHeapScan.
	 */
	ExecReScan(outerPlanState(node));
	outerPlanState(node->bhsState) = outerPlanState(node);

	return true;
}

TupleTableSlot *
ExecDynamicBitmapHeapScan(PlanState *pstate)
{
	DynamicBitmapHeapScanState *node = castNode(DynamicBitmapHeapScanState, pstate);
	TupleTableSlot *slot = NULL;

	DynamicBitmapHeapScan 	   *plan = (DynamicBitmapHeapScan *) node->ss.ps.plan;
	node->as_valid_subplans = NULL;
	if (NULL != plan->join_prune_paramids && !node->did_pruning)
	{
		node->did_pruning = true;
		node->as_valid_subplans =
				ExecFindMatchingSubPlans(node->as_prune_state,
										 node->ss.ps.state,
										 list_length(plan->partOids),
										 plan->join_prune_paramids);

		int i = 0;
		int partOidIdx = -1;
		List *newPartOids = NIL;
		ListCell *lc;
		for(i = 0; i < bms_num_members(node->as_valid_subplans); i++)
		{
			partOidIdx = bms_next_member(node->as_valid_subplans, partOidIdx);
			newPartOids = lappend_oid(newPartOids, node->partOids[partOidIdx]);
		}

		node->partOids = palloc(sizeof(Oid) * list_length(newPartOids));
		foreach_with_count(lc, newPartOids, i)
			node->partOids[i] = lfirst_oid(lc);
		node->nOids = list_length(newPartOids);
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

		slot = ExecProcNode(&node->bhsState->ss.ps);

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
		Assert(scanState->ss.ss_currentRelation != NULL);
		table_close(scanState->ss.ss_currentRelation, NoLock);
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

	if (node->ss.ps.ps_ResultTupleSlot)
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

	// reset partition internal state
	/*
	 * If any PARAM_EXEC Params used in pruning expressions have changed, then
	 * we'd better unset the valid subplans so that they are reselected for
	 * the new parameter values.
	 */
	if (node->as_prune_state &&
		bms_overlap(node->ss.ps.chgParam,
					node->as_prune_state->execparamids))
	{
		bms_free(node->as_valid_subplans);
		node->as_valid_subplans = NULL;
	}
	node->whichPart = -1;
}
