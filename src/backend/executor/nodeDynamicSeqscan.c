/*-------------------------------------------------------------------------
 *
 * nodeDynamicSeqscan.c
 *	  Support routines for scanning one or more relations that are
 *	  determined at run time. The relations could be Heap, AppendOnly Row,
 *	  AppendOnly Columnar.
 *
 * DynamicSeqScan node scans each relation one after the other. For each
 * relation, it opens the table, scans the tuple, and returns relevant tuples.
 *
 * This has a smaller plan size than using an append with many partitions.
 * Instead of determining the column mapping for each partition during planning,
 * this mapping is determined during execution. When there are many partitions
 * with many columns, the plan size improvement becomes very large, on the order of
 * 100+ MB in some cases.
 *
 * Portions Copyright (c) 2012 - present, EMC/Greenplum
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeDynamicSeqscan.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "executor/executor.h"
#include "executor/instrument.h"
#include "nodes/execnodes.h"
#include "executor/execPartition.h"
#include "executor/nodeDynamicSeqscan.h"
#include "executor/nodeSeqscan.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "access/table.h"
#include "access/tableam.h"

static void CleanupOnePartition(DynamicSeqScanState *node);

DynamicSeqScanState *
ExecInitDynamicSeqScan(DynamicSeqScan *node, EState *estate, int eflags)
{
	DynamicSeqScanState *state;
	Oid			reloid;
	ListCell *lc;
	int i;

	Assert((eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)) == 0);

	state = makeNode(DynamicSeqScanState);
	state->eflags = eflags;
	state->ss.ps.plan = (Plan *) node;
	state->ss.ps.state = estate;
	state->ss.ps.ExecProcNode = ExecDynamicSeqScan;
	state->did_pruning = false;
	state->scan_state = SCAN_INIT;

	/* Initialize child expressions. This is needed to find subplans. */
	state->ss.ps.qual =
		ExecInitQual(node->seqscan.plan.qual, (PlanState *) state);

	Relation scanRel = ExecOpenScanRelation(estate, node->seqscan.scanrelid, eflags);
	ExecInitScanTupleSlot(estate, &state->ss, RelationGetDescr(scanRel), table_slot_callbacks(scanRel));

	/* Initialize result tuple type. */
	ExecInitResultTypeTL(&state->ss.ps);
	ExecAssignScanProjectionInfo(&state->ss);

	state->nOids = list_length(node->partOids);
	state->partOids = palloc(sizeof(Oid) * state->nOids);
	foreach_with_count(lc, node->partOids, i)
		state->partOids[i] = lfirst_oid(lc);
	state->whichPart = -1;

	reloid = exec_rt_fetch(node->seqscan.scanrelid, estate)->relid;
	Assert(OidIsValid(reloid));

	/* lastRelOid is used to remap varattno for heterogeneous partitions */
	state->lastRelOid = reloid;

	state->scanrelid = node->seqscan.scanrelid;

	state->as_prune_state = NULL;

	/*
	 * This context will be reset per-partition to free up per-partition
	 * qual and targetlist allocations
	 */
	state->partitionMemoryContext = AllocSetContextCreate(CurrentMemoryContext,
									 "DynamicSeqScanPerPartition",
									 ALLOCSET_DEFAULT_MINSIZE,
									 ALLOCSET_DEFAULT_INITSIZE,
									 ALLOCSET_DEFAULT_MAXSIZE);
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
initNextTableToScan(DynamicSeqScanState *node)
{
	ScanState  *scanState = (ScanState *) node;
	DynamicSeqScan *plan = (DynamicSeqScan *) scanState->ps.plan;
	EState	   *estate = scanState->ps.state;
	Relation	lastScannedRel;
	TupleDesc	partTupDesc;
	TupleDesc	lastTupDesc;
	AttrNumber *attMap;
	Oid		   *pid;
	Relation	currentRelation;

	if (++node->whichPart < node->nOids)
		pid = &node->partOids[node->whichPart];
	else
		return false;

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
		node->lastRelOid = *pid;
		pfree(attMap);
	}

	node->seqScanState = ExecInitSeqScanForPartition(&plan->seqscan, estate,
													 currentRelation);
	return true;
}


TupleTableSlot *
ExecDynamicSeqScan(PlanState *pstate)
{
	DynamicSeqScanState *node = castNode(DynamicSeqScanState, pstate);
	TupleTableSlot *slot = NULL;

	DynamicSeqScan	   *plan = (DynamicSeqScan *) node->ss.ps.plan;
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
		if (!node->seqScanState)
		{
			/* No partition open. Open the next one, if any. */
			if (!initNextTableToScan(node))
				break;
		}

		slot = ExecProcNode(&node->seqScanState->ss.ps);

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
CleanupOnePartition(DynamicSeqScanState *scanState)
{
	Assert(NULL != scanState);

	if (scanState->seqScanState)
	{
		ExecEndSeqScan(scanState->seqScanState);
		scanState->seqScanState = NULL;
		Assert(scanState->ss.ss_currentRelation != NULL);
		table_close(scanState->ss.ss_currentRelation, NoLock);
		scanState->ss.ss_currentRelation = NULL;
	}
}

/*
 * DynamicSeqScanEndCurrentScan
 *		Cleans up any ongoing scan.
 */
static void
DynamicSeqScanEndCurrentScan(DynamicSeqScanState *node)
{
	CleanupOnePartition(node);
}

/*
 * ExecEndDynamicSeqScan
 *		Ends the scanning of this DynamicSeqScanNode and frees
 *		up all the resources.
 */
void
ExecEndDynamicSeqScan(DynamicSeqScanState *node)
{
	DynamicSeqScanEndCurrentScan(node);

	if (node->ss.ps.ps_ResultTupleSlot)
		ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
}

/*
 * ExecReScanDynamicSeqScan
 *		Prepares the internal states for a rescan.
 */
void
ExecReScanDynamicSeqScan(DynamicSeqScanState *node)
{
	DynamicSeqScanEndCurrentScan(node);

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
