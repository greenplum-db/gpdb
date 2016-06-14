/*-------------------------------------------------------------------------
 *
 * nodeSubqueryscan.c
 *	  Support routines for scanning subqueries (subselects in rangetable).
 *
 * This is just enough different from sublinks (nodeSubplan.c) to mean that
 * we need two sets of code.  Ought to look at trying to unify the cases.
 *
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/executor/nodeSubqueryscan.c,v 1.35 2007/01/05 22:19:28 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		ExecSubqueryScan			scans a subquery.
 *		ExecSubqueryNext			retrieve next tuple in sequential order.
 *		ExecInitSubqueryScan		creates and initializes a subqueryscan node.
 *		ExecEndSubqueryScan			releases any storage allocated.
 *		ExecSubqueryReScan			rescans the relation
 *
 */
#include "postgres.h"

#include "cdb/cdbvars.h"
#include "executor/execdebug.h"
#include "executor/nodeSubqueryscan.h"
#include "optimizer/var.h"              /* CDB: contain_var_reference() */
#include "parser/parsetree.h"

static TupleTableSlot *SubqueryNext(SubqueryScanState *node);

/* ----------------------------------------------------------------
 *						Scan Support
 * ----------------------------------------------------------------
 */
/* ----------------------------------------------------------------
 *		SubqueryNext
 *
 *		This is a workhorse for ExecSubqueryScan
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
SubqueryNext(SubqueryScanState *node)
{
	EState	   *estate;
	ScanDirection direction;
	TupleTableSlot *slot;

	/*
	 * get information from the estate and scan state
	 */
	estate = node->ss.ps.state;
	direction = estate->es_direction;

	/*
	 * We need not support EvalPlanQual here, since we are not scanning a real
	 * relation.
	 */

	/*
	 * Get the next tuple from the sub-query.
	 */
	node->sss_SubEState->es_direction = direction;

	slot = ExecProcNode(node->subplan);

	/*
	 * We just overwrite our ScanTupleSlot with the subplan's result slot,
	 * rather than expending the cycles for ExecCopySlot().
	 */
	node->ss.ss_ScanTupleSlot = slot;

    /*
     * CDB: Label each row with a synthetic ctid if needed for subquery dedup.
     */
    if (node->cdb_want_ctid &&
        !TupIsNull(slot))
    {
    	slot_set_ctid_from_fake(slot, &node->cdb_fake_ctid);
    }

    if (!TupIsNull(slot))
    {
        Gpmon_M_Incr_Rows_Out(GpmonPktFromSubqueryScanState(node));
        CheckSendPlanStateGpmonPkt(&node->ss.ps);
    }

    return slot;
}

/* ----------------------------------------------------------------
 *		ExecSubqueryScan(node)
 *
 *		Scans the subquery sequentially and returns the next qualifying
 *		tuple.
 *		It calls the ExecScan() routine and passes it the access method
 *		which retrieve tuples sequentially.
 *
 */

TupleTableSlot *
ExecSubqueryScan(SubqueryScanState *node)
{
	/*
	 * use SubqueryNext as access method
	 */
	return ExecScan(&node->ss, (ExecScanAccessMtd) SubqueryNext);
}

/* ----------------------------------------------------------------
 *		ExecInitSubqueryScan
 * ----------------------------------------------------------------
 */
SubqueryScanState *
ExecInitSubqueryScan(SubqueryScan *node, EState *estate, int eflags)
{
	SubqueryScanState *subquerystate;
	EState	   *sp_estate;

	/* check for unsupported flags */
	Assert(!(eflags & EXEC_FLAG_MARK));

	/*
	 * SubqueryScan should not have any "normal" children.  Also, if planner
	 * left anything in subrtable, it's fishy.
	 */
	Assert(outerPlan(node) == NULL);
	Assert(innerPlan(node) == NULL);
	Assert(node->subrtable == NIL);

	/*
	 * Since subquery nodes create its own executor state,
	 * and pass it down to its child nodes, we always
	 * initialize the subquery node. However, some
	 * fields are not initialized if not necessary, see
	 * below.
	 */

	/*
	 * create state structure
	 */
	subquerystate = makeNode(SubqueryScanState);
	subquerystate->ss.ps.plan = (Plan *) node;
	subquerystate->ss.ps.state = estate;

	/*
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &subquerystate->ss.ps);

	/*
	 * initialize child expressions
	 */
	subquerystate->ss.ps.targetlist = (List *)
		ExecInitExpr((Expr *) node->scan.plan.targetlist,
					 (PlanState *) subquerystate);
	subquerystate->ss.ps.qual = (List *)
		ExecInitExpr((Expr *) node->scan.plan.qual,
					 (PlanState *) subquerystate);

	/* Check if targetlist or qual contains a var node referencing the ctid column */
	subquerystate->cdb_want_ctid = contain_ctid_var_reference(&node->scan);
	ItemPointerSetInvalid(&subquerystate->cdb_fake_ctid);

#define SUBQUERYSCAN_NSLOTS 2

	/*
	 * tuple table initialization
	 */
	ExecInitResultTupleSlot(estate, &subquerystate->ss.ps);
	ExecInitScanTupleSlot(estate, &subquerystate->ss);

	/*
	 * initialize subquery
	 *
	 * This should agree with ExecInitSubPlan
	 *
	 * The subquery needs its own EState because it has its own rangetable. It
	 * shares our Param ID space and es_query_cxt, however.  XXX if rangetable
	 * access were done differently, the subquery could share our EState,
	 * which would eliminate some thrashing about in this module...
	 */
	sp_estate = CreateSubExecutorState(estate);
	subquerystate->sss_SubEState = sp_estate;

	sp_estate->es_range_table = estate->es_range_table;
	sp_estate->es_param_list_info = estate->es_param_list_info;
	sp_estate->es_param_exec_vals = estate->es_param_exec_vals;
	sp_estate->es_tupleTable = NIL;
	sp_estate->es_snapshot = estate->es_snapshot;
	sp_estate->es_crosscheck_snapshot = estate->es_crosscheck_snapshot;
	sp_estate->es_instrument = estate->es_instrument;
	sp_estate->es_plannedstmt = estate->es_plannedstmt;

	/*
	 * "Loan" the global slice table and map to the subplan EState.  The
	 * global state is already set up by the code that called us.
	 */
	sp_estate->es_sliceTable = estate->es_sliceTable;
	sp_estate->currentSliceIdInPlan = estate->currentSliceIdInPlan;
	sp_estate->currentExecutingSliceId = estate->currentExecutingSliceId;
	sp_estate->rootSliceId = estate->currentExecutingSliceId;
	
	/* 
	 * also load shared nodes list 
	 */
	sp_estate->es_sharenode = estate->es_sharenode;

	/*
	 * also loan the motion later state and interconnect state
	 */
	sp_estate->motionlayer_context = estate->motionlayer_context;
	sp_estate->interconnect_context = estate->interconnect_context;


	/*
	 * Start up the subplan (this is a very cut-down form of InitPlan())
	 */
	subquerystate->subplan = ExecInitNode(node->subplan, sp_estate, eflags);

	/* return borrowed share node list */
	estate->es_sharenode = sp_estate->es_sharenode;
	/*subquerystate->ss.ps.ps_TupFromTlist = false;*/

	/*
	 * Initialize scan tuple type (needed by ExecAssignScanProjectionInfo).
	 * Because the subplan is in its own memory context, we need to copy its
	 * result tuple type not just link to it; else the tupdesc will disappear
	 * too soon during shutdown.
	 */
	ExecAssignScanType(&subquerystate->ss,
			CreateTupleDescCopy(ExecGetResultType(subquerystate->subplan)));

	/*
	 * Initialize result tuple type and projection info.
	 */
	ExecAssignResultTypeFromTL(&subquerystate->ss.ps);
	ExecAssignScanProjectionInfo(&subquerystate->ss);

	initGpmonPktForSubqueryScan((Plan *)node, &subquerystate->ss.ps.gpmon_pkt, estate);

	return subquerystate;
}

int
ExecCountSlotsSubqueryScan(SubqueryScan *node)
{
	/*
	 * The subplan has its own tuple table and must not be counted here!
	 */
	return ExecCountSlotsNode(outerPlan(node)) +
		ExecCountSlotsNode(innerPlan(node)) +
		SUBQUERYSCAN_NSLOTS;
}

/* ----------------------------------------------------------------
 *		ExecEndSubqueryScan
 *
 *		frees any storage allocated through C routines.
 * ----------------------------------------------------------------
 */
void
ExecEndSubqueryScan(SubqueryScanState *node)
{
	/*
	 * Free the exprcontext
	 */
	ExecFreeExprContext(&node->ss.ps);

	/*
	 * clean out the upper tuple table
	 */
	if (node->ss.ss_ScanTupleSlot != NULL)
	{
		ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
		node->ss.ss_ScanTupleSlot = NULL;	/* not ours to clear */
	}

	/* gpmon */
	EndPlanStateGpmonPkt(&node->ss.ps);

	/*
	 * close down subquery
	 */
	ExecEndPlan(node->subplan, node->sss_SubEState);

	FreeExecutorState(node->sss_SubEState);
}

/* ----------------------------------------------------------------
 *		ExecSubqueryReScan
 *
 *		Rescans the relation.
 * ----------------------------------------------------------------
 */
void
ExecSubqueryReScan(SubqueryScanState *node, ExprContext *exprCtxt)
{
	EState	   *estate;

	estate = node->ss.ps.state;

	ItemPointerSet(&node->cdb_fake_ctid, 0, 0);

	/*
	 * ExecReScan doesn't know about my subplan, so I have to do
	 * changed-parameter signaling myself.	This is just as well, because the
	 * subplan has its own memory context in which its chgParam state lives.
	 */
	if (node->ss.ps.chgParam != NULL)
		UpdateChangedParamSet(node->subplan, node->ss.ps.chgParam);

	/*
	 * if chgParam of subnode is not null then plan will be re-scanned by
	 * first ExecProcNode.
	 */
	if (node->subplan->chgParam == NULL)
		ExecReScan(node->subplan, NULL);

	node->ss.ss_ScanTupleSlot = NULL;
	/*node->ss.ps.ps_TupFromTlist = false;*/

	Gpmon_M_Incr(GpmonPktFromSubqueryScanState(node), GPMON_SUBQUERYSCAN_RESCAN);
	CheckSendPlanStateGpmonPkt(&node->ss.ps);
}
	
void
initGpmonPktForSubqueryScan(Plan *planNode, gpmon_packet_t *gpmon_pkt, EState *estate)
{
	RangeTblEntry *rte;
	Assert(planNode != NULL && gpmon_pkt != NULL && IsA(planNode, SubqueryScan));

	rte = rt_fetch(((SubqueryScan *)planNode)->scan.scanrelid, estate->es_range_table);

	if (rte && rte->rtekind != RTE_VOID)
	{
		InitPlanNodeGpmonPkt(planNode, gpmon_pkt, estate, PMNT_SubqueryScan,
							 (int64)planNode->plan_rows, 
							 rte->eref->aliasname); 
	}
}
