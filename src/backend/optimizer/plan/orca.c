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
#include "cdb/cdbplan.h"
#include "cdb/cdbvars.h"
#include "nodes/makefuncs.h"
#include "optimizer/orca.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/transform.h"
#include "portability/instr_time.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"

/* GPORCA entry point */
extern PlannedStmt * GPOPTOptimizedPlan(Query *parse, bool *had_unexpected_failure);

static Plan *remove_redundant_results(PlannerInfo *root, Plan *plan);
static Node *remove_redundant_results_mutator(Node *node, void *);
static bool can_replace_tlist(Plan *plan);
static Node *push_down_expr_mutator(Node *node, List *child_tlist);

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

	/* optimizer failed to produce a plan, log failure */
	if ((OPTIMIZER_ALL_FAIL == optimizer_log_failure) ||
		(fUnexpectedFailure && OPTIMIZER_UNEXPECTED_FAIL == optimizer_log_failure) || 		/* unexpected fall back */
		(!fUnexpectedFailure && OPTIMIZER_EXPECTED_FAIL == optimizer_log_failure))			/* expected fall back */
	{
		if (fUnexpectedFailure)
		{
			elog(LOG, "Pivotal Optimizer (GPORCA) failed to produce plan (unexpected)");
		}
		else
		{
			elog(LOG, "Pivotal Optimizer (GPORCA) failed to produce plan");
		}
		return;
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
	PlannerInfo		*root;
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
	glob->subplans = NIL;
	glob->subroots = NIL;
	glob->rewindPlanIDs = NULL;
	glob->transientPlan = false;
	glob->oneoffPlan = false;
	glob->share.shared_inputs = NULL;
	glob->share.shared_input_count = 0;
	glob->share.motStack = NIL;
	glob->share.qdShares = NULL;
	/* these will be filled in below, in the pre- and post-processing steps */
	glob->finalrtable = NIL;
	glob->subplans = NIL;
	glob->relationOids = NIL;
	glob->invalItems = NIL;
	glob->nParamExec = 0;

	root = makeNode(PlannerInfo);
	root->parse = parse;
	root->glob = glob;
	root->query_level = 1;
	root->planner_cxt = CurrentMemoryContext;
	root->wt_param_id = -1;

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
	pqueryCopy = preprocess_query_optimizer(root, pqueryCopy, boundParams);

	/* Ok, invoke ORCA. */
	result = GPOPTOptimizedPlan(pqueryCopy, &fUnexpectedFailure);

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
	glob->subplan_sliceIds = result->subplan_sliceIds;
	glob->numSlices = result->numSlices;
	glob->slices = result->slices;

	/*
	 * Fake a subroot for each subplan, so that postprocessing steps don't
	 * choke.
	 */
	glob->subroots = NIL;
	foreach(lp, glob->subplans)
	{
		PlannerInfo *subroot = makeNode(PlannerInfo);
		subroot->glob = glob;
		glob->subroots = lappend(glob->subroots, subroot);
	}

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

		collect_shareinput_producers(root, subplan);
	}
	collect_shareinput_producers(root, result->planTree);

	/* Post-process ShareInputScan nodes */
	(void) apply_shareinput_xslice(result->planTree, root);

	/*
	 * Fix ShareInputScans for EXPLAIN, like in standard_planner(). For all
	 * subplans first, and then for the main plan tree.
	 */
	foreach(lp, glob->subplans)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);

		lfirst(lp) = replace_shareinput_targetlists(root, subplan);
	}
	result->planTree = replace_shareinput_targetlists(root, result->planTree);

	result->planTree = remove_redundant_results(root, result->planTree);

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

		cdb_extract_plan_dependencies(root, subplan);
	}
	cdb_extract_plan_dependencies(root, result->planTree);

	/*
	 * Also extract dependencies from the original Query tree. This is needed
	 * to capture dependencies to e.g. views, which have been expanded at
	 * planning to the underlying tables, and don't appear anywhere in the
	 * resulting plan.
	 */
	extract_query_dependencies((Node *) pqueryCopy,
							   &relationOids,
							   &invalItems,
							   &pqueryCopy->hasRowSecurity);
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

/*
 * ORCA tends to generate gratuitous Result nodes for various reasons. We
 * try to clean it up here, as much as we can, by eliminating the Results
 * that are not really needed.
 */
static Plan *
remove_redundant_results(PlannerInfo *root, Plan *plan)
{
	plan_tree_base_prefix ctx;

	ctx.node = (Node *) root;

	return (Plan *) remove_redundant_results_mutator((Node *) plan, &ctx);
}

static Node *
remove_redundant_results_mutator(Node *node, void *ctx)
{
	if (!node)
		return NULL;

	if (IsA(node, Result))
	{
		Result	   *result_plan = (Result *) node;
		Plan	   *child_plan = result_plan->plan.lefttree;

		/*
		 * If this Result doesn't contain quals, hash filter or anything else
		 * funny, and the child node is projection capable, we can let the
		 * child node do the projection, and eliminate this Result.
		 *
		 * (We could probably push down quals and some other stuff to the child
		 * node if we worked a bit harder.)
		 */
		if (result_plan->resconstantqual == NULL &&
			result_plan->numHashFilterCols == 0 &&
			result_plan->plan.initPlan == NIL &&
			result_plan->plan.qual == NIL &&
			!expression_returns_set((Node *) result_plan->plan.targetlist) &&
			can_replace_tlist(child_plan))
		{
			List	   *tlist = result_plan->plan.targetlist;
			ListCell   *lc;

			child_plan = (Plan *)
				remove_redundant_results_mutator((Node *) child_plan, ctx);

			foreach(lc, tlist)
			{
				TargetEntry *tle = (TargetEntry *) lfirst(lc);

				tle->expr = (Expr *) push_down_expr_mutator((Node *) tle->expr,
															child_plan->targetlist);
			}

			child_plan->targetlist = tlist;
			child_plan->flow = result_plan->plan.flow;

			return (Node *) child_plan;
		}
	}

	return plan_tree_mutator(node,
							 remove_redundant_results_mutator,
							 ctx,
							 true);
}

/*
 * Can the target list of a Plan node safely be replaced?
 */
static bool
can_replace_tlist(Plan *plan)
{
	if (!plan)
		return false;

	/*
	 * SRFs in targetlists are quite funky. Don't mess with them.
	 * We could probably be smarter about them, but doesn't seem
	 * worth the trouble.
	 */
	if (expression_returns_set((Node *) plan->targetlist))
		return false;

	if (!is_projection_capable_plan(plan))
		return false;

	/*
	 * The Hash Filter column indexes in a Result node are based on
	 * the output target list. Can't change the target list if there's
	 * a Hash Filter, or it would mess up the column indexes.
	 */
	if (IsA(plan, Result))
	{
		Result	   *rplan = (Result *) plan;

		if (rplan->numHashFilterCols > 0)
			return false;
	}

	/*
	 * Split Update node also calculates a hash based on the output
	 * targetlist, like a Result with a Hash Filter.
	 */
	if (IsA(plan, SplitUpdate))
		return false;

	return true;
}

/*
 * Fix up a target list, by replacing outer-Vars with the exprs from
 * the child target list, when we're stripping off a Result node.
 */
static Node *
push_down_expr_mutator(Node *node, List *child_tlist)
{
	if (!node)
		return NULL;

	if (IsA(node, Var))
	{
		Var		   *var = (Var *) node;

		if (var->varno == OUTER_VAR && var->varattno > 0)
		{
			TargetEntry *child_tle = (TargetEntry *)
				list_nth(child_tlist, var->varattno - 1);
			return (Node *) child_tle->expr;
		}
	}
	return expression_tree_mutator(node, push_down_expr_mutator, child_tlist);
}
