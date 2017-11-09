/*-------------------------------------------------------------------------
 *
 * nodeExternalscan.c
 *	  Support routines for scans of external relations (on flat files for example)
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeExternalscan.c
 *
 *-------------------------------------------------------------------------
 */

/*
 * INTERFACE ROUTINES
 *		ExecExternalScan				sequentially scans a relation.
 *		ExecExternalNext				retrieve next tuple in sequential order.
 *		ExecInitExternalScan			creates and initializes a externalscan node.
 *		ExecEndExternalScan				releases any storage allocated.
 *		ExecStopExternalScan			closes external resources before EOD.
 *		ExecExternalReScan				rescans the relation
 */
#include "postgres.h"

#include "access/fileam.h"
#include "access/heapam.h"
#include "cdb/cdbvars.h"
#include "executor/execdebug.h"
#include "executor/nodeExternalscan.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "parser/parsetree.h"
#include "optimizer/var.h"
#include "optimizer/clauses.h"

static TupleTableSlot *ExternalNext(ExternalScanState *node);

static bool
ExternalConstraintCheck(TupleTableSlot *slot, ExternalScanState *node)
{
	FileScanDesc	scandesc = node->ess_ScanDesc;
	Relation		rel = scandesc->fs_rd;
	TupleConstr		*constr = rel->rd_att->constr;
	ConstrCheck		*check = constr->check;
	uint16			ncheck = constr->num_check;
	EState			*estate = node->ss.ps.state;
	ExprContext		*econtext = NULL;
	MemoryContext	oldContext = NULL;
	List	*qual = NULL;
	int		i = 0;

	/* No constraints */
	if (ncheck == 0)
	{
		return true;
	}

	/*
	 * Build expression nodetrees for rel's constraint expressions.
	 * Keep them in the per-query memory context so they'll survive throughout the query.
	 */
	if (scandesc->fs_constraintExprs == NULL)
	{
		oldContext = MemoryContextSwitchTo(estate->es_query_cxt);
		scandesc->fs_constraintExprs =
			(List **) palloc(ncheck * sizeof(List *));
		for (i = 0; i < ncheck; i++)
		{
			/* ExecQual wants implicit-AND form */
			qual = make_ands_implicit(stringToNode(check[i].ccbin));
			scandesc->fs_constraintExprs[i] = (List *)
				ExecPrepareExpr((Expr *) qual, estate);
		}
		MemoryContextSwitchTo(oldContext);
	}

	/*
	 * We will use the EState's per-tuple context for evaluating constraint
	 * expressions (creating it if it's not already there).
	 */
	econtext = GetPerTupleExprContext(estate);

	/* Arrange for econtext's scan tuple to be the tuple under test */
	econtext->ecxt_scantuple = slot;

	/* And evaluate the constraints */
	for (i = 0; i < ncheck; i++)
	{
		qual = scandesc->fs_constraintExprs[i];

		if (!ExecQual(qual, econtext, true))
			return false;
	}
	
	return true;
}
/* ----------------------------------------------------------------
*						Scan Support
* ----------------------------------------------------------------
*/
/* ----------------------------------------------------------------
*		ExternalNext
*
*		This is a workhorse for ExecExtScan
* ----------------------------------------------------------------
*/
static TupleTableSlot *
ExternalNext(ExternalScanState *node)
{
	HeapTuple	tuple;
	FileScanDesc scandesc;
	EState	   *estate;
	ScanDirection direction;
	TupleTableSlot *slot;
	bool		scanNext = true;

	/*
	 * get information from the estate and scan state
	 */
	estate = node->ss.ps.state;
	scandesc = node->ess_ScanDesc;
	direction = estate->es_direction;
	slot = node->ss.ss_ScanTupleSlot;

	/*
	 * get the next tuple from the file access methods
	 */
	while(scanNext)
	{
		tuple = external_getnext(scandesc, direction);

		/*
		 * save the tuple and the buffer returned to us by the access methods in
		 * our scan tuple slot and return the slot.  Note: we pass 'false' because
		 * tuples returned by heap_getnext() are pointers onto disk pages and were
		 * not created with palloc() and so should not be pfree()'d.  Note also
		 * that ExecStoreTuple will increment the refcount of the buffer; the
		 * refcount will not be dropped until the tuple table slot is cleared.
		 */
		if (tuple)
		{
			ExecStoreHeapTuple(tuple, slot, InvalidBuffer, true);
			if (node->ess_ScanDesc->fs_hasConstraints && !ExternalConstraintCheck(slot, node))
			{
				ExecClearTuple(slot);
				continue;
			}
		    /*
		     * CDB: Label each row with a synthetic ctid if needed for subquery dedup.
		     */
		    if (node->cdb_want_ctid &&
		        !TupIsNull(slot))
		    {
		    	slot_set_ctid_from_fake(slot, &node->cdb_fake_ctid);
		    }
		}
		else
		{
			ExecClearTuple(slot);

			if (!node->ss.ps.delayEagerFree)
			{
				ExecEagerFreeExternalScan(node);
			}
		}
		scanNext = false;
	}
	

	return slot;
}

/* ----------------------------------------------------------------
*		ExecExternalScan(node)
*
*		Scans the external relation sequentially and returns the next qualifying
*		tuple.
*		It calls the ExecScan() routine and passes it the access method
*		which retrieve tuples sequentially.
*
*/

TupleTableSlot *
ExecExternalScan(ExternalScanState *node)
{
	/*
	 * use SeqNext as access method
	 */
	return ExecScan(&node->ss, (ExecScanAccessMtd) ExternalNext);
}


/* ----------------------------------------------------------------
*		ExecInitExternalScan
* ----------------------------------------------------------------
*/
ExternalScanState *
ExecInitExternalScan(ExternalScan *node, EState *estate, int eflags)
{
	ExternalScanState *externalstate;
	Relation	currentRelation;
	FileScanDesc currentScanDesc;

	Assert(outerPlan(node) == NULL);
	Assert(innerPlan(node) == NULL);

	/*
	 * create state structure
	 */
	externalstate = makeNode(ExternalScanState);
	externalstate->ss.ps.plan = (Plan *) node;
	externalstate->ss.ps.state = estate;

	/*
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &externalstate->ss.ps);

	/*
	 * initialize child expressions
	 */
	externalstate->ss.ps.targetlist = (List *)
		ExecInitExpr((Expr *) node->scan.plan.targetlist,
					 (PlanState *) externalstate);
	externalstate->ss.ps.qual = (List *)
		ExecInitExpr((Expr *) node->scan.plan.qual,
					 (PlanState *) externalstate);

	/* Check if targetlist or qual contains a var node referencing the ctid column */
	externalstate->cdb_want_ctid = contain_ctid_var_reference(&node->scan);
	ItemPointerSetInvalid(&externalstate->cdb_fake_ctid);

#define EXTSCAN_NSLOTS 2

	/*
	 * tuple table initialization
	 */
	ExecInitResultTupleSlot(estate, &externalstate->ss.ps);
	ExecInitScanTupleSlot(estate, &externalstate->ss);

	/*
	 * get the relation object id from the relid'th entry in the range table
	 * and open that relation.
	 */
	currentRelation = ExecOpenScanExternalRelation(estate, node->scan.scanrelid);


	currentScanDesc = external_beginscan(currentRelation,
									 node->scan.scanrelid,
									 node->scancounter,
									 node->uriList,
									 node->fmtOpts,
									 node->fmtType,
									 node->isMasterOnly,
									 node->rejLimit,
									 node->rejLimitInRows,
									 node->fmterrtbl,
									 node->encoding);

	externalstate->ss.ss_currentRelation = currentRelation;
	externalstate->ess_ScanDesc = currentScanDesc;

	ExecAssignScanType(&externalstate->ss, RelationGetDescr(currentRelation));

	/*
	 * Initialize result tuple type and projection info.
	 */
	ExecAssignResultTypeFromTL(&externalstate->ss.ps);
	ExecAssignScanProjectionInfo(&externalstate->ss);

	/*
	 * If eflag contains EXEC_FLAG_REWIND or EXEC_FLAG_BACKWARD or EXEC_FLAG_MARK,
	 * then this node is not eager free safe.
	 */
	externalstate->ss.ps.delayEagerFree =
		((eflags & (EXEC_FLAG_REWIND | EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)) != 0);

	return externalstate;
}


int
ExecCountSlotsExternalScan(ExternalScan *node)
{
	return ExecCountSlotsNode(outerPlan(node)) +
	ExecCountSlotsNode(innerPlan(node)) +
	EXTSCAN_NSLOTS;
}

/* ----------------------------------------------------------------
*		ExecEndExternalScan
*
*		frees any storage allocated through C routines.
* ----------------------------------------------------------------
*/
void
ExecEndExternalScan(ExternalScanState *node)
{
	/*
	 * Free the exprcontext
	 */
	ExecFreeExprContext(&node->ss.ps);

	/*
	 * clean out the tuple table
	 */
	ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
	ExecClearTuple(node->ss.ss_ScanTupleSlot);

	ExecEagerFreeExternalScan(node);
	pfree(node->ess_ScanDesc);

	/*
	 * close the external relation.
	 *
	 * MPP-8040: make sure we don't close it if it hasn't completed setup, or
	 * if we've already closed it.
	 */
	if (node->ss.ss_currentRelation)
	{
		Relation	relation = node->ss.ss_currentRelation;

		node->ss.ss_currentRelation = NULL;
		ExecCloseScanRelation(relation);
	}
	EndPlanStateGpmonPkt(&node->ss.ps);
}

/* ----------------------------------------------------------------
*		ExecStopExternalScan
*
*		Performs identically to ExecEndExternalScan except that
*		closure errors are ignored.  This function is called for
*		normal termination when the external data source is NOT
*		exhausted (such as for a LIMIT clause).
* ----------------------------------------------------------------
*/
void
ExecStopExternalScan(ExternalScanState *node)
{
	FileScanDesc fileScanDesc;

	/*
	 * get information from node
	 */
	fileScanDesc = node->ess_ScanDesc;

	/*
	 * stop the file scan
	 */
	external_stopscan(fileScanDesc);
}


/* ----------------------------------------------------------------
*						Join Support
* ----------------------------------------------------------------
*/

/* ----------------------------------------------------------------
*		ExecExternalReScan
*
*		Rescans the relation.
* ----------------------------------------------------------------
*/
void
ExecExternalReScan(ExternalScanState *node, ExprContext *exprCtxt)
{
	EState	   *estate;
	Index		scanrelid;
	FileScanDesc fileScan;

	estate = node->ss.ps.state;
	scanrelid = ((SeqScan *) node->ss.ps.plan)->scanrelid;

	/* If this is re-scanning of PlanQual ... */
	if (estate->es_evTuple != NULL &&
		estate->es_evTuple[scanrelid - 1] != NULL)
	{
		estate->es_evTupleNull[scanrelid - 1] = false;
		return;
	}
	fileScan = node->ess_ScanDesc;

	ItemPointerSet(&node->cdb_fake_ctid, 0, 0);

	external_rescan(fileScan);
}

void
ExecEagerFreeExternalScan(ExternalScanState *node)
{
	Assert(node->ess_ScanDesc != NULL);
	external_endscan(node->ess_ScanDesc);
}
