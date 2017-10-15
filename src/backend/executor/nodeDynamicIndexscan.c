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
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeDynamicIndexscan.c
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
#include "executor/nodeIndexscan.h"
#include "executor/nodeDynamicIndexscan.h"
#include "access/genam.h"
#include "nodes/nodeFuncs.h"
#include "utils/memutils.h"

/*
 * Account for the number of tuple slots required for DynamicIndexScan
 *
 * XXX: We have backported the PostgreSQL patch that made these functions
 * obsolete. The returned value isn't used for anything, so just return 0.
 */
int
ExecCountSlotsDynamicIndexScan(DynamicIndexScan *node)
{
	return 0;
}

/*
 * Initialize ScanState in DynamicIndexScan.
 */
DynamicIndexScanState *
ExecInitDynamicIndexScan(DynamicIndexScan *node, EState *estate, int eflags)
{
	DynamicIndexScanState *dynamicIndexScanState;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	dynamicIndexScanState = makeNode(DynamicIndexScanState);
	dynamicIndexScanState->ss.ps.plan = (Plan *) node;
	dynamicIndexScanState->ss.ps.state = estate;
	dynamicIndexScanState->eflags = eflags;

	dynamicIndexScanState->ss.scan_state = SCAN_INIT;

	/*
	 * Initialize child expressions
	 *
	 * These are not used for anything, we rely on the child IndexScan node
	 * to do all evaluation for us. But I think this is still needed to
	 * find and process any SubPlans. See comment in ExecInitIndexScan.
	 */
	dynamicIndexScanState->ss.ps.targetlist = (List *)
		ExecInitExpr((Expr *) node->scan.plan.targetlist,
					 (PlanState *) dynamicIndexScanState);
	dynamicIndexScanState->ss.ps.qual = (List *)
		ExecInitExpr((Expr *) node->scan.plan.qual,
					 (PlanState *) dynamicIndexScanState);
	(void) ExecInitExpr((Expr *) node->indexqualorig,
						(PlanState *) dynamicIndexScanState);

	/*
	 * tuple table initialization
	 */
	ExecInitResultTupleSlot(estate, &dynamicIndexScanState->ss.ps);

	/*
	 * Initialize result tuple type and projection info.
	 */
	ExecAssignResultTypeFromTL(&dynamicIndexScanState->ss.ps);

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
DynamicIndexScan_ReMapColumns(DynamicIndexScan *indexScan, Oid oldOid, Oid newOid)
{
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
		LogicalIndexInfo *logicalIndexInfo = indexScan->logicalIndexInfo;

		IndexScan_MapLogicalIndexInfo(logicalIndexInfo, attMap, indexScan->scan.scanrelid);

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
	currentRelation = OpenScanRelationByOid(tableOid);

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
		node->columnLayoutOid = rel_partition_get_root(tableOid);
	}
	DynamicIndexScan_ReMapColumns(dynamicIndexScan,
								  node->columnLayoutOid, tableOid);
	node->columnLayoutOid = tableOid;

	indexOid = getPhysicalIndexRelid(currentRelation, dynamicIndexScan->logicalIndexInfo);
	if (!OidIsValid(indexOid))
		elog(ERROR, "failed to find index for partition \"%s\" in dynamic index scan",
			 RelationGetRelationName(currentRelation));

	node->indexScanState = ExecInitIndexScanForPartition((IndexScan *) dynamicIndexScan, estate,
														 node->eflags,
														 currentRelation, indexOid);
	/* The IndexScan node takes ownership of currentRelation, and will close it when done */
	node->tuptable = estate->es_tupleTable;
	estate->es_tupleTable = save_tupletable;
	MemoryContextSwitchTo(oldCxt);

	if (node->outer_exprContext)
		ExecIndexReScan(node->indexScanState, node->outer_exprContext);
}

static void
endCurrentIndexScan(DynamicIndexScanState *node)
{
	if (node->indexScanState)
	{
		ExecEndIndexScan(node->indexScanState);
		node->indexScanState = NULL;
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
	if (node->ss.scan_state == SCAN_INIT ||
		node->ss.scan_state == SCAN_DONE)
	{
		/* This is the oid of a partition of the table (*not* index) */
		Oid			tableOid;
		Oid		   *pid;

		pid = hash_seq_search(&node->pidxStatus);
		if (pid == NULL)
		{
			/* Return if all parts have been scanned. */
			node->shouldCallHashSeqTerm = false;
			return false;
		}
		tableOid = *pid;

		/* Collect number of partitions scanned in EXPLAIN ANALYZE */
		if (node->ss.ps.instrument)
			node->ss.ps.instrument->numPartScanned ++;

		endCurrentIndexScan(node);

		beginCurrentIndexScan(node, estate, tableOid);

		node->ss.scan_state = SCAN_SCAN;
	}

	return true;
}

/*
 * setPidIndex
 *   Set the hash table of Oids to scan.
 */
static void
setPidIndex(DynamicIndexScanState *node)
{
	Assert(node->pidxIndex == NULL);

	IndexScanState *indexState = (IndexScanState *)node;
	EState *estate = indexState->ss.ps.state;
	DynamicIndexScan *plan = (DynamicIndexScan *)indexState->ss.ps.plan;
	Assert(estate->dynamicTableScanInfo != NULL);
	/*
	 * Ensure that the dynahash exists even if the partition selector
	 * didn't choose any partition for current scan node [MPP-24169].
	 */
	InsertPidIntoDynamicTableScanInfo(estate, plan->scan.partIndex,
									  InvalidOid, InvalidPartitionSelectorId);

	Assert(NULL != estate->dynamicTableScanInfo->pidIndexes);
	Assert(estate->dynamicTableScanInfo->numScans >= plan->scan.partIndex);
	node->pidxIndex = estate->dynamicTableScanInfo->pidIndexes[plan->scan.partIndex - 1];
	Assert(node->pidxIndex != NULL);
}

/*
 * Execution of DynamicIndexScan
 */
TupleTableSlot *
ExecDynamicIndexScan(DynamicIndexScanState *node)
{
	TupleTableSlot *slot = NULL;

	/*
	 * If this is called the first time, find the pid index that contains all unique
	 * partition pids for this node to scan.
	 */
	if (node->pidxIndex == NULL)
	{
		setPidIndex(node);
		Assert(node->pidxIndex != NULL);

		hash_seq_init(&node->pidxStatus, node->pidxIndex);
		node->shouldCallHashSeqTerm = true;
	}

	/*
	 * Scan index to find next tuple to return. If the current index
	 * is exhausted, close it and open the next index for scan.
	 */
	while (TupIsNull(slot) &&
		   initNextIndexToScan(node))
	{
		slot = ExecIndexScan(node->indexScanState);

		if (TupIsNull(slot))
		{
			endCurrentIndexScan(node);

			node->ss.scan_state = SCAN_INIT;
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

	node->ss.scan_state = SCAN_END;

	if (node->shouldCallHashSeqTerm)
	{
		hash_seq_term(&node->pidxStatus);
		node->shouldCallHashSeqTerm = false;
	}

	EndPlanStateGpmonPkt(&node->ss.ps);

	MemoryContextDelete(node->partitionMemoryContext);
}

/*
 * Allow rescanning an index.
 */
void
ExecDynamicIndexReScan(DynamicIndexScanState *node, ExprContext *exprCtxt)
{
	if (node->indexScanState)
	{
		ExecEndIndexScan(node->indexScanState);
		node->indexScanState = NULL;
	}
	node->ss.scan_state = SCAN_INIT;

	if (node->shouldCallHashSeqTerm)
	{
		hash_seq_term(&node->pidxStatus);
		node->shouldCallHashSeqTerm = false;
	}

	/*
	 * If we are being passed an outer tuple, save it so that we can
	 * pass it on to the underlying Index Scan when we create it.
	 */
	if (exprCtxt)
		node->outer_exprContext = exprCtxt;

	/* Force reloading the hash table */
	node->pidxIndex = NULL;

	CheckSendPlanStateGpmonPkt(&node->ss.ps);
}


/*
 * IndexScan_MapLogicalIndexInfo
 *             Remaps the columns of the expressions of a provided logicalIndexInfo
 *             using attMap for a given varno.
 *
 *             Returns true if mapping was done.
 */
bool
IndexScan_MapLogicalIndexInfo(LogicalIndexInfo *logicalIndexInfo, AttrNumber *attMap, Index varno)
{
	if (NULL == attMap)
	{
		/* Columns are already aligned, and therefore, no mapping is necessary */
		return false;
	}
	/* map the attrnos if necessary */
	for (int i = 0; i < logicalIndexInfo->nColumns; i++)
	{
		if (logicalIndexInfo->indexKeys[i] != 0)
		{
			logicalIndexInfo->indexKeys[i] = attMap[(logicalIndexInfo->indexKeys[i]) - 1];
		}
	}

	/* map the attrnos in the indExprs and indPred */
	change_varattnos_of_a_varno((Node *)logicalIndexInfo->indPred, attMap, varno);
	change_varattnos_of_a_varno((Node *)logicalIndexInfo->indExprs, attMap, varno);

	return true;
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

	AttrNumber      *attMap;

	Relation oldRel = heap_open(oldOid, AccessShareLock);
	Relation newRel = heap_open(newOid, AccessShareLock);

	TupleDesc oldTupDesc = oldRel->rd_att;
	TupleDesc newTupDesc = newRel->rd_att;

	attMap = varattnos_map(oldTupDesc, newTupDesc);

	heap_close(oldRel, AccessShareLock);
	heap_close(newRel, AccessShareLock);

	return attMap;
}
