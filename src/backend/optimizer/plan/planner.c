/*-------------------------------------------------------------------------
 *
 * planner.c
 *	  The query optimizer external interface.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/optimizer/plan/planner.c,v 1.240 2008/08/07 01:11:50 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <limits.h>

#include "catalog/pg_operator.h"
#include "executor/executor.h"
#include "executor/execHHashagg.h"
#include "executor/nodeAgg.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/orca.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/prep.h"
#include "optimizer/subselect.h"
#include "optimizer/transform.h"
#include "optimizer/tlist.h"
#include "optimizer/var.h"
#ifdef OPTIMIZER_DEBUG
#include "nodes/print.h"
#endif
#include "parser/parse_expr.h"
#include "parser/parse_oper.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "utils/lsyscache.h"
#include "utils/selfuncs.h"
#include "utils/syscache.h"

#include "catalog/pg_proc.h"
#include "cdb/cdbllize.h"
#include "cdb/cdbmutate.h"		/* apply_shareinput */
#include "cdb/cdbpartition.h"
#include "cdb/cdbpath.h"		/* cdbpath_segments */
#include "cdb/cdbpathtoplan.h"	/* cdbpathtoplan_create_flow() */
#include "cdb/cdbgroup.h"		/* grouping_planner extensions */
#include "cdb/cdbsetop.h"		/* motion utilities */
#include "cdb/cdbvars.h"


/* GUC parameter */
double cursor_tuple_fraction = DEFAULT_CURSOR_TUPLE_FRACTION;

/* Hook for plugins to get control in planner() */
planner_hook_type planner_hook = NULL;


/* Expression kind codes for preprocess_expression */
#define EXPRKIND_QUAL		0
#define EXPRKIND_TARGET		1
#define EXPRKIND_RTFUNC		2
#define EXPRKIND_VALUES		3
#define EXPRKIND_LIMIT		4
#define EXPRKIND_APPINFO	5
#define EXPRKIND_WINDOW_BOUND 6


static Node *preprocess_expression(PlannerInfo *root, Node *expr, int kind);
static void preprocess_qual_conditions(PlannerInfo *root, Node *jtnode);
static Plan *inheritance_planner(PlannerInfo *root);
static Plan *grouping_planner(PlannerInfo *root, double tuple_fraction);
static double preprocess_limit(PlannerInfo *root,
				 double tuple_fraction,
				 int64 *offset_est, int64 *count_est);
static void preprocess_groupclause(PlannerInfo *root);
static bool choose_hashed_distinct(PlannerInfo *root,
					   Plan *input_plan, List *input_pathkeys,
					   double tuple_fraction, double limit_tuples,
					   double dNumDistinctRows);
static List *make_subplanTargetList(PlannerInfo *root, List *tlist,
					   AttrNumber **groupColIdx, Oid **groupOperators, bool *need_tlist_eval);
static List *register_ordered_aggs(List *tlist, Node *havingqual, List *sub_tlist);

typedef struct
{
	List	   *tlist;
	Node	   *havingqual;
	List	   *sub_tlist;
	Index		last_sgr;
}	register_ordered_aggs_context;


static Node *register_ordered_aggs_mutator(Node *node,
							  register_ordered_aggs_context * context);
static void register_AggOrder(AggOrder * aggorder,
				  register_ordered_aggs_context * context);
static void locate_grouping_columns(PlannerInfo *root,
						List *stlist,
						List *sub_tlist,
						AttrNumber *groupColIdx);
static List *postprocess_setop_tlist(List *new_tlist, List *orig_tlist);

static Bitmapset *canonicalize_colref_list(Node *node);
static List *canonicalize_gs_list(List *gsl, bool ordinary);
static List *rollup_gs_list(List *gsl);
static List *add_gs_combinations(List *list, int n, int i, Bitmapset **base, Bitmapset **work);
static List *cube_gs_list(List *gsl);
static CanonicalGroupingSets *make_canonical_groupingsets(List *groupClause);
static int	gs_compare(const void *a, const void *b);
static void sort_canonical_gs_list(List *gs, int *p_nsets, Bitmapset ***p_sets);

static Plan *pushdown_preliminary_limit(Plan *plan, Node *limitCount, int64 count_est, Node *limitOffset, int64 offset_est);
static bool is_dummy_plan(Plan *plan);

static bool isSimplyUpdatableQuery(Query *query);


/*****************************************************************************
 *
 *	   Query optimizer entry point
 *
 * To support loadable plugins that monitor or modify planner behavior,
 * we provide a hook variable that lets a plugin get control before and
 * after the standard planning process.  The plugin would normally call
 * standard_planner().
 *
 * Note to plugin authors: standard_planner() scribbles on its Query input,
 * so you'd better copy that data structure if you want to plan more than once.
 *
 *****************************************************************************/
PlannedStmt *
planner(Query *parse, int cursorOptions, ParamListInfo boundParams)
{
	PlannedStmt *result;
	instr_time	starttime, endtime;

	if (planner_hook)
	{
		if (gp_log_optimization_time)
			INSTR_TIME_SET_CURRENT(starttime);

		START_MEMORY_ACCOUNT(MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_PlannerHook));
		{
			result = (*planner_hook) (parse, cursorOptions, boundParams);
		}
		END_MEMORY_ACCOUNT();

		if (gp_log_optimization_time)
		{
			INSTR_TIME_SET_CURRENT(endtime);
			INSTR_TIME_SUBTRACT(endtime, starttime);
			elog(LOG, "Planner Hook(s): %.3f ms", INSTR_TIME_GET_MILLISEC(endtime));
		}
	}
	else
		result = standard_planner(parse, cursorOptions, boundParams);

	return result;
}

PlannedStmt *
standard_planner(Query *parse, int cursorOptions, ParamListInfo boundParams)
{
	PlannedStmt *result;
	PlannerGlobal *glob;
	double		tuple_fraction;
	PlannerInfo *root;
	Plan	   *top_plan;
	ListCell   *lp,
			   *lr;
	PlannerConfig *config;
	instr_time		starttime;
	instr_time		endtime;

	/*
	 * If ORCA has been enabled, and we are in a state in which ORCA planning
	 * is supported, then go ahead.
	 */
	if (optimizer &&
		GP_ROLE_UTILITY != Gp_role && MASTER_CONTENT_ID == GpIdentity.segindex)
	{
		if (gp_log_optimization_time)
			INSTR_TIME_SET_CURRENT(starttime);

		START_MEMORY_ACCOUNT(MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Optimizer));
		{
			result = optimize_query(parse, boundParams);
		}
		END_MEMORY_ACCOUNT();

		if (gp_log_optimization_time)
		{
			INSTR_TIME_SET_CURRENT(endtime);
			INSTR_TIME_SUBTRACT(endtime, starttime);
			elog(LOG, "Optimizer Time: %.3f ms", INSTR_TIME_GET_MILLISEC(endtime));
		}

		if (result)
			return result;
	}

	/*
	 * Fall back to using the PostgreSQL planner in case Orca didn't run (in
	 * utility mode or on a segment) or if it didn't produce a plan.
	 */
	if (gp_log_optimization_time)
		INSTR_TIME_SET_CURRENT(starttime);

	/*
	 * Incorrectly indented on purpose to avoid re-indenting an entire upstream
	 * function
	 */
	START_MEMORY_ACCOUNT(MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Planner));
	{

	/* Cursor options may come from caller or from DECLARE CURSOR stmt */
	if (parse->utilityStmt &&
		IsA(parse->utilityStmt, DeclareCursorStmt))
	{
		cursorOptions |= ((DeclareCursorStmt *) parse->utilityStmt)->options;

		/* Also try to make any cursor declared with DECLARE CURSOR updatable. */
		cursorOptions |= CURSOR_OPT_UPDATABLE;
	}

	/*
	 * Set up global state for this planner invocation.  This data is needed
	 * across all levels of sub-Query that might exist in the given command,
	 * so we keep it in a separate struct that's linked to by each per-Query
	 * PlannerInfo.
	 */
	glob = makeNode(PlannerGlobal);

	glob->boundParams = boundParams;
	glob->paramlist = NIL;
	glob->subplans = NIL;
	glob->subrtables = NIL;
	glob->rewindPlanIDs = NULL;
	glob->finalrtable = NIL;
	glob->relationOids = NIL;
	glob->invalItems = NIL;
	glob->lastPHId = 0;
	glob->transientPlan = false;
	glob->oneoffPlan = false;
	/* ApplyShareInputContext initialization. */
	glob->share.producers = NULL;
	glob->share.producer_count = 0;
	glob->share.sliceMarks = NULL;
	glob->share.motStack = NIL;
	glob->share.qdShares = NIL;
	glob->share.qdSlices = NIL;
	glob->share.nextPlanId = 0;

	if ((cursorOptions & CURSOR_OPT_UPDATABLE) != 0)
		glob->simplyUpdatable = isSimplyUpdatableQuery(parse);
	else
		glob->simplyUpdatable = false;

	/* Determine what fraction of the plan is likely to be scanned */
	if (cursorOptions & CURSOR_OPT_FAST_PLAN)
	{
		/*
		 * We have no real idea how many tuples the user will ultimately FETCH
		 * from a cursor, but it is often the case that he doesn't want 'em
		 * all, or would prefer a fast-start plan anyway so that he can
		 * process some of the tuples sooner.  Use a GUC parameter to decide
		 * what fraction to optimize for.
		 */
		tuple_fraction = cursor_tuple_fraction;

		/*
		 * We document cursor_tuple_fraction as simply being a fraction,
		 * which means the edge cases 0 and 1 have to be treated specially
		 * here.  We convert 1 to 0 ("all the tuples") and 0 to a very small
		 * fraction.
		 */
		if (tuple_fraction >= 1.0)
			tuple_fraction = 0.0;
		else if (tuple_fraction <= 0.0)
			tuple_fraction = 1e-10;
	}
	else
	{
		/* Default assumption is we need all the tuples */
		tuple_fraction = 0.0;
	}

	parse = normalize_query(parse);

	config = DefaultPlannerConfig();

	/* primary planning entry point (may recurse for subqueries) */
	top_plan = subquery_planner(glob, parse, NULL, false, tuple_fraction, &root, config);

	/*
	 * If creating a plan for a scrollable cursor, make sure it can run
	 * backwards on demand.  Add a Material node at the top at need.
	 */
	if (cursorOptions & CURSOR_OPT_SCROLL)
	{
		if (!ExecSupportsBackwardScan(top_plan))
			top_plan = materialize_finished_plan(root, top_plan);
	}


	/*
	 * Fix sharing id and shared id.
	 *
	 * This must be called before set_plan_references and cdbparallelize.  The other mutator
	 * or tree walker assumes the input is a tree.  If there is plan sharing, we have a DAG. 
	 *
	 * apply_shareinput will fix shared_id, and change the DAG to a tree.
	 */
	forboth(lp, glob->subplans, lr, glob->subrtables)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);
		List	   *subrtable = (List *) lfirst(lr);

		lfirst(lp) = apply_shareinput_dag_to_tree(glob, subplan, subrtable);
	}
	top_plan = apply_shareinput_dag_to_tree(glob, top_plan, root->parse->rtable);

	/* final cleanup of the plan */
	Assert(glob->finalrtable == NIL);
	Assert(parse == root->parse);
	top_plan = set_plan_references(glob, top_plan, root->parse->rtable);
	/* ... and the subplans (both regular subplans and initplans) */
	Assert(list_length(glob->subplans) == list_length(glob->subrtables));
	forboth(lp, glob->subplans, lr, glob->subrtables)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);
		List	   *subrtable = (List *) lfirst(lr);

		lfirst(lp) = set_plan_references(glob, subplan, subrtable);
	}

	/* walk plan and remove unused initplans and their params */
	remove_unused_initplans(top_plan, root);

	/* walk subplans and fixup subplan node referring to same plan_id */
	SubPlanWalkerContext subplan_context;
	fixup_subplans(top_plan, root, &subplan_context);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		top_plan = cdbparallelize(root, top_plan, parse,
								  cursorOptions,
								  boundParams);

		/*
		 * cdbparallelize() mutates all the nodes, so the producer nodes we
		 * memorized earlier are no longer valid. apply_shareinput_xslice()
		 * will re-populate it, but clear it for now, just to make sure that
		 * we don't access the obsolete copies of the nodes.
		 */
		if (glob->share.producer_count > 0)
			memset(glob->share.producers, 0, glob->share.producer_count * sizeof(ShareInputScan *));

		/*
		 * cdbparallelize may create additional slices that may affect share
		 * input. need to mark material nodes that are split acrossed multi
		 * slices.
		 */
		top_plan = apply_shareinput_xslice(top_plan, glob);
	}

	/*
	 * Remove unused subplans.
	 * Executor initializes state for subplans even they are unused.
	 * When the generated subplan is not used and has motion inside,
	 * causing motionID not being assigned, which will break sanity
	 * check when executor tries to initialize subplan state.
	 */
	remove_unused_subplans(root, &subplan_context);
	bms_free(subplan_context.bms_subplans);

	top_plan = zap_trivial_result(root, top_plan);

	/* fix ShareInputScans for EXPLAIN */
	foreach(lp, glob->subplans)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);

		lfirst(lp) = replace_shareinput_targetlists(glob, subplan, glob->finalrtable);
	}
	top_plan = replace_shareinput_targetlists(glob, top_plan, glob->finalrtable);

	/*
	 * To save on memory, and on the network bandwidth when the plan is
	 * dispatched QEs, strip all subquery RTEs of the original Query objects.
	 */
	remove_subquery_in_RTEs((Node *) glob->finalrtable);

	/* build the PlannedStmt result */
	result = makeNode(PlannedStmt);

	result->commandType = parse->commandType;
	result->canSetTag = parse->canSetTag;
	result->transientPlan = glob->transientPlan;
	result->oneoffPlan = glob->oneoffPlan;
	result->planTree = top_plan;
	result->rtable = glob->finalrtable;
	result->resultRelations = root->resultRelations;
	result->utilityStmt = parse->utilityStmt;
	result->intoClause = parse->intoClause;
	result->subplans = glob->subplans;
	result->rewindPlanIDs = glob->rewindPlanIDs;
	result->returningLists = root->returningLists;
	result->result_partitions = root->result_partitions;
	result->result_aosegnos = root->result_aosegnos;
	result->rowMarks = parse->rowMarks;
	result->relationOids = glob->relationOids;
	result->invalItems = glob->invalItems;
	result->nParamExec = list_length(glob->paramlist);
	result->nMotionNodes = top_plan->nMotionNodes;
	result->nInitPlans = top_plan->nInitPlans;
	result->intoPolicy = GpPolicyCopy(CurrentMemoryContext, parse->intoPolicy);
	result->queryPartOids = NIL;
	result->queryPartsMetadata = NIL;
	result->numSelectorsPerScanId = NIL;

	result->simplyUpdatable = glob->simplyUpdatable;

	{
		ListCell *lc;

		foreach(lc, glob->relationOids)
		{
			Oid reloid = lfirst_oid(lc);

			if (rel_is_partitioned(reloid))
				result->queryPartOids = lappend_oid(result->queryPartOids, reloid);
		}
	}

	Assert(result->utilityStmt == NULL || IsA(result->utilityStmt, DeclareCursorStmt));

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		/*
		 * Generate a plan node id for each node. Used by gpmon. Note that
		 * this needs to be the last step of the planning when the structure
		 * of the plan is final.
		 */
		assign_plannode_id(result);
	}

	if (gp_log_optimization_time)
	{
		INSTR_TIME_SET_CURRENT(endtime);
		INSTR_TIME_SUBTRACT(endtime, starttime);
		elog(LOG, "Planner Time: %.3f ms", INSTR_TIME_GET_MILLISEC(endtime));
	}

	}
	END_MEMORY_ACCOUNT();

	return result;
}


/*--------------------
 * subquery_planner
 *	  Invokes the planner on a subquery.  We recurse to here for each
 *	  sub-SELECT found in the query tree.
 *
 * glob is the global state for the current planner run.
 * parse is the querytree produced by the parser & rewriter.
 * parent_root is the immediate parent Query's info (NULL at the top level).
 * hasRecursion is true if this is a recursive WITH query.
 * tuple_fraction is the fraction of tuples we expect will be retrieved.
 * tuple_fraction is interpreted as explained for grouping_planner, below.
 *
 * If subroot isn't NULL, we pass back the query's final PlannerInfo struct;
 * among other things this tells the output sort ordering of the plan.
 *
 * Basically, this routine does the stuff that should only be done once
 * per Query object.  It then calls grouping_planner.  At one time,
 * grouping_planner could be invoked recursively on the same Query object;
 * that's not currently true, but we keep the separation between the two
 * routines anyway, in case we need it again someday.
 *
 * subquery_planner will be called recursively to handle sub-Query nodes
 * found within the query's expressions and rangetable.
 *
 * Returns a query plan.
 *--------------------
 */
Plan *
subquery_planner(PlannerGlobal *glob, Query *parse,
				 PlannerInfo *parent_root,
				 bool hasRecursion,
				 double tuple_fraction,
				 PlannerInfo **subroot,
				 PlannerConfig *config)
{
	int			num_old_subplans = list_length(glob->subplans);
	PlannerInfo *root;
	Plan	   *plan;
	List	   *newHaving;
	bool		hasOuterJoins;
	ListCell   *l;

	/* Create a PlannerInfo data structure for this subquery */
	root = makeNode(PlannerInfo);
	root->parse = parse;
	root->glob = glob;
	root->query_level = parent_root ? parent_root->query_level + 1 : 1;
	root->parent_root = parent_root;
	root->planner_cxt = CurrentMemoryContext;
	root->init_plans = NIL;
	root->cte_plan_ids = NIL;
	root->eq_classes = NIL;
	root->init_plans = NIL;

	root->list_cteplaninfo = NIL;
	if (parse->cteList != NIL)
	{
		root->list_cteplaninfo = init_list_cteplaninfo(list_length(parse->cteList));
	}

	root->append_rel_list = NIL;

	Assert(config);
	root->config = config;

	if (Gp_role == GP_ROLE_DISPATCH && gp_session_id > -1)
	{
		/* Choose a segdb to which our singleton gangs should be dispatched. */
		gp_singleton_segindex = gp_session_id % getgpsegmentCount();
	}

	root->hasRecursion = hasRecursion;
	if (hasRecursion)
		root->wt_param_id = SS_assign_worktable_param(root);
	else
		root->wt_param_id = -1;
	root->non_recursive_plan = NULL;

	/*
	 * If there is a WITH list, process each WITH query and build an
	 * initplan SubPlan structure for it.
	 *
	 * Unlike upstrem, we do not use initplan + CteScan, so SS_process_ctes
	 * will generate unused initplans. Commenting out the following two
	 * lines.
	 */

	/*
	if (parse->cteList)
		SS_process_ctes(root);
	 */

	/*
	 * Ensure that jointree has been normalized. See
	 * normalize_query_jointree_mutator()
	 */
	AssertImply(parse->jointree->fromlist, list_length(parse->jointree->fromlist) == 1);

	/* CDB: Stash current query level's relids before pulling up subqueries. */
	root->currlevel_relids = get_relids_in_jointree((Node *) parse->jointree, false);

	/*
	 * Look for ANY and EXISTS SubLinks in WHERE and JOIN/ON clauses, and try
	 * to transform them into joins. Note that this step does not descend
	 * into subqueries; if we pull up any subqueries below, their SubLinks are
	 * processed just before pulling them up.
	 */
	if (parse->hasSubLinks)
		pull_up_sublinks(root);

	/*
	 * Scan the rangetable for set-returning functions, and inline them
	 * if possible (producing subqueries that might get pulled up next).
	 * Recursion issues here are handled in the same way as for IN clauses.
	 */
	inline_set_returning_functions(root);

	/*
	 * Check to see if any subqueries in the rangetable can be merged into
	 * this query.
	 */
	parse->jointree = (FromExpr *)
		pull_up_subqueries(root, (Node *) parse->jointree, NULL, NULL);

	/*
	 * Detect whether any rangetable entries are RTE_JOIN kind; if not, we can
	 * avoid the expense of doing flatten_join_alias_vars().  Also check for
	 * outer joins --- if none, we can skip reduce_outer_joins().
	 * This must be done after we have done pull_up_subqueries, of course.
	 */
	root->hasJoinRTEs = false;
	hasOuterJoins = false;
	foreach(l, parse->rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(l);

		if (rte->rtekind == RTE_JOIN)
		{
			root->hasJoinRTEs = true;
			if (IS_OUTER_JOIN(rte->jointype))
			{
				hasOuterJoins = true;
				/* Can quit scanning once we find an outer join */
				break;
			}
		}
	}

	/*
	 * Expand any rangetable entries that are inheritance sets into "append
	 * relations".  This can add entries to the rangetable, but they must be
	 * plain base relations not joins, so it's OK (and marginally more
	 * efficient) to do it after checking for join RTEs.  We must do it after
	 * pulling up subqueries, else we'd fail to handle inherited tables in
	 * subqueries.
	 */
	expand_inherited_tables(root);

	/* CDB: If parent RTE belongs to current query level, children do too. */
	foreach(l, root->append_rel_list)
	{
		AppendRelInfo *appinfo = (AppendRelInfo *) lfirst(l);

		if (bms_is_member(appinfo->parent_relid, root->currlevel_relids))
			root->currlevel_relids = bms_add_member(root->currlevel_relids,
													appinfo->child_relid);
	}

	/*
	 * Set hasHavingQual to remember if HAVING clause is present.  Needed
	 * because preprocess_expression will reduce a constant-true condition to
	 * an empty qual list ... but "HAVING TRUE" is not a semantic no-op.
	 */
	root->hasHavingQual = (parse->havingQual != NULL);

	/* Clear this flag; might get set in distribute_qual_to_rels */
	root->hasPseudoConstantQuals = false;

	/*
	 * Do expression preprocessing on targetlist and quals.
	 */
	parse->targetList = (List *)
		preprocess_expression(root, (Node *) parse->targetList,
							  EXPRKIND_TARGET);

	parse->returningList = (List *)
		preprocess_expression(root, (Node *) parse->returningList,
							  EXPRKIND_TARGET);

	preprocess_qual_conditions(root, (Node *) parse->jointree);

	parse->havingQual = preprocess_expression(root, parse->havingQual,
											  EXPRKIND_QUAL);

	parse->scatterClause = (List *)
		preprocess_expression(root, (Node *) parse->scatterClause,
							  EXPRKIND_TARGET);

	/*
	 * Do expression preprocessing on other expressions.
	 */
	foreach(l, parse->windowClause)
	{
		WindowClause *wc = (WindowClause *) lfirst(l);

		/* partitionClause/orderClause are sort/group expressions */
		wc->startOffset = preprocess_expression(root, wc->startOffset,
												EXPRKIND_WINDOW_BOUND);
		wc->endOffset = preprocess_expression(root, wc->endOffset,
											  EXPRKIND_WINDOW_BOUND);
	}

	parse->limitOffset = preprocess_expression(root, parse->limitOffset,
											   EXPRKIND_LIMIT);
	parse->limitCount = preprocess_expression(root, parse->limitCount,
											  EXPRKIND_LIMIT);

	root->append_rel_list = (List *)
		preprocess_expression(root, (Node *) root->append_rel_list,
							  EXPRKIND_APPINFO);

	/* Also need to preprocess expressions for function and values RTEs */
	foreach(l, parse->rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(l);

		if (rte->rtekind == RTE_FUNCTION || rte->rtekind == RTE_TABLEFUNCTION)
			rte->funcexpr = preprocess_expression(root, rte->funcexpr,
												  EXPRKIND_RTFUNC);
		else if (rte->rtekind == RTE_VALUES)
			rte->values_lists = (List *)
				preprocess_expression(root, (Node *) rte->values_lists,
									  EXPRKIND_VALUES);
	}

	/*
	 * In some cases we may want to transfer a HAVING clause into WHERE. We
	 * cannot do so if the HAVING clause contains aggregates (obviously) or
	 * volatile functions (since a HAVING clause is supposed to be executed
	 * only once per group).  Also, it may be that the clause is so expensive
	 * to execute that we're better off doing it only once per group, despite
	 * the loss of selectivity.  This is hard to estimate short of doing the
	 * entire planning process twice, so we use a heuristic: clauses
	 * containing subplans are left in HAVING.	Otherwise, we move or copy the
	 * HAVING clause into WHERE, in hopes of eliminating tuples before
	 * aggregation instead of after.
	 *
	 * If the query has explicit grouping then we can simply move such a
	 * clause into WHERE; any group that fails the clause will not be in the
	 * output because none of its tuples will reach the grouping or
	 * aggregation stage.  Otherwise we must have a degenerate (variable-free)
	 * HAVING clause, which we put in WHERE so that query_planner() can use it
	 * in a gating Result node, but also keep in HAVING to ensure that we
	 * don't emit a bogus aggregated row. (This could be done better, but it
	 * seems not worth optimizing.)
	 *
	 * Note that both havingQual and parse->jointree->quals are in
	 * implicitly-ANDed-list form at this point, even though they are declared
	 * as Node *.
	 */
	newHaving = NIL;
	foreach(l, (List *) parse->havingQual)
	{
		Node	   *havingclause = (Node *) lfirst(l);

		if (contain_agg_clause(havingclause) ||
			contain_volatile_functions(havingclause) ||
			contain_subplans(havingclause))
		{
			/* keep it in HAVING */
			newHaving = lappend(newHaving, havingclause);
		}
		else if (parse->groupClause &&
				 !contain_extended_grouping(parse->groupClause))
		{
			/* move it to WHERE */
			parse->jointree->quals = (Node *)
				lappend((List *) parse->jointree->quals, havingclause);
		}
		else
		{
			/* put a copy in WHERE, keep it in HAVING */
			parse->jointree->quals = (Node *)
				lappend((List *) parse->jointree->quals,
						copyObject(havingclause));
			newHaving = lappend(newHaving, havingclause);
		}
	}
	parse->havingQual = (Node *) newHaving;

	/*
	 * If we have any outer joins, try to reduce them to plain inner joins.
	 * This step is most easily done after we've done expression
	 * preprocessing.
	 */
	if (hasOuterJoins)
		reduce_outer_joins(root);

	/*
	 * Do the main planning.  If we have an inherited target relation, that
	 * needs special processing, else go straight to grouping_planner.
	 */
	if (parse->resultRelation &&
		rt_fetch(parse->resultRelation, parse->rtable)->inh)
		plan = inheritance_planner(root);
	else
		plan = grouping_planner(root, tuple_fraction);

	/*
	 * If any subplans were generated, or if we're inside a subplan, build
	 * initPlan list and extParam/allParam sets for plan nodes, and attach the
	 * initPlans to the top plan node.
	 */
	if (list_length(glob->subplans) != num_old_subplans ||
		root->query_level > 1)
	{
		Assert(root->parse == parse); /* GPDB isn't always careful about this. */
		SS_finalize_plan(root, plan, true);
	}

	/* Return internal info if caller wants it */
	if (subroot)
		*subroot = root;

	return plan;
}

/*
 * preprocess_expression
 *		Do subquery_planner's preprocessing work for an expression,
 *		which can be a targetlist, a WHERE clause (including JOIN/ON
 *		conditions), or a HAVING clause.
 */
static Node *
preprocess_expression(PlannerInfo *root, Node *expr, int kind)
{
	/*
	 * Fall out quickly if expression is empty.  This occurs often enough to
	 * be worth checking.  Note that null->null is the correct conversion for
	 * implicit-AND result format, too.
	 */
	if (expr == NULL)
		return NULL;

	/*
	 * If the query has any join RTEs, replace join alias variables with
	 * base-relation variables. We must do this before sublink processing,
	 * else sublinks expanded out from join aliases wouldn't get processed. We
	 * can skip it in VALUES lists, however, since they can't contain any Vars
	 * at all.
	 */
	if (root->hasJoinRTEs && kind != EXPRKIND_VALUES)
		expr = flatten_join_alias_vars(root, expr);

	if (root->parse->hasFuncsWithExecRestrictions)
	{
		if (kind == EXPRKIND_RTFUNC)
		{
			/* allowed */
		}
		else if (kind == EXPRKIND_TARGET)
		{
			/*
			 * Allowed in simple cases with no range table. For example,
			 * "SELECT func()" is allowed, but "SELECT func() FROM foo" is not.
			 */
			if (root->parse->rtable &&
				check_execute_on_functions((Node *) root->parse->targetList) != PROEXECLOCATION_ANY)
			{
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("function with EXECUTE ON restrictions cannot be used in the SELECT list of a query with FROM")));
			}
		}
		else
		{
			if (check_execute_on_functions((Node *) root->parse->targetList) != PROEXECLOCATION_ANY)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("function with EXECUTE ON restrictions cannot be used here")));
		}
	}

	/*
	 * Simplify constant expressions.
	 *
	 * Note: one essential effect here is to insert the current actual values
	 * of any default arguments for functions.  To ensure that happens, we
	 * *must* process all expressions here.  Previous PG versions sometimes
	 * skipped const-simplification if it didn't seem worth the trouble, but
	 * we can't do that anymore.
	 *
	 * Note: this also flattens nested AND and OR expressions into N-argument
	 * form.  All processing of a qual expression after this point must be
	 * careful to maintain AND/OR flatness --- that is, do not generate a tree
	 * with AND directly under AND, nor OR directly under OR.
	 */
	expr = eval_const_expressions(root, expr);

	/*
	 * If it's a qual or havingQual, canonicalize it.
	 */
	if (kind == EXPRKIND_QUAL)
	{
		expr = (Node *) canonicalize_qual((Expr *) expr);

#ifdef OPTIMIZER_DEBUG
		printf("After canonicalize_qual()\n");
		pprint(expr);
#endif
	}

	/* Expand SubLinks to SubPlans */
	if (root->parse->hasSubLinks)
		expr = SS_process_sublinks(root, expr, (kind == EXPRKIND_QUAL));

	/*
	 * XXX do not insert anything here unless you have grokked the comments in
	 * SS_replace_correlation_vars ...
	 */

	/* Replace uplevel vars with Param nodes (this IS possible in VALUES) */
	if (root->query_level > 1)
		expr = SS_replace_correlation_vars(root, expr);

	/*
	 * If it's a qual or havingQual, convert it to implicit-AND format. (We
	 * don't want to do this before eval_const_expressions, since the latter
	 * would be unable to simplify a top-level AND correctly. Also,
	 * SS_process_sublinks expects explicit-AND format.)
	 */
	if (kind == EXPRKIND_QUAL)
		expr = (Node *) make_ands_implicit((Expr *) expr);

	return expr;
}

/*
 * preprocess_qual_conditions
 *		Recursively scan the query's jointree and do subquery_planner's
 *		preprocessing work on each qual condition found therein.
 */
static void
preprocess_qual_conditions(PlannerInfo *root, Node *jtnode)
{
	if (jtnode == NULL)
		return;
	if (IsA(jtnode, RangeTblRef))
	{
		/* nothing to do here */
	}
	else if (IsA(jtnode, FromExpr))
	{
		FromExpr   *f = (FromExpr *) jtnode;
		ListCell   *l;

		foreach(l, f->fromlist)
			preprocess_qual_conditions(root, lfirst(l));

		f->quals = preprocess_expression(root, f->quals, EXPRKIND_QUAL);
	}
	else if (IsA(jtnode, JoinExpr))
	{
		JoinExpr   *j = (JoinExpr *) jtnode;

		preprocess_qual_conditions(root, j->larg);
		preprocess_qual_conditions(root, j->rarg);

		j->quals = preprocess_expression(root, j->quals, EXPRKIND_QUAL);
	}
	else
		elog(ERROR, "unrecognized node type: %d",
			 (int) nodeTag(jtnode));
}

/*
 * inheritance_planner
 *	  Generate a plan in the case where the result relation is an
 *	  inheritance set.
 *
 * We have to handle this case differently from cases where a source relation
 * is an inheritance set. Source inheritance is expanded at the bottom of the
 * plan tree (see allpaths.c), but target inheritance has to be expanded at
 * the top.  The reason is that for UPDATE, each target relation needs a
 * different targetlist matching its own column set.  Also, for both UPDATE
 * and DELETE, the executor needs the Append plan node at the top, else it
 * can't keep track of which table is the current target table.  Fortunately,
 * the UPDATE/DELETE target can never be the nullable side of an outer join,
 * so it's OK to generate the plan this way.
 *
 * Returns a query plan.
 */
static Plan *
inheritance_planner(PlannerInfo *root)
{
	Query	   *parse = root->parse;
	Index		parentRTindex = parse->resultRelation;
	List	   *subplans = NIL;
	List	   *resultRelations = NIL;
	List	   *returningLists = NIL;
	List	   *tlist = NIL;
	PlannerInfo subroot;
	ListCell   *l;

	/* MPP */
	Plan	   *plan;
	CdbLocusType append_locustype = CdbLocusType_Null;
	bool		locus_ok = TRUE;

	foreach(l, root->append_rel_list)
	{
		AppendRelInfo *appinfo = (AppendRelInfo *) lfirst(l);
		Plan	   *subplan;

		/* append_rel_list contains all append rels; ignore others */
		if (appinfo->parent_relid != parentRTindex)
			continue;

		/*
		 * Generate modified query with this rel as target.
		 */
		memcpy(&subroot, root, sizeof(PlannerInfo));
		subroot.parse = (Query *)
			adjust_appendrel_attrs(&subroot, (Node *) parse,
								   appinfo);
		subroot.returningLists = NIL;
		subroot.init_plans = NIL;
		/* We needn't modify the child's append_rel_list */
		/* There shouldn't be any OJ info to translate, as yet */
		Assert(subroot.join_info_list == NIL);
		/* and we haven't created PlaceHolderInfos, either */
		Assert(subroot.placeholder_list == NIL);

		/* Generate plan */
		subplan = grouping_planner(&subroot, 0.0 /* retrieve all tuples */ );

		/*
		 * If this child rel was excluded by constraint exclusion, exclude it
		 * from the plan.
		 *
		 * MPP-1544: perform this check before testing for loci compatibility
		 * we might have inserted a dummy table with incorrect locus
		 */
		if (is_dummy_plan(subplan))
			continue;

		/* MPP needs target loci to match. */
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			CdbLocusType locustype = (subplan->flow == NULL) ?
			CdbLocusType_Null : subplan->flow->locustype;

			if (append_locustype == CdbLocusType_Null && locus_ok)
			{
				append_locustype = locustype;
			}
			else
			{
				switch (locustype)
				{
					case CdbLocusType_Entry:
						locus_ok = locus_ok && (locustype == append_locustype);
						break;
					case CdbLocusType_Hashed:
					case CdbLocusType_HashedOJ:
					case CdbLocusType_Strewn:
						/* MPP-2023: Among subplans, these loci are okay. */
						break;
					case CdbLocusType_Null:
					case CdbLocusType_SingleQE:
					case CdbLocusType_General:
					case CdbLocusType_Replicated:
						/* These loci are not valid on base relations */
						locus_ok = FALSE;
						break;
					default:
						/* We should not be hitting this */
						locus_ok = FALSE;
						Assert(0);
						break;
				}
			}
			if (!locus_ok)
			{
				ereport(ERROR, (
								errcode(ERRCODE_CDB_INTERNAL_ERROR),
					 errmsg("incompatible loci in target inheritance set")));
			}
		}

		/* Save tlist from first rel for use below */
		if (subplans == NIL)
		{
			tlist = subplan->targetlist;
		}

		/**
		 * The grouping planner scribbles on the rtable e.g. to add pseudo columns.
		 * We need to keep track of this.
		 */
		parse->rtable = subroot.parse->rtable;

		subplans = lappend(subplans, subplan);

		/* Make sure any initplans from this rel get into the outer list */
		root->init_plans = list_concat(root->init_plans, subroot.init_plans);

		/* Build target-relations list for the executor */
		resultRelations = lappend_int(resultRelations, appinfo->child_relid);

		/* Build list of per-relation RETURNING targetlists */
		if (parse->returningList)
		{
			Assert(list_length(subroot.returningLists) == 1);
			returningLists = list_concat(returningLists,
										 subroot.returningLists);
		}
	}

	/**
	 * If due to constraint exclusions all the result relations have been removed,
	 * we need something upstream.
	 */
	if (resultRelations)
	{
		root->resultRelations = resultRelations;
	}
	else
	{
		root->resultRelations = list_make1_int(parse->resultRelation);
	}
	
	root->returningLists = returningLists;

	/* Mark result as unordered (probably unnecessary) */
	root->query_pathkeys = NIL;

	/*
	 * If we managed to exclude every child rel, return a dummy plan
	 */
	if (subplans == NIL)
	{
		root->resultRelations = list_make1_int(parentRTindex);
		/* although dummy, it must have a valid tlist for executor */
		tlist = preprocess_targetlist(root, parse->targetList);
		plan = (Plan *) make_result(root,
									tlist,
									(Node *) list_make1(makeBoolConst(false,
																	  false)),
									NULL);

		if (Gp_role == GP_ROLE_DISPATCH)
			mark_plan_general(plan);

		return plan;
	}

	/* Suppress Append if there's only one surviving child rel */
	if (list_length(subplans) == 1)
		plan = (Plan *) linitial(subplans);
	else
	{
		plan = (Plan *) make_append(subplans, true, tlist);

		/* MPP dispatch needs to know the kind of locus. */
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			switch (append_locustype)
			{
				case CdbLocusType_Entry:
					mark_plan_entry(plan);
					break;

				case CdbLocusType_Hashed:
				case CdbLocusType_HashedOJ:
				case CdbLocusType_Strewn:
					/* Depend on caller to avoid incompatible hash keys. */

					/*
					 * For our purpose (UPD/DEL target), strewn is good
					 * enough.
					 */
					mark_plan_strewn(plan);
					break;

				default:
					ereport(ERROR,
							(errcode(ERRCODE_CDB_INTERNAL_ERROR),
							 errmsg("unexpected locus assigned to target inheritance set")));
			}
		}
	}

	return plan;
}

#ifdef USE_ASSERT_CHECKING

static void grouping_planner_output_asserts(PlannerInfo *root, Plan *plan);

/**
 * Ensure goodness of plans returned by grouping planner
 */
void
grouping_planner_output_asserts(PlannerInfo *root, Plan *plan)
{
	/*
	 * Ensure that plan refers to vars that have varlevelsup = 0 AND varno is
	 * in the rtable
	 */
	List	   *allVars = extract_nodes(root->glob, (Node *) plan, T_Var);
	ListCell   *lc = NULL;

	foreach(lc, allVars)
	{
		Var		   *var = (Var *) lfirst(lc);

		Assert(var->varlevelsup == 0 && "Plan contains vars that refer to outer plan.");
		Assert((var->varno == OUTER
		|| (var->varno > 0 && var->varno <= list_length(root->parse->rtable)))
			   && "Plan contains var that refer outside the rtable.");
		Assert(var->varno == var->varnoold && "Varno and varnoold do not agree!");

		/** If a pseudo column, there should be a corresponding entry in the relation */
		if (var->varattno <= FirstLowInvalidHeapAttributeNumber)
		{
			RangeTblEntry *rte = rt_fetch(var->varno, root->parse->rtable);

			Assert(rte);
			Assert(rte->pseudocols);
			Assert(list_length(rte->pseudocols) > var->varattno - FirstLowInvalidHeapAttributeNumber);
		}
	}
}
#endif

/*
 * getAnySubplan
 *	 Return one subplan for the given node.
 *
 * If the given node is an Append, the first subplan is returned.
 * If the given node is a SubqueryScan, its subplan is returned.
 * Otherwise, the lefttree of the given node is returned.
 */
static Plan *
getAnySubplan(Plan *node)
{
	Assert(is_plan_node((Node *) node));

	if (IsA(node, Append))
	{
		Append	   *append = (Append *) node;

		Assert(list_length(append->appendplans) > 0);
		return (Plan *) linitial(append->appendplans);
	}

	else if (IsA(node, SubqueryScan))
	{
		SubqueryScan *subqueryScan = (SubqueryScan *) node;

		return subqueryScan->subplan;
	}

	return node->lefttree;
}

/*--------------------
 * grouping_planner
 *	  Perform planning steps related to grouping, aggregation, etc.
 *	  This primarily means adding top-level processing to the basic
 *	  query plan produced by query_planner.
 *
 * tuple_fraction is the fraction of tuples we expect will be retrieved
 *
 * tuple_fraction is interpreted as follows:
 *	  0: expect all tuples to be retrieved (normal case)
 *	  0 < tuple_fraction < 1: expect the given fraction of tuples available
 *		from the plan to be retrieved
 *	  tuple_fraction >= 1: tuple_fraction is the absolute number of tuples
 *		expected to be retrieved (ie, a LIMIT specification)
 *
 * Returns a query plan.  Also, root->query_pathkeys is returned as the
 * actual output ordering of the plan (in pathkey format).
 *--------------------
 */
static Plan *
grouping_planner(PlannerInfo *root, double tuple_fraction)
{
	Query	   *parse = root->parse;
	List	   *tlist = parse->targetList;
	int64		offset_est = 0;
	int64		count_est = 0;
	double		limit_tuples = -1.0;
	Plan	   *result_plan;
	List	   *current_pathkeys = NIL;
	CdbPathLocus current_locus;
	Path	   *best_path = NULL;
	double		dNumGroups = 0;
	double		numDistinct = 1;
	List	   *distinctExprs = NIL;
	bool		must_gather;

	double		motion_cost_per_row =
	(gp_motion_cost_per_row > 0.0) ?
	gp_motion_cost_per_row :
	2.0 * cpu_tuple_cost;

	CdbPathLocus_MakeNull(&current_locus);

	/* Tweak caller-supplied tuple_fraction if have LIMIT/OFFSET */
	if (parse->limitCount || parse->limitOffset)
	{
		tuple_fraction = preprocess_limit(root, tuple_fraction,
										  &offset_est, &count_est);

		/*
		 * If we have a known LIMIT, and don't have an unknown OFFSET, we can
		 * estimate the effects of using a bounded sort.
		 */
		if (count_est > 0 && offset_est >= 0)
			limit_tuples = (double) count_est + (double) offset_est;
	}

	if (parse->setOperations)
	{
		List	   *set_sortclauses;

		/*
		 * If there's a top-level ORDER BY, assume we have to fetch all the
		 * tuples.	This might be too simplistic given all the hackery below
		 * to possibly avoid the sort; but the odds of accurate estimates
		 * here are pretty low anyway.
		 */
		if (parse->sortClause)
			tuple_fraction = 0.0;

		/*
		 * Construct the plan for set operations.  The result will not need
		 * any work except perhaps a top-level sort and/or LIMIT.  Note that
		 * any special work for recursive unions is the responsibility of
		 * plan_set_operations.
		 */
		result_plan = plan_set_operations(root, tuple_fraction,
										  &set_sortclauses);

		/*
		 * Calculate pathkeys representing the sort order (if any) of the set
		 * operation's result.  We have to do this before overwriting the sort
		 * key information...
		 */
		current_pathkeys = make_pathkeys_for_sortclauses(root,
														 set_sortclauses,
													 result_plan->targetlist,
														 true);

		/*
		 * We should not need to call preprocess_targetlist, since we must be
		 * in a SELECT query node.	Instead, use the targetlist returned by
		 * plan_set_operations (since this tells whether it returned any
		 * resjunk columns!), and transfer any sort key information from the
		 * original tlist.
		 */
		Assert(parse->commandType == CMD_SELECT);

		tlist = postprocess_setop_tlist(copyObject(result_plan->targetlist),
										tlist);

		/*
		 * Can't handle FOR UPDATE/SHARE here (parser should have checked
		 * already, but let's make sure).
		 */
		if (parse->rowMarks)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("SELECT FOR UPDATE/SHARE is not allowed with UNION/INTERSECT/EXCEPT")));

		/*
		 * Calculate pathkeys that represent result ordering requirements
		 */
		Assert(parse->distinctClause == NIL);
		root->sort_pathkeys = make_pathkeys_for_sortclauses(root,
															parse->sortClause,
															tlist,
															true);
	}
	else if ( parse->windowClause && parse->targetList &&
			  contain_window_function((Node *) parse->targetList) )
	{
		if (extract_nodes(NULL, (Node *) tlist, T_PercentileExpr) != NIL)
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("window function with WITHIN GROUP aggregate is not supported")));
		}
		/*
		 * Calculate pathkeys that represent ordering requirements. Stash
		 * them in PlannerInfo so that query_planner can canonicalize them.
		 */
		root->group_pathkeys = NIL;
		root->sort_pathkeys =
			make_pathkeys_for_sortclauses(root, parse->sortClause, tlist, true);

		
		result_plan = window_planner(root, tuple_fraction, &current_pathkeys);

		/*
		 * Recover sort pathkeys for use later.  These may or may not match
		 * the current_pathkeys resulting from the window plan.
		 */
		root->sort_pathkeys = make_pathkeys_for_sortclauses(root,
															parse->sortClause,
															result_plan->targetlist,
															true);
	}
	else
	{
		/* No set operations, do regular planning */
		List	   *sub_tlist;
		AttrNumber *groupColIdx = NULL;
		Oid		   *groupOperators = NULL;
		bool		need_tlist_eval = true;
		QualCost	tlist_cost;
		Path	   *cheapest_path;
		Path	   *sorted_path;
		long		numGroups = 0;
		AggClauseCounts agg_counts;
		int			numGroupCols;
		bool		use_hashed_grouping = false;
		bool		grpext = false;
		bool		has_within = false;
		CanonicalGroupingSets *canonical_grpsets;

		MemSet(&agg_counts, 0, sizeof(AggClauseCounts));

		/* A recursive query should always have setOperations */
		Assert(!root->hasRecursion);

		/* Preprocess GROUP BY clause, if any */
		if (parse->groupClause)
			preprocess_groupclause(root);
		numGroupCols = list_length(parse->groupClause);

		/* Preprocess targetlist */
		tlist = preprocess_targetlist(root, tlist);

		/* Obtain canonical grouping sets */
		canonical_grpsets = make_canonical_groupingsets(parse->groupClause);
		numGroupCols = canonical_grpsets->num_distcols;

		/*
		 * Clean up parse->groupClause if the grouping set is an empty
		 * set.
		 */
		if (numGroupCols == 0)
		{
			list_free(parse->groupClause);
			parse->groupClause = NIL;
		}

		grpext = is_grouping_extension(canonical_grpsets);
		has_within = extract_nodes(NULL, (Node *) tlist, T_PercentileExpr) != NIL;
		has_within |= extract_nodes(NULL, parse->havingQual, T_PercentileExpr) != NIL;

		if (grpext && has_within)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("WITHIN GROUP aggregate cannot be used in GROUPING SETS query")));

		/*
		 * Calculate pathkeys that represent grouping/ordering requirements.
		 * Stash them in PlannerInfo so that query_planner can canonicalize
		 * them after EquivalenceClasses have been formed.  The sortClause
		 * is certainly sort-able, but GROUP BY and DISTINCT might not be,
		 * in which case we just leave their pathkeys empty.
		 */
		if (parse->groupClause &&
			grouping_is_sortable(parse->groupClause))
			root->group_pathkeys =
			make_pathkeys_for_groupclause(root,
										  parse->groupClause,
										  tlist);
		else
			root->group_pathkeys = NIL;

		if (parse->distinctClause &&
			grouping_is_sortable(parse->distinctClause))
			root->distinct_pathkeys =
				make_pathkeys_for_sortclauses(root,
											  parse->distinctClause,
											  tlist,
											  false);
		else
			root->distinct_pathkeys = NIL;

		root->sort_pathkeys =
			make_pathkeys_for_sortclauses(root,
										  parse->sortClause,
										  tlist,
										  false);

		/*
		 * Will need actual number of aggregates for estimating costs.
		 *
		 * Note: we do not attempt to detect duplicate aggregates here; a
		 * somewhat-overestimated count is okay for our present purposes.
		 *
		 * Note: think not that we can turn off hasAggs if we find no aggs. It
		 * is possible for constant-expression simplification to remove all
		 * explicit references to aggs, but we still have to follow the
		 * aggregate semantics (eg, producing only one output row).
		 */
		MemSet(&agg_counts, 0, sizeof(AggClauseCounts));

		if (parse->hasAggs)
		{
			count_agg_clauses((Node *) tlist, &agg_counts);
			count_agg_clauses(parse->havingQual, &agg_counts);
		}

		/*
		 * Generate appropriate target list for subplan; may be different from
		 * tlist if grouping or aggregation is needed.
		 */
		sub_tlist = make_subplanTargetList(root, tlist,
										   &groupColIdx, &groupOperators, &need_tlist_eval);

		/*
		 * Augment the subplan target list to include targets for ordered
		 * aggregates.  As a side effect, this may scribble updated sort group
		 * ref values into AggOrder nodes within Aggref nodes of the query.  A
		 * pity, but it would harder to do this earlier.
		 */
		sub_tlist = register_ordered_aggs(tlist,
										  root->parse->havingQual,
										  sub_tlist);

		/*
		 * Figure out whether we want a sorted result from query_planner.
		 *
		 * If we have a sortable GROUP BY clause, then we want a result sorted
		 * properly for grouping.  Otherwise, if there's a sortable DISTINCT
		 * clause that's more rigorous than the ORDER BY clause, we try to
		 * produce output that's sufficiently well sorted for the DISTINCT.
		 * Otherwise, if there is an ORDER BY clause, we want to sort by the
		 * ORDER BY clause.
		 *
		 * Note: if we have both ORDER BY and GROUP BY, and ORDER BY is a
		 * superset of GROUP BY, it would be tempting to request sort by ORDER
		 * BY --- but that might just leave us failing to exploit an available
		 * sort order at all.  Needs more thought.  The choice for DISTINCT
		 * versus ORDER BY is much easier, since we know that the parser
		 * ensured that one is a superset of the other.
		 */
		if (root->group_pathkeys)
			root->query_pathkeys = root->group_pathkeys;
		else if (list_length(root->distinct_pathkeys) >
				 list_length(root->sort_pathkeys))
			root->query_pathkeys = root->distinct_pathkeys;
		else if (root->sort_pathkeys)
			root->query_pathkeys = root->sort_pathkeys;
		else
			root->query_pathkeys = NIL;

		/*
		 * Generate the best unsorted and presorted paths for this Query (but
		 * note there may not be any presorted path).  query_planner will also
		 * estimate the number of groups in the query, and canonicalize all
		 * the pathkeys.
		 */
		query_planner(root, sub_tlist, tuple_fraction, limit_tuples,
					  &cheapest_path, &sorted_path, &dNumGroups);

		/*
		 * If grouping, decide whether to use sorted or hashed grouping.
		 */
		if (parse->groupClause)
		{
			bool	can_hash;
			bool	can_sort;

			/*
			 * Executor doesn't support hashed aggregation with DISTINCT
			 * aggregates.  (Doing so would imply storing *all* the input
			 * values in the hash table, which seems like a certain loser.)
			 */
			can_hash = (agg_counts.numDistinctAggs == 0 &&
						grouping_is_hashable(parse->groupClause));
			can_sort = grouping_is_sortable(parse->groupClause);
			if (can_hash && can_sort)
			{
				/* we have a meaningful choice to make ... */
				use_hashed_grouping =
					choose_hashed_grouping(root,
										   tuple_fraction, limit_tuples,
										   cheapest_path, sorted_path,
										   numGroupCols,
										   dNumGroups, &agg_counts);
			}
			else if (can_hash)
				use_hashed_grouping = true;
			else if (can_sort)
				use_hashed_grouping = false;
			else
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("could not implement GROUP BY"),
						 errdetail("Some of the datatypes only support hashing, while others only support sorting.")));

			/* Also convert # groups to long int --- but 'ware overflow! */
			numGroups = (long) Min(dNumGroups, (double) LONG_MAX);
		}

		/*
		 * Select the best path.  If we are doing hashed grouping, we will
		 * always read all the input tuples, so use the cheapest-total path.
		 * Otherwise, trust query_planner's decision about which to use.
		 */
		if (use_hashed_grouping || !sorted_path)
			best_path = cheapest_path;
		else
			best_path = sorted_path;

		/*
		 * CDB:  For now, we either - construct a general parallel plan, - let
		 * the sequential planner handle the situation, or - construct a
		 * sequential plan using the mix-max index optimization.
		 *
		 * Eventually we should add a parallel version of the min-max
		 * optimization.  For now, it's either-or.
		 */
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			bool		querynode_changed = false;
			bool		pass_subtlist = false;
			GroupContext group_context;

			pass_subtlist = (agg_counts.aggOrder != NIL || has_within);
			group_context.best_path = best_path;
			group_context.cheapest_path = cheapest_path;
			group_context.subplan = NULL;
			group_context.sub_tlist = pass_subtlist ? sub_tlist : NIL;
			group_context.tlist = tlist;
			group_context.use_hashed_grouping = use_hashed_grouping;
			group_context.tuple_fraction = tuple_fraction;
			group_context.canonical_grpsets = canonical_grpsets;
			group_context.grouping = 0;
			group_context.numGroupCols = 0;
			group_context.groupColIdx = NULL;
			group_context.groupOperators = NULL;
			group_context.numDistinctCols = 0;
			group_context.distinctColIdx = NULL;
			group_context.p_dNumGroups = &dNumGroups;
			group_context.pcurrent_pathkeys = &current_pathkeys;
			group_context.querynode_changed = &querynode_changed;

			/* within_agg_planner calls cdb_grouping_planner */
			if (has_within)
				result_plan = within_agg_planner(root,
												 &agg_counts,
												 &group_context);
			else
				result_plan = cdb_grouping_planner(root,
												   &agg_counts,
												   &group_context);

			/* Add the Repeat node if needed. */
			if (result_plan != NULL &&
				canonical_grpsets != NULL &&
				canonical_grpsets->grpset_counts != NULL)
			{
				bool		need_repeat_node = false;
				int			grpset_no;
				int			repeat_count = 0;

				for (grpset_no = 0; grpset_no < canonical_grpsets->ngrpsets; grpset_no++)
				{
					if (canonical_grpsets->grpset_counts[grpset_no] > 1)
					{
						need_repeat_node = true;
						break;
					}
				}

				if (canonical_grpsets->ngrpsets == 1)
					repeat_count = canonical_grpsets->grpset_counts[0];

				if (need_repeat_node)
				{
					result_plan = add_repeat_node(result_plan, repeat_count, 0);
				}
			}

			if (result_plan != NULL && querynode_changed)
			{
				/*
				 * We want to re-write sort_pathkeys here since the 2-stage
				 * aggregation subplan or grouping extension subplan may
				 * change the previous root->parse Query node, which makes the
				 * current sort_pathkeys invalid.
				 */
				if (parse->distinctClause)
					root->distinct_pathkeys =
						make_pathkeys_for_sortclauses(root,
													  parse->distinctClause,
													  result_plan->targetlist,
													  true);
				if (parse->sortClause)
					root->sort_pathkeys =
						make_pathkeys_for_sortclauses(root,
													  parse->sortClause,
													  result_plan->targetlist,
													  true);
			}
		}
		else	/* Not GP_ROLE_DISPATCH */
		{
			/*
			 * Check to see if it's possible to optimize MIN/MAX aggregates.
			 * If so, we will forget all the work we did so far to choose a
			 * "regular" path ... but we had to do it anyway to be able to
			 * tell which way is cheaper.
			 */
			result_plan = optimize_minmax_aggregates(root,
													 tlist,
													 best_path);
			if (result_plan != NULL)
			{
				/*
				 * optimize_minmax_aggregates generated the full plan, with
				 * the right tlist, and it has no sort order.
				 */
				current_pathkeys = NIL;
				mark_plan_entry(result_plan);
			}

		}

		if (result_plan == NULL)
		{
			/*
			 * Normal case --- create a plan according to query_planner's
			 * results.
			 */
			bool	need_sort_for_grouping = false;

			result_plan = create_plan(root, best_path);
			current_pathkeys = best_path->pathkeys;
			current_locus = best_path->locus;	/* just use keys, don't copy */

			/* Detect if we'll need an explicit sort for grouping */
			if (parse->groupClause && !use_hashed_grouping &&
				!pathkeys_contained_in(root->group_pathkeys, current_pathkeys))
			{
				need_sort_for_grouping = true;

				/*
				 * Always override query_planner's tlist, so that we don't
				 * sort useless data from a "physical" tlist.
				 */
				need_tlist_eval = true;
			}

			/*
			 * create_plan() returns a plan with just a "flat" tlist of
			 * required Vars.  Usually we need to insert the sub_tlist as the
			 * tlist of the top plan node.	However, we can skip that if we
			 * determined that whatever query_planner chose to return will be
			 * good enough.
			 */
			if (need_tlist_eval)
			{
				/*
				 * If the top-level plan node is one that cannot do expression
				 * evaluation, we must insert a Result node to project the
				 * desired tlist.
				 */
				result_plan = plan_pushdown_tlist(root, result_plan, sub_tlist);

				/*
				 * Also, account for the cost of evaluation of the sub_tlist.
				 *
				 * Up to now, we have only been dealing with "flat" tlists,
				 * containing just Vars.  So their evaluation cost is zero
				 * according to the model used by cost_qual_eval() (or if you
				 * prefer, the cost is factored into cpu_tuple_cost).  Thus we
				 * can avoid accounting for tlist cost throughout
				 * query_planner() and subroutines.  But now we've inserted a
				 * tlist that might contain actual operators, sub-selects, etc
				 * --- so we'd better account for its cost.
				 *
				 * Below this point, any tlist eval cost for added-on nodes
				 * should be accounted for as we create those nodes.
				 * Presently, of the node types we can add on, only Agg and
				 * Group project new tlists (the rest just copy their input
				 * tuples) --- so make_agg() and make_group() are responsible
				 * for computing the added cost.
				 */
				cost_qual_eval(&tlist_cost, sub_tlist, root);
				result_plan->startup_cost += tlist_cost.startup;
				result_plan->total_cost += tlist_cost.startup +
					tlist_cost.per_tuple * result_plan->plan_rows;
			}
			else
			{
				/*
				 * Since we're using query_planner's tlist and not the one
				 * make_subplanTargetList calculated, we have to refigure any
				 * grouping-column indexes make_subplanTargetList computed.
				 */
				locate_grouping_columns(root, tlist, result_plan->targetlist,
										groupColIdx);
			}

			Assert(result_plan->flow);

			/*
			 * Insert AGG or GROUP node if needed, plus an explicit sort step
			 * if necessary.
			 *
			 * HAVING clause, if any, becomes qual of the Agg or Group node.
			 */
			if (!grpext && use_hashed_grouping)
			{
				/* Hashed aggregate plan --- no sort needed */
				result_plan = (Plan *) make_agg(root,
												tlist,
												(List *) parse->havingQual,
												AGG_HASHED, false,
												numGroupCols,
												groupColIdx,
												groupOperators,
												numGroups,
												/* GPDB_84_MERGE_FIXME: What would be
												 * appropriate values for these extra
												 * arguments? */
												0, /* num_nullcols */
												0, /* input_grouping */
												0, /* grouping */
												0, /* rollup_gs_times */
												agg_counts.numAggs,
												agg_counts.transitionSpace,
												result_plan);

				if (canonical_grpsets != NULL &&
					canonical_grpsets->grpset_counts != NULL &&
					canonical_grpsets->grpset_counts[0] > 1)
				{
					result_plan->flow = pull_up_Flow(result_plan, result_plan->lefttree);
					result_plan = add_repeat_node(result_plan,
										 canonical_grpsets->grpset_counts[0],
												  0);
				}

				/* Hashed aggregation produces randomly-ordered results */
				current_pathkeys = NIL;
				CdbPathLocus_MakeNull(&current_locus);
			}
			else if (!grpext && (parse->hasAggs || parse->groupClause))
			{
				/* Plain aggregate plan --- sort if needed */
				AggStrategy aggstrategy;

				if (parse->groupClause)
				{
					if (need_sort_for_grouping)
					{
						result_plan = (Plan *)
							make_sort_from_groupcols(root,
													 parse->groupClause,
													 groupColIdx,
													 false,
													 result_plan);
						current_pathkeys = root->group_pathkeys;

						/* Decorate the Sort node with a Flow node. */
						mark_sort_locus(result_plan);
					}
					aggstrategy = AGG_SORTED;

					/*
					 * The AGG node will not change the sort ordering of its
					 * groups, so current_pathkeys describes the result too.
					 */
				}
				else
				{
					aggstrategy = AGG_PLAIN;
					/* Result will be only one row anyway; no sort order */
					current_pathkeys = NIL;
				}

				/*
				 * We make a single Agg node if this is not a grouping extension.
				 */
				result_plan = (Plan *) make_agg(root,
												tlist,
												(List *) parse->havingQual,
												aggstrategy, false,
												numGroupCols,
												groupColIdx,
												groupOperators,
												numGroups,
												0, /* num_nullcols */
												0, /* input_grouping */
												0, /* grouping */
												0, /* rollup_gs_times */
												agg_counts.numAggs,
												agg_counts.transitionSpace,
												result_plan);

				if (canonical_grpsets != NULL &&
					canonical_grpsets->grpset_counts != NULL &&
					canonical_grpsets->grpset_counts[0] > 1)
				{
					result_plan->flow = pull_up_Flow(result_plan, result_plan->lefttree);
					result_plan = add_repeat_node(result_plan,
										 canonical_grpsets->grpset_counts[0],
												  0);
				}

				CdbPathLocus_MakeNull(&current_locus);
			}
			else if (grpext && (parse->hasAggs || parse->groupClause))
			{
				/* Plan the grouping extension */
				ListCell   *lc;
				bool		querynode_changed = false;

				/*
				 * Make a copy of tlist. Really need to?
				 */
				List	   *new_tlist = copyObject(tlist);

				/* Make EXPLAIN output look nice */
				foreach(lc, result_plan->targetlist)
				{
					TargetEntry *tle = (TargetEntry *) lfirst(lc);

					if (IsA(tle->expr, Var) &&tle->resname == NULL)
					{
						TargetEntry *vartle = tlist_member((Node *) tle->expr, tlist);

						if (vartle != NULL && vartle->resname != NULL)
							tle->resname = pstrdup(vartle->resname);
					}
				}

				result_plan = plan_grouping_extension(root, best_path, tuple_fraction,
													  use_hashed_grouping,
													  &new_tlist, result_plan->targetlist,
													  true, false,
													  (List *) parse->havingQual,
													  &numGroupCols,
													  &groupColIdx,
													  &groupOperators,
													  &agg_counts,
													  canonical_grpsets,
													  &dNumGroups,
													  &querynode_changed,
													  &current_pathkeys,
													  result_plan);
				if (querynode_changed)
				{
					/*
					 * We want to re-write sort_pathkeys here since the
					 * 2-stage aggregation subplan or grouping extension
					 * subplan may change the previous root->parse Query node,
					 * which makes the current sort_pathkeys invalid.
					 */
					if (parse->distinctClause &&
						grouping_is_sortable(parse->distinctClause))
						root->distinct_pathkeys =
							make_pathkeys_for_sortclauses(root,
														  parse->distinctClause,
														  result_plan->targetlist,
														  true);
					if (parse->sortClause)
						root->sort_pathkeys =
							make_pathkeys_for_sortclauses(root,
														  parse->sortClause,
														  result_plan->targetlist,
														  true);
					CdbPathLocus_MakeNull(&current_locus);
				}
			}
			else if (root->hasHavingQual)
			{
				/*
				 * No aggregates, and no GROUP BY, but we have a HAVING qual.
				 * This is a degenerate case in which we are supposed to emit
				 * either 0 or 1 row depending on whether HAVING succeeds.
				 * Furthermore, there cannot be any variables in either HAVING
				 * or the targetlist, so we actually do not need the FROM
				 * table at all!  We can just throw away the plan-so-far and
				 * generate a Result node.	This is a sufficiently unusual
				 * corner case that it's not worth contorting the structure of
				 * this routine to avoid having to generate the plan in the
				 * first place.
				 */
				result_plan = (Plan *) make_result(root,
												   tlist,
												   parse->havingQual,
												   NULL);
				/* Result will be only one row anyway; no sort order */
				current_pathkeys = NIL;
				mark_plan_general(result_plan);
				CdbPathLocus_MakeNull(&current_locus);
			}
		}						/* end of non-minmax-aggregate case */

		/* free canonical_grpsets */
		free_canonical_groupingsets(canonical_grpsets);
	}							/* end of if (setOperations) */

	/*
	 * Decorate the top node with a Flow node if it doesn't have one yet. (In
	 * such cases we require the next-to-top node to have a Flow node from
	 * which we can obtain the distribution info.)
	 */
	if (!result_plan->flow)
		result_plan->flow = pull_up_Flow(result_plan, getAnySubplan(result_plan));

	/*
	 * An ORDER BY or DISTINCT doesn't make much sense, unless we bring all
	 * the data to a single node. Otherwise it's just a partial order. (If
	 * there's a LIMIT or OFFSET clause, we'll take care of this below, after
	 * inserting the Limit node).
	 *
	 * In a subquery, though, a partial order is OK. In fact, we could
	 * probably not bother with the sort at all, unless there's a LIMIT or
	 * OFFSET, because it's not going to make any difference to the overall
	 * query's result. For example, in "WHERE x IN (SELECT ...  ORDER BY
	 * foo)", the ORDER BY in the subquery will make no difference. PostgreSQL
	 * honors the sort, though, and historically, GPDB has also done a partial
	 * sort, separately on each node. So keep that behavior for now.
	 *
	 * A SELECT INTO or CREATE TABLE AS is similar to a subquery: the order
	 * doesn't really matter, but let's keep the partial order anyway.
	 *
	 * In a TABLE function's input subquery, a partial order is the documented
	 * behavior, so in that case that's definitely what we want.
	 */
	if ((parse->distinctClause || parse->sortClause) &&
		result_plan->flow->flotype != FLOW_SINGLETON &&
		(root->config->honor_order_by || !root->parent_root) &&
		!parse->intoClause &&
		/*
		 * GPDB_84_MERGE_FIXME: Does this do the right thing, if you have a
		 * SELECT DISTINCT query as argument to a table function?
		 */
		!parse->isTableValueSelect &&
		!parse->limitCount && !parse->limitOffset)
	{
		must_gather = true;
	}
	else
		must_gather = false;

	/*
	 * If there is a DISTINCT clause, add the necessary node(s).
	 */
	if (parse->distinctClause)
	{
		double	dNumDistinctRows;
		long	numDistinctRows;
		bool	use_hashed_distinct;
		bool	can_sort;
		bool	can_hash;

		/*
		 * If there was grouping or aggregation, use the current number of
		 * rows as the estimated number of DISTINCT rows (ie, assume the
		 * result was already mostly unique).  If not, use the number of
		 * distinct-groups calculated by query_planner.
		 */
		if (parse->groupClause || root->hasHavingQual || parse->hasAggs)
			dNumDistinctRows = result_plan->plan_rows;
		else
			dNumDistinctRows = dNumGroups;

		/* Also convert to long int --- but 'ware overflow! */
		numDistinctRows = (long) Min(dNumDistinctRows, (double) LONG_MAX);

		/*
		 * If we have a sortable DISTINCT ON clause, we always use sorting.
		 * This enforces the expected behavior of DISTINCT ON.
		 */
		can_sort = grouping_is_sortable(parse->distinctClause);
		if (can_sort && parse->hasDistinctOn)
			use_hashed_distinct = false;
		else
		{
			can_hash = grouping_is_hashable(parse->distinctClause);

			/* GPDB_84_MERGE_FIXME: The hash Agg we build for DISTINCT currently
			 * loses the GROUP_ID() information, so don't use it if there's a
			 * GROUP_ID().
			 */
			if (can_hash && contain_group_id((Node *) result_plan->targetlist))
				can_hash = false;

			if (can_hash && can_sort)
			{
				/* we have a meaningful choice to make ... */
				use_hashed_distinct =
					choose_hashed_distinct(root,
										   result_plan, current_pathkeys,
										   tuple_fraction, limit_tuples,
										   dNumDistinctRows);
			}
			else if (can_hash)
				use_hashed_distinct = true;
			else if (can_sort)
				use_hashed_distinct = false;
			else
			{
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("could not implement DISTINCT"),
						 errdetail("Some of the datatypes only support hashing, while others only support sorting.")));
				use_hashed_distinct = false;	/* keep compiler quiet */
			}
		}

		/*
		 * MPP: If there's a DISTINCT clause and we're not collocated on the
		 * distinct key, we need to redistribute on that key.  In addition, we
		 * need to consider whether to "pre-unique" by doing a Sort-Unique
		 * operation on the data as currently distributed, redistributing on the
		 * district key, and doing the Sort-Unique again. This 2-phase approach
		 * will be a win, if the cost of redistributing the entire input exceeds
		 * the cost of an extra Redistribute-Sort-Unique on the pre-uniqued
		 * (reduced) input.
		 */
		distinctExprs = get_sortgrouplist_exprs(parse->distinctClause,
												result_plan->targetlist);
		numDistinct = estimate_num_groups(root, distinctExprs,
										  result_plan->plan_rows);

		if (CdbPathLocus_IsNull(current_locus))
		{
			current_locus = cdbpathlocus_from_flow(result_plan->flow);
		}

		if (Gp_role == GP_ROLE_DISPATCH && CdbPathLocus_IsPartitioned(current_locus))
		{
			bool		needMotion = !cdbpathlocus_collocates(root, current_locus,
															  root->distinct_pathkeys, false /* exact_match */ );

			/* Apply the preunique optimization, if enabled and worthwhile. */
			/* GPDB_84_MERGE_FIXME: pre-unique for hash distinct not implemented. */
			if (root->config->gp_enable_preunique && needMotion && !use_hashed_distinct)
			{
				double		base_cost,
							alt_cost;
				Path		sort_path;	/* dummy for result of cost_sort */

				base_cost = motion_cost_per_row * result_plan->plan_rows;
				alt_cost = motion_cost_per_row * numDistinct;
				cost_sort(&sort_path, root, NIL, alt_cost,
						  numDistinct, result_plan->plan_rows, -1.0);
				alt_cost += sort_path.startup_cost;
				alt_cost += cpu_operator_cost * numDistinct
					* list_length(parse->distinctClause);

				if (alt_cost < base_cost || root->config->gp_eager_preunique)
				{
					/*
					 * Reduce the number of rows to move by adding a [Sort
					 * and] Unique prior to the redistribute Motion.
					 */
					if (root->sort_pathkeys)
					{
						if (!pathkeys_contained_in(root->sort_pathkeys, current_pathkeys))
						{
							result_plan = (Plan *) make_sort_from_pathkeys(root,
																		   result_plan,
																		   root->sort_pathkeys,
																		   limit_tuples,
																		   true);
							((Sort *) result_plan)->noduplicates = gp_enable_sort_distinct;
							current_pathkeys = root->sort_pathkeys;
							mark_sort_locus(result_plan);
						}
					}

					result_plan = (Plan *) make_unique(result_plan, parse->distinctClause);

					result_plan->flow = pull_up_Flow(result_plan, result_plan->lefttree);

					result_plan->plan_rows = numDistinct;

					/*
					 * Our sort node (under the unique node), unfortunately
					 * can't guarantee uniqueness -- so we aren't allowed to
					 * push the limit into the sort; but we can avoid moving
					 * the entire sorted result-set by plunking a limit on the
					 * top of the unique-node.
					 */
					if (parse->limitCount)
					{
						/*
						 * Our extra limit operation is basically a
						 * third-phase on multi-phase limit (see 2-phase limit
						 * below)
						 */
						result_plan = pushdown_preliminary_limit(result_plan, parse->limitCount, count_est, parse->limitOffset, offset_est);
					}
				}
			}

			if (needMotion)
			{
				result_plan = (Plan *) make_motion_hash(root, result_plan, distinctExprs);
				result_plan->total_cost += motion_cost_per_row * result_plan->plan_rows;
				current_pathkeys = NIL;		/* Any pre-existing order now lost. */
			}
		}
		else if ( result_plan->flow->flotype == FLOW_SINGLETON )
			; /* Already collocated. */
		else
		{
			ereport(ERROR, (errcode(ERRCODE_CDB_INTERNAL_ERROR),
							errmsg("unexpected input locus to distinct")));
		}

		if (use_hashed_distinct)
		{
			/* Hashed aggregate plan --- no sort needed */

			result_plan = (Plan *) make_agg(root,
											result_plan->targetlist,
											NIL,
											AGG_HASHED,
											false, /* streaming */
										  list_length(parse->distinctClause),
								 extract_grouping_cols(parse->distinctClause,
													result_plan->targetlist),
								 extract_grouping_ops(parse->distinctClause),
											numDistinctRows,
											0, /* num_nullcols */
											0, /* input_grouping */
											0, /* grouping */
											0, /* rollupGSTimes */
											0,
											0, /* transSpace */
											result_plan);
			/* Hashed aggregation produces randomly-ordered results */
			current_pathkeys = NIL;
		}
		else
		{
			/*
			 * Use a Unique node to implement DISTINCT.  Add an explicit sort
			 * if we couldn't make the path come out the way the Unique node
			 * needs it.  If we do have to sort, always sort by the more
			 * rigorous of DISTINCT and ORDER BY, to avoid a second sort
			 * below.  However, for regular DISTINCT, don't sort now if we
			 * don't have to --- sorting afterwards will likely be cheaper,
			 * and also has the possibility of optimizing via LIMIT.  But for
			 * DISTINCT ON, we *must* force the final sort now, else it won't
			 * have the desired behavior.
			 */
			List	   *needed_pathkeys;

			if (parse->hasDistinctOn &&
				list_length(root->distinct_pathkeys) <
				list_length(root->sort_pathkeys))
				needed_pathkeys = root->sort_pathkeys;
			else
				needed_pathkeys = root->distinct_pathkeys;

			if (!pathkeys_contained_in(needed_pathkeys, current_pathkeys))
			{
				if (list_length(root->distinct_pathkeys) >=
					list_length(root->sort_pathkeys))
					current_pathkeys = root->distinct_pathkeys;
				else
				{
					current_pathkeys = root->sort_pathkeys;
					/* Assert checks that parser didn't mess up... */
					Assert(pathkeys_contained_in(root->distinct_pathkeys,
												 current_pathkeys));
				}

				result_plan = (Plan *) make_sort_from_pathkeys(root,
															   result_plan,
															current_pathkeys,
															   -1.0,
															   true);
				mark_sort_locus(result_plan);
			}

			if (must_gather)
			{
				/*
				 * As an optimization, eliminate any duplicates within the segment,
				 * before the motion.
				 */
				if (IsA(result_plan, Sort) &&gp_enable_sort_distinct)
					((Sort *) result_plan)->noduplicates = true;
				result_plan = (Plan *) make_unique(result_plan, parse->distinctClause);
				result_plan->flow = pull_up_Flow(result_plan, result_plan->lefttree);

				result_plan = (Plan *) make_motion_gather(root, result_plan, -1,
														  current_pathkeys);
				must_gather = false;
			}

			result_plan = (Plan *) make_unique(result_plan,
											   parse->distinctClause);
			result_plan->plan_rows = dNumDistinctRows;
			/* The Unique node won't change sort ordering */
		}
		result_plan->flow = pull_up_Flow(result_plan, result_plan->lefttree);
	}

	/*
	 * If ORDER BY was given and we were not able to make the plan come out in
	 * the right order, add an explicit sort step.
	 */
	if (parse->sortClause)
	{
		if (!pathkeys_contained_in(root->sort_pathkeys, current_pathkeys))
		{
			result_plan = (Plan *) make_sort_from_pathkeys(root,
														   result_plan,
														   root->sort_pathkeys,
														   limit_tuples,
														   true);
			mark_sort_locus(result_plan);
			current_pathkeys = root->sort_pathkeys;
			result_plan->flow = pull_up_Flow(result_plan, result_plan->lefttree);
		}

		if (must_gather)
		{
			/*
			 * current_pathkeys might contain unneeded columns that have been
			 * eliminated from the final target list, and we cannot maintain
			 * such an order in the Motion anymore. So use root->sortpathkeys
			 * rather than current_pathkeys here. (See similar case in LIMIT
			 * handling below.
			 */
			current_pathkeys = root->sort_pathkeys;
			result_plan = (Plan *) make_motion_gather(root, result_plan, -1,
													  current_pathkeys);
			must_gather = false;
		}
	}

	/*
	 * Finally, if there is a LIMIT/OFFSET clause, add the LIMIT node.
	 */
	if (parse->limitCount || parse->limitOffset)
	{
		if (Gp_role == GP_ROLE_DISPATCH && result_plan->flow->flotype == FLOW_PARTITIONED)
		{
			/* pushdown the first phase of multi-phase limit (which takes offset into account) */
			result_plan = pushdown_preliminary_limit(result_plan, parse->limitCount, count_est, parse->limitOffset, offset_est);
			
			/*
			 * Focus on QE [merge to preserve order], prior to final LIMIT.
			 *
			 * If there is an ORDER BY, the input should be in the required
			 * order now, and we must preserve the order in the merge. But if
			 * there is no ORDER BY, don't try to maintain the current input
			 * order. In that case, current_pathkeys might contain unneeded
			 * columns that have been eliminated from the final target list,
			 * and we cannot maintain such an order in the Motion anymore.
			 */
			if (parse->sortClause)
			{
				if (!pathkeys_contained_in(root->sort_pathkeys, current_pathkeys))
					elog(ERROR, "invalid result order generated for ORDER BY + LIMIT");
				current_pathkeys = root->sort_pathkeys;
				result_plan = (Plan *) make_motion_gather_to_QE(root, result_plan,
																current_pathkeys);
			}
			else
				result_plan = (Plan *) make_motion_gather_to_QE(root, result_plan, NIL);
			result_plan->total_cost += motion_cost_per_row * result_plan->plan_rows;
		}

		if (current_pathkeys == NIL)
		{
			/* This used to be a WARNING.  If reinstated, it should be a NOTICE
			 * and steps taken to avoid issuing it at inopportune times, e.g.,
			 * from the query generated by psql tab-completion.
			 */
			ereport(DEBUG1, (errmsg("LIMIT/OFFSET applied to unordered result.") ));
		}

		/* For multi-phase limit, this is the final limit */
		result_plan = (Plan *) make_limit(result_plan,
										  parse->limitOffset,
										  parse->limitCount,
										  offset_est,
										  count_est);
		result_plan->flow = pull_up_Flow(result_plan, result_plan->lefttree);
	}

	/*
	 * Deal with explicit redistribution requirements for TableValueExpr
	 * subplans with explicit distribitution
	 */
	if (parse->scatterClause)
	{
		bool		r;
		List	   *exprList;

		/* Deal with the special case of SCATTER RANDOMLY */
		if (list_length(parse->scatterClause) == 1 && linitial(parse->scatterClause) == NULL)
			exprList = NIL;
		else
			exprList = parse->scatterClause;

		/*
		 * Repartition the subquery plan based on our distribution
		 * requirements
		 */
		r = repartitionPlan(result_plan, false, false, exprList);
		if (!r)
		{
			/*
			 * This should not be possible, repartitionPlan should never fail
			 * when both stable and rescannable are false.
			 */
			elog(ERROR, "failure repartitioning plan");
		}
	}

	Insist(result_plan->flow);

	/*
	 * Deal with the RETURNING clause if any.  It's convenient to pass the
	 * returningList through setrefs.c now rather than at top level (if we
	 * waited, handling inherited UPDATE/DELETE would be much harder).
	 */
	if (parse->returningList)
	{
		List	   *rlist;

		Assert(parse->resultRelation);
		rlist = set_returning_clause_references(root->glob,
												parse->returningList,
												result_plan,
												parse->resultRelation);
		root->returningLists = list_make1(rlist);
	}
	else
		root->returningLists = NIL;

	/* Compute result-relations list if needed */
	if (parse->resultRelation)
		root->resultRelations = list_make1_int(parse->resultRelation);
	else
		root->resultRelations = NIL;

	/*
	 * Return the actual output ordering in query_pathkeys for possible use by
	 * an outer query level.
	 */
	root->query_pathkeys = current_pathkeys;

#ifdef USE_ASSERT_CHECKING
	grouping_planner_output_asserts(root, result_plan);
#endif

	return result_plan;
}

/*
 * Entry is through is_dummy_plan().
 *
 * Detect whether a plan node is a "dummy" plan created when a relation
 * is deemed not to need scanning due to constraint exclusion.
 *
 * At bottom, such dummy plans are Result nodes with constant FALSE
 * filter quals.  However, we also recognize simple plans that are
 * known to return no rows because they contain a dummy.
 *
 * BTW The plan_tree_walker framework is overkill here, but it's good to 
 *     do things the standard way.
 */
static bool
is_dummy_plan_walker(Node *node, bool *context)
{
	/*
	 * We are only interested in Plan nodes.
	 */
	if (node == NULL || !is_plan_node(node))
		return false;

	switch (nodeTag(node))
	{
		case T_Result:

			/*
			 * This tests the base case of a dummy plan which is a Result node
			 * with a constant FALSE filter quals.  (This is the case
			 * constructed as an empty Append path by set_plain_rel_pathlist
			 * in allpaths.c and made into a Result plan by create_append_plan
			 * in createplan.c.
			 */
			{
				List	   *rcqual = (List *) ((Result *) node)->resconstantqual;

				if (list_length(rcqual) == 1)
				{
					Const	   *constqual = (Const *) linitial(rcqual);

					if (constqual && IsA(constqual, Const))
					{
						if (!constqual->constisnull &&
							!DatumGetBool(constqual->constvalue))
							*context = true;
						return true;
					}
				}
			}
			return false;

		case T_SubqueryScan:

			/*
			 * A SubqueryScan is dummy, if its subplan is dummy.
			 */
			{
				SubqueryScan *subqueryscan = (SubqueryScan *) node;
				Plan	   *subplan = subqueryscan->subplan;

				if (is_dummy_plan(subplan))
				{
					*context = true;
					return true;
				}
			}
			return false;

		case T_NestLoop:
		case T_MergeJoin:
		case T_HashJoin:

			/*
			 * Joins with dummy inner and/or outer plans are dummy or not
			 * based on the type of join.
			 */
			{
				switch (((Join *) node)->jointype)
				{
					case JOIN_INNER:	/* either */
						*context = is_dummy_plan(innerPlan(node))
							|| is_dummy_plan(outerPlan(node));
						break;

					case JOIN_LEFT:
					case JOIN_FULL:
					case JOIN_RIGHT:	/* both */
						*context = is_dummy_plan(innerPlan(node))
							&& is_dummy_plan(outerPlan(node));
						break;

					case JOIN_SEMI:
					case JOIN_LASJ_NOTIN:
					case JOIN_ANTI:		/* outer */
						*context = is_dummy_plan(outerPlan(node));
						break;

					default:
						break;
				}

				return true;
			}

			/*
			 * It may seem that we should check for Append or SetOp nodes with
			 * all dummy branches, but that case should not occur.  It would
			 * cause big problems elsewhere in the code.
			 */

		case T_Hash:
		case T_Material:
		case T_Sort:
		case T_Unique:

			/*
			 * Some node types are dummy, if their outer plan is dummy so we
			 * just recur.
			 *
			 * We don't include "tricky" nodes like Motion that might affect
			 * plan topology, even though we know they will return no rows
			 * from a dummy.
			 */
			return plan_tree_walker(node, is_dummy_plan_walker, context);

		default:

			/*
			 * Other node types are "opaque" so we choose a conservative
			 * course and terminate the walk.
			 */
			return true;
	}
	/* not reached */
}


static bool
is_dummy_plan(Plan *plan)
{
	bool		is_dummy = false;

	is_dummy_plan_walker((Node *) plan, &is_dummy);

	return is_dummy;
}

/*
 * preprocess_limit - do pre-estimation for LIMIT and/or OFFSET clauses
 *
 * We try to estimate the values of the LIMIT/OFFSET clauses, and pass the
 * results back in *count_est and *offset_est.	These variables are set to
 * 0 if the corresponding clause is not present, and -1 if it's present
 * but we couldn't estimate the value for it.  (The "0" convention is OK
 * for OFFSET but a little bit bogus for LIMIT: effectively we estimate
 * LIMIT 0 as though it were LIMIT 1.  But this is in line with the planner's
 * usual practice of never estimating less than one row.)  These values will
 * be passed to make_limit, which see if you change this code.
 *
 * The return value is the suitably adjusted tuple_fraction to use for
 * planning the query.	This adjustment is not overridable, since it reflects
 * plan actions that grouping_planner() will certainly take, not assumptions
 * about context.
 */
static double
preprocess_limit(PlannerInfo *root, double tuple_fraction,
				 int64 *offset_est, int64 *count_est)
{
	Query	   *parse = root->parse;
	Node	   *est;
	double		limit_fraction;

	/* Should not be called unless LIMIT or OFFSET */
	Assert(parse->limitCount || parse->limitOffset);

	/*
	 * Try to obtain the clause values.  We use estimate_expression_value
	 * primarily because it can sometimes do something useful with Params.
	 */
	if (parse->limitCount)
	{
		est = estimate_expression_value(root, parse->limitCount);
		if (est && IsA(est, Const))
		{
			if (((Const *) est)->constisnull)
			{
				/* NULL indicates LIMIT ALL, ie, no limit */
				*count_est = 0; /* treat as not present */
			}
			else
			{
				if (((Const *) est)->consttype == INT4OID)
					*count_est = DatumGetInt32(((Const *) est)->constvalue);
				else
					*count_est = DatumGetInt64(((Const *) est)->constvalue);
				if (*count_est <= 0)
					*count_est = 1;		/* force to at least 1 */
			}
		}
		else
			*count_est = -1;	/* can't estimate */
	}
	else
		*count_est = 0;			/* not present */

	if (parse->limitOffset)
	{
		est = estimate_expression_value(root, parse->limitOffset);
		if (est && IsA(est, Const))
		{
			if (((Const *) est)->constisnull)
			{
				/* Treat NULL as no offset; the executor will too */
				*offset_est = 0;	/* treat as not present */
			}
			else
			{
				if (((Const *) est)->consttype == INT4OID)
					*offset_est = DatumGetInt32(((Const *) est)->constvalue);
				else
					*offset_est = DatumGetInt64(((Const *) est)->constvalue);

				if (*offset_est < 0)
					*offset_est = 0;	/* less than 0 is same as 0 */
			}
		}
		else
			*offset_est = -1;	/* can't estimate */
	}
	else
		*offset_est = 0;		/* not present */

	if (*count_est != 0)
	{
		/*
		 * A LIMIT clause limits the absolute number of tuples returned.
		 * However, if it's not a constant LIMIT then we have to guess; for
		 * lack of a better idea, assume 10% of the plan's result is wanted.
		 */
		if (*count_est < 0 || *offset_est < 0)
		{
			/* LIMIT or OFFSET is an expression ... punt ... */
			limit_fraction = 0.10;
		}
		else
		{
			/* LIMIT (plus OFFSET, if any) is max number of tuples needed */
			limit_fraction = (double) *count_est + (double) *offset_est;
		}

		/*
		 * If we have absolute limits from both caller and LIMIT, use the
		 * smaller value; likewise if they are both fractional.  If one is
		 * fractional and the other absolute, we can't easily determine which
		 * is smaller, but we use the heuristic that the absolute will usually
		 * be smaller.
		 */
		if (tuple_fraction >= 1.0)
		{
			if (limit_fraction >= 1.0)
			{
				/* both absolute */
				tuple_fraction = Min(tuple_fraction, limit_fraction);
			}
			else
			{
				/* caller absolute, limit fractional; use caller's value */
			}
		}
		else if (tuple_fraction > 0.0)
		{
			if (limit_fraction >= 1.0)
			{
				/* caller fractional, limit absolute; use limit */
				tuple_fraction = limit_fraction;
			}
			else
			{
				/* both fractional */
				tuple_fraction = Min(tuple_fraction, limit_fraction);
			}
		}
		else
		{
			/* no info from caller, just use limit */
			tuple_fraction = limit_fraction;
		}
	}
	else if (*offset_est != 0 && tuple_fraction > 0.0)
	{
		/*
		 * We have an OFFSET but no LIMIT.	This acts entirely differently
		 * from the LIMIT case: here, we need to increase rather than decrease
		 * the caller's tuple_fraction, because the OFFSET acts to cause more
		 * tuples to be fetched instead of fewer.  This only matters if we got
		 * a tuple_fraction > 0, however.
		 *
		 * As above, use 10% if OFFSET is present but unestimatable.
		 */
		if (*offset_est < 0)
			limit_fraction = 0.10;
		else
			limit_fraction = (double) *offset_est;

		/*
		 * If we have absolute counts from both caller and OFFSET, add them
		 * together; likewise if they are both fractional.	If one is
		 * fractional and the other absolute, we want to take the larger, and
		 * we heuristically assume that's the fractional one.
		 */
		if (tuple_fraction >= 1.0)
		{
			if (limit_fraction >= 1.0)
			{
				/* both absolute, so add them together */
				tuple_fraction += limit_fraction;
			}
			else
			{
				/* caller absolute, limit fractional; use limit */
				tuple_fraction = limit_fraction;
			}
		}
		else
		{
			if (limit_fraction >= 1.0)
			{
				/* caller fractional, limit absolute; use caller's value */
			}
			else
			{
				/* both fractional, so add them together */
				tuple_fraction += limit_fraction;
				if (tuple_fraction >= 1.0)
					tuple_fraction = 0.0;		/* assume fetch all */
			}
		}
	}

	return tuple_fraction;
}


/*
 * preprocess_groupclause - do preparatory work on GROUP BY clause
 *
 * The idea here is to adjust the ordering of the GROUP BY elements
 * (which in itself is semantically insignificant) to match ORDER BY,
 * thereby allowing a single sort operation to both implement the ORDER BY
 * requirement and set up for a Unique step that implements GROUP BY.
 *
 * In principle it might be interesting to consider other orderings of the
 * GROUP BY elements, which could match the sort ordering of other
 * possible plans (eg an indexscan) and thereby reduce cost.  We don't
 * bother with that, though.  Hashed grouping will frequently win anyway.
 *
 * Note: we need no comparable processing of the distinctClause because
 * the parser already enforced that that matches ORDER BY.
 */
static void
preprocess_groupclause(PlannerInfo *root)
{
	Query	   *parse = root->parse;
	List	   *new_groupclause;
	bool		partial_match;
	ListCell   *sl;
	ListCell   *gl;

	/* If no ORDER BY, nothing useful to do here */
	if (parse->sortClause == NIL)
		return;

	/*
	 * GPDB: The grouping clause might contain grouping sets, not just plain
	 * SortGroupClauses. Give up if we see any. (Yes, we could probably do
	 * better than that, but this will do for now.)
	 */
	foreach(gl, parse->groupClause)
	{
		Node *node = lfirst(gl);

		if (node == NULL || !IsA(node, SortGroupClause))
			return;
	}

	/*
	 * Scan the ORDER BY clause and construct a list of matching GROUP BY
	 * items, but only as far as we can make a matching prefix.
	 *
	 * This code assumes that the sortClause contains no duplicate items.
	 */
	new_groupclause = NIL;
	foreach(sl, parse->sortClause)
	{
		SortGroupClause *sc = (SortGroupClause *) lfirst(sl);

		foreach(gl, parse->groupClause)
		{
			SortGroupClause *gc = (SortGroupClause *) lfirst(gl);

			if (equal(gc, sc))
			{
				new_groupclause = lappend(new_groupclause, gc);
				break;
			}
		}
		if (gl == NULL)
			break;				/* no match, so stop scanning */
	}

	/* Did we match all of the ORDER BY list, or just some of it? */
	partial_match = (sl != NULL);

	/* If no match at all, no point in reordering GROUP BY */
	if (new_groupclause == NIL)
		return;

	/*
	 * Add any remaining GROUP BY items to the new list, but only if we
	 * were able to make a complete match.  In other words, we only
	 * rearrange the GROUP BY list if the result is that one list is a
	 * prefix of the other --- otherwise there's no possibility of a
	 * common sort.  Also, give up if there are any non-sortable GROUP BY
	 * items, since then there's no hope anyway.
	 */
	foreach(gl, parse->groupClause)
	{
		SortGroupClause *gc = (SortGroupClause *) lfirst(gl);

		if (list_member_ptr(new_groupclause, gc))
			continue;			/* it matched an ORDER BY item */
		if (partial_match)
			return;				/* give up, no common sort possible */
		if (!OidIsValid(gc->sortop))
			return;				/* give up, GROUP BY can't be sorted */
		new_groupclause = lappend(new_groupclause, gc);
	}

	/* Success --- install the rearranged GROUP BY list */
	Assert(list_length(parse->groupClause) == list_length(new_groupclause));
	parse->groupClause = new_groupclause;
}

/*
 * choose_hashed_grouping - should we use hashed grouping?
 *
 * Note: this is only applied when both alternatives are actually feasible.
 */
bool
choose_hashed_grouping(PlannerInfo *root,
					   double tuple_fraction, double limit_tuples,
					   Path *cheapest_path, Path *sorted_path,
					   int numGroupOps,
					   double dNumGroups, AggClauseCounts *agg_counts)
{
	int			numGroupCols;
	double		cheapest_path_rows;
	int			cheapest_path_width;
	double		hashentrysize;
	List	   *target_pathkeys;
	List	   *current_pathkeys;
	Path		hashed_p;
	Path		sorted_p;

	HashAggTableSizes hash_info;

	/* Prefer sorting when enable_hashagg is off */
	if (!root->config->enable_hashagg)
		return false;

	/*
	 * CDB: The preliminary function is used to merge transient values during
	 * hash reloading (see execHHashagg.c). So hash agg is not allowed if one
	 * of the aggregates doesn't have its preliminary function.
	 */
	if (agg_counts->missing_prelimfunc)
		return false;

	/*
	 * CDB: The parallel grouping planner cannot use hashed aggregation for
	 * ordered aggregates.
	 */
	if (agg_counts->aggOrder != NIL)
		return false;

	/*
	 * Don't do it if it doesn't look like the hashtable will fit into
	 * work_mem.
	 *
	 * Beware here of the possibility that cheapest_path->parent is NULL. This
	 * could happen if user does something silly like SELECT 'foo' GROUP BY 1;
	 */
	if (cheapest_path->parent)
	{
		cheapest_path_rows = cdbpath_rows(root, cheapest_path);
		cheapest_path_width = cheapest_path->parent->width;
	}
	else
	{
		cheapest_path_rows = 1; /* assume non-set result */
		cheapest_path_width = 100;		/* arbitrary */
	}

	/* Estimate per-hash-entry space at tuple width... */
	hashentrysize = agg_hash_entrywidth(agg_counts->numAggs,
							   sizeof(HeapTupleData) + sizeof(HeapTupleHeaderData) + cheapest_path_width,
							   agg_counts->transitionSpace);

	if (!calcHashAggTableSizes(global_work_mem(root),
							   dNumGroups,
							   hashentrysize,
							   false,
							   &hash_info))
	{
		return false;
	}

	/*
	 * When we have both GROUP BY and DISTINCT, use the more-rigorous of
	 * DISTINCT and ORDER BY as the assumed required output sort order.
	 * This is an oversimplification because the DISTINCT might get
	 * implemented via hashing, but it's not clear that the case is common
	 * enough (or that our estimates are good enough) to justify trying to
	 * solve it exactly.
	 */
	if (list_length(root->distinct_pathkeys) >
		list_length(root->sort_pathkeys))
		target_pathkeys = root->distinct_pathkeys;
	else
		target_pathkeys = root->sort_pathkeys;

	/*
	 * See if the estimated cost is no more than doing it the other way. While
	 * avoiding the need for sorted input is usually a win, the fact that the
	 * output won't be sorted may be a loss; so we need to do an actual cost
	 * comparison.
	 *
	 * We need to consider cheapest_path + hashagg [+ final sort] versus
	 * either cheapest_path [+ sort] + group or agg [+ final sort] or
	 * presorted_path + group or agg [+ final sort] where brackets indicate a
	 * step that may not be needed. We assume query_planner() will have
	 * returned a presorted path only if it's a winner compared to
	 * cheapest_path for this purpose.
	 *
	 * These path variables are dummies that just hold cost fields; we don't
	 * make actual Paths for these steps.
	 */
	numGroupCols = num_distcols_in_grouplist(root->parse->groupClause);
	cost_agg(&hashed_p, root, AGG_HASHED, agg_counts->numAggs,
			 numGroupCols, dNumGroups,
			 cheapest_path->startup_cost, cheapest_path->total_cost,
			 cheapest_path_rows, hash_info.workmem_per_entry,
			 hash_info.nbatches, hash_info.hashentry_width, false);
	/* Result of hashed agg is always unsorted */
	if (target_pathkeys)
		cost_sort(&hashed_p, root, target_pathkeys, hashed_p.total_cost,
				  dNumGroups, cheapest_path_width, limit_tuples);

	if (sorted_path)
	{
		sorted_p.startup_cost = sorted_path->startup_cost;
		sorted_p.total_cost = sorted_path->total_cost;
		current_pathkeys = sorted_path->pathkeys;
	}
	else
	{
		sorted_p.startup_cost = cheapest_path->startup_cost;
		sorted_p.total_cost = cheapest_path->total_cost;
		current_pathkeys = cheapest_path->pathkeys;
	}
	if (!pathkeys_contained_in(root->group_pathkeys, current_pathkeys))
	{
		cost_sort(&sorted_p, root, root->group_pathkeys, sorted_p.total_cost,
				  cheapest_path_rows, cheapest_path_width, -1.0);
		current_pathkeys = root->group_pathkeys;
	}

	if (root->parse->hasAggs)
		cost_agg(&sorted_p, root, AGG_SORTED, agg_counts->numAggs,
				 numGroupCols, dNumGroups,
				 sorted_p.startup_cost, sorted_p.total_cost,
				 cheapest_path_rows, 0.0, 0.0, 0.0, false);
	else
		cost_group(&sorted_p, root, numGroupCols, dNumGroups,
				   sorted_p.startup_cost, sorted_p.total_cost,
				   cheapest_path_rows);
	/* The Agg or Group node will preserve ordering */
	if (target_pathkeys &&
		!pathkeys_contained_in(target_pathkeys, current_pathkeys))
		cost_sort(&sorted_p, root, target_pathkeys, sorted_p.total_cost,
				  dNumGroups, cheapest_path_width, limit_tuples);

	/*
	 * Now make the decision using the top-level tuple fraction.  First we
	 * have to convert an absolute count (LIMIT) into fractional form.
	 */
	if (tuple_fraction >= 1.0)
		tuple_fraction /= dNumGroups;

	if (!root->config->enable_groupagg)
		return true;

	if (compare_fractional_path_costs(&hashed_p, &sorted_p, 
									  tuple_fraction) < 0)
	{
		/* Hashed is cheaper, so use it */
		return true;
	}
	return false;
}

/*
 * choose_hashed_distinct - should we use hashing for DISTINCT?
 *
 * This is fairly similar to choose_hashed_grouping, but there are enough
 * differences that it doesn't seem worth trying to unify the two functions.
 *
 * But note that making the two choices independently is a bit bogus in
 * itself.  If the two could be combined into a single choice operation
 * it'd probably be better, but that seems far too unwieldy to be practical,
 * especially considering that the combination of GROUP BY and DISTINCT
 * isn't very common in real queries.  By separating them, we are giving
 * extra preference to using a sorting implementation when a common sort key
 * is available ... and that's not necessarily wrong anyway.
 *
 * Note: this is only applied when both alternatives are actually feasible.
 */
static bool
choose_hashed_distinct(PlannerInfo *root,
					   Plan *input_plan, List *input_pathkeys,
					   double tuple_fraction, double limit_tuples,
					   double dNumDistinctRows)
{
	int			numDistinctCols = list_length(root->parse->distinctClause);
	Size		hashentrysize;
	List	   *current_pathkeys;
	List	   *needed_pathkeys;
	Path		hashed_p;
	Path		sorted_p;

	/* Prefer sorting when enable_hashagg is off */
	if (!enable_hashagg)
		return false;

	/*
	 * Don't do it if it doesn't look like the hashtable will fit into
	 * work_mem.
	 */
	hashentrysize = MAXALIGN(input_plan->plan_width) + MAXALIGN(sizeof(MinimalTupleData));

	if (hashentrysize * dNumDistinctRows > work_mem * 1024L)
		return false;

	/*
	 * See if the estimated cost is no more than doing it the other way. While
	 * avoiding the need for sorted input is usually a win, the fact that the
	 * output won't be sorted may be a loss; so we need to do an actual cost
	 * comparison.
	 *
	 * We need to consider input_plan + hashagg [+ final sort] versus
	 * input_plan [+ sort] + group [+ final sort] where brackets indicate
	 * a step that may not be needed.
	 *
	 * These path variables are dummies that just hold cost fields; we don't
	 * make actual Paths for these steps.
	 */
	cost_agg(&hashed_p, root, AGG_HASHED, 0,
			 numDistinctCols, dNumDistinctRows,
			 input_plan->startup_cost, input_plan->total_cost,
			 input_plan->plan_rows,
			 /* GPDB_84_MERGE_FIXME: What would be appropriate values for these extra
			  * arguments? */
			 0, /* input_width */
			 0, /* hash_batches */
			 0, /* hashentry_width */
			 false /* hash_streaming */);
	/*
	 * Result of hashed agg is always unsorted, so if ORDER BY is present
	 * we need to charge for the final sort.
	 */
	if (root->parse->sortClause)
		cost_sort(&hashed_p, root, root->sort_pathkeys, hashed_p.total_cost,
				  dNumDistinctRows, input_plan->plan_width, limit_tuples);

	/*
	 * Now for the GROUP case.  See comments in grouping_planner about the
	 * sorting choices here --- this code should match that code.
	 */
	sorted_p.startup_cost = input_plan->startup_cost;
	sorted_p.total_cost = input_plan->total_cost;
	current_pathkeys = input_pathkeys;
	if (root->parse->hasDistinctOn &&
		list_length(root->distinct_pathkeys) <
		list_length(root->sort_pathkeys))
		needed_pathkeys = root->sort_pathkeys;
	else
		needed_pathkeys = root->distinct_pathkeys;
	if (!pathkeys_contained_in(needed_pathkeys, current_pathkeys))
	{
		if (list_length(root->distinct_pathkeys) >=
			list_length(root->sort_pathkeys))
			current_pathkeys = root->distinct_pathkeys;
		else
			current_pathkeys = root->sort_pathkeys;
		cost_sort(&sorted_p, root, current_pathkeys, sorted_p.total_cost,
				  input_plan->plan_rows, input_plan->plan_width, -1.0);
	}
	cost_group(&sorted_p, root, numDistinctCols, dNumDistinctRows,
			   sorted_p.startup_cost, sorted_p.total_cost,
			   input_plan->plan_rows);
	if (root->parse->sortClause &&
		!pathkeys_contained_in(root->sort_pathkeys, current_pathkeys))
		cost_sort(&sorted_p, root, root->sort_pathkeys, sorted_p.total_cost,
				  dNumDistinctRows, input_plan->plan_width, limit_tuples);

	/*
	 * Now make the decision using the top-level tuple fraction.  First we
	 * have to convert an absolute count (LIMIT) into fractional form.
	 */
	if (tuple_fraction >= 1.0)
		tuple_fraction /= dNumDistinctRows;

	if (compare_fractional_path_costs(&hashed_p, &sorted_p,
									  tuple_fraction) < 0)
	{
		/* Hashed is cheaper, so use it */
		return true;
	}
	return false;
}

/*---------------
 * make_subplanTargetList
 *	  Generate appropriate target list when grouping is required.
 *
 * When grouping_planner inserts Aggregate, Group, or Result plan nodes
 * above the result of query_planner, we typically want to pass a different
 * target list to query_planner than the outer plan nodes should have.
 * This routine generates the correct target list for the subplan.
 *
 * The initial target list passed from the parser already contains entries
 * for all ORDER BY and GROUP BY expressions, but it will not have entries
 * for variables used only in HAVING clauses; so we need to add those
 * variables to the subplan target list.  Also, we flatten all expressions
 * except GROUP BY items into their component variables; the other expressions
 * will be computed by the inserted nodes rather than by the subplan.
 * For example, given a query like
 *		SELECT a+b,SUM(c+d) FROM table GROUP BY a+b;
 * we want to pass this targetlist to the subplan:
 *		a,b,c,d,a+b
 * where the a+b target will be used by the Sort/Group steps, and the
 * other targets will be used for computing the final results.	(In the
 * above example we could theoretically suppress the a and b targets and
 * pass down only c,d,a+b, but it's not really worth the trouble to
 * eliminate simple var references from the subplan.  We will avoid doing
 * the extra computation to recompute a+b at the outer level; see
 * fix_upper_expr() in setrefs.c.)
 *
 * If we are grouping or aggregating, *and* there are no non-Var grouping
 * expressions, then the returned tlist is effectively dummy; we do not
 * need to force it to be evaluated, because all the Vars it contains
 * should be present in the output of query_planner anyway.
 *
 * 'tlist' is the query's target list.
 * 'groupColIdx' receives an array of column numbers for the GROUP BY
 *			expressions (if there are any) in the subplan's target list.
 * 'groupOperators' receives an array of equality operators corresponding
 *			the GROUP BY expressions.
 * 'need_tlist_eval' is set true if we really need to evaluate the
 *			result tlist.
 *
 * The result is the targetlist to be passed to the subplan.
 *---------------
 */
static List *
make_subplanTargetList(PlannerInfo *root,
					   List *tlist,
					   AttrNumber **groupColIdx,
					   Oid **groupOperators,
					   bool *need_tlist_eval)
{
	Query	   *parse = root->parse;
	List	   *sub_tlist;
	List	   *extravars;
	int			numCols;

	*groupColIdx = NULL;

	/*
	 * If we're not grouping or aggregating, there's nothing to do here;
	 * query_planner should receive the unmodified target list.
	 */
	if (!parse->hasAggs && !parse->groupClause && !root->hasHavingQual)
	{
		*need_tlist_eval = true;
		return tlist;
	}

	/*
	 * Otherwise, start with a "flattened" tlist (having just the vars
	 * mentioned in the targetlist and HAVING qual --- but not upper-level
	 * Vars; they will be replaced by Params later on).
	 */
	sub_tlist = flatten_tlist(tlist);
	extravars = pull_var_clause(parse->havingQual, true);
	sub_tlist = add_to_flat_tlist(sub_tlist, extravars, false /* resjunk */);
	list_free(extravars);

	/*
	 * XXX Set need_tlist_eval to true for group queries.
	 *
	 * Reason: We are doing an aggregate on top.  No matter what we do, hash
	 * or sort, we may spill.  Every unnecessary columns means useless I/O,
	 * and heap_form/deform_tuple.  It is almost always better to to the
	 * projection.
	 */
	if (parse->groupClause)
		*need_tlist_eval = true;
	else
		*need_tlist_eval = false;		/* only eval if not flat tlist */

	/*
	 * If grouping, create sub_tlist entries for all GROUP BY expressions
	 * (GROUP BY items that are simple Vars should be in the list already),
	 * and make an array showing where the group columns are in the sub_tlist.
	 */
	numCols = num_distcols_in_grouplist(parse->groupClause);

	if (numCols > 0)
	{
		int			keyno = 0;
		AttrNumber *grpColIdx;
		Oid		   *grpOperators;
		List	   *grouptles;
		List	   *sortops;
		List	   *eqops;
		ListCell   *lc_tle;
		ListCell   *lc_eqop;

		grpColIdx = (AttrNumber *) palloc(sizeof(AttrNumber) * numCols);
		grpOperators = (Oid *) palloc(sizeof(Oid) * numCols);
		*groupColIdx = grpColIdx;
		*groupOperators = grpOperators;

		get_sortgroupclauses_tles(parse->groupClause, tlist,
								  &grouptles, &sortops, &eqops);
		Assert(numCols == list_length(grouptles) &&
			   numCols == list_length(sortops) &&
			   numCols == list_length(eqops));
		forboth(lc_tle, grouptles, lc_eqop, eqops)
		{
			Node	   *groupexpr;
			TargetEntry *tle;
			TargetEntry *sub_tle = NULL;
			ListCell   *sl = NULL;

			tle = (TargetEntry *) lfirst(lc_tle);
			groupexpr = (Node *) tle->expr;

			/*
			 * Find or make a matching sub_tlist entry.
			 */
			foreach(sl, sub_tlist)
			{
				sub_tle = (TargetEntry *) lfirst(sl);
				if (equal(groupexpr, sub_tle->expr)
					&& (sub_tle->ressortgroupref == 0))
					break;
			}
			if (!sl)
			{
				sub_tle = makeTargetEntry((Expr *) groupexpr,
										  list_length(sub_tlist) + 1,
										  NULL,
										  false);
				sub_tlist = lappend(sub_tlist, sub_tle);
				*need_tlist_eval = true;		/* it's not flat anymore */
			}

			/* Set its group reference and save its resno */
			sub_tle->ressortgroupref = tle->ressortgroupref;
			grpColIdx[keyno] = sub_tle->resno;
			grpOperators[keyno] = lfirst_oid(lc_eqop);
			if (!OidIsValid(grpOperators[keyno]))           /* shouldn't happen */
				elog(ERROR, "could not find equality operator for grouping column");
			keyno++;
		}
		Assert(keyno == numCols);
	}

	return sub_tlist;
}


/*
 * Function: register_ordered_aggs
 *
 * Update the AggOrder nodes found in Aggref nodes of the given Query
 * node for a grouping/aggregating query to refer to targets in the
 * indirectly given subplan target list.  As a side-effect, new targets
 * may be added to he subplan target list.
 *
 * The idea is that Aggref nodes from the input Query node specify
 * ordering expressions corresponding to sort specifications that must
 * refer (via sortgroupref values as usual) to the target list of the
 * node below them in the plan.  Initially they may not, so we must find
 * or add them to the indirectly given subplan targetlist and adjust the
 * AggOrder node to match.
 *
 * This may scribble on the Query!	(This isn't too bad since only the
 * tleSortGroupRef fields of SortClause nodes and the corresponding
 * ressortgroupref fields of TargetEntry nodes in the AggOrder node in
 * an Aggref change, and the interpretation of the list is the same
 * afterward.)
 */
List *
register_ordered_aggs(List *tlist, Node *havingqual, List *sub_tlist)
{
	ListCell   *lc;
	register_ordered_aggs_context ctx;

	ctx.tlist = tlist;			/* aggregating target list */
	ctx.havingqual = havingqual;	/* aggregating HAVING qual */
	ctx.sub_tlist = sub_tlist;	/* input target list */
	ctx.last_sgr = 0;			/* 0 = unassigned */

	/* There may be Aggrefs in the query's target list. */
	foreach(lc, ctx.tlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);

		tle->expr = (Expr *) register_ordered_aggs_mutator((Node *) tle->expr, &ctx);
	}

	/* There may be Aggrefs in the query's having clause */
	ctx.havingqual = register_ordered_aggs_mutator(ctx.havingqual, &ctx);

	return ctx.sub_tlist;
}

/*
 * Function: register_ordered_aggs_mutator
 *
 * Update the AggOrder nodes found in Aggref nodes of the given expression
 * to refer to targets in the context's subplan target list.  New targets
 * may be added to he subplan target list as a side effect.
 */
Node *
register_ordered_aggs_mutator(Node *node,
							  register_ordered_aggs_context * context)
{
	if (node == NULL)
		return NULL;
	if (IsA(node, Aggref))
	{
		Aggref	   *aggref = (Aggref *) node;

		if (aggref->aggorder)
		{
			register_AggOrder(aggref->aggorder, context);
		}
	}
	return expression_tree_mutator(node,
								   register_ordered_aggs_mutator,
								   (void *) context);
}


/*
 * Function register_AggOrder
 *
 * Find or add the sort targets in the given AggOrder node to the
 * indirectly given subplan target list.  If we add a target, give
 * it a distinct sortgroupref value.  
 * 
 * Then update the AggOrder node to refer to the subplan target list.  
 * We need to update the target node too, so the sort specification 
 * continues to refer to its target in the AggOrder.  Note, however,
 * that we need to defer these updates to the end so that we don't
 * mess up the correspondence in the AggOrder before we're done
 * using it.
 */
typedef struct agg_order_update_spec
{
	SortGroupClause *sort;
	TargetEntry *entry;
	Index sortgroupref;
}
agg_order_update_spec;

void register_AggOrder(AggOrder *aggorder, 
					   register_ordered_aggs_context *context)
{	
	ListCell *lc;
	List *updates = NIL;
	agg_order_update_spec *update;
	
	/* In the first release, targets and orders are 1:1.  This may
	 * change, but for now ... */
	Assert( list_length(aggorder->sortTargets) == 
		    list_length(aggorder->sortClause) );
	
	foreach (lc, aggorder->sortClause)
	{
		SortGroupClause *sort;
		TargetEntry *sort_tle;
		TargetEntry *sub_tle;
		
		sort = (SortGroupClause *) lfirst(lc);
		Assert( sort->tleSortGroupRef != 0 );
		sort_tle = get_sortgroupclause_tle(sort, aggorder->sortTargets);
		
		/* Find sort expression in the given target list, ... */
		sub_tle = tlist_member((Node*)sort_tle->expr, context->sub_tlist);
		
		/* ... or add it. */
		if ( !sub_tle )
		{
			sub_tle = makeTargetEntry(copyObject(sort_tle->expr),
									  list_length(context->sub_tlist) + 1,
									  NULL,
									  false);
			/* We fill in the sortgroupref below. */
			context->sub_tlist = lappend( context->sub_tlist, sub_tle );
		}
		
		if ( sub_tle->ressortgroupref == 0 )
		{
			/* Lazy initialize next sortgroupref value. */
			if ( context->last_sgr == 0 )
			{
				ListCell *c;
				/* Targets in sub_tlist and main tlist must not conflict. */
				foreach( c, context->tlist )
				{
					TargetEntry *tle = (TargetEntry*)lfirst(c);
					if ( context->last_sgr < tle->ressortgroupref )
						context->last_sgr = tle->ressortgroupref;
				}
				
				/* Might there be non-zero SGRs in sub_tlist? Don't see
				 * how, but be safe.
				 */
				foreach( c, context->sub_tlist )
				{
					TargetEntry *tle = (TargetEntry*)lfirst(c);
					if ( context->last_sgr < tle->ressortgroupref )
						context->last_sgr = tle->ressortgroupref;
				}
			}

			sub_tle->ressortgroupref = ++context->last_sgr;
		}
		
		/* Update AggOrder to agree with the tle in the target list. */
		update = (agg_order_update_spec*)palloc(sizeof(agg_order_update_spec));
		update->sort = sort;
		update->entry = sort_tle;
		update->sortgroupref = sub_tle->ressortgroupref;
		updates = lappend(updates, update);
	}
	
	foreach (lc, updates)
	{
		update = (agg_order_update_spec*)lfirst(lc);
		
		update->sort->tleSortGroupRef = update->sortgroupref;
		update->entry->ressortgroupref = update->sortgroupref;
	}
	list_free(updates);
}


/*
 * locate_grouping_columns
 *		Locate grouping columns in the tlist chosen by query_planner.
 *
 * This is only needed if we don't use the sub_tlist chosen by
 * make_subplanTargetList.	We have to forget the column indexes found
 * by that routine and re-locate the grouping vars in the real sub_tlist.
 */
static void
locate_grouping_columns(PlannerInfo *root,
						List *tlist,
						List *sub_tlist,
						AttrNumber *groupColIdx)
{
	int			keyno = 0;
	List	   *grouptles;
	List	   *sortops;
	List	   *eqops;
	ListCell   *ge;

	/*
	 * No work unless grouping.
	 */
	if (!root->parse->groupClause)
	{
		Assert(groupColIdx == NULL);
		return;
	}
	Assert(groupColIdx != NULL);

	get_sortgroupclauses_tles(root->parse->groupClause, tlist,
							  &grouptles, &sortops, &eqops);

	foreach (ge, grouptles)
	{
		TargetEntry *groupte = (TargetEntry *)lfirst(ge);
		Node	*groupexpr;

		TargetEntry *te = NULL;
		ListCell   *sl;

		groupexpr = (Node *) groupte->expr;

		foreach(sl, sub_tlist)
		{
			te = (TargetEntry *) lfirst(sl);
			if (equal(groupexpr, te->expr))
				break;
		}
		if (!sl)
			elog(ERROR, "failed to locate grouping columns");

		groupColIdx[keyno++] = te->resno;
	}
}

/*
 * postprocess_setop_tlist
 *	  Fix up targetlist returned by plan_set_operations().
 *
 * We need to transpose sort key info from the orig_tlist into new_tlist.
 * NOTE: this would not be good enough if we supported resjunk sort keys
 * for results of set operations --- then, we'd need to project a whole
 * new tlist to evaluate the resjunk columns.  For now, just ereport if we
 * find any resjunk columns in orig_tlist.
 */
static List *
postprocess_setop_tlist(List *new_tlist, List *orig_tlist)
{
	ListCell   *l;
	ListCell   *orig_tlist_item = list_head(orig_tlist);

	/* empty orig has no effect on info in new (MPP-2655) */
	if (orig_tlist_item == NULL)
		return new_tlist;

	foreach(l, new_tlist)
	{
		TargetEntry *new_tle = (TargetEntry *) lfirst(l);
		TargetEntry *orig_tle;

		/* ignore resjunk columns in setop result */
		if (new_tle->resjunk)
			continue;

		Assert(orig_tlist_item != NULL);
		orig_tle = (TargetEntry *) lfirst(orig_tlist_item);
		orig_tlist_item = lnext(orig_tlist_item);
		if (orig_tle->resjunk)	/* should not happen */
			elog(ERROR, "resjunk output columns are not implemented");
		Assert(new_tle->resno == orig_tle->resno);
		new_tle->ressortgroupref = orig_tle->ressortgroupref;
	}
	if (orig_tlist_item != NULL)
		elog(ERROR, "resjunk output columns are not implemented");
	return new_tlist;
}

/*
 * Produce the canonical form of a GROUP BY clause given the parse
 * tree form.
 *
 * The result is a CanonicalGroupingSets, which contains a list of
 * Bitmapsets.  Each Bitmapset contains the sort-group reference
 * values of the attributes in one of the grouping sets specified in
 * the GROUP BY clause.  The number of list elements is the number of
 * grouping sets specified.
 */
static CanonicalGroupingSets *
make_canonical_groupingsets(List *groupClause)
{
	CanonicalGroupingSets *canonical_grpsets = 
		(CanonicalGroupingSets *) palloc0(sizeof(CanonicalGroupingSets));
	ListCell *lc;
	List *ord_grping = NIL; /* the ordinary grouping */
	List *rollups = NIL;    /* the grouping sets from ROLLUP */
	List *grpingsets = NIL; /* the grouping sets from GROUPING SETS */
	List *cubes = NIL;      /* the grouping sets from CUBE */
	Bitmapset *bms = NULL;
	List *final_grpingsets = NIL;
	List *list_grpingsets = NIL;
	int setno;
	int prev_setno = 0;

	if (groupClause == NIL)
		return canonical_grpsets;

	foreach (lc, groupClause)
	{
		GroupingClause *gc;

		Node *node = lfirst(lc);

		if (node == NULL)
			continue;

		/* Note that the top-level empty sets have been removed
		 * in the parser.
		 */
		Assert(IsA(node, SortGroupClause) ||
			   IsA(node, GroupingClause) ||
			   IsA(node, List));

		if (IsA(node, SortGroupClause) ||
			IsA(node, List))
		{
			ord_grping = lappend(ord_grping,
								 canonicalize_colref_list(node));
			continue;
		}

		gc = (GroupingClause *)node;
		switch (gc->groupType)
		{
			case GROUPINGTYPE_ROLLUP:
				rollups = lappend(rollups,
								  rollup_gs_list(canonicalize_gs_list(gc->groupsets, true)));
				break;
			case GROUPINGTYPE_CUBE:
				cubes = lappend(cubes,
								cube_gs_list(canonicalize_gs_list(gc->groupsets, true)));
				break;
			case GROUPINGTYPE_GROUPING_SETS:
				grpingsets = lappend(grpingsets,
									 canonicalize_gs_list(gc->groupsets, false));
				break;
			default:
				elog(ERROR, "invalid grouping set");
		}
	}

	/* Obtain the cartesian product of grouping sets generated for ordinary
	 * grouping sets, rollups, cubes, and grouping sets.
	 *
	 * We apply a small optimization here. We always append grouping sets
	 * generated for rollups, cubes and grouping sets to grouping sets for
	 * ordinary sets. This makes it easier to tell if there is a partial
	 * rollup. Consider the example of GROUP BY rollup(i,j),k. There are
	 * three grouping sets for rollup(i,j): (i,j), (i), (). If we append
	 * k after each grouping set for rollups, we get three sets:
	 * (i,j,k), (i,k) and (k). We can not easily tell that this is a partial
	 * rollup. However, if we append each grouping set after k, we get
	 * these three sets: (k,i,j), (k,i), (k), which is obviously a partial
	 * rollup.
	 */

	/* First, we bring all columns in ordinary grouping sets together into
	 * one list.
	 */
	foreach (lc, ord_grping)
	{
	    Bitmapset *sub_bms = (Bitmapset *)lfirst(lc);
		bms = bms_add_members(bms, sub_bms);
	}

	final_grpingsets = lappend(final_grpingsets, bms);

	/* Make the list of grouping sets */
	if (rollups)
		list_grpingsets = list_concat(list_grpingsets, rollups);
	if (cubes)
		list_grpingsets = list_concat(list_grpingsets, cubes);
	if (grpingsets)
		list_grpingsets = list_concat(list_grpingsets, grpingsets);

	/* Obtain the cartesian product of grouping sets generated from ordinary
	 * grouping sets, rollups, cubes, and grouping sets.
	 */
	foreach (lc, list_grpingsets)
	{
		List *bms_list = (List *)lfirst(lc);
		ListCell *tmp_lc;
		List *tmp_list;

		tmp_list = final_grpingsets;
		final_grpingsets = NIL;

		foreach (tmp_lc, tmp_list)
		{
			Bitmapset *tmp_bms = (Bitmapset *)lfirst(tmp_lc);
			ListCell *bms_lc;

			foreach (bms_lc, bms_list)
			{
				bms = bms_copy(tmp_bms);
				bms = bms_add_members(bms, (Bitmapset *)lfirst(bms_lc));
				final_grpingsets = lappend(final_grpingsets, bms);
			}
		}
	}

	/* Sort final_grpingsets */
	sort_canonical_gs_list(final_grpingsets,
						   &(canonical_grpsets->ngrpsets),
						   &(canonical_grpsets->grpsets));

	/* Combine duplicate grouping sets and set the counts for
	 * each grouping set.
	 */
	canonical_grpsets->grpset_counts =
		(int *)palloc0(canonical_grpsets->ngrpsets * sizeof(int));
	prev_setno = 0;
	canonical_grpsets->grpset_counts[0] = 1;
	for (setno = 1; setno<canonical_grpsets->ngrpsets; setno++)
	{
		if (bms_equal(canonical_grpsets->grpsets[setno],
					  canonical_grpsets->grpsets[prev_setno]))
		{
			canonical_grpsets->grpset_counts[prev_setno]++;
			if (canonical_grpsets->grpsets[setno])
				pfree(canonical_grpsets->grpsets[setno]);
		}

		else
		{
			prev_setno++;
			canonical_grpsets->grpsets[prev_setno] =
				canonical_grpsets->grpsets[setno];
			canonical_grpsets->grpset_counts[prev_setno]++;
		}
	}
	/* Reset ngrpsets to eliminate duplicate groupint sets */
	canonical_grpsets->ngrpsets = prev_setno + 1;

	/* Obtain the number of distinct columns appeared in these
	 * grouping sets.
	 */
	{
		Bitmapset *distcols = NULL;
		for (setno =0; setno < canonical_grpsets->ngrpsets; setno++)
			distcols =
				bms_add_members(distcols, canonical_grpsets->grpsets[setno]);
		
		canonical_grpsets->num_distcols = bms_num_members(distcols);
		bms_free(distcols);
	}
	

	/* Release spaces */
	list_free_deep(ord_grping);
	list_free_deep(list_grpingsets);
	list_free(final_grpingsets);
	
	return canonical_grpsets;
}

/* Produce the canonical representation of a column reference list.
 *
 * A column reference list (in SQL) is a comma-delimited list of
 * column references which are represented by the parser as a
 * List of GroupClauses.  No nesting is allowed in column reference 
 * lists.
 *
 * As a convenience, this function also recognizes a bare column
 * reference.
 *
 * The result is a Bitmapset of the sort-group-ref values in the list.
 */
static Bitmapset* canonicalize_colref_list(Node * node)
{
	ListCell *lc;
	SortGroupClause *gc;
	Bitmapset* gs = NULL;
	
	if ( node == NULL )
		elog(ERROR,"invalid column reference list");
	
	if ( IsA(node, SortGroupClause) )
	{
		gc = (SortGroupClause *) node;
		return bms_make_singleton(gc->tleSortGroupRef);
	}
	
	if ( !IsA(node, List) )
		elog(ERROR,"invalid column reference list");
	
	foreach (lc, (List*)node)
	{
		Node *cr = lfirst(lc);
		
		if ( cr == NULL )
			continue;
			
		if ( !IsA(cr, SortGroupClause) )
			elog(ERROR,"invalid column reference list");

		gc = (SortGroupClause *) cr;
		gs = bms_add_member(gs, gc->tleSortGroupRef);	
	}
	return gs;
}

/* Produce the list of canonical grouping sets corresponding to a
 * grouping set list or an ordinary grouping set list.
 * 
 * An ordinary grouping set list (in SQL) is a comma-delimited list 
 * of ordinary grouping sets.  
 * 
 * Each ordinary grouping set is either a grouping column reference 
 * or a parenthesized list of grouping column references.  No nesting 
 * is allowed.  
 *
 * A grouping set list (in SQL) is a comma-delimited list of grouping 
 * sets.  
 *
 * Each grouping set is either an ordinary grouping set, a rollup list, 
 * a cube list, the empty grouping set, or (recursively) a grouping set 
 * list.
 *
 * The parse tree form of an ordinary grouping set is a  list containing
 * GroupClauses and lists of GroupClauses (without nesting).  In the case
 * of a (general) grouping set, the parse tree list may also include
 * NULLs and GroupingClauses.
 *
 * The result is a list of bit map sets.
 */
static List *canonicalize_gs_list(List *gsl, bool ordinary)
{
	ListCell *lc;
	List *list = NIL;

	foreach (lc, gsl)
	{
		Node *node = lfirst(lc);

		if ( node == NULL )
		{
			if ( ordinary )
				elog(ERROR,"invalid ordinary grouping set");
			
			list = lappend(list, NIL); /* empty grouping set */
		}
		else if ( IsA(node, SortGroupClause) || IsA(node, List) )
		{
			/* ordinary grouping set */
			list = lappend(list, canonicalize_colref_list(node));
		}
		else if ( IsA(node, GroupingClause) )
		{	
			List *gs = NIL;
			GroupingClause *gc = (GroupingClause*)node;
			
			if ( ordinary )
				elog(ERROR,"invalid ordinary grouping set");
				
			switch ( gc->groupType )
			{
				case GROUPINGTYPE_ROLLUP:
					gs = rollup_gs_list(canonicalize_gs_list(gc->groupsets, true));
					break;
				case GROUPINGTYPE_CUBE:
					gs = cube_gs_list(canonicalize_gs_list(gc->groupsets, true));
					break;
				case GROUPINGTYPE_GROUPING_SETS:
					gs = canonicalize_gs_list(gc->groupsets, false);
					break;
				default:
					elog(ERROR,"invalid grouping set");
			}
			list = list_concat(list,gs);
		}
		else
		{
			elog(ERROR,"invalid grouping set list");
		}
	}
	return list;
}

/* Produce the list of N+1 canonical grouping sets corresponding
 * to the rollup of the given list of N canonical grouping sets.
 * These N+1 grouping sets are listed in the descending order
 * based on the number of columns.
 *
 * Argument and result are both a list of bit map sets.
 */
static List *rollup_gs_list(List *gsl)
{
	ListCell *lc;
	Bitmapset **bms;
	int i, n = list_length(gsl);
	
	if ( n == 0 )
		elog(ERROR,"invalid grouping ordinary grouping set list");
	
	if ( n > 1 )
	{
		/* Reverse the elements in gsl */
		List *new_gsl = NIL;
		foreach (lc, gsl)
		{
			new_gsl = lcons(lfirst(lc), new_gsl);
		}
		list_free(gsl);
		gsl = new_gsl;

		bms = (Bitmapset**)palloc(n*sizeof(Bitmapset*));
		i = 0;
		foreach (lc, gsl)
		{
			bms[i++] = (Bitmapset*)lfirst(lc);
		}
		for ( i = n-2; i >= 0; i-- )
		{
			bms[i] = bms_add_members(bms[i], bms[i+1]);
		}
		pfree(bms);
	}

	return lappend(gsl, NULL);
}

/* Subroutine for cube_gs_list. */
static List *add_gs_combinations(List *list, int n, int i,
								 Bitmapset **base, Bitmapset **work)
{
	if ( i < n )
	{
		work[i] = base[i];
		list = add_gs_combinations(list, n, i+1, base, work);
		work[i] = NULL;
		list = add_gs_combinations(list, n, i+1, base, work);	
	}
	else
	{
		Bitmapset *gs = NULL;
		int j;
		for ( j = 0; j < n; j++ )
		{
			gs = bms_add_members(gs, work[j]);
		}
		list = lappend(list,gs);
	}
	return list;
}

/* Produce the list of 2^N canonical grouping sets corresponding
 * to the cube of the given list of N canonical grouping sets.
 *
 * We could do this more efficiently, but the number of grouping
 * sets should be small, so don't bother.
 *
 * Argument and result are both a list of bit map sets.
 */
static List *cube_gs_list(List *gsl)
{
	ListCell *lc;
	Bitmapset **bms_base;
	Bitmapset **bms_work;
	int i, n = list_length(gsl);
	
	if ( n == 0 )
		elog(ERROR,"invalid grouping ordinary grouping set list");
	
	bms_base = (Bitmapset**)palloc(n*sizeof(Bitmapset*));
	bms_work = (Bitmapset**)palloc(n*sizeof(Bitmapset*));
	i = 0;
	foreach (lc, gsl)
	{
		bms_work[i] = NULL;
		bms_base[i++] = (Bitmapset*)lfirst(lc);
	}

	return add_gs_combinations(NIL, n, 0, bms_base, bms_work);
}

/* Subroutine for sort_canonical_gs_list. */
static int gs_compare(const void *a, const void*b)
{
	/* Put the larger grouping sets before smaller ones. */
	return (0-bms_compare(*(Bitmapset**)a, *(Bitmapset**)b));
}

/* Produce a sorted array of Bitmapsets from the given list of Bitmapsets in
 * descending order.
 */
static void sort_canonical_gs_list(List *gs, int *p_nsets, Bitmapset ***p_sets)
{
	ListCell *lc;
	int nsets = list_length(gs);
	Bitmapset **sets = palloc(nsets*sizeof(Bitmapset*));
	int i = 0;
	
	foreach (lc, gs)
	{
		sets[i++] =  (Bitmapset*)lfirst(lc);
	}
	
	qsort(sets, nsets, sizeof(Bitmapset*), gs_compare);
	
	Assert( p_nsets != NULL && p_sets != NULL );
	
	*p_nsets = nsets;
	*p_sets = sets;
}

/*
 * In any plan where we are doing multi-phase limit, the first phase needs
 * to take the offset into account.
 */
static Plan *
pushdown_preliminary_limit(Plan *plan, Node *limitCount, int64 count_est, Node *limitOffset, int64 offset_est)
{
	Node *precount = copyObject(limitCount);
	int64 precount_est = count_est;
	Plan *result_plan = plan;

	/*
	 * If we've specified an offset *and* a limit, we need to collect
	 * from tuples from 0 -> count + offset
	 *
	 * add offset to each QEs requested contribution. 
	 * ( MPP-1370: Do it even if no ORDER BY was specified) 
	 */	
	if (precount && limitOffset)
	{
		precount = (Node*)make_op(NULL,
								  list_make1(makeString(pstrdup("+"))),
								  copyObject(limitOffset),
								  precount,
								  -1);
		precount_est += offset_est;
	}
			
	if (precount != NULL)
	{
		/*
		 * Add a prelimary LIMIT on the partitioned results. This may
		 * reduce the amount of work done on the QEs.
		 */
		result_plan = (Plan *) make_limit(result_plan,
										  NULL,
										  precount,
										  0,
										  precount_est);

		result_plan->flow = pull_up_Flow(result_plan, result_plan->lefttree);
	}

	return result_plan;
}


/*
 * isSimplyUpdatableQuery -
 *  determine whether a query is a simply updatable scan of a relation
 *
 * A query is simply updatable if, and only if, it...
 * - has no window clauses
 * - has no sort clauses
 * - has no grouping, having, distinct clauses, or simple aggregates
 * - has no subqueries
 * - has no LIMIT/OFFSET
 * - references only one range table (i.e. no joins, self-joins)
 *   - this range table must itself be updatable
 */
static bool
isSimplyUpdatableQuery(Query *query)
{
	if (query->commandType == CMD_SELECT &&
		query->windowClause == NIL &&
		query->sortClause == NIL &&
		query->groupClause == NIL &&
		query->havingQual == NULL &&
		query->distinctClause == NIL &&
		!query->hasAggs &&
		!query->hasSubLinks &&
		query->limitCount == NULL &&
		query->limitOffset == NULL &&
		list_length(query->rtable) == 1)
	{
		RangeTblEntry *rte = (RangeTblEntry *) linitial(query->rtable);
		if (isSimplyUpdatableRelation(rte->relid, true))
			return true;
	}
	return false;
}
