/*-------------------------------------------------------------------------
 *
 * nodeDynamicIndexscan.c
 *	  Support routines for scanning one or more indexes that are
 *	  determined at runtime.
 *
 * DynamicIndexScan node scans each index one after the other. For each
 * index, it creates a regular IndexScan executor node, scans, and returns
 * the relevant tuples.
 *
 * Portions Copyright (c) 2013 - present, EMC/Greenplum
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeDynamicIndexscan.c
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
#include "executor/nodeIndexscan.h"
#include "executor/nodeDynamicIndexscan.h"
#include "access/table.h"
#include "access/tableam.h"
#include "utils/memutils.h"
#include "utils/rel.h"

/*
 * Initialize ScanState in DynamicIndexScan.
 */
DynamicIndexScanState *
ExecInitDynamicIndexScan(DynamicIndexScan *node, EState *estate, int eflags)
{
	DynamicIndexScanState *dynamicIndexScanState;
	ListCell   *lc;
	int			i;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	dynamicIndexScanState = makeNode(DynamicIndexScanState);
	dynamicIndexScanState->ss.ps.plan = (Plan *) node;
	dynamicIndexScanState->ss.ps.state = estate;
	dynamicIndexScanState->ss.ps.ExecProcNode = ExecDynamicIndexScan;
	dynamicIndexScanState->eflags = eflags;

	dynamicIndexScanState->scan_state = SCAN_INIT;
	dynamicIndexScanState->whichPart = -1;
	dynamicIndexScanState->nOids = list_length(node->partOids);
	dynamicIndexScanState->partOids = palloc(sizeof(Oid) * dynamicIndexScanState->nOids);
	foreach_with_count(lc, node->partOids, i)
		dynamicIndexScanState->partOids[i] = lfirst_oid(lc);

	/*
	 * Initialize child expressions
	 *
	 * These are not used for anything, we rely on the child IndexScan node to
	 * do all evaluation for us. But I think this is still needed to find and
	 * process any SubPlans. See comment in ExecInitIndexScan.
	 */
	dynamicIndexScanState->ss.ps.qual = ExecInitQual(node->indexscan.scan.plan.qual,
													 (PlanState *) dynamicIndexScanState);

	/*
	 * tuple table initialization
	 */
	Relation	scanRel = ExecOpenScanRelation(estate, node->indexscan.scan.scanrelid, eflags);

	ExecInitScanTupleSlot(estate, &dynamicIndexScanState->ss, RelationGetDescr(scanRel), table_slot_callbacks(scanRel));

	/*
	 * Initialize result tuple type and projection info.
	 */
	ExecInitResultTypeTL(&dynamicIndexScanState->ss.ps);

	/*
	 * This context will be reset per-partition to free up per-partition copy
	 * of LogicalIndexInfo
	 */
	dynamicIndexScanState->partitionMemoryContext = AllocSetContextCreate(CurrentMemoryContext,
																		  "DynamicIndexScanPerPartition",
																		  ALLOCSET_DEFAULT_MINSIZE,
																		  ALLOCSET_DEFAULT_INITSIZE,
																		  ALLOCSET_DEFAULT_MAXSIZE);

	return dynamicIndexScanState;
}

/*
 * Execution of DynamicIndexScan
 */
TupleTableSlot *
ExecDynamicIndexScan(PlanState *pstate)
{
	DynamicIndexScanState *node = castNode(DynamicIndexScanState, pstate);

	DynamicIndexScan *plan = (DynamicIndexScan *) node->ss.ps.plan;

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
