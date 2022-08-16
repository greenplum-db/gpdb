/*-------------------------------------------------------------------------
 *
 * nodeDynamicBitmapIndexscan.c
 *	  Support routines for bitmap-scanning a partition of a table, where
 *	  the partition is determined at runtime.
 *
 * This is a thin wrapper around a regular non-dynamic Bitmap Index Scan
 * executor node. The Begin function doesn't do much. But when
 * MultiExecDynamicBitmapIndexScan() is called, to get the result,
 * we initialize a BitmapIndexScanState executor node for the currently
 * open partition, and call MultiExecBitmapIndexScan() on it. On rescan,
 * the underlying BitmapIndexScanState is destroyed.
 *
 * This is somewhat different from a Dynamic Index Scan. While a Dynamic
 * Index Scan needs to iterate through all the active partitions, a Dynamic
 * Bitmap Index Scan works as a slave of a dynamic Bitmap Heap Scan node
 * above it. It scans only one partition at a time, but the partition
 * can change at a rescan.
 *
 * Portions Copyright (c) 2013 - present, EMC/Greenplum
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeDynamicBitmapIndexscan.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/partition.h"
#include "executor/executor.h"
#include "executor/instrument.h"
#include "nodes/execnodes.h"
#include "executor/nodeBitmapIndexscan.h"
#include "executor/nodeDynamicIndexscan.h"
#include "executor/nodeDynamicBitmapIndexscan.h"
#include "access/table.h"
#include "access/tableam.h"
#include "utils/memutils.h"
#include "utils/rel.h"

/*
 * Initialize ScanState in DynamicBitmapIndexScan.
 */
DynamicBitmapIndexScanState *
ExecInitDynamicBitmapIndexScan(DynamicBitmapIndexScan *node, EState *estate, int eflags)
{
	DynamicBitmapIndexScanState *dynamicBitmapIndexScanState;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	dynamicBitmapIndexScanState = makeNode(DynamicBitmapIndexScanState);
	dynamicBitmapIndexScanState->ss.ps.plan = (Plan *) node;
	dynamicBitmapIndexScanState->ss.ps.state = estate;
	dynamicBitmapIndexScanState->ss.ps.ExecProcNode = ExecDynamicIndexScan;
	dynamicBitmapIndexScanState->eflags = eflags;

	dynamicBitmapIndexScanState->scan_state = SCAN_INIT;

	/*
	 * Initialize child expressions
	 *
	 * These are not used for anything, we rely on the child IndexScan node
	 * to do all evaluation for us. But I think this is still needed to
	 * find and process any SubPlans. See comment in ExecInitIndexScan.
	 */
	dynamicBitmapIndexScanState->ss.ps.qual =
			ExecInitQual(node->biscan.scan.plan.qual,
						 (PlanState *) dynamicBitmapIndexScanState);

	/*
	 * tuple table initialization
	 */
	Relation scanRel = ExecOpenScanRelation(estate, node->biscan.scan.scanrelid, eflags);
	ExecInitScanTupleSlot(estate, &dynamicBitmapIndexScanState->ss, RelationGetDescr(scanRel), table_slot_callbacks(scanRel));

	/*
	 * Initialize result tuple type and projection info.
	 */
	ExecInitResultTypeTL(&dynamicBitmapIndexScanState->ss.ps);

	/*
	 * This context will be reset per-partition to free up per-partition
	 * copy of LogicalIndexInfo
	 */
	dynamicBitmapIndexScanState->partitionMemoryContext = AllocSetContextCreate(CurrentMemoryContext,
									 "DynamicBitmapIndexScanPerPartition",
									 ALLOCSET_DEFAULT_MINSIZE,
									 ALLOCSET_DEFAULT_INITSIZE,
									 ALLOCSET_DEFAULT_MAXSIZE);

	return dynamicBitmapIndexScanState;
}

static void
BitmapIndexScan_ReMapColumns(DynamicBitmapIndexScan *dbiScan, Oid oldOid, Oid newOid)
{
	AttrNumber *attMap;

	Assert(OidIsValid(newOid));

	if (oldOid == newOid)
	{
		/*
		 * If we have only one partition and we are rescanning
		 * then we can have this scenario.
		 */
		return;
	}

	attMap = IndexScan_GetColumnMapping(oldOid, newOid);

	if (attMap)
	{
		/* A bitmap index scan has no target list or quals */
		// FIXME: no quals remapping?

		pfree(attMap);
	}
}


/*
 * Find the correct index in the given partition, and create a BitmapIndexScan executor
 * node to scan it.
 */
static void
beginCurrentBitmapIndexScan(DynamicBitmapIndexScanState *node, EState *estate,
							Oid tableOid)
{
	DynamicBitmapIndexScan *dbiScan = (DynamicBitmapIndexScan *) node->ss.ps.plan;
	Relation	currentRelation;
	Oid			indexOid;
	MemoryContext oldCxt;
	List	   *save_tupletable;


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
		node->columnLayoutOid = get_partition_parent(tableOid);
	}
	BitmapIndexScan_ReMapColumns(dbiScan, node->columnLayoutOid, tableOid);
	node->columnLayoutOid = tableOid;

	/*
	 * The is the oid of the partition of an *index*. Note: a partitioned table
	 * has a root and a set of partitions (may be multi-level). An index
	 * on a partitioned table also has a root and a set of index partitions.
	 * We started at table level, and now we are fetching the oid of an index
	 * partition.
	 */
	Oid indexOidOld = dbiScan->biscan.indexid;
	indexOid = index_get_partition(currentRelation, dbiScan->biscan.indexid);
	if (!OidIsValid(indexOid))
		elog(ERROR, "failed to find index for partition \"%s\" for \"%d\"in dynamic index scan",
			 RelationGetRelationName(currentRelation), dbiScan->biscan.indexid);

	table_close(currentRelation, NoLock);
	/* Modify the plan node with the index ID */
	dbiScan->biscan.indexid = indexOid;

	node->bitmapIndexScanState = ExecInitBitmapIndexScan(&dbiScan->biscan,
														 estate,
														 node->eflags);

	dbiScan->biscan.indexid = indexOidOld;
	if (node->ss.ps.instrument)
	{
		/* Let the BitmapIndexScan share our Instrument node */
		node->bitmapIndexScanState->ss.ps.instrument = node->ss.ps.instrument;
	}

	node->tuptable = estate->es_tupleTable;
	estate->es_tupleTable = save_tupletable;
	MemoryContextSwitchTo(oldCxt);

	if (node->outer_exprContext)
		ExecReScanBitmapIndexScan(node->bitmapIndexScanState);
}

/*
 * End the currently open BitmapIndexScan executor node, if any.
 */
static void
endCurrentBitmapIndexScan(DynamicBitmapIndexScanState *node)
{
	if (node->bitmapIndexScanState)
	{
		ExecEndBitmapIndexScan(node->bitmapIndexScanState);
		node->bitmapIndexScanState = NULL;
	}
	ExecResetTupleTable(node->tuptable, true);
	node->tuptable = NIL;
	MemoryContextReset(node->partitionMemoryContext);
}

/*
 * Execution of DynamicBitmapIndexScan
 */
Node *
MultiExecDynamicBitmapIndexScan(DynamicBitmapIndexScanState *node)
{
	EState	   *estate = node->ss.ps.state;
	Oid			tableOid;

	/* close previously open scan, if any. */
	endCurrentBitmapIndexScan(node);

	/*
	 * Fetch the OID of the current partition, and of the index on
	 * that partition to scan.
	 */
	if (estate->partitionOid > 0) {
		tableOid = estate->partitionOid;
	} else {
		return NULL;
	}

	/*
	 * Create the underlying regular BitmapIndexScan executor node,
	 * for the current partition, and call it.
	 *
	 * Note: don't close the BitmapIndexScan executor node yet,
	 * because it might return a streaming bitmap, which still needs
	 * the underlying scan if more tuples are pulled from it after
	 * we return.
	 */
	beginCurrentBitmapIndexScan(node, estate, tableOid);

	return MultiExecBitmapIndexScan(node->bitmapIndexScanState);
}

/*
 * Release resources of DynamicBitmapIndexScan
 */
void
ExecEndDynamicBitmapIndexScan(DynamicBitmapIndexScanState *node)
{
	endCurrentBitmapIndexScan(node);
	node->scan_state = SCAN_END;
	MemoryContextDelete(node->partitionMemoryContext);
}

/*
 * Allow rescanning an index.
 *
 * The current partition might've changed.
 */
void
ExecReScanDynamicBitmapIndex(DynamicBitmapIndexScanState *node)
{
	if (node->bitmapIndexScanState)
	{
		ExecEndBitmapIndexScan(node->bitmapIndexScanState);
		node->bitmapIndexScanState = NULL;
	}
	node->scan_state = SCAN_INIT;
}
