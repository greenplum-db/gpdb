/*-------------------------------------------------------------------------
 *
 * execDynamicIndexScan.c
 *	  Support routines for iterating through dynamically chosen partitions
 *	  of an index relation
 *
 * Portions Copyright (c) 2014-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/execDynamicIndexScan.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "cdb/cdbpartition.h"
#include "cdb/partitionselection.h"
#include "executor/execDynamicIndexScan.h"
#include "executor/execDynamicScan.h"
#include "executor/nodeIndexscan.h"
#include "executor/instrument.h"
#include "executor/execIndexscan.h"
#include "parser/parsetree.h"
#include "utils/memutils.h"
#include "access/genam.h"

static void
IndexScan_CleanupOneIndexRelation(IndexScanState *indexScanState);

static Oid
IndexScan_GetIndexOid(IndexScanState *indexScanState, Oid tableOid);

static bool
IndexScan_OpenIndexRelation(IndexScanState *scanState, Oid tableOid);

static void
IndexScan_RemapLogicalIndexInfo(IndexScanState *indexScanState, AttrNumber *attMap);

static bool
IndexScan_RemapIndexScanVars(IndexScanState *indexScanState, bool initQual, bool initTargetList);

static void
IndexScan_InitIndexExpressions(IndexScanState *indexScanState, bool initQual, bool initTargetList);

static void
IndexScan_PrepareIndexScanKeys(IndexScanState *indexScanState, MemoryContext partitionContext, bool initQual, bool initTargetList, bool supportsArrayKeys);

static void
IndexScan_PrepareExpressionContext(IndexScanState *indexScanState);

static void
IndexScan_InitRuntimeKeys(IndexScanState *indexScanState);

/*
 * -------------------------------------
 * Static functions
 * -------------------------------------
 */

/*
 * IndexScan_RemapIndexScanVars
 * 		Remaps the columns in the logical index descriptor, target list,
 * 		and qual to handle column alignment issues because of dropped columns.
 *
 * 		Returns true if remapping was needed.
 */
static bool
IndexScan_RemapIndexScanVars(IndexScanState *indexScanState, bool initQual, bool initTargetList)
{
	IndexScan *indexScan = (IndexScan *) indexScanState->ss.ps.plan;

	Oid oldOid = indexScanState->tableOid;
	Oid newOid = DynamicScan_GetTableOid((ScanState *) indexScanState);

	if (!OidIsValid(oldOid))
	{
		/* Very first partition */
		oldOid = rel_partition_get_root(newOid);
	}

	Assert(OidIsValid(oldOid));

	indexScanState->tableOid = newOid;

	AttrNumber	*attMap = IndexScan_GetColumnMapping(oldOid, newOid);

	if (NULL != attMap)
	{
		IndexScan_RemapLogicalIndexInfo(indexScanState, attMap);

		DynamicScan_RemapExpression((ScanState *)indexScanState, attMap, (Node*)indexScan->indexqual);

		if (initQual)
		{
			DynamicScan_RemapExpression((ScanState *)indexScanState, attMap, (Node*)indexScan->scan.plan.qual);
		}

		if (initTargetList)
		{
			DynamicScan_RemapExpression((ScanState *)indexScanState, attMap, (Node*)indexScan->scan.plan.targetlist);
		}

		pfree(attMap);

		return true;
	}

	return false;
}

/*
 * IndexScan_GetIndexOid
 * 		Returns the Oid of a suitable index for the IndexScan.
 *
 * 		Note: this assumes that the indexScan->logicalIndexInfo varattno
 * 		are already mapped.
 *
 * 		This method will return the indexScan->indexid if called during
 * 		the SCAN_INIT state or for non-partitioned case.
 */
static Oid
IndexScan_GetIndexOid(IndexScanState *indexScanState, Oid tableOid)
{
	IndexScan *indexScan = (IndexScan *) indexScanState->ss.ps.plan;
	Relation partRel;

	/*
	 * We return the plan node's indexid for non-partitioned case.
	 * For partitioned case, we also return the plan node's indexid
	 * if we are in the initialization phase (i.e., we don't yet know
	 * which partitions to scan).
	 */
	if (!isDynamicScan(&indexScan->scan) || SCAN_INIT == indexScanState->ss.scan_state)
	{
		return indexScan->indexid;
	}

	Assert(NULL != indexScan->logicalIndexInfo);
	Assert(OidIsValid(tableOid));

	/*
	 * The is the oid of the partition of an *index*. Note: a partitioned table
	 * has a root and a set of partitions (may be multi-level). An index
	 * on a partitioned table also has a root and a set of index partitions.
	 * We started at table level, and now we are fetching the oid of an index
	 * partition.
	 */
	Oid indexOid;

	partRel = OpenScanRelationByOid(tableOid);
	indexOid = getPhysicalIndexRelid(partRel, indexScan->logicalIndexInfo);
	CloseScanRelation(partRel);

	Assert(OidIsValid(indexOid));

	return indexOid;
}

/*
 * IndexScan_OpenIndexRelation
 * 		Opens the index relation of the scanState using proper locks.
 *
 * 		Returns false if the previously opened index relation can be
 * 		reused.
 *
 * 		Otherwise it closes the previously opened index relation and
 * 		opens the newly requested one, and returns true.
 */
static bool
IndexScan_OpenIndexRelation(IndexScanState *scanState, Oid tableOid)
{
	/* Look up the correct index oid for the provided tableOid */
	Oid indexOid = IndexScan_GetIndexOid(scanState, tableOid);
	Assert(OidIsValid(indexOid));

	/*
	 * If we already have an open index, and that index's relation
	 * oid is the same as the newly determined one, then we don't
	 * open any new relation and we just return false.
	 */
	if (NULL != scanState->iss_RelationDesc &&
			indexOid == RelationGetRelid(scanState->iss_RelationDesc))
	{
		/*
		 * Put the tableOid as we may not have a valid
		 * tableOid, if the original relation was opened
		 * during initialization based on indexScan->indexid.
		 */
		scanState->tableOid = tableOid;
		return false;
	}

	/* We cannot reuse, therefore, clean up previously opened index */
	IndexScan_CleanupOneIndexRelation(scanState);

	Assert(NULL == scanState->iss_RelationDesc);
	Assert(!OidIsValid(scanState->tableOid));

	LOCKMODE lockMode = AccessShareLock;

	if (!isDynamicScan((Scan *)&scanState->ss.ps.plan) &&
			ExecRelationIsTargetRelation(scanState->ss.ps.state, ((Scan *)scanState->ss.ps.plan)->scanrelid))
	{
		lockMode = NoLock;
	}

	Assert(NULL == scanState->iss_RelationDesc);
	scanState->iss_RelationDesc = index_open(indexOid, lockMode);
	scanState->tableOid = tableOid;

	return true;
}

/*
 * IndexScan_InitIndexExpressions
 * 		Initializes the plan state's qual and target list from the
 * 		corresponding plan's qual and targetlist for an index.
 */
static void
IndexScan_InitIndexExpressions(IndexScanState *indexScanState, bool initQual, bool initTargetList)
{
	ScanState *scanState = (ScanState *)indexScanState;

	Plan *plan = scanState->ps.plan;

	if (initQual)
	{
		scanState->ps.qual = (List *)ExecInitExpr((Expr *)plan->qual, (PlanState*)scanState);
	}

	if (initTargetList)
	{
		scanState->ps.targetlist = (List *)ExecInitExpr((Expr *)plan->targetlist, (PlanState*)scanState);
	}
}

/*
 * IndexScan_PrepareIndexScanKeys
 * 		Prepares the various scan keys for an index, such
 * 		as scan keys, runtime keys and array keys.
 */
static void
IndexScan_PrepareIndexScanKeys(IndexScanState *indexScanState, MemoryContext partitionContext, bool initQual, bool initTargetList, bool supportsArrayKeys)
{
	IndexScan *plan = (IndexScan *)indexScanState->ss.ps.plan;

	MemoryContext oldCxt = NULL;

	Assert(NULL != partitionContext);
	oldCxt = MemoryContextSwitchTo(partitionContext);
	Assert(NULL != oldCxt);

	IndexArrayKeyInfo **arrayKeys = supportsArrayKeys ? &indexScanState->iss_ArrayKeys : NULL;
	int *numArrayKeys = supportsArrayKeys ? &indexScanState->iss_NumArrayKeys : NULL;

	/*
	 * initialize child expressions
	 *
	 * We don't need to initialize targetlist or qual since neither are used.
	 *
	 * Note: we don't initialize all of the indexqual expression, only the
	 * sub-parts corresponding to runtime keys
	 */
	ExecIndexBuildScanKeys((PlanState *) indexScanState,
						   indexScanState->iss_RelationDesc,
						   plan->indexqual,
						   &indexScanState->iss_ScanKeys,
						   &indexScanState->iss_NumScanKeys,
						   &indexScanState->iss_RuntimeKeys,
						   &indexScanState->iss_NumRuntimeKeys,
						   arrayKeys,
						   numArrayKeys);

	MemoryContextSwitchTo(oldCxt);
}

/*
 * IndexScan_PrepareExpressionContext
 * 		Prepares a new expression context for the evaluation of the
 * 		runtime keys of an index.
 */
static void
IndexScan_PrepareExpressionContext(IndexScanState *indexScanState)
{
	Assert(NULL == indexScanState->iss_RuntimeContext);

	ExprContext *stdecontext = indexScanState->ss.ps.ps_ExprContext;

	ExecAssignExprContext(indexScanState->ss.ps.state, &indexScanState->ss.ps);
	indexScanState->iss_RuntimeContext = indexScanState->ss.ps.ps_ExprContext;
	indexScanState->ss.ps.ps_ExprContext = stdecontext;
}

/*
 * IndexScan_InitRuntimeKeys
 * 		Initializes the runtime keys and array keys for an index scan
 * 		by evaluating them.
 */
static void
IndexScan_InitRuntimeKeys(IndexScanState *indexScanState)
{
	ExprContext *econtext = indexScanState->iss_RuntimeContext;		/* context for runtime keys */

	indexScanState->iss_RuntimeKeysReady = true;

	if (0 != indexScanState->iss_NumRuntimeKeys)
	{
		Assert(NULL != econtext);
		Assert(NULL != indexScanState->iss_RuntimeKeys);

		ExecIndexEvalRuntimeKeys(econtext,
				indexScanState->iss_RuntimeKeys,
				indexScanState->iss_NumRuntimeKeys);
	}

	if (0 != indexScanState->iss_NumArrayKeys)
	{
		Assert(NULL != econtext);
		Assert(NULL != indexScanState->iss_ArrayKeys);

		indexScanState->iss_RuntimeKeysReady =
			ExecIndexEvalArrayKeys(econtext,
								   indexScanState->iss_ArrayKeys,
								   indexScanState->iss_NumArrayKeys);
	}
}

/*
 * IndexScan_RemapLogicalIndexInfo
 * 		Remaps the columns of the expressions of a the logicalIndexInfo
 * 		in a IndexScanState.
 */
static void
IndexScan_RemapLogicalIndexInfo(IndexScanState *indexScanState, AttrNumber *attMap)
{
	IndexScan *indexScan = (IndexScan*)indexScanState->ss.ps.plan;

	if (!isDynamicScan((Scan *)(&indexScan->scan.plan)))
	{
		return;
	}

	LogicalIndexInfo *logicalIndexInfo = indexScan->logicalIndexInfo;

	Assert(NULL != logicalIndexInfo);
	IndexScan_MapLogicalIndexInfo(logicalIndexInfo, attMap, indexScan->scan.scanrelid);
}

/*
 * IndexScan_CleanupOneIndexRelation
 * 		Cleans up one index relation by closing the scan
 * 		descriptor and the index relation.
 */
static void
IndexScan_CleanupOneIndexRelation(IndexScanState *indexScanState)
{
	Assert(NULL != indexScanState);

	Assert(SCAN_END != indexScanState->ss.scan_state);

	/*
	 * For SCAN_INIT and SCAN_FIRST we don't yet have any open
	 * scan descriptor.
	 */
	Assert(NULL != indexScanState->iss_ScanDesc ||
			SCAN_FIRST == indexScanState->ss.scan_state ||
			SCAN_INIT == indexScanState->ss.scan_state);

	if (NULL != indexScanState->iss_ScanDesc)
	{
		index_endscan(indexScanState->iss_ScanDesc);
		indexScanState->iss_ScanDesc = NULL;
	}

	if (NULL != indexScanState->iss_RelationDesc)
	{
		index_close(indexScanState->iss_RelationDesc, NoLock);
		indexScanState->iss_RelationDesc = NULL;
	}

	/*
	 * We use the tableOid to figure out if we can reuse an
	 * existing open relation. So, we set this field to InvalidOid
	 * when we clean up an open relation.
	 */
	indexScanState->tableOid = InvalidOid;
}

/*
 * IndexScan_ReMapLogicalIndexInfoDirect
 * 		Remaps the columns of the expressions of a provided logicalIndexInfo
 * 		using attMap for a given varno.
 *
 * 		Returns true if mapping was done.
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
 * IndexScan_BeginIndexScan
 * 		Prepares the index scan state for a new index scan by calling
 * 		various helper methods.
 */
void
IndexScan_BeginIndexScan(IndexScanState *indexScanState, MemoryContext partitionContext, bool initQual,
		bool initTargetList, bool supportsArrayKeys)
{
	Assert(NULL == indexScanState->iss_RelationDesc);
	Assert(NULL == indexScanState->iss_ScanDesc);

	Assert(SCAN_INIT == indexScanState->ss.scan_state);

	/*
	 * Create the expression context now, so that we can save any passed
	 * tuple in ReScan call for later runtime key evaluation. Note: a ReScan
	 * call may precede any BeginIndexPartition call.
	 */
	IndexScan_PrepareExpressionContext(indexScanState);

	IndexScan_OpenIndexRelation(indexScanState, InvalidOid /* SCAN_INIT doesn't need any valid table Oid */);
	Assert(NULL != indexScanState->iss_RelationDesc);

	IndexScan_PrepareIndexScanKeys(indexScanState, partitionContext, initQual, initTargetList, supportsArrayKeys);
	IndexScan_InitIndexExpressions(indexScanState, initQual, initTargetList);

	/*
	 * SCAN_FIRST implies that we haven't scanned any
	 * relation yet. Therefore, a call to scan the very
	 * same relation would not result in a SCAN_DONE.
	 */
	indexScanState->ss.scan_state = SCAN_FIRST;
}

/*
 * IndexScan_BeginIndexPartition
 * 		Prepares index scan state for scanning an index relation
 * 		(possibly a dynamically determined relation).
 *
 * Parameters:
 * 		indexScanState: The scan state for this index scan node
 * 		partitionContext: A per-partition memory context to use for
 * 			allocating per-partition data structures. Gets reset whenever we have a
 * 			partition whose column mapping is different than the previous partition
 *		initQual: Should we initialize qualifiers? BitmapIndexScan
 *			doesn't evaluate qualifiers, as it depends on BitmapTableScan
 *			to do so. However, Btree index scan (i.e., regular DynamicIndexScan)
 *			would initialize and evaluate qual.
 *		initTargetList: Similar to initQual, it indicates whether to evaluate
 *			target list expressions. Again, BitmapIndexScan doesn't have
 *			target list, while DynamicIndexScan does have.
 *		supportArrayKeys: Whether to calculate array keys from scan keys. Bitmap
 *		index has array keys, while Btree doesn't.
 *
 * 		Returns true if we have a valid relation to scan.
 */
bool
IndexScan_BeginIndexPartition(IndexScanState *indexScanState, MemoryContext partitionContext, bool initQual,
		bool initTargetList, bool supportsArrayKeys)
{
	/*
	 * Either the SCAN_INIT should open the relation during SCAN_INIT -> SCAN_FIRST
	 * transition or we already have an open relation from SCAN_SCAN state of the
	 * last scanned relation.
	 */
	Assert(NULL != indexScanState->iss_RelationDesc);
	Assert(NULL != indexScanState->iss_ScanDesc ||
			SCAN_FIRST == indexScanState->ss.scan_state);

	/*
	 * Once the previous scan is done, we don't allow any
	 * further scanning of this relation. Note, we don't
	 * close the relation yet, and we hope that a rescan
	 * call would be able to reuse this relation.
	 */
	if (SCAN_DONE == indexScanState->ss.scan_state)
	{
		return false;
	}

	/*
	 * We already have a scan in progress. So, we don't initalize
	 * any new relation.
	 */
	if (SCAN_SCAN == indexScanState->ss.scan_state)
	{
		return true;
	}

	/* These are the valid ancestor state of the SCAN_SCAN */
	Assert(SCAN_FIRST == indexScanState->ss.scan_state ||
			SCAN_NEXT == indexScanState->ss.scan_state ||
			SCAN_RESCAN == indexScanState->ss.scan_state);

	bool isNewRelation = false;
	bool isDynamic = isDynamicScan((Scan *)indexScanState->ss.ps.plan);

	if (isDynamic)
	{
		Oid newTableOid = DynamicScan_GetTableOid((ScanState *)indexScanState);
		Assert(OidIsValid(indexScanState->tableOid) ||
				SCAN_FIRST == indexScanState->ss.scan_state);

		if (!OidIsValid(newTableOid))
		{
			indexScanState->ss.scan_state = SCAN_DONE;
			return false;
		}

		if ((newTableOid != indexScanState->tableOid))
		{
			/* We have a new relation, so ensure proper column mapping */
			bool remappedVars = IndexScan_RemapIndexScanVars(indexScanState, initQual, initTargetList);
			/*
			 * During SCAN_INIT -> SCAN_FIRST we opened a relation based on
			 * optimizer provided indexid. However, the corresponding tableOid
			 * of the indexid was set to invalid. Therefore, it is possible to
			 * assume that we have a new index, while in reality, after resolving
			 * logicalIndexInfo, we may not have a new index relation.
			 */
			isNewRelation = IndexScan_OpenIndexRelation(indexScanState, newTableOid);

			AssertImply(remappedVars, isNewRelation);

			if (remappedVars)
			{
				/* Allocations for previous part are no longer necessary */
				MemoryContextReset(partitionContext);

				/*
				 * If remapping is necessary, we also need to prepare the scan keys,
				 * and initialize the expressions one more time.
				 */
				IndexScan_PrepareIndexScanKeys(indexScanState, partitionContext, initQual, initTargetList, supportsArrayKeys);
				IndexScan_InitIndexExpressions(indexScanState, initQual, initTargetList);
			}

			/*
			 * Either a rescan requires recomputation of runtime keys (if an expression context is
			 * provided during rescan) or we have a new relation that must recompute index's runtime
			 * keys. Therefore, we need to preserve an already outstanding request for recomputation
			 * [MPP-24628] even if we don't have a new relation.
			 */
			if (indexScanState->iss_RuntimeKeysReady)
			{
				/* A new relation would require us to recompute runtime keys */
				indexScanState->iss_RuntimeKeysReady = !isNewRelation;
			}
		}
	}
	else if (SCAN_FIRST != indexScanState->ss.scan_state &&
			SCAN_RESCAN != indexScanState->ss.scan_state)
	{
		/*
		 * For non-partitioned case, if we already scanned
		 * the relation, and we don't have a rescan request
		 * we flag the scan as "done".
		 */
		indexScanState->ss.scan_state = SCAN_DONE;
		return false;
	}

	Assert(NULL != indexScanState->iss_RelationDesc);

	if (!indexScanState->iss_RuntimeKeysReady)
	{
		IndexScan_InitRuntimeKeys(indexScanState);
	}

	Assert(indexScanState->iss_RuntimeKeysReady);

	/*
	 * If this is the first relation to scan or if we
	 * had to open a new relation, then we also need
	 * a new scan descriptor.
	 */
	if (SCAN_FIRST == indexScanState->ss.scan_state ||
			isNewRelation)
	{
		Assert(NULL == indexScanState->iss_ScanDesc);
		/*
		 * Initialize scan descriptor.
		 */
		indexScanState->iss_ScanDesc =
				index_beginscan(
					indexScanState->ss.ss_currentRelation,
					indexScanState->iss_RelationDesc,
					indexScanState->ss.ps.state->es_snapshot,
					indexScanState->iss_NumScanKeys,
					indexScanState->iss_ScanKeys);
	}
	else
	{
		Assert(NULL != indexScanState->iss_ScanDesc);
		/* We already have a reusable scan descriptor, so just initiate a rescan */
		index_rescan(indexScanState->iss_ScanDesc, indexScanState->iss_ScanKeys);
	}

	Assert(NULL != indexScanState->iss_ScanDesc);
	indexScanState->ss.scan_state = SCAN_SCAN;

	return true;
}

/*
 * IndexScan_EndIndexPartition
 * 		Ends scanning of *one* partition.
 */
void
IndexScan_EndIndexPartition(IndexScanState *indexScanState)
{
	Assert(NULL != indexScanState);
	Assert(SCAN_SCAN == indexScanState->ss.scan_state);

	/* For non-partitioned case, we flag the scan as "done" */
	if (!isDynamicScan((Scan *)indexScanState->ss.ps.plan))
	{
		indexScanState->ss.scan_state = SCAN_DONE;
	}
	else
	{
		/*
		 * For dynamic scan, we decide if the scanning is done
		 * during fetching of the next partition.
		 */
		indexScanState->ss.scan_state = SCAN_NEXT;
	}
}

/*
 * IndexScan_RescanIndex
 * 		Prepares for a rescan of an index by saving the
 * 		outer tuple (if any) and starting a new index scan.
 *
 * Parameters:
 * 		indexScanState: The scan state for this index scan node
 *		exprCtxt: The expression context that might contain the
 *			outer tuple during rescanning (e.g., NLJ)
 *		initQual: Should we initialize qualifiers? BitmapIndexScan
 *			doesn't evaluate qualifiers, as it depends on BitmapTableScan
 *			to do so. However, Btree index scan (i.e., regular DynamicIndexScan)
 *			would initialize and evaluate qual.
 *		initTargetList: Similar to initQual, it indicates whether to evaluate
 *			target list expressions. Again, BitmapIndexScan doesn't have
 *			target list, while DynamicIndexScan does have.
 *		supportArrayKeys: Whether to calculate array keys from scan keys. Bitmap
 *		index has array keys, while Btree doesn't.
 */
void
IndexScan_RescanIndex(IndexScanState *indexScanState, ExprContext *exprCtxt, bool initQual, bool initTargetList, bool supportsArrayKeys)
{
	/*
	 * Rescan should only be called after we passed ExecInit
	 * or after we are done scanning one relation or after
	 * we are done with all relations.
	 */
	Assert(SCAN_FIRST == indexScanState->ss.scan_state ||
			SCAN_NEXT == indexScanState->ss.scan_state ||
			SCAN_DONE == indexScanState->ss.scan_state ||
			SCAN_RESCAN == indexScanState->ss.scan_state);

	Assert(NULL != indexScanState->iss_RuntimeContext);

	/* Context for runtime keys */
	ExprContext *econtext = indexScanState->iss_RuntimeContext;

	Assert(NULL != econtext);

	/*
	 * If we are being passed an outer tuple, save it for runtime key
	 * calc.
	 */
	if (NULL != exprCtxt)
	{
		econtext->ecxt_outertuple = exprCtxt->ecxt_outertuple;
	}

	/* There might be a change of parameter values. Therefore, recompute the runtime keys */
	indexScanState->iss_RuntimeKeysReady = false;

	/*
	 * Reset the runtime-key context so we don't leak memory as each outer
	 * tuple is scanned.  Note this assumes that we will recalculate *all*
	 * runtime keys on each call.
	 */
	ResetExprContext(econtext);

	/*
	 * If we have already passed SCAN_FIRST (i.e., have a valid scan descriptor)
	 * then don't trigger a "forced" re-initialization.
	 */
	if (SCAN_FIRST != indexScanState->ss.scan_state)
	{
		indexScanState->ss.scan_state = SCAN_RESCAN;
	}
}

/*
 * IndexScan_EndIndexScan
 * 		Prepares for a rescan of an index by saving the
 * 		outer tuple (if any) and starting a new index scan
 * 		to evaluate the runtime keys.
 */
void
IndexScan_EndIndexScan(IndexScanState *indexScanState)
{
	if (SCAN_END == indexScanState->ss.scan_state)
	{
		Assert(NULL == indexScanState->iss_RelationDesc);
		Assert(NULL == indexScanState->iss_ScanDesc);
		Assert(NULL == indexScanState->iss_RuntimeContext);
		return;
	}

	IndexScan_CleanupOneIndexRelation(indexScanState);
	Assert(NULL == indexScanState->iss_RelationDesc);
	Assert(NULL == indexScanState->iss_ScanDesc);

	FreeRuntimeKeysContext(indexScanState);
	Assert(NULL == indexScanState->iss_RuntimeContext);

	indexScanState->ss.scan_state = SCAN_END;
}

/*
 * IndexScan_GetColumnMapping
 * 		Returns the mapping of columns between two relation Oids because of
 * 		dropped attributes.
 *
 * 		Returns NULL for identical mapping.
 */
AttrNumber*
IndexScan_GetColumnMapping(Oid oldOid, Oid newOid)
{
	if (oldOid == newOid)
	{
		return NULL;
	}

	AttrNumber	*attMap = NULL;

	Relation oldRel = heap_open(oldOid, AccessShareLock);
	Relation newRel = heap_open(newOid, AccessShareLock);

	TupleDesc oldTupDesc = oldRel->rd_att;
	TupleDesc newTupDesc = newRel->rd_att;

	attMap = varattnos_map(oldTupDesc, newTupDesc);

	heap_close(oldRel, AccessShareLock);
	heap_close(newRel, AccessShareLock);

	return attMap;
}
