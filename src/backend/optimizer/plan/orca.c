/*-------------------------------------------------------------------------
 *
 * orca.c
 *	  entrypoint to the ORCA planner and supporting routines
 *
 * This contains the entrypoint to the ORCA planner which is invoked via the
 * standard_planner function when the optimizer GUC is set to on. Additionally,
 * some supporting routines for planning with ORCA are contained herein.
 *
 * Portions Copyright (c) 2010-Present, Pivotal Inc
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/plan/orca.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "cdb/cdbmutate.h"		/* apply_shareinput */
#include "cdb/cdbvars.h"
#include "nodes/makefuncs.h"
#include "optimizer/orca.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/transform.h"
#include "portability/instr_time.h"
#include "utils/lsyscache.h"

/* GPORCA entry point */
extern PlannedStmt * PplstmtOptimize(Query *parse, bool *pfUnexpectedFailure);

/*
 * Logging of optimization outcome
 */
static void
log_optimizer(PlannedStmt *plan, bool fUnexpectedFailure)
{
	/* optimizer logging is not enabled */
	if (!optimizer_log)
		return;

	if (plan != NULL)
	{
		elog(DEBUG1, "GPORCA produced plan");
		return;
	}

	if (optimizer_trace_fallback)
		elog(INFO, "GPORCA failed to produce a plan, falling back to planner");

	/* optimizer failed to produce a plan, log failure */
	if (OPTIMIZER_ALL_FAIL == optimizer_log_failure)
	{
		elog(LOG, "Planner produced plan :%d", fUnexpectedFailure);
		return;
	}

	if (fUnexpectedFailure && OPTIMIZER_UNEXPECTED_FAIL == optimizer_log_failure)
	{
		/* unexpected fall back */
		elog(LOG, "Planner produced plan :%d", fUnexpectedFailure);
		return;
	}

	if (!fUnexpectedFailure && OPTIMIZER_EXPECTED_FAIL == optimizer_log_failure)
	{
		/* expected fall back */
		elog(LOG, "Planner produced plan :%d", fUnexpectedFailure);
	}
}


/*
 * optimize_query
 *		Plan the query using the GPORCA planner
 *
 * This is the main entrypoint for invoking Orca.
 */
PlannedStmt *
optimize_query(Query *parse, ParamListInfo boundParams)
{
	/* flag to check if optimizer unexpectedly failed to produce a plan */
	bool			fUnexpectedFailure = false;
	PlannerGlobal  *glob;
	Query		   *pqueryCopy;
	PlannedStmt    *result;
	List		   *relationOids;
	List		   *invalItems;
	ListCell	   *lc;
	ListCell	   *lp;

	/*
	 * Initialize a dummy PlannerGlobal struct. ORCA doesn't use it, but the
	 * pre- and post-processing steps do.
	 */
	glob = makeNode(PlannerGlobal);
	glob->paramlist = NIL;
	glob->subrtables = NIL;
	glob->rewindPlanIDs = NULL;
	glob->transientPlan = false;
	glob->oneoffPlan = false;
	glob->share.producers = NULL;
	glob->share.producer_count = 0;
	glob->share.sliceMarks = NULL;
	glob->share.motStack = NIL;
	glob->share.qdShares = NIL;
	glob->share.qdSlices = NIL;
	glob->share.nextPlanId = 0;
	/* these will be filled in below, in the pre- and post-processing steps */
	glob->finalrtable = NIL;
	glob->subplans = NIL;
	glob->relationOids = NIL;
	glob->invalItems = NIL;

	/* create a local copy to hand to the optimizer */
	pqueryCopy = (Query *) copyObject(parse);

	/*
	 * Pre-process the Query tree before calling optimizer. Currently, this
	 * performs only constant folding.
	 *
	 * Constant folding will add dependencies to functions or relations in
	 * glob->invalItems, for any functions that are inlined or eliminated
	 * away. (We will find dependencies to other objects later, after planning).
	 */
	pqueryCopy = preprocess_query_optimizer(glob, pqueryCopy, boundParams);

	/* Ok, invoke ORCA. */
	result = PplstmtOptimize(pqueryCopy, &fUnexpectedFailure);

	log_optimizer(result, fUnexpectedFailure);

	CHECK_FOR_INTERRUPTS();

	/*
	 * If ORCA didn't produce a plan, bail out and fall back to the Postgres
	 * planner.
	 */
	if (!result)
		return NULL;

	/*
	 * Post-process the plan.
	 */

	/*
	 * ORCA filled in the final range table and subplans directly in the
	 * PlannedStmt. We might need to modify them still, so copy them out to
	 * the PlannerGlobal struct.
	 */
	glob->finalrtable = result->rtable;
	glob->subplans = result->subplans;

	/*
	 * For optimizer, we already have share_id and the plan tree is already a
	 * tree. However, the apply_shareinput_dag_to_tree walker does more than
	 * DAG conversion. It will also populate column names for RTE_CTE entries
	 * that will be later used for readable column names in EXPLAIN, if
	 * needed.
	 */
	foreach(lp, glob->subplans)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);

		collect_shareinput_producers(glob, subplan, result->rtable);
	}
	collect_shareinput_producers(glob, result->planTree, result->rtable);

	/* Post-process ShareInputScan nodes */
	(void) apply_shareinput_xslice(result->planTree, glob);

	/*
	 * Fix ShareInputScans for EXPLAIN, like in standard_planner(). For all
	 * subplans first, and then for the main plan tree.
	 */
	foreach(lp, glob->subplans)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);

		lfirst(lp) = replace_shareinput_targetlists(glob, subplan, result->rtable);
	}
	result->planTree = replace_shareinput_targetlists(glob, result->planTree, result->rtable);

	/*
	 * To save on memory, and on the network bandwidth when the plan is
	 * dispatched to QEs, strip all subquery RTEs of the original Query
	 * objects.
	 */
	remove_subquery_in_RTEs((Node *) glob->finalrtable);

	/*
	 * For plan cache invalidation purposes, extract the OIDs of all
	 * relations in the final range table, and of all functions used in
	 * expressions in the plan tree. (In the regular planner, this is done
	 * in set_plan_references, see that for more comments.)
	 */
	foreach(lc, glob->finalrtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(lc);

		if (rte->rtekind == RTE_RELATION)
			glob->relationOids = lappend_oid(glob->relationOids,
											 rte->relid);
	}
	foreach(lp, glob->subplans)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);

		cdb_extract_plan_dependencies(glob, subplan);
	}
	cdb_extract_plan_dependencies(glob, result->planTree);

	/*
	 * Also extract dependencies from the original Query tree. This is needed
	 * to capture dependencies to e.g. views, which have been expanded at
	 * planning to the underlying tables, and don't appear anywhere in the
	 * resulting plan.
	 */
	extract_query_dependencies(list_make1(pqueryCopy),
							   &relationOids,
							   &invalItems);
	glob->relationOids = list_concat(glob->relationOids, relationOids);
	glob->invalItems = list_concat(glob->invalItems, invalItems);

	/*
	 * All done! Copy the PlannerGlobal fields that we modified back to the
	 * PlannedStmt before returning.
	 */
	result->rtable = glob->finalrtable;
	result->subplans = glob->subplans;
	result->relationOids = glob->relationOids;
	result->invalItems = glob->invalItems;
	result->oneoffPlan = glob->oneoffPlan;
	result->transientPlan = glob->transientPlan;

	return result;
}
