/*
 * nodeDynamicIndexscan.c 
 *	Support routines for scanning one or more indexes that
 *	are determined at runtime.
 *
 *	DynamicIndexScan node scans each index one after the other.
 *	For each index, it opens the index, scans the index, and returns
 *	relevant tuples.
 *
 * Copyright (c) 2013 - present, EMC/Greenplum
 */

#include "postgres.h"

#include "executor/executor.h"
#include "executor/instrument.h"
#include "nodes/execnodes.h"
#include "executor/execIndexscan.h"
#include "executor/nodeIndexscan.h"
#include "executor/execDynamicIndexScan.h"
#include "executor/nodeDynamicIndexscan.h"
#include "cdb/cdbpartition.h"
#include "parser/parsetree.h"
#include "access/genam.h"
#include "catalog/catquery.h"
#include "nodes/nodeFuncs.h"
#include "utils/memutils.h"
#include "cdb/cdbvars.h"

/*
 * Free resources from a partition.
 */
static inline void
CleanupOnePartition(IndexScanState *indexState);

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
	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	DynamicIndexScanState *dynamicIndexScanState = makeNode(DynamicIndexScanState);

	dynamicIndexScanState->indexScanState.ss.scan_state = SCAN_INIT;


	/*
	 * This context will be reset per-partition to free up per-partition
	 * copy of LogicalIndexInfo
	 */
	dynamicIndexScanState->partitionMemoryContext = AllocSetContextCreate(CurrentMemoryContext,
									 "DynamicIndexScanPerPartition",
									 ALLOCSET_DEFAULT_MINSIZE,
									 ALLOCSET_DEFAULT_INITSIZE,
									 ALLOCSET_DEFAULT_MAXSIZE);

	IndexScanState *indexState = &(dynamicIndexScanState->indexScanState);

	InitCommonIndexScanState((IndexScanState *)dynamicIndexScanState, (IndexScan *)node, estate, eflags);
	/* We don't support eager free in DynamicIndexScan */
	indexState->ss.ps.delayEagerFree = true;

	InitRuntimeKeysContext(indexState);

	initGpmonPktForDynamicIndexScan((Plan *)node, &indexState->ss.ps.gpmon_pkt, estate);

	return dynamicIndexScanState;
}

static bool
DynamicIndexScan_ReMapColumns(DynamicIndexScanState *scanState, Oid newOid)
{
	Assert(OidIsValid(newOid));

	IndexScan *indexScan = (IndexScan *) scanState->indexScanState.ss.ps.plan;

	if (!isDynamicScan(&indexScan->scan))
	{
		return false;
	}

	Oid oldOid = scanState->columnLayoutOid;

	if (!OidIsValid(oldOid))
	{
		/* Very first partition */

		oldOid = rel_partition_get_root(newOid);
	}

	Assert(OidIsValid(oldOid));

	if (oldOid == newOid)
	{
		/*
		 * If we have only one partition and we are rescanning
		 * then we can have this scenario.
		 */

		return false;
	}

	AttrNumber	*attMap = IndexScan_GetColumnMapping(oldOid, newOid);

	scanState->columnLayoutOid = newOid;

	if (attMap)
	{
		IndexScan_MapLogicalIndexInfo(indexScan->logicalIndexInfo, attMap, indexScan->scan.scanrelid);
		change_varattnos_of_a_varno((Node*)indexScan->scan.plan.targetlist, attMap, indexScan->scan.scanrelid);
		change_varattnos_of_a_varno((Node*)indexScan->indexqual, attMap, indexScan->scan.scanrelid);

		pfree(attMap);

		return true;
	}
	else
	{
		return false;
	}
}

/*
 * This function initializes a part and returns true if a new index has been prepared for scanning.
 */
static bool
initNextIndexToScan(DynamicIndexScanState *node)
{
	IndexScanState *indexState = &(node->indexScanState);
	DynamicIndexScan *dynamicIndexScan = (DynamicIndexScan *)node->indexScanState.ss.ps.plan;
	EState *estate = indexState->ss.ps.state;

	/* Load new index when the scanning of the previous index is done. */
	if (indexState->ss.scan_state == SCAN_INIT ||
		indexState->ss.scan_state == SCAN_DONE)
	{
		/* This is the oid of a partition of the table (*not* index) */
		Oid *pid = hash_seq_search(&node->pidxStatus);
		if (pid == NULL)
		{
			/* Return if all parts have been scanned. */
			node->shouldCallHashSeqTerm = false;
			return false;
		}

		/* Collect number of partitions scanned in EXPLAIN ANALYZE */
		if(NULL != indexState->ss.ps.instrument)
		{
			Instrumentation *instr = indexState->ss.ps.instrument;
			instr->numPartScanned ++;
		}

		DynamicIndexScan_ReMapColumns(node, *pid);

		/*
		 * The is the oid of the partition of an *index*. Note: a partitioned table
		 * has a root and a set of partitions (may be multi-level). An index
		 * on a partitioned table also has a root and a set of index partitions.
		 * We started at table level, and now we are fetching the oid of an index
		 * partition.
		 */
		Oid pindex = getPhysicalIndexRelid(dynamicIndexScan->logicalIndexInfo,
					 *pid);

		Assert(OidIsValid(pindex));

		Relation currentRelation = OpenScanRelationByOid(*pid);
		indexState->ss.ss_currentRelation = currentRelation;

		indexState->ss.ss_ScanTupleSlot->tts_tableOid = *pid;

		ExecAssignScanType(&indexState->ss, RelationGetDescr(currentRelation));

		/*
		 * Initialize result tuple type and projection info.
		 */
		ExecAssignResultTypeFromTL(&indexState->ss.ps);
		ExecAssignScanProjectionInfo(&indexState->ss);

		MemoryContextReset(node->partitionMemoryContext);
		MemoryContext oldCxt = MemoryContextSwitchTo(node->partitionMemoryContext);

		/* Initialize child expressions */
		indexState->ss.ps.qual = (List *) ExecInitExpr((Expr *) indexState->ss.ps.plan->qual, (PlanState *) indexState);
		indexState->ss.ps.targetlist = (List *) ExecInitExpr((Expr *) indexState->ss.ps.plan->targetlist, (PlanState *) indexState);

		indexState->iss_RelationDesc =
			OpenIndexRelation(estate, pindex, *pid);

		/*
		 * build the index scan keys from the index qualification
		 */
		ExecIndexBuildScanKeys((PlanState *) indexState,
						   indexState->iss_RelationDesc,
						   dynamicIndexScan->indexqual,
						   dynamicIndexScan->indexstrategy,
						   dynamicIndexScan->indexsubtype,
						   &indexState->iss_ScanKeys,
						   &indexState->iss_NumScanKeys,
						   &indexState->iss_RuntimeKeys,
						   &indexState->iss_NumRuntimeKeys,
						   NULL,
						   NULL);

		MemoryContextSwitchTo(oldCxt);

		if (indexState->iss_NumRuntimeKeys != 0)
		{
			ExecIndexEvalRuntimeKeys(indexState->iss_RuntimeContext,
									 indexState->iss_RuntimeKeys,
									 indexState->iss_NumRuntimeKeys);
		}
		indexState->iss_RuntimeKeysReady = true;

		indexState->iss_ScanDesc = index_beginscan(currentRelation,
				indexState->iss_RelationDesc,
				estate->es_snapshot,
				indexState->iss_NumScanKeys,
				indexState->iss_ScanKeys);

		indexState->ss.scan_state = SCAN_SCAN;
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
	InsertPidIntoDynamicTableScanInfo(plan->scan.partIndex, InvalidOid, InvalidPartitionSelectorId);

	Assert(NULL != estate->dynamicTableScanInfo->pidIndexes);
	Assert(estate->dynamicTableScanInfo->numScans >= plan->scan.partIndex);
	node->pidxIndex = estate->dynamicTableScanInfo->pidIndexes[plan->scan.partIndex - 1];
	Assert(node->pidxIndex != NULL);

	if (optimizer_partition_selection_log)
	{
		LogSelectedPartitionOids(node->pidxIndex);
	}
}

/*
 * Execution of DynamicIndexScan
 */
TupleTableSlot *
ExecDynamicIndexScan(DynamicIndexScanState *node)
{
	Assert(node);

	IndexScanState *indexState = &(node->indexScanState);

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
		slot = ExecScan(&indexState->ss, (ExecScanAccessMtd) IndexNext);

		if (!TupIsNull(slot))
		{
			/* Report output rows to Gpmon */
			Gpmon_M_Incr_Rows_Out(GpmonPktFromDynamicIndexScanState(node));
			CheckSendPlanStateGpmonPkt(&indexState->ss.ps);
		}
		else
		{
			CleanupOnePartition(indexState);
		}

	}
	return slot;
}

/*
 * Release resources for one part (this includes closing the index and
 * the relation).
 */
static inline void
CleanupOnePartition(IndexScanState *indexState)
{
	Assert(NULL != indexState);

	/* Reset index state and release locks. */
	ExecClearTuple(indexState->ss.ps.ps_ResultTupleSlot);
	ExecClearTuple(indexState->ss.ss_ScanTupleSlot);

	if ((indexState->ss.scan_state & SCAN_SCAN) != 0)
	{
		Assert(indexState->iss_ScanDesc != NULL);
		Assert(indexState->iss_RelationDesc != NULL);
		Assert(indexState->ss.ss_currentRelation != NULL);

		index_endscan(indexState->iss_ScanDesc);
		indexState->iss_ScanDesc = NULL;

		index_close(indexState->iss_RelationDesc, NoLock);
		indexState->iss_RelationDesc = NULL;

		ExecCloseScanRelation(indexState->ss.ss_currentRelation);
		indexState->ss.ss_currentRelation = NULL;
	}

	indexState->ss.scan_state = SCAN_INIT;
}

/*
 * Ends current scan by closing relations, and ending hash
 * iteration
 */
static void
DynamicIndexScanEndCurrentScan(DynamicIndexScanState *node)
{
	IndexScanState *indexState = &(node->indexScanState);

	CleanupOnePartition(indexState);

	if (node->shouldCallHashSeqTerm)
	{
		hash_seq_term(&node->pidxStatus);
		node->shouldCallHashSeqTerm = false;
	}
}

/*
 * Release resources of DynamicIndexScan
 */
void 
ExecEndDynamicIndexScan(DynamicIndexScanState *node)
{
	DynamicIndexScanEndCurrentScan(node);

	IndexScanState *indexState = &(node->indexScanState);

	FreeRuntimeKeysContext((IndexScanState *) node);
	EndPlanStateGpmonPkt(&indexState->ss.ps);

	MemoryContextDelete(node->partitionMemoryContext);
}

/*
 * Allow rescanning an index.
 */
void 
ExecDynamicIndexReScan(DynamicIndexScanState *node, ExprContext *exprCtxt)
{
	DynamicIndexScanEndCurrentScan(node);

	/* Force reloading the hash table */
	node->pidxIndex = NULL;

	/* Context for runtime keys */
	ExprContext *econtext = node->indexScanState.iss_RuntimeContext;

	if (econtext)
	{
		/*
		 * If we are being passed an outer tuple, save it for runtime key
		 * calc.  We also need to link it into the "regular" per-tuple
		 * econtext, so it can be used during indexqualorig evaluations.
		 */
		if (exprCtxt != NULL)
		{
			econtext->ecxt_outertuple = exprCtxt->ecxt_outertuple;
			ExprContext *stdecontext = node->indexScanState.ss.ps.ps_ExprContext;
			stdecontext->ecxt_outertuple = exprCtxt->ecxt_outertuple;
		}

		/*
		 * Reset the runtime-key context so we don't leak memory as each outer
		 * tuple is scanned.  Note this assumes that we will recalculate *all*
		 * runtime keys on each call.
		 */
		ResetExprContext(econtext);
	}

	Gpmon_M_Incr(GpmonPktFromDynamicIndexScanState(node), GPMON_DYNAMICINDEXSCAN_RESCAN);
	CheckSendPlanStateGpmonPkt(&node->indexScanState.ss.ps);
}

/*
 * Method for reporting DynamicIndexScan progress to gpperfmon
 */
void
initGpmonPktForDynamicIndexScan(Plan *planNode, gpmon_packet_t *gpmon_pkt, EState *estate)
{
	Assert(planNode != NULL && gpmon_pkt != NULL && IsA(planNode, DynamicIndexScan));

	{
		RangeTblEntry *rte = rt_fetch(((Scan *)planNode)->scanrelid, estate->es_range_table);
		char schema_rel_name[SCAN_REL_NAME_BUF_SIZE] = {0};

		Assert(GPMON_DYNAMICINDEXSCAN_TOTAL <= (int)GPMON_QEXEC_M_COUNT);

		InitPlanNodeGpmonPkt(planNode, gpmon_pkt, estate, PMNT_DynamicIndexScan,
					(int64) planNode->plan_rows, GetScanRelNameGpmon(rte->relid, schema_rel_name));
	}
}
