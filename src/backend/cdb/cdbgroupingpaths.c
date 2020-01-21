/*-------------------------------------------------------------------------
 *
 * cdbgroupingpaths.c
 *	  Routines to aid in planning grouping queries for parallel
 *    execution.  This is, essentially, an extension of the file
 *    optimizer/prep/planner.c, although some functions are not
 *    externalized.
 *
 * Portions Copyright (c) 2019-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbgroupingpaths.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "cdb/cdbgroupingpaths.h"
#include "cdb/cdbhash.h"
#include "cdb/cdbpath.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "executor/execHHashagg.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/tlist.h"
#include "optimizer/var.h"
#include "parser/parse_clause.h"
#include "parser/parse_oper.h"
#include "utils/lsyscache.h"

/*
 * For convenience, we collect various inputs and intermediate planning results
 * in this struct, instead of passing a dozen arguments to all subroutines.
 */
typedef struct
{
	/* Inputs from the caller */
	PathTarget *target;
	PathTarget *partial_grouping_target;
	double		dNumGroups;
	const AggClauseCosts *agg_costs;
	const AggClauseCosts *agg_partial_costs;
	const AggClauseCosts *agg_final_costs;
	List *rollup_lists;
	List *rollup_groupclauses;
} cdb_agg_planning_context;

static Index add_gsetid_tlist(List *tlist);

static List *add_gsetid_groupclause(List *groupClause, Index groupref);

static void add_twostage_group_agg_path(PlannerInfo *root,
										Path *path,
										bool is_sorted,
										cdb_agg_planning_context *ctx,
										RelOptInfo *output_rel);

static void add_twostage_hash_agg_path(PlannerInfo *root,
									   Path *path,
									   cdb_agg_planning_context *ctx,
									   RelOptInfo *output_rel);

static void add_single_dqa_hash_agg_path(PlannerInfo *root,
										 Path *path,
										 cdb_agg_planning_context *ctx,
										 RelOptInfo *output_rel);

static bool analyze_dqas(PlannerInfo *root,
						 Path *path,
						 cdb_agg_planning_context *ctx,
						 PathTarget **dqa_input_target_p,
						 List	   **dqa_group_clause_p);

/*
 * Function: cdb_grouping_planner
 *
 * This is basically an extension of the function create_grouping_paths() from
 * planner.c.  It creates two- and three-stage Paths to implement aggregates
 * and/or GROUP BY.
 */
void
cdb_create_twostage_grouping_paths(PlannerInfo *root,
								   RelOptInfo *input_rel,
								   RelOptInfo *output_rel,
								   PathTarget *target,
								   PathTarget *partial_grouping_target,
								   bool can_sort,
								   bool can_hash,
								   double dNumGroups,
								   const AggClauseCosts *agg_costs,
								   const AggClauseCosts *agg_partial_costs,
								   const AggClauseCosts *agg_final_costs,
								   List *rollup_lists,
								   List *rollup_groupclauses)
{
	Query	   *parse = root->parse;
	Path	   *cheapest_path = input_rel->cheapest_total_path;
	bool		has_ordered_aggs = agg_costs->numPureOrderedAggs > 0;
	cdb_agg_planning_context cxt;

	/* The caller should've checked these already */
	Assert(parse->hasAggs || parse->groupClause);
	/*
	 * This prohibition could be relaxed if we tracked missing combine
	 * functions per DQA and were willing to plan some DQAs as single and
	 * some as multiple phases.  Not currently, however.
	 */
	Assert(!agg_costs->hasNonCombine && !agg_costs->hasNonSerial);
	Assert(root->config->gp_enable_multiphase_agg);

	/* The caller already constructed a one-stage plan. */

	/*
	 * Ordered aggregates need to run the transition function on the
	 * values in sorted order, which in turn translates into single phase
	 * aggregation.
	 */
	if (has_ordered_aggs)
		return;

	/*
	 * We are currently unwilling to redistribute a gathered intermediate
	 * across the cluster.  This might change one day.
	 */
	if (!CdbPathLocus_IsPartitioned(cheapest_path->locus))
		return;

	memset(&cxt, 0, sizeof(cxt));
	cxt.target = target;
	cxt.partial_grouping_target = partial_grouping_target;
	cxt.dNumGroups = dNumGroups;
	cxt.agg_costs = agg_costs;
	cxt.agg_partial_costs = agg_partial_costs;
	cxt.agg_final_costs = agg_final_costs;
	cxt.rollup_lists = rollup_lists;
	cxt.rollup_groupclauses = rollup_groupclauses;

	/*
	 * Consider 2-phase aggs
	 */
	if (can_sort)
	{
		ListCell   *lc;

		foreach(lc, input_rel->pathlist)
		{
			Path	   *path = (Path *) lfirst(lc);
			bool		is_sorted;

			is_sorted = pathkeys_contained_in(root->group_pathkeys,
											  path->pathkeys);
			if (path == cheapest_path || is_sorted)
			{
				add_twostage_group_agg_path(root,
											path,
											is_sorted,
											&cxt,
											output_rel);
			}
		}
	}

	if (can_hash && list_length(agg_costs->distinctAggrefs) == 0)
	{
		add_twostage_hash_agg_path(root,
								   cheapest_path,
								   &cxt,
								   output_rel);
	}

	if ((can_hash || parse->groupClause == NIL) && list_length(agg_costs->distinctAggrefs) > 0)
	{
		/*
		 * Try possible plans for DISTINCT-qualified aggregate.
		 */
		add_single_dqa_hash_agg_path(root,
									 cheapest_path,
									 &cxt,
									 output_rel);
	}
	/*
	 * GPDB_96_MERGE_FIXME: multiple DISTINCT-Qualified Aggregates
	 * not re-implemented yet
	 */
}

/*
 * Add a TargetEntry node of type GroupingSetId to the tlist.
 * Return its ressortgroupref.
 */
static Index
add_gsetid_tlist(List *tlist)
{
	TargetEntry *tle;
	GroupingSetId *gsetid;
	ListCell *lc;

	foreach(lc, tlist)
	{
		tle = lfirst_node(TargetEntry, lc);
		if (IsA(tle->expr, GroupingSetId))
			elog(ERROR, "GROUPINGSET_ID already exists in tlist");
	}

	gsetid = makeNode(GroupingSetId);
	tle = makeTargetEntry((Expr *)gsetid, list_length(tlist) + 1,
			"GROUPINGSET_ID", true);
	assignSortGroupRef(tle, tlist);
	tlist = lappend(tlist, tle);

	return tle->ressortgroupref;
}

/*
 * Add a SortGroupClause node to the groupClause representing the GroupingSetId.
 * Note we insert the new node to the head of groupClause.
 */
static List *
add_gsetid_groupclause(List *groupClause, Index groupref)
{
	SortGroupClause *gc;
	Oid         sortop;
	Oid         eqop;
	bool        hashable;

	get_sort_group_operators(INT4OID,
			false, true, false,
			&sortop, &eqop, NULL,
			&hashable);

	gc = makeNode(SortGroupClause);
	gc->tleSortGroupRef = groupref;
	gc->eqop = eqop;
	gc->sortop = sortop;
	gc->nulls_first = false;
	gc->hashable = hashable;

	groupClause = lcons(gc, groupClause);

	return groupClause;
}

static void
add_twostage_group_agg_path(PlannerInfo *root,
							Path *path,
							bool is_sorted,
							cdb_agg_planning_context *ctx,
							RelOptInfo *output_rel)
{
	Query	   *parse = root->parse;
	CdbPathLocus singleQE_locus;
	Path	   *initial_agg_path = NULL;
	CdbPathLocus group_locus;
	bool		need_redistribute;
	int			extra_aggsplitops = 0;
	List		*motion_pathkeys = NIL;
	List		*grouping_sets_tlist = NIL;
	List		*grouping_sets_groupClause = NIL;
	PathTarget	*grouping_sets_partial_target = NULL;

	/*
	 * For twostage grouping sets, we perform grouping sets aggregation in
	 * partial stage and normal aggregation in final stage.
	 *
	 * With this method, there is a problem, i.e., in the final stage of
	 * aggregation, we don't have a way to distinguish which tuple comes from
	 * which grouping set, which turns out to be needed for merging the partial
	 * results.
	 *
	 * For instance, suppose we have a table t(c1, c2, c3) containing one row
	 * (1, NULL, 3), and we are selecting agg(c3) group by grouping sets
	 * ((c1,c2), (c1)). Then there would be two tuples as partial results for
	 * that row, both are (1, NULL, agg(3)), one is from group by (c1,c2) and
	 * one is from group by (c1). If we cannot tell that the two tuples are
	 * from two different grouping sets, we will merge them incorrectly.
	 *
	 * So we add a hidden column 'GROUPINGSET_ID', representing grouping set
	 * id, to the targetlist of Partial Aggregate node, as well as to the sort
	 * keys and group keys for Finalize Aggregate node. So only tuples coming
	 * from the same grouping set can get merged in the final stage of
	 * aggregation. Note that we need to keep 'GROUPINGSET_ID' at the head of
	 * sort keys in final stage to ensure correctness.
	 *
	 *
	 * Below is a plan to illustrate this idea:
	 *
	 * # explain (costs off, verbose)
	 * select c1, c2, c3, avg(c3) from gstest group by grouping sets((c1,c2),(c1),(c2,c3));
	 *                                 QUERY PLAN
	 * ---------------------------------------------------------------------------
	 *  Finalize GroupAggregate
	 *    Output: c1, c2, c3, avg(c3)
	 *    Group Key: (GROUPINGSET_ID()), gstest.c1, gstest.c2, gstest.c3
	 *    ->  Sort
	 *          Output: c1, c2, c3, (PARTIAL avg(c3)), (GROUPINGSET_ID())
	 *          Sort Key: (GROUPINGSET_ID()), gstest.c1, gstest.c2, gstest.c3
	 *          ->  Gather Motion 3:1  (slice1; segments: 3)
	 *                Output: c1, c2, c3, (PARTIAL avg(c3)), (GROUPINGSET_ID())
	 *                ->  Partial GroupAggregate
	 *                      Output: c1, c2, c3, PARTIAL avg(c3), GROUPINGSET_ID()
	 *                      Group Key: gstest.c1, gstest.c2
	 *                      Group Key: gstest.c1
	 *                      Sort Key: gstest.c2, gstest.c3
	 *                        Group Key: gstest.c2, gstest.c3
	 *                      ->  Sort
	 *                            Output: c1, c2, c3
	 *                            Sort Key: gstest.c1, gstest.c2
	 *                            ->  Seq Scan on public.gstest
	 *                                  Output: c1, c2, c3
	 *  Optimizer: Postgres query optimizer
	 * (20 rows)
	 *
	 */
	if (parse->groupingSets)
	{
		Index groupref;
		GroupingSetId *gsetid = makeNode(GroupingSetId);

		grouping_sets_tlist = copyObject(root->processed_tlist);
		groupref = add_gsetid_tlist(grouping_sets_tlist);

		grouping_sets_groupClause =
			add_gsetid_groupclause(copyObject(parse->groupClause), groupref);

		grouping_sets_partial_target = copyObject(ctx->partial_grouping_target);
		if (!list_member(grouping_sets_partial_target->exprs, gsetid))
			add_column_to_pathtarget(grouping_sets_partial_target,
									 (Expr *)gsetid, groupref);
	}

	group_locus = cdb_choose_grouping_locus(root, path, ctx->target,
											parse->groupClause,
											ctx->rollup_lists,
											ctx->rollup_groupclauses,
											&need_redistribute);
	/*
	 * If the distribution of this path is suitable, two-stage aggregation
	 * is not applicable.
	 */
	if (!need_redistribute)
		return;

	if (ctx->agg_costs->distinctAggrefs)
	{
		PathTarget *input_target;
		List	   *dqa_group_clause;
		CdbPathLocus distinct_locus;
		bool		distinct_need_redistribute;

		if (!analyze_dqas(root, path, ctx, &input_target, &dqa_group_clause))
			return;

		path = (Path *) create_projection_path(root, path->parent, path, input_target);

		distinct_locus = cdb_choose_grouping_locus(root, path,
												   input_target,
												   dqa_group_clause, NIL, NIL,
												   &distinct_need_redistribute);

		/* If the input distribution matches the distinct, we can proceed */
		if (distinct_need_redistribute)
			return;

		extra_aggsplitops = AGGSPLITOP_DEDUPLICATED;
	}

	if (!is_sorted)
	{
		path = (Path *) create_sort_path(root,
										 output_rel,
										 path,
										 root->group_pathkeys,
										 -1.0);
	}

	if (parse->groupingSets)
	{
		/*
		 * We have grouping sets, possibly with aggregation.  Make
		 * a GroupingSetsPath.
		 */
		initial_agg_path =
			(Path *) create_groupingsets_path(root,
											  output_rel,
											  path,
											  grouping_sets_partial_target,
											  AGGSPLIT_INITIAL_SERIAL,
											  (List *) parse->havingQual,
											  ctx->rollup_lists,
											  ctx->rollup_groupclauses,
											  ctx->agg_partial_costs,
											  ctx->dNumGroups * getgpsegmentCount());

		motion_pathkeys = NIL;
	}
	else if (parse->hasAggs || parse->groupClause)
	{

		initial_agg_path =
			(Path *) create_agg_path(root,
									 output_rel,
									 path,
									 ctx->partial_grouping_target,
									 parse->groupClause ? AGG_SORTED : AGG_PLAIN,
									 AGGSPLIT_INITIAL_SERIAL,
									 false, /* streaming */
									 parse->groupClause,
									 NIL,
									 ctx->agg_partial_costs,
									 ctx->dNumGroups * getgpsegmentCount(),
									 NULL);

		motion_pathkeys = initial_agg_path->pathkeys;
	}
	else
	{
		Assert(false);
	}

	/*
	 * GroupAgg -> GATHER MOTION -> GroupAgg.
	 *
	 * This has the advantage that it retains the input order. The
	 * downside is that it gathers everything to a single node. If that's
	 * where the final result is needed anyway, that's quite possibly better
	 * than scattering the partial aggregate results and having another
	 * motion to gather the final results, though,
	 *
	 * Alternatively, we could redistribute based on the GROUP BY key. That
	 * would have the advantage that the Finalize Agg stage could run in
	 * parallel. However, it would destroy the sort order, so it's probaly
	 * not a good idea.
	 */
	CdbPathLocus_MakeSingleQE(&singleQE_locus, getgpsegmentCount());
	path = cdbpath_create_motion_path(root,
									  initial_agg_path,
									  motion_pathkeys,
									  false,
									  singleQE_locus);

	if (parse->groupingSets)
	{

		List		*pathkeys;

		pathkeys = make_pathkeys_for_sortclauses(root,
												 grouping_sets_groupClause,
												 grouping_sets_tlist);

		path = (Path *) create_sort_path(root,
										 output_rel,
										 path,
										 pathkeys,
										 -1.0);

		path = (Path *) create_agg_path(root,
										output_rel,
										path,
										ctx->target,
										AGG_SORTED,
										AGGSPLIT_FINAL_DESERIAL,
										false, /* streaming */
										grouping_sets_groupClause,
										(List *) parse->havingQual,
										ctx->agg_final_costs,
										ctx->dNumGroups,
										NULL);
	}
	else if (parse->hasAggs || parse->groupClause)
	{
		path = (Path *) create_agg_path(root,
										output_rel,
										path,
										ctx->target,
										parse->groupClause ? AGG_SORTED : AGG_PLAIN,
										AGGSPLIT_FINAL_DESERIAL,
										false, /* streaming */
										parse->groupClause,
										(List *) parse->havingQual,
										ctx->agg_final_costs,
										ctx->dNumGroups,
										NULL);
	}
	else
	{
		Assert(false);
	}

	add_path(output_rel, path);
}

static void
add_twostage_hash_agg_path(PlannerInfo *root,
						   Path *path,
						   cdb_agg_planning_context *ctx,
						   RelOptInfo *output_rel)
{
	Query	   *parse = root->parse;
	Path	   *initial_agg_path;
	CdbPathLocus group_locus;
	bool		need_redistribute;
	HashAggTableSizes hash_info;

	group_locus = cdb_choose_grouping_locus(root, path, ctx->target,
											parse->groupClause, NIL, NIL,
											&need_redistribute);
	/*
	 * If the distribution of this path is suitable, two-stage aggregation
	 * is not applicable.
	 */
	if (!need_redistribute)
		return;

	if (!calcHashAggTableSizes(work_mem * 1024L,
							   ctx->dNumGroups,
							   path->pathtarget->width,
							   false,	/* force */
							   &hash_info))
		return;	/* don't try to hash */

	initial_agg_path = (Path *) create_agg_path(root,
												output_rel,
												path,
												ctx->partial_grouping_target,
												AGG_HASHED,
												AGGSPLIT_INITIAL_SERIAL,
												false, /* streaming */
												parse->groupClause,
												NIL,
												ctx->agg_partial_costs,
												ctx->dNumGroups * getgpsegmentCount(),
												&hash_info);

	/*
	 * HashAgg -> Redistribute or Gather Motion -> HashAgg.
	 */
	path = cdbpath_create_motion_path(root, initial_agg_path, NIL, false,
									  group_locus);

	path = (Path *) create_agg_path(root,
									output_rel,
									path,
									ctx->target,
									AGG_HASHED,
									AGGSPLIT_FINAL_DESERIAL,
									false, /* streaming */
									parse->groupClause,
									(List *) parse->havingQual,
									ctx->agg_final_costs,
									ctx->dNumGroups,
									&hash_info);
	add_path(output_rel, path);
}

static Node *
strip_aggdistinct_mutator(Node *node, void *context)
{
	if (node == NULL)
		return NULL;

	if (IsA(node, Aggref))
	{
		Aggref	   *newAggref = (Aggref *) copyObject(node);

		newAggref->aggdistinct = NIL;

		node = (Node *) newAggref;
	}
	return expression_tree_mutator(node, strip_aggdistinct_mutator, context);
}

static PathTarget *
strip_aggdistinct(PathTarget *target)
{
	PathTarget *result;

	result = copy_pathtarget(target);
	result->exprs = (List *) strip_aggdistinct_mutator((Node *) result->exprs, NULL);

	return result;
}

static bool
analyze_dqas(PlannerInfo *root,
			 Path *path,
			 cdb_agg_planning_context *ctx,
			 PathTarget **dqa_input_target_p,
			 List	   **dqa_group_clause_p)
{
	Query	   *parse = root->parse;
	PathTarget *input_target;
	List	   *dqa_group_clause;
	ListCell   *lc;
	Index		sortgroupref;
	Index		maxRef;

	/* Prepare a modifiable copy of the input path target */
	input_target = copy_pathtarget(path->pathtarget);
	maxRef = 0;
	if (input_target->sortgrouprefs)
	{
		for (int idx = 0; idx < list_length(input_target->exprs); idx++)
		{
			if (input_target->sortgrouprefs[idx] > maxRef)
				maxRef = input_target->sortgrouprefs[idx];
		}
	}
	else
		input_target->sortgrouprefs = (Index *) palloc0(list_length(input_target->exprs) * sizeof(Index));

	/*
	 * Analyze the DISTINCT argument, to see if it's something we can
	 * support.
	 */
	dqa_group_clause = list_copy(parse->groupClause);
	sortgroupref = 0;
	foreach (lc, ctx->agg_costs->distinctAggrefs)
	{
		Aggref	   *aggref = (Aggref *) lfirst(lc);
		SortGroupClause *arg_sortcl;
		SortGroupClause *sortcl = NULL;
		TargetEntry *arg_tle;
		int			idx;
		ListCell   *lcc;

		if (list_length(aggref->aggdistinct) != 1)
			return false;		/* I don't think the parser can even produce this */

		arg_sortcl = (SortGroupClause *) linitial(aggref->aggdistinct);
		arg_tle = get_sortgroupref_tle(arg_sortcl->tleSortGroupRef, aggref->args);

		if (!arg_sortcl->hashable)
		{
			/*
			 * XXX: I'm not sure if the hashable flag is always set correctly
			 * for DISTINCT args. DISTINCT aggs are never implemented with hashing
			 * in PostgreSQL.
			 */
			return false;
		}

		/* Now find this expression in the sub-path's target list */
		idx = 0;
		foreach(lcc, input_target->exprs)
		{
			Expr		*expr = lfirst(lcc);

			if (equal(expr, arg_tle->expr))
				break;
			idx++;
		}
		if (idx == list_length(input_target->exprs))
			add_column_to_pathtarget(input_target, arg_tle->expr, ++maxRef);
		else if (input_target->sortgrouprefs[idx] == 0)
			input_target->sortgrouprefs[idx] = ++maxRef;

		sortcl = copyObject(arg_sortcl);
		sortcl->tleSortGroupRef = input_target->sortgrouprefs[idx];
		sortcl->hashable = true;	/* we verified earlier that it's hashable */

		/*
		 * There can be only one DISTINCT-qualified aggregate.
		 */
		if (sortgroupref == 0)
			sortgroupref = sortcl->tleSortGroupRef;
		else if (sortgroupref != sortcl->tleSortGroupRef)
		{
			return false;
		}
		else
			continue;

		dqa_group_clause = lappend(dqa_group_clause, sortcl);
	}

	/* Check that there are no non-DISTINCT aggregates mixed in. */
	List *varnos = pull_var_clause((Node *) ctx->target->exprs,
								   PVC_INCLUDE_AGGREGATES |
								   PVC_INCLUDE_WINDOWFUNCS |
								   PVC_INCLUDE_PLACEHOLDERS);
	foreach (lc, varnos)
	{
		Node	   *node = lfirst(lc);

		if (IsA(node, Aggref))
		{
			Aggref	   *aggref = (Aggref *) node;

			if (!aggref->aggdistinct)
			{
				/* mixing DISTINCT and non-DISTINCT aggs not supported */
				return false;
			}
		}
	}

	/* Ok, we can do this */
	*dqa_input_target_p = input_target;
	*dqa_group_clause_p = dqa_group_clause;
	return true;
}

/*
 * Create Paths for an Aggregate with one DISTINCT-qualified aggregate.
 */
static void
add_single_dqa_hash_agg_path(PlannerInfo *root,
							 Path *path,
							 cdb_agg_planning_context *ctx,
							 RelOptInfo *output_rel)
{
	List	   *dqa_group_clause;
	Query	   *parse = root->parse;
	CdbPathLocus group_locus;
	bool		group_need_redistribute;
	CdbPathLocus distinct_locus;
	bool		distinct_need_redistribute;
	PathTarget *input_target = NULL;
	HashAggTableSizes hash_info;

	if (!gp_enable_agg_distinct)
		return;

	if (!analyze_dqas(root, path, ctx, &input_target, &dqa_group_clause))
		return;


	/*
	 * GPDB_96_MERGE_FIXME: compute the hash table size once. But we create
	 * several different Hash Aggs below, depending on the query. Is this
	 * computation sensible for all of them?
	 */
	if (!calcHashAggTableSizes(work_mem * 1024L,
							   ctx->dNumGroups,
							   path->pathtarget->width,
							   false,	/* force */
							   &hash_info))
		return;	/* don't try to hash */

	path = (Path *) create_projection_path(root, path->parent, path, input_target);

	distinct_locus = cdb_choose_grouping_locus(root, path,
											   input_target,
											   dqa_group_clause, NIL, NIL,
											   &distinct_need_redistribute);
	group_locus = cdb_choose_grouping_locus(root, path,
											input_target,
											parse->groupClause, NIL, NIL,
											&group_need_redistribute);
	if (!distinct_need_redistribute || ! group_need_redistribute)
	{
		/*
		 * 1. If the input's locus matches the DISTINCT, but not GROUP BY:
		 *
		 *  HashAggregate
		 *     -> Redistribute (according to GROUP BY)
		 *         -> HashAggregate (to eliminate duplicates)
		 *             -> input (hashed by GROUP BY + DISTINCT)
		 *
		 * 2. If the input's locus matches the GROUP BY:
		 *
		 *  HashAggregate (to aggregate)
		 *     -> HashAggregate (to eliminate duplicates)
		 *           -> input (hashed by GROUP BY)
		 *
		 * The main planner should already have created the single-stage
		 * Group Agg path.
		 *
		 * XXX: not sure if this makes sense. If hash distinct is a good
		 * idea, why doesn't PostgreSQL's agg node implement that?
		 */
		path = (Path *) create_agg_path(root,
										output_rel,
										path,
										input_target,
										AGG_HASHED,
										AGGSPLIT_SIMPLE,
										false, /* streaming */
										dqa_group_clause,
										NIL,
										ctx->agg_partial_costs, /* FIXME */
										ctx->dNumGroups * getgpsegmentCount(),
										&hash_info);

		if (group_need_redistribute)
			path = cdbpath_create_motion_path(root, path, NIL, false,
											  group_locus);

		path = (Path *) create_agg_path(root,
										output_rel,
										path,
										ctx->target,
										parse->groupClause ? AGG_HASHED : AGG_PLAIN,
										AGGSPLIT_DEDUPLICATED,
										false, /* streaming */
										parse->groupClause,
										(List *) parse->havingQual,
										ctx->agg_final_costs,
										ctx->dNumGroups,
										&hash_info);
		add_path(output_rel, path);
	}
	else if (CdbPathLocus_IsHashed(group_locus))
	{
		/*
		 *  HashAgg (to aggregate)
		 *     -> HashAgg (to eliminate duplicates)
		 *          -> Redistribute (according to GROUP BY)
		 *               -> Streaming HashAgg (to eliminate duplicates)
		 *                    -> input
		 *
		 * It may seem silly to have two Aggs on top of each other like this,
		 * but the Agg node can't do DISTINCT-aggregation by hashing at the
		 * moment. So we have to do it with two separate Aggs steps.
		 */
		if (gp_enable_dqa_pruning)
			path = (Path *) create_agg_path(root,
											output_rel,
											path,
											input_target,
											AGG_HASHED,
											AGGSPLIT_SIMPLE,
											true, /* streaming */
											dqa_group_clause,
											NIL,
											ctx->agg_partial_costs, /* FIXME */
											ctx->dNumGroups * getgpsegmentCount(),
											&hash_info);

		path = cdbpath_create_motion_path(root, path, NIL, false,
										  group_locus);
		path = (Path *) create_agg_path(root,
										output_rel,
										path,
										input_target,
										AGG_HASHED,
										AGGSPLIT_SIMPLE,
										false, /* streaming */
										dqa_group_clause,
										NIL,
										ctx->agg_partial_costs, /* FIXME */
										ctx->dNumGroups * getgpsegmentCount(),
										&hash_info);

		path = (Path *) create_agg_path(root,
										output_rel,
										path,
										ctx->target,
										parse->groupClause ? AGG_HASHED : AGG_PLAIN,
										AGGSPLIT_DEDUPLICATED,
										false, /* streaming */
										parse->groupClause,
										(List *) parse->havingQual,
										ctx->agg_final_costs,
										ctx->dNumGroups,
										&hash_info);
		add_path(output_rel, path);
	}
	else if (CdbPathLocus_IsHashed(distinct_locus))
	{
		/*
		 *  Finalize Aggregate
		 *     -> Gather Motion
		 *          -> Partial Aggregate
		 *              -> HashAggregate, to remove duplicates
		 *                  -> Redistribute Motion (according to DISTINCT arg)
		 *                      -> Streaming HashAgg (to eliminate duplicates)
		 *                          -> input
		 */
		path = (Path *) create_agg_path(root,
										output_rel,
										path,
										input_target,
										AGG_HASHED,
										AGGSPLIT_SIMPLE,
										true, /* streaming */
										dqa_group_clause,
										NIL,
										ctx->agg_partial_costs, /* FIXME */
										ctx->dNumGroups * getgpsegmentCount(),
										&hash_info);

		path = cdbpath_create_motion_path(root, path, NIL, false,
										  distinct_locus);
		path = (Path *) create_agg_path(root,
										output_rel,
										path,
										input_target,
										AGG_HASHED,
										AGGSPLIT_SIMPLE,
										false, /* streaming */
										dqa_group_clause,
										NIL,
										ctx->agg_partial_costs, /* FIXME */
										ctx->dNumGroups * getgpsegmentCount(),
										&hash_info);

		path = (Path *) create_agg_path(root,
										output_rel,
										path,
										strip_aggdistinct(ctx->partial_grouping_target),
										parse->groupClause ? AGG_HASHED : AGG_PLAIN,
										AGGSPLIT_INITIAL_SERIAL | AGGSPLITOP_DEDUPLICATED,
										false, /* streaming */
										parse->groupClause,
										NIL,
										ctx->agg_partial_costs,
										ctx->dNumGroups * getgpsegmentCount(),
										&hash_info);
		path = cdbpath_create_motion_path(root, path, NIL, false,
										  group_locus);

		path = (Path *) create_agg_path(root,
										output_rel,
										path,
										ctx->target,
										parse->groupClause ? AGG_HASHED : AGG_PLAIN,
										AGGSPLIT_FINAL_DESERIAL | AGGSPLITOP_DEDUPLICATED,
										false, /* streaming */
										parse->groupClause,
										(List *) parse->havingQual,
										ctx->agg_final_costs,
										ctx->dNumGroups,
										&hash_info);

		add_path(output_rel, path);
	}
	else
		return;
}

/*
 * Figure out the desired data distribution to perform the grouping.
 *
 * In case of a simple GROUP BY, we prefer to distribute the data according to
 * the GROUP BY. With multiple grouping sets, identify the set of common
 * entries, and distribute based on that. For example, if you do
 * GROUP BY GROUPING SETS ((a, b, c), (b, c)), the common cols are b and c.
 */
CdbPathLocus
cdb_choose_grouping_locus(PlannerInfo *root, Path *path,
					  PathTarget *target,
					  List *groupClause,
					  List *rollup_lists,
					  List *rollup_groupclauses,
					  bool *need_redistribute_p)
{
	List	   *tlist = make_tlist_from_pathtarget(target);
	CdbPathLocus locus;
	bool		need_redistribute;

	/*
	 * If the input is already collected to a single segment, just perform the
	 * aggregation there. We could redistribute it, so that we could perform
	 * the aggregation in parallel, but Motions are pretty expensive so it's
	 * probably not worthwhile.
	 */
	if (CdbPathLocus_IsBottleneck(path->locus))
	{
		need_redistribute = false;
		CdbPathLocus_MakeNull(&locus);
	}
	else
	{
		List	   *group_tles;
		List	   *hash_exprs;
		List	   *hash_opfamilies;
		List	   *hash_sortrefs;
		ListCell   *lc;
		Bitmapset  *common_groupcols = NULL;
		bool		first = true;
		int			x;

		if (rollup_lists)
		{
			ListCell   *lcl, *lcc;

			forboth(lcl, rollup_lists, lcc, rollup_groupclauses)
			{
				List *rlist = (List *) lfirst(lcl);
				List *rclause = (List *) lfirst(lcc);
				List *last_list = (List *) llast(rlist);
				Bitmapset *this_groupcols = NULL;

				this_groupcols = NULL;
				foreach (lc, last_list)
				{
					SortGroupClause *sc = list_nth(rclause, lfirst_int(lc));

					this_groupcols = bms_add_member(this_groupcols, sc->tleSortGroupRef);
				}

				if (first)
					common_groupcols = this_groupcols;
				else
				{
					common_groupcols = bms_int_members(common_groupcols, this_groupcols);
					bms_free(this_groupcols);
				}
				first = false;
			}
		}
		else
		{
			foreach(lc, groupClause)
			{
				SortGroupClause *sc = lfirst(lc);

				common_groupcols = bms_add_member(common_groupcols, sc->tleSortGroupRef);
			}
		}

		x = -1;
		group_tles = NIL;
		while ((x = bms_next_member(common_groupcols, x)) >= 0)
		{
			TargetEntry *tle = get_sortgroupref_tle(x, tlist);

			group_tles = lappend(group_tles, tle);
		}

		if (!group_tles)
			need_redistribute = true;
		else
			need_redistribute = !cdbpathlocus_is_hashed_on_tlist(path->locus, group_tles, true);

		hash_exprs = NIL;
		hash_opfamilies = NIL;
		hash_sortrefs = NIL;
		foreach(lc, group_tles)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(lc);
			Oid			typeoid = exprType((Node *) tle->expr);
			Oid			opfamily;
			Oid			eqopoid;

			opfamily = cdb_default_distribution_opfamily_for_type(typeoid);
			if (!OidIsValid(opfamily))
				continue;

			/*
			 * If the datatype isn't mergejoinable, then we cannot represent
			 * the grouping in the locus. Skip such expressions.
			 */
			eqopoid = cdb_eqop_in_hash_opfamily(opfamily, typeoid);
			if (!op_mergejoinable(eqopoid, typeoid))
				continue;

			hash_exprs = lappend(hash_exprs, tle->expr);
			hash_opfamilies = lappend_oid(hash_opfamilies, opfamily);
			hash_sortrefs = lappend_int(hash_sortrefs, tle->ressortgroupref);
		}

		if (need_redistribute)
		{
			if (hash_exprs)
				locus = cdbpathlocus_from_exprs(root, hash_exprs, hash_opfamilies, hash_sortrefs, getgpsegmentCount());
			else
				CdbPathLocus_MakeSingleQE(&locus, getgpsegmentCount());
		}
		else
			CdbPathLocus_MakeNull(&locus);
	}

	*need_redistribute_p = need_redistribute;
	return locus;
}
