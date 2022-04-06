/*-------------------------------------------------------------------------
 *
 * nodeDynamicSeqscan.c
 *	  Support routines for scanning one or more relations that are
 *	  determined at run time. The relations could be Heap, AppendOnly Row,
 *	  AppendOnly Columnar.
 *
 * DynamicSeqScan node scans each relation one after the other. For each
 * relation, it opens the table, scans the tuple, and returns relevant tuples.
 *
 * GPDB_12_MERGE_FIXME: This is currently disabled altogether. If it is
 * resurrected, some changes are needed to GPORCA. The way Partition
 * Selectors work has been heavily rewritten, there's no global hash table
 * of selected partitions anymore. For "static selection", there's a
 * partOids field in the DynamicSeqScan plan node that holds the selected
 * partitions; but none of the code in this file has been fixed to
 * actually work that way. Same with all the other Dynamic*Scan nodes.
 *
 * Portions Copyright (c) 2012 - present, EMC/Greenplum
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeDynamicSeqscan.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "executor/executor.h"
#include "executor/instrument.h"
#include "nodes/execnodes.h"
#include "nodes/nodeFuncs.h"
#include "executor/execDynamicScan.h"
#include "executor/nodeDynamicSeqscan.h"
#include "executor/nodeSeqscan.h"
#include "utils/hsearch.h"
#include "parser/parsetree.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "cdb/cdbvars.h"
#include "access/table.h"
#include "access/tableam.h"

static void CleanupOnePartition(DynamicSeqScanState *node);

/*
 * During attribute re-mapping for heterogeneous partitions, we use
 * this struct to identify which varno's attributes will be re-mapped.
 * Using this struct as a *context* during expression tree walking, we
 * can skip varattnos that do not belong to a given varno.
 */
typedef struct AttrMapContext
{
	const AttrNumber *newattno; /* The mapping table to remap the varattno */
	Index		varno;			/* Which rte's varattno to re-map */
} AttrMapContext;

/*
 * Remaps the varattno of a varattno in a Var node using an attribute map.
 */
static bool
change_varattnos_varno_walker(Node *node, const AttrMapContext *attrMapCxt)
{
	if (node == NULL)
		return false;
	if (IsA(node, Var))
	{
		Var		   *var = (Var *) node;

		if (var->varlevelsup == 0 && (var->varno == attrMapCxt->varno) &&
			var->varattno > 0)
		{
			/*
			 * ??? the following may be a problem when the node is multiply
			 * referenced though stringToNode() doesn't create such a node
			 * currently.
			 */
			Assert(attrMapCxt->newattno[var->varattno - 1] > 0);
			var->varattno = var->varoattno = attrMapCxt->newattno[var->varattno - 1];
		}
		return false;
	}
	return expression_tree_walker(node, change_varattnos_varno_walker,
	                              (void *) attrMapCxt);
}

/*
 * Replace varattno values for a given varno RTE index in an expression
 * tree according to the given map array, that is, varattno N is replaced
 * by newattno[N-1].  It is caller's responsibility to ensure that the array
 * is long enough to define values for all user varattnos present in the tree.
 * System column attnos remain unchanged.
 *
 * Note that the passed node tree is modified in-place!
 */
static void
change_varattnos_of_a_varno(Node *node, const AttrNumber *newattno, Index varno)
{
	AttrMapContext attrMapCxt;

	attrMapCxt.newattno = newattno;
	attrMapCxt.varno = varno;

	(void) change_varattnos_varno_walker(node, &attrMapCxt);
}

DynamicSeqScanState *
ExecInitDynamicSeqScan(DynamicSeqScan *node, EState *estate, int eflags)
{
	DynamicSeqScanState *state;
	Oid			reloid;
	ListCell *lc;
	int i;

	Assert((eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)) == 0);

	state = makeNode(DynamicSeqScanState);
	state->eflags = eflags;
	state->ss.ps.plan = (Plan *) node;
	state->ss.ps.state = estate;
	state->ss.ps.ExecProcNode = ExecDynamicSeqScan;

	state->scan_state = SCAN_INIT;

	/* Initialize child expressions. This is needed to find subplans. */
	state->ss.ps.qual =
		ExecInitQual(node->seqscan.plan.qual, (PlanState *) state);

	Relation scanRel = ExecOpenScanRelation(estate, node->seqscan.scanrelid, eflags);
	ExecInitScanTupleSlot(estate, &state->ss, RelationGetDescr(scanRel), table_slot_callbacks(scanRel));

	/* Initialize result tuple type. */
	ExecInitResultTypeTL(&state->ss.ps);
	ExecAssignScanProjectionInfo(&state->ss);

	state->nOids = list_length(node->partOids);
	state->partOids = palloc(sizeof(Oid) * state->nOids);
	foreach_with_count(lc, node->partOids, i)
		state->partOids[i] = lfirst_oid(lc);
	state->whichPart = -1;

	reloid = exec_rt_fetch(node->seqscan.scanrelid, estate)->relid;
	Assert(OidIsValid(reloid));

	state->firstPartition = true;

	/* lastRelOid is used to remap varattno for heterogeneous partitions */
	state->lastRelOid = reloid;

	state->scanrelid = node->seqscan.scanrelid;

	/*
	 * This context will be reset per-partition to free up per-partition
	 * qual and targetlist allocations
	 */
	state->partitionMemoryContext = AllocSetContextCreate(CurrentMemoryContext,
									 "DynamicSeqScanPerPartition",
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
 * If no more table is found, this function returns false.
 */
static bool
initNextTableToScan(DynamicSeqScanState *node)
{
	ScanState  *scanState = (ScanState *) node;
	DynamicSeqScan *plan = (DynamicSeqScan *) scanState->ps.plan;
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

	currentRelation = scanState->ss_currentRelation =
		table_open(node->partOids[node->whichPart], AccessShareLock);

	if (currentRelation->rd_rel->relkind != RELKIND_RELATION)
	{
		/* shouldn't happen */
		elog(ERROR, "unexpected relkind in Dynamic Scan: %c", currentRelation->rd_rel->relkind);
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
		/*
		 * FIXME: Ewww, this doesn't really belong in the executor. The optimizer
		 * really should explicitly pass a qual and a tlist to us, for each
		 * partition
		 */
		change_varattnos_of_a_varno((Node*)scanState->ps.plan->qual, attMap, node->scanrelid);
		change_varattnos_of_a_varno((Node*)scanState->ps.plan->targetlist, attMap, node->scanrelid);

		/*
		 * Now that the varattno mapping has been changed, change the relation that
		 * the new varnos correspond to
		 */
		node->lastRelOid = *pid;
	}

	/*
	 * For the very first partition, the targetlist of planstate is set to null. So, we must
	 * initialize quals and targetlist, regardless of remapping requirements. For later
	 * partitions, we only initialize quals and targetlist if a column re-mapping is necessary.
	 */
	if (attMap || node->firstPartition)
	{
		node->firstPartition = false;
		MemoryContextReset(node->partitionMemoryContext);
		MemoryContext oldCxt = MemoryContextSwitchTo(node->partitionMemoryContext);

		/* Initialize child expressions */
		scanState->ps.qual =
			ExecInitQual(scanState->ps.plan->qual, (PlanState *) scanState);

		MemoryContextSwitchTo(oldCxt);
	}

	if (attMap)
		pfree(attMap);

//	DynamicScan_SetTableOid(&node->ss, *pid);
	node->seqScanState = ExecInitSeqScanForPartition(&plan->seqscan, estate,
													 currentRelation);
	return true;
}


TupleTableSlot *
ExecDynamicSeqScan(PlanState *pstate)
{
	DynamicSeqScanState *node = castNode(DynamicSeqScanState, pstate);
	TupleTableSlot *slot = NULL;

	/*
	 * Scan the table to find next tuple to return. If the current table
	 * is finished, close it and open the next table for scan.
	 */
	for (;;)
	{
		if (!node->seqScanState)
		{
			/* No partition open. Open the next one, if any. */
			if (!initNextTableToScan(node))
				break;
		}

		slot = ExecProcNode(&node->seqScanState->ss.ps);

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
CleanupOnePartition(DynamicSeqScanState *scanState)
{
	Assert(NULL != scanState);

	if (scanState->seqScanState)
	{
		ExecEndSeqScan(scanState->seqScanState);
		scanState->seqScanState = NULL;
		Assert(scanState->ss.ss_currentRelation != NULL);
		table_close(scanState->ss.ss_currentRelation, NoLock);
		scanState->ss.ss_currentRelation = NULL;
	}
}

/*
 * DynamicSeqScanEndCurrentScan
 *		Cleans up any ongoing scan.
 */
static void
DynamicSeqScanEndCurrentScan(DynamicSeqScanState *node)
{
	CleanupOnePartition(node);
}

/*
 * ExecEndDynamicSeqScan
 *		Ends the scanning of this DynamicSeqScanNode and frees
 *		up all the resources.
 */
void
ExecEndDynamicSeqScan(DynamicSeqScanState *node)
{
	DynamicSeqScanEndCurrentScan(node);

	if (node->ss.ps.ps_ResultTupleSlot)
		ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
}

/*
 * ExecReScanDynamicSeqScan
 *		Prepares the internal states for a rescan.
 */
void
ExecReScanDynamicSeqScan(DynamicSeqScanState *node)
{
	DynamicSeqScanEndCurrentScan(node);

	/* Force reloading the partition hash table */
}
