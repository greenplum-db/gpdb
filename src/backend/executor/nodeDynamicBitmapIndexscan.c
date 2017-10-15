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
 * NOTE: There is no DynamicBitmapIndexScan plan node. A BitmapIndexScan
 * plan node is backed by either a BitmapIndexScanState or a
 * DynamicIndexScanState executor node, depending on whether the
 * BitmapIndexScan is beneath a dynamic or non-dynamic BitmapHeapScan node
 * The choice between the two is in ExecInitNode().
 *
 * Portions Copyright (c) 2013 - present, EMC/Greenplum
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeDynamicBitmapIndexscan.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "cdb/cdbpartition.h"
#include "cdb/cdbvars.h"
#include "cdb/partitionselection.h"
#include "executor/executor.h"
#include "executor/instrument.h"
#include "nodes/execnodes.h"
#include "executor/nodeBitmapIndexscan.h"
#include "executor/execDynamicScan.h"
#include "executor/nodeDynamicIndexscan.h"
#include "executor/nodeDynamicBitmapIndexscan.h"
#include "access/genam.h"
#include "nodes/nodeFuncs.h"
#include "utils/memutils.h"

/*
 * Initialize ScanState in DynamicBitmapIndexScan.
 */
DynamicBitmapIndexScanState *
ExecInitDynamicBitmapIndexScan(BitmapIndexScan *node, EState *estate, int eflags)
{
	DynamicBitmapIndexScanState *dynamicBitmapIndexScanState;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	dynamicBitmapIndexScanState = makeNode(DynamicBitmapIndexScanState);
	dynamicBitmapIndexScanState->ss.ps.plan = (Plan *) node;
	dynamicBitmapIndexScanState->ss.ps.state = estate;
	dynamicBitmapIndexScanState->eflags = eflags;

	dynamicBitmapIndexScanState->ss.scan_state = SCAN_INIT;

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
BitmapIndexScan_ReMapColumns(BitmapIndexScan *indexScan, Oid oldOid, Oid newOid)
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
		LogicalIndexInfo *logicalIndexInfo = indexScan->logicalIndexInfo;

		IndexScan_MapLogicalIndexInfo(logicalIndexInfo, attMap, indexScan->scan.scanrelid);

		/* A bitmap index scan has no target list or quals */

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
	BitmapIndexScan *bitmapIndexScan = (BitmapIndexScan *) node->ss.ps.plan;
	Relation	currentRelation;
	Oid			indexOid;
	MemoryContext oldCxt;

	oldCxt = MemoryContextSwitchTo(node->partitionMemoryContext);

	/*
	 * Re-map the index columns, per the new partition, and find the correct
	 * index.
	 */
	if (!OidIsValid(node->columnLayoutOid))
	{
		/* Very first partition */
		node->columnLayoutOid = rel_partition_get_root(tableOid);
	}
	BitmapIndexScan_ReMapColumns(bitmapIndexScan,
								 node->columnLayoutOid, tableOid);
	node->columnLayoutOid = tableOid;

	/*
	 * The is the oid of the partition of an *index*. Note: a partitioned table
	 * has a root and a set of partitions (may be multi-level). An index
	 * on a partitioned table also has a root and a set of index partitions.
	 * We started at table level, and now we are fetching the oid of an index
	 * partition.
	 */
	currentRelation = OpenScanRelationByOid(tableOid);
	indexOid = getPhysicalIndexRelid(currentRelation, bitmapIndexScan->logicalIndexInfo);
	if (!OidIsValid(indexOid))
		elog(ERROR, "failed to find index for partition \"%s\" in dynamic index scan",
			 RelationGetRelationName(currentRelation));
	ExecCloseScanRelation(currentRelation);

	/* Modify the plan node with the index ID */
	bitmapIndexScan->indexid = indexOid;

	node->bitmapIndexScanState = ExecInitBitmapIndexScan(bitmapIndexScan,
														 estate,
														 node->eflags);

	if (node->ss.ps.instrument)
	{
		/* Let the BitmapIndexScan share our Instrument node */
		node->bitmapIndexScanState->ss.ps.instrument = node->ss.ps.instrument;
	}

	MemoryContextSwitchTo(oldCxt);

	if (node->outer_exprContext)
		ExecBitmapIndexReScan(node->bitmapIndexScanState, node->outer_exprContext);
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
	node->ss.scan_state = SCAN_FIRST;
	tableOid = DynamicScan_GetTableOid(&node->ss);

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

	EndPlanStateGpmonPkt(&node->ss.ps);

	MemoryContextDelete(node->partitionMemoryContext);
}

/*
 * Allow rescanning an index.
 *
 * The current partition might've changed.
 */
void
ExecDynamicBitmapIndexReScan(DynamicBitmapIndexScanState *node, ExprContext *exprCtxt)
{
	if (node->bitmapIndexScanState)
	{
		ExecEndBitmapIndexScan(node->bitmapIndexScanState);
		node->bitmapIndexScanState = NULL;
	}

	/*
	 * If we are being passed an outer tuple, save it so that we can
	 * pass it on to the underlying Index Scan when we create it.
	 */
	if (exprCtxt)
		node->outer_exprContext = exprCtxt;

	node->ss.scan_state = SCAN_INIT;

	CheckSendPlanStateGpmonPkt(&node->ss.ps);
}
