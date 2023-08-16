/*-------------------------------------------------------------------------
 *
 * nodeDynamicIndexOnlyscan.c
 *	  Support routines for scanning one or more indexes that are
 *	  determined at runtime.
 *
 * DynamicIndexOnlyScan node scans each index one after the other. For each
 * index, it creates a regular IndexOnlyScan executor node, scans, and returns
 * the relevant tuples.
 *
 * Copyright (C) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeDynamicIndexOnlyscan.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/partition.h"
#include "executor/executor.h"
#include "executor/execDynamicIndexes.h"
#include "executor/instrument.h"
#include "nodes/execnodes.h"
#include "executor/execPartition.h"
#include "executor/nodeIndexonlyscan.h"
#include "executor/nodeDynamicIndexOnlyscan.h"
#include "access/table.h"
#include "access/tableam.h"
#include "utils/memutils.h"
#include "utils/rel.h"

/*
 * Initialize ScanState in DynamicIndexOnlyScan.
 */
DynamicIndexScanState *
ExecInitDynamicIndexOnlyScan(DynamicIndexOnlyScan *node, EState *estate, int eflags)
{
	DynamicIndexScanState *dynamicIndexOnlyScanState;
	ListCell   *lc;
	int			i;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	dynamicIndexOnlyScanState = makeNode(DynamicIndexScanState);
	dynamicIndexOnlyScanState->ss.ps.plan = (Plan *) node;
	dynamicIndexOnlyScanState->ss.ps.state = estate;
	dynamicIndexOnlyScanState->ss.ps.ExecProcNode = ExecDynamicIndexOnlyScan;
	dynamicIndexOnlyScanState->eflags = eflags;

	dynamicIndexOnlyScanState->scan_state = SCAN_INIT;
	dynamicIndexOnlyScanState->whichPart = -1;
	dynamicIndexOnlyScanState->nOids = list_length(node->partOids);
	dynamicIndexOnlyScanState->partOids = palloc(sizeof(Oid) * dynamicIndexOnlyScanState->nOids);
	foreach_with_count(lc, node->partOids, i)
		dynamicIndexOnlyScanState->partOids[i] = lfirst_oid(lc);

	/*
	 * Initialize child expressions
	 *
	 * These are not used for anything, we rely on the child IndexOnlyScan
	 * node to do all evaluation for us. But I think this is still needed to
	 * find and process any SubPlans. See comment in ExecInitIndexOnlyScan.
	 */
	dynamicIndexOnlyScanState->ss.ps.qual = ExecInitQual(node->indexscan.scan.plan.qual,
														 (PlanState *) dynamicIndexOnlyScanState);

	/*
	 * tuple table initialization
	 */
	Relation	scanRel = ExecOpenScanRelation(estate, node->indexscan.scan.scanrelid, eflags);

	ExecInitScanTupleSlot(estate, &dynamicIndexOnlyScanState->ss, RelationGetDescr(scanRel), table_slot_callbacks(scanRel));

	/*
	 * Initialize result tuple type and projection info.
	 */
	ExecInitResultTypeTL(&dynamicIndexOnlyScanState->ss.ps);

	/*
	 * This context will be reset per-partition to free up per-partition copy
	 * of LogicalIndexInfo
	 */
	dynamicIndexOnlyScanState->partitionMemoryContext = AllocSetContextCreate(CurrentMemoryContext,
																			  "DynamicIndexOnlyScanPerPartition",
																			  ALLOCSET_DEFAULT_MINSIZE,
																			  ALLOCSET_DEFAULT_INITSIZE,
																			  ALLOCSET_DEFAULT_MAXSIZE);

	return dynamicIndexOnlyScanState;
}

/*
 * Execution of DynamicIndexOnlyScan
 */
TupleTableSlot *
ExecDynamicIndexOnlyScan(PlanState *pstate)
{
	DynamicIndexScanState *node = castNode(DynamicIndexScanState, pstate);

	DynamicIndexOnlyScan *plan = (DynamicIndexOnlyScan *) node->ss.ps.plan;

	node->as_valid_subplans = NULL;
	if (NULL != plan->join_prune_paramids && !node->did_pruning)
	{
		node->did_pruning = true;
		node->as_valid_subplans =
			ExecFindMatchingSubPlans(node->as_prune_state,
									 node->ss.ps.state,
									 list_length(plan->partOids),
									 plan->join_prune_paramids);

		int			i;
		int			partOidIdx = -1;
		List	   *newPartOids = NIL;
		ListCell   *lc;

		for (i = 0; i < bms_num_members(node->as_valid_subplans); i++)
		{
			partOidIdx = bms_next_member(node->as_valid_subplans, partOidIdx);
			newPartOids = lappend_oid(newPartOids, node->partOids[partOidIdx]);
		}

		node->partOids = palloc(sizeof(Oid) * list_length(newPartOids));
		foreach_with_count(lc, newPartOids, i)
			node->partOids[i] = lfirst_oid(lc);
		node->nOids = list_length(newPartOids);
	}

	return ExecNextDynamicIndexScan(node);
}
