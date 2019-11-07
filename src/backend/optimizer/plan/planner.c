/*-------------------------------------------------------------------------
 *
 * planner.c
 *	  The query optimizer external interface.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/plan/planner.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <limits.h>

#include "access/htup_details.h"
#include "executor/executor.h"
#include "executor/execHHashagg.h"
#include "executor/nodeAgg.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#ifdef OPTIMIZER_DEBUG
#include "nodes/print.h"
#endif
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/orca.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/plancat.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/prep.h"
#include "optimizer/subselect.h"
#include "optimizer/transform.h"
#include "optimizer/tlist.h"
#include "parser/analyze.h"
#include "parser/parse_oper.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/selfuncs.h"

#include "catalog/gp_segment_config.h"
#include "catalog/pg_proc.h"
#include "cdb/cdbhash.h"
#include "cdb/cdbllize.h"
#include "cdb/cdbmutate.h"		/* apply_shareinput */
#include "cdb/cdbpartition.h"
#include "cdb/cdbpath.h"		/* cdbpath_segments */
#include "cdb/cdbpullup.h"
#include "cdb/cdbgroup.h"		/* grouping_planner extensions */
#include "cdb/cdbsetop.h"		/* motion utilities */
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "storage/lmgr.h"
#include "utils/guc.h"


/* GUC parameter */
double		cursor_tuple_fraction = DEFAULT_CURSOR_TUPLE_FRACTION;

/* Hook for plugins to get control in planner() */
planner_hook_type planner_hook = NULL;


/* Expression kind codes for preprocess_expression */
#define EXPRKIND_QUAL			0
#define EXPRKIND_TARGET			1
#define EXPRKIND_RTFUNC			2
#define EXPRKIND_RTFUNC_LATERAL 3
#define EXPRKIND_VALUES			4
#define EXPRKIND_VALUES_LATERAL 5
#define EXPRKIND_LIMIT			6
#define EXPRKIND_APPINFO		7
#define EXPRKIND_PHV			8
#define EXPRKIND_WINDOW_BOUND	9

/* Passthrough data for standard_qp_callback */
typedef struct
{
	List	   *tlist;			/* preprocessed query targetlist */
	List	   *activeWindows;	/* active windows, if any */
} standard_qp_extra;

/*
 * Temporary structure for use during WindowClause reordering in order to be
 * be able to sort WindowClauses on partitioning/ordering prefix.
 */
typedef struct
{
	WindowClause *wc;
	List	   *uniqueOrder;	/* A List of unique ordering/partitioning
								 * clauses per Window */
} WindowClauseSortData;

/* Local functions */
static Node *preprocess_expression(PlannerInfo *root, Node *expr, int kind);
static void preprocess_qual_conditions(PlannerInfo *root, Node *jtnode);
static Plan *inheritance_planner(PlannerInfo *root);
static Plan *grouping_planner(PlannerInfo *root, double tuple_fraction);
static void preprocess_rowmarks(PlannerInfo *root);
static double preprocess_limit(PlannerInfo *root,
				 double tuple_fraction,
				 int64 *offset_est, int64 *count_est);
static bool limit_needed(Query *parse);
static void preprocess_groupclause(PlannerInfo *root);
static void standard_qp_callback(PlannerInfo *root, void *extra);
static bool choose_hashed_distinct(PlannerInfo *root,
					   double tuple_fraction, double limit_tuples,
					   double path_rows, int path_width,
					   Cost cheapest_startup_cost, Cost cheapest_total_cost,
					   Cost sorted_startup_cost, Cost sorted_total_cost,
					   List *sorted_pathkeys,
					   double dNumDistinctRows);
static List *make_subplanTargetList(PlannerInfo *root, List *tlist,
					   AttrNumber **groupColIdx, Oid **groupOperators, bool *need_tlist_eval);
static void locate_grouping_columns(PlannerInfo *root,
						List *stlist,
						List *sub_tlist,
						AttrNumber *groupColIdx);
static List *postprocess_setop_tlist(List *new_tlist, List *orig_tlist);
static List *select_active_windows(PlannerInfo *root, WindowFuncLists *wflists);
static List *make_windowInputTargetList(PlannerInfo *root,
						   List *tlist, List *activeWindows);
static List *make_pathkeys_for_window(PlannerInfo *root, WindowClause *wc,
						 List *tlist);
static void get_column_info_for_window(PlannerInfo *root, WindowClause *wc,
						   List *tlist,
						   int numSortCols, AttrNumber *sortColIdx,
						   int *partNumCols,
						   AttrNumber **partColIdx,
						   Oid **partOperators,
						   int *ordNumCols,
						   AttrNumber **ordColIdx,
						   Oid **ordOperators);
static int	common_prefix_cmp(const void *a, const void *b);

static Bitmapset *canonicalize_colref_list(Node *node);
static List *canonicalize_gs_list(List *gsl, bool ordinary);
static List *rollup_gs_list(List *gsl);
static List *add_gs_combinations(List *list, int n, int i, Bitmapset **base, Bitmapset **work);
static List *cube_gs_list(List *gsl);
static CanonicalGroupingSets *make_canonical_groupingsets(List *groupClause);
static int	gs_compare(const void *a, const void *b);
static void sort_canonical_gs_list(List *gs, int *p_nsets, Bitmapset ***p_sets);

static Plan *pushdown_preliminary_limit(Plan *plan, Node *limitCount, int64 count_est, Node *limitOffset, int64 offset_est);

static Plan *getAnySubplan(Plan *node);
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
	MemoryAccountIdType curMemoryAccountId;

	/*
	 * Use ORCA only if it is enabled and we are in a master QD process.
	 *
	 * ORCA excels in complex queries, most of which will access distributed
	 * tables. We can't run such queries from the segments slices anyway because
	 * they require dispatching a query within another - which is not allowed in
	 * GPDB (see querytree_safe_for_qe()). Note that this restriction also
	 * applies to non-QD master slices.  Furthermore, ORCA doesn't currently
	 * support pl/<lang> statements (relevant when they are planned on the segments).
	 * For these reasons, restrict to using ORCA on the master QD processes only.
	 */
	if (optimizer &&
		GP_ROLE_DISPATCH == Gp_role &&
		IS_QUERY_DISPATCHER())
	{
		if (gp_log_optimization_time)
			INSTR_TIME_SET_CURRENT(starttime);

		result = optimize_query(parse, boundParams);

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

	curMemoryAccountId = MemoryAccounting_GetOrCreatePlannerAccount();
	/*
	 * Incorrectly indented on purpose to avoid re-indenting an entire upstream
	 * function
	 */
	START_MEMORY_ACCOUNT(curMemoryAccountId);
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
	glob->subplans = NIL;
	glob->subroots = NIL;
	glob->rewindPlanIDs = NULL;
	glob->finalrtable = NIL;
	glob->finalrowmarks = NIL;
	glob->resultRelations = NIL;
	glob->relationOids = NIL;
	glob->invalItems = NIL;
	glob->nParamExec = 0;
	glob->lastPHId = 0;
	glob->lastRowMarkId = 0;
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
		 * We document cursor_tuple_fraction as simply being a fraction, which
		 * means the edge cases 0 and 1 have to be treated specially here.  We
		 * convert 1 to 0 ("all the tuples") and 0 to a very small fraction.
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
	top_plan = subquery_planner(glob, parse, NULL,
								false, tuple_fraction, &root,
								config);

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
	forboth(lp, glob->subplans, lr, glob->subroots)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);
		PlannerInfo	   *subroot = (PlannerInfo *) lfirst(lr);

		lfirst(lp) = apply_shareinput_dag_to_tree(subroot, subplan);
	}
	top_plan = apply_shareinput_dag_to_tree(root, top_plan);

	/* final cleanup of the plan */
	Assert(glob->finalrtable == NIL);
	Assert(glob->finalrowmarks == NIL);
	Assert(glob->resultRelations == NIL);
	Assert(parse == root->parse);
	top_plan = set_plan_references(root, top_plan);
	/* ... and the subplans (both regular subplans and initplans) */
	Assert(list_length(glob->subplans) == list_length(glob->subroots));
	forboth(lp, glob->subplans, lr, glob->subroots)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);
		PlannerInfo *subroot = (PlannerInfo *) lfirst(lr);

		lfirst(lp) = set_plan_references(subroot, subplan);
	}

	/* walk plan and remove unused initplans and their params */
	remove_unused_initplans(top_plan, root);

	/* walk subplans and fixup subplan node referring to same plan_id */
	SubPlanWalkerContext subplan_context;
	fixup_subplans(top_plan, root, &subplan_context);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		top_plan = cdbparallelize(root, top_plan, parse);

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
		top_plan = apply_shareinput_xslice(top_plan, root);
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

	/* fix ShareInputScans for EXPLAIN */
	foreach(lp, glob->subplans)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);

		lfirst(lp) = replace_shareinput_targetlists(root, subplan);
	}
	top_plan = replace_shareinput_targetlists(root, top_plan);

	/* build the PlannedStmt result */
	result = makeNode(PlannedStmt);

	result->commandType = parse->commandType;
	result->queryId = parse->queryId;
	result->hasReturning = (parse->returningList != NIL);
	result->hasModifyingCTE = parse->hasModifyingCTE;
	result->canSetTag = parse->canSetTag;
	result->transientPlan = glob->transientPlan;
	result->oneoffPlan = glob->oneoffPlan;
	result->planTree = top_plan;
	result->rtable = glob->finalrtable;
	result->resultRelations = glob->resultRelations;
	result->utilityStmt = parse->utilityStmt;
	result->subplans = glob->subplans;
	result->rewindPlanIDs = glob->rewindPlanIDs;
	result->result_partitions = root->result_partitions;
	result->result_aosegnos = root->result_aosegnos;
	result->rowMarks = glob->finalrowmarks;
	result->relationOids = glob->relationOids;
	result->invalItems = glob->invalItems;
	result->nParamExec = glob->nParamExec;

	result->nMotionNodes = top_plan->nMotionNodes;
	result->nInitPlans = top_plan->nInitPlans;
	result->intoPolicy = GpPolicyCopy(parse->intoPolicy);
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
		 * Generate a plan node id for each node. Used by gpmon and instrument.
		 * Note that this needs to be the last step of the planning when the
		 * structure of the plan is final.
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
				 bool hasRecursion, double tuple_fraction,
				 PlannerInfo **subroot,
				 PlannerConfig *config)
{
	int			num_old_subplans = list_length(glob->subplans);
	PlannerInfo *root;
	Plan	   *plan;
	List	   *newWithCheckOptions;
	List	   *newHaving;
	bool		hasOuterJoins;
	ListCell   *l;

	/* Create a PlannerInfo data structure for this subquery */
	root = makeNode(PlannerInfo);
	root->parse = parse;
	root->glob = glob;
	root->query_level = parent_root ? parent_root->query_level + 1 : 1;
	root->parent_root = parent_root;
	root->plan_params = NIL;
	root->planner_cxt = CurrentMemoryContext;
	root->init_plans = NIL;
	root->cte_plan_ids = NIL;
	root->eq_classes = NIL;
	root->non_eq_clauses = NIL;
	root->init_plans = NIL;

	root->list_cteplaninfo = NIL;
	if (parse->cteList != NIL)
	{
		root->list_cteplaninfo = init_list_cteplaninfo(list_length(parse->cteList));
	}

	root->append_rel_list = NIL;
	root->rowMarks = NIL;
	root->hasInheritedTarget = false;
	root->upd_del_replicated_table = 0;

	Assert(config);
	root->config = config;

	root->hasRecursion = hasRecursion;
	if (hasRecursion)
		root->wt_param_id = SS_assign_special_param(root);
	else
		root->wt_param_id = -1;
	root->non_recursive_plan = NULL;
	root->is_correlated_subplan = false;

	/*
	 * If there is a WITH list, process each WITH query and build an initplan
	 * SubPlan structure for it.
	 *
	 * Unlike upstream, we do not use initplan + CteScan, so SS_process_ctes
	 * will generate unused initplans. Commenting out the following two
	 * lines.
	 */
#if 0
	if (parse->cteList)
		SS_process_ctes(root);
#endif

	/*
	 * Ensure that jointree has been normalized. See
	 * normalize_query_jointree_mutator()
	 */
	AssertImply(parse->jointree->fromlist, list_length(parse->jointree->fromlist) == 1);

	/*
	 * Look for ANY and EXISTS SubLinks in WHERE and JOIN/ON clauses, and try
	 * to transform them into joins.  Note that this step does not descend
	 * into subqueries; if we pull up any subqueries below, their SubLinks are
	 * processed just before pulling them up.
	 */
	if (parse->hasSubLinks)
		pull_up_sublinks(root);

	/*
	 * Scan the rangetable for set-returning functions, and inline them if
	 * possible (producing subqueries that might get pulled up next).
	 * Recursion issues here are handled in the same way as for SubLinks.
	 */
	inline_set_returning_functions(root);

	/*
	 * Check to see if any subqueries in the jointree can be merged into this
	 * query.
	 */
	parse->jointree = (FromExpr *)
		pull_up_subqueries(root, (Node *) parse->jointree);

	/*
	 * If this is a simple UNION ALL query, flatten it into an appendrel. We
	 * do this now because it requires applying pull_up_subqueries to the leaf
	 * queries of the UNION ALL, which weren't touched above because they
	 * weren't referenced by the jointree (they will be after we do this).
	 */
	if (parse->setOperations)
		flatten_simple_union_all(root);

	if ((parent_root && parent_root->is_correlated_subplan) ||
		((Gp_role == GP_ROLE_DISPATCH) &&
		root->config->is_under_subplan &&
		IsSubqueryCorrelated(parse)))
	{
		root->is_correlated_subplan = true;
		/*
		 * Generate the plan for the subquery with certain options disabled.
		 */
		config->gp_enable_direct_dispatch = false;
		config->gp_enable_multiphase_agg = false;

		/*
		 * The MIN/MAX optimization works by inserting a subplan with LIMIT 1.
		 * That effectively turns a correlated subquery into a multi-level
		 * correlated subquery, which we don't currently support. (See check
		 * above.)
		 */
		config->gp_enable_minmax_optimization = false;
	}

	/*
	 * Detect whether any rangetable entries are RTE_JOIN kind; if not, we can
	 * avoid the expense of doing flatten_join_alias_vars().  Also check for
	 * outer joins --- if none, we can skip reduce_outer_joins().  And check
	 * for LATERAL RTEs, too.  This must be done after we have done
	 * pull_up_subqueries(), of course.
	 */
	root->hasJoinRTEs = false;
	root->hasLateralRTEs = false;
	hasOuterJoins = false;
	foreach(l, parse->rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(l);

		if (rte->rtekind == RTE_JOIN)
		{
			root->hasJoinRTEs = true;
			if (IS_OUTER_JOIN(rte->jointype))
				hasOuterJoins = true;
		}
		if (rte->lateral)
			root->hasLateralRTEs = true;
	}

	/*
	 * Preprocess RowMark information.  We need to do this after subquery
	 * pullup (so that all non-inherited RTEs are present) and before
	 * inheritance expansion (so that the info is available for
	 * expand_inherited_tables to examine and modify).
	 */
	preprocess_rowmarks(root);

	/*
	 * Expand any rangetable entries that are inheritance sets into "append
	 * relations".  This can add entries to the rangetable, but they must be
	 * plain base relations not joins, so it's OK (and marginally more
	 * efficient) to do it after checking for join RTEs.  We must do it after
	 * pulling up subqueries, else we'd fail to handle inherited tables in
	 * subqueries.
	 */
	expand_inherited_tables(root);

	/*
	 * Set hasHavingQual to remember if HAVING clause is present.  Needed
	 * because preprocess_expression will reduce a constant-true condition to
	 * an empty qual list ... but "HAVING TRUE" is not a semantic no-op.
	 */
	root->hasHavingQual = (parse->havingQual != NULL);

	/* Clear this flag; might get set in distribute_qual_to_rels */
	root->hasPseudoConstantQuals = false;

	/*
	 * Do expression preprocessing on targetlist and quals, as well as other
	 * random expressions in the querytree.  Note that we do not need to
	 * handle sort/group expressions explicitly, because they are actually
	 * part of the targetlist.
	 */
	parse->targetList = (List *)
		preprocess_expression(root, (Node *) parse->targetList,
							  EXPRKIND_TARGET);

	newWithCheckOptions = NIL;
	foreach(l, parse->withCheckOptions)
	{
		WithCheckOption *wco = (WithCheckOption *) lfirst(l);

		wco->qual = preprocess_expression(root, wco->qual,
										  EXPRKIND_QUAL);
		if (wco->qual != NULL)
			newWithCheckOptions = lappend(newWithCheckOptions, wco);
	}
	parse->withCheckOptions = newWithCheckOptions;

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

	/* Also need to preprocess expressions within RTEs */
	foreach(l, parse->rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(l);
		int			kind;

		if (rte->rtekind == RTE_SUBQUERY)
		{
			/*
			 * We don't want to do all preprocessing yet on the subquery's
			 * expressions, since that will happen when we plan it.  But if it
			 * contains any join aliases of our level, those have to get
			 * expanded now, because planning of the subquery won't do it.
			 * That's only possible if the subquery is LATERAL.
			 */
			if (rte->lateral && root->hasJoinRTEs)
				rte->subquery = (Query *)
					flatten_join_alias_vars(root, (Node *) rte->subquery);
		}
		else if (rte->rtekind == RTE_FUNCTION || rte->rtekind == RTE_TABLEFUNCTION)
		{
			/* Preprocess the function expression(s) fully */
			kind = rte->lateral ? EXPRKIND_RTFUNC_LATERAL : EXPRKIND_RTFUNC;
			rte->functions = (List *) preprocess_expression(root, (Node *) rte->functions, kind);
		}
		else if (rte->rtekind == RTE_VALUES)
		{
			/* Preprocess the values lists fully */
			kind = rte->lateral ? EXPRKIND_VALUES_LATERAL : EXPRKIND_VALUES;
			rte->values_lists = (List *)
				preprocess_expression(root, (Node *) rte->values_lists, kind);
		}
	}

	/*
	 * In some cases we may want to transfer a HAVING clause into WHERE. We
	 * cannot do so if the HAVING clause contains aggregates (obviously) or
	 * volatile functions (since a HAVING clause is supposed to be executed
	 * only once per group).  Also, it may be that the clause is so expensive
	 * to execute that we're better off doing it only once per group, despite
	 * the loss of selectivity.  This is hard to estimate short of doing the
	 * entire planning process twice, so we use a heuristic: clauses
	 * containing subplans are left in HAVING.  Otherwise, we move or copy the
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
	{
		plan = grouping_planner(root, tuple_fraction);
		/* If it's not SELECT, we need a ModifyTable node */
		if (parse->commandType != CMD_SELECT)
		{
			List	   *withCheckOptionLists;
			List	   *returningLists;
			List	   *rowMarks;

			/*
			 * Set up the WITH CHECK OPTION and RETURNING lists-of-lists, if
			 * needed.
			 */
			if (parse->withCheckOptions)
				withCheckOptionLists = list_make1(parse->withCheckOptions);
			else
				withCheckOptionLists = NIL;

			if (parse->returningList)
				returningLists = list_make1(parse->returningList);
			else
				returningLists = NIL;

			/*
			 * If there was a FOR [KEY] UPDATE/SHARE clause, the LockRows node
			 * will have dealt with fetching non-locked marked rows, else we
			 * need to have ModifyTable do that.
			 */
			if (parse->rowMarks)
				rowMarks = NIL;
			else
				rowMarks = root->rowMarks;

			plan = (Plan *) make_modifytable(root,
											 parse->commandType,
											 parse->canSetTag,
									   list_make1_int(parse->resultRelation),
											 list_make1(plan),
											 withCheckOptionLists,
											 returningLists,
											 list_make1_int(root->is_split_update),
											 rowMarks,
											 SS_assign_special_param(root));
		}
	}

	/*
	 * If any subplans were generated, or if there are any parameters to worry
	 * about, build initPlan list and extParam/allParam sets for plan nodes,
	 * and attach the initPlans to the top plan node.
	 */
	if (list_length(glob->subplans) != num_old_subplans ||
		root->glob->nParamExec > 0)
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
 *		conditions), a HAVING clause, or a few other things.
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
	 * base-relation variables.  We must do this before sublink processing,
	 * else sublinks expanded out from join aliases would not get processed.
	 * We can skip it in non-lateral RTE functions and VALUES lists, however,
	 * since they can't contain any Vars of the current query level.
	 */
	if (root->hasJoinRTEs &&
		!(kind == EXPRKIND_RTFUNC || kind == EXPRKIND_VALUES))
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
	 * Note: an essential effect of this is to convert named-argument function
	 * calls to positional notation and insert the current actual values of
	 * any default arguments for functions.  To ensure that happens, we *must*
	 * process all expressions here.  Previous PG versions sometimes skipped
	 * const-simplification if it didn't seem worth the trouble, but we can't
	 * do that anymore.
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
		expr = (Node *) canonicalize_qual_ext((Expr *) expr, false);

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
 * preprocess_phv_expression
 *	  Do preprocessing on a PlaceHolderVar expression that's been pulled up.
 *
 * If a LATERAL subquery references an output of another subquery, and that
 * output must be wrapped in a PlaceHolderVar because of an intermediate outer
 * join, then we'll push the PlaceHolderVar expression down into the subquery
 * and later pull it back up during find_lateral_references, which runs after
 * subquery_planner has preprocessed all the expressions that were in the
 * current query level to start with.  So we need to preprocess it then.
 */
Expr *
preprocess_phv_expression(PlannerInfo *root, Expr *expr)
{
	return (Expr *) preprocess_expression(root, (Node *) expr, EXPRKIND_PHV);
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
 * different targetlist matching its own column set.  Fortunately,
 * the UPDATE/DELETE target can never be the nullable side of an outer join,
 * so it's OK to generate the plan this way.
 *
 * Returns a query plan.
 */
static Plan *
inheritance_planner(PlannerInfo *root)
{
	Query	   *parse = root->parse;
	int			parentRTindex = parse->resultRelation;
	Bitmapset  *subqueryRTindexes;
	Bitmapset  *modifiableARIindexes;
	List	   *final_rtable = NIL;
	int			save_rel_array_size = 0;
	RelOptInfo **save_rel_array = NULL;
	List	   *subplans = NIL;
	List	   *resultRelations = NIL;
	List	   *withCheckOptionLists = NIL;
	List	   *is_split_updates = NIL;
	List	   *returningLists = NIL;
	List	   *rowMarks;
	ListCell   *lc;
	Index		rti;

	GpPolicy   *parentPolicy = NULL;
	Oid			parentOid = InvalidOid;

	/* MPP */
	CdbLocusType append_locustype = CdbLocusType_Null;
	bool		locus_ok = TRUE;

	/*
	 * We generate a modified instance of the original Query for each target
	 * relation, plan that, and put all the plans into a list that will be
	 * controlled by a single ModifyTable node.  All the instances share the
	 * same rangetable, but each instance must have its own set of subquery
	 * RTEs within the finished rangetable because (1) they are likely to get
	 * scribbled on during planning, and (2) it's not inconceivable that
	 * subqueries could get planned differently in different cases.  We need
	 * not create duplicate copies of other RTE kinds, in particular not the
	 * target relations, because they don't have either of those issues.  Not
	 * having to duplicate the target relations is important because doing so
	 * (1) would result in a rangetable of length O(N^2) for N targets, with
	 * at least O(N^3) work expended here; and (2) would greatly complicate
	 * management of the rowMarks list.
	 *
	 * To begin with, generate a bitmapset of the relids of the subquery RTEs.
	 */
	subqueryRTindexes = NULL;
	rti = 1;
	foreach(lc, parse->rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(lc);

		if (rte->rtekind == RTE_SUBQUERY || rte->rtekind == RTE_CTE)
			subqueryRTindexes = bms_add_member(subqueryRTindexes, rti);
		rti++;
	}

	/*
	 * Next, we want to identify which AppendRelInfo items contain references
	 * to any of the aforesaid subquery RTEs.  These items will need to be
	 * copied and modified to adjust their subquery references; whereas the
	 * other ones need not be touched.  It's worth being tense over this
	 * because we can usually avoid processing most of the AppendRelInfo
	 * items, thereby saving O(N^2) space and time when the target is a large
	 * inheritance tree.  We can identify AppendRelInfo items by their
	 * child_relid, since that should be unique within the list.
	 */
	modifiableARIindexes = NULL;
	if (subqueryRTindexes != NULL)
	{
		foreach(lc, root->append_rel_list)
		{
			AppendRelInfo *appinfo = (AppendRelInfo *) lfirst(lc);

			if (bms_is_member(appinfo->parent_relid, subqueryRTindexes) ||
				bms_is_member(appinfo->child_relid, subqueryRTindexes) ||
				bms_overlap(pull_varnos((Node *) appinfo->translated_vars),
							subqueryRTindexes))
				modifiableARIindexes = bms_add_member(modifiableARIindexes,
													  appinfo->child_relid);
		}
	}

	/*
	 * And now we can get on with generating a plan for each child table.
	 */
	foreach(lc, root->append_rel_list)
	{
		AppendRelInfo *appinfo = (AppendRelInfo *) lfirst(lc);
		PlannerInfo subroot;
		Plan	   *subplan;

		/* append_rel_list contains all append rels; ignore others */
		if (appinfo->parent_relid != parentRTindex)
			continue;

		if (!parentPolicy)
		{
			parentPolicy = GpPolicyFetch(appinfo->parent_reloid);
			parentOid = appinfo->parent_reloid;

			Assert(parentPolicy != NULL);
			Assert(parentOid != InvalidOid);
		}

		Assert(parentOid == appinfo->parent_reloid);

		/*
		 * We need a working copy of the PlannerInfo so that we can control
		 * propagation of information back to the main copy.
		 */
		memcpy(&subroot, root, sizeof(PlannerInfo));

		/*
		 * Generate modified query with this rel as target.  We first apply
		 * adjust_appendrel_attrs, which copies the Query and changes
		 * references to the parent RTE to refer to the current child RTE,
		 * then fool around with subquery RTEs.
		 */
		subroot.parse = (Query *)
			adjust_appendrel_attrs(&subroot, (Node *) parse,
								   appinfo);

		/*
		 * The rowMarks list might contain references to subquery RTEs, so
		 * make a copy that we can apply ChangeVarNodes to.  (Fortunately, the
		 * executor doesn't need to see the modified copies --- we can just
		 * pass it the original rowMarks list.)
		 */
		subroot.rowMarks = (List *) copyObject(root->rowMarks);

		/*
		 * The append_rel_list likewise might contain references to subquery
		 * RTEs (if any subqueries were flattenable UNION ALLs).  So prepare
		 * to apply ChangeVarNodes to that, too.  As explained above, we only
		 * want to copy items that actually contain such references; the rest
		 * can just get linked into the subroot's append_rel_list.
		 *
		 * If we know there are no such references, we can just use the outer
		 * append_rel_list unmodified.
		 */
		if (modifiableARIindexes != NULL)
		{
			ListCell   *lc2;

			subroot.append_rel_list = NIL;
			foreach(lc2, root->append_rel_list)
			{
				AppendRelInfo *appinfo2 = (AppendRelInfo *) lfirst(lc2);

				if (bms_is_member(appinfo2->child_relid, modifiableARIindexes))
					appinfo2 = (AppendRelInfo *) copyObject(appinfo2);

				subroot.append_rel_list = lappend(subroot.append_rel_list,
												  appinfo2);
			}
		}

		/*
		 * Add placeholders to the child Query's rangetable list to fill the
		 * RT indexes already reserved for subqueries in previous children.
		 * These won't be referenced, so there's no need to make them very
		 * valid-looking.
		 */
		while (list_length(subroot.parse->rtable) < list_length(final_rtable))
			subroot.parse->rtable = lappend(subroot.parse->rtable,
											makeNode(RangeTblEntry));

		/*
		 * If this isn't the first child Query, generate duplicates of all
		 * subquery RTEs, and adjust Var numbering to reference the
		 * duplicates. To simplify the loop logic, we scan the original rtable
		 * not the copy just made by adjust_appendrel_attrs; that should be OK
		 * since subquery RTEs couldn't contain any references to the target
		 * rel.
		 */
		if (final_rtable != NIL && subqueryRTindexes != NULL)
		{
			ListCell   *lr;

			rti = 1;
			foreach(lr, parse->rtable)
			{
				RangeTblEntry *rte = (RangeTblEntry *) lfirst(lr);

				/*
				 * In GPDB CTEs are treated much more like subqueries than in
				 * upstream. As a result, we will generally plan a separate
				 * SubqueryScan for each CTE. Unlike in upstream, we will insert
				 * the plan for a subquery scan into the RelOptInfo for the
				 * CTE. As of b3aaf9081a1a95c245fd605dcf02c91b3a5c3a29, we
				 * expect that every SubqueryScan node that references the
				 * global RangeTable will match the corresponding entry in the
				 * rte.
				 * GPDB_92_MERGE_FIXME: Is treating CTE references like
				 * SubQueries sufficient here? Do we lose an opportunity to use
				 * shared scan here? Is there a way to treat it similarly with
				 * gp_cte_sharing turned on?
				 */
				if (bms_is_member(rti, subqueryRTindexes))
				{
					Index		newrti;

					/*
					 * The RTE can't contain any references to its own RT
					 * index, so we can save a few cycles by applying
					 * ChangeVarNodes before we append the RTE to the
					 * rangetable.
					 */
					newrti = list_length(subroot.parse->rtable) + 1;
					ChangeVarNodes((Node *) subroot.parse, rti, newrti, 0);
					ChangeVarNodes((Node *) subroot.rowMarks, rti, newrti, 0);
					/* Skip processing unchanging parts of append_rel_list */
					if (modifiableARIindexes != NULL)
					{
						ListCell   *lc2;

						foreach(lc2, subroot.append_rel_list)
						{
							AppendRelInfo *appinfo2 = (AppendRelInfo *) lfirst(lc2);

							if (bms_is_member(appinfo2->child_relid,
											  modifiableARIindexes))
								ChangeVarNodes((Node *) appinfo2, rti, newrti, 0);
						}
					}
					rte = copyObject(rte);
					subroot.parse->rtable = lappend(subroot.parse->rtable,
													rte);
				}
				rti++;
			}
		}

		/* There shouldn't be any OJ or LATERAL info to translate, as yet */
		Assert(subroot.join_info_list == NIL);
		Assert(subroot.lateral_info_list == NIL);
		/* and we haven't created PlaceHolderInfos, either */
		Assert(subroot.placeholder_list == NIL);
		/* hack to mark target relation as an inheritance partition */
		subroot.hasInheritedTarget = true;

		/* Generate plan */
		subplan = grouping_planner(&subroot, 0.0 /* retrieve all tuples */ );

		/*
		 * Planning may have modified the query result relation (if there were
		 * security barrier quals on the result RTE).
		 */
		appinfo->child_relid = subroot.parse->resultRelation;

		/*
		 * If this child rel was excluded by constraint exclusion, exclude it
		 * from the result plan.
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
						/* FIXME: is HashedOJ really OK here? */
						/* MPP-2023: Among subplans, these loci are okay. */
						break;
					case CdbLocusType_SegmentGeneral:
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
								errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("incompatible loci in target inheritance set")));
			}
		}

		subplans = lappend(subplans, subplan);

		/*
		 * If this is the first non-excluded child, its post-planning rtable
		 * becomes the initial contents of final_rtable; otherwise, append
		 * just its modified subquery RTEs to final_rtable.
		 */
		if (final_rtable == NIL)
			final_rtable = subroot.parse->rtable;
		else
		{
			List	   *tmp_rtable = NIL;
			ListCell   *cell1,
					   *cell2;

			/*
			 * Check to see if any of the original RTEs were turned into
			 * subqueries during planning.  Currently, this should only ever
			 * happen due to securityQuals being involved which push a
			 * relation down under a subquery, to ensure that the security
			 * barrier quals are evaluated first.
			 *
			 * When this happens, we want to use the new subqueries in the
			 * final rtable.
			 */
			forboth(cell1, final_rtable, cell2, subroot.parse->rtable)
			{
				RangeTblEntry *rte1 = (RangeTblEntry *) lfirst(cell1);
				RangeTblEntry *rte2 = (RangeTblEntry *) lfirst(cell2);

				if (rte1->rtekind == RTE_RELATION &&
					rte2->rtekind == RTE_SUBQUERY)
				{
					/* Should only be when there are securityQuals today */
					Assert(rte1->securityQuals != NIL);
					tmp_rtable = lappend(tmp_rtable, rte2);
				}
				else
					tmp_rtable = lappend(tmp_rtable, rte1);
			}

			final_rtable = list_concat(tmp_rtable,
									   list_copy_tail(subroot.parse->rtable,
												 list_length(final_rtable)));
		}

		/*
		 * We need to collect all the RelOptInfos from all child plans into
		 * the main PlannerInfo, since setrefs.c will need them.  We use the
		 * last child's simple_rel_array (previous ones are too short), so we
		 * have to propagate forward the RelOptInfos that were already built
		 * in previous children.
		 */
		Assert(subroot.simple_rel_array_size >= save_rel_array_size);
		for (rti = 1; rti < save_rel_array_size; rti++)
		{
			RelOptInfo *brel = save_rel_array[rti];

			if (brel)
				subroot.simple_rel_array[rti] = brel;
		}
		save_rel_array_size = subroot.simple_rel_array_size;
		save_rel_array = subroot.simple_rel_array;

		/* Make sure any initplans from this rel get into the outer list */
		root->init_plans = subroot.init_plans;

		/* Build list of target-relation RT indexes */
		resultRelations = lappend_int(resultRelations, appinfo->child_relid);

		/* Build lists of per-relation WCO and RETURNING targetlists */
		if (parse->withCheckOptions)
			withCheckOptionLists = lappend(withCheckOptionLists,
										   subroot.parse->withCheckOptions);
		if (parse->returningList)
			returningLists = lappend(returningLists,
									 subroot.parse->returningList);

		/*
		 * If this subplan requires a Split Update, pass that information
		 * back to the top.
		 */
		is_split_updates = lappend_int(is_split_updates, subroot.is_split_update);
	}

	Assert(parentPolicy != NULL);

	/* Mark result as unordered (probably unnecessary) */
	root->query_pathkeys = NIL;

	if (subplans == NIL)
	{
		/*
		 * We managed to exclude every child rel, so generate a dummy path
		 * representing the empty set.  Although it's clear that no data will
		 * be updated or deleted, we will still need to have a ModifyTable
		 * node so that any statement triggers are executed.  (This could be
		 * cleaner if we fixed nodeModifyTable.c to support zero child nodes,
		 * but that probably wouldn't be a net win.)
		 */
		List	   *tlist;
		Plan	   *subplan;

		/* tlist processing never got done, either */
		tlist = preprocess_targetlist(root, parse->targetList);

		subplan = (Plan *) make_result(root,
									   tlist,
									 (Node *) list_make1(makeBoolConst(false,
																	 false)),
									   NULL);

		/*
		 * Attach flow to the dummy Result node, so as to avoid an assertion
		 * failure.  See GitHub issue #1433.
		 */
		if (Gp_role == GP_ROLE_DISPATCH)
			mark_plan_general(subplan, parentPolicy->numsegments);

		/* These lists must be nonempty to make a valid ModifyTable node */
		subplans = list_make1(subplan);
		resultRelations = list_make1_int(parse->resultRelation);
		if (parse->withCheckOptions)
			withCheckOptionLists = list_make1(parse->withCheckOptions);
		if (parse->returningList)
			returningLists = list_make1(parse->returningList);

		/* for a dummy subplan, set is_split_update to false */
		is_split_updates = lappend_int(is_split_updates, false);
	}
	else
	{
		/*
		 * Put back the final adjusted rtable into the master copy of the
		 * Query.  (We mustn't do this if we found no non-excluded children,
		 * since we never saved an adjusted rtable at all.)
		 */
		parse->rtable = final_rtable;
		root->simple_rel_array_size = save_rel_array_size;
		root->simple_rel_array = save_rel_array;
	}

	/*
	 * If there was a FOR [KEY] UPDATE/SHARE clause, the LockRows node will
	 * have dealt with fetching non-locked marked rows, else we need to have
	 * ModifyTable do that.
	 */
	if (parse->rowMarks)
		rowMarks = NIL;
	else
		rowMarks = root->rowMarks;

	/* And last, tack on a ModifyTable node to do the UPDATE/DELETE work */
	return (Plan *) make_modifytable(root,
									 parse->commandType,
									 parse->canSetTag,
									 resultRelations,
									 subplans,
									 withCheckOptionLists,
									 returningLists,
									 is_split_updates,
									 rowMarks,
									 SS_assign_special_param(root));
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

		/*
		 * GPDB_92_MERGE_FIXME: In PG 9.2, there is a new varno 'INDEX_VAR'.
		 * GPDB codes should revise to work with the new varno.
		 */
		Assert(var->varlevelsup == 0 && "Plan contains vars that refer to outer plan.");
		Assert((var->varno == OUTER_VAR || var->varno == INDEX_VAR
		|| (var->varno > 0 && var->varno <= list_length(root->parse->rtable)))
			   && "Plan contains var that refer outside the rtable.");

#if 0
		Assert(var->varno == var->varnoold && "Varno and varnoold do not agree!");
#endif

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
	else if (IsA(node, ModifyTable))
	{
		ModifyTable *mt = (ModifyTable *) node;

		if (!mt->plans)
			elog(ERROR, "ModifyTable has no child plans");

		return (Plan *) linitial(mt->plans);
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
	bool		use_hashed_distinct = false;
	bool		tested_hashed_distinct = false;
	double		numDistinct = 1;
	List	   *distinctExprs = NIL;
	List	   *distinct_dist_pathkeys = NIL;
	List	   *distinct_dist_exprs = NIL;
	List	   *distinct_dist_opfamilies = NIL;
	bool		must_gather;

	double		motion_cost_per_row =
	(gp_motion_cost_per_row > 0.0) ?
	gp_motion_cost_per_row :
	2.0 * cpu_tuple_cost;

	/*
	 * If limit clause contains volatile functions, they should be
	 * evaluated only once. For such cases, we should not push down
	 * the limit.
	 *
	 * Words on multi-stage limit: current interconnect implementation
	 * model is sender will send when buffer is full. Under such
	 * condition, multi-stage limit might improve performance for
	 * some cases.
	 *
	 * TODO: we might investigate that evaluating limit clause first,
	 * and then doing pushdown it in future.
	 */
	bool        limit_contain_volatile_functions;
	limit_contain_volatile_functions = (contain_volatile_functions(parse->limitCount)
										|| contain_volatile_functions(parse->limitOffset));


	CdbPathLocus_MakeNull(&current_locus, GP_POLICY_INVALID_NUMSEGMENTS());

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
		 * tuples.  This might be too simplistic given all the hackery below
		 * to possibly avoid the sort; but the odds of accurate estimates here
		 * are pretty low anyway.
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
													result_plan->targetlist);

		/*
		 * We should not need to call preprocess_targetlist, since we must be
		 * in a SELECT query node.  Instead, use the targetlist returned by
		 * plan_set_operations (since this tells whether it returned any
		 * resjunk columns!), and transfer any sort key information from the
		 * original tlist.
		 */
		Assert(parse->commandType == CMD_SELECT);

		tlist = postprocess_setop_tlist(copyObject(result_plan->targetlist),
										tlist);

		/*
		 * Can't handle FOR [KEY] UPDATE/SHARE here (parser should have
		 * checked already, but let's make sure).
		 */
		if (parse->rowMarks)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			/*------
			  translator: %s is a SQL row locking clause such as FOR UPDATE */
					 errmsg("%s is not allowed with UNION/INTERSECT/EXCEPT",
							LCS_asString(((RowMarkClause *)
									linitial(parse->rowMarks))->strength))));

		/*
		 * Calculate pathkeys that represent result ordering requirements
		 */
		Assert(parse->distinctClause == NIL);
		root->sort_pathkeys = make_pathkeys_for_sortclauses(root,
															parse->sortClause,
															tlist);
	}
	else
	{
		/* No set operations, do regular planning */
		List	   *sub_tlist;
		AttrNumber *groupColIdx = NULL;
		Oid		   *groupOperators = NULL;
		bool		need_tlist_eval = true;
		standard_qp_extra qp_extra;
		RelOptInfo *final_rel;
		Path	   *cheapest_path;
		Path	   *sorted_path;
		long		numGroups = 0;
		AggClauseCosts agg_costs;
		int			numGroupCols;
		double		path_rows;
		int			path_width;
		bool		use_hashed_grouping = false;
		WindowFuncLists *wflists = NULL;
		List	   *activeWindows = NIL;
		bool		grpext = false;
		CanonicalGroupingSets *canonical_grpsets;

		MemSet(&agg_costs, 0, sizeof(AggClauseCosts));

		/* A recursive query should always have setOperations */
		Assert(!root->hasRecursion);

		/* Preprocess GROUP BY clause, if any */
		if (parse->groupClause)
			preprocess_groupclause(root);
		numGroupCols = list_length(parse->groupClause);

		/* Preprocess targetlist */
		tlist = preprocess_targetlist(root, tlist);

		/*
		 * Expand any rangetable entries that have security barrier quals.
		 * This may add new security barrier subquery RTEs to the rangetable.
		 */
		expand_security_quals(root, tlist);

		/*
		 * Locate any window functions in the tlist.  (We don't need to look
		 * anywhere else, since expressions used in ORDER BY will be in there
		 * too.)  Note that they could all have been eliminated by constant
		 * folding, in which case we don't need to do any more work.
		 */
		if (parse->hasWindowFuncs)
		{
			wflists = find_window_functions((Node *) tlist,
											list_length(parse->windowClause));
			if (wflists->numWindowFuncs > 0)
				activeWindows = select_active_windows(root, wflists);
			else
				parse->hasWindowFuncs = false;
		}

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

		/*
		 * Generate appropriate target list for subplan; may be different from
		 * tlist if grouping or aggregation is needed.
		 */
		sub_tlist = make_subplanTargetList(root, tlist,
										   &groupColIdx, &groupOperators, &need_tlist_eval);

		/*
		 * Do aggregate preprocessing, if the query has any aggs.
		 *
		 * Note: think not that we can turn off hasAggs if we find no aggs. It
		 * is possible for constant-expression simplification to remove all
		 * explicit references to aggs, but we still have to follow the
		 * aggregate semantics (eg, producing only one output row).
		 */
		if (parse->hasAggs)
		{
			/*
			 * Collect statistics about aggregates for estimating costs. Note:
			 * we do not attempt to detect duplicate aggregates here; a
			 * somewhat-overestimated cost is okay for our present purposes.
			 */
			count_agg_clauses(root, (Node *) tlist, &agg_costs);
			count_agg_clauses(root, parse->havingQual, &agg_costs);

			/*
			 * Preprocess MIN/MAX aggregates, if any.  Note: be careful about
			 * adding logic between here and the optimize_minmax_aggregates
			 * call.  Anything that is needed in MIN/MAX-optimizable cases
			 * will have to be duplicated in planagg.c.
			 */
			preprocess_minmax_aggregates(root, tlist);
		}

		/* Make tuple_fraction accessible to lower-level routines */
		root->tuple_fraction = tuple_fraction;

		/*
		 * Figure out whether there's a hard limit on the number of rows that
		 * query_planner's result subplan needs to return.  Even if we know a
		 * hard limit overall, it doesn't apply if the query has any
		 * grouping/aggregation operations.
		 */
		if (parse->groupClause ||
			parse->distinctClause ||
			parse->hasAggs ||
			parse->hasWindowFuncs ||
			root->hasHavingQual)
			root->limit_tuples = -1.0;
		else
			root->limit_tuples = limit_tuples;

		/* Set up data needed by standard_qp_callback */
		qp_extra.tlist = tlist;
		qp_extra.activeWindows = activeWindows;

		/*
		 * Generate the best unsorted and presorted paths for this Query (but
		 * note there may not be any presorted paths).  We also generate (in
		 * standard_qp_callback) pathkey representations of the query's sort
		 * clause, distinct clause, etc.
		 */
		final_rel = query_planner(root, sub_tlist,
								  standard_qp_callback, &qp_extra);

		/*
		 * Extract rowcount and width estimates for use below.  If final_rel
		 * has been proven dummy, its rows estimate will be zero; clamp it to
		 * one to avoid zero-divide in subsequent calculations.
		 */
		path_rows = clamp_row_est(final_rel->rows);
		path_width = final_rel->width;

		/*
		 * If there's grouping going on, estimate the number of result groups.
		 * We couldn't do this any earlier because it depends on relation size
		 * estimates that are created within query_planner().
		 *
		 * Then convert tuple_fraction to fractional form if it is absolute,
		 * and if grouping or aggregation is involved, adjust tuple_fraction
		 * to describe the fraction of the underlying un-aggregated tuples
		 * that will be fetched.
		 */
		dNumGroups = 1;			/* in case not grouping */

		if (parse->groupClause)
		{
			List	   *groupExprs;

			groupExprs = get_grouplist_exprs(parse->groupClause,
											 parse->targetList);
			if (groupExprs == NULL)
				dNumGroups = 1;
			else
				dNumGroups = estimate_num_groups(root, groupExprs, path_rows);

			/*
			 * In GROUP BY mode, an absolute LIMIT is relative to the number
			 * of groups not the number of tuples.  If the caller gave us a
			 * fraction, keep it as-is.  (In both cases, we are effectively
			 * assuming that all the groups are about the same size.)
			 */
			if (tuple_fraction >= 1.0)
				tuple_fraction /= dNumGroups;

			/*
			 * If both GROUP BY and ORDER BY are specified, we will need two
			 * levels of sort --- and, therefore, certainly need to read all
			 * the tuples --- unless ORDER BY is a subset of GROUP BY.
			 * Likewise if we have both DISTINCT and GROUP BY, or if we have a
			 * window specification not compatible with the GROUP BY.
			 */
			if (!pathkeys_contained_in(root->sort_pathkeys,
									   root->group_pathkeys) ||
				!pathkeys_contained_in(root->distinct_pathkeys,
									   root->group_pathkeys) ||
				!pathkeys_contained_in(root->window_pathkeys,
									   root->group_pathkeys))
				tuple_fraction = 0.0;
		}
		else if (parse->hasAggs || root->hasHavingQual)
		{
			/*
			 * Ungrouped aggregate will certainly want to read all the tuples,
			 * and it will deliver a single result row (so leave dNumGroups
			 * set to 1).
			 */
			tuple_fraction = 0.0;
		}
		else if (parse->distinctClause)
		{
			/*
			 * Since there was no grouping or aggregation, it's reasonable to
			 * assume the UNIQUE filter has effects comparable to GROUP BY.
			 * (If DISTINCT is used with grouping, we ignore its effects for
			 * rowcount estimation purposes; this amounts to assuming the
			 * grouped rows are distinct already.)
			 */
			List	   *distinctExprs;

			distinctExprs = get_sortgrouplist_exprs(parse->distinctClause,
													parse->targetList);
			dNumGroups = estimate_num_groups(root, distinctExprs, path_rows);

			/*
			 * Adjust tuple_fraction the same way as for GROUP BY, too.
			 */
			if (tuple_fraction >= 1.0)
				tuple_fraction /= dNumGroups;
		}
		else
		{
			/*
			 * Plain non-grouped, non-aggregated query: an absolute tuple
			 * fraction can be divided by the number of tuples.
			 */
			if (tuple_fraction >= 1.0)
				tuple_fraction /= path_rows;
		}

		/*
		 * Pick out the cheapest-total path as well as the cheapest presorted
		 * path for the requested pathkeys (if there is one).  We should take
		 * the tuple fraction into account when selecting the cheapest
		 * presorted path, but not when selecting the cheapest-total path,
		 * since if we have to sort then we'll have to fetch all the tuples.
		 * (But there's a special case: if query_pathkeys is NIL, meaning
		 * order doesn't matter, then the "cheapest presorted" path will be
		 * the cheapest overall for the tuple fraction.)
		 */
		cheapest_path = final_rel->cheapest_total_path;

		sorted_path =
			get_cheapest_fractional_path_for_pathkeys(final_rel->pathlist,
													  root->query_pathkeys,
													  NULL,
													  tuple_fraction);

		/* Don't consider same path in both guises; just wastes effort */
		if (sorted_path == cheapest_path)
			sorted_path = NULL;

		/*
		 * Forget about the presorted path if it would be cheaper to sort the
		 * cheapest-total path.  Here we need consider only the behavior at
		 * the tuple_fraction point.  Also, limit_tuples is only relevant if
		 * not grouping/aggregating, so use root->limit_tuples in the
		 * cost_sort call.
		 */
		if (sorted_path)
		{
			Path		sort_path;		/* dummy for result of cost_sort */

			if (root->query_pathkeys == NIL ||
				pathkeys_contained_in(root->query_pathkeys,
									  cheapest_path->pathkeys))
			{
				/* No sort needed for cheapest path */
				sort_path.startup_cost = cheapest_path->startup_cost;
				sort_path.total_cost = cheapest_path->total_cost;
			}
			else
			{
				/* Figure cost for sorting */
				cost_sort(&sort_path, root, root->query_pathkeys,
						  cheapest_path->total_cost,
						  path_rows, path_width,
						  0.0, work_mem, root->limit_tuples);
			}

			if (compare_fractional_path_costs(sorted_path, &sort_path,
											  tuple_fraction) > 0)
			{
				/* Presorted path is a loser */
				sorted_path = NULL;
			}
		}

		/*
		 * Consider whether we want to use hashing instead of sorting.
		 */
		if (parse->groupClause)
		{
			/*
			 * If grouping, decide whether to use sorted or hashed grouping.
			 */
			use_hashed_grouping =
				choose_hashed_grouping(root,
									   tuple_fraction, limit_tuples,
									   path_rows, path_width,
									   cheapest_path, sorted_path,
									   numGroupCols,
									   dNumGroups, &agg_costs);
			/* Also convert # groups to long int --- but 'ware overflow! */
			numGroups = (long) Min(dNumGroups, (double) LONG_MAX);
		}
		else if (parse->distinctClause && sorted_path &&
				 !root->hasHavingQual && !parse->hasAggs && !activeWindows)
		{
			/*
			 * We'll reach the DISTINCT stage without any intermediate
			 * processing, so figure out whether we will want to hash or not
			 * so we can choose whether to use cheapest or sorted path.
			 */
			use_hashed_distinct =
				choose_hashed_distinct(root,
									   tuple_fraction, limit_tuples,
									   path_rows, path_width,
									   cheapest_path->startup_cost,
									   cheapest_path->total_cost,
									   sorted_path->startup_cost,
									   sorted_path->total_cost,
									   sorted_path->pathkeys,
									   dNumGroups);
			tested_hashed_distinct = true;
		}

		/*
		 * Select the best path.  If we are doing hashed grouping, we will
		 * always read all the input tuples, so use the cheapest-total path.
		 * Otherwise, the comparison above is correct.
		 */
		if (use_hashed_grouping || use_hashed_distinct || !sorted_path)
			best_path = cheapest_path;
		else
			best_path = sorted_path;

		/*
		 * Check to see if it's possible to optimize MIN/MAX aggregates. If
		 * so, we will forget all the work we did so far to choose a "regular"
		 * path ... but we had to do it anyway to be able to tell which way is
		 * cheaper.
		 */
		result_plan = optimize_minmax_aggregates(root,
												 tlist,
												 &agg_costs,
												 best_path);
		if (result_plan != NULL)
		{
			/*
			 * optimize_minmax_aggregates generated the full plan, with the
			 * right tlist, and it has no sort order.
			 */
			current_pathkeys = NIL;
			mark_plan_entry(result_plan);
		}

		/*
		 * CDB:  For now, we either - construct a general parallel plan, - let
		 * the sequential planner handle the situation, or - construct a
		 * sequential plan using the min-max index optimization.
		 *
		 * Eventually we should add a parallel version of the min-max
		 * optimization.  For now, it's either-or.
		 */
		if (Gp_role == GP_ROLE_DISPATCH && result_plan == NULL)
		{
			bool		querynode_changed = false;
			bool		pass_subtlist = agg_costs.numOrderedAggs > 0;
			GroupContext group_context;

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

			result_plan = cdb_grouping_planner(root,
											   &agg_costs,
											   &group_context);

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
													  result_plan->targetlist);
				if (parse->sortClause)
					root->sort_pathkeys =
						make_pathkeys_for_sortclauses(root,
													  parse->sortClause,
													  result_plan->targetlist);
			}
		}

		if (result_plan == NULL)	/* Not GP_ROLE_DISPATCH */
		{
			/*
			 * Normal case --- create a plan according to query_planner's
			 * results.
			 */
			bool		need_sort_for_grouping = false;

			result_plan = create_plan(root, best_path);
			current_pathkeys = best_path->pathkeys;
			current_locus = best_path->locus;	/* just use keys, don't copy */

			/*
			 * The returned plan might be ordered by TLEs that we don't need
			 * in the final result, and will therefore not be present in the
			 * final target list. Also remove them from current_pathkeys, so
			 * that current_pathkeys only contains expressions that can be
			 * evaluated using the new target list. This is not required in
			 * PostgreSQL, because in PostgreSQL current_pathkeys is only
			 * compared against, and there's no need to re-evaluate it. But
			 * in GPDB, we might use current_pathkeys to maintain the order
			 * in a Motion node that we create, so we must be able to
			 * evaluate it.
			 */
			current_pathkeys =
				cdbpullup_truncatePathKeysForTargetList(current_pathkeys,
														sub_tlist);

			/* Detect if we'll need an explicit sort for grouping */
			if (parse->groupClause && !use_hashed_grouping &&
			  !pathkeys_contained_in(root->group_pathkeys, current_pathkeys))
			{
				need_sort_for_grouping = true;

				/*
				 * Always override create_plan's tlist, so that we don't sort
				 * useless data from a "physical" tlist.
				 */
				need_tlist_eval = true;
			}

			/*
			 * create_plan returns a plan with just a "flat" tlist of required
			 * Vars.  Usually we need to insert the sub_tlist as the tlist of
			 * the top plan node.  However, we can skip that if we determined
			 * that whatever create_plan chose to return will be good enough.
			 */
			if (need_tlist_eval)
			{
				/*
				 * If the top-level plan node is one that cannot do expression
				 * evaluation and its existing target list isn't already what
				 * we need, we must insert a Result node to project the
				 * desired tlist.
				 */
				result_plan = plan_pushdown_tlist(root, result_plan, sub_tlist);

				/*
				 * Also, account for the cost of evaluation of the sub_tlist.
				 * See comments for add_tlist_costs_to_plan() for more info.
				 */
				add_tlist_costs_to_plan(root, result_plan, sub_tlist);
			}
			else
			{
				/*
				 * Since we're using create_plan's tlist and not the one
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
												AGG_HASHED,
												&agg_costs,
												false, /* streaming */
												numGroupCols,
												groupColIdx,
												groupOperators,
												numGroups,
												0, /* num_nullcols */
												0, /* input_grouping */
												0, /* grouping */
												0, /* rollup_gs_times */
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
				CdbPathLocus_MakeNull(&current_locus, GP_POLICY_INVALID_NUMSEGMENTS());
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
												aggstrategy,
												&agg_costs,
												false, /* streaming */
												numGroupCols,
												groupColIdx,
												groupOperators,
												numGroups,
												0, /* num_nullcols */
												0, /* input_grouping */
												0, /* grouping */
												0, /* rollup_gs_times */
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

				CdbPathLocus_MakeNull(&current_locus, GP_POLICY_INVALID_NUMSEGMENTS());
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
													  false,
													  (List *) parse->havingQual,
													  &numGroupCols,
													  &groupColIdx,
													  &groupOperators,
													  &agg_costs,
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
														  result_plan->targetlist);
					if (parse->sortClause)
						root->sort_pathkeys =
							make_pathkeys_for_sortclauses(root,
														  parse->sortClause,
														  result_plan->targetlist);
					CdbPathLocus_MakeNull(&current_locus, GP_POLICY_INVALID_NUMSEGMENTS());
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
				 * generate a Result node.  This is a sufficiently unusual
				 * corner case that it's not worth contorting the structure of
				 * this routine to avoid having to generate the plan in the
				 * first place.
				 */
				/* FIXME: numsegments, is policy needed? */

				result_plan = (Plan *) make_result(root,
												   tlist,
												   parse->havingQual,
												   NULL);
				/* Result will be only one row anyway; no sort order */
				current_pathkeys = NIL;
				mark_plan_general(result_plan, getgpsegmentCount());
				CdbPathLocus_MakeNull(&current_locus, GP_POLICY_INVALID_NUMSEGMENTS());
			}
		}						/* end of non-minmax-aggregate case */

		/*
		 * Since each window function could require a different sort order, we
		 * stack up a WindowAgg node for each window, with sort steps between
		 * them as needed.
		 */
		if (activeWindows)
		{
			List	   *window_tlist;
			ListCell   *l;

			/*
			 * If the top-level plan node is one that cannot do expression
			 * evaluation, we must insert a Result node to project the desired
			 * tlist.  (In some cases this might not really be required, but
			 * it's not worth trying to avoid it.  In particular, think not to
			 * skip adding the Result if the initial window_tlist matches the
			 * top-level plan node's output, because we might change the tlist
			 * inside the following loop.)	Note that on second and subsequent
			 * passes through the following loop, the top-level node will be a
			 * WindowAgg which we know can project; so we only need to check
			 * once.
			 */
			if (!is_projection_capable_plan(result_plan))
			{
				result_plan = (Plan *) make_result(root,
												   NIL,
												   NULL,
												   result_plan);

				result_plan->flow = pull_up_Flow(result_plan,
												 getAnySubplan(result_plan));
			}

			/*
			 * The "base" targetlist for all steps of the windowing process is
			 * a flat tlist of all Vars and Aggs needed in the result.  (In
			 * some cases we wouldn't need to propagate all of these all the
			 * way to the top, since they might only be needed as inputs to
			 * WindowFuncs.  It's probably not worth trying to optimize that
			 * though.)  We also add window partitioning and sorting
			 * expressions to the base tlist, to ensure they're computed only
			 * once at the bottom of the stack (that's critical for volatile
			 * functions).  As we climb up the stack, we'll add outputs for
			 * the WindowFuncs computed at each level.
			 */
			window_tlist = make_windowInputTargetList(root,
													  tlist,
													  activeWindows);

			/* Add any Vars needed to compute the distribution key. */
			window_tlist = add_to_flat_tlist_junk(window_tlist,
												  result_plan->flow->hashExprs,
												  true /* resjunk */);

			/*
			 * The copyObject steps here are needed to ensure that each plan
			 * node has a separately modifiable tlist.  (XXX wouldn't a
			 * shallow list copy do for that?)
			 */
			result_plan->targetlist = (List *) copyObject(window_tlist);

			foreach(l, activeWindows)
			{
				WindowClause *wc = (WindowClause *) lfirst(l);
				List	   *window_pathkeys;
				int			partNumCols;
				AttrNumber *partColIdx;
				Oid		   *partOperators;
				int			ordNumCols;
				AttrNumber *ordColIdx;
				Oid		   *ordOperators;
				int			firstOrderCol = 0;
				Oid			firstOrderCmpOperator = InvalidOid;
				bool		firstOrderNullsFirst = false;
				bool		need_gather_for_partitioning;

				/*
				 * Unless the PARTITION BY in the window happens to match the
				 * current distribution, we need a motion. Each partition
				 * needs to be handled in the same segment.
				 *
				 * If there is no PARTITION BY, then all rows form a single
				 * partition, so we need to gather all the tuples to a single
				 * node. But we'll do that after the Sort, so that the Sort
				 * is parallelized.
				 */
				if (CdbPathLocus_IsGeneral(current_locus))
					need_gather_for_partitioning = false;
				else
				{
					List	   *partition_dist_pathkeys;
					List	   *partition_dist_exprs;
					List	   *partition_dist_opfamilies;

					make_distribution_exprs_for_groupclause(root,
															wc->partitionClause,
															tlist,
															&partition_dist_pathkeys,
															&partition_dist_exprs,
															&partition_dist_opfamilies);
					if (!partition_dist_exprs)
					{
						/*
						 * There is no PARTITION BY, or none of the PARTITION BY
						 * expressions can be used as a distribution key. Have to
						 * gather everything to a single node.
						 */
						need_gather_for_partitioning = true;
					}
					else if (cdbpathlocus_collocates_pathkeys(root, current_locus, partition_dist_pathkeys, false))
					{
						need_gather_for_partitioning = false;
					}
					else
					{
						result_plan = (Plan *) make_motion_hash(root, result_plan,
																partition_dist_exprs,
																partition_dist_opfamilies);
						result_plan->total_cost += motion_cost_per_row * result_plan->plan_rows;
						current_pathkeys = NIL; /* no longer sorted */
						Assert(result_plan->flow);

						/*
						 * Change current_locus based on the new distribution
						 * pathkeys.
						 */
						List *partition_dist_distkeys = NIL;
						ListCell *lc;
						ListCell *lc2;
						forboth(lc, partition_dist_pathkeys, lc2, partition_dist_opfamilies)
						{
							PathKey	   *pk = (PathKey *) lfirst(lc);
							Oid			opfamily = lfirst_oid(lc2);
							DistributionKey *dk = makeNode(DistributionKey);

							dk->dk_opfamily = opfamily;
							dk->dk_eclasses = list_make1(pk->pk_eclass);
							partition_dist_distkeys = lappend(partition_dist_distkeys, dk);
						}

						CdbPathLocus_MakeHashed(&current_locus, partition_dist_distkeys,
												CdbPathLocus_NumSegments(current_locus));
						need_gather_for_partitioning = false;
					}
				}

				window_pathkeys = make_pathkeys_for_window(root,
														   wc,
														   tlist);

				/*
				 * This is a bit tricky: we build a sort node even if we don't
				 * really have to sort.  Even when no explicit sort is needed,
				 * we need to have suitable resjunk items added to the input
				 * plan's tlist for any partitioning or ordering columns that
				 * aren't plain Vars.  (In theory, make_windowInputTargetList
				 * should have provided all such columns, but let's not assume
				 * that here.)	Furthermore, this way we can use existing
				 * infrastructure to identify which input columns are the
				 * interesting ones.
				 */
				if (window_pathkeys)
				{
					Sort	   *sort_plan;

					sort_plan = make_sort_from_pathkeys(root,
														result_plan,
														window_pathkeys,
														-1.0,
														true);
					if (!pathkeys_contained_in(window_pathkeys,
											   current_pathkeys))
					{
						/* we do indeed need to sort */
						result_plan = (Plan *) sort_plan;
						current_pathkeys = window_pathkeys;
						mark_sort_locus(result_plan);

						if (!result_plan->flow)
							result_plan->flow = pull_up_Flow(result_plan,
															 getAnySubplan(result_plan));
					}
					/* In either case, extract the per-column information */
					get_column_info_for_window(root, wc, tlist,
											   sort_plan->numCols,
											   sort_plan->sortColIdx,
											   &partNumCols,
											   &partColIdx,
											   &partOperators,
											   &ordNumCols,
											   &ordColIdx,
											   &ordOperators);
				}
				else
				{
					/* empty window specification, nothing to sort */
					partNumCols = 0;
					partColIdx = NULL;
					partOperators = NULL;
					ordNumCols = 0;
					ordColIdx = NULL;
					ordOperators = NULL;
				}

				if (wc->orderClause)
				{
					SortGroupClause *sortcl = (SortGroupClause *) linitial(wc->orderClause);
					ListCell	*l_tle;

					firstOrderCol = 0;
					foreach(l_tle, window_tlist)
					{
						TargetEntry *tle = (TargetEntry *) lfirst(l_tle);

						firstOrderCol++;
						if (sortcl->tleSortGroupRef == tle->ressortgroupref)
							break;
					}
					if (!l_tle)
						elog(ERROR, "failed to locate ORDER BY column");

					firstOrderCmpOperator = sortcl->sortop;
					firstOrderNullsFirst = sortcl->nulls_first;
				}

				/*
				 * If the input's locus doesn't match the PARTITION BY, gather the result.
				 */
				if (need_gather_for_partitioning &&
					!CdbPathLocus_IsGeneral(current_locus) &&
					result_plan->flow->flotype != FLOW_SINGLETON)
				{
					result_plan =
						(Plan *) make_motion_gather_to_QE(root, result_plan, current_pathkeys);
				}

				if (lnext(l))
				{
					/* Add the current WindowFuncs to the running tlist */
					window_tlist = add_to_flat_tlist(window_tlist,
										   wflists->windowFuncs[wc->winref]);
				}
				else
				{
					/* Install the original tlist in the topmost WindowAgg */
					window_tlist = tlist;
				}

				/* ... and make the WindowAgg plan node */
				result_plan = (Plan *)
					make_windowagg(root,
								   (List *) copyObject(window_tlist),
								   wflists->windowFuncs[wc->winref],
								   wc->winref,
								   partNumCols,
								   partColIdx,
								   partOperators,
								   ordNumCols,
								   ordColIdx,
								   ordOperators,
								   firstOrderCol,
								   firstOrderCmpOperator,
								   firstOrderNullsFirst,
								   wc->frameOptions,
								   wc->startOffset,
								   wc->endOffset,
								   result_plan);
			}
		}

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
		(root->config->honor_order_by || !root->parent_root) &&
		parse->parentStmtType == PARENTSTMTTYPE_NONE &&
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
		double		dNumDistinctRows;
		long		numDistinctRows;

		/*
		 * If there was grouping or aggregation, use the current number of
		 * rows as the estimated number of DISTINCT rows (ie, assume the
		 * result was already mostly unique).  If not, use the number of
		 * distinct-groups calculated previously.
		 */
		if (parse->groupClause || root->hasHavingQual || parse->hasAggs)
			dNumDistinctRows = result_plan->plan_rows;
		else
			dNumDistinctRows = dNumGroups;

		/* Also convert to long int --- but 'ware overflow! */
		numDistinctRows = (long) Min(dNumDistinctRows, (double) LONG_MAX);

		/* Choose implementation method if we didn't already */
		if (!tested_hashed_distinct)
		{
			/*
			 * At this point, either hashed or sorted grouping will have to
			 * work from result_plan, so we pass that as both "cheapest" and
			 * "sorted".
			 */
			use_hashed_distinct =
				choose_hashed_distinct(root,
									   tuple_fraction, limit_tuples,
									   result_plan->plan_rows,
									   result_plan->plan_width,
									   result_plan->startup_cost,
									   result_plan->total_cost,
									   result_plan->startup_cost,
									   result_plan->total_cost,
									   current_pathkeys,
									   dNumDistinctRows);
		}

		if (CdbPathLocus_IsNull(current_locus))
			current_locus = cdbpathlocus_from_flow(result_plan->flow);

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
		make_distribution_exprs_for_groupclause(root,
												parse->distinctClause,
												result_plan->targetlist,
												&distinct_dist_pathkeys,
												&distinct_dist_exprs,
												&distinct_dist_opfamilies);

		distinctExprs = get_sortgrouplist_exprs(parse->distinctClause,
												result_plan->targetlist);
		numDistinct = estimate_num_groups(root, distinctExprs,
										  result_plan->plan_rows);

		if (Gp_role == GP_ROLE_DISPATCH && CdbPathLocus_IsPartitioned(current_locus))
		{
			bool		needMotion;

			needMotion = !cdbpathlocus_collocates_pathkeys(root, current_locus,
														   distinct_dist_pathkeys, false /* exact_match */ );

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
						  numDistinct, result_plan->plan_rows,
						  0, work_mem, -1.0);
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
					if (parse->limitCount &&
						!limit_contain_volatile_functions)
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
				if (distinct_dist_exprs)
				{
					result_plan = (Plan *) make_motion_hash(root, result_plan,
															distinct_dist_exprs,
															distinct_dist_opfamilies);
					current_pathkeys = NIL;		/* Any pre-existing order now lost. */
				}
				else
				{
					result_plan = (Plan *) make_motion_gather(root, result_plan, current_pathkeys);
				}
				result_plan->total_cost += motion_cost_per_row * result_plan->plan_rows;
			}
		}
		else if ( result_plan->flow->flotype == FLOW_SINGLETON )
			; /* Already collocated. */
		else
		{
			ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
							errmsg("unexpected input locus to distinct")));
		}

		if (use_hashed_distinct)
		{
			/* Hashed aggregate plan --- no sort needed */
			result_plan = (Plan *) make_agg(root,
											result_plan->targetlist,
											NIL,
											AGG_HASHED,
											NULL,
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

			if (must_gather && result_plan->flow->flotype != FLOW_SINGLETON)
			{
				/*
				 * As an optimization, eliminate any duplicates within the segment,
				 * before the motion.
				 */
				if (IsA(result_plan, Sort) &&gp_enable_sort_distinct)
					((Sort *) result_plan)->noduplicates = true;
				result_plan = (Plan *) make_unique(result_plan, parse->distinctClause);
				result_plan->flow = pull_up_Flow(result_plan, result_plan->lefttree);

				result_plan = (Plan *) make_motion_gather(root, result_plan, current_pathkeys);
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

		if (must_gather && result_plan->flow->flotype != FLOW_SINGLETON)
		{
			/*
			 * current_pathkeys might contain unneeded columns that have been
			 * eliminated from the final target list, and we cannot maintain
			 * such an order in the Motion anymore. So use root->sortpathkeys
			 * rather than current_pathkeys here. (See similar case in LIMIT
			 * handling below.
			 */
			current_pathkeys = root->sort_pathkeys;
			result_plan = (Plan *) make_motion_gather(root, result_plan, current_pathkeys);
		}
	}

	/*
	 * If there is a FOR [KEY] UPDATE/SHARE clause, add the LockRows node.
	 * (Note: we intentionally test parse->rowMarks not root->rowMarks here.
	 * If there are only non-locking rowmarks, they should be handled by the
	 * ModifyTable node instead.)
	 */
	if (parse->rowMarks)
	{
		ListCell   *lc;
		List	   *newmarks = NIL;

		/*
		 * select for update will lock the whole table, we do it at addRangeTableEntry.
		 * The reason is that gpdb is an MPP database, the result tuples may not be on
		 * the same segment. And for cursor statement, reader gang cannot get Xid to lock
		 * the tuples. (More details: https://groups.google.com/a/greenplum.org/forum/#!topic/gpdb-dev/p-6_dNjnRMQ)
		 * Upgrading the lock mode (see below) for distributed table is probably
		 * not needed for all the cases and we may want to enhance this later.
		 */
		foreach(lc, root->rowMarks)
		{
			PlanRowMark *rc = (PlanRowMark *) lfirst(lc);

			if (rc->markType == ROW_MARK_EXCLUSIVE || rc->markType == ROW_MARK_SHARE)
			{
				RelOptInfo *brel = root->simple_rel_array[rc->rti];

				if (GpPolicyIsPartitioned(brel->cdbpolicy) ||
					GpPolicyIsReplicated(brel->cdbpolicy))
				{
					if (rc->markType == ROW_MARK_EXCLUSIVE)
						rc->markType = ROW_MARK_TABLE_EXCLUSIVE;
					else
						rc->markType = ROW_MARK_TABLE_SHARE;
				}
			}

			/* We only need LockRows for the tuple-level locks */
			if (rc->markType != ROW_MARK_TABLE_EXCLUSIVE &&
				rc->markType != ROW_MARK_TABLE_SHARE)
				newmarks = lappend(newmarks, rc);
		}

		if (newmarks)
		{
			result_plan = (Plan *) make_lockrows(result_plan,
												 newmarks,
												 SS_assign_special_param(root));
			result_plan->flow = pull_up_Flow(result_plan, result_plan->lefttree);

			/*
			 * The result can no longer be assumed sorted, since locking might
			 * cause the sort key columns to be replaced with new values.
			 */
			current_pathkeys = NIL;
		}
	}

	/*
	 * Finally, if there is a LIMIT/OFFSET clause, add the LIMIT node.
	 */
	if (limit_needed(parse))
	{
		if (Gp_role == GP_ROLE_DISPATCH &&
			(result_plan->flow->flotype == FLOW_PARTITIONED ||
			 result_plan->flow->locustype == CdbLocusType_SegmentGeneral))
		{

			if (result_plan->flow->flotype == FLOW_PARTITIONED &&
				!limit_contain_volatile_functions)
			{
				/* pushdown the first phase of multi-phase limit (which takes offset into account) */
				result_plan = pushdown_preliminary_limit(result_plan, parse->limitCount, count_est, parse->limitOffset, offset_est);
			}

			/*
			 * Focus on QE [merge to preserve order], prior to final LIMIT.
			 *
			 * If there is an ORDER BY, the input should be in the required
			 * order now, and we must preserve the order in the merge.
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
		List	   *opfamilies;
		ListCell   *lc;

		/* Deal with the special case of SCATTER RANDOMLY */
		if (list_length(parse->scatterClause) == 1 && linitial(parse->scatterClause) == NULL)
			exprList = NIL;
		else
			exprList = parse->scatterClause;

		opfamilies = NIL;
		foreach(lc, exprList)
		{
			Node	   *expr = lfirst(lc);
			Oid			opfamily;

			opfamily = cdb_default_distribution_opfamily_for_type(exprType(expr));
			opfamilies = lappend_oid(opfamilies, opfamily);
		}

		/*
		 * Repartition the subquery plan based on our distribution
		 * requirements
		 */
		r = repartitionPlan(result_plan, false, false,
							exprList, opfamilies,
							result_plan->flow->numsegments);
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
 * add_tlist_costs_to_plan
 *
 * Estimate the execution costs associated with evaluating the targetlist
 * expressions, and add them to the cost estimates for the Plan node.
 *
 * If the tlist contains set-returning functions, also inflate the Plan's cost
 * and plan_rows estimates accordingly.  (Hence, this must be called *after*
 * any logic that uses plan_rows to, eg, estimate qual evaluation costs.)
 *
 * Note: during initial stages of planning, we mostly consider plan nodes with
 * "flat" tlists, containing just Vars.  So their evaluation cost is zero
 * according to the model used by cost_qual_eval() (or if you prefer, the cost
 * is factored into cpu_tuple_cost).  Thus we can avoid accounting for tlist
 * cost throughout query_planner() and subroutines.  But once we apply a
 * tlist that might contain actual operators, sub-selects, etc, we'd better
 * account for its cost.  Any set-returning functions in the tlist must also
 * affect the estimated rowcount.
 *
 * Once grouping_planner() has applied a general tlist to the topmost
 * scan/join plan node, any tlist eval cost for added-on nodes should be
 * accounted for as we create those nodes.  Presently, of the node types we
 * can add on later, only Agg, WindowAgg, and Group project new tlists (the
 * rest just copy their input tuples) --- so make_agg(), make_windowagg() and
 * make_group() are responsible for calling this function to account for their
 * tlist costs.
 */
void
add_tlist_costs_to_plan(PlannerInfo *root, Plan *plan, List *tlist)
{
	QualCost	tlist_cost;
	double		tlist_rows;

	cost_qual_eval(&tlist_cost, tlist, root);
	plan->startup_cost += tlist_cost.startup;
	plan->total_cost += tlist_cost.startup +
		tlist_cost.per_tuple * plan->plan_rows;

	tlist_rows = tlist_returns_set_rows(tlist);
	if (tlist_rows > 1)
	{
		/*
		 * We assume that execution costs of the tlist proper were all
		 * accounted for by cost_qual_eval.  However, it still seems
		 * appropriate to charge something more for the executor's general
		 * costs of processing the added tuples.  The cost is probably less
		 * than cpu_tuple_cost, though, so we arbitrarily use half of that.
		 */
		plan->total_cost += plan->plan_rows * (tlist_rows - 1) *
			cpu_tuple_cost / 2;

		plan->plan_rows *= tlist_rows;
	}
}

/*
 * Entry is through is_dummy_plan().
 *
 * Detect whether a plan node is a "dummy" plan created when a relation
 * is deemed not to need scanning due to constraint exclusion.
 *
 * Currently, such dummy plans are Result nodes with constant FALSE
 * filter quals (see set_dummy_rel_pathlist and create_append_plan).
 *
 * XXX this probably ought to be somewhere else, but not clear where.
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
		case T_LockRows:
		case T_SubqueryScan:

			/*
			 * Some node types are dummy, if their outer plan or subplan is
			 * dummy so we just recur.
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


bool
is_dummy_plan(Plan *plan)
{
	bool		is_dummy = false;

	is_dummy_plan_walker((Node *) plan, &is_dummy);

	return is_dummy;
}

/*
 * Create a bitmapset of the RT indexes of live base relations
 *
 * Helper for preprocess_rowmarks ... at this point in the proceedings,
 * the only good way to distinguish baserels from appendrel children
 * is to see what is in the join tree.
 */
static Bitmapset *
get_base_rel_indexes(Node *jtnode)
{
	Bitmapset  *result;

	if (jtnode == NULL)
		return NULL;
	if (IsA(jtnode, RangeTblRef))
	{
		int			varno = ((RangeTblRef *) jtnode)->rtindex;

		result = bms_make_singleton(varno);
	}
	else if (IsA(jtnode, FromExpr))
	{
		FromExpr   *f = (FromExpr *) jtnode;
		ListCell   *l;

		result = NULL;
		foreach(l, f->fromlist)
			result = bms_join(result,
							  get_base_rel_indexes(lfirst(l)));
	}
	else if (IsA(jtnode, JoinExpr))
	{
		JoinExpr   *j = (JoinExpr *) jtnode;

		result = bms_join(get_base_rel_indexes(j->larg),
						  get_base_rel_indexes(j->rarg));
	}
	else
	{
		elog(ERROR, "unrecognized node type: %d",
			 (int) nodeTag(jtnode));
		result = NULL;			/* keep compiler quiet */
	}
	return result;
}

/*
 * preprocess_rowmarks - set up PlanRowMarks if needed
 */
static void
preprocess_rowmarks(PlannerInfo *root)
{
	Query	   *parse = root->parse;
	Bitmapset  *rels;
	List	   *prowmarks;
	ListCell   *l;
	int			i;

	if (parse->rowMarks)
	{
		/*
		 * We've got trouble if FOR [KEY] UPDATE/SHARE appears inside
		 * grouping, since grouping renders a reference to individual tuple
		 * CTIDs invalid.  This is also checked at parse time, but that's
		 * insufficient because of rule substitution, query pullup, etc.
		 */
		CheckSelectLocking(parse, ((RowMarkClause *)
								   linitial(parse->rowMarks))->strength);
	}
	else
	{
		/*
		 * We only need rowmarks for UPDATE, DELETE, or FOR [KEY]
		 * UPDATE/SHARE.
		 */
		if (parse->commandType != CMD_UPDATE &&
			parse->commandType != CMD_DELETE)
			return;
	}

	/*
	 * We need to have rowmarks for all base relations except the target. We
	 * make a bitmapset of all base rels and then remove the items we don't
	 * need or have FOR [KEY] UPDATE/SHARE marks for.
	 */
	rels = get_base_rel_indexes((Node *) parse->jointree);
	if (parse->resultRelation)
		rels = bms_del_member(rels, parse->resultRelation);

	/*
	 * Convert RowMarkClauses to PlanRowMark representation.
	 */
	prowmarks = NIL;
	foreach(l, parse->rowMarks)
	{
		RowMarkClause *rc = (RowMarkClause *) lfirst(l);
		RangeTblEntry *rte = rt_fetch(rc->rti, parse->rtable);
		PlanRowMark *newrc;

		/*
		 * Currently, it is syntactically impossible to have FOR UPDATE et al
		 * applied to an update/delete target rel.  If that ever becomes
		 * possible, we should drop the target from the PlanRowMark list.
		 */
		Assert(rc->rti != parse->resultRelation);

		/*
		 * Ignore RowMarkClauses for subqueries; they aren't real tables and
		 * can't support true locking.  Subqueries that got flattened into the
		 * main query should be ignored completely.  Any that didn't will get
		 * ROW_MARK_COPY items in the next loop.
		 */
		if (rte->rtekind != RTE_RELATION)
			continue;

		/*
		 * Similarly, ignore RowMarkClauses for foreign tables; foreign tables
		 * will instead get ROW_MARK_COPY items in the next loop.  (FDWs might
		 * choose to do something special while fetching their rows, but that
		 * is of no concern here.)
		 */
		if (rte->relkind == RELKIND_FOREIGN_TABLE)
			continue;

		rels = bms_del_member(rels, rc->rti);

		newrc = makeNode(PlanRowMark);
		newrc->rti = newrc->prti = rc->rti;
		newrc->rowmarkId = ++(root->glob->lastRowMarkId);
		switch (rc->strength)
		{
			case LCS_FORUPDATE:
				newrc->markType = ROW_MARK_EXCLUSIVE;
				break;
			case LCS_FORNOKEYUPDATE:
				newrc->markType = ROW_MARK_NOKEYEXCLUSIVE;
				break;
			case LCS_FORSHARE:
				newrc->markType = ROW_MARK_SHARE;
				break;
			case LCS_FORKEYSHARE:
				newrc->markType = ROW_MARK_KEYSHARE;
				break;
		}
		newrc->noWait = rc->noWait;
		newrc->isParent = false;

		prowmarks = lappend(prowmarks, newrc);
	}

	/*
	 * Now, add rowmarks for any non-target, non-locked base relations.
	 */
	i = 0;
	foreach(l, parse->rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(l);
		PlanRowMark *newrc;

		i++;
		if (!bms_is_member(i, rels))
			continue;

		newrc = makeNode(PlanRowMark);
		newrc->rti = newrc->prti = i;
		newrc->rowmarkId = ++(root->glob->lastRowMarkId);
		/* real tables support REFERENCE, anything else needs COPY */
		if (rte->rtekind == RTE_RELATION &&
			rte->relkind != RELKIND_FOREIGN_TABLE)
			newrc->markType = ROW_MARK_REFERENCE;
		else
			newrc->markType = ROW_MARK_COPY;
		newrc->noWait = false;	/* doesn't matter */
		newrc->isParent = false;

		prowmarks = lappend(prowmarks, newrc);
	}

	root->rowMarks = prowmarks;
}

/*
 * preprocess_limit - do pre-estimation for LIMIT and/or OFFSET clauses
 *
 * We try to estimate the values of the LIMIT/OFFSET clauses, and pass the
 * results back in *count_est and *offset_est.  These variables are set to
 * 0 if the corresponding clause is not present, and -1 if it's present
 * but we couldn't estimate the value for it.  (The "0" convention is OK
 * for OFFSET but a little bit bogus for LIMIT: effectively we estimate
 * LIMIT 0 as though it were LIMIT 1.  But this is in line with the planner's
 * usual practice of never estimating less than one row.)  These values will
 * be passed to make_limit, which see if you change this code.
 *
 * The return value is the suitably adjusted tuple_fraction to use for
 * planning the query.  This adjustment is not overridable, since it reflects
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
					*offset_est = 0;	/* treat as not present */
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
		 * We have an OFFSET but no LIMIT.  This acts entirely differently
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
		 * together; likewise if they are both fractional.  If one is
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
 * limit_needed - do we actually need a Limit plan node?
 *
 * If we have constant-zero OFFSET and constant-null LIMIT, we can skip adding
 * a Limit node.  This is worth checking for because "OFFSET 0" is a common
 * locution for an optimization fence.  (Because other places in the planner
 * merely check whether parse->limitOffset isn't NULL, it will still work as
 * an optimization fence --- we're just suppressing unnecessary run-time
 * overhead.)
 *
 * This might look like it could be merged into preprocess_limit, but there's
 * a key distinction: here we need hard constants in OFFSET/LIMIT, whereas
 * in preprocess_limit it's good enough to consider estimated values.
 */
static bool
limit_needed(Query *parse)
{
	Node	   *node;

	node = parse->limitCount;
	if (node)
	{
		if (IsA(node, Const))
		{
			/* NULL indicates LIMIT ALL, ie, no limit */
			if (!((Const *) node)->constisnull)
				return true;	/* LIMIT with a constant value */
		}
		else
			return true;		/* non-constant LIMIT */
	}

	node = parse->limitOffset;
	if (node)
	{
		if (IsA(node, Const))
		{
			/* Treat NULL as no offset; the executor would too */
			if (!((Const *) node)->constisnull)
			{
				int64		offset = DatumGetInt64(((Const *) node)->constvalue);

				if (offset != 0)
					return true;	/* OFFSET with a nonzero value */
			}
		}
		else
			return true;		/* non-constant OFFSET */
	}

	return false;				/* don't need a Limit plan node */
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
	 * Add any remaining GROUP BY items to the new list, but only if we were
	 * able to make a complete match.  In other words, we only rearrange the
	 * GROUP BY list if the result is that one list is a prefix of the other
	 * --- otherwise there's no possibility of a common sort.  Also, give up
	 * if there are any non-sortable GROUP BY items, since then there's no
	 * hope anyway.
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
 * Compute query_pathkeys and other pathkeys during plan generation
 */
static void
standard_qp_callback(PlannerInfo *root, void *extra)
{
	Query	   *parse = root->parse;
	standard_qp_extra *qp_extra = (standard_qp_extra *) extra;
	List	   *tlist = qp_extra->tlist;
	List	   *activeWindows = qp_extra->activeWindows;

	/*
	 * Calculate pathkeys that represent grouping/ordering requirements.  The
	 * sortClause is certainly sort-able, but GROUP BY and DISTINCT might not
	 * be, in which case we just leave their pathkeys empty.
	 */
	if (parse->groupClause &&
		grouping_is_sortable(parse->groupClause))
		root->group_pathkeys =
			make_pathkeys_for_groupclause(root,
										  parse->groupClause,
										  tlist);
	else
		root->group_pathkeys = NIL;

	/* We consider only the first (bottom) window in pathkeys logic */
	if (activeWindows != NIL)
	{
		WindowClause *wc = (WindowClause *) linitial(activeWindows);

		root->window_pathkeys = make_pathkeys_for_window(root,
														 wc,
														 tlist);
	}
	else
		root->window_pathkeys = NIL;

	if (parse->distinctClause &&
		grouping_is_sortable(parse->distinctClause))
		root->distinct_pathkeys =
			make_pathkeys_for_sortclauses(root,
										  parse->distinctClause,
										  tlist);
	else
		root->distinct_pathkeys = NIL;

	root->sort_pathkeys =
		make_pathkeys_for_sortclauses(root,
									  parse->sortClause,
									  tlist);

	/*
	 * Figure out whether we want a sorted result from query_planner.
	 *
	 * If we have a sortable GROUP BY clause, then we want a result sorted
	 * properly for grouping.  Otherwise, if we have window functions to
	 * evaluate, we try to sort for the first window.  Otherwise, if there's a
	 * sortable DISTINCT clause that's more rigorous than the ORDER BY clause,
	 * we try to produce output that's sufficiently well sorted for the
	 * DISTINCT.  Otherwise, if there is an ORDER BY clause, we want to sort
	 * by the ORDER BY clause.
	 *
	 * Note: if we have both ORDER BY and GROUP BY, and ORDER BY is a superset
	 * of GROUP BY, it would be tempting to request sort by ORDER BY --- but
	 * that might just leave us failing to exploit an available sort order at
	 * all.  Needs more thought.  The choice for DISTINCT versus ORDER BY is
	 * much easier, since we know that the parser ensured that one is a
	 * superset of the other.
	 */
	if (root->group_pathkeys)
		root->query_pathkeys = root->group_pathkeys;
	else if (root->window_pathkeys)
		root->query_pathkeys = root->window_pathkeys;
	else if (list_length(root->distinct_pathkeys) >
			 list_length(root->sort_pathkeys))
		root->query_pathkeys = root->distinct_pathkeys;
	else if (root->sort_pathkeys)
		root->query_pathkeys = root->sort_pathkeys;
	else
		root->query_pathkeys = NIL;
}

/*
 * choose_hashed_grouping - should we use hashed grouping?
 *
 * Returns TRUE to select hashing, FALSE to select sorting.
 */
bool
choose_hashed_grouping(PlannerInfo *root,
					   double tuple_fraction, double limit_tuples,
					   double path_rows, int path_width,
					   Path *cheapest_path, Path *sorted_path,
					   int numGroupOps,
					   double dNumGroups, AggClauseCosts *agg_costs)
{
	Query	   *parse = root->parse;
	int			numGroupCols;
	bool		can_hash;
	bool		can_sort;
	Size		hashentrysize;
	List	   *target_pathkeys;
	List	   *current_pathkeys;
	Path		hashed_p;
	Path		sorted_p;
	HashAggTableSizes hash_info;

	/*
	 * Executor doesn't support hashed aggregation with DISTINCT or ORDER BY
	 * aggregates.  (Doing so would imply storing *all* the input values in
	 * the hash table, and/or running many sorts in parallel, either of which
	 * seems like a certain loser.)  We similarly don't support ordered-set
	 * aggregates in hashed aggregation, but that case is included in the
	 * numOrderedAggs count.
	 */
	can_hash = (agg_costs->numOrderedAggs == 0 &&
				grouping_is_hashable(parse->groupClause));
	can_sort = grouping_is_sortable(parse->groupClause);

	/* Quick out if only one choice is workable */
	if (!(can_hash && can_sort))
	{
		if (can_hash)
			return true;
		else if (can_sort)
			return false;
		else
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("could not implement GROUP BY"),
					 errdetail("Some of the datatypes only support hashing, while others only support sorting.")));
	}

	/* Prefer sorting when enable_hashagg is off */
	if (!root->config->enable_hashagg)
		return false;

	/*
	 * CDB: The combine function is used to merge transient values during
	 * hash reloading (see execHHashagg.c). So hash agg is not allowed if one
	 * of the aggregates doesn't have a combine function. Likewise, if a
	 * transition value cannot be serialized, a hash agg is not allowed.
	 */
	if (agg_costs->hasNonCombine || agg_costs->hasNonSerial)
		return false;

	/*
	 * CDB: The parallel grouping planner cannot use hashed aggregation for
	 * ordered aggregates.
	 */
	if (agg_costs->numOrderedAggs != 0)
		return false;

	/*
	 * Don't do it if it doesn't look like the hashtable will fit into
	 * work_mem.
	 */

	/* Estimate per-hash-entry space at tuple width... */
	hashentrysize = agg_hash_entrywidth(agg_costs->numAggs,
							   sizeof(HeapTupleData) + sizeof(HeapTupleHeaderData) + path_width,
							   agg_costs->transitionSpace);

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
	 * DISTINCT and ORDER BY as the assumed required output sort order. This
	 * is an oversimplification because the DISTINCT might get implemented via
	 * hashing, but it's not clear that the case is common enough (or that our
	 * estimates are good enough) to justify trying to solve it exactly.
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
	 * step that may not be needed.  We assume grouping_planner() will have
	 * passed us a presorted path only if it's a winner compared to
	 * cheapest_path for this purpose.
	 *
	 * These path variables are dummies that just hold cost fields; we don't
	 * make actual Paths for these steps.
	 */
	numGroupCols = num_distcols_in_grouplist(root->parse->groupClause);
	cost_agg(&hashed_p, root, AGG_HASHED, agg_costs,
			 numGroupCols, dNumGroups,
			 cheapest_path->startup_cost, cheapest_path->total_cost,
			 path_rows, hash_info.workmem_per_entry,
			 hash_info.nbatches, hash_info.hashentry_width, false);
	/* Result of hashed agg is always unsorted */
	if (target_pathkeys)
		cost_sort(&hashed_p, root, target_pathkeys, hashed_p.total_cost,
				  dNumGroups, path_width,
				  0.0, work_mem, limit_tuples);

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
				  path_rows, path_width,
				  0.0, work_mem, -1.0);
		current_pathkeys = root->group_pathkeys;
	}

	if (parse->hasAggs)
		cost_agg(&sorted_p, root, AGG_SORTED, agg_costs,
				 numGroupCols, dNumGroups,
				 sorted_p.startup_cost, sorted_p.total_cost,
				 path_rows, 0.0, 0.0, 0.0, false);
	else
		cost_group(&sorted_p, root, numGroupCols, dNumGroups,
				   sorted_p.startup_cost, sorted_p.total_cost,
				   path_rows);
	/* The Agg or Group node will preserve ordering */
	if (target_pathkeys &&
		!pathkeys_contained_in(target_pathkeys, current_pathkeys))
		cost_sort(&sorted_p, root, target_pathkeys, sorted_p.total_cost,
				  dNumGroups, path_width,
				  0.0, work_mem, limit_tuples);

	/*
	 * Now make the decision using the top-level tuple fraction.
	 */
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
 * (One difference is that we sometimes apply this after forming a Plan,
 * so the input alternatives can't be represented as Paths --- instead we
 * pass in the costs as individual variables.)
 *
 * But note that making the two choices independently is a bit bogus in
 * itself.  If the two could be combined into a single choice operation
 * it'd probably be better, but that seems far too unwieldy to be practical,
 * especially considering that the combination of GROUP BY and DISTINCT
 * isn't very common in real queries.  By separating them, we are giving
 * extra preference to using a sorting implementation when a common sort key
 * is available ... and that's not necessarily wrong anyway.
 *
 * Returns TRUE to select hashing, FALSE to select sorting.
 */
static bool
choose_hashed_distinct(PlannerInfo *root,
					   double tuple_fraction, double limit_tuples,
					   double path_rows, int path_width,
					   Cost cheapest_startup_cost, Cost cheapest_total_cost,
					   Cost sorted_startup_cost, Cost sorted_total_cost,
					   List *sorted_pathkeys,
					   double dNumDistinctRows)
{
	Query	   *parse = root->parse;
	int			numDistinctCols = list_length(parse->distinctClause);
	bool		can_sort;
	bool		can_hash;
	Size		hashentrysize;
	List	   *current_pathkeys;
	List	   *needed_pathkeys;
	Path		hashed_p;
	Path		sorted_p;
	HashAggTableSizes hash_info;

	/*
	 * If we have a sortable DISTINCT ON clause, we always use sorting. This
	 * enforces the expected behavior of DISTINCT ON.
	 */
	can_sort = grouping_is_sortable(parse->distinctClause);
	if (can_sort && parse->hasDistinctOn)
		return false;

	can_hash = grouping_is_hashable(parse->distinctClause);

	/* Quick out if only one choice is workable */
	if (!(can_hash && can_sort))
	{
		if (can_hash)
			return true;
		else if (can_sort)
			return false;
		else
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("could not implement DISTINCT"),
					 errdetail("Some of the datatypes only support hashing, while others only support sorting.")));
	}

	/* Prefer sorting when enable_hashagg is off */
	if (!root->config->enable_hashagg)
		return false;

	/*
	 * Don't do it if it doesn't look like the hashtable will fit into
	 * work_mem.
	 */

	/* 
	 * Note that HashAgg uses a HHashTable for performing the aggregations. So
	 * estimate the hash table size using GPDB specific methods.
	 */
	hashentrysize = agg_hash_entrywidth(0 /* numaggs */,
									/* The following estimate is very rough but good enough for planning. */
									sizeof(HeapTupleData) + sizeof(HeapTupleHeaderData) + path_width,
									0 /* transitionSpace */);

	if (!calcHashAggTableSizes(global_work_mem(root),
							   dNumDistinctRows,
							   hashentrysize,
							   false,
							   &hash_info))
	{
		return false;
	}


	/*
	 * See if the estimated cost is no more than doing it the other way. While
	 * avoiding the need for sorted input is usually a win, the fact that the
	 * output won't be sorted may be a loss; so we need to do an actual cost
	 * comparison.
	 *
	 * We need to consider cheapest_path + hashagg [+ final sort] versus
	 * sorted_path [+ sort] + group [+ final sort] where brackets indicate a
	 * step that may not be needed.
	 *
	 * These path variables are dummies that just hold cost fields; we don't
	 * make actual Paths for these steps.
	 */
	cost_agg(&hashed_p, root, AGG_HASHED, NULL,
			 numDistinctCols, dNumDistinctRows,
			 cheapest_startup_cost, cheapest_total_cost,
			 path_rows,
			 hash_info.workmem_per_entry,
			 hash_info.nbatches,
			 hash_info.hashentry_width,
			 false /* hash_streaming */);

	/*
	 * Result of hashed agg is always unsorted, so if ORDER BY is present we
	 * need to charge for the final sort.
	 */
	if (parse->sortClause)
		cost_sort(&hashed_p, root, root->sort_pathkeys, hashed_p.total_cost,
				  dNumDistinctRows, path_width,
				  0.0, work_mem, limit_tuples);

	/*
	 * Now for the GROUP case.  See comments in grouping_planner about the
	 * sorting choices here --- this code should match that code.
	 */
	sorted_p.startup_cost = sorted_startup_cost;
	sorted_p.total_cost = sorted_total_cost;
	current_pathkeys = sorted_pathkeys;
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
			current_pathkeys = root->sort_pathkeys;
		cost_sort(&sorted_p, root, current_pathkeys, sorted_p.total_cost,
				  path_rows, path_width,
				  0.0, work_mem, -1.0);
	}
	cost_group(&sorted_p, root, numDistinctCols, dNumDistinctRows,
			   sorted_p.startup_cost, sorted_p.total_cost,
			   path_rows);
	if (parse->sortClause &&
		!pathkeys_contained_in(root->sort_pathkeys, current_pathkeys))
		cost_sort(&sorted_p, root, root->sort_pathkeys, sorted_p.total_cost,
				  dNumDistinctRows, path_width,
				  0.0, work_mem, limit_tuples);

	/*
	 * Now make the decision using the top-level tuple fraction.
	 */
	if (compare_fractional_path_costs(&hashed_p, &sorted_p,
									  tuple_fraction) < 0)
	{
		/* Hashed is cheaper, so use it */
		return true;
	}
	return false;
}

/*
 * make_subplanTargetList
 *	  Generate appropriate target list when grouping is required.
 *
 * When grouping_planner inserts grouping or aggregation plan nodes
 * above the scan/join plan constructed by query_planner+create_plan,
 * we typically want the scan/join plan to emit a different target list
 * than the outer plan nodes should have.  This routine generates the
 * correct target list for the scan/join subplan.
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
 *		a+b,c,d
 * where the a+b target will be used by the Sort/Group steps, and the
 * other targets will be used for computing the final results.
 *
 * If we are grouping or aggregating, *and* there are no non-Var grouping
 * expressions, then the returned tlist is effectively dummy; we do not
 * need to force it to be evaluated, because all the Vars it contains
 * should be present in the "flat" tlist generated by create_plan, though
 * possibly in a different order.  In that case we'll use create_plan's tlist,
 * and the tlist made here is only needed as input to query_planner to tell
 * it which Vars are needed in the output of the scan/join plan.
 *
 * 'tlist' is the query's target list.
 * 'groupColIdx' receives an array of column numbers for the GROUP BY
 *			expressions (if there are any) in the returned target list.
 * 'groupOperators' receives an array of equality operators corresponding
 *			the GROUP BY expressions.
 * 'need_tlist_eval' is set true if we really need to evaluate the
 *			returned tlist as-is.  (Note: locate_grouping_columns assumes
 *			that if this is FALSE, all grouping columns are simple Vars.)
 *
 * The result is the targetlist to be passed to query_planner.
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
	if (!parse->hasAggs && !parse->groupClause && !root->hasHavingQual &&
		!parse->hasWindowFuncs)
	{
		*need_tlist_eval = true;
		return tlist;
	}

	/*
	 * Otherwise, start with a "flattened" tlist (having just the Vars
	 * mentioned in the targetlist and HAVING qual).  Note this includes Vars
	 * used in resjunk items, so we are covering the needs of ORDER BY and
	 * window specifications.  Vars used within Aggrefs will be pulled out
	 * here, too.
	 */
	sub_tlist = flatten_tlist(tlist,
							  PVC_RECURSE_AGGREGATES,
							  PVC_INCLUDE_PLACEHOLDERS);
	extravars = pull_var_clause(parse->havingQual,
								PVC_RECURSE_AGGREGATES,
								PVC_INCLUDE_PLACEHOLDERS);
	sub_tlist = add_to_flat_tlist(sub_tlist, extravars);
	list_free(extravars);

	{
		ListCell *lc;

		foreach(lc, root->parse->windowClause)
		{
			WindowClause *window = (WindowClause *) lfirst(lc);

			extravars = pull_var_clause(window->startOffset,
										PVC_REJECT_AGGREGATES,
										PVC_INCLUDE_PLACEHOLDERS);
			sub_tlist = add_to_flat_tlist(sub_tlist, extravars);
			list_free(extravars);

			extravars = pull_var_clause(window->endOffset,
										PVC_REJECT_AGGREGATES,
										PVC_INCLUDE_PLACEHOLDERS);
			sub_tlist = add_to_flat_tlist(sub_tlist, extravars);
			list_free(extravars);
		}
	}

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

	/*
	 * GPDB_92_MERGE_FIXME: The codes below are different from PG 9.2.
	 * We believe our logic is the same with upstream.
	 */
	if (numCols > 0)
	{
		int			keyno = 0;

		/*
		 * If grouping, create sub_tlist entries for all GROUP BY columns, and
		 * make an array showing where the group columns are in the sub_tlist.
		 *
		 * Note: with this implementation, the array entries will always be
		 * 1..N, but we don't want callers to assume that.
		 */
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
 * locate_grouping_columns
 *		Locate grouping columns in the tlist chosen by create_plan.
 *
 * This is only needed if we don't use the sub_tlist chosen by
 * make_subplanTargetList.  We have to forget the column indexes found
 * by that routine and re-locate the grouping exprs in the real sub_tlist.
 * We assume the grouping exprs are just Vars (see make_subplanTargetList).
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
		Var		   *groupexpr = (Var *) lfirst(ge);
		TargetEntry *te;

		/*
		 * The grouping column returned by create_plan might not have the same
		 * typmod as the original Var.  (This can happen in cases where a
		 * set-returning function has been inlined, so that we now have more
		 * knowledge about what it returns than we did when the original Var
		 * was created.)  So we can't use tlist_member() to search the tlist;
		 * instead use tlist_member_match_var.  For safety, still check that
		 * the vartype matches.
		 */
		if (!(groupexpr && IsA(groupexpr, Var)))
			elog(ERROR, "grouping column is not a Var as expected");
		te = tlist_member_match_var(groupexpr, sub_tlist);
		if (!te)
			elog(ERROR, "failed to locate grouping columns");
		Assert(((Var *) te->expr)->vartype == groupexpr->vartype);
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
 * select_active_windows
 *		Create a list of the "active" window clauses (ie, those referenced
 *		by non-deleted WindowFuncs) in the order they are to be executed.
 */
static List *
select_active_windows(PlannerInfo *root, WindowFuncLists *wflists)
{
	List	   *windowClause = root->parse->windowClause;
	List	   *result = NIL;
	ListCell   *lc;
	int			nActive = 0;
	WindowClauseSortData *actives = palloc(sizeof(WindowClauseSortData)
										   * list_length(windowClause));

	/* First, construct an array of the active windows */
	foreach(lc, windowClause)
	{
		WindowClause *wc = (WindowClause *) lfirst(lc);

		/* It's only active if wflists shows some related WindowFuncs */
		Assert(wc->winref <= wflists->maxWinRef);
		if (wflists->windowFuncs[wc->winref] == NIL)
			continue;

		actives[nActive].wc = wc;	/* original clause */

		/*
		 * For sorting, we want the list of partition keys followed by the
		 * list of sort keys. But pathkeys construction will remove duplicates
		 * between the two, so we can as well (even though we can't detect all
		 * of the duplicates, since some may come from ECs - that might mean
		 * we miss optimization chances here). We must, however, ensure that
		 * the order of entries is preserved with respect to the ones we do
		 * keep.
		 *
		 * partitionClause and orderClause had their own duplicates removed in
		 * parse analysis, so we're only concerned here with removing
		 * orderClause entries that also appear in partitionClause.
		 */
		actives[nActive].uniqueOrder =
			list_concat_unique(list_copy(wc->partitionClause),
							   wc->orderClause);
		nActive++;
	}

	/*
	 * Sort active windows by their partitioning/ordering clauses, ignoring
	 * any framing clauses, so that the windows that need the same sorting are
	 * adjacent in the list. When we come to generate paths, this will avoid
	 * inserting additional Sort nodes.
	 *
	 * This is how we implement a specific requirement from the SQL standard,
	 * which says that when two or more windows are order-equivalent (i.e.
	 * have matching partition and order clauses, even if their names or
	 * framing clauses differ), then all peer rows must be presented in the
	 * same order in all of them. If we allowed multiple sort nodes for such
	 * cases, we'd risk having the peer rows end up in different orders in
	 * equivalent windows due to sort instability. (See General Rule 4 of
	 * <window clause> in SQL2008 - SQL2016.)
	 *
	 * Additionally, if the entire list of clauses of one window is a prefix
	 * of another, put first the window with stronger sorting requirements.
	 * This way we will first sort for stronger window, and won't have to sort
	 * again for the weaker one.
	 */
	qsort(actives, nActive, sizeof(WindowClauseSortData), common_prefix_cmp);

	/* build ordered list of the original WindowClause nodes */
	for (int i = 0; i < nActive; i++)
		result = lappend(result, actives[i].wc);

	pfree(actives);

	return result;
}

/*
 * common_prefix_cmp
 *	  QSort comparison function for WindowClauseSortData
 *
 * Sort the windows by the required sorting clauses. First, compare the sort
 * clauses themselves. Second, if one window's clauses are a prefix of another
 * one's clauses, put the window with more sort clauses first.
 */
static int
common_prefix_cmp(const void *a, const void *b)
{
	const WindowClauseSortData *wcsa = a;
	const WindowClauseSortData *wcsb = b;
	ListCell   *item_a;
	ListCell   *item_b;

	forboth(item_a, wcsa->uniqueOrder, item_b, wcsb->uniqueOrder)
	{
		/*
		 * GPDB_100_MERGE_FIXME: replace with lfirst_node() calls when commit
		 * 8f0530f58061b185dc385df42e62d78a18d4ae3e is merged.
		 */
		SortGroupClause *sca = (SortGroupClause *) lfirst(item_a);
		SortGroupClause *scb = (SortGroupClause *) lfirst(item_b);

		if (sca->tleSortGroupRef > scb->tleSortGroupRef)
			return -1;
		else if (sca->tleSortGroupRef < scb->tleSortGroupRef)
			return 1;
		else if (sca->sortop > scb->sortop)
			return -1;
		else if (sca->sortop < scb->sortop)
			return 1;
		else if (sca->nulls_first && !scb->nulls_first)
			return -1;
		else if (!sca->nulls_first && scb->nulls_first)
			return 1;
		/* no need to compare eqop, since it is fully determined by sortop */
	}

	if (list_length(wcsa->uniqueOrder) > list_length(wcsb->uniqueOrder))
		return -1;
	else if (list_length(wcsa->uniqueOrder) < list_length(wcsb->uniqueOrder))
		return 1;

	return 0;
}

/*
 * make_windowInputTargetList
 *	  Generate appropriate target list for initial input to WindowAgg nodes.
 *
 * When grouping_planner inserts one or more WindowAgg nodes into the plan,
 * this function computes the initial target list to be computed by the node
 * just below the first WindowAgg.  This list must contain all values needed
 * to evaluate the window functions, compute the final target list, and
 * perform any required final sort step.  If multiple WindowAggs are needed,
 * each intermediate one adds its window function results onto this tlist;
 * only the topmost WindowAgg computes the actual desired target list.
 *
 * This function is much like make_subplanTargetList, though not quite enough
 * like it to share code.  As in that function, we flatten most expressions
 * into their component variables.  But we do not want to flatten window
 * PARTITION BY/ORDER BY clauses, since that might result in multiple
 * evaluations of them, which would be bad (possibly even resulting in
 * inconsistent answers, if they contain volatile functions).  Also, we must
 * not flatten GROUP BY clauses that were left unflattened by
 * make_subplanTargetList, because we may no longer have access to the
 * individual Vars in them.
 *
 * Another key difference from make_subplanTargetList is that we don't flatten
 * Aggref expressions, since those are to be computed below the window
 * functions and just referenced like Vars above that.
 *
 * 'tlist' is the query's final target list.
 * 'activeWindows' is the list of active windows previously identified by
 *			select_active_windows.
 *
 * The result is the targetlist to be computed by the plan node immediately
 * below the first WindowAgg node.
 */
static List *
make_windowInputTargetList(PlannerInfo *root,
						   List *tlist,
						   List *activeWindows)
{
	Query	   *parse = root->parse;
	Bitmapset  *sgrefs;
	List	   *new_tlist;
	List	   *flattenable_cols;
	List	   *flattenable_vars;
	ListCell   *lc;

	Assert(parse->hasWindowFuncs);

	/*
	 * Collect the sortgroupref numbers of window PARTITION/ORDER BY clauses
	 * into a bitmapset for convenient reference below.
	 */
	sgrefs = NULL;
	foreach(lc, activeWindows)
	{
		WindowClause *wc = (WindowClause *) lfirst(lc);
		ListCell   *lc2;

		foreach(lc2, wc->partitionClause)
		{
			SortGroupClause *sortcl = (SortGroupClause *) lfirst(lc2);

			sgrefs = bms_add_member(sgrefs, sortcl->tleSortGroupRef);
		}
		foreach(lc2, wc->orderClause)
		{
			SortGroupClause *sortcl = (SortGroupClause *) lfirst(lc2);

			sgrefs = bms_add_member(sgrefs, sortcl->tleSortGroupRef);
		}
	}

	/* Add in sortgroupref numbers of GROUP BY clauses, too */
	foreach(lc, parse->groupClause)
	{
		SortGroupClause *grpcl = (SortGroupClause *) lfirst(lc);

		sgrefs = bms_add_member(sgrefs, grpcl->tleSortGroupRef);
	}

	/*
	 * Construct a tlist containing all the non-flattenable tlist items, and
	 * save aside the others for a moment.
	 */
	new_tlist = NIL;
	flattenable_cols = NIL;

	foreach(lc, tlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);

		/*
		 * Don't want to deconstruct window clauses or GROUP BY items.  (Note
		 * that such items can't contain window functions, so it's okay to
		 * compute them below the WindowAgg nodes.)
		 */
		if (tle->ressortgroupref != 0 &&
			bms_is_member(tle->ressortgroupref, sgrefs))
		{
			/* Don't want to deconstruct this value, so add to new_tlist */
			TargetEntry *newtle;

			newtle = makeTargetEntry(tle->expr,
									 list_length(new_tlist) + 1,
									 NULL,
									 false);
			/* Preserve its sortgroupref marking, in case it's volatile */
			newtle->ressortgroupref = tle->ressortgroupref;
			new_tlist = lappend(new_tlist, newtle);
		}
		else
		{
			/*
			 * Column is to be flattened, so just remember the expression for
			 * later call to pull_var_clause.  There's no need for
			 * pull_var_clause to examine the TargetEntry node itself.
			 */
			flattenable_cols = lappend(flattenable_cols, tle->expr);
		}
	}

	/*
	 * Pull out all the Vars and Aggrefs mentioned in flattenable columns, and
	 * add them to the result tlist if not already present.  (Some might be
	 * there already because they're used directly as window/group clauses.)
	 *
	 * Note: it's essential to use PVC_INCLUDE_AGGREGATES here, so that the
	 * Aggrefs are placed in the Agg node's tlist and not left to be computed
	 * at higher levels.
	 */
	flattenable_vars = pull_var_clause((Node *) flattenable_cols,
									   PVC_INCLUDE_AGGREGATES,
									   PVC_INCLUDE_PLACEHOLDERS);
	new_tlist = add_to_flat_tlist(new_tlist, flattenable_vars);

	/* clean up cruft */
	list_free(flattenable_vars);
	list_free(flattenable_cols);

	/*
	 * Add any Vars that appear in the start/end bounds. In PostgreSQL,
	 * they're not allowed to contain any Vars of the same query level, but
	 * we do allow it in GPDB. They shouldn't contain any aggregates, though.
	 */
	foreach(lc, activeWindows)
	{
		WindowClause *wc = (WindowClause *) lfirst(lc);

		flattenable_vars = pull_var_clause(wc->startOffset,
										   PVC_REJECT_AGGREGATES,
										   PVC_INCLUDE_PLACEHOLDERS);
		new_tlist = add_to_flat_tlist(new_tlist, flattenable_vars);
		list_free(flattenable_vars);

		flattenable_vars = pull_var_clause(wc->endOffset,
										   PVC_REJECT_AGGREGATES,
										   PVC_INCLUDE_PLACEHOLDERS);
		new_tlist = add_to_flat_tlist(new_tlist, flattenable_vars);
		list_free(flattenable_vars);
	}

	return new_tlist;
}

/*
 * make_pathkeys_for_window
 *		Create a pathkeys list describing the required input ordering
 *		for the given WindowClause.
 *
 * The required ordering is first the PARTITION keys, then the ORDER keys.
 * In the future we might try to implement windowing using hashing, in which
 * case the ordering could be relaxed, but for now we always sort.
 */
static List *
make_pathkeys_for_window(PlannerInfo *root, WindowClause *wc,
						 List *tlist)
{
	List	   *window_pathkeys;
	List	   *window_sortclauses;

	/* Throw error if can't sort */
	if (!grouping_is_sortable(wc->partitionClause))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("could not implement window PARTITION BY"),
				 errdetail("Window partitioning columns must be of sortable datatypes.")));
	if (!grouping_is_sortable(wc->orderClause))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("could not implement window ORDER BY"),
		errdetail("Window ordering columns must be of sortable datatypes.")));

	/* Okay, make the combined pathkeys */
	window_sortclauses = list_concat(list_copy(wc->partitionClause),
									 list_copy(wc->orderClause));
	window_pathkeys = make_pathkeys_for_sortclauses(root,
													window_sortclauses,
													tlist);
	list_free(window_sortclauses);
	return window_pathkeys;
}

/*----------
 * get_column_info_for_window
 *		Get the partitioning/ordering column numbers and equality operators
 *		for a WindowAgg node.
 *
 * This depends on the behavior of make_pathkeys_for_window()!
 *
 * We are given the target WindowClause and an array of the input column
 * numbers associated with the resulting pathkeys.  In the easy case, there
 * are the same number of pathkey columns as partitioning + ordering columns
 * and we just have to copy some data around.  However, it's possible that
 * some of the original partitioning + ordering columns were eliminated as
 * redundant during the transformation to pathkeys.  (This can happen even
 * though the parser gets rid of obvious duplicates.  A typical scenario is a
 * window specification "PARTITION BY x ORDER BY y" coupled with a clause
 * "WHERE x = y" that causes the two sort columns to be recognized as
 * redundant.)	In that unusual case, we have to work a lot harder to
 * determine which keys are significant.
 *
 * The method used here is a bit brute-force: add the sort columns to a list
 * one at a time and note when the resulting pathkey list gets longer.  But
 * it's a sufficiently uncommon case that a faster way doesn't seem worth
 * the amount of code refactoring that'd be needed.
 *----------
 */
static void
get_column_info_for_window(PlannerInfo *root, WindowClause *wc, List *tlist,
						   int numSortCols, AttrNumber *sortColIdx,
						   int *partNumCols,
						   AttrNumber **partColIdx,
						   Oid **partOperators,
						   int *ordNumCols,
						   AttrNumber **ordColIdx,
						   Oid **ordOperators)
{
	int			numPart = list_length(wc->partitionClause);
	int			numOrder = list_length(wc->orderClause);

	if (numSortCols == numPart + numOrder)
	{
		/* easy case */
		*partNumCols = numPart;
		*partColIdx = sortColIdx;
		*partOperators = extract_grouping_ops(wc->partitionClause);
		*ordNumCols = numOrder;
		*ordColIdx = sortColIdx + numPart;
		*ordOperators = extract_grouping_ops(wc->orderClause);
	}
	else
	{
		List	   *sortclauses;
		List	   *pathkeys;
		int			scidx;
		ListCell   *lc;

		/* first, allocate what's certainly enough space for the arrays */
		*partNumCols = 0;
		*partColIdx = (AttrNumber *) palloc(numPart * sizeof(AttrNumber));
		*partOperators = (Oid *) palloc(numPart * sizeof(Oid));
		*ordNumCols = 0;
		*ordColIdx = (AttrNumber *) palloc(numOrder * sizeof(AttrNumber));
		*ordOperators = (Oid *) palloc(numOrder * sizeof(Oid));
		sortclauses = NIL;
		pathkeys = NIL;
		scidx = 0;
		foreach(lc, wc->partitionClause)
		{
			SortGroupClause *sgc = (SortGroupClause *) lfirst(lc);
			List	   *new_pathkeys;

			sortclauses = lappend(sortclauses, sgc);
			new_pathkeys = make_pathkeys_for_sortclauses(root,
														 sortclauses,
														 tlist);
			if (list_length(new_pathkeys) > list_length(pathkeys))
			{
				/* this sort clause is actually significant */
				(*partColIdx)[*partNumCols] = sortColIdx[scidx++];
				(*partOperators)[*partNumCols] = sgc->eqop;
				(*partNumCols)++;
				pathkeys = new_pathkeys;
			}
		}
		foreach(lc, wc->orderClause)
		{
			SortGroupClause *sgc = (SortGroupClause *) lfirst(lc);
			List	   *new_pathkeys;

			sortclauses = lappend(sortclauses, sgc);
			new_pathkeys = make_pathkeys_for_sortclauses(root,
														 sortclauses,
														 tlist);
			if (list_length(new_pathkeys) > list_length(pathkeys))
			{
				/* this sort clause is actually significant */
				(*ordColIdx)[*ordNumCols] = sortColIdx[scidx++];
				(*ordOperators)[*ordNumCols] = sgc->eqop;
				(*ordNumCols)++;
				pathkeys = new_pathkeys;
			}
		}
		/* complain if we didn't eat exactly the right number of sort cols */
		if (scidx != numSortCols)
			elog(ERROR, "failed to deconstruct sort operators into partitioning/ordering operators");
	}
}

/*
 * expression_planner
 *		Perform planner's transformations on a standalone expression.
 *
 * Various utility commands need to evaluate expressions that are not part
 * of a plannable query.  They can do so using the executor's regular
 * expression-execution machinery, but first the expression has to be fed
 * through here to transform it from parser output to something executable.
 *
 * Currently, we disallow sublinks in standalone expressions, so there's no
 * real "planning" involved here.  (That might not always be true though.)
 * What we must do is run eval_const_expressions to ensure that any function
 * calls are converted to positional notation and function default arguments
 * get inserted.  The fact that constant subexpressions get simplified is a
 * side-effect that is useful when the expression will get evaluated more than
 * once.  Also, we must fix operator function IDs.
 *
 * Note: this must not make any damaging changes to the passed-in expression
 * tree.  (It would actually be okay to apply fix_opfuncids to it, but since
 * we first do an expression_tree_mutator-based walk, what is returned will
 * be a new node tree.)
 */
Expr *
expression_planner(Expr *expr)
{
	Node	   *result;

	/*
	 * Convert named-argument function calls, insert default arguments and
	 * simplify constant subexprs
	 */
	result = eval_const_expressions(NULL, (Node *) expr);

	/* Fill in opfuncid values if missing */
	fix_opfuncids(result);

	return (Expr *) result;
}


/*
 * plan_cluster_use_sort
 *		Use the planner to decide how CLUSTER should implement sorting
 *
 * tableOid is the OID of a table to be clustered on its index indexOid
 * (which is already known to be a btree index).  Decide whether it's
 * cheaper to do an indexscan or a seqscan-plus-sort to execute the CLUSTER.
 * Return TRUE to use sorting, FALSE to use an indexscan.
 *
 * Note: caller had better already hold some type of lock on the table.
 */
bool
plan_cluster_use_sort(Oid tableOid, Oid indexOid)
{
	PlannerInfo *root;
	Query	   *query;
	PlannerGlobal *glob;
	RangeTblEntry *rte;
	RelOptInfo *rel;
	IndexOptInfo *indexInfo;
	QualCost	indexExprCost;
	Cost		comparisonCost;
	Path	   *seqScanPath;
	Path		seqScanAndSortPath;
	IndexPath  *indexScanPath;
	ListCell   *lc;

	/* Set up mostly-dummy planner state */
	query = makeNode(Query);
	query->commandType = CMD_SELECT;

	glob = makeNode(PlannerGlobal);

	root = makeNode(PlannerInfo);
	root->parse = query;
	root->glob = glob;
	root->query_level = 1;
	root->planner_cxt = CurrentMemoryContext;
	root->wt_param_id = -1;

	root->config = DefaultPlannerConfig();

	/* Build a minimal RTE for the rel */
	rte = makeNode(RangeTblEntry);
	rte->rtekind = RTE_RELATION;
	rte->relid = tableOid;
	rte->relkind = RELKIND_RELATION;	/* Don't be too picky. */
	rte->lateral = false;
	rte->inh = false;
	rte->inFromCl = true;
	query->rtable = list_make1(rte);

	/* Set up RTE/RelOptInfo arrays */
	setup_simple_rel_arrays(root);

	/* Build RelOptInfo */
	rel = build_simple_rel(root, 1, RELOPT_BASEREL);

	/* Locate IndexOptInfo for the target index */
	indexInfo = NULL;
	foreach(lc, rel->indexlist)
	{
		indexInfo = (IndexOptInfo *) lfirst(lc);
		if (indexInfo->indexoid == indexOid)
			break;
	}

	/*
	 * It's possible that get_relation_info did not generate an IndexOptInfo
	 * for the desired index; this could happen if it's not yet reached its
	 * indcheckxmin usability horizon, or if it's a system index and we're
	 * ignoring system indexes.  In such cases we should tell CLUSTER to not
	 * trust the index contents but use seqscan-and-sort.
	 */
	if (lc == NULL)				/* not in the list? */
		return true;			/* use sort */

	/*
	 * Rather than doing all the pushups that would be needed to use
	 * set_baserel_size_estimates, just do a quick hack for rows and width.
	 */
	rel->rows = rel->tuples;
	rel->width = get_relation_data_width(tableOid, NULL);

	root->total_table_pages = rel->pages;

	/*
	 * Determine eval cost of the index expressions, if any.  We need to
	 * charge twice that amount for each tuple comparison that happens during
	 * the sort, since tuplesort.c will have to re-evaluate the index
	 * expressions each time.  (XXX that's pretty inefficient...)
	 */
	cost_qual_eval(&indexExprCost, indexInfo->indexprs, root);
	comparisonCost = 2.0 * (indexExprCost.startup + indexExprCost.per_tuple);

	/* Estimate the cost of seq scan + sort */
	seqScanPath = create_seqscan_path(root, rel, NULL);
	cost_sort(&seqScanAndSortPath, root, NIL,
			  seqScanPath->total_cost, rel->tuples, rel->width,
			  comparisonCost, maintenance_work_mem, -1.0);

	/* Estimate the cost of index scan */
	indexScanPath = create_index_path(root, indexInfo,
									  NIL, NIL, NIL, NIL, NIL,
									  ForwardScanDirection, false,
									  NULL, 1.0);

	return (seqScanAndSortPath.total_cost < indexScanPath->path.total_cost);
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
