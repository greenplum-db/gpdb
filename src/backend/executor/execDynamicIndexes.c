/*-------------------------------------------------------------------------
 *
 * execDynamicIndexes.c
 *	  Common support routines for scanning one or more indexes that are
 *	  determined at runtime.
 *
 * Functions called by nodeDynamicIndexscan.c and nodeDynamicIndexOnlyscan.c.
 *
 * Copyright (C) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/execDynamicIndexes.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/partition.h"
#include "executor/executor.h"
#include "executor/instrument.h"
#include "nodes/execnodes.h"
#include "executor/execDynamicIndexes.h"
#include "executor/execPartition.h"
#include "executor/nodeIndexscan.h"
#include "executor/nodeIndexonlyscan.h"
#include "executor/nodeDynamicIndexOnlyscan.h"
#include "executor/nodeDynamicIndexscan.h"
#include "access/table.h"
#include "access/tableam.h"
#include "utils/memutils.h"
#include "utils/rel.h"

/*
 * GetColumnMapping
 *             Returns the mapping of columns between two relation Oids because of
 *             dropped attributes.
 *
 *             Returns NULL for identical mapping.
 */
AttrNumber *
GetColumnMapping(Oid oldOid, Oid newOid)
{
	Assert(OidIsValid(newOid));

	if (oldOid == newOid)
	{
		/*
		 * If we have only one partition and we are rescanning then we can
		 * have this scenario.
		 */
		return NULL;
	}

	AttrNumber *attMap;

	Relation	oldRel = heap_open(oldOid, AccessShareLock);
	Relation	newRel = heap_open(newOid, AccessShareLock);

	TupleDesc	oldTupDesc = oldRel->rd_att;
	TupleDesc	newTupDesc = newRel->rd_att;

	attMap = convert_tuples_by_name_map_if_req(oldTupDesc, newTupDesc, "unused msg");

	heap_close(oldRel, AccessShareLock);
	heap_close(newRel, AccessShareLock);

	return attMap;
}

static void
DynamicIndexScan_ReMapColumns(DynamicIndexScan *dIndexScan, Oid oldOid, Oid newOid)
{
	IndexScan  *indexScan = &dIndexScan->indexscan;
	AttrNumber *attMap;

	attMap = GetColumnMapping(oldOid, newOid);

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

static void
DynamicIndexOnlyScan_ReMapColumns(DynamicIndexOnlyScan *dIndexOnlyScan, Oid oldOid, Oid newOid)
{
	IndexOnlyScan *indexScan = &dIndexOnlyScan->indexscan;
	AttrNumber *attMap;

	attMap = GetColumnMapping(oldOid, newOid);

	if (attMap)
	{
		/* Also map attrnos in targetlist and quals */
		change_varattnos_of_a_varno((Node *) indexScan->scan.plan.targetlist,
									attMap, indexScan->scan.scanrelid);
		change_varattnos_of_a_varno((Node *) indexScan->scan.plan.qual,
									attMap, indexScan->scan.scanrelid);
		change_varattnos_of_a_varno((Node *) indexScan->indexqual,
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
	bool		is_indexonly_scan = nodeTag(node->ss.ps.plan) == T_DynamicIndexOnlyScan;
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
		/*
		 * Just get the direct parent, we don't support multi-level
		 * partitioning
		 */
		node->columnLayoutOid = get_partition_parent(tableOid);
	}
	if (is_indexonly_scan)
	{
		DynamicIndexOnlyScan *dynamicIndexOnlyScan = (DynamicIndexOnlyScan *) node->ss.ps.plan;

		DynamicIndexOnlyScan_ReMapColumns(dynamicIndexOnlyScan,
										  tableOid, node->columnLayoutOid);
		node->columnLayoutOid = tableOid;

		indexOid = index_get_partition(currentRelation, dynamicIndexOnlyScan->indexscan.indexid);
		if (!OidIsValid(indexOid))
			elog(ERROR, "failed to find index for partition \"%s\" in dynamic index scan",
				 RelationGetRelationName(currentRelation));

		node->indexOnlyScanState = ExecInitIndexOnlyScanForPartition(&dynamicIndexOnlyScan->indexscan, estate,
																	 node->eflags,
																	 currentRelation, indexOid);
		node->indexScanState = NULL;
	}
	else
	{
		DynamicIndexScan *dynamicIndexScan = (DynamicIndexScan *) node->ss.ps.plan;

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
		node->indexOnlyScanState = NULL;
	}

	/*
	 * The IndexScan node takes ownership of currentRelation, and will close
	 * it when done
	 */
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
	else if (node->indexOnlyScanState)
	{
		ExecEndIndexOnlyScan(node->indexOnlyScanState);
		node->indexOnlyScanState = NULL;
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
	EState	   *estate = node->ss.ps.state;

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
			node->ss.ps.instrument->numPartScanned++;

		endCurrentIndexScan(node);

		beginCurrentIndexScan(node, estate, tableOid);

		node->scan_state = SCAN_SCAN;
	}

	return true;
}

TupleTableSlot *
ExecNextDynamicIndexScan(DynamicIndexScanState *node)
{
	TupleTableSlot *slot = NULL;

	/*
	 * Scan index to find next tuple to return. If the current index is
	 * exhausted, close it and open the next index for scan.
	 */
	for (;;)
	{
		if (!initNextIndexToScan(node))
			return NULL;

		Assert((node->indexScanState == NULL && node->indexOnlyScanState != NULL) ||
			   (node->indexScanState != NULL && node->indexOnlyScanState == NULL));

		if (node->indexScanState)
			slot = ExecProcNode(&node->indexScanState->ss.ps);
		else if (node->indexOnlyScanState)
			slot = ExecProcNode(&node->indexOnlyScanState->ss.ps);
		else
			return NULL;

		if (TupIsNull(slot))
		{
			endCurrentIndexScan(node);

			node->scan_state = SCAN_INIT;
		}
		else
			break;
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
	else if (node->indexOnlyScanState)
	{
		ExecEndIndexOnlyScan(node->indexOnlyScanState);
		node->indexOnlyScanState = NULL;
	}
	node->scan_state = SCAN_INIT;
	/* reset partition internal state */
	node->whichPart = -1;
}
