/*-------------------------------------------------------------------------
 *
 * nodeDynamicForeignscan.c
 *	  Support routines for scanning one or more foreign relations, including
 *    dynamic partition elimination if corresponding partition selector(s) are
 *    present
 *
 * DynamicForeignScan node scans each relation one after the other. For each
 * relation, it opens the table, scans the tuple, and returns relevant tuples.
 * This is fairly similar in structure to nodeDynamicSeqScan.c
 *
 * This has a smaller plan size than using an append with many partitions.
 * Instead of determining the column mapping for each partition during planning,
 * this mapping is determined during execution. When there are many partitions
 * with many columns, the plan size improvement becomes very large, on the order of
 * 100+ MB in some cases. This node also populates the fdw_private field for each
 * partition. This is determined during planning, as it requires calling out to
 * the fdw api. If we did this during execution, it would need to be done for each
 * partition on each segment, which would negatively impact fdws that don't support
 * many simultaneous calls
 *
 *
 *
 * Copyright (C) 2023 VMware Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeDynamicForeignscan.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "executor/executor.h"
#include "executor/instrument.h"
#include "nodes/execnodes.h"
#include "executor/execPartition.h"
#include "executor/nodeDynamicForeignscan.h"
#include "executor/nodeForeignscan.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "access/table.h"
#include "access/tableam.h"

static void CleanupOnePartition(DynamicForeignScanState *node);

DynamicForeignScanState *
ExecInitDynamicForeignScan(DynamicForeignScan *node, EState *estate, int eflags)
{
	DynamicForeignScanState *state;
	Oid			reloid;
	ListCell *lc;
	int i;
	void **fdw_private_array; /* array of fdw_private*/

	Assert((eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)) == 0);

	state = makeNode(DynamicForeignScanState);
	state->eflags = eflags;
	state->ss.ps.plan = (Plan *) node;
	state->ss.ps.state = estate;
	state->ss.ps.ExecProcNode = ExecDynamicForeignScan;
	state->did_pruning = false;
	state->scan_state = SCAN_INIT;

	/* Initialize child expressions. This is needed to find subplans. */
	state->ss.ps.qual =
		ExecInitQual(node->foreignscan.scan.plan.qual, (PlanState *) state);

	Relation scanRel = ExecOpenScanRelation(estate, node->foreignscan.scan.scanrelid, eflags);
	ExecInitScanTupleSlot(estate, &state->ss, RelationGetDescr(scanRel), table_slot_callbacks(scanRel));

	/* Initialize result tuple type. */
	ExecInitResultTypeTL(&state->ss.ps);
	ExecAssignScanProjectionInfo(&state->ss);

	state->nOids = list_length(node->partOids);
	state->partOids = palloc(sizeof(Oid) * state->nOids);
	foreach_with_count(lc, node->partOids, i)
		state->partOids[i] = lfirst_oid(lc);
	state->whichPart = -1;

	reloid = exec_rt_fetch(node->foreignscan.scan.scanrelid, estate)->relid;
	Assert(OidIsValid(reloid));

	/* lastRelOid is used to remap varattno for heterogeneous partitions */
	state->lastRelOid = reloid;

	state->scanrelid = node->foreignscan.scan.scanrelid;

	state->as_prune_state = NULL;

	/* populate fdw_private array from list so we can access by index later */
	fdw_private_array = (void **) palloc(state->nOids * sizeof(void *));
	i = 0;
	foreach_with_count(lc, node->fdw_private_list, i)
		fdw_private_array[i] = (void *) lfirst(lc);;
	state->fdw_private_array = fdw_private_array;

	/*
	 * This context will be reset per-partition to free up per-partition
	 * qual and targetlist allocations
	 */
	state->partitionMemoryContext = AllocSetContextCreate(CurrentMemoryContext,
									 "DynamicForeignScanPerPartition",
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
 * If no more tables are found, this function returns false.
 */
static bool
initNextTableToScan(DynamicForeignScanState *node)
{
	ScanState  *scanState = (ScanState *) node;
	DynamicForeignScan *plan = (DynamicForeignScan *) scanState->ps.plan;
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

	if (currentRelation->rd_rel->relkind != RELKIND_FOREIGN_TABLE)
	{
		/* shouldn't happen */
		elog(ERROR, "unexpected relkind in Dynamic Foreign Scan: %c", currentRelation->rd_rel->relkind);
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
	RangeTblEntry *rte = estate->es_range_table_array[node->scanrelid-1];
	rte->relid = *pid;
	plan->foreignscan.fdw_private = node->fdw_private_array[node->whichPart];
	node->foreignScanState = ExecInitForeignScanForPartition(&plan->foreignscan, estate, node->eflags,
													 currentRelation);
	return true;
}


TupleTableSlot *
ExecDynamicForeignScan(PlanState *pstate)
{
	DynamicForeignScanState *node = castNode(DynamicForeignScanState, pstate);
	TupleTableSlot *slot = NULL;

	DynamicForeignScan	   *plan = (DynamicForeignScan *) node->ss.ps.plan;
	node->as_valid_subplans = NULL;
	if (NULL != plan->join_prune_paramids && !node->did_pruning)
	{
		node->did_pruning = true;
		node->as_valid_subplans =
			ExecFindMatchingSubPlans(node->as_prune_state,
									 node->ss.ps.state,
									 list_length(plan->partOids),
									 plan->join_prune_paramids);

		int	i = 0;
		int	partOidIdx = -1;
		node->nOids = bms_num_members(node->as_valid_subplans);
		List	*newPartOids = NIL;
		void	**new_fdw_private_array; /* array of fdw_private*/
		ListCell	*lc;
		/* Need to re-populate fdw_private_array based on dynamically eliminated partitions */
		new_fdw_private_array = (void **) palloc(node->nOids * sizeof(void *));
		for(i = 0; i < node->nOids; i++)
		{
			partOidIdx = bms_next_member(node->as_valid_subplans, partOidIdx);
			newPartOids = lappend_oid(newPartOids, node->partOids[partOidIdx]);
			new_fdw_private_array[i] = node->fdw_private_array[partOidIdx];
		}
		pfree(node->partOids);
		pfree(node->fdw_private_array);
		node->partOids = palloc(sizeof(Oid) * node->nOids);
		foreach_with_count(lc, newPartOids, i)
			node->partOids[i] = lfirst_oid(lc);
		node->fdw_private_array = new_fdw_private_array;
	}

	/*
	 * Scan the table to find next tuple to return. If the current table
	 * is finished, close it and open the next table for scan.
	 */
	for (;;)
	{
		if (!node->foreignScanState)
		{
			/* No partition open. Open the next one, if any. */
			if (!initNextTableToScan(node))
				break;
		}

		slot = ExecProcNode(&node->foreignScanState->ss.ps);

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
CleanupOnePartition(DynamicForeignScanState *scanState)
{
	Assert(NULL != scanState);

	if (scanState->foreignScanState)
	{
		ExecEndForeignScan(scanState->foreignScanState);
		scanState->foreignScanState = NULL;
		Assert(scanState->ss.ss_currentRelation != NULL);
		table_close(scanState->ss.ss_currentRelation, NoLock);
		scanState->ss.ss_currentRelation = NULL;
	}
}

/*
 * DynamicForeignScanEndCurrentScan
 *		Cleans up any ongoing scan.
 */
static void
DynamicForeignScanEndCurrentScan(DynamicForeignScanState *node)
{
	CleanupOnePartition(node);
}

/*
 * ExecEndDynamicForeignScan
 *		Ends the scanning of this DynamicForeignScanNode and frees
 *		up all the resources.
 */
void
ExecEndDynamicForeignScan(DynamicForeignScanState *node)
{
	DynamicForeignScanEndCurrentScan(node);

	if (node->ss.ps.ps_ResultTupleSlot)
		ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
}

/*
 * ExecReScanDynamicForeignScan
 *		Prepares the internal states for a rescan.
 */
void
ExecReScanDynamicForeignScan(DynamicForeignScanState *node)
{
	DynamicForeignScanEndCurrentScan(node);

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

	// reset partition internal state
	node->whichPart = -1;

}
