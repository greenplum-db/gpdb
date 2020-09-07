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
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/tlist.h"
#include "optimizer/var.h"
#include "parser/parse_clause.h"
#include "parser/parse_oper.h"
#include "utils/lsyscache.h"
#include "utils/selfuncs.h"

typedef enum
{
	INVALID_DQA = -1,
	SINGLE_DQA, /* only one unique DQA expr */
	MULTI_DQAS, /* multiple DQA exprs */
	SINGLE_DQA_WITHAGG, /* only one unique DQA expr with agg */
	MULTI_DQAS_WITHAGG/* mixed DQA and normal aggregate */
} DQAType;

/*
 * For convenience, we collect various inputs and intermediate planning results
 * in this struct, instead of passing a dozen arguments to all subroutines.
 */
typedef struct
{
	/* Inputs from the caller */
	DQAType     type;
	PathTarget *target;
	PathTarget *partial_grouping_target;
	double		dNumGroupsTotal;		/* total number of groups in the result, across all QEs */
	const AggClauseCosts *agg_costs;
	const AggClauseCosts *agg_partial_costs;
	const AggClauseCosts *agg_final_costs;
	List *rollup_lists;
	List *rollup_groupclauses;
} cdb_agg_planning_context;

typedef struct
{
	DQAType     type;

	PathTarget  *final_target;          /* finalize agg tlist */
	PathTarget  *partial_target;        /* partial agg tlist */
	PathTarget  *tup_split_target;      /* AggExprId + subpath_proj_target */
	PathTarget  *input_proj_target;     /* input tuple tlist + DQA expr */

	List        *dqa_group_clause;      /* DQA exprs + group by clause for remove duplication */

	List        *dqa_expr_lst;          /* DQAExpr list */
	double		 dNumDistinctGroups;	/* # of distinct combinations of GROUP BY and DISTINCT exprs */

} cdb_multi_dqas_info;


static CdbPathLocus choose_grouping_locus(PlannerInfo *root, Path *path,
										  PathTarget *target,
										  List *groupClause,
										  List *rollup_lists,
										  List *rollup_groupclauses,
										  bool *need_redistribute_p);

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
										 RelOptInfo *output_rel,
										 PathTarget *input_target,
										 List       *dqa_group_clause,
										 double dNumDistinctGroups);

static void add_single_mixed_dqa_hash_agg_path(PlannerInfo *root,
                                               Path *path,
                                               cdb_agg_planning_context *ctx,
                                               RelOptInfo *output_rel,
                                               PathTarget *input_target,
                                               List       *dqa_group_clause);
static void
add_multi_dqas_hash_agg_path(PlannerInfo *root,
							 Path *path,
							 cdb_agg_planning_context *ctx,
							 RelOptInfo *output_rel,
							 cdb_multi_dqas_info *info);

static void
fetch_single_dqa_info(PlannerInfo *root,
					  Path *path,
					  cdb_agg_planning_context *ctx,
					  cdb_multi_dqas_info *info);

static void
fetch_multi_dqas_info(PlannerInfo *root,
					  Path *path,
					  cdb_agg_planning_context *ctx,
					  cdb_multi_dqas_info *info);

static DQAType
recognize_dqa_type(cdb_agg_planning_context *ctx);

static PathTarget *
strip_aggdistinct(PathTarget *target);

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
								   double dNumGroupsTotal,
								   const AggClauseCosts *agg_costs,
								   const AggClauseCosts *agg_partial_costs,
								   const AggClauseCosts *agg_final_costs,
								   List *rollup_lists,
								   List *rollup_groupclauses)
{
	Query	   *parse = root->parse;
	Path	   *cheapest_path = input_rel->cheapest_total_path;
	bool		has_ordered_aggs = agg_costs->numPureOrderedAggs > 0;
	cdb_agg_planning_context ctx;

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

	memset(&ctx, 0, sizeof(ctx));
	ctx.target = target;
	ctx.partial_grouping_target = partial_grouping_target;
	ctx.dNumGroupsTotal = dNumGroupsTotal;
	ctx.agg_costs = agg_costs;
	ctx.agg_partial_costs = agg_partial_costs;
	ctx.agg_final_costs = agg_final_costs;
	ctx.rollup_lists = rollup_lists;
	ctx.rollup_groupclauses = rollup_groupclauses;

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
											&ctx,
											output_rel);
			}
		}
	}

	if (can_hash && list_length(agg_costs->distinctAggrefs) == 0)
	{
		add_twostage_hash_agg_path(root,
								   cheapest_path,
								   &ctx,
								   output_rel);
	}

	if ((can_hash || parse->groupClause == NIL) && list_length(agg_costs->distinctAggrefs) > 0)
	{
		/*
		 * Try possible plans for DISTINCT-qualified aggregate.
		 */
		cdb_multi_dqas_info info = {};
		DQAType type = recognize_dqa_type(&ctx);
		switch (type)
		{
		case SINGLE_DQA:
			{
				fetch_single_dqa_info(root, cheapest_path, &ctx, &info);

				add_single_dqa_hash_agg_path(root,
											 cheapest_path,
											 &ctx,
											 output_rel,
											 info.input_proj_target,
											 info.dqa_group_clause,
											 info.dNumDistinctGroups);
			}
			break;
		case SINGLE_DQA_WITHAGG:
			{
				fetch_single_dqa_info(root, cheapest_path, &ctx, &info);

				add_single_mixed_dqa_hash_agg_path(root,
				                                   cheapest_path,
				                                   &ctx,
				                                   output_rel,
				                                   info.input_proj_target,
				                                   info.dqa_group_clause);
			}
			break;
		case MULTI_DQAS:
			{
				fetch_multi_dqas_info(root, cheapest_path, &ctx, &info);

				add_multi_dqas_hash_agg_path(root,
											 cheapest_path,
											 &ctx,
											 output_rel,
											 &info);
			}
			break;
		case MULTI_DQAS_WITHAGG:
			break;
		default:
			break;
		}
	}
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
	DQAType     dqa_type;
	CdbPathLocus group_locus;
	bool		need_redistribute;
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

	group_locus = choose_grouping_locus(root, path, ctx->target,
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
		cdb_multi_dqas_info info = {};
		CdbPathLocus distinct_locus;
		bool		distinct_need_redistribute;

		dqa_type = recognize_dqa_type(ctx);

		if (dqa_type != SINGLE_DQA)
			return;

		fetch_single_dqa_info(root, path, ctx, &info);

		/*
		 * If subpath is projection capable, we do not want to generate a
		 * projection plan. The reason is that the projection plan does not
		 * constrain a child tlist when it creates subplan. Thus, GROUP BY expr
		 * may not be found in the scan targetlist.
		 */
		path = apply_projection_to_path(root, path->parent, path, info.input_proj_target);

		distinct_locus = choose_grouping_locus(root, path,
											   info.input_proj_target,
											   info.dqa_group_clause, NIL, NIL,
											   &distinct_need_redistribute);

		/* If the input distribution matches the distinct, we can proceed */
		if (distinct_need_redistribute)
			return;
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
		 *
		 * NOTE: We don't pass the HAVING quals here. HAVING quals can
		 * only be evaluated in the Finalize stage, after computing the
		 * final aggregate values.
		 */
		initial_agg_path =
			(Path *) create_groupingsets_path(root,
											  output_rel,
											  path,
											  grouping_sets_partial_target,
											  AGGSPLIT_INITIAL_SERIAL,
											  NIL,
											  ctx->rollup_lists,
											  ctx->rollup_groupclauses,
											  ctx->agg_partial_costs,
											  estimate_num_groups_on_segment(ctx->dNumGroupsTotal,
																			 path->rows, path->locus));

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
									 estimate_num_groups_on_segment(ctx->dNumGroupsTotal,
																	path->rows, path->locus),
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
										ctx->dNumGroupsTotal,
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
										ctx->dNumGroupsTotal,
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
	double		dNumGroups;

	group_locus = choose_grouping_locus(root, path, ctx->target,
										parse->groupClause, NIL, NIL,
										&need_redistribute);
	/*
	 * If the distribution of this path is suitable, two-stage aggregation
	 * is not applicable.
	 */
	if (!need_redistribute)
		return;

	/*
	 * Calculate the number of groups in the second stage, per segment.
	 */
	if (CdbPathLocus_IsPartitioned(group_locus))
		dNumGroups = clamp_row_est(ctx->dNumGroupsTotal /
								   CdbPathLocus_NumSegments(group_locus));
	else
		dNumGroups = ctx->dNumGroupsTotal;

	if (!calcHashAggTableSizes(work_mem * 1024L,
							   dNumGroups,
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
												estimate_num_groups_on_segment(ctx->dNumGroupsTotal,
																			   path->rows, path->locus),
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
									dNumGroups,
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

/*
 * Create Paths for an Aggregate with one DISTINCT-qualified aggregate and
 * multi normal aggregate.
 */
static void add_single_mixed_dqa_hash_agg_path(PlannerInfo *root,
                                               Path *path,
                                               cdb_agg_planning_context *ctx,
                                               RelOptInfo *output_rel,
                                               PathTarget *input_target,
                                               List       *dqa_group_clause)
{
	Query	   *parse = root->parse;
	CdbPathLocus distinct_locus;
	bool		distinct_need_redistribute;
	HashAggTableSizes hash_info;
	CdbPathLocus singleQE_locus;

	if (!gp_enable_agg_distinct)
		return;

	if (parse->groupClause)
		return;

	/*
	 * If subpath is projection capable, we do not want to generate a
	 * projection plan. The reason is that the projection plan does not
	 * constrain a child tlist when it creates subplan. Thus, GROUP BY expr
	 * may not be found in the scan targetlist.
	 */
	path = apply_projection_to_path(root, path->parent, path, input_target);

	distinct_locus = choose_grouping_locus(root, path,
										   input_target,
										   dqa_group_clause, NIL, NIL,
										   &distinct_need_redistribute);

	if (!CdbPathLocus_IsPartitioned(distinct_locus))
		return;

	if (distinct_need_redistribute)
		path = cdbpath_create_motion_path(root, path, NIL, false,
										  distinct_locus);

	path = (Path *) create_agg_path(root,
									output_rel,
									path,
									ctx->partial_grouping_target,
									AGG_PLAIN,
									AGGSPLIT_INITIAL_SERIAL,
									false, /* streaming */
									parse->groupClause,
									NIL,
									ctx->agg_partial_costs, /* FIXME */
									estimate_num_groups_on_segment(ctx->dNumGroupsTotal,
																   path->rows, path->locus),
									&hash_info);

	CdbPathLocus_MakeSingleQE(&singleQE_locus, getgpsegmentCount());
	path = cdbpath_create_motion_path(root, path, NIL, false, singleQE_locus);

	path = (Path *) create_agg_path(root,
									output_rel,
									path,
									ctx->target,
									AGG_PLAIN,
									AGGSPLIT_FINAL_DESERIAL,
									false, /* streaming */
									parse->groupClause,
									(List *) parse->havingQual,
									ctx->agg_final_costs,
									ctx->dNumGroupsTotal,
									&hash_info);
	add_path(output_rel, path);
}

/*
 * Create Paths for an Aggregate with one DISTINCT-qualified aggregate.
 */
static void
add_single_dqa_hash_agg_path(PlannerInfo *root,
							 Path *path,
							 cdb_agg_planning_context *ctx,
							 RelOptInfo *output_rel,
							 PathTarget *input_target,
							 List       *dqa_group_clause,
							 double dNumDistinctGroups)
{
	Query	   *parse = root->parse;
	int			num_input_segments;
	CdbPathLocus group_locus;
	double		dNumGroups;
	bool		group_need_redistribute;
	CdbPathLocus distinct_locus;
	bool		distinct_need_redistribute;
	HashAggTableSizes hash_info;

	if (!gp_enable_agg_distinct)
		return;

	/*
	 * If subpath is projection capable, we do not want to generate a
	 * projection plan. The reason is that the projection plan does not
	 * constrain a child tlist when it creates subplan. Thus, GROUP BY expr
	 * may not be found in the scan targetlist.
	 */
	path = apply_projection_to_path(root, path->parent, path, input_target);

	if (CdbPathLocus_IsPartitioned(path->locus))
		num_input_segments = CdbPathLocus_NumSegments(path->locus);
	else
		num_input_segments = 1;

	distinct_locus = choose_grouping_locus(root, path,
										   input_target,
										   dqa_group_clause, NIL, NIL,
										   &distinct_need_redistribute);

	/*
	 * Calculate the number of groups in the final stage, per segment.
	 * group_locus is the corresponding locus for the final stage.
	 */
	group_locus = choose_grouping_locus(root, path,
										input_target,
										parse->groupClause, NIL, NIL,
										&group_need_redistribute);
	if (CdbPathLocus_IsPartitioned(group_locus))
		dNumGroups = clamp_row_est(ctx->dNumGroupsTotal /
								   CdbPathLocus_NumSegments(path->locus));
	else
		dNumGroups = ctx->dNumGroupsTotal;

	/*
	 * GPDB_96_MERGE_FIXME: compute the hash table size once. But we create
	 * several different Hash Aggs below, depending on the query. Is this
	 * computation sensible for all of them?
	 */
	if (!calcHashAggTableSizes(work_mem * 1024L,
							   dNumGroups,
							   path->pathtarget->width,
							   false,	/* force */
							   &hash_info))
		return;	/* don't try to hash */

	if (!distinct_need_redistribute || !group_need_redistribute)
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
										dNumDistinctGroups / (double) num_input_segments,
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
										dNumGroups,
										&hash_info);
		add_path(output_rel, path);
	}
	else if (CdbPathLocus_IsHashed(group_locus))
	{
		double		input_rows = path->rows;

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
											estimate_num_groups_on_segment(dNumDistinctGroups,
																		   input_rows, path->locus),
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
										dNumDistinctGroups / CdbPathLocus_NumSegments(group_locus),
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
										dNumGroups,
										&hash_info);
		add_path(output_rel, path);
	}
	else if (CdbPathLocus_IsHashed(distinct_locus))
	{
		double			input_rows = path->rows;

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
										estimate_num_groups_on_segment(dNumDistinctGroups,
																	   input_rows, path->locus),
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
										dNumDistinctGroups / CdbPathLocus_NumSegments(distinct_locus),
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
										estimate_num_groups_on_segment(ctx->dNumGroupsTotal, input_rows, path->locus),
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
										dNumGroups,
										&hash_info);

		add_path(output_rel, path);
	}
	else
		return;
}

/*
 * Create Paths for Multiple DISTINCT-qualified aggregates.
 *
 * The goal is that using a single execution path to handle all DQAs, so
 * before removing duplication a SplitTuple node is created. This node handles
 * each input tuple to n output tuples(n is DQA expr number). Each output tuple
 * only contains an AggExprId, one DQA expr and all GROUP by expr. For example,
 * SELECT DQA(a), DQA(b) FROM foo GROUP BY c;
 * After the tuple split, two tuples are generated:
 * -------------------
 * | 1 | a | n/a | c |
 * -------------------
 * -------------------
 * | 2 | n/a | b | c |
 * -------------------
 *
 * In an aggregate executor, if the input tuple contains AggExprId, that means
 * the tuple is split. Each value of AggExprId points to a bitmap set to
 * represent args AttrNumber. In the Agg executor, each transfunc also keeps
 * its own args bitmap set. The transfunc is invoked only if bitmapset matches
 * with each other.
 */
static void
add_multi_dqas_hash_agg_path(PlannerInfo *root,
							 Path *path,
							 cdb_agg_planning_context *ctx,
							 RelOptInfo *output_rel,
							 cdb_multi_dqas_info *info)
{
	CdbPathLocus distinct_locus;
	bool		distinct_need_redistribute;
	HashAggTableSizes hash_info;

	/*
	 * Check if the final Hash Agg would be too large.
	 */
	if (!calcHashAggTableSizes(work_mem * 1024L,
							   ctx->dNumGroupsTotal,
							   path->pathtarget->width,
							   false,	/* force */
							   &hash_info))
		return;

	/*
	 * If subpath is projection capable, we do not want to generate a
	 * projection plan. The reason is that the projection plan does not
	 * constrain a child tlist when it creates subplan. Thus, GROUP BY expr
	 * may not be found in the scan targetlist.
	 */
	path = apply_projection_to_path(root, path->parent, path, info->input_proj_target);

	/*
	 * Finalize Aggregate
	 *   -> Gather Motion
	 *        -> Partial Aggregate
	 *             -> HashAggregate, to remote duplicates
	 *                  -> Redistribute Motion
	 *                       -> TupleSplit (according to DISTINCT expr)
	 *                            -> input
	 */
	path = (Path *) create_tup_split_path(root,
										  output_rel,
										  path,
										  info->tup_split_target,
										  root->parse->groupClause,
										  info->dqa_expr_lst);

	AggClauseCosts DedupCost = {};
	get_agg_clause_costs(root, (Node *) info->tup_split_target->exprs,
						 AGGSPLIT_SIMPLE,
						 &DedupCost);

	if (gp_enable_dqa_pruning)
	{
		/*
		 * If we are grouping, we charge an additional cpu_operator_cost per
		 * **grouping column** per input tuple for grouping comparisons.
		 *
		 * But in the tuple split case, other columns not for this DQA are
		 * NULLs, the actual cost is way less than the number calculating based
		 * on the length of grouping clause.
		 *
		 * So here we create a dummy grouping clause whose length is 1 (the
		 * most common case of DQA), use it to calculate the cost, then set the
		 * actual one back into the path.
		 */
		List *dummy_group_clause = list_make1(list_head(info->dqa_group_clause));

		path = (Path *) create_agg_path(root,
										output_rel,
										path,
										info->tup_split_target,
										AGG_HASHED,
										AGGSPLIT_SIMPLE,
										true, /* streaming */
										dummy_group_clause, /* only its length 1 is being used here */
										NIL,
										&DedupCost,
										estimate_num_groups_on_segment(info->dNumDistinctGroups,
																	   path->rows, path->locus),
										&hash_info);

		/* set the actual group clause back */
		((AggPath *)path)->groupClause = info->dqa_group_clause;
	}

	distinct_locus = choose_grouping_locus(root, path,
										   info->tup_split_target,
										   info->dqa_group_clause, NIL, NIL,
										   &distinct_need_redistribute);

	if (distinct_need_redistribute)
		path = cdbpath_create_motion_path(root, path, NIL, false,
										  distinct_locus);

	AggStrategy split = AGG_PLAIN;
	unsigned long DEDUPLICATED_FLAG = 0;
	PathTarget *partial_target = info->partial_target;
	double		input_rows = path->rows;

	if (root->parse->groupClause)
	{
		path = (Path *) create_agg_path(root,
										output_rel,
										path,
										info->tup_split_target,
										AGG_HASHED,
										AGGSPLIT_SIMPLE,
										false, /* streaming */
										info->dqa_group_clause,
										NIL,
										&DedupCost,
										info->dNumDistinctGroups / CdbPathLocus_NumSegments(distinct_locus),
										&hash_info);

		split = AGG_HASHED;
		DEDUPLICATED_FLAG = AGGSPLITOP_DEDUPLICATED;
		partial_target = strip_aggdistinct(info->partial_target);
	}

	path = (Path *) create_agg_path(root,
									output_rel,
									path,
									partial_target,
									split,
									AGGSPLIT_INITIAL_SERIAL | DEDUPLICATED_FLAG,
									false, /* streaming */
									root->parse->groupClause,
									NIL,
									ctx->agg_partial_costs,
									estimate_num_groups_on_segment(ctx->dNumGroupsTotal,
																   input_rows, path->locus),
									&hash_info);

	CdbPathLocus singleQE_locus;
	CdbPathLocus_MakeSingleQE(&singleQE_locus, getgpsegmentCount());
	path = cdbpath_create_motion_path(root,
									  path,
									  NIL,
									  false,
									  singleQE_locus);

	path = (Path *) create_agg_path(root,
									output_rel,
									path,
									info->final_target,
									split,
									AGGSPLIT_FINAL_DESERIAL | DEDUPLICATED_FLAG,
									false, /* streaming */
									root->parse->groupClause,
									(List *) root->parse->havingQual,
									ctx->agg_final_costs,
									ctx->dNumGroupsTotal,
									&hash_info);

	add_path(output_rel, path);
}

/*
 * Figure out the desired data distribution to perform the grouping.
 *
 * In case of a simple GROUP BY, we prefer to distribute the data according to
 * the GROUP BY. With multiple grouping sets, identify the set of common
 * entries, and distribute based on that. For example, if you do
 * GROUP BY GROUPING SETS ((a, b, c), (b, c)), the common cols are b and c.
 */
static CdbPathLocus
choose_grouping_locus(PlannerInfo *root, Path *path,
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

static DQAType
recognize_dqa_type(cdb_agg_planning_context *ctx)
{
	ListCell    *lc, *lcc;
	List        *dqaArgs = NIL;
	ctx->type = INVALID_DQA;

	foreach (lc, ctx->agg_costs->distinctAggrefs)
	{
		Aggref *aggref = (Aggref *) lfirst(lc);
		SortGroupClause *arg_sortcl;

		/* I can not give a case for a DQA have order by yet. */
		if (aggref->aggorder != NIL)
			return ctx->type;

		foreach (lcc, aggref->aggdistinct)
		{
			arg_sortcl = (SortGroupClause *) lfirst(lcc);
			if (!arg_sortcl->hashable)
			{
				/*
				 * XXX: I'm not sure if the hashable flag is always set correctly
				 * for DISTINCT args. DISTINCT aggs are never implemented with hashing
				 * in PostgreSQL.
				 */
				return ctx->type;
			}
		}

		/* get the first dqa arguments */
		if (dqaArgs == NIL)
		{
			dqaArgs = aggref->args;
			ctx->type = SINGLE_DQA;
		}
		/* if there is another dqa with different args, it's MULTI_DQAS */
		else if (!equal(dqaArgs, aggref->args))
		{
			ctx->type = MULTI_DQAS;
			break;
		}
	}

	if (ctx->type != INVALID_DQA)
	{
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
					/* mixing DISTINCT and non-DISTINCT aggs */
					if (ctx->type == SINGLE_DQA)
						ctx->type = SINGLE_DQA_WITHAGG;
					else
						ctx->type = MULTI_DQAS_WITHAGG;

					return ctx->type;
				}
			}
		}
	}

	return ctx->type;
}

/*
 * fetch_multi_dqas_info
 *
 * 1. fetch all dqas path required information as single dqa's function.
 *
 * 2. append an AggExprId into Pathtarget to indicate which DQA expr is
 * in the output tuple after TupleSplit.
 */
static void
fetch_multi_dqas_info(PlannerInfo *root,
					  Path *path,
					  cdb_agg_planning_context *ctx,
					  cdb_multi_dqas_info *info)
{
	ListCell    *lc;
	ListCell    *lcc;
	Index		maxRef = 0;
	PathTarget *proj_target;
	int			num_input_segments;
	double		num_total_input_rows;
	List	   *group_exprs;
	double		dNumDistinctGroups;

	if (CdbPathLocus_IsPartitioned(path->locus))
		num_input_segments = CdbPathLocus_NumSegments(path->locus);
	else
		num_input_segments = 1;
	num_total_input_rows = path->rows * num_input_segments;

	group_exprs = get_sortgrouplist_exprs(root->parse->groupClause,
										  make_tlist_from_pathtarget(path->pathtarget));

	proj_target = copy_pathtarget(path->pathtarget);
	if (proj_target->sortgrouprefs)
	{
		for (int idx = 0; idx < list_length(proj_target->exprs); idx++)
		{
			if (proj_target->sortgrouprefs[idx] > maxRef)
				maxRef = proj_target->sortgrouprefs[idx];
		}
	}
	else
		proj_target->sortgrouprefs = (Index *) palloc0(list_length(proj_target->exprs) * sizeof(Index));

	info->dqa_expr_lst = NIL;

	/*
	 * assign numDisDQAs and agg_args_id_bms
	 *
	 * find all DQAs with different args, count the number, store their args bitmapsets
	 */
	dNumDistinctGroups = 0;
	forboth(lc, ctx->agg_partial_costs->distinctAggrefs,
	        lcc, ctx->agg_final_costs->distinctAggrefs)
	{
		Aggref	        *aggref = (Aggref *) lfirst(lc);
		Aggref	        *aggref_final = (Aggref *) lfirst(lcc);
		SortGroupClause *arg_sortcl;
		TargetEntry     *arg_tle;
		ListCell        *lc2;
		Bitmapset       *bms = NULL;
		List		   *this_dqa_group_exprs;

		this_dqa_group_exprs = list_copy(group_exprs);

		foreach (lc2, aggref->aggdistinct)
		{
			arg_sortcl = (SortGroupClause *) lfirst(lc2);
			arg_tle = get_sortgroupclause_tle(arg_sortcl, aggref->args);
			ListCell    *lc3;
			int         dqa_idx = 0;

			foreach (lc3, proj_target->exprs)
			{
				Expr    *expr = lfirst(lc3);

				if (equal(arg_tle->expr, expr))
					break;

				dqa_idx++;
			}

			/*
			 * DQA expr is not in PathTarget
			 *
			 * SELECT DQA(a + b) from foo;
			 */
			if (dqa_idx == list_length(proj_target->exprs))
			{
				add_column_to_pathtarget(proj_target, arg_tle->expr, ++maxRef);

				SortGroupClause *sortcl;

				sortcl = copyObject(arg_sortcl);
				sortcl->tleSortGroupRef = maxRef;
				sortcl->hashable = true;	/* we verified earlier that it's hashable */

				info->dqa_group_clause = lappend(info->dqa_group_clause, sortcl);
				this_dqa_group_exprs = lappend(this_dqa_group_exprs, arg_tle->expr);

				bms = bms_add_member(bms, maxRef);
			}
			else if (proj_target->sortgrouprefs[dqa_idx] == 0)
			{
				/*
				 * DQA expr in PathTarget but no reference
				 *
				 * SELECT DQA(a) FROM foo ;
				 */
				proj_target->sortgrouprefs[dqa_idx] = ++maxRef;

				SortGroupClause *sortcl;

				sortcl = copyObject(arg_sortcl);
				sortcl->tleSortGroupRef = maxRef;
				sortcl->hashable = true;	/* we verified earlier that it's hashable */

				info->dqa_group_clause = lappend(info->dqa_group_clause, sortcl);
				this_dqa_group_exprs = lappend(this_dqa_group_exprs, arg_tle->expr);

				bms = bms_add_member(bms, maxRef);
			}
			else
			{
				/*
				 * DQA expr in PathTarget and referenced by GROUP BY clause
				 *
				 * SELECT DQA(a) FROM foo GROUP BY a;
				 */
				Index exprRef = proj_target->sortgrouprefs[dqa_idx];
				bms = bms_add_member(bms, exprRef);
			}
		}

		/*
		 * DQA(a, b) and DQA(b, a) and their filter is same, as well as, they
		 * do not contain volatile expression, then they can share one split
		 * tuple.
		 */
		Index agg_expr_id ;
		if (!contain_volatile_functions((Node *)aggref->aggfilter))
		{
			ListCell *lc_dqa;
			agg_expr_id = 1;
			foreach (lc_dqa, info->dqa_expr_lst)
			{
				DQAExpr *dqaExpr = (DQAExpr *)lfirst(lc_dqa);

				if (bms_equal(bms, dqaExpr->agg_args_id_bms)
					&& equal(aggref->aggfilter, dqaExpr->agg_filter))
					break;

				agg_expr_id++;
			}
		}
		else
		{
			agg_expr_id = list_length(info->dqa_expr_lst) + 1;
		}

		/* If DQA(expr1) FILTER (WHERE expr2) is different with previous, create new one */
		if ((agg_expr_id - 1) == list_length(info->dqa_expr_lst))
		{
			DQAExpr *dqaExpr= makeNode(DQAExpr);

			dqaExpr->agg_expr_id = agg_expr_id;
			dqaExpr->agg_args_id_bms = bms;
			dqaExpr->agg_filter = (Expr *)copyObject(aggref->aggfilter);
			info->dqa_expr_lst = lappend(info->dqa_expr_lst, dqaExpr);

			/*
			 * How many distinct combinations of GROUP BY columns and the
			 * DISTINCT arguments of this aggregate are there? Add it to the
			 * total.
			 */
			dNumDistinctGroups += estimate_num_groups(root,
			                                          this_dqa_group_exprs,
			                                          num_total_input_rows,
			                                          NULL);
		}

		/* assign an agg_expr_id value to aggref*/
		aggref->agg_expr_id = agg_expr_id;

		/* rid of filter in aggref */
		aggref->aggfilter = NULL;
		aggref_final->aggfilter = NULL;
	}
	info->dNumDistinctGroups = dNumDistinctGroups;

	info->input_proj_target = proj_target;
	info->tup_split_target = copy_pathtarget(proj_target);
	{
		AggExprId *a_expr_id = makeNode(AggExprId);
		add_column_to_pathtarget(info->tup_split_target, (Expr *)a_expr_id, ++maxRef);

		Oid eqop;
		bool hashable;
		SortGroupClause *sortcl;
		get_sort_group_operators(INT4OID, false, true, false, NULL, &eqop, NULL, &hashable);

		sortcl = makeNode(SortGroupClause);
		sortcl->tleSortGroupRef = maxRef;
		sortcl->hashable = hashable;
		sortcl->eqop = eqop;
		info->dqa_group_clause = lcons(sortcl, info->dqa_group_clause);
	}

	info->dqa_group_clause = list_concat(info->dqa_group_clause,
										 list_copy(root->parse->groupClause));

	info->partial_target= ctx->partial_grouping_target;
	info->final_target = ctx->target;
}

/*
 * fetch_single_dqa_info
 *
 * fetch single dqa path required information and store in cdb_multi_dqas_info
 *
 * info->input_target contains subpath target expr + all DISTINCT expr
 *
 * info->dqa_group_clause contains DISTINCT expr + GROUP BY expr
 */
static void
fetch_single_dqa_info(PlannerInfo *root,
					  Path *path,
					  cdb_agg_planning_context *ctx,
					  cdb_multi_dqas_info *info)
{
	Index		maxRef;
	List	   *dqa_group_exprs;

	/* Prepare a modifiable copy of the input path target */
	info->input_proj_target = copy_pathtarget(path->pathtarget);
	maxRef = 0;
	List *exprLst = info->input_proj_target->exprs;
	if (info->input_proj_target->sortgrouprefs)
	{
		for (int idx = 0; idx < list_length(exprLst); idx++)
		{
			if (info->input_proj_target->sortgrouprefs[idx] > maxRef)
				maxRef = info->input_proj_target->sortgrouprefs[idx];
		}
	}
	else
		info->input_proj_target->sortgrouprefs = (Index *) palloc0(list_length(exprLst) * sizeof(Index));

	dqa_group_exprs = get_sortgrouplist_exprs(root->parse->groupClause,
											  make_tlist_from_pathtarget(path->pathtarget));

	Aggref	   *aggref = list_nth(ctx->agg_costs->distinctAggrefs, 0);
	SortGroupClause *arg_sortcl;
	SortGroupClause *sortcl = NULL;
	TargetEntry *arg_tle;
	int			idx = 0;
	ListCell   *lc;
	ListCell   *lcc;

	foreach (lc, aggref->aggdistinct)
	{
		arg_sortcl = (SortGroupClause *) lfirst(lc);
		arg_tle = get_sortgroupref_tle(arg_sortcl->tleSortGroupRef, aggref->args);

		/* Now find this expression in the sub-path's target list */
		idx = 0;
		foreach(lcc, info->input_proj_target->exprs)
		{
			Expr		*expr = lfirst(lcc);

			if (equal(expr, arg_tle->expr))
				break;
			idx++;
		}

		if (idx == list_length(info->input_proj_target->exprs))
			add_column_to_pathtarget(info->input_proj_target, arg_tle->expr, ++maxRef);
		else if (info->input_proj_target->sortgrouprefs[idx] == 0)
			info->input_proj_target->sortgrouprefs[idx] = ++maxRef;

		sortcl = copyObject(arg_sortcl);
		sortcl->tleSortGroupRef = info->input_proj_target->sortgrouprefs[idx];
		sortcl->hashable = true;	/* we verified earlier that it's hashable */

		info->dqa_group_clause = lappend(info->dqa_group_clause, sortcl);
		dqa_group_exprs = lappend(dqa_group_exprs, arg_tle->expr);
	}

	info->dqa_group_clause = list_concat(list_copy(root->parse->groupClause),
										 info->dqa_group_clause);

	/*
	 * Estimate how many groups there are in DISTINCT + GROUP BY, per segment.
	 * For example in query:
	 *
	 * select count(distinct c) from t group by b;
	 *
	 * dNumDistinctGroups is the estimate of distinct combinations of b and c.
	 */
	double		num_total_input_rows;
	if (CdbPathLocus_IsPartitioned(path->locus))
		num_total_input_rows = path->rows * CdbPathLocus_NumSegments(path->locus);
	else
		num_total_input_rows = path->rows;
	info->dNumDistinctGroups = estimate_num_groups(root,
												   dqa_group_exprs,
												   num_total_input_rows,
												   NULL);
}

/*
 * Prepare the input path for sorted Agg node.
 *
 * The input to a (sorted) Agg node must be:
 *
 * 1. distributed so that rows belonging to the same group reside on the
 *    same segment, and
 *
 * 2. sorted according to the pathkeys.
 *
 * If the input is already suitably distributed, this is no different from
 * upstream, and we just add a Sort node if the input isn't already sorted.
 *
 * This also works for the degenerate case with no pathkeys, which means
 * simple aggregation without grouping. For that, all the rows must be
 * brought to a single node, but no sorting is needed.
 */
Path *
cdb_prepare_path_for_sorted_agg(PlannerInfo *root,
								bool is_sorted,
								/* args corresponding to create_sort_path */
								RelOptInfo *rel,
								Path *subpath,
								PathTarget *target,
								List *group_pathkeys,
								double limit_tuples,
								/* extra arguments */
								List *groupClause,
								List *rollup_lists,
								List *rollup_groupclauses)
{
	CdbPathLocus locus;
	bool		need_redistribute;

	locus = choose_grouping_locus(root,
								  subpath,
								  target,
								  groupClause,
								  rollup_lists,
								  rollup_groupclauses,
								  &need_redistribute);

	if (!need_redistribute)
	{
		/*
		 * The input is already suitably distributed. Just add a Sort node,
		 * if needed.
		 */
		if (!is_sorted)
		{
			subpath = (Path *) create_sort_path(root,
												rel,
												subpath,
												group_pathkeys,
												-1.0);
		}
		return subpath;
	}

	if (is_sorted && group_pathkeys)
	{
		/*
		 * The input is already conveniently sorted. We could redistribute
		 * it by the grouping keys, but then we'd need to re-sort it. That
		 * doesn't seem like a good idea, so we prefer to gather it all, and
		 * take advantage of the sort order.
		 */
		CdbPathLocus_MakeSingleQE(&locus, getgpsegmentCount());
		subpath = cdbpath_create_motion_path(root,
											 subpath,
											 group_pathkeys,
											 false, locus);
	}
	else if (!is_sorted && group_pathkeys)
	{
		/*
		 * If we need to redistribute, it's usually best to redistribute
		 * the data first, and then sort in parallel on each segment.
		 *
		 * But if we don't have any expressions to redistribute on, i.e.
		 * if we are gathering all data to a single node to perform the
		 * aggregation, then it's better to sort all the data on the
		 * segments first, in parallel, and do a order-preserving motion
		 * to merge the inputs.
		 */
		if (CdbPathLocus_IsPartitioned(locus))
			subpath = cdbpath_create_motion_path(root, subpath, NIL,
												 false, locus);

		subpath = (Path *) create_sort_path(root,
											rel,
											subpath,
											group_pathkeys,
											-1.0);

		if (!CdbPathLocus_IsPartitioned(locus))
			subpath = cdbpath_create_motion_path(root, subpath,
												 group_pathkeys,
												 false, locus);
	}
	else
	{
		/*
		 * The grouping doesn't require any sorting, i.e. the GROUP BY
		 * consists entirely of (pseudo-)constants.
		 *
		 * The locus could be Hashed, which is a bit silly because with
		 * all-constant grouping keys, all the rows will end up on a
		 * single QE anyway. We could mark the locus as SingleQE here, so
		 * that in simple cases where the result needs to end up in the QD,
		 * the planner could Gather the result there directly. However, in
		 * other cases hashing the result to one QE node is more helpful
		 * for the plan above this.
		 */
		Assert(!group_pathkeys);
		subpath = cdbpath_create_motion_path(root,
											 subpath,
											 NIL,
											 false, locus);
	}

	return subpath;
}

/*
 * Prepare the input path for hashed Agg node.
 *
 * This is much simpler than the sorted case. We only need to care about
 * distribution, not sorting.
 */
Path *
cdb_prepare_path_for_hashed_agg(PlannerInfo *root,
								Path *subpath,
								PathTarget *target,
								/* extra arguments */
								List *groupClause,
								List *rollup_lists,
								List *rollup_groupclauses)
{
	CdbPathLocus locus;
	bool		need_redistribute;

	locus = choose_grouping_locus(root,
								  subpath,
								  target,
								  groupClause,
								  rollup_lists,
								  rollup_groupclauses,
								  &need_redistribute);

	/*
	 * Redistribute if needed.
	 *
	 * The hash agg doesn't care about input order, and it destroys any
	 * order there was, so don't bother with a order-preserving Motion even
	 * if we could.
	 */
	if (need_redistribute)
		subpath = cdbpath_create_motion_path(root,
											 subpath,
											 NIL /* pathkeys */,
											 false,
											 locus);

	return subpath;
}
