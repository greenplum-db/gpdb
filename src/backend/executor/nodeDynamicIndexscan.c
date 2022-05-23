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
	ListCell *lc;
	int i;

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
	 * These are not used for anything, we rely on the child IndexScan node
	 * to do all evaluation for us. But I think this is still needed to
	 * find and process any SubPlans. See comment in ExecInitIndexScan.
	 */
	dynamicIndexScanState->ss.ps.qual = ExecInitQual(node->indexscan.scan.plan.qual,
					 (PlanState *) dynamicIndexScanState);

	/*
	 * tuple table initialization
	 */
	Relation scanRel = ExecOpenScanRelation(estate, node->indexscan.scan.scanrelid, eflags);
	ExecInitScanTupleSlot(estate, &dynamicIndexScanState->ss, RelationGetDescr(scanRel), table_slot_callbacks(scanRel));

	/*
	 * Initialize result tuple type and projection info.
	 */
	ExecInitResultTypeTL(&dynamicIndexScanState->ss.ps);

	/*
	 * This context will be reset per-partition to free up per-partition
	 * copy of LogicalIndexInfo
	 */
	dynamicIndexScanState->partitionMemoryContext = AllocSetContextCreate(CurrentMemoryContext,
									 "DynamicIndexScanPerPartition",
									 ALLOCSET_DEFAULT_MINSIZE,
									 ALLOCSET_DEFAULT_INITSIZE,
									 ALLOCSET_DEFAULT_MAXSIZE);

	return dynamicIndexScanState;
}

static void
DynamicIndexScan_ReMapColumns(DynamicIndexScan *dIndexScan, Oid oldOid, Oid newOid)
{
	IndexScan *indexScan = &dIndexScan->indexscan;
	AttrNumber *attMap;

	Assert(OidIsValid(newOid));

	if (oldOid == newOid)
	{
		/*
		 * If we have only one partition and we are rescanning then we can
		 * have this scenario.
		 */
		return;
	}

	attMap = IndexScan_GetColumnMapping(oldOid, newOid);

	if (attMap)
	{
		/* Also map attrnos in targetlist and quals */
		change_varattnos_of_a_varno((Node *) indexScan->scan.plan.targetlist,
									attMap, indexScan->scan.scanrelid);
		change_varattnos_of_a_varno((Node *) indexScan->scan.plan.qual,
									attMap, indexScan->scan.scanrelid);
		change_varattnos_of_a_varno((Node *) indexScan->indexqual,
									attMap, indexScan->scan.scanrelid);
		change_varattnos_of_a_varno((Node *) indexScan->indexqualorig,
									attMap, indexScan->scan.scanrelid);

		pfree(attMap);
	}
}

/*
 * Find the correct index in the given partition, and create a IndexScan executor
 * node to scan it.
 */
static void
beginCurrentIndexScan(DynamicIndexScanState *node, EState *estate,
					  Oid tableOid)
{
	DynamicIndexScan *dynamicIndexScan = (DynamicIndexScan *) node->ss.ps.plan;
	Relation	currentRelation;
	Oid			indexOid;
	List	   *save_tupletable;
	MemoryContext oldCxt;

	/*
	 * open the base relation and acquire appropriate lock on it.
	 */
	currentRelation = table_open(tableOid, AccessShareLock);
	node->ss.ss_currentRelation = currentRelation;

	save_tupletable = estate->es_tupleTable;
	estate->es_tupleTable = NIL;
	oldCxt = MemoryContextSwitchTo(node->partitionMemoryContext);

	/*
	 * Re-map the index columns, per the new partition, and find the correct
	 * index.
	 */
	if (!OidIsValid(node->columnLayoutOid))
	{
		/* Very first partition */
		// Just get the direct parent, we don't support multi-level partitioning
		node->columnLayoutOid = get_partition_parent(tableOid);
	}
	DynamicIndexScan_ReMapColumns(dynamicIndexScan,
								  tableOid, node->columnLayoutOid);
	node->columnLayoutOid = tableOid;

	indexOid = index_get_partition(currentRelation, dynamicIndexScan->indexscan.indexid);
	if (!OidIsValid(indexOid))
		elog(ERROR, "failed to find index for partition \"%s\" in dynamic index scan",
			 RelationGetRelationName(currentRelation));

	node->indexScanState = ExecInitIndexScanForPartition(&dynamicIndexScan->indexscan, estate,
														 node->eflags,
														 currentRelation, indexOid);
	/* The IndexScan node takes ownership of currentRelation, and will close it when done */
	node->tuptable = estate->es_tupleTable;
	estate->es_tupleTable = save_tupletable;
	MemoryContextSwitchTo(oldCxt);

	if (node->outer_exprContext)
		ExecReScanIndexScan(node->indexScanState);
}

static void
endCurrentIndexScan(DynamicIndexScanState *node)
{
	if (node->indexScanState)
	{
		ExecEndIndexScan(node->indexScanState);
		node->indexScanState = NULL;
		table_close(node->ss.ss_currentRelation, NoLock);
	}
	ExecResetTupleTable(node->tuptable, true);
	node->tuptable = NIL;
	MemoryContextReset(node->partitionMemoryContext);
}

/*
 * This function initializes a part and returns true if a new index has been prepared for scanning.
 */
static bool
initNextIndexToScan(DynamicIndexScanState *node)
{
	EState *estate = node->ss.ps.state;

	/* Load new index when the scanning of the previous index is done. */
	if (node->scan_state == SCAN_INIT ||
		node->scan_state == SCAN_DONE)
	{
		/* This is the oid of a partition of the table (*not* index) */
		Oid			tableOid;
		if (++node->whichPart < node->nOids)
			tableOid = node->partOids[node->whichPart];
		else
			return false;

		/* Collect number of partitions scanned in EXPLAIN ANALYZE */
		if (node->ss.ps.instrument)
			node->ss.ps.instrument->numPartScanned ++;

		endCurrentIndexScan(node);

		beginCurrentIndexScan(node, estate, tableOid);

		node->scan_state = SCAN_SCAN;
	}

	return true;
}

/*
 * Execution of DynamicIndexScan
 */
TupleTableSlot *
ExecDynamicIndexScan(PlanState *pstate)
{
	DynamicIndexScanState *node = castNode(DynamicIndexScanState, pstate);
	TupleTableSlot *slot = NULL;

	DynamicIndexScan	   *plan = (DynamicIndexScan *) node->ss.ps.plan;
	node->as_valid_subplans = NULL;
	if (NULL != plan->join_prune_paramids && !node->did_pruning)
	{
		node->did_pruning = true;
		node->as_valid_subplans =
				ExecFindMatchingSubPlans(node->as_prune_state,
										 node->ss.ps.state,
										 list_length(plan->partOids),
										 plan->join_prune_paramids);

		int i;
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
	 * Scan index to find next tuple to return. If the current index
	 * is exhausted, close it and open the next index for scan.
	 */
	while (TupIsNull(slot) &&
		   initNextIndexToScan(node))
	{
		slot = ExecProcNode(&node->indexScanState->ss.ps);

		if (TupIsNull(slot))
		{
			endCurrentIndexScan(node);

			node->scan_state = SCAN_INIT;
		}
	}
	return slot;
}

/*
 * Release resources of DynamicIndexScan
 */
void
ExecEndDynamicIndexScan(DynamicIndexScanState *node)
{
	endCurrentIndexScan(node);

	node->scan_state = SCAN_END;

	MemoryContextDelete(node->partitionMemoryContext);
}

/*
 * Allow rescanning an index.
 */
void
ExecReScanDynamicIndex(DynamicIndexScanState *node)
{
	if (node->indexScanState)
	{
		ExecEndIndexScan(node->indexScanState);
		node->indexScanState = NULL;
	}
	node->scan_state = SCAN_INIT;
	// reset partition internal state
	node->whichPart = -1;
}

/*
 * IndexScan_GetColumnMapping
 *             Returns the mapping of columns between two relation Oids because of
 *             dropped attributes.
 *
 *             Returns NULL for identical mapping.
 */
AttrNumber*
IndexScan_GetColumnMapping(Oid oldOid, Oid newOid)
{
	if (oldOid == newOid)
		return NULL;

	AttrNumber	  *attMap;

	Relation oldRel = heap_open(oldOid, AccessShareLock);
	Relation newRel = heap_open(newOid, AccessShareLock);

	TupleDesc oldTupDesc = oldRel->rd_att;
	TupleDesc newTupDesc = newRel->rd_att;

	attMap = convert_tuples_by_name_map_if_req(oldTupDesc, newTupDesc, "unused msg");

	heap_close(oldRel, AccessShareLock);
	heap_close(newRel, AccessShareLock);

	return attMap;
}
