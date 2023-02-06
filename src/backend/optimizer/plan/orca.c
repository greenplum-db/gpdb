/*-------------------------------------------------------------------------
 *
 * orca.c
 *	  entrypoint to the ORCA planner and supporting routines
 *
 * This contains the entrypoint to the ORCA planner which is invoked via the
 * standard_planner function when the optimizer GUC is set to on. Additionally,
 * some supporting routines for planning with ORCA are contained herein.
 *
 * Portions Copyright (c) 2010-Present, VMware, Inc. or its affiliates
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
#include "optimizer/clauses.h"
#include "optimizer/optimizer.h"
#include "optimizer/orca.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/tlist.h"
#include "optimizer/transform.h"
#include "parser/parse_collate.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"
#include "portability/instr_time.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"

/* GPORCA entry point */
extern PlannedStmt * GPOPTOptimizedPlan(Query *parse, bool *had_unexpected_failure);

static Node *transformGroupedWindows(Node *node, void *context);

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
optimize_query(Query *parse, int cursorOptions, ParamListInfo boundParams)
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
	 * Fall back for updatable cursor
	 */
	if ((cursorOptions & CURSOR_OPT_UPDATABLE) != 0)
		return NULL;

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

	root = makeNode(PlannerInfo);
	root->parse = parse;
	root->glob = glob;
	root->query_level = 1;
	root->planner_cxt = CurrentMemoryContext;
	root->wt_param_id = -1;

	/* create a local copy to hand to the optimizer */
	pqueryCopy = (Query *) copyObject(parse);

	/*
	 * Pre-process the Query tree before calling optimizer.
	 *
	 * Constant folding will add dependencies to functions or relations in
	 * glob->invalItems, for any functions that are inlined or eliminated
	 * away. (We will find dependencies to other objects later, after planning).
	 */
	pqueryCopy = fold_constants(root, pqueryCopy, boundParams, GPOPT_MAX_FOLDED_CONSTANT_SIZE);

	/*
	 * If any Query in the tree mixes window functions and aggregates, we need to
	 * transform it such that the grouped query appears as a subquery
	 */
	pqueryCopy = (Query *) transformGroupedWindows((Node *) pqueryCopy, NULL);

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



/*
 * ORCA cannot deal with window functions in the same query with
 * grouping. If a query contains both, transformGroupedWindows()
 * transforms it into a a query with a subquer to avoid that:
 *
 * If an input query (Q) mixes window functions with aggregate
 * functions or grouping, then (per SQL:2003) we need to divide
 * it into an outer query, Q', that contains no aggregate calls
 * or grouping and an inner query, Q'', that contains no window
 * calls.
 *
 * Q' will have a 1-entry range table whose entry corresponds to
 * the results of Q''.
 *
 * Q'' will have the same range as Q and will be pushed down into
 * a subquery range table entry in Q'.
 *
 * As a result, the depth of outer references in Q'' and below
 * will increase, so we need to adjust non-zero xxxlevelsup fields
 * (Var, Aggref, and WindowFunc nodes) in Q'' and below.  At the end,
 * there will be no levelsup items referring to Q'.  Prior references
 * to Q will now refer to Q''; prior references to blocks above Q will
 * refer to the same blocks above Q'.)
 *
 * We do all this by creating a new Query node, subq, for Q''.  We
 * modify the input Query node, qry, in place for Q'.  (Since qry is
 * also the input, Q, be careful not to destroy values before we're
 * done with them.
 *
 * The function is structured as a mutator, so that we can transform
 * all of the Query nodes in the entire tree, bottom-up.
 */

/* Context for transformGroupedWindows() which mutates components
 * of a query that mixes windowing and aggregation or grouping.  It
 * accumulates context for eventual construction of a subquery (the
 * grouping query) during mutation of components of the outer query
 * (the windowing query).
 */
typedef struct
{
	List	   *subtlist;		/* target list for subquery */
	List	   *subgroupClause; /* group clause for subquery */
	List	   *subgroupingSets;	/* grouping sets for subquery */
	List	   *windowClause;	/* window clause for outer query */

	/*
	 * Scratch area for init_grouped_window context and map_sgr_mutator.
	 */
	Index	   *sgr_map;
	int			sgr_map_size;

	/*
	 * Scratch area for grouped_window_mutator and var_for_grouped_window_expr.
	 */
	List	   *subrtable;
	int			call_depth;
	TargetEntry *tle;
} grouped_window_ctx;

static void init_grouped_window_context(grouped_window_ctx * ctx, Query *qry);
static Var *var_for_grouped_window_expr(grouped_window_ctx * ctx, Node *expr, bool force);
static void discard_grouped_window_context(grouped_window_ctx * ctx);
static Node *map_sgr_mutator(Node *node, void *context);
static Node *grouped_window_mutator(Node *node, void *context);
static Alias *make_replacement_alias(Query *qry, const char *aname);
static char *generate_positional_name(AttrNumber attrno);
static List *generate_alternate_vars(Var *var, grouped_window_ctx * ctx);

static Node *
transformGroupedWindows(Node *node, void *context)
{
	if (node == NULL)
		return NULL;

	if (IsA(node, Query))
	{
		// do a depth-first recursion into any subqueries
		Query *qry = (Query *) query_tree_mutator((Query *) node, transformGroupedWindows, context, 0);

		Assert(IsA(qry, Query));

		/*
		 * we are done if this query doesn't have both window functions and group by/aggregates
		 */
		if (!qry->hasWindowFuncs ||
			!(qry->groupClause || qry->groupingSets || qry->hasAggs))
			return (Node *) qry;

		Query	   *subq;
		RangeTblEntry *rte;
		RangeTblRef *ref;
		Alias	   *alias;
		bool		hadSubLinks = qry->hasSubLinks;

		grouped_window_ctx ctx;

		Assert(qry->commandType == CMD_SELECT);
		Assert(!PointerIsValid(qry->utilityStmt));
		Assert(qry->returningList == NIL);

		/*
		 * Make the new subquery (Q'').  Note that (per SQL:2003) there can't be
		 * any window functions called in the WHERE, GROUP BY, or HAVING clauses.
		 */
		subq = makeNode(Query);
		subq->commandType = CMD_SELECT;
		subq->querySource = QSRC_PARSER;
		subq->canSetTag = true;
		subq->utilityStmt = NULL;
		subq->resultRelation = 0;
		subq->hasAggs = qry->hasAggs;
		subq->hasWindowFuncs = false;	/* reevaluate later */
		subq->hasSubLinks = qry->hasSubLinks;	/* reevaluate later */

		/* Core of subquery input table expression: */
		subq->rtable = qry->rtable; /* before windowing */
		subq->jointree = qry->jointree; /* before windowing */
		subq->targetList = NIL;		/* fill in later */

		subq->returningList = NIL;
		subq->groupClause = qry->groupClause;	/* before windowing */
		subq->groupingSets = qry->groupingSets; /* before windowing */
		subq->havingQual = qry->havingQual; /* before windowing */
		subq->windowClause = NIL;	/* by construction */
		subq->distinctClause = NIL; /* after windowing */
		subq->sortClause = NIL;		/* after windowing */
		subq->limitOffset = NULL;	/* after windowing */
		subq->limitCount = NULL;	/* after windowing */
		subq->rowMarks = NIL;
		subq->setOperations = NULL;

		/*
		 * Check if there is a window function in the join tree. If so we must
		 * mark hasWindowFuncs in the sub query as well.
		 */
		if (contain_window_function((Node *) subq->jointree))
			subq->hasWindowFuncs = true;

		/*
		 * Make the single range table entry for the outer query Q' as a wrapper
		 * for the subquery (Q'') currently under construction.
		 */
		rte = makeNode(RangeTblEntry);
		rte->rtekind = RTE_SUBQUERY;
		rte->subquery = subq;
		rte->alias = NULL;			/* fill in later */
		rte->eref = NULL;			/* fill in later */
		rte->inFromCl = true;
		rte->requiredPerms = ACL_SELECT;

		/*
		 * Default? rte->inh = 0; rte->checkAsUser = 0;
		 */

		/*
		 * Make a reference to the new range table entry .
		 */
		ref = makeNode(RangeTblRef);
		ref->rtindex = 1;

		/*
		 * Set up context for mutating the target list.  Careful. This is trickier
		 * than it looks.  The context will be "primed" with grouping targets.
		 */
		init_grouped_window_context(&ctx, qry);

		/*
		 * Begin rewriting the outer query in place.
		 */
		qry->hasAggs = false;		/* by construction */
		/* qry->hasSubLinks -- reevaluate later. */

		/* Core of outer query input table expression: */
		qry->rtable = list_make1(rte);
		qry->jointree = (FromExpr *) makeNode(FromExpr);
		qry->jointree->fromlist = list_make1(ref);
		qry->jointree->quals = NULL;
		/* qry->targetList -- to be mutated from Q to Q' below */

		qry->groupClause = NIL;		/* by construction */
		qry->groupingSets = NIL;	/* by construction */
		qry->havingQual = NULL;		/* by construction */

		/*
		 * Mutate the Q target list and windowClauses for use in Q' and, at the
		 * same time, update state with info needed to assemble the target list
		 * for the subquery (Q'').
		 */
		qry->targetList = (List *) grouped_window_mutator((Node *) qry->targetList, &ctx);
		qry->windowClause = (List *) grouped_window_mutator((Node *) qry->windowClause, &ctx);
		qry->hasSubLinks = checkExprHasSubLink((Node *) qry->targetList);

		/*
		 * New subquery fields
		 */
		subq->targetList = ctx.subtlist;
		subq->groupClause = ctx.subgroupClause;
		subq->groupingSets = ctx.subgroupingSets;

		/*
		 * We always need an eref, but we shouldn't really need a filled in alias.
		 * However, view deparse (or at least the fix for MPP-2189) wants one.
		 */
		alias = make_replacement_alias(subq, "Window");
		rte->eref = copyObject(alias);
		rte->alias = alias;

		/*
		 * Accommodate depth change in new subquery, Q''.
		 */
		IncrementVarSublevelsUpInTransformGroupedWindows((Node *) subq, 1, 1);

		/* Might have changed. */
		subq->hasSubLinks = checkExprHasSubLink((Node *) subq);

		Assert(PointerIsValid(qry->targetList));
		Assert(IsA(qry->targetList, List));

		/*
		 * Use error instead of assertion to "use" hadSubLinks and keep compiler
		 * happy.
		 */
		if (hadSubLinks != (qry->hasSubLinks || subq->hasSubLinks))
			elog(ERROR, "inconsistency detected in internal grouped windows transformation");

		discard_grouped_window_context(&ctx);

		return (Node *) qry;
	}

	/*
	 * for all other node types, just keep walking the tree
	 */
	return expression_tree_mutator(node, transformGroupedWindows, context);
}


/* Helper for transformGroupedWindows:
 *
 * Prime the subquery target list in the context with the grouping
 * and windowing attributes from the given query and adjust the
 * subquery group clauses in the context to agree.
 *
 * Note that we arrange dense sortgroupref values and stash the
 * referents on the front of the subquery target list.  This may
 * be over-kill, but the grouping extension code seems to like it
 * this way.
 *
 * Note that we only transfer sortgroupref values associated with
 * grouping and windowing to the subquery context.  The subquery
 * shouldn't care about ordering, etc. XXX
 */
static void
init_grouped_window_context(grouped_window_ctx * ctx, Query *qry)
{
	List	   *grp_tles;
	List	   *grp_sortops;
	List	   *grp_eqops;
	ListCell   *lc = NULL;
	Index		maxsgr = 0;

	get_sortgroupclauses_tles(qry->groupClause, qry->targetList,
							  &grp_tles, &grp_sortops, &grp_eqops);
	list_free(grp_sortops);
	maxsgr = maxSortGroupRef(grp_tles, true);

	ctx->subtlist = NIL;
	ctx->subgroupClause = NIL;
	ctx->subgroupingSets = NIL;

	/*
	 * Set up scratch space.
	 */

	ctx->subrtable = qry->rtable;

	/*
	 * Map input = outer query sortgroupref values to subquery values while
	 * building the subquery target list prefix.
	 */
	ctx->sgr_map = palloc0((maxsgr + 1) * sizeof(ctx->sgr_map[0]));
	ctx->sgr_map_size = maxsgr + 1;
	foreach(lc, grp_tles)
	{
		TargetEntry *tle;
		Index		old_sgr;

		tle = (TargetEntry *) copyObject(lfirst(lc));
		old_sgr = tle->ressortgroupref;

		ctx->subtlist = lappend(ctx->subtlist, tle);
		tle->resno = list_length(ctx->subtlist);
		tle->ressortgroupref = tle->resno;
		tle->resjunk = false;

		ctx->sgr_map[old_sgr] = tle->ressortgroupref;
	}

	/* Miscellaneous scratch area. */
	ctx->call_depth = 0;
	ctx->tle = NULL;

	/* Revise grouping into ctx->subgroupClause */
	ctx->subgroupClause = (List *) map_sgr_mutator((Node *) qry->groupClause, ctx);
	ctx->subgroupingSets = (List *) map_sgr_mutator((Node *) qry->groupingSets, ctx);
}


/* Helper for transformGroupedWindows */
static void
discard_grouped_window_context(grouped_window_ctx * ctx)
{
	ctx->subtlist = NIL;
	ctx->subgroupClause = NIL;
	ctx->subgroupingSets = NIL;
	ctx->tle = NULL;
	if (ctx->sgr_map)
		pfree(ctx->sgr_map);
	ctx->sgr_map = NULL;
	ctx->subrtable = NULL;
}


/* Helper for transformGroupedWindows:
 *
 * Look for the given expression in the context's subtlist.  If
 * none is found and the force argument is true, add a target
 * for it.  Make and return a variable referring to the target
 * with the matching expression, or return NULL, if no target
 * was found/added.
 */
static Var *
var_for_grouped_window_expr(grouped_window_ctx * ctx, Node *expr, bool force)
{
	Var		   *var = NULL;
	TargetEntry *tle = tlist_member((Expr *) expr, ctx->subtlist);

	if (tle == NULL && force)
	{
		tle = makeNode(TargetEntry);
		ctx->subtlist = lappend(ctx->subtlist, tle);
		tle->expr = (Expr *) expr;
		tle->resno = list_length(ctx->subtlist);

		/*
		 * See comment in grouped_window_mutator for why level 3 is
		 * appropriate.
		 */
		if (ctx->call_depth == 3 && ctx->tle != NULL && ctx->tle->resname != NULL)
		{
			tle->resname = pstrdup(ctx->tle->resname);
		}
		else
		{
			tle->resname = generate_positional_name(tle->resno);
		}
		tle->ressortgroupref = 0;
		tle->resorigtbl = 0;
		tle->resorigcol = 0;
		tle->resjunk = false;
	}

	if (tle != NULL)
	{
		var = makeNode(Var);
		var->varno = 1;			/* one and only */
		var->varattno = tle->resno; /* by construction */
		var->vartype = exprType((Node *) tle->expr);
		var->vartypmod = exprTypmod((Node *) tle->expr);
		var->varcollid = exprCollation((Node *) tle->expr);
		var->varlevelsup = 0;
		var->varnoold = 1;
		var->varoattno = tle->resno;
		var->location = 0;
	}

	return var;
}


/* Helper for transformGroupedWindows:
 *
 * Mutator for subquery groupingClause to adjust sortgroupref values
 * based on map developed while priming context target list.
 */
static Node *
map_sgr_mutator(Node *node, void *context)
{
	grouped_window_ctx *ctx = (grouped_window_ctx *) context;

	if (!node)
		return NULL;

	if (IsA(node, List))
	{
		ListCell   *lc;
		List	   *new_lst = NIL;

		foreach(lc, (List *) node)
		{
			Node	   *newnode = lfirst(lc);

			newnode = map_sgr_mutator(newnode, ctx);
			new_lst = lappend(new_lst, newnode);
		}
		return (Node *) new_lst;
	}
	else if (IsA(node, IntList))
	{
		ListCell   *lc;
		List	   *new_lst = NIL;

		foreach(lc, (List *) node)
		{
			int			sortgroupref = lfirst_int(lc);

			if (sortgroupref < 0 || sortgroupref >= ctx->sgr_map_size)
				elog(ERROR, "sortgroupref %d out of bounds", sortgroupref);

			sortgroupref = ctx->sgr_map[sortgroupref];

			new_lst = lappend_int(new_lst, sortgroupref);
		}
		return (Node *) new_lst;
	}
	else if (IsA(node, SortGroupClause))
	{
		SortGroupClause *g = (SortGroupClause *) node;
		SortGroupClause *new_g = makeNode(SortGroupClause);

		memcpy(new_g, g, sizeof(SortGroupClause));
		new_g->tleSortGroupRef = ctx->sgr_map[g->tleSortGroupRef];
		return (Node *) new_g;
	}
	else if (IsA(node, GroupingSet))
	{
		GroupingSet *gset = (GroupingSet *) node;
		GroupingSet *newgset = (GroupingSet *) node;

		newgset = makeNode(GroupingSet);
		newgset->kind = gset->kind;
		newgset->content = (List *) map_sgr_mutator((Node *) gset->content, context);
		newgset->location = gset->location;

		return (Node *) newgset;
	}
	else
		elog(ERROR, "unexpected node type %d", nodeTag(node));
}




/*
 * Helper for transformGroupedWindows:
 *
 * Transform targets from Q into targets for Q' and place information
 * needed to eventually construct the target list for the subquery Q''
 * in the context structure.
 *
 * The general idea is to add expressions that must be evaluated in the
 * subquery to the subquery target list (in the context) and to replace
 * them with Var nodes in the outer query.
 *
 * If there are any Agg nodes in the Q'' target list, arrange
 * to set hasAggs to true in the subquery. (This should already be
 * done, though).
 *
 * If we're pushing down an entire TLE that has a resname, use
 * it as an alias in the upper TLE, too.  Facilitate this by copying
 * down the resname from an immediately enclosing TargetEntry, if any.
 *
 * The algorithm repeatedly searches the subquery target list under
 * construction (quadric), however we don't expect many targets so
 * we don't optimize this.  (Could, for example, use a hash or divide
 * the target list into var, expr, and group/aggregate function lists.)
 */

static Node *
grouped_window_mutator(Node *node, void *context)
{
	Node	   *result = NULL;

	grouped_window_ctx *ctx = (grouped_window_ctx *) context;

	if (!node)
		return result;

	ctx->call_depth++;

	if (IsA(node, TargetEntry))
	{
		TargetEntry *tle = (TargetEntry *) node;
		TargetEntry *new_tle = makeNode(TargetEntry);

		/* Copy the target entry. */
		new_tle->resno = tle->resno;
		if (tle->resname == NULL)
		{
			new_tle->resname = generate_positional_name(new_tle->resno);
		}
		else
		{
			new_tle->resname = pstrdup(tle->resname);
		}
		new_tle->ressortgroupref = tle->ressortgroupref;
		new_tle->resorigtbl = InvalidOid;
		new_tle->resorigcol = 0;
		new_tle->resjunk = tle->resjunk;

		/*
		 * This is pretty shady, but we know our call pattern.  The target
		 * list is at level 1, so we're interested in target entries at level
		 * 2.  We record them in context so var_for_grouped_window_expr can maybe make a
		 * better than default choice of alias.
		 */
		if (ctx->call_depth == 2)
		{
			ctx->tle = tle;
		}
		else
		{
			ctx->tle = NULL;
		}

		new_tle->expr = (Expr *) grouped_window_mutator((Node *) tle->expr, ctx);

		ctx->tle = NULL;
		result = (Node *) new_tle;
	}
	else if (IsA(node, Aggref))
	{
		/* Aggregation expression */
		result = (Node *) var_for_grouped_window_expr(ctx, node, true);
	}
	else if (IsA(node, GroupingFunc))
	{
		GroupingFunc *gfunc = (GroupingFunc *) node;
		GroupingFunc *newgfunc;

		newgfunc = (GroupingFunc *) copyObject((Node *) gfunc);

		newgfunc->refs = (List *) map_sgr_mutator((Node *) newgfunc->refs, ctx);

		result = (Node *) var_for_grouped_window_expr(ctx, (Node *) newgfunc, true);
	}
	else if (IsA(node, Var))
	{
		Var		   *var = (Var *) node;

		/*
		 * Since this is a Var (leaf node), we must be able to mutate it, else
		 * we can't finish the transformation and must give up.
		 */
		result = (Node *) var_for_grouped_window_expr(ctx, node, false);

		if (!result)
		{
			List	   *altvars = generate_alternate_vars(var, ctx);
			ListCell   *lc;

			foreach(lc, altvars)
			{
				result = (Node *) var_for_grouped_window_expr(ctx, lfirst(lc), false);
				if (result)
					break;
			}
		}

		if (!result)
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("unresolved grouping key in window query"),
					 errhint("You might need to use explicit aliases and/or to refer to grouping keys in the same way throughout the query, or turn optimizer=off.")));
		}
	}
	else if (IsA(node, SubLink))
	{
		/* put the subquery into Q'' */
		result = (Node *) var_for_grouped_window_expr(ctx, node, true /* force */);
	}
	else
	{
		/* Grouping expression; may not find one. */
		result = (Node *) var_for_grouped_window_expr(ctx, node, false /* force */);
	}


	if (!result)
	{
		result = expression_tree_mutator(node, grouped_window_mutator, ctx);
	}

	ctx->call_depth--;
	return result;
}

/*
 * Helper for transformGroupedWindows:
 *
 * Build an Alias for a subquery RTE representing the given Query.
 * The input string aname is the name for the overall Alias. The
 * attribute names are all found or made up.
 */
static Alias *
make_replacement_alias(Query *qry, const char *aname)
{
	ListCell   *lc = NULL;
	char	   *name = NULL;
	Alias	   *alias = makeNode(Alias);
	AttrNumber	attrno = 0;

	alias->aliasname = pstrdup(aname);
	alias->colnames = NIL;

	foreach(lc, qry->targetList)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);

		attrno++;

		if (tle->resname)
		{
			/* Prefer the target's resname. */
			name = pstrdup(tle->resname);
		}
		else if (IsA(tle->expr, Var))
		{
			/*
			 * If the target expression is a Var, use the name of the
			 * attribute in the query's range table.
			 */
			Var		   *var = (Var *) tle->expr;
			RangeTblEntry *rte = rt_fetch(var->varno, qry->rtable);

			name = pstrdup(get_rte_attribute_name(rte, var->varattno));
		}
		else
		{
			/* If all else, fails, generate a name based on position. */
			name = generate_positional_name(attrno);
		}

		alias->colnames = lappend(alias->colnames, makeString(name));
	}
	return alias;
}

/*
 * Helper for transformGroupedWindows:
 *
 * Make a palloc'd C-string named for the input attribute number.
 */
static char *
generate_positional_name(AttrNumber attrno)
{
	int			rc = 0;
	char		buf[NAMEDATALEN];

	rc = snprintf(buf, sizeof(buf),
				  "att_%d", attrno);
	if (rc == EOF || rc < 0 || rc >= sizeof(buf))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("can't generate internal attribute name")));
	}
	return pstrdup(buf);
}

/*
 * Helper for transformGroupedWindows:
 *
 * Find alternate Vars on the range of the input query that are aliases
 * (modulo ANSI join) of the input Var on the range and that occur in the
 * target list of the input query.
 *
 * If the input Var references a join result, there will be a single
 * alias.  If not, we need to search the range table for occurrences
 * of the input Var in some join result's RTE and add a Var referring
 * to the appropriate attribute of the join RTE to the list.
 *
 * This is not efficient, but the need is rare (MPP-12082) so we don't
 * bother to precompute this.
 */
static List *
generate_alternate_vars(Var *invar, grouped_window_ctx * ctx)
{
	List	   *rtable = ctx->subrtable;
	RangeTblEntry *inrte;
	List	   *alternates = NIL;

	Assert(IsA(invar, Var));

	inrte = rt_fetch(invar->varno, rtable);

	if (inrte->rtekind == RTE_JOIN)
	{
		Node	   *ja = list_nth(inrte->joinaliasvars, invar->varattno - 1);

		/*
		 * Though Node types other than Var (e.g., CoalesceExpr or Const) may
		 * occur as joinaliasvars, we ignore them.
		 */
		if (IsA(ja, Var))
		{
			alternates = lappend(alternates, copyObject(ja));
		}
	}
	else
	{
		ListCell   *jlc;
		Index		varno = 0;

		foreach(jlc, rtable)
		{
			RangeTblEntry *rte = (RangeTblEntry *) lfirst(jlc);

			varno++;			/* This RTE's varno */

			if (rte->rtekind == RTE_JOIN)
			{
				ListCell   *alc;
				AttrNumber	attno = 0;

				foreach(alc, rte->joinaliasvars)
				{
					ListCell   *tlc;
					Node	   *altnode = lfirst(alc);
					Var		   *altvar = (Var *) altnode;

					attno++;	/* This attribute's attno in its join RTE */

					if (!IsA(altvar, Var) || !equal(invar, altvar))
						continue;

					/* Look for a matching Var in the target list. */

					foreach(tlc, ctx->subtlist)
					{
						TargetEntry *tle = (TargetEntry *) lfirst(tlc);
						Var		   *v = (Var *) tle->expr;

						if (IsA(v, Var) && v->varno == varno && v->varattno == attno)
						{
							alternates = lappend(alternates, tle->expr);
						}
					}
				}
			}
		}
	}
	return alternates;
}
