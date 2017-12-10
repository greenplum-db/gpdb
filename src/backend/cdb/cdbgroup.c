/*-------------------------------------------------------------------------
 *
 * cdbgroup.c
 *	  Routines to aid in planning grouping queries for parallel
 *    execution.  This is, essentially, an extension of the file
 *    optimizer/prep/planner.c, although some functions are not
 *    externalized.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbgroup.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <limits.h>

#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "executor/nodeAgg.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/print.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/planpartition.h"
#include "optimizer/planshare.h"
#include "optimizer/prep.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/subselect.h"
#include "optimizer/tlist.h"
#include "optimizer/var.h"
#include "parser/parsetree.h"
#include "parser/parse_agg.h"
#include "parser/parse_clause.h"
#include "parser/parse_coerce.h"
#include "parser/parse_expr.h"
#include "parser/parse_oper.h"
#include "parser/parse_relation.h"
#include "utils/lsyscache.h"
#include "utils/selfuncs.h"
#include "utils/syscache.h"
#include "catalog/pg_aggregate.h"

#include "cdb/cdbllize.h"
#include "cdb/cdbpathtoplan.h"	/* cdbpathtoplan_create_flow() */
#include "cdb/cdbpath.h"
#include "cdb/cdbplan.h"
#include "cdb/cdbpullup.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbhash.h"		/* isGreenplumDbHashable() */

#include "cdb/cdbsetop.h"
#include "cdb/cdbgroup.h"

extern void UpdateScatterClause(Query *query, List *newtlist);

/*
 * MppGroupPrep represents a strategy by which to precondition the
 * argument to  a parallel aggregation plan.
 *
 * MPP_GRP_PREP_NONE
 *    Use the input plan as is.
 * MPP_GRP_PREP_HASH_GROUPS
 *    Redistribute the input to collocate groups.
 * MPP_GRP_PREP_HASH_DISTINCT
 *    Redistribute the input on the sole distinct expression used as
 *    an aggregate argument.
 * MPP_GRP_PREP_FOCUS_QE
 *    Focus a partitioned input on the utility QE.
 * MPP_GRP_PREP_FOCUS_QD
 *    Focus a partitioned input on the QD.
 * MPP_GRP_PREP_BROADCAST
 *    Broadcast to convert a partitioned input into a replicated one.
 */
typedef enum MppGroupPrep
{
	MPP_GRP_PREP_NONE = 0,
	MPP_GRP_PREP_HASH_GROUPS,
	MPP_GRP_PREP_HASH_DISTINCT,
	MPP_GRP_PREP_FOCUS_QE,
	MPP_GRP_PREP_FOCUS_QD,
	MPP_GRP_PREP_BROADCAST
} MppGroupPrep;

/*
 * MppGroupType represents a stategy by which to implement parallel
 * aggregation on a possibly preconditioned input plan.
 *
 * MPP_GRP_TYPE_NONE
 *    No parallel plan found.
 * MPP_GRP_TYPE_BASEPLAN
 *    Use the sequential plan as is.
 * MPP_GRP_TYPE_PLAIN_2STAGE,
 *    Ungrouped, 2-stage aggregation.
 * MPP_GRP_TYPE_GROUPED_2STAGE,
 *    Grouped, 2-stage aggregation.
 * MPP_GRP_TYPE_PLAIN_DQA_2STAGE,
 *    Ungrouped, 2-stage aggregation.
 * MPP_GRP_TYPE_GROUPED_DQA_2STAGE,
 *    Grouped, 2-stage aggregation.
 *
 * TODO: Add types for min-max optimization, when ready:
 *
 * MPP_GRP_TYPE_MINMAX
 *    Use the sequential min-max optimization plan as is.
 * MPP_GRP_TYPE_MINMAX_2STAGE
 *    Use a 2-stage variant of min-max aggregation.
 */
typedef enum MppGroupType
{
	MPP_GRP_TYPE_NONE = 0,
	MPP_GRP_TYPE_BASEPLAN,
	MPP_GRP_TYPE_PLAIN_2STAGE,
	MPP_GRP_TYPE_GROUPED_2STAGE,
	MPP_GRP_TYPE_PLAIN_DQA_2STAGE,
	MPP_GRP_TYPE_GROUPED_DQA_2STAGE,
} MppGroupType;


/* Summary values detailing the post-Motion part of a coplan for
 * three-phase aggreation.  The code is as follows:
 *	S = Sort
 *	G = GroupAgg
 *	H = HashAgg
 *	P = PlainAgg
 *
 * So GSH means (GroupAgg (Sort (HashAgg X))).
 */
typedef enum DqaCoplanType
{
	DQACOPLAN_GGS,
	DQACOPLAN_GSH,
	DQACOPLAN_SHH,
	DQACOPLAN_HH,
	DQACOPLAN_PGS,
	DQACOPLAN_PH
} DqaCoplanType;

typedef enum DqaJoinStrategy
{
	DqaJoinUndefined = 0,
	DqaJoinNone,				/* No join required for solitary DQA argument. */
	DqaJoinCross,				/* Scalar aggregation uses cross product. */
	DqaJoinHash,				/* Hash join (possibly with subsequent sort) */
	DqaJoinMerge,				/* Merge join */

	/*
	 * These last are abstract and will be replaced by DqaJoinHash aor
	 * DqaJoinMerge once planning is complete.
	 */
	DqaJoinSorted,				/* Sorted output required. */
	DqaJoinCheapest,			/* No sort requirement. */
} DqaJoinStrategy;

/* DQA coplan information */
typedef struct DqaInfo
{
	Node	   *distinctExpr;	/* By reference from agg_counts for
								 * convenience. */
	AttrNumber	base_index;		/* Index of attribute in base plan targetlist */
	bool		can_hash;
	double		num_rows;		/* Estimated cardinality of grouping key, dqa
								 * arg */
	Plan	   *coplan;			/* Coplan for this (later this and all prior)
								 * coplan */
	Query	   *parse;			/* Plausible root->parse for the coplan. */
	bool		distinctkey_collocate;	/* Whether the input plan collocates
										 * on this distinct key */

	/*
	 * These fields are for costing and planning.  Before constructing the
	 * coplan for this DQA argument, determine cheapest way to get the answer
	 * and cheapest way to get the answer in grouping key order.
	 */
	bool		use_hashed_preliminary;
	Cost		cost_sorted;
	DqaCoplanType coplan_type_sorted;
	Cost		cost_cheapest;
	DqaCoplanType coplan_type_cheapest;
} DqaInfo;

/* Information about the overall plan for a one-, two- or one coplan of
 * a three-phase aggregation.
 */
typedef struct AggPlanInfo
{
	/*
	 * The input is either represented as a Path or a Plan and a Path. If
	 * input_plan is given, use this plan instead of creating one through
	 * input_path.
	 */
	Path	   *input_path;
	Plan	   *input_plan;

	/* These are the ordinary fields characterizing an aggregation */
	CdbPathLocus input_locus;
	MppGroupPrep group_prep;
	MppGroupType group_type;
	CdbPathLocus output_locus;
	bool		distinctkey_collocate;	/* Whether the input plan collocates
										 * on the distinct key */

	/* These are extra for 3-phase plans */
	DqaJoinStrategy join_strategy;
	bool		use_sharing;

	/* These summarize the status of the structure's cost value. */
	bool		valid;
	Cost		plan_cost;
} AggPlanInfo;

typedef struct MppGroupContext
{
	MppGroupPrep prep;
	MppGroupType type;

	List	   *tlist;			/* The preprocessed targetlist of the original
								 * query. */
	Node	   *havingQual;		/* The proprocessed having qual of the
								 * original query. */
	Path	   *best_path;
	Path	   *cheapest_path;
	Plan	   *subplan;
	AggClauseCounts *agg_counts;
	double		tuple_fraction;
	double	   *p_dNumGroups;	/* Group count estimate shared up the call
								 * tree. */
	CanonicalGroupingSets *canonical_grpsets;
	int64		grouping;		/* the GROUPING value */
	bool		is_grpext;		/* identify if this is a grouping extension
								 * query */

	List	   *sub_tlist;		/* Derived (in cdb_grouping_planner) input
								 * targetlist. */
	int			numGroupCols;
	AttrNumber *groupColIdx;
	Oid		   *groupOperators;
	int			numDistinctCols;
	AttrNumber *distinctColIdx;
	DqaInfo    *dqaArgs;
	bool		use_hashed_grouping;
	CdbPathLocus input_locus;
	CdbPathLocus output_locus;

	/*
	 * Indicate whether the input plan collocates on the distinct key if any.
	 * It is used for one or two-phase aggregation. For three-phase
	 * aggregation, distinctkey_collocate inside DqaInfo is used.
	 */
	bool		distinctkey_collocate;
	List	   *current_pathkeys;

	/*
	 * Indicate if root->parse has been changed during planning.  Carry in
	 * pointer to root for miscellaneous globals.
	 */
	bool		querynode_changed;
	PlannerInfo *root;

	/* Work space for aggregate/tlist deconstruction and reconstruction */
	Index		final_varno;	/* input */
	bool		use_irefs_tlist;	/* input */
	bool		use_dqa_pruning;	/* input */
	List	   *prefs_tlist;	/* Aggref attributes for prelim_tlist */
	List	   *irefs_tlist;	/* Aggref attributes for optional inter_tlist */
	List	   *frefs_tlist;	/* Aggref attributes for optional join tlists */
	List	   *dqa_tlist;		/* DQA argument attributes for prelim_tlist */
	List	  **dref_tlists;	/* Array of DQA Aggref tlists (dqa_tlist
								 * order) */
	List	   *grps_tlist;		/* Grouping attributes for prelim_tlist */
	List	   *fin_tlist;		/* Final tlist cache. */
	List	   *fin_hqual;		/* Final having qual cache. */
	Index		split_aggref_sortgroupref;	/* for TargetEntrys made in
											 * split_aggref */
	Index		outer_varno;	/* work */
	Index		inner_varno;	/* work */
	int		   *dqa_offsets;	/* work */
	List	   *top_tlist;		/* work - the target list to finalize */

	/* 3-phase DQA decisions */
	DqaJoinStrategy join_strategy;
	bool		use_sharing;

	List	   *wagSortClauses; /* List of List; within-agg multi sort level */
} MppGroupContext;

/* Constants for aggregation approaches.
 */
#define AGG_NONE		0x00

#define AGG_1PHASE		0x01
#define AGG_2PHASE		0x02
#define AGG_2PHASE_DQA	0x04
#define AGG_3PHASE		0x08

#define AGG_SINGLEPHASE	(AGG_1PHASE)
#define AGG_MULTIPHASE	(AGG_2PHASE | AGG_2PHASE_DQA | AGG_3PHASE)

#define AGG_ALL			(AGG_SINGLEPHASE | AGG_MULTIPHASE)

/* Constants for DQA pruning:
 */
static const Index grp_varno = 1;	/* var refers to grps_tlist */
static const Index ref_varno = 2;	/* var refers to prefs_tlist or relatives */
static const Index dqa_base_varno = 3;	/* refers to one of the dref_tlists */

/* Coefficients for cost calculation adjustments: These are candidate GUCs
 * or, perhaps, replacements for the gp_eager_... series.  We wouldn't
 * need these if our statistics and cost calculations were correct, but
 * as of 3.2, they not.
 *
 * Early testing suggested that (1.0, 0.45, 1.7) was about right, but the
 * risk of introducing skew in the initial redistribution of a 1-phase plan
 * is great (especially given the 3.2 tendency to way underestimate the
 * cardinality of joins), so we penalize 1-phase and normalize to the
 * 2-phase cost (approximately).
 */
static const double gp_coefficient_1phase_agg = 20.0;	/* penalty */
static const double gp_coefficient_2phase_agg = 1.0;	/* normalized */
static const double gp_coefficient_3phase_agg = 3.3;	/* increase systematic
														 * under estimate */

/* Forward declarations */

static Plan *make_one_stage_agg_plan(PlannerInfo *root, MppGroupContext *ctx);
static Plan *make_two_stage_agg_plan(PlannerInfo *root, MppGroupContext *ctx);
static Plan *make_three_stage_agg_plan(PlannerInfo *root, MppGroupContext *ctx);
static Plan *make_plan_for_one_dqa(PlannerInfo *root, MppGroupContext *ctx,
					  int dqa_index, Plan *result_plan,
					  Query **coquery_p);
static Plan *join_dqa_coplan(PlannerInfo *root, MppGroupContext *ctx, Plan *plan, int dqa_index);
static int	compareDqas(const void *larg, const void *rarg);
static void planDqaJoinOrder(PlannerInfo *root, MppGroupContext *ctx,
				 double input_rows);
static List *make_subplan_tlist(List *tlist, Node *havingQual,
				   List *grp_clauses, int *pnum_gkeys, AttrNumber **pcols_gkeys, Oid **pcols_gops,
				   List *dqa_args, int *pnum_dqas, AttrNumber **pcols_dqas);
static List *describe_subplan_tlist(List *sub_tlist,
					   List *tlist, Node *havingQual,
					   List *grp_clauses, int *pnum_gkeys, AttrNumber **pcols_gkeys, Oid **pcols_gops,
					   List *dqa_args, int *pnum_dqas, AttrNumber **pcols_dqas);
static void generate_multi_stage_tlists(MppGroupContext *ctx,
							List **p_prelim_tlist,
							List **p_inter_tlist,
							List **p_final_tlist,
							List **p_final_qual);
static void prepare_dqa_pruning_tlists(MppGroupContext *ctx);
static void generate_dqa_pruning_tlists(MppGroupContext *ctx,
							int dqa_index,
							List **p_prelim_tlist,
							List **p_inter_tlist,
							List **p_final_tlist,
							List **p_final_qual);
static void deconstruct_agg_info(MppGroupContext *ctx);
static void reconstruct_agg_info(MppGroupContext *ctx,
					 List **p_prelim_tlist,
					 List **p_inter_tlist,
					 List **p_final_tlist,
					 List **p_final_qual);
static void reconstruct_coplan_info(MppGroupContext *ctx,
						int dqa_index,
						List **p_prelim_tlist,
						List **p_inter_tlist,
						List **p_final_tlist);
static Expr *deconstruct_expr(Expr *expr, MppGroupContext *ctx);
static Node *deconstruct_expr_mutator(Node *node, MppGroupContext *ctx);
static Node *split_aggref(Aggref *aggref, MppGroupContext *ctx);
static List *make_vars_tlist(List *tlist, Index varno, AttrNumber offset);
static Plan *add_subqueryscan(PlannerInfo *root, List **p_pathkeys,
				 Index varno, Query *subquery, Plan *subplan);
static List *seq_tlist_concat(List *tlist1, List *tlist2);
static Node *finalize_split_expr(Node *expr, MppGroupContext *ctx);
static Node *finalize_split_expr_mutator(Node *node, MppGroupContext *ctx);
static Oid	lookup_agg_transtype(Aggref *aggref);
static bool hash_safe_type(Oid type);
static bool sorting_prefixes_grouping(PlannerInfo *root);
static bool gp_hash_safe_grouping(PlannerInfo *root);

static Cost cost_common_agg(PlannerInfo *root, MppGroupContext *ctx, AggPlanInfo *info, Plan *dummy);
static Cost cost_1phase_aggregation(PlannerInfo *root, MppGroupContext *ctx, AggPlanInfo *info);
static Cost cost_2phase_aggregation(PlannerInfo *root, MppGroupContext *ctx, AggPlanInfo *info);
static Cost cost_3phase_aggregation(PlannerInfo *root, MppGroupContext *ctx, AggPlanInfo *info);
static void set_cost_of_join_strategies(MppGroupContext *ctx, Cost *hashjoin_cost, Cost *mergejoin_cost);
static void initAggPlanInfo(AggPlanInfo *info, Path *input_path, Plan *input_plan);
static void set_coplan_strategies(PlannerInfo *root, MppGroupContext *ctx, DqaInfo *dqaArg, Path *input);
static Cost incremental_sort_cost(double rows, int width, int numKeyCols);
static Cost incremental_agg_cost(double rows, int width, AggStrategy strategy,
					 int numGroupCols, double numGroups,
					 int numAggs, int transSpace);
static Cost incremental_motion_cost(double sendrows, double recvrows);

static bool contain_aggfilters(Node *node);

/*---------------------------------------------
 * WITHIN stuff
 *---------------------------------------------*/

/*
 * WithinAggContext is a variable set used throughout plan_within_agg_persort().
 */
typedef struct
{
	bool		use_deduplicate;	/* true to choose deduplicate strategy */
	AttrNumber	pc_pos;			/* resno for peer count in outer tlist */
	AttrNumber	tc_pos;			/* resno for total count in inner tlist */
	List	   *current_pathkeys;	/* pathkeys tracking */
	List	   *inner_pathkeys; /* pathkeys for inner plan */
	List	   *rtable;			/* outer/inner RTE of the output */
} WithinAggContext;

static void rebuild_simple_rel_and_rte(PlannerInfo *root);

/*
 * add_motion_to_dqa_plan
 * 		Add a Redistribute motion to a dqa child plan if the plan is not already
 * 		distributed on the grouping columns
 */
static Plan *add_motion_to_dqa_child(Plan *plan, PlannerInfo *root, bool *motion_added);

/*
 * Function: cdb_grouping_planner
 *
 * This is basically an extension of the function grouping_planner() from
 * planner.c.  It (conditionally) replaces the logic in the "normal case"
 * (the aggregation/grouping branch) of the main "if" statement.
 *
 * The result is a Plan for one-, two-, or three-phase grouping/aggregation
 * (possibly including a top-level join in case of DQA pruning) or NULL.
 *
 * A NULL result means that the ordinary sequential planner will produce
 * a correct (and preferred) result, so the "normal case" code should  be
 * used.
 *
 * A non-NULL result is taken as a partially formed Plan to be processed
 * by the trailing sort/distinct/limit logic of grouping_planner().  In
 * otherwords, after any associated changes to the local environment (see
 * the calling code), the resulting plan should be treated as if from the
 * "normal case" or the function optimize_minmax_aggregates().
 *
 * The pointer at location pcurrent_pathkeys is updated to refer to the
 * ordering pathkey or NIL, if none.  The parse tree at root->parse may
 * be modified in place to reflect changes in the context (e.g. current
 * range table).
 */

Plan *
cdb_grouping_planner(PlannerInfo *root,
					 AggClauseCounts *agg_counts,
					 GroupContext *group_context)
{
	MppGroupContext ctx;
	Plan	   *result_plan = NULL;
	List	   *sub_tlist = NIL;
	bool		has_groups = root->parse->groupClause != NIL;
	bool		has_aggs = agg_counts->numAggs > 0;
	bool		has_ordered_aggs = agg_counts->hasOrderedAggs;
	ListCell   *lc;

	bool		is_grpext = false;
	unsigned char consider_agg = AGG_NONE;
	AggPlanInfo plan_1p;
	AggPlanInfo plan_2p;
	AggPlanInfo plan_3p;
	AggPlanInfo *plan_info = NULL;

	/*
	 * We used to assert here that if has_groups is true, root->group_pathkeys
	 * != NIL. That is not a safe assumption anymore: For constants like
	 * "SELECT DISTINCT 1 FROM foo", the planner will correctly deduce that
	 * the constant "1" is the same for every row, and group_pathkeys will be
	 * NIL. But we still need to group, to remove duplicate "dummy" rows
	 * coming from all the segments.
	 */

	memset(&ctx, 0, sizeof(ctx));

	*(group_context->querynode_changed) = false;

	/*
	 * We always use sequential plans for distinct-qualified rollup queries,
	 * so don't waste time working on alternatives.
	 */
	is_grpext = is_grouping_extension(group_context->canonical_grpsets);
	if (is_grpext && agg_counts->numOrderedAggs > 0)
		return NULL;

	/*
	 * First choose a one-stage plan.  Since there's always a way to do this,
	 * it serves as our default choice.
	 */
	if (group_context->subplan == NULL)
	{
		Path	   *input_path = group_context->cheapest_path;

		/*
		 * Should we prefer the "best" path?  Only for vector aggregation of
		 * input already sorted and collocated on the grouping key.
		 */
		if (has_groups &&
			pathkeys_contained_in(root->group_pathkeys, group_context->best_path->pathkeys) &&
			cdbpathlocus_collocates(root, group_context->best_path->locus, root->group_pathkeys, false /* exact_match */ ))
		{
			input_path = group_context->best_path;
		}

		initAggPlanInfo(&plan_1p, input_path, group_context->subplan);
	}

	else
	{
		initAggPlanInfo(&plan_1p, group_context->best_path, group_context->subplan);
		plan_1p.input_locus = group_context->best_path->locus;
	}

	if (!CdbPathLocus_IsPartitioned(plan_1p.input_locus))
	{
		/* Can use base plan with no motion yielding same locus. */
		plan_1p.group_prep = MPP_GRP_PREP_NONE;
		plan_1p.output_locus = plan_1p.input_locus;
		plan_1p.distinctkey_collocate = true;
	}
	else if (has_groups)		/* and not single or replicated */
	{
		if (root->group_pathkeys != NULL &&
			cdbpathlocus_collocates(root, plan_1p.input_locus, root->group_pathkeys, false /* exact_match */ ))
		{
			plan_1p.group_prep = MPP_GRP_PREP_NONE;
			plan_1p.output_locus = plan_1p.input_locus; /* may be less
														 * discriminating that
														 * group locus */
			plan_1p.distinctkey_collocate = true;
		}
		else
		{
			if (root->group_pathkeys == NIL)
			{
				/*
				 * Grouping, but no grouping key. This arises in cases like
				 * SELECT DISTINCT <constant>, where we need to eliminate
				 * duplicates, but there is no key to hash on.
				 */
				plan_1p.group_prep = MPP_GRP_PREP_HASH_GROUPS;
				CdbPathLocus_MakeGeneral(&plan_1p.output_locus);
			}
			else if (gp_hash_safe_grouping(root))
			{
				plan_1p.group_prep = MPP_GRP_PREP_HASH_GROUPS;
				CdbPathLocus_MakeHashed(&plan_1p.output_locus, root->group_pathkeys);
			}
			else
			{
				plan_1p.group_prep = MPP_GRP_PREP_FOCUS_QE;
				CdbPathLocus_MakeSingleQE(&plan_1p.output_locus);
			}
		}
	}
	else if (has_aggs)			/* and not grouped and not single or
								 * replicated  */
	{
		plan_1p.group_prep = MPP_GRP_PREP_FOCUS_QE;
		CdbPathLocus_MakeSingleQE(&plan_1p.output_locus);
	}

	/*
	 * If the sequential planner can handle the situation with no Motion
	 * involved, let it do so.  Don't bother to investigate the 2-stage
	 * approach.
	 *
	 * If the GUC enable_groupagg is set to off and this is a DQA query, we
	 * won't use the sequential plan. This is because the sequential plan for
	 * a DQA query always uses GroupAgg.
	 */
	if (plan_1p.group_prep == MPP_GRP_PREP_NONE)
	{
		if (enable_groupagg || agg_counts->numOrderedAggs == 0)
		{
			*(group_context->pcurrent_pathkeys) = NIL;
			return NULL;
		}
	}

	/*
	 * When an input plan is given, use it, including its target list. When an
	 * input target list (and no plan) is given, use it for the plan to be
	 * created. When neither is given, generate a phase 1 target list for the
	 * plan to be created. Also note the location of any grouping attributes
	 * in the target list (numGroupCols, groupColIdx).
	 *
	 * Also make sure there's a target entry with a non-zero sortgroupref for
	 * each DQA argument and note the location of the attributes
	 * (numDistinctCols, distinctColIdx).
	 */
	if (group_context->subplan != NULL)
	{
		sub_tlist = group_context->subplan->targetlist;
	}
	else if (group_context->sub_tlist != NULL)
	{
		sub_tlist = group_context->sub_tlist;
		sub_tlist = describe_subplan_tlist(sub_tlist,
										   group_context->tlist,
										   root->parse->havingQual,
										   root->parse->groupClause,
										   &(group_context->numGroupCols),
										   &(group_context->groupColIdx),
										   &(group_context->groupOperators),
										   agg_counts->dqaArgs,
										   &(group_context->numDistinctCols),
										   &(group_context->distinctColIdx));
	}
	else
	{
		sub_tlist = make_subplan_tlist(group_context->tlist,
									   root->parse->havingQual,
									   root->parse->groupClause,
									   &(group_context->numGroupCols),
									   &(group_context->groupColIdx),
									   &(group_context->groupOperators),
									   agg_counts->dqaArgs,
									   &(group_context->numDistinctCols),
									   &(group_context->distinctColIdx));

		/*
		 * Where we need to and we can, add column names to the sub_tlist
		 * entries to make EXPLAIN output look nice.  Note that we could dig
		 * further than this (if we come up empty handed) by probing the range
		 * table (root->parse->rtable), but this covers the ordinary cases.
		 */
		foreach(lc, sub_tlist)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(lc);

			if (IsA(tle->expr, Var) &&tle->resname == NULL)
			{
				TargetEntry *vartle =
				tlist_member((Node *) tle->expr, group_context->tlist);

				if (vartle != NULL && vartle->resname != NULL)
					tle->resname = pstrdup(vartle->resname);
			}
		}
	}

	/*
	 * At this point, we're committed to producing a one- , two- or
	 * three-stage plan with motion. Determine what aggregation approaches to
	 * explore. Per MPP-2378, we don't insist on has_aggs for multi-phase
	 * plans.
	 */
	{
		unsigned char allowed_agg;
		unsigned char possible_agg;

		allowed_agg = AGG_ALL;

		if (!root->config->gp_enable_multiphase_agg)
			allowed_agg &= AGG_SINGLEPHASE;

		/*
		 * This prohibition could be relaxed if we tracked missing preliminary
		 * functions per DQA and were willing to plan some DQAs as single and
		 * some as multiple phases.  Not currently, however.
		 */
		if (agg_counts->missing_prelimfunc)
			allowed_agg &= ~AGG_MULTIPHASE;

		/*
		 * Ordered aggregates need to run the transition function on the
		 * values in sorted order, which in turn translates into single phase
		 * aggregation.
		 */
		if (has_ordered_aggs)
			allowed_agg &= ~AGG_MULTIPHASE;

		/*
		 * Same with DISTINCT aggregates (they are not counted as ordered aggs)
		 */
		if ( agg_counts->numOrderedAggs > 0 )
			allowed_agg &= ~ AGG_MULTIPHASE;

		/*
		 * We are currently unwilling to redistribute a gathered intermediate
		 * across the cluster.  This might change one day.
		 */
		if (!CdbPathLocus_IsPartitioned(plan_1p.input_locus))
			allowed_agg &= AGG_SINGLEPHASE;


		if (!root->config->gp_enable_agg_distinct)
			allowed_agg &= ~AGG_2PHASE_DQA;

		if (!root->config->gp_enable_dqa_pruning)
			allowed_agg &= ~AGG_3PHASE;

		/*
		 * GPDB_84_MERGE_FIXME: Don't do three-phase aggregation if any of the
		 * aggregates use FILTERs. We used to do it, with the old, hacky,
		 * FILTER implementation, but it doesn't work with the new one without
		 * some extra work.
		 */
		if (contain_aggfilters((Node *) group_context->tlist))
			allowed_agg &= ~AGG_3PHASE;

		possible_agg = AGG_SINGLEPHASE;

		if (gp_hash_safe_grouping(root))
		{
			switch (list_length(agg_counts->dqaArgs))
			{
				case 0:
					possible_agg |= AGG_2PHASE;
					break;
				case 1:
					possible_agg |= AGG_2PHASE_DQA | AGG_3PHASE;
					break;
				default:		/* > 1 */
					possible_agg |= AGG_3PHASE;
					break;
			}
		}
		if (is_grpext)
			possible_agg &= ~(AGG_2PHASE_DQA | AGG_3PHASE);

		consider_agg = allowed_agg & possible_agg;
	}
	Assert(consider_agg & AGG_1PHASE);	/* Always possible! */

	if (consider_agg & (AGG_2PHASE | AGG_2PHASE_DQA))
	{
		/* XXX initAggPlanInfo(&plan_2p, group_context->cheapest_path); */
		initAggPlanInfo(&plan_2p, group_context->best_path,
						group_context->subplan);	/* but why? */

		/* Common 2-phase setup. */
		if (has_groups)
		{
			plan_2p.group_type = MPP_GRP_TYPE_GROUPED_2STAGE;
			if (root->group_pathkeys == NIL)
				CdbPathLocus_MakeGeneral(&plan_2p.output_locus);
			else
				CdbPathLocus_MakeHashed(&plan_2p.output_locus, root->group_pathkeys);
		}
		else
		{
			plan_2p.group_type = MPP_GRP_TYPE_PLAIN_2STAGE;
			CdbPathLocus_MakeSingleQE(&plan_2p.output_locus);
		}

		if (consider_agg & AGG_2PHASE_DQA)
		{
			PathKey    *distinct_pathkey;
			List	   *l;

			/* Either have DQA or not! */
			Assert(!(consider_agg & AGG_2PHASE));

			Insist(IsA(agg_counts->dqaArgs, List) &&
				   list_length((List *) agg_counts->dqaArgs) == 1);
			distinct_pathkey = cdb_make_pathkey_for_expr(root,
														 linitial(agg_counts->dqaArgs),
														 list_make1(makeString("=")),
														 true);
			l = list_make1(distinct_pathkey);

			if (!cdbpathlocus_collocates(root, plan_2p.input_locus, l, false /* exact_match */ ))
			{
				plan_2p.group_prep = MPP_GRP_PREP_HASH_DISTINCT;
				CdbPathLocus_MakeHashed(&plan_2p.input_locus, l);
			}
			else
			{
				plan_2p.group_prep = MPP_GRP_PREP_HASH_DISTINCT;
				plan_2p.output_locus = plan_2p.input_locus;
				plan_2p.distinctkey_collocate = true;
			}

			list_free(l);
		}
	}

	if (consider_agg & AGG_3PHASE)
	{
		initAggPlanInfo(&plan_3p, group_context->cheapest_path,
						group_context->subplan);

		if (has_groups)
		{
			plan_3p.group_type = MPP_GRP_TYPE_GROUPED_DQA_2STAGE;
			if (root->group_pathkeys == NIL)
				CdbPathLocus_MakeGeneral(&plan_3p.output_locus);
			else
				CdbPathLocus_MakeHashed(&plan_3p.output_locus, root->group_pathkeys);
		}
		else
		{
			plan_3p.group_type = MPP_GRP_TYPE_PLAIN_DQA_2STAGE;
			CdbPathLocus_MakeSingleQE(&plan_3p.output_locus);
		}
	}

	/*
	 * Package up byproducts for the actual planner.
	 */
	ctx.prep = plan_1p.group_prep;
	ctx.type = plan_1p.group_type;
	ctx.tlist = group_context->tlist;
	ctx.havingQual = root->parse->havingQual;
	ctx.sub_tlist = sub_tlist;
	ctx.numGroupCols = group_context->numGroupCols;
	ctx.groupColIdx = group_context->groupColIdx;
	ctx.groupOperators = group_context->groupOperators;
	ctx.numDistinctCols = group_context->numDistinctCols;
	ctx.distinctColIdx = group_context->distinctColIdx;
	ctx.use_hashed_grouping = group_context->use_hashed_grouping;
	ctx.best_path = group_context->best_path;
	ctx.cheapest_path = group_context->cheapest_path;
	ctx.subplan = group_context->subplan;
	ctx.input_locus = plan_1p.input_locus;
	ctx.output_locus = plan_1p.output_locus;
	ctx.distinctkey_collocate = plan_1p.distinctkey_collocate;
	ctx.agg_counts = agg_counts;
	ctx.tuple_fraction = group_context->tuple_fraction;
	ctx.p_dNumGroups = group_context->p_dNumGroups;
	ctx.canonical_grpsets = group_context->canonical_grpsets;
	ctx.grouping = group_context->grouping;
	ctx.is_grpext = is_grpext;
	ctx.current_pathkeys = NIL; /* Initialize to be tidy. */
	ctx.querynode_changed = false;
	ctx.root = root;

	/*
	 * If we're to consider 3-phase plans, do some preparation.
	 */
	if (ctx.numDistinctCols > 0 && (consider_agg & AGG_3PHASE))
	{
		int			i;

		/*
		 * Collect row count estimates and other info for the partial results
		 * of grouping over combined grouping and distinct (DQA) keys.  Order
		 * the output array of DqaInfo structures (in the context) according
		 * to how they should be joined.
		 */
		planDqaJoinOrder(root, &ctx, plan_3p.input_path->parent->rows);

		/*
		 * Plan the post-Motion portions of each coplan in two ways: one to
		 * produce the result in the cheapest way and one to produce the
		 * result ordered by the grouping key in the cheapest way. (For use by
		 * make_plan_for_one_dqa called by make_three_stage_agg_plan.)
		 */
		for (i = 0; i < ctx.numDistinctCols; i++)
		{
			PathKey    *distinct_pathkey;
			List	   *l;

			set_coplan_strategies(root, &ctx, &ctx.dqaArgs[i], plan_3p.input_path);

			/*
			 * Determine if the input plan already collocates on the distinct
			 * key.
			 */
			distinct_pathkey = cdb_make_pathkey_for_expr(root,
														 ctx.dqaArgs[i].distinctExpr,
														 list_make1(makeString("=")),
														 true);
			l = list_make1(distinct_pathkey);

			if (cdbpathlocus_collocates(root, plan_3p.input_locus, l, false /* exact_match */ ))
			{
				ctx.dqaArgs[i].distinctkey_collocate = true;
			}

			list_free(l);
		}
	}


	plan_info = NULL;			/* Most cost-effective, feasible plan. */

	if (consider_agg & AGG_1PHASE)
	{
		cost_1phase_aggregation(root, &ctx, &plan_1p);
		if (gp_dev_notice_agg_cost)
			elog(NOTICE, "1-phase cost: %.6f", plan_1p.plan_cost);
		if (plan_info == NULL || plan_info->plan_cost > plan_1p.plan_cost)
			plan_info = &plan_1p;
	}

	if (consider_agg & (AGG_2PHASE | AGG_2PHASE_DQA))
	{
		cost_2phase_aggregation(root, &ctx, &plan_2p);
		if (gp_dev_notice_agg_cost)
			elog(NOTICE, "2-phase cost: %.6f", plan_2p.plan_cost);
		if (plan_info == NULL || plan_info->plan_cost > plan_2p.plan_cost)
			plan_info = &plan_2p;
	}

	if (consider_agg & AGG_3PHASE)
	{
		cost_3phase_aggregation(root, &ctx, &plan_3p);
		if (gp_dev_notice_agg_cost)
			elog(NOTICE, "3-phase cost: %.6f", plan_3p.plan_cost);
		if (plan_info == NULL || !enable_groupagg || plan_info->plan_cost > plan_3p.plan_cost)
			plan_info = &plan_3p;
	}

	Insist(plan_info != NULL);

	ctx.prep = plan_info->group_prep;
	ctx.type = plan_info->group_type;
	ctx.input_locus = plan_info->input_locus;
	ctx.output_locus = plan_info->output_locus;
	ctx.distinctkey_collocate = plan_info->distinctkey_collocate;
	ctx.join_strategy = plan_info->join_strategy;
	ctx.use_sharing = plan_info->use_sharing;


	/* Call appropriate planner. */
	if (ctx.type == MPP_GRP_TYPE_BASEPLAN)
	{
		if (ctx.prep != MPP_GRP_PREP_NONE)
			result_plan = make_one_stage_agg_plan(root, &ctx);
		else
			result_plan = NULL; /* allow sequential planner to do the work. */
	}
	else if (ctx.type == MPP_GRP_TYPE_PLAIN_2STAGE ||
			 ctx.type == MPP_GRP_TYPE_GROUPED_2STAGE)
		result_plan = make_two_stage_agg_plan(root, &ctx);
	else if (ctx.type == MPP_GRP_TYPE_PLAIN_DQA_2STAGE ||
			 ctx.type == MPP_GRP_TYPE_GROUPED_DQA_2STAGE)
		result_plan = make_three_stage_agg_plan(root, &ctx);
	else
		ereport(ERROR,
				(errcode(ERRCODE_CDB_INTERNAL_ERROR),
				 errmsg("no parallel plan for aggregation")));

	if (!is_grpext && result_plan != NULL &&
		IsA(result_plan, Agg))
		((Agg *) result_plan)->lastAgg = true;

	*(group_context->querynode_changed) = ctx.querynode_changed;
	*(group_context->pcurrent_pathkeys) = ctx.current_pathkeys;
	return result_plan;
}

/*
 * Function make_one_stage_agg_plan
 *
 * Construct a one-stage aggregation plan by redistributing the result
 * of the input plan appropriately
 */
static Plan *
make_one_stage_agg_plan(PlannerInfo *root,
						MppGroupContext *ctx)
{
	Query	   *parse = root->parse;
	List	   *tlist = ctx->tlist;
	List	   *sub_tlist = ctx->sub_tlist;
	int			numGroupCols = ctx->numGroupCols;
	AttrNumber *groupColIdx = ctx->groupColIdx;
	Oid		   *groupOperators = ctx->groupOperators;
	Path	   *best_path = ctx->best_path;
	Path	   *cheapest_path = ctx->cheapest_path;
	Path	   *path = NULL;
	bool		use_hashed_grouping = ctx->use_hashed_grouping;
	long		numGroups = (*(ctx->p_dNumGroups) < 0) ? 0 :
	(*(ctx->p_dNumGroups) > LONG_MAX) ? LONG_MAX :
	(long) *(ctx->p_dNumGroups);

	List	   *groupExprs = NIL;
	List	   *current_pathkeys;
	QualCost	tlist_cost;
	int			i;

	Plan	   *result_plan;
	bool		is_sorted;

	/*
	 * The argument to the "lower" Agg node will use a "flattened" tlist
	 * (having just the (levelsup==0) vars mentioned in the SELECT targetlist
	 * and HAVING qual plus entries for any GROUP BY expressions that are not
	 * simple Vars.  This is the same sub_tlist as that used for 1-stage
	 * aggregation in grouping_planner.
	 */

	/*
	 * Create the base plan.  If the best path is in grouping key order and we
	 * don't plan to move it around and this is a vector aggregation, we
	 * should use best path.  In other cases, however, use cheapest.
	 */
	if (ctx->subplan == NULL)
	{
		is_sorted = pathkeys_contained_in(root->group_pathkeys, best_path->pathkeys);
		path = cheapest_path;
		if (is_sorted && ctx->prep == MPP_GRP_PREP_NONE && numGroupCols > 0)
			path = best_path;
		result_plan = create_plan(root, path);
		current_pathkeys = path->pathkeys;

		/*
		 * It's possible that the sub_tlist doesn't contain all the sort pathkeys.
		 * Forget about such pathkeys, because we won't be able to sort back to that
		 * order, for example as part of a gather motion. (Alternatively, we could
		 * modify sub_tlist to include the any missing columns.)
		 */
		current_pathkeys =
			cdbpullup_truncatePathKeysForTargetList(current_pathkeys,
													sub_tlist);

		/*
		 * Instead of the flat target list produced above, use the sub_tlist
		 * constructed in cdb_grouping_planner.  Add a Result node if the base
		 * plan can't project. (This may be unnecessary, but, if so, the
		 * Result node will be removed later.)
		 */
		result_plan = plan_pushdown_tlist(root, result_plan, sub_tlist);

		Assert(result_plan->flow);

		/* Account for the cost of evaluation of the sub_tlist. */
		cost_qual_eval(&tlist_cost, sub_tlist, root);
		result_plan->startup_cost += tlist_cost.startup;
		result_plan->total_cost +=
			tlist_cost.startup +
			tlist_cost.per_tuple * result_plan->plan_rows;
	}
	else
	{
		result_plan = ctx->subplan;
		current_pathkeys = ctx->current_pathkeys;
	}

	/*
	 * Precondition the input by adjusting its locus prior to adding the Agg
	 * or Group node to the base plan, if needed.
	 */
	switch (ctx->prep)
	{
		case MPP_GRP_PREP_NONE:
			break;

		case MPP_GRP_PREP_HASH_GROUPS:
			Assert(numGroupCols > 0);
			for (i = 0; i < numGroupCols; i++)
			{
				TargetEntry *tle = get_tle_by_resno(sub_tlist, groupColIdx[i]);

				groupExprs = lappend(groupExprs, copyObject(tle->expr));
			}

			result_plan = (Plan *) make_motion_hash(root, result_plan, groupExprs);
			result_plan->total_cost +=
				incremental_motion_cost(result_plan->plan_rows,
										result_plan->plan_rows);
			current_pathkeys = NIL; /* No longer sorted. */
			break;

		case MPP_GRP_PREP_FOCUS_QE:
			result_plan = (Plan *) make_motion_gather_to_QE(root, result_plan, current_pathkeys);
			result_plan->total_cost +=
				incremental_motion_cost(result_plan->plan_rows,
										result_plan->plan_rows * root->config->cdbpath_segments);
			break;

		case MPP_GRP_PREP_FOCUS_QD:
			result_plan = (Plan *) make_motion_gather_to_QD(root, result_plan, current_pathkeys);
			result_plan->total_cost +=
				incremental_motion_cost(result_plan->plan_rows,
										result_plan->plan_rows * root->config->cdbpath_segments);
			break;

		case MPP_GRP_PREP_HASH_DISTINCT:
		case MPP_GRP_PREP_BROADCAST:
			ereport(ERROR,
					(errcode(ERRCODE_CDB_INTERNAL_ERROR),
					 errmsg("no parallel plan for aggregation")));
			break;				/* Never */
	}

	Assert(result_plan->flow);

	/*
	 * Insert AGG or GROUP node if needed, plus an explicit sort step if
	 * necessary.
	 *
	 * HAVING clause, if any, becomes qual of the Agg or Group node.
	 */
	if (!ctx->is_grpext && use_hashed_grouping)
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
										0,	/* num_nullcols */
										0,	/* input_grouping */
										ctx->grouping,
										0,	/* rollup_gs_times */
										ctx->agg_counts->numAggs,
										ctx->agg_counts->transitionSpace,
										result_plan);
		/* Hashed aggregation produces randomly-ordered results */
		current_pathkeys = NIL;
	}
	else if (parse->hasAggs || parse->groupClause)
	{
		if (!ctx->is_grpext)
		{
			/* Plain aggregate plan --- sort if needed */
			AggStrategy aggstrategy;

			if (parse->groupClause)
			{
				if (!pathkeys_contained_in(root->group_pathkeys,
										   current_pathkeys))
				{
					result_plan = (Plan *)
						make_sort_from_groupcols(root,
												 parse->groupClause,
												 groupColIdx,
												 false,
												 result_plan);
					current_pathkeys = root->group_pathkeys;
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

			result_plan = (Plan *) make_agg(root,
											tlist,
											(List *) parse->havingQual,
											aggstrategy, false,
											numGroupCols,
											groupColIdx,
											groupOperators,
											numGroups,
											0,	/* num_nullcols */
											0,	/* input_grouping */
											ctx->grouping,
											0,	/* rollup_gs_times */
											ctx->agg_counts->numAggs,
											ctx->agg_counts->transitionSpace,
											result_plan);
		}

		else
		{
			result_plan = plan_grouping_extension(root, path, ctx->tuple_fraction,
												  ctx->use_hashed_grouping,
												  &tlist, sub_tlist,
												  true, false,
												  (List *) parse->havingQual,
												  &numGroupCols,
												  &groupColIdx,
												  &groupOperators,
												  ctx->agg_counts,
												  ctx->canonical_grpsets,
												  ctx->p_dNumGroups,
												  &(ctx->querynode_changed),
												  &current_pathkeys,
												  result_plan);
		}
	}
	else if (root->hasHavingQual)
	{
		/*
		 * No aggregates, and no GROUP BY, but a HAVING qual is a degenerate
		 * case discussed in grouping_planner.  We can just throw away the
		 * plan-so-far and let the caller handle the whole enchilada.
		 */
		return NULL;
	}

	/*
	 * Decorate the top node with a Flow node if it doesn't have one yet. (In
	 * such cases we require the next-to-top node to have a Flow node from
	 * which we can obtain the distribution info.)
	 */
	if (!result_plan->flow)
	{
		Assert(!IsA(result_plan, Motion));
		result_plan->flow = pull_up_Flow(result_plan,
										 result_plan->lefttree);
	}

	/* Marshal implicit results. Return explicit result. */
	ctx->current_pathkeys = current_pathkeys;
	return result_plan;
}

/*
 * Function make_two_stage_agg_plan
 *
 * Construct a two-stage aggregation plan.
 */
static Plan *
make_two_stage_agg_plan(PlannerInfo *root,
						MppGroupContext *ctx)
{
	Query	   *parse = root->parse;
	List	   *prelim_tlist = NIL;
	List	   *final_tlist = NIL;
	List	   *final_qual = NIL;
	List	   *distinctExpr = NIL;
	List	   *groupExprs = NIL;
	List	   *current_pathkeys;
	Plan	   *result_plan;
	QualCost	tlist_cost;
	AggStrategy aggstrategy;
	int			i;
	int			numGroupCols;
	AttrNumber *groupColIdx;
	Oid		   *groupOperators;
	AttrNumber *prelimGroupColIdx;
	Oid		   *prelimGroupOperators;
	Path	   *path = ctx->best_path;	/* no use for ctx->cheapest_path */
	long		numGroups = (*(ctx->p_dNumGroups) < 0) ? 0 :
	(*(ctx->p_dNumGroups) > LONG_MAX) ? LONG_MAX :
	(long) *(ctx->p_dNumGroups);

	/*
	 * Copy these from context rather than using them directly because we may
	 * scribble on them in plan_grouping_extension().  It would be good to
	 * clean this up, but not today.
	 */
	numGroupCols = ctx->numGroupCols;
	groupColIdx = ctx->groupColIdx;
	groupOperators = ctx->groupOperators;

	/*
	 * Create the base plan which will serve as the outer plan (argument) of
	 * the partial Agg node.
	 */
	if (ctx->subplan == NULL)
	{
		result_plan = create_plan(root, path);
		current_pathkeys = path->pathkeys;

		/*
		 * Instead of the flat target list produced by create_plan above, use
		 * the sub_tlist constructed in cdb_grouping_planner.  This consists
		 * of just the (levelsup==0) vars mentioned in the SELECT and HAVING
		 * clauses plus entries for any GROUP BY expressions that are not
		 * simple Vars.  (This is the same sub_tlist as used in 1-stage
		 * aggregation and in normal aggregation in grouping_planner).
		 *
		 * If the base plan is of a type that can't project, add a Result node
		 * to carry the new target list, else install it directly. (Though the
		 * result node may not always be necessary, it is safe, and
		 * superfluous Result nodes are removed later.)
		 */
		result_plan = plan_pushdown_tlist(root, result_plan, ctx->sub_tlist);

		/* Account for the cost of evaluation of the sub_tlist. */
		cost_qual_eval(&tlist_cost, ctx->sub_tlist, root);
		result_plan->startup_cost += tlist_cost.startup;
		result_plan->total_cost +=
			tlist_cost.startup +
			tlist_cost.per_tuple * result_plan->plan_rows;
	}
	else
	{
		result_plan = ctx->subplan;
		current_pathkeys = ctx->current_pathkeys;
	}

	/*
	 * At this point result_plan produces the input relation for two-stage
	 * aggregation.
	 *
	 * Begin by preconditioning the input, if necessary, to collocate on
	 * non-distinct values of a single DISTINCT argument.
	 */
	switch (ctx->prep)
	{
		case MPP_GRP_PREP_NONE:
			break;

		case MPP_GRP_PREP_HASH_DISTINCT:
			Assert(list_length(ctx->agg_counts->dqaArgs) == 1);
			Assert(ctx->agg_counts->dqaArgs != NIL);
			if (!ctx->distinctkey_collocate)
			{
				distinctExpr = list_make1(linitial(ctx->agg_counts->dqaArgs));
				distinctExpr = copyObject(distinctExpr);
				result_plan = (Plan *) make_motion_hash(root, result_plan, distinctExpr);
				result_plan->total_cost +=
					incremental_motion_cost(result_plan->plan_rows,
											result_plan->plan_rows);
				current_pathkeys = NIL; /* No longer sorted. */
			}

			break;

		case MPP_GRP_PREP_FOCUS_QD:
		case MPP_GRP_PREP_FOCUS_QE:
		case MPP_GRP_PREP_HASH_GROUPS:
		case MPP_GRP_PREP_BROADCAST:
			ereport(ERROR,
					(errcode(ERRCODE_CDB_INTERNAL_ERROR),
					 errmsg("unexpected call for two-stage aggregation")));
			break;				/* Never */
	}

	/*
	 * Get the target lists for the preliminary and final aggregations and the
	 * qual (HAVING clause) for the final aggregation based on the target list
	 * of the base plan. Grouping attributes go on front of preliminary target
	 * list.
	 */
	generate_multi_stage_tlists(ctx,
								&prelim_tlist,
								NULL,
								&final_tlist,
								&final_qual);

	/*
	 * Since the grouping attributes, if any, are on the front and in order on
	 * the preliminary targetlist, we need a different vector of grouping
	 * attribute numbers: (1, 2, 3, ...).  Later, we'll need
	 */
	prelimGroupColIdx = NULL;
	prelimGroupOperators = NULL;
	if (numGroupCols > 0)
	{
		prelimGroupColIdx = (AttrNumber *) palloc(numGroupCols * sizeof(AttrNumber));
		prelimGroupOperators = (Oid *) palloc(numGroupCols * sizeof(Oid));
		for (i = 0; i < numGroupCols; i++)
		{
			prelimGroupColIdx[i] = i + 1;
			prelimGroupOperators[i] = groupOperators[i];
		}
	}

	/*
	 * Add the Preliminary Agg Node.
	 *
	 * When this aggregate is a ROLLUP, we add a sequence of preliminary Agg
	 * node.
	 */
	/* Determine the aggregation strategy to use. */
	if (ctx->use_hashed_grouping)
	{
		aggstrategy = AGG_HASHED;
		current_pathkeys = NIL;
	}
	else
	{
		if (parse->groupClause)
		{
			if (!ctx->is_grpext && !pathkeys_contained_in(root->group_pathkeys,
														  current_pathkeys))
			{
				/*
				 * TODO -- Investigate WHY we might sort here!
				 *
				 * Good reasons would be that one of the grouping expressions
				 * isn't "hashable" or that too may groups are anticipated.
				 *
				 * A bad reason would be that the final result will be in
				 * order of the grouping key.  (Redistribution will remove the
				 * ordering.)
				 */
				result_plan = (Plan *)
					make_sort_from_groupcols(root,
											 parse->groupClause,
											 groupColIdx,
											 false,
											 result_plan);
				current_pathkeys = root->group_pathkeys;
				mark_sort_locus(result_plan);
			}
			aggstrategy = AGG_SORTED;

			/*
			 * The AGG node will not change the sort ordering of its groups,
			 * so current_pathkeys describes the result too.
			 */
		}
		else
		{
			aggstrategy = AGG_PLAIN;
			current_pathkeys = NIL; /* One row, no sort order */
		}
	}

	if (!ctx->is_grpext)
	{
		result_plan = (Plan *) make_agg(root,
										prelim_tlist,
										NIL,	/* no havingQual */
										aggstrategy, root->config->gp_hashagg_streambottom,
										numGroupCols,
										groupColIdx,
										groupOperators,
										numGroups,
										0,	/* num_nullcols */
										0,	/* input_grouping */
										0,	/* grouping */
										0,	/* rollup_gs_times */
										ctx->agg_counts->numAggs,
										ctx->agg_counts->transitionSpace,
										result_plan);
		/* May lose useful locus and sort. Unlikely, but could do better. */
		mark_plan_strewn(result_plan);
		current_pathkeys = NIL;
	}

	else
	{
		result_plan = plan_grouping_extension(root, path, ctx->tuple_fraction,
											  ctx->use_hashed_grouping,
											  &prelim_tlist, ctx->sub_tlist,
											  true, true,
											  NIL,	/* no havingQual */
											  &numGroupCols,
											  &groupColIdx,
											  &groupOperators,
											  ctx->agg_counts,
											  ctx->canonical_grpsets,
											  ctx->p_dNumGroups,
											  &(ctx->querynode_changed),
											  &current_pathkeys,
											  result_plan);

		/*
		 * Since we add Grouping as an additional grouping column, we need to
		 * add it into prelimGroupColIdx.
		 */
		if (prelimGroupColIdx != NULL)
		{
			prelimGroupColIdx = (AttrNumber *)
				repalloc(prelimGroupColIdx,
						 numGroupCols * sizeof(AttrNumber));
			prelimGroupOperators = (Oid *) repalloc(prelimGroupOperators,
													numGroupCols * sizeof(Oid));
		}
		else
		{
			prelimGroupColIdx = (AttrNumber *)
				palloc0(numGroupCols * sizeof(AttrNumber));
			prelimGroupOperators = (Oid *)
				palloc0(numGroupCols * sizeof(Oid));
		}

		Assert(numGroupCols >= 2);
		prelimGroupColIdx[numGroupCols - 1] = groupColIdx[numGroupCols - 1];
		prelimGroupOperators[numGroupCols - 1] = groupOperators[numGroupCols - 1];
		prelimGroupColIdx[numGroupCols - 2] = groupColIdx[numGroupCols - 2];
		prelimGroupOperators[numGroupCols - 2] = groupOperators[numGroupCols - 2];
	}

	/*
	 * Add Intermediate Motion to Gather or Hash on Groups
	 */
	switch (ctx->type)
	{
		case MPP_GRP_TYPE_GROUPED_2STAGE:
			groupExprs = NIL;
			Assert(numGroupCols > 0);
			for (i = 0; i < numGroupCols; i++)
			{
				TargetEntry *tle;

				/* skip Grouping/GroupId columns */
				if (ctx->is_grpext && (i == numGroupCols - 1 || i == numGroupCols - 2))
					continue;

				tle = get_tle_by_resno(prelim_tlist, prelimGroupColIdx[i]);
				groupExprs = lappend(groupExprs, copyObject(tle->expr));
			}
			result_plan = (Plan *) make_motion_hash(root, result_plan, groupExprs);
			result_plan->total_cost +=
				incremental_motion_cost(result_plan->plan_rows,
										result_plan->plan_rows);
			break;

		case MPP_GRP_TYPE_PLAIN_2STAGE:
			result_plan = (Plan *) make_motion_gather_to_QE(root, result_plan, NULL);
			result_plan->total_cost +=
				incremental_motion_cost(result_plan->plan_rows,
										result_plan->plan_rows * root->config->cdbpath_segments);
			break;

		case MPP_GRP_TYPE_NONE:
		case MPP_GRP_TYPE_BASEPLAN:
		case MPP_GRP_TYPE_GROUPED_DQA_2STAGE:
		case MPP_GRP_TYPE_PLAIN_DQA_2STAGE:
			ereport(ERROR,
					(errcode(ERRCODE_CDB_INTERNAL_ERROR),
					 errmsg("unexpected use of 2-stage aggregation")));
			break;				/* Never */
	}

	/*
	 * Add Sort on Groups if needed for AGG_SORTED strategy
	 */
	if (aggstrategy == AGG_SORTED)
	{
		result_plan = (Plan *)
			make_sort_from_groupcols(root,
									 parse->groupClause,
									 prelimGroupColIdx,
									 ctx->is_grpext,
									 result_plan);
		current_pathkeys = root->group_pathkeys;
		mark_sort_locus(result_plan);
	}

	result_plan = add_second_stage_agg(root,
									   true,
									   prelim_tlist,
									   final_tlist,
									   final_qual,
									   aggstrategy,
									   numGroupCols,
									   prelimGroupColIdx,
									   prelimGroupOperators,
									   0,	/* num_nullcols */
									   0,	/* input_grouping */
									   ctx->grouping,
									   0,	/* rollup_gs_times */
									   *ctx->p_dNumGroups,
									   ctx->agg_counts->numAggs,
									   ctx->agg_counts->transitionSpace,
									   "partial_aggregation",
									   &current_pathkeys,
									   result_plan,
									   !ctx->is_grpext,
									   true);

	if (ctx->is_grpext)
	{
		ListCell   *lc;
		bool		found = false;

		((Agg *) result_plan)->inputHasGrouping = true;

		/*
		 * We want to make sure that the targetlist of result plan contains
		 * either GROUP_ID or a targetentry to represent the value of GROUP_ID
		 * from the subplans. This is because we may need this entry to
		 * determine if a tuple will be outputted repeatly, by the later
		 * Repeat node. In the current grouping extension planner, if there is
		 * no GROUP_ID entry, then it must be the last entry in the targetlist
		 * of the subplan.
		 */
		foreach(lc, result_plan->targetlist)
		{
			TargetEntry *te = (TargetEntry *) lfirst(lc);

			/*
			 * Find out if GROUP_ID in the final targetlist. It should point
			 * to the last attribute in the subplan targetlist.
			 */
			if (IsA(te->expr, Var))
			{
				Var		   *var = (Var *) te->expr;

				if (var->varattno == list_length(prelim_tlist))
				{
					found = true;
					break;
				}
			}
		}

		if (!found)
		{
			/*
			 * Add a new target entry in the targetlist which point to
			 * GROUP_ID attribute in the subplan. Mark this entry as Junk.
			 */
			TargetEntry *te = get_tle_by_resno(prelim_tlist,
											   list_length(prelim_tlist));
			Expr	   *expr;
			TargetEntry *new_te;

			expr = (Expr *) makeVar(1,
									te->resno,
									exprType((Node *) te->expr),
									exprTypmod((Node *) te->expr),
									0);
			new_te = makeTargetEntry(expr,
									 list_length(result_plan->targetlist) + 1,
									 "group_id",
									 true);
			result_plan->targetlist = lappend(result_plan->targetlist,
											  new_te);
		}
	}

	/* Marshal implicit results. Return explicit result. */
	ctx->current_pathkeys = current_pathkeys;
	ctx->querynode_changed = true;
	return result_plan;
}


/*
 * Function make_three_stage_agg_plan
 *
 * Construct a three-stage aggregation plan involving DQAs (DISTINCT-qualified
 * aggregate functions.
 *
 * Such a plan will always involve the following three aggregation phases:
 *
 * - preliminary -- remove duplicate (grouping key, DQA argument) values
 *   from an arbitrarily partitioned input; pre-aggregate plain aggregate
 *   functions.
 *
 * - intermediate -- remove duplicate (grouping key, DQA argument) values
 *   from an input partitioned on the grouping key; pre-aggregate the
 *   pre-aggregated results of preliminary plain aggregate functions.
 *
 * - final -- apply ordinary aggregation to DQA arguments (now distinct
 *   within their group) and final aggregation to the pre-aggregated results
 *   of the previous phase.
 *
 * In addition, if there is more than one DQA in the query, the plan will
 * join the results of the individual three-phase aggregations into the
 * final result.
 *
 * The preliminary aggregation phase occurs prior to the collocating
 * motion and is planned independently on the theory that any ordering
 * will be disrupted by the motion.  There are cases where this isn't
 * necessarily the case, but they are unexploited for now.
 *
 * The intermediate and final aggregation phases...
 */
static Plan *
make_three_stage_agg_plan(PlannerInfo *root, MppGroupContext *ctx)
{
	Plan	   *result_plan;
	QualCost	tlist_cost;
	Path	   *path = ctx->best_path;	/* no use for ctx->cheapest_path */

	/*
	 * We assume that we are called only when - there are no grouping
	 * extensions (like ROLLUP), - the input is partitioned and needs no
	 * preparatory Motion, - the required transformation involves DQAs.
	 */
	Assert(!is_grouping_extension(ctx->canonical_grpsets));
	Assert(ctx->prep == MPP_GRP_PREP_NONE);
	Assert(ctx->type == MPP_GRP_TYPE_GROUPED_DQA_2STAGE
		   || ctx->type == MPP_GRP_TYPE_PLAIN_DQA_2STAGE);

	/*
	 * Create the base plan which will serve as the outer plan (argument) of
	 * the partial Agg node(s).
	 */
	if (ctx->subplan == NULL)
	{
		result_plan = create_plan(root, path);

		/*
		 * Instead of the flat target list produced above, use the sub_tlist
		 * constructed in cdb_grouping_planner.  Add a Result node if the base
		 * plan can't project. (This may be unnecessary, but, if so, the
		 * Result node will be removed later.)
		 */
		result_plan = plan_pushdown_tlist(root, result_plan, ctx->sub_tlist);

		Assert(result_plan->flow);

		/* Account for the cost of evaluation of the sub_tlist. */
		cost_qual_eval(&tlist_cost, ctx->sub_tlist, root);
		result_plan->startup_cost += tlist_cost.startup;
		result_plan->total_cost +=
			tlist_cost.startup +
			tlist_cost.per_tuple * result_plan->plan_rows;
	}
	else
	{
		result_plan = ctx->subplan;
	}

	/* Use caller specified join_strategy: None, Cross, Hash, or Merge. */

	prepare_dqa_pruning_tlists(ctx);

	if (list_length(ctx->agg_counts->dqaArgs) == 1)
	{
		/*
		 * Note: single-DQA plans don't require a join and are handled
		 * specially by make_plan_for_one_dqa so we can return the result
		 * directly.
		 */
		Query	   *query;

		result_plan = make_plan_for_one_dqa(root, ctx, 0,
											result_plan, &query);
		memcpy(root->parse, query, sizeof(Query));

		pfree(query);
	}
	else
	{
		/*
		 * Multi-DQA plans are trickier because of the need to consider input
		 * sharing and the need to join the coplans back together.
		 */
		List	   *share_partners;
		int			i;
		List	   *rtable = NIL;

		if (ctx->use_sharing)
		{
			share_partners = share_plan(root, result_plan, ctx->numDistinctCols);
		}
		else
		{
			share_partners = NIL;
			share_partners = lappend(share_partners, result_plan);
			for (i = 1; i < ctx->numDistinctCols; i++)
			{
				share_partners = lappend(share_partners, copyObject(result_plan));
			}
		}

		/* Construct a coplan for each distinct DQA argument. */
		for (i = 0; i < ctx->numDistinctCols; i++)
		{
			char		buffer[50];
			int			j;
			ListCell   *l;
			Alias	   *eref;
			Plan	   *coplan;
			Query	   *coquery;

			coplan = (Plan *) list_nth(share_partners, i);
			coplan = make_plan_for_one_dqa(root, ctx, i,
										   coplan, &coquery);

			eref = makeNode(Alias);
			sprintf(buffer, "dqa_coplan_%d", i + 1);
			eref->aliasname = pstrdup(buffer);
			eref->colnames = NIL;
			j = 1;
			foreach(l, coplan->targetlist)
			{
				TargetEntry *tle = (TargetEntry *) lfirst(l);
				Value	   *colname = get_tle_name(tle, coquery->rtable, buffer);

				eref->colnames = lappend(eref->colnames, colname);
				j++;
			}

			rtable = lappend(rtable,
							 package_plan_as_rte(coquery, coplan, eref, NIL));
			ctx->dqaArgs[i].coplan = add_subqueryscan(root, NULL, i + 1, coquery, coplan);
		}

		/* Begin with the first coplan, then join in each suceeding coplan. */
		result_plan = ctx->dqaArgs[0].coplan;
		for (i = 1; i < ctx->numDistinctCols; i++)
		{
			result_plan = join_dqa_coplan(root, ctx, result_plan, i);
		}

		/*
		 * Finalize the last join plan so it has the correct target list and
		 * having qual.
		 */
		ctx->top_tlist = result_plan->targetlist;

		result_plan->targetlist = (List *) finalize_split_expr((Node *) ctx->fin_tlist, ctx);
		result_plan->qual = (List *) finalize_split_expr((Node *) ctx->fin_hqual, ctx);

		/*
		 * Reconstruct the flow since the targetlist for the result_plan may
		 * have changed.
		 */
		result_plan->flow = pull_up_Flow(result_plan, result_plan->lefttree);

		/* Need to adjust root.  Is this enuf?  I think so. */
		root->parse->rtable = rtable;
		root->parse->targetList = copyObject(result_plan->targetlist);

		/* We modified the parse tree, signal that to the caller */
		ctx->querynode_changed = true;
	}
	/* Rebuild arrays for RelOptInfo and RangeTblEntry for the PlannerInfo */
	/* since the underlying range tables have been transformed */
	rebuild_simple_rel_and_rte(root);

	return result_plan;
}


/* Helper for qsort in planDqaJoinOrder. */
int
compareDqas(const void *larg, const void *rarg)
{
	double		lft = ((DqaInfo *) larg)->num_rows;
	double		rgt = ((DqaInfo *) rarg)->num_rows;

	return (lft < rgt) ? -1 : (lft == rgt) ? 0 : 1;
}

/* Collect per distinct DQA argument information for use in single- and
 * multiple-DQA planning and cache it in the context as a new array of
 * DqaInfo structures anchored at ctx->dqaArgs.  The order of elements
 * in the array determines join order for a multi-DQA plan.
 *
 * Note: The original list of distinct DQA arguments was collected by
 * the count_agg_clauses earlier in planning.  Later,  make_subplan_tlist
 * used it to guarantee that the DQA arguments have target entries with
 * non-zero sortgroupref values and to generate  vector ctx->distinctColIdx
 * to locate those entries.  Here, however, we use that vector to locate
 * the DQA arguments and reorder the vector to agree with join order.
 */
void
planDqaJoinOrder(PlannerInfo *root, MppGroupContext *ctx,
				 double input_rows)
{
	int			i;
	DqaInfo    *args;
	Node	   *distinctExpr;

	Assert(ctx->numDistinctCols == list_length(ctx->agg_counts->dqaArgs));

	/* Collect row count estimates for the partial results. */
	if (ctx->numDistinctCols == 0)
	{
		ctx->dqaArgs = NULL;
		return;
	}

	args = (DqaInfo *) palloc(ctx->numDistinctCols * sizeof(DqaInfo));

	for (i = 0; i < ctx->numDistinctCols; i++)
	{
		TargetEntry *dtle;
		List	   *x;
		int			j;

		/*
		 * Like PG and the SQL standard, we assume that a DQA may take only a
		 * single argument -- no REGR_SXY(DISTINCT X,Y). This is what allows
		 * distinctExpr to be an expression rather than a list of expressions.
		 */
		dtle = get_tle_by_resno(ctx->sub_tlist, ctx->distinctColIdx[i]);
		distinctExpr = (Node *) dtle->expr;

		x = NIL;
		for (j = 0; j < ctx->numGroupCols; j++)
		{
			TargetEntry *tle;

			tle = get_tle_by_resno(ctx->sub_tlist, ctx->groupColIdx[j]);
			x = lappend(x, tle->expr);
		}
		x = lappend(x, distinctExpr);

		args[i].distinctExpr = distinctExpr;	/* no copy */
		args[i].base_index = dtle->resno;
		args[i].num_rows = estimate_num_groups(root, x, input_rows);
		args[i].can_hash = hash_safe_type(exprType(distinctExpr));

		list_free(x);
	}
	qsort(args, ctx->numDistinctCols, sizeof(DqaInfo), compareDqas);

	/* Reorder ctx->distinctColIdx to agree with join order. */
	for (i = 0; i < ctx->numDistinctCols; i++)
	{
		ctx->distinctColIdx[i] = args[i].base_index;
	}

	ctx->dqaArgs = args;
}


/* Function make_plan_for_one_dqa
 *
 * Subroutine for make_three_stage_agg_plan constructs a coplan for
 * the specified DQA index [0..numDistinctCols-1] which selects a DqaInfo
 * entry from the context.
 *
 * In multi-DQA plans, coplans have minimal targetlists (just grouping
 * keys, DQA arguments, and results of single aggregate functions).  In
 * case this is a single-DQA (join-less) plan, the coplan target list is
 * "finalized" to produce the result requested by the user (which may
 * include expressions over the minimal list in the targetlist and/or
 * having qual).
 *
 * A Query (including range table) which approximates a query for the
 * returned plan is stored back into *coquery_p, if coquery_p is not NULL.
 */
static Plan *
make_plan_for_one_dqa(PlannerInfo *root, MppGroupContext *ctx, int dqa_index,
					  Plan *result_plan, Query **coquery_p)
{
	DqaCoplanType coplan_type;
	List	   *prelim_tlist = NIL;
	List	   *inter_tlist = NIL;
	List	   *final_tlist = NIL;
	List	   *final_qual = NIL;
	List	   *groupExprs = NIL;
	List	   *current_pathkeys;
	AggStrategy aggstrategy;
	AttrNumber *prelimGroupColIdx;
	Oid		   *prelimGroupOperators;
	AttrNumber *inputGroupColIdx;
	Oid		   *inputGroupOperators;
	List	   *extendedGroupClause;
	Query	   *original_parse;
	bool		groups_sorted = false;
	long		numGroups;
	int			i,
				n;
	DqaInfo    *dqaArg = &ctx->dqaArgs[dqa_index];
	bool		sort_coplans = (ctx->join_strategy == DqaJoinMerge);
	bool		groupkeys_collocate = cdbpathlocus_collocates(root, ctx->input_locus, root->group_pathkeys, false /* exact_match */ );
	bool		need_inter_agg = false;
	bool		dqaduphazard = false;
	bool		stream_bottom_agg = root->config->gp_hashagg_streambottom;	/* Take hint */

	/*
	 * Planning will perturb root->parse, so we copy it's content aside so we
	 * can restore it later.  We flat copy instead of resetting because code
	 * in the stack may have a local variable set to the value of root->parse.
	 */
	original_parse = makeNode(Query);
	memcpy(original_parse, root->parse, sizeof(Query));

	/*
	 * Our caller, make_three_stage_agg_plan, pushed ctx->sub_tlist onto
	 * result_plan.  This contains all the keys and arguments for the whole
	 * query.  While it would be possible to generate a smaller targetlist to
	 * use for this single DQA it is probably not worth the complexity.  Just
	 * use sub_tlist as given.
	 *
	 * The DQA argument of interest is attribute dqaArg->baseIndex.
	 *
	 * Get the target lists for the preliminary, intermediate and final
	 * aggregations and the qual (HAVING clause) for the final aggregation
	 * based on the target list of the base plan. Grouping attributes go on
	 * front of preliminary and intermediate target lists.
	 */
	generate_dqa_pruning_tlists(ctx,
								dqa_index,
								&prelim_tlist,
								&inter_tlist,
								&final_tlist,
								&final_qual);

	/*
	 * For the first aggregation phases the original grouping attributes
	 * (maybe zero of them) must be extended to include the DQA argument
	 * attribute (exactly one of them) to be pruned.
	 *
	 * The grouping attributes and a single DQA argument are on the front and
	 * in order on the preliminary and intermediate targetlists so we need a
	 * new vector of grouping attributes, prelimGroupColIdx = (1, 2, 3, ...),
	 * for use in these aggregations. The vector inputGroupColIdx plays a
	 * similar role for sub_tlist.
	 *
	 * The initial-phase group clause, extendedGroupClause, is the one in the
	 * query (assumed to have no grouping extensions) augmented by a
	 * GroupClause node for the DQA argument.  This is where the sort operator
	 * for the DQA argument is selected.
	 */
	 {
		SortGroupClause *gc;
		TargetEntry *tle;
		Oid			dqaArg_orderingop;
		Oid			dqaArg_eqop;

		get_sort_group_operators(exprType((Node *) dqaArg->distinctExpr),
								 true, true, false,
								 &dqaArg_orderingop, &dqaArg_eqop, NULL);

		n = ctx->numGroupCols + 1;	/* add the DQA argument as a grouping key */
		Assert(n > 0);

		prelimGroupColIdx = (AttrNumber *) palloc(n * sizeof(AttrNumber));
		prelimGroupOperators = (Oid *) palloc(n * sizeof(Oid));

		gc = makeNode(SortGroupClause);
		tle = get_tle_by_resno(ctx->sub_tlist, dqaArg->base_index);
		gc->tleSortGroupRef = tle->ressortgroupref;
		gc->eqop = dqaArg_eqop;
		gc->sortop = dqaArg_orderingop;

		extendedGroupClause = list_copy(root->parse->groupClause);
		extendedGroupClause = lappend(extendedGroupClause, gc);

		for (i = 0; i < ctx->numGroupCols; i++)
		{
			prelimGroupColIdx[i] = i + 1;
			prelimGroupOperators[i] = ctx->groupOperators[i];
		}
		prelimGroupColIdx[i] = i + 1;
		prelimGroupOperators[i] = dqaArg_eqop;
		if (!OidIsValid(prelimGroupOperators[i]))	/* shouldn't happen */
			elog(ERROR, "could not find equality operator for ordering operator %u",
				 prelimGroupOperators[i]);

		inputGroupColIdx = (AttrNumber *) palloc(n * sizeof(AttrNumber));
		inputGroupOperators = (Oid *) palloc(n * sizeof(Oid));

		for (i = 0; i < ctx->numGroupCols; i++)
		{
			inputGroupColIdx[i] = ctx->groupColIdx[i];
			inputGroupOperators[i] = ctx->groupOperators[i];
		}
		inputGroupColIdx[ctx->numGroupCols] = dqaArg->base_index;
		inputGroupOperators[ctx->numGroupCols] = dqaArg_eqop;
	}

	/*
	 * Determine the first-phase aggregation strategy to use.  Prefer hashing
	 * to sorting because the benefit of the sort will be lost by the Motion
	 * to follow.
	 */
	if (dqaArg->use_hashed_preliminary)
	{
		aggstrategy = AGG_HASHED;
		current_pathkeys = NIL;
	}
	else
	{
		/*
		 * Here we need to sort! The input pathkeys won't contain the DQA
		 * argument, so just do it.
		 */
		result_plan = (Plan *)
			make_sort_from_groupcols(root,
									 extendedGroupClause,
									 inputGroupColIdx,
									 false,
									 result_plan);
		current_pathkeys = root->group_pathkeys;
		mark_sort_locus(result_plan);
		aggstrategy = AGG_SORTED;

		/*
		 * The AGG node will not change the sort ordering of its groups, so
		 * current_pathkeys describes the result too.
		 */
	}

	/*
	 * Preliminary Aggregation:  With the pre-existing distribution, group by
	 * the combined grouping key and DQA argument.  In the case of the first
	 * coplan, this phase also pre-aggregates any non-DQAs.  This eliminates
	 * duplicate values of the DQA argument on each QE.
	 */
	numGroups = (dqaArg->num_rows < 0) ? 0 :
		(dqaArg->num_rows > LONG_MAX) ? LONG_MAX :
		(long) dqaArg->num_rows;

	/*
	 * If the data is distributed on the distinct qualified aggregate's key
	 * and there is no grouping key, then we prefer to not stream the bottom
	 * agg
	 */
	if (dqaArg->distinctkey_collocate && ctx->numGroupCols == 0)
	{
		stream_bottom_agg = false;
	}

	result_plan = (Plan *) make_agg(root,
									prelim_tlist,
									NIL, /* no havingQual */
									aggstrategy, stream_bottom_agg,
									ctx->numGroupCols + 1,
									inputGroupColIdx,
									inputGroupOperators,
									numGroups,
									0, /* num_nullcols */
									0, /* input_grouping */
									0, /* grouping */
									0, /* rollup_gs_times */
									ctx->agg_counts->numAggs - ctx->agg_counts->numOrderedAggs + 1,
									ctx->agg_counts->transitionSpace, /* worst case */
									result_plan);

	dqaduphazard = (aggstrategy == AGG_HASHED && stream_bottom_agg);

	result_plan->flow = pull_up_Flow(result_plan, result_plan->lefttree);

	current_pathkeys = NIL;

	/*
	 * Intermediate Motion: Gather or Hash on Groups to get colocation on the
	 * grouping key.  Note that this may bring duplicate values of the DQA
	 * argument together on the QEs.
	 */
	switch (ctx->type)
	{
		case MPP_GRP_TYPE_GROUPED_DQA_2STAGE:
			if (!groupkeys_collocate)
			{
				groupExprs = NIL;
				Assert(ctx->numGroupCols > 0);
				for (i = 0; i < ctx->numGroupCols; i++)
				{
					TargetEntry *tle;

					tle = get_tle_by_resno(prelim_tlist, prelimGroupColIdx[i]);
					groupExprs = lappend(groupExprs, copyObject(tle->expr));
				}
				result_plan = (Plan *) make_motion_hash(root, result_plan, groupExprs);
				result_plan->total_cost +=
					incremental_motion_cost(result_plan->plan_rows,
											result_plan->plan_rows);
			}

			break;

		case MPP_GRP_TYPE_PLAIN_DQA_2STAGE:

			/*
			 * Assert that this is only called for a plain DQA like select
			 * count(distinct x) from foo
			 */

			Assert(ctx->numGroupCols == 0); /* No group-by */
			Assert(n == 1);

			/* If already collocated on DQA arg, don't redistribute */
			if (!dqaArg->distinctkey_collocate)
			{
				TargetEntry *tle = get_tle_by_resno(ctx->sub_tlist, dqaArg->base_index);

				Assert(tle);
				groupExprs = lappend(NIL, copyObject(tle->expr));

				result_plan = (Plan *) make_motion_hash(root, result_plan, groupExprs);
				result_plan->total_cost +=
					incremental_motion_cost(result_plan->plan_rows,
											result_plan->plan_rows);
			}
			break;

		case MPP_GRP_TYPE_NONE:
		case MPP_GRP_TYPE_BASEPLAN:
		case MPP_GRP_TYPE_GROUPED_2STAGE:
		case MPP_GRP_TYPE_PLAIN_2STAGE:
			ereport(ERROR,
					(errcode(ERRCODE_CDB_INTERNAL_ERROR),
					 errmsg("unexpected use of DQA pruned 2-phase aggregation")));
			break;				/* Never */
	}
	current_pathkeys = NIL;

	groups_sorted = false;

	if (sort_coplans)
	{
		coplan_type = dqaArg->coplan_type_sorted;
	}
	else
	{
		coplan_type = dqaArg->coplan_type_cheapest;
	}

	if (dqaduphazard ||
		(!dqaArg->distinctkey_collocate && !groupkeys_collocate))
	{
		/*
		 * Intermediate Aggregation: Grouping key values are colocated so
		 * group by the combined grouping key and DQA argument while
		 * intermediate- aggregating any non-DQAs.  This once again (and
		 * finally) eliminates duplicate values of the DQA argument on each
		 * QE.
		 */
		need_inter_agg = true;

		switch (coplan_type)
		{
			case DQACOPLAN_GGS:
			case DQACOPLAN_PGS:
				aggstrategy = AGG_SORTED;

				/* pre-sort required on combined grouping key and DQA argument */
				result_plan = (Plan *)
					make_sort_from_groupcols(root,
											 extendedGroupClause,
											 prelimGroupColIdx,
											 false,
											 result_plan);
				groups_sorted = true;
				current_pathkeys = root->group_pathkeys;
				mark_sort_locus(result_plan);
				break;

			case DQACOPLAN_GSH:
			case DQACOPLAN_SHH:
			case DQACOPLAN_HH:
			case DQACOPLAN_PH:
				aggstrategy = AGG_HASHED;
				groups_sorted = false;
				break;
		}

		result_plan = add_second_stage_agg(root,
										   true,
										   prelim_tlist,
										   inter_tlist,
										   NULL,
										   aggstrategy,
										   ctx->numGroupCols + 1,
										   prelimGroupColIdx,
										   prelimGroupOperators,
										   0,	/* num_nullcols */
										   0,	/* input_grouping */
										   0,	/* grouping */
										   0,	/* rollup_gs_times */
										   dqaArg->num_rows,
										   ctx->agg_counts->numAggs,
										   ctx->agg_counts->transitionSpace,
										   "partial_aggregation",
										   &current_pathkeys,
										   result_plan,
										   true, false);
	}

	/*
	 * Final Aggregation: Group by the grouping key, aggregate the now
	 * distinct values of the DQA argument using non-distinct-qualified
	 * aggregation, final aggregate the intermediate values of any non-DQAs.
	 */

	switch (coplan_type)
	{
		case DQACOPLAN_GSH:
			/* pre-sort required on grouping key */
			result_plan = (Plan *)
				make_sort_from_groupcols(root,
										 root->parse->groupClause,
										 prelimGroupColIdx,
										 false,
										 result_plan);
			groups_sorted = true;
			current_pathkeys = root->group_pathkeys;
			mark_sort_locus(result_plan);
			/* Fall though. */

		case DQACOPLAN_GGS:
			aggstrategy = AGG_SORTED;
			break;

		case DQACOPLAN_SHH:
		case DQACOPLAN_HH:
			aggstrategy = AGG_HASHED;
			groups_sorted = false;
			break;

		case DQACOPLAN_PGS:
		case DQACOPLAN_PH:
			/* plainagg */
			aggstrategy = AGG_PLAIN;
			groups_sorted = false;
			break;
	}

	/**
	 * In the case where there is no grouping key, we need to gather up all the rows in a single segment to compute the final aggregate.
	 */
	if (ctx->type == MPP_GRP_TYPE_PLAIN_DQA_2STAGE)
	{
		/*
		 * Assert that this is only called for a plain DQA like select
		 * count(distinct x) from foo
		 */
		Assert(ctx->numGroupCols == 0); /* No grouping columns */
		Assert(n == 1);

		result_plan = (Plan *) make_motion_gather_to_QE(root, result_plan, NULL);
		result_plan->total_cost +=
			incremental_motion_cost(result_plan->plan_rows,
									result_plan->plan_rows * root->config->cdbpath_segments);
	}

	result_plan = add_second_stage_agg(root,
									   true,
									   need_inter_agg ? inter_tlist : prelim_tlist,
									   final_tlist,
									   final_qual,
									   aggstrategy,
									   ctx->numGroupCols,
									   prelimGroupColIdx,
									   prelimGroupOperators,
									   0,	/* num_nullcols */
									   0,	/* input_grouping */
									   ctx->grouping,
									   0,	/* rollup_gs_times */
									   *ctx->p_dNumGroups,
									   ctx->agg_counts->numAggs,
									   ctx->agg_counts->transitionSpace,
									   "partial_aggregation",
									   &current_pathkeys,
									   result_plan,
									   true,
									   false);

	/* Final sort */
	switch (coplan_type)
	{
		case DQACOPLAN_SHH:
			/* post-sort required */
			result_plan = (Plan *)
				make_sort_from_groupcols(root,
										 root->parse->groupClause,
										 prelimGroupColIdx,
										 false,
										 result_plan);
			groups_sorted = true;
			current_pathkeys = root->group_pathkeys;
			mark_sort_locus(result_plan);
			break;

		case DQACOPLAN_GGS:
		case DQACOPLAN_GSH:
		case DQACOPLAN_HH:
		case DQACOPLAN_PGS:
		case DQACOPLAN_PH:
			break;
	}

	/* Marshal implicit results. Return explicit result. */
	if (groups_sorted)
	{
		/*
		 * The following settings work correctly though they seem wrong.
		 * Though we changed the query tree, we say that we did not so that
		 * planner.c will notice the useful sort order we have produced. We
		 * also reset the current pathkeys to the original group keys. (Though
		 * our target list may differ, its attribute-wise ordering is on the
		 * group keys.)
		 */
		ctx->current_pathkeys = root->group_pathkeys;	/* current_pathkeys are
														 * wrong! */
		ctx->querynode_changed = false;
	}
	else
	{
		ctx->current_pathkeys = NIL;
		ctx->querynode_changed = true;
	}

	/* If requested, copy our modified Query (at root->parse) for caller. */
	if (coquery_p != NULL)
	{
		*coquery_p = makeNode(Query);
		memcpy(*coquery_p, root->parse, sizeof(Query));
	}

	/* Restore the original referent of root->parse. */
	memcpy(root->parse, original_parse, sizeof(Query));
	pfree(original_parse);

	return result_plan;
}


static Plan *
join_dqa_coplan(PlannerInfo *root, MppGroupContext *ctx, Plan *outer, int dqa_index)
{
	Plan	   *join_plan = NULL;
	Plan	   *inner = ctx->dqaArgs[dqa_index].coplan;
	List	   *join_tlist = NIL;
	List	   *tlist = NIL;
	Index		outer_varno = 1;
	Index		inner_varno = dqa_index + 1;
	Index		varno = 1;
	int			i,
				ng,
				nd;

	/*---------------------------------------------------------------------
	 * Make the target list for this join.  The outer and inner target lists
	 * will look like
	 *		(<g'> <D0'> ... <Dn-1'> <F'>) and (<g'> <Dn>)
	 * or
	 *		(<g'> <D0> <F>) and (<g'> <Dn>)
	 * The join target list should look like
	 *		(<g'> <D0'> ... <Dn'> <F'>)
	 *---------------------------------------------------------------------
	 */
	/* Use varno 1 for grouping key. */
	join_tlist = make_vars_tlist(ctx->grps_tlist, varno, 0);

	ng = list_length(join_tlist);	/* (<g'>) */
	nd = ng + list_length(ctx->dref_tlists[0]); /* (<g'> <D0'>) */

	for (i = 0; i <= dqa_index; i++)
	{
		tlist = make_vars_tlist(ctx->dref_tlists[i], varno + i, ng);
		join_tlist = seq_tlist_concat(join_tlist, tlist);	/* (... <Di'>) */
	}

	tlist = make_vars_tlist(ctx->frefs_tlist, varno, nd);
	join_tlist = seq_tlist_concat(join_tlist, tlist);	/* (... <F'>) */

	/*
	 * Make the join which will be either a cartesian product (in case of
	 * scalar aggregation) or a merge or hash join (in case of grouped
	 * aggregation.)
	 */
	if (ctx->numGroupCols > 0)	/* MergeJoin: 1x1 */
	{
		List	   *joinclause = NIL;
		List	   *hashclause = NIL;
		AttrNumber	attrno;

		Insist(ctx->join_strategy == DqaJoinMerge || ctx->join_strategy == DqaJoinHash);

		/*
		 * Make the join clause -- a conjunction of IS NOT DISTINCT FROM
		 * predicates on the attributes of the grouping key.
		 */
		for (attrno = 1; attrno <= ctx->numGroupCols; attrno++)
		{
			Expr	   *qual;
			Var		   *outer_var;
			Var		   *inner_var;
			RestrictInfo *rinfo;
			TargetEntry *tle = get_tle_by_resno(outer->targetlist, attrno);

			Assert(tle && IsA(tle->expr, Var));

			outer_var = (Var *) copyObject(tle->expr);
			outer_var->varno = outer_varno;
			outer_var->varnoold = outer_varno;

			inner_var = (Var *) copyObject(tle->expr);
			inner_var->varno = inner_varno;
			inner_var->varnoold = inner_varno;

			/* outer should always be on the left */
			if (ctx->join_strategy == DqaJoinHash)
			{
				qual = make_op(NULL, list_make1(makeString("=")),
							   (Node *) outer_var,
							   (Node *) inner_var, -1);
				hashclause = lappend(hashclause, copyObject(qual));
			}
			rinfo = make_mergeclause((Node *) outer_var, (Node *) inner_var);

			joinclause = lappend(joinclause, rinfo);
		}

		if (ctx->join_strategy == DqaJoinHash)
		{
			/* Make the hash join. */
			bool		motion_added_outer = false;
			bool		motion_added_inner = false;
			Oid			skewTable = InvalidOid;
			AttrNumber	skewColumn = InvalidAttrNumber;
			Oid			skewColType = InvalidOid;
			int32		skewColTypmod = -1;

			outer = add_motion_to_dqa_child(outer, root, &motion_added_outer);
			inner = add_motion_to_dqa_child(inner, root, &motion_added_inner);

			bool		prefetch_inner = motion_added_outer || motion_added_inner;

			if (motion_added_outer || motion_added_inner)
			{
				ctx->current_pathkeys = NULL;
			}

			/*
			 * If there is a single join clause and we can identify the outer
			 * variable as a simple column reference, supply its identity for
			 * possible use in skew optimization.  (Note: in principle we could
			 * do skew optimization with multiple join clauses, but we'd have
			 * to be able to determine the most common combinations of outer
			 * values, which we don't currently have enough stats for.)
			 */
			if (list_length(hashclause) == 1)
			{
				OpExpr	   *clause = (OpExpr *) linitial(hashclause);
				Node	   *node;

				Assert(is_opclause(clause));
				node = (Node *) linitial(clause->args);
				if (IsA(node, RelabelType))
					node = (Node *) ((RelabelType *) node)->arg;
				if (IsA(node, Var))
				{
					Var		   *var = (Var *) node;
					RangeTblEntry *rte;

					rte = root->simple_rte_array[var->varno];
					if (rte->rtekind == RTE_RELATION)
					{
						skewTable = rte->relid;
						skewColumn = var->varattno;
						skewColType = var->vartype;
						skewColTypmod = var->vartypmod;
					}
				}
			}

			Hash	   *hash_plan = make_hash(inner,
											  skewTable,
											  skewColumn,
											  skewColType,
											  skewColTypmod);

			joinclause = get_actual_clauses(joinclause);
			join_plan = (Plan *) make_hashjoin(join_tlist,
											   NIL, /* joinclauses */
											   NIL, /* otherclauses */
											   hashclause,	/* hashclauses */
											   joinclause,	/* hashqualclauses */
											   outer, (Plan *) hash_plan,
											   JOIN_INNER);
			((Join *) join_plan)->prefetch_inner = prefetch_inner;
		}
		else
		{
			/*
			 * Make the merge join noting that the outer plan produces rows
			 * distinct in the join key.  (So does the inner, for that matter,
			 * but the MJ algorithm is only sensitive to the outer.)
			 */
			Oid		   *mergefamilies = palloc(sizeof(Oid) * list_length(joinclause));
			int		   *mergestrategies = palloc(sizeof(int) * list_length(joinclause));
			bool	   *mergenullsfirst = palloc(sizeof(bool) * list_length(joinclause));
			ListCell   *l;
			int			i = 0;

			foreach(l, joinclause)
			{
				RestrictInfo *rinfo = (RestrictInfo *) lfirst(l);

				mergefamilies[i] = linitial_oid(rinfo->mergeopfamilies);
				mergestrategies[i] = BTLessStrategyNumber;
				mergenullsfirst[i] = false;
				i++;
			}

			joinclause = get_actual_clauses(joinclause);
			join_plan = (Plan *) make_mergejoin(join_tlist,
												NIL, NIL,
												joinclause,
												mergefamilies,
												mergestrategies,
												mergenullsfirst,
												outer, inner,
												JOIN_INNER);
			((MergeJoin *) join_plan)->unique_outer = true;
		}
	}
	else						/* NestLoop: Cartesian product: 1x1 */
	{
		Insist(ctx->join_strategy == DqaJoinCross);

		join_plan = (Plan *) make_nestloop(join_tlist,
										   NIL, NIL,
										   outer, inner,
										   JOIN_INNER);
		((NestLoop *) join_plan)->singleton_outer = true;
	}

	join_plan->startup_cost = outer->startup_cost + inner->startup_cost;
	join_plan->plan_rows = outer->plan_rows;
	join_plan->plan_width = outer->plan_width + inner->plan_width;	/* too high for MJ */
	join_plan->total_cost = outer->total_cost + inner->total_cost;
	join_plan->total_cost += cpu_tuple_cost * join_plan->plan_rows;

	join_plan->flow = pull_up_Flow(join_plan, join_plan->lefttree);

	return join_plan;
}


/*
 * Function make_subplan_tlist (for multi-phase aggregation)
 *
 * The input to the "lower" Agg node will use a "flattened" tlist (having
 * just the (levelsup==0) vars mentioned in the targetlist and HAVING qual.
 * This is similar to the target list produced by make_subplanTargetList
 * and used for 1-stage aggregation in grouping_planner.
 *
 * The result target list contains entries for all the simple Var attributes
 * of the original SELECT and HAVING clauses, plus entries for any GROUP BY
 * expressions and DQA arguments that are not simple Vars.
 *
 * The implicit results are
 *
 * - the number of grouping attributes and a vector of their positions
 *   (which are equal to their resno's) in the target list delivered through
 *   pointers pnum_gkeys and pcols_gkeys, and
 *
 * - the number of distinct arguments to DISTINCT-qualified aggregate
 *   function and a vector of their positions (which are equal to their
 *   resno's) in the target list delivered through pointers pnum_dqas and
 *   pcols_dqas.  These arguments are guaranteed (by the call to function
 *   augment_subplan_tlist) to appear as attributes of the subplan target
 *   list.
 *
 * There are no similar results for sort and distinct attributes since
 * they don't necessarily appear in the subplan target list.
 */
List *
make_subplan_tlist(List *tlist, Node *havingQual,
				   List *grp_clauses,
				   int *pnum_gkeys, AttrNumber **pcols_gkeys, Oid **pcols_gops,
				   List *dqa_args,
				   int *pnum_dqas, AttrNumber **pcols_dqas)
{
	List	   *sub_tlist;
	List	   *extravars;

	int			num_gkeys;
	AttrNumber *cols_gkeys;
	Oid		   *cols_gops;

	Assert(dqa_args != NIL ? pnum_dqas != NULL && pcols_dqas != NULL : true);

	/* GPDB_84_MERGE_FIXME: Should we pass includePlaceHolderVars as true */
	/* in pull_var_clause ? */
	sub_tlist = flatten_tlist(tlist,
							  PVC_RECURSE_AGGREGATES,
							  PVC_INCLUDE_PLACEHOLDERS);
	extravars = pull_var_clause(havingQual,
								PVC_RECURSE_AGGREGATES,
								PVC_REJECT_PLACEHOLDERS);
	sub_tlist = add_to_flat_tlist(sub_tlist, extravars);
	list_free(extravars);

	num_gkeys = num_distcols_in_grouplist(grp_clauses);
	if (num_gkeys > 0)
	{
		int			keyno = 0;
		List	   *tles;
		List	   *sortops;
		List	   *eqops;
		ListCell   *lc_tle;
		ListCell   *lc_eqop;

		cols_gkeys = (AttrNumber *) palloc(sizeof(AttrNumber) * num_gkeys);
		cols_gops = (Oid *) palloc(sizeof(Oid) * num_gkeys);

		get_sortgroupclauses_tles(grp_clauses, tlist, &tles, &sortops, &eqops);

		forboth (lc_tle, tles, lc_eqop, eqops)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(lc_tle);
			Node	   *expr = (Node *) tle->expr;;
			TargetEntry *sub_tle = NULL;
			ListCell   *sl;

			/* Find or make a matching sub_tlist entry. */
			foreach(sl, sub_tlist)
			{
				sub_tle = (TargetEntry *) lfirst(sl);
				if (equal(expr, sub_tle->expr) && (sub_tle->ressortgroupref == 0))
					break;
			}
			if (!sl)
			{
				sub_tle = makeTargetEntry((Expr *) expr,
										  list_length(sub_tlist) + 1,
										  NULL,
										  false);
				sub_tlist = lappend(sub_tlist, sub_tle);
			}

			/* Set its group reference and save its resno */
			sub_tle->ressortgroupref = tle->ressortgroupref;
			cols_gkeys[keyno] = sub_tle->resno;

			cols_gops[keyno] = lfirst_oid(lc_eqop);
			if (!OidIsValid(cols_gops[keyno]))          /* shouldn't happen */
				elog(ERROR, "could not find equality operator for ordering operator %u",
					 cols_gops[keyno]);
			keyno++;
		}
		*pnum_gkeys = num_gkeys;
		*pcols_gkeys = cols_gkeys;
		*pcols_gops = cols_gops;
	}
	else
	{
		*pnum_gkeys = 0;
		*pcols_gkeys = NULL;
		*pcols_gops = NULL;
	}

	if (dqa_args != NIL)
		sub_tlist = augment_subplan_tlist(sub_tlist, dqa_args, pnum_dqas, pcols_dqas, true);

	return sub_tlist;			/* Possibly modified by appending expression
								 * entries. */
}


/* Function augment_subplan_tlist
 *
 * Called from make_subplan_tlist, not directly.
 *
 * Make a target list like the input that includes sortgroupref'd entries
 * for the expressions in exprs.  Note that the entries in the input expression
 * list must be distinct.
 *
 * New entries corresponding to the expressions in the input exprs list
 * (if any) are added to the argument list.  Existing entries are modified
 * (if necessary) in place.
 *
 * Return the (modified) input targetlist.
 *
 * Implicitly return an array of resno values for exprs in (pnum, *pcols), if
 * return_resno is true.
 */
List *
augment_subplan_tlist(List *tlist, List *exprs, int *pnum, AttrNumber **pcols,
					  bool return_resno)
{
	int			num;
	AttrNumber *cols = NULL;

	num = list_length(exprs);	/* Known to be distinct. */
	if (num > 0)
	{
		int			keyno = 0;
		ListCell   *lx,
				   *lt;
		TargetEntry *tle,
				   *matched_tle;
		Index		max_sortgroupref = 0;

		foreach(lt, tlist)
		{
			tle = (TargetEntry *) lfirst(lt);
			if (tle->ressortgroupref > max_sortgroupref)
				max_sortgroupref = tle->ressortgroupref;
		}

		if (return_resno)
			cols = (AttrNumber *) palloc(sizeof(AttrNumber) * num);

		foreach(lx, exprs)
		{
			Node	   *expr = (Node *) lfirst(lx);

			matched_tle = NULL;

			foreach(lt, tlist)
			{
				tle = (TargetEntry *) lfirst(lt);

				if (equal(expr, tle->expr))
				{
					matched_tle = tle;
					break;
				}
			}

			if (matched_tle == NULL)
			{
				matched_tle = makeTargetEntry((Expr *) expr,
											  list_length(tlist) + 1,
											  NULL,
											  false);
				tlist = lappend(tlist, matched_tle);
			}

			if (matched_tle->ressortgroupref == 0)
				matched_tle->ressortgroupref = ++max_sortgroupref;

			if (return_resno)
				cols[keyno++] = matched_tle->resno;
		}

		if (return_resno)
		{
			*pnum = num;
			*pcols = cols;
		}
	}
	else
	{
		if (return_resno)
		{
			*pnum = 0;
			*pcols = NULL;
		}
	}

	/*
	 * Note that result is a copy, possibly modified by appending expression
	 * targetlist entries and/or updating sortgroupref values.
	 */
	return tlist;
}

/*
 * Function describe_subplan_tlist (for single-phase aggregation)
 *
 * Implicitly return extra information about a supplied target list with having
 * qual and and corresponding sub plan target list for single-phase aggregation.
 * This does, essentially, what make_subplan_tlist does, but for a precalculated
 * subplan target list.  In particular
 *
 * - it constructs the grouping key -> subplan target list resno map.
 * - it may extend the subplan targetlist for DQAs and record entries
 *   in the DQA argument -> subplan target list resno map.
 *
 * In the later case, the subplan target list may be extended, so return it.
 * This function is for the case when a subplan target list (not a whole plan)
 * is supplied to cdb_grouping_planner.
 */
List *
describe_subplan_tlist(List *sub_tlist,
					   List *tlist, Node *havingQual,
					   List *grp_clauses, int *pnum_gkeys, AttrNumber **pcols_gkeys, Oid **pcols_gops,
					   List *dqa_args, int *pnum_dqas, AttrNumber **pcols_dqas)
{
	int			nkeys;
	AttrNumber *cols;
	Oid		   *grpops;

	nkeys = num_distcols_in_grouplist(grp_clauses);
	if (nkeys > 0)
	{
		List	   *tles;
		List	   *sortops;
		List	   *eqops;
		ListCell   *lc_tle;
		ListCell   *lc_eqop;
		int			keyno = 0;

		cols = (AttrNumber *) palloc0(sizeof(AttrNumber) * nkeys);
		grpops = (Oid *) palloc0(sizeof(Oid) * nkeys);

		get_sortgroupclauses_tles(grp_clauses, tlist, &tles, &sortops, &eqops);

		forboth (lc_tle, tles, lc_eqop, eqops)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(lc_tle);
			TargetEntry *sub_tle;

			sub_tle = tlist_member((Node *) tle->expr, sub_tlist);
			Assert(tle->ressortgroupref != 0);
			Assert(tle->ressortgroupref == sub_tle->ressortgroupref);
			Assert(keyno < nkeys);

			cols[keyno] = sub_tle->resno;

			grpops[keyno] = lfirst_oid(lc_eqop);
			if (!OidIsValid(grpops[keyno]))          /* shouldn't happen */
				elog(ERROR, "could not find equality operator for ordering operator %u",
					 grpops[keyno]);
			keyno++;
		}
		Assert(nkeys == keyno);
		*pnum_gkeys = nkeys;
		*pcols_gkeys = cols;
		*pcols_gops = grpops;
	}
	else
	{
		*pnum_gkeys = 0;
		*pcols_gkeys = NULL;
		*pcols_gops = NULL;
	}

	if (dqa_args != NIL)
		sub_tlist = augment_subplan_tlist(sub_tlist, dqa_args, pnum_dqas, pcols_dqas, true);

	return sub_tlist;
}

/*
 * Generate targetlist for a SubqueryScan node to wrap the stage-one
 * Agg node (partial aggregation) of a 2-Stage aggregation sequence.
 *
 * varno: varno to use in generated Vars
 * input_tlist: targetlist of this node's input node
 *
 * Result is a "flat" (all simple Var node) targetlist in which
 * varattno and resno match and are sequential.
 *
 * This function also returns a map between the original targetlist
 * entry to new target list entry using resno values. The index
 * positions for resno_map represent the original resnos, while the
 * array elements represent the new resnos. The array is allocated
 * by the caller, which should have length of list_length(input_tlist).
 */
List *
generate_subquery_tlist(Index varno, List *input_tlist,
						bool keep_resjunk, int **p_resno_map)
{
	List	   *tlist = NIL;
	int			resno = 1;
	ListCell   *j;
	TargetEntry *tle;
	Node	   *expr;

	*p_resno_map = (int *) palloc0(list_length(input_tlist) * sizeof(int));

	foreach(j, input_tlist)
	{
		TargetEntry *inputtle = (TargetEntry *) lfirst(j);

		Assert(inputtle->resno == resno && inputtle->resno >= 1);

		/* Don't pull up constants, always use a Var to reference the input. */
		expr = (Node *) makeVar(varno,
								inputtle->resno,
								exprType((Node *) inputtle->expr),
								exprTypmod((Node *) inputtle->expr),
								0);

		(*p_resno_map)[inputtle->resno - 1] = resno;

		tle = makeTargetEntry((Expr *) expr,
							  (AttrNumber) resno++,
							  (inputtle->resname == NULL) ?
							  NULL :
							  pstrdup(inputtle->resname),
							  keep_resjunk ? inputtle->resjunk : false);
		tle->ressortgroupref = inputtle->ressortgroupref;
		tlist = lappend(tlist, tle);
	}

	return tlist;
}


/*
 * Function: cdbpathlocus_collocates
 *
 * Is a relation with the given locus guaranteed to collocate tuples with
 * non-distinct values of the key.  The key is a list of PathKeys.
 *
 * Collocation is guaranteed if the locus specifies a single process or
 * if the result is partitioned on a subset of the keys that must be
 * collocated.
 *
 * We ignore other sorts of collocation, e.g., replication or partitioning
 * on a range since these cannot occur at the moment (MPP 2.3).
 */
bool
cdbpathlocus_collocates(PlannerInfo *root, CdbPathLocus locus, List *pathkeys,
						bool exact_match)
{
	ListCell   *i;
	List	   *pk_eclasses;

	if (CdbPathLocus_IsBottleneck(locus))
		return true;

	if (!CdbPathLocus_IsHashed(locus))
		return false;			/* Or would HashedOJ ok, too? */

	if (exact_match && list_length(pathkeys) != list_length(locus.partkey_h))
		return false;

	/*
	 * Extract a list of eclasses from the pathkeys.
	 */
	pk_eclasses = NIL;
	foreach(i, pathkeys)
	{
		PathKey    *pathkey = (PathKey *) lfirst(i);
		EquivalenceClass *ec;

		ec = pathkey->pk_eclass;
		while (ec->ec_merged != NULL)
			ec = ec->ec_merged;

		pk_eclasses = lappend(pk_eclasses, ec);
	}

	/*
	 * Check for containment of locus in pk_eclasses.
	 *
	 * We ignore constants in the locus hash key. A constant has the same
	 * value everywhere, so it doesn't affect collocation.
	 */
	return cdbpathlocus_is_hashed_on_eclasses(locus, pk_eclasses, true);
}


/*
 * Function: cdbpathlocus_from_flow
 *
 * Generate a locus from a flow.  Since the information needed to produce
 * canonical path keys is unavailable, this function will never return a
 * hashed locus.
 */
CdbPathLocus
cdbpathlocus_from_flow(Flow *flow)
{
	CdbPathLocus locus;

	CdbPathLocus_MakeNull(&locus);

	if (!flow)
		return locus;

	switch (flow->flotype)
	{
		case FLOW_SINGLETON:
			if (flow->segindex == -1)
				CdbPathLocus_MakeEntry(&locus);
			else
				CdbPathLocus_MakeSingleQE(&locus);
			break;
		case FLOW_REPLICATED:
			CdbPathLocus_MakeReplicated(&locus);
			break;
		case FLOW_PARTITIONED:
			CdbPathLocus_MakeStrewn(&locus);
			break;
		case FLOW_UNDEFINED:
		default:
			Insist(0);
	}
	return locus;
}

/*
 * Generate 3 target lists for a sequence of consecutive Agg nodes.
 *
 * This is intended for a sequence of consecutive Agg nodes used in
 * a ROLLUP. '*p_tlist3' is for the upper Agg node, and '*p_tlist2' is
 * for any Agg node in the middle, and '*p_tlist1' is for the
 * bottom Agg node.
 *
 * '*p_tlist1' and '*p_tlist2' have similar target lists. '*p_tlist3'
 * is constructed based on tlist and outputs from *p_tlist2 or
 * '*p_tlist1' if twostage is true.
 *
 * NB This function is called externally (from plangroupext.c) and not
 * used in this file!  Beware: the API is now legacy here!
 */
void
generate_three_tlists(List *tlist,
					  bool twostage,
					  List *sub_tlist,
					  Node *havingQual,
					  int numGroupCols,
					  AttrNumber *groupColIdx,
					  Oid *groupOperators,
					  List **p_tlist1,
					  List **p_tlist2,
					  List **p_tlist3,
					  List **p_final_qual)
{
	ListCell   *lc;
	int			resno = 1;

	MppGroupContext ctx;		/* Just for API matching! */

	/*
	 * Similar to the final tlist entries in two-stage aggregation, we use
	 * consistent varno in the middle tlist entries.
	 */
	int			middle_varno = 1;

	/*
	 * Generate the top and bottom tlists by calling the multi-phase
	 * aggregation code in cdbgroup.c.
	 */
	ctx.tlist = tlist;
	ctx.sub_tlist = sub_tlist;
	ctx.havingQual = havingQual;
	ctx.numGroupCols = numGroupCols;
	ctx.groupColIdx = groupColIdx;
	ctx.groupOperators = groupOperators;
	ctx.numDistinctCols = 0;
	ctx.distinctColIdx = NULL;

	generate_multi_stage_tlists(&ctx,
								p_tlist1,
								NULL,
								p_tlist3,
								p_final_qual);

	/*
	 * Read target entries in '*p_tlist1' one by one, and construct the
	 * entries for '*p_tlist2'.
	 */
	foreach(lc, *p_tlist1)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);
		Expr	   *new_expr;
		TargetEntry *new_tle;

		if (IsA(tle->expr, Aggref))
		{
			Aggref	   *aggref = (Aggref *) tle->expr;
			Aggref	   *new_aggref = makeNode(Aggref);

			new_aggref->aggfnoid = aggref->aggfnoid;
			new_aggref->aggtype = aggref->aggtype;
			new_aggref->args =
				list_make1((Expr *)
						   makeTargetEntry((Expr *) makeVar(middle_varno, tle->resno, aggref->aggtype, -1, 0), 1, NULL, false));
			/* FILTER is evaluated at the PARTIAL stage. */
			new_aggref->agglevelsup = 0;
			new_aggref->aggstar = false;
			new_aggref->aggkind = aggref->aggkind;
			new_aggref->aggdistinct = NIL; /* handled in preliminary aggregation */
			new_aggref->aggstage = AGGSTAGE_INTERMEDIATE;
			new_aggref->location = -1;

			new_expr = (Expr *) new_aggref;
		}

		else
		{
			/* Just make a new Var. */
			new_expr = (Expr *) makeVar(middle_varno,
										tle->resno,
										exprType((Node *) tle->expr),
										exprTypmod((Node *) tle->expr),
										0);

		}

		new_tle = makeTargetEntry(new_expr, resno,
								  (tle->resname == NULL) ?
								  NULL :
								  pstrdup(tle->resname),
								  false);
		new_tle->ressortgroupref = tle->ressortgroupref;
		*p_tlist2 = lappend(*p_tlist2, new_tle);

		resno++;
	}

	/*
	 * This may be called inside a two-stage aggregation. In this case, We
	 * want to make sure all entries in the '*p_tlist3' are visible.
	 */
	foreach(lc, *p_tlist3)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);

		if (twostage)
			tle->resjunk = false;

		/*
		 * We also set aggstage to AGGSTAGE_INTERMEDIATE if this is in a
		 * two-stage aggregation, because the agg node in the second stage
		 * aggregation will do the finalize.
		 */
		if (twostage && IsA(tle->expr, Aggref))
		{
			Aggref	   *aggref = (Aggref *) tle->expr;

			aggref->aggstage = AGGSTAGE_INTERMEDIATE;
		}
	}
}


/*
 * Function: generate_multi_stage_tlists
 *
 * Generate target lists and having qual for multi-stage aggregation.
 *
 * Input is
 *
 * From the ctx argument:
 *	 tlist - the preprocessed target list of the original query
 *	 sub_tlist - the reduced target list to use as input to the aggregation
 *               (If use_dqa_pruning, the all DQA arguments must appear in
 *               this list and must have non-zero sortgrouprefs.)
 *	 havingQual - the preprocesses having qual of the originaly query
 *                (in list-of-conjunct-Exprs form)
 *	 numGroupCols - number of grouping attributes (no grouping extensions)
 *	 groupColIdx - resnos (= attr numbers) of the grouping attributes
 *	 numDistinctCols - number of DISTINCT-qualified argument attributes
 *	 distinctColIdx - resnos (= attr numbers) of the DQA argument attributes
 *
 * From use_dqa_pruning:
 *   Do we want to construct the tlists to support pruning DQAs?
 *
 * Output is pointers to
 *
 *	prelim_tlist - the target list of the preliminary Agg node.
 *  inter_tlist - an optional intermediate target list for an Agg node
 *                used in multi-phase DQA pruning (p_inter_tlist non-null).
 *	final_tlist - the target list of the final Agg node.
 *	final_qual - the qual of the final Agg node.
 */
void
generate_multi_stage_tlists(MppGroupContext *ctx,
							List **p_prelim_tlist,
							List **p_inter_tlist,
							List **p_final_tlist,
							List **p_final_qual)
{
	/*
	 * Use consistent varno in final and intermediate tlist entries.  It will
	 * refer to the sole RTE (a Subquery RTE) of a SubqueryScan.
	 */
	ctx->final_varno = 1;

	/* Do we need to build an intermediate tlist in irefs_tlist? */
	ctx->use_irefs_tlist = (p_inter_tlist != NULL);

	/* Don't do DQA pruning.  Use prepare/generate_dqa_pruning_tlists! */
	ctx->use_dqa_pruning = false;

	deconstruct_agg_info(ctx);
	reconstruct_agg_info(ctx,
						 p_prelim_tlist, p_inter_tlist,
						 p_final_tlist, p_final_qual);
}


/*
 * Function: prepare_dqa_pruning_tlists
 *
 * Performs the first phase of generate_multi_phase_tlist, but waits for
 * a subquent call to generate_dqa_pruning_tlists to actually produce the
 * target list.  (This allows for the fact that DQA pruning may involve
 * several "coplans" each with its own target list requirements.  This
 * function lays the groundwork for all such target lists.
 */
void
prepare_dqa_pruning_tlists(MppGroupContext *ctx)
{
	/*
	 * Use consistent varno in final and intermediate tlist entries.  It will
	 * refer to the sole RTE (a Subquery RTE) of a SubqueryScan.
	 */
	ctx->final_varno = 1;

	/* Do we need to build an intermediate tlist in irefs_tlist? */
	ctx->use_irefs_tlist = true;

	/*
	 * Do we want to do DQA pruning (in case there are any DISTINCT-qualified
	 * aggregate functions)?
	 */
	ctx->use_dqa_pruning = true;

	deconstruct_agg_info(ctx);
}

/*
 * Function: generate_dqa_pruning_tlists
 *
 * Performs the last phase of generate_multi_phase_tlist in the context of
 * DQA pruning.
 */
void
generate_dqa_pruning_tlists(MppGroupContext *ctx,
							int dqa_index,
							List **p_prelim_tlist,
							List **p_inter_tlist,
							List **p_final_tlist,
							List **p_final_qual)
{
	Assert(p_inter_tlist != NULL);	/* optional elsewhere, required here. */
	Assert(ctx->use_dqa_pruning);

	if (ctx->numDistinctCols == 1)
	{
		/* Finalized results for single-DQA (join-less) plan. */
		reconstruct_agg_info(ctx,
							 p_prelim_tlist,
							 p_inter_tlist,
							 p_final_tlist,
							 p_final_qual);
	}
	else
	{
		/* Minimal results for multi-DQA (join) plan. */
		reconstruct_coplan_info(ctx,
								dqa_index,
								p_prelim_tlist,
								p_inter_tlist,
								p_final_tlist);
		*p_final_qual = NIL;
	}
}

/* Function: deconstruct_agg_info
 *
 * Top-level deconstruction of the target list and having qual of an
 * aggregate Query into intermediate structures that will later guide
 * reconstruction of the various target lists and expressions involved
 * in a multi-phase aggregation plan, possibly with DISTINCT-qualified
 * aggregate functions (DQAs).
 */
void
deconstruct_agg_info(MppGroupContext *ctx)
{
	int			i;
	ListCell   *lc;

	/*
	 * Initialize temporaries to hold the parts of the preliminary target list
	 * under construction.
	 */
	ctx->grps_tlist = NIL;
	ctx->dqa_tlist = NIL;
	ctx->prefs_tlist = NIL;
	ctx->irefs_tlist = NIL;
	ctx->frefs_tlist = NIL;
	ctx->dref_tlists = NULL;
	ctx->fin_tlist = NIL;
	ctx->fin_hqual = NIL;

	/*
	 * Begin constructing the target list for the preliminary Agg node by
	 * placing targets for the grouping attributes on the grps_tlist
	 * temporary. Make sure ressortgroupref matches the original. Copying the
	 * expression may be overkill, but it is safe.
	 */
	for (i = 0; i < ctx->numGroupCols; i++)
	{
		TargetEntry *sub_tle,
				   *prelim_tle;

		sub_tle = get_tle_by_resno(ctx->sub_tlist, ctx->groupColIdx[i]);
		prelim_tle = makeTargetEntry(copyObject(sub_tle->expr),
									 list_length(ctx->grps_tlist) + 1,
									 (sub_tle->resname == NULL) ?
									 NULL :
									 pstrdup(sub_tle->resname),
									 false);
		prelim_tle->ressortgroupref = sub_tle->ressortgroupref;
		prelim_tle->resjunk = false;
		ctx->grps_tlist = lappend(ctx->grps_tlist, prelim_tle);
	}

	/*
	 * Continue to construct the target list for the preliminary Agg node by
	 * placing targets for the argument attribute of each DQA on the dqa_tlist
	 * temporary. Make sure ressortgroupref matches the original.
	 */
	for (i = 0; i < ctx->numDistinctCols; i++)
	{
		TargetEntry *sub_tle,
				   *prelim_tle;

		sub_tle = get_tle_by_resno(ctx->sub_tlist, ctx->distinctColIdx[i]);
		prelim_tle = makeTargetEntry(copyObject(sub_tle->expr),
									 list_length(ctx->dqa_tlist) + 1,
									 (sub_tle->resname == NULL) ?
									 NULL :
									 pstrdup(sub_tle->resname),
									 false);
		prelim_tle->ressortgroupref = sub_tle->ressortgroupref;
		prelim_tle->resjunk = false;
		ctx->dqa_tlist = lappend(ctx->dqa_tlist, prelim_tle);
	}

	/*
	 * Initialize the array of Aggref target lists corresponding to the DQA
	 * argument target list just constructed.
	 */
	ctx->dref_tlists = (List **) palloc0(ctx->numDistinctCols * sizeof(List *));

	/*
	 * Derive the final target list with entries corresponding to the input
	 * target list, but referring to the attributes of the preliminary target
	 * list rather than to the input attributes.  Note that this involves
	 * augmenting the prefs_tlist temporary as we encounter new Aggref nodes.
	 */
	foreach(lc, ctx->tlist)
	{
		TargetEntry *tle,
				   *final_tle;
		Expr	   *expr;

		tle = (TargetEntry *) lfirst(lc);
		ctx->split_aggref_sortgroupref = tle->ressortgroupref;	/* for deconstruction
																 * subroutines */
		expr = deconstruct_expr(tle->expr, ctx);
		ctx->split_aggref_sortgroupref = 0;
		final_tle = makeTargetEntry(expr,
									tle->resno,
									(tle->resname == NULL) ?
									NULL :
									pstrdup(tle->resname),
									tle->resjunk);
		final_tle->ressortgroupref = tle->ressortgroupref;
		ctx->fin_tlist = lappend(ctx->fin_tlist, final_tle);
	}

	/*
	 * Derive the final qual while augmenting the preliminary target list.
	 */
	ctx->fin_hqual = (List *) deconstruct_expr((Expr *) ctx->havingQual, ctx);


	/* Now cache some values to avoid repeated recalculation by subroutines. */

	/*
	 * Use consistent varno in final, intermediate an join tlist entries.
	 * final refers to the sole RTE (a Subquery RTE) of a SubqueryScan. outer
	 * and inner to the respective inputs to a join.
	 */
	ctx->final_varno = 1;
	ctx->outer_varno = OUTER;
	ctx->inner_varno = INNER;

	/*---------------------------------------------------------------------
	 * Target lists used in multi-phase planning at or above the level of
	 * individual DQA coplans have one of the forms
	 *
	 *   [G][D0...Dn][R]
	 * where
	 *   G represents the grouping key attributes (if any)
	 *   Di represents the results of the DQAs that take the i-th
	 *      unique DQA argument (if any)
	 *   R represents the results of regular aggregate functions (if any)
	 *
	 * The offset at index position i is the number of attributes that precede
	 * the first for the i-th DQA (index origin 0). The last is the offset of
	 * the first attribute following the DQA attributes.
	 *---------------------------------------------------------------------
	 */
	ctx->dqa_offsets = palloc(sizeof(int) * (1 + ctx->numDistinctCols));
	ctx->dqa_offsets[0] = ctx->numGroupCols;
	for (i = 0; i < ctx->numDistinctCols; i++)
	{
		ctx->dqa_offsets[i + 1] = ctx->dqa_offsets[i]
			+ list_length(ctx->dref_tlists[i]);
	}
}

/* Function: reconstruct_agg_info
 *
 * Construct the preliminay, optional intermediate, and final target lists
 * and the final having qual for the aggregation plan.  If we are doing
 * DQA pruning, this function is appropriate only for the cases of 0 or 1
 * DQA.
 *
 * During processing we set ctx->top_tlist to be the flat target list
 * containing only the grouping key and the results of individual aggregate
 * functions.  This list is transient -- it drives the production of the
 * final target list and having qual through finalize_split_expression.
 */
void
reconstruct_agg_info(MppGroupContext *ctx,
					 List **p_prelim_tlist,
					 List **p_inter_tlist,
					 List **p_final_tlist,
					 List **p_final_qual)
{
	List	   *prelim_tlist = NIL;
	List	   *inter_tlist = NIL;
	List	   *final_tlist = NIL;

	/* Grouping keys */

	prelim_tlist = ctx->grps_tlist;
	if (p_inter_tlist != NULL)
		inter_tlist = make_vars_tlist(ctx->grps_tlist, ctx->final_varno, 0);
	final_tlist = make_vars_tlist(ctx->grps_tlist, ctx->final_varno, 0);

	/* If applicable, single DQA argument, corresponding DQAs */

	if (ctx->use_dqa_pruning)
	{
		if (list_length(ctx->dqa_tlist) == 1)
		{
			int			n = list_length(prelim_tlist);
			TargetEntry *tle = (TargetEntry *) linitial(ctx->dqa_tlist);

			tle->resno = n + 1;

			prelim_tlist = lappend(prelim_tlist, tle);
			if (p_inter_tlist != NULL)
			{
				inter_tlist = list_concat(inter_tlist,
										  make_vars_tlist(ctx->dqa_tlist,
														  ctx->final_varno, n));
			}
			final_tlist = seq_tlist_concat(final_tlist, ctx->dref_tlists[0]);
		}
		else if (list_length(ctx->dqa_tlist) != 0)
		{
			/* Shouldn't use this function for multi-DQA pruning. */
			elog(ERROR, "Unexpected use of DISTINCT-qualified aggregate pruning");
		}
	}

	/* Aggrefs */

	prelim_tlist = seq_tlist_concat(prelim_tlist, ctx->prefs_tlist);
	if (p_inter_tlist != NULL)
	{
		inter_tlist = seq_tlist_concat(inter_tlist, ctx->irefs_tlist);
	}
	final_tlist = seq_tlist_concat(final_tlist, ctx->frefs_tlist);

	/* Set implicit results */

	*p_prelim_tlist = prelim_tlist;
	if (p_inter_tlist != NULL)
		*p_inter_tlist = inter_tlist;

	ctx->top_tlist = final_tlist;

	*p_final_tlist = (List *) finalize_split_expr((Node *) ctx->fin_tlist, ctx);
	*p_final_qual = (List *) finalize_split_expr((Node *) ctx->fin_hqual, ctx);
}

/* Function: reconstruct_coplan_info
 *
 * Construct the preliminary, intermediate, and final target lists
 * for the DQA pruning aggregation coplan specified by dqa_index.
 *
 * Note: Similar to reconstruct_agg_info but stop short of finalization
 *       and is sensitive to dqa_index.  Ordinarily this function would
 *       be used only for multiple-DQA planning.
 */
void
reconstruct_coplan_info(MppGroupContext *ctx,
						int dqa_index,
						List **p_prelim_tlist,
						List **p_inter_tlist,
						List **p_final_tlist)
{
	List	   *prelim_tlist = NIL;
	List	   *inter_tlist = NIL;
	List	   *final_tlist = NIL;

	int			n;
	TargetEntry *tle;

	/* Grouping keys */

	prelim_tlist = copyObject(ctx->grps_tlist);
	if (p_inter_tlist != NULL)
		inter_tlist = make_vars_tlist(ctx->grps_tlist, ctx->final_varno, 0);
	final_tlist = make_vars_tlist(ctx->grps_tlist, ctx->final_varno, 0);

	/* Single DQA argument, corresponding DQAs */

	Assert(ctx->use_dqa_pruning);

	n = list_length(prelim_tlist);
	tle = (TargetEntry *) list_nth(ctx->dqa_tlist, dqa_index);
	tle->resno = n + 1;

	prelim_tlist = lappend(prelim_tlist, tle);
	if (p_inter_tlist != NULL)
	{
		List	   *x = list_make1(tle);

		inter_tlist = list_concat(inter_tlist,
								  make_vars_tlist(x, ctx->final_varno, n));
		list_free(x);
	}
	final_tlist = seq_tlist_concat(final_tlist, ctx->dref_tlists[dqa_index]);


	/* Plain Aggrefs go only on the first coplan! */
	if (dqa_index == 0)
	{
		prelim_tlist = seq_tlist_concat(prelim_tlist, ctx->prefs_tlist);
		if (p_inter_tlist != NULL)
		{
			inter_tlist = seq_tlist_concat(inter_tlist, ctx->irefs_tlist);
		}
		final_tlist = seq_tlist_concat(final_tlist, ctx->frefs_tlist);
	}

	/* Set implicit results */

	*p_prelim_tlist = prelim_tlist;
	if (p_inter_tlist != NULL)
	{
		*p_inter_tlist = inter_tlist;
	}
	*p_final_tlist = final_tlist;
}


/*
 * Function: deconstruct_expr
 *
 * Prepare an expression for execution within 2-stage aggregation.
 * This involves adding targets as needed to the target list of the
 * first (partial) aggregation and referring to this target list from
 * the modified expression for use in the second (final) aggregation.
 */
Expr *
deconstruct_expr(Expr *expr, MppGroupContext *ctx)
{
	return (Expr *) deconstruct_expr_mutator((Node *) expr, ctx);
}

/*
 * Function: deconstruct_expr_mutator
 *
 * Work for deconstruct_expr.
 */
Node *
deconstruct_expr_mutator(Node *node, MppGroupContext *ctx)
{
	TargetEntry *tle;

	if (node == NULL)
		return NULL;

	if (IsA(node, Aggref))
	{
		Aggref	   *aggref = (Aggref *) node;

		return split_aggref(aggref, ctx);
	}

	/*
	 * If the given expression is a grouping expression, replace it with a Var
	 * node referring to the (lower) preliminary aggregation's target list.
	 *
	 * While building subplan targetlist we flatten (deduplicate) the
	 * targetlist ignoring RelabelType node. Including RelabelType will cause
	 * inconsistent top level target list and final target list for
	 * aggregation plans.
	 */
	tle = tlist_member_ignore_relabel(node, ctx->grps_tlist);
	if (tle != NULL)
	{
		Var *var = makeVar(grp_varno, tle->resno,
							exprType((Node*)tle->expr),
							exprTypmod((Node*)tle->expr), 0);

		/*
		 * If a target list entry is found under a relabeltype node, the newly created
		 * var node should be nested inside the relabeltype node if the vartype
		 * of the var node is different than the resulttype of the relabeltype node.
		 * Otherwise, the cast information is lost and executor will complain type mismatch
		 */
		if (IsA(node, RelabelType) && exprType(node) != exprType((Node*)tle->expr))
		{
			RelabelType *relabeltypeNode = (RelabelType *) node;
			return (Node *) makeRelabelType((Expr *) var,
										relabeltypeNode->resulttype,
										relabeltypeNode->resulttypmod,
										relabeltypeNode->relabelformat);
		}
		return (Node*) var;
	}

	return expression_tree_mutator(node, deconstruct_expr_mutator, (void *) ctx);
}


/*
 * Function: split_aggref
 *
 * Find or add a partial-stage Aggref expression for the argument in the
 * preliminary target list under construction.  Return the final-stage
 * Aggref with a single Var node argument referring to the partial-stage
 * Aggref.  In case of a DQA argument reduction, however, there is no
 * partial-stage Aggref and the final-stage Aggref differs from the original
 * in that (1) it does not specify DISTINCT and (2) it refers to its arguments
 * via Vars on the lower range.
 *
 * For the normal 2-phase case:
 *
 * Note that he result type of the partial-stage Aggref will be the
 * transition type of the aggregate function.
 *
 * At execution, the function used to compute the transition type in the
 * lower aggregation will be the normal accumulator function for the
 * aggregate function.  The final function, however, will not be used.
 * The  result will be the ending transition value.
 *
 * At execution, the function used to compute the transition type in the
 * upper aggregation will be the prelim function (known to exist, else
 * we would have rejected 2-stage aggregation as a strategy) on input
 * values of the transition type.  The normal accumulator function for the
 * aggregate function will not  be used.  The normal final function will
 * be used to convert the ending transition value to the result type.
 * aggregation
 */
Node *
split_aggref(Aggref *aggref, MppGroupContext *ctx)
{
	ListCell   *cell;
	Node	   *final_node;

	Assert(aggref != NULL && aggref->agglevelsup == 0);

	if (aggref->aggdistinct && ctx->use_dqa_pruning)
	{
		Index		arg_attno;
		Index		dqa_attno;
		TargetEntry *dqa_tle = NULL;
		TargetEntry *arg_tle;
		List	   *dref_tlist = NIL;

		/*
		 * First find the DQA argument.  Since this is a DQA, its argument
		 * list must contain a single expression that matches one of the
		 * target expressions in ctx->dqa_tlist.
		 */
		arg_tle = NULL;
		if (list_length(aggref->args) == 1) /* safer than Assert */
		{
			arg_tle = tlist_member(linitial(aggref->args), ctx->dqa_tlist);
		}
		if (arg_tle == NULL)
			elog(ERROR, "Unexpected use of DISTINCT-qualified aggregation");
		arg_attno = arg_tle->resno; /* [1..numDistinctCols] */

		/*
		 * We may have seen a DQA just like this one already.  Look for one in
		 * the distinct Aggref target list to date.
		 */
		dref_tlist = ctx->dref_tlists[arg_attno - 1];
		dqa_attno = 1;
		foreach(cell, dref_tlist)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(cell);
			Aggref	   *ref = (Aggref *) tle->expr;

			/*
			 * Check similarity, avoid aggtype and aggstage (which we control)
			 * and don't bother with agglevelsup (which is always 0 here) or
			 * aggdistinct.
			 */
			if (aggref->aggfnoid == ref->aggfnoid
				&& aggref->aggstar == ref->aggstar
				&& equal(aggref->args, ref->args))
			{
				dqa_tle = tle;
				break;
			}
			dqa_attno++;
		}

		if (dqa_tle == NULL)
		{
			/*
			 * Didn't find a target for the DQA Aggref so make a new one.
			 */
			Var		   *arg_var;
			Aggref	   *dqa_aggref;

			arg_var = makeVar(ctx->final_varno, ctx->numGroupCols + 1,
							  exprType(linitial(aggref->args)),
							  exprTypmod(linitial(aggref->args)),
							  0);

			dqa_aggref = makeNode(Aggref);
			memcpy(dqa_aggref, aggref, sizeof(Aggref)); /* flat copy */
			dqa_aggref->args = list_make1(arg_var);
			dqa_aggref->aggdistinct = NIL;

			dqa_tle = makeTargetEntry((Expr*)dqa_aggref, dqa_attno, NULL, false);
			dref_tlist = lappend(dref_tlist, dqa_tle);
		}
		ctx->dref_tlists[arg_attno - 1] = dref_tlist;

		/*
		 * Make the "final" target for the DQA case, a reference to the DQA
		 * Aggref we just found or constructed.
		 */
		final_node = (Node *) makeVar(dqa_base_varno + arg_attno - 1,
									  dqa_attno,
									  exprType((Node *) arg_tle->expr),
									  exprTypmod((Node *) arg_tle->expr),
									  0);
	}
	else						/* Ordinary Aggref -or- DQA but
								 * ctx->use_dqa_pruning is off. */
	{
		Aggref	   *pref;
		Aggref	   *iref;
		Aggref	   *fref;
		Oid			transtype = InvalidOid;
		AttrNumber	attrno;
		TargetEntry *prelim_tle = NULL;

		/*
		 * We may have seen an Aggref just like this one already.  Look for
		 * the preliminary form of such in the preliminary Aggref target list
		 * to date.
		 */
		foreach(cell, ctx->prefs_tlist)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(cell);
			Aggref	   *ref = (Aggref *) tle->expr;

			/*
			 * Check similarity, avoid aggtype and aggstage (which we control)
			 * and don't bother with agglevelsup (which is always 0 here).
			 */
			if (aggref->aggfnoid == ref->aggfnoid
				&& aggref->aggstar == ref->aggstar
				&& equal(aggref->aggdistinct, ref->aggdistinct)
				&& equal(aggref->args, ref->args)
				&& equal(aggref->aggfilter, ref->aggfilter))
			{
				prelim_tle = tle;
				transtype = ref->aggtype;
				attrno = prelim_tle->resno;
				break;
			}
		}

		/*
		 * If no existing preliminary Aggref target matched, add one that
		 * does.
		 */
		if (prelim_tle == NULL)
		{
			TargetEntry *final_tle;
			Var		   *args;
			Oid			inputTypes[FUNC_MAX_ARGS];
			int			nargs;

			/* Get type information for the Aggref */
			transtype = lookup_agg_transtype(aggref);
			nargs = get_aggregate_argtypes(aggref, inputTypes);
			transtype = resolve_aggregate_transtype(aggref->aggfnoid,
													transtype,
													inputTypes,
													nargs);

			/*
			 * Make a new preliminary Aggref wrapped as a new target entry.
			 * Like the input Aggref, the preliminary refers to the lower
			 * range.
			 */
			pref = (Aggref *) copyObject(aggref);
			pref->aggtype = transtype;
			pref->aggstage = AGGSTAGE_PARTIAL;

			attrno = 1 + list_length(ctx->prefs_tlist);
			prelim_tle = makeTargetEntry((Expr *) pref, attrno, NULL, false);
			prelim_tle->ressortgroupref = ctx->split_aggref_sortgroupref;
			ctx->prefs_tlist = lappend(ctx->prefs_tlist, prelim_tle);

			args = makeVar(ctx->final_varno,
						   ctx->numGroupCols
						   + (ctx->use_dqa_pruning ? 1 : 0)
						   + attrno,
						   transtype, -1, 0);

			if (ctx->use_irefs_tlist)
			{
				TargetEntry *inter_tle;

				iref = makeNode(Aggref);
				iref->aggfnoid = pref->aggfnoid;
				iref->aggtype = transtype;
				iref->args = list_make1((Expr *) makeTargetEntry(copyObject(args), 1, NULL, false));
				/* FILTER is evaluated at the PARTIAL stage. */
				iref->agglevelsup = 0;
				iref->aggstar = false;
				iref->aggkind = aggref->aggkind;
				iref->aggdistinct = NIL;
				iref->aggstage = AGGSTAGE_INTERMEDIATE;
				iref->location = -1;

				inter_tle = makeTargetEntry((Expr *) iref, attrno, NULL, false);
				inter_tle->ressortgroupref = ctx->split_aggref_sortgroupref;
				ctx->irefs_tlist = lappend(ctx->irefs_tlist, inter_tle);
			}

			/* Make a new final Aggref. */
			fref = makeNode(Aggref);

			fref->aggfnoid = aggref->aggfnoid;
			fref->aggtype = aggref->aggtype;
			fref->args = list_make1((Expr *) makeTargetEntry((Expr *) args, 1, NULL, false));
			/* FILTER is evaluated at the PARTIAL stage. */
			fref->agglevelsup = 0;
			fref->aggstar = false;
			fref->aggkind = aggref->aggkind;
			fref->aggdistinct = aggref->aggdistinct;
			fref->aggstage = AGGSTAGE_FINAL;
			fref->location = -1;
			final_tle = makeTargetEntry((Expr *) fref, attrno, NULL, false);
			final_tle->ressortgroupref = ctx->split_aggref_sortgroupref;
			ctx->frefs_tlist = lappend(ctx->frefs_tlist, final_tle);
		}

		final_node = (Node *) makeVar(ref_varno, attrno, aggref->aggtype, -1, 0);
	}

	return final_node;
}


/* Function: make_vars_tlist
 *
 * Make a targetlist similar to the given length n tlist but consisting of
 * simple Var nodes with the given varno and varattno in offset + [1..N].
 */
List *
make_vars_tlist(List *tlist, Index varno, AttrNumber offset)
{
	List	   *new_tlist = NIL;
	AttrNumber	attno = offset;
	ListCell   *lc;

	foreach(lc, tlist)
	{
		Var		   *new_var;
		TargetEntry *new_tle;

		TargetEntry *tle = (TargetEntry *) lfirst(lc);

		attno++;

		new_var = makeVar(varno, attno,
						  exprType((Node *) tle->expr),
						  exprTypmod((Node *) tle->expr), 0);

		new_tle = makeTargetEntry((Expr *) new_var,
								  attno,	/* resno always matches attnr */
								  (tle->resname == NULL) ? NULL : pstrdup(tle->resname),
								  false);
		new_tle->ressortgroupref = tle->ressortgroupref;

		new_tlist = lappend(new_tlist, new_tle);
	}
	return new_tlist;
}

/* Function: seq_tlist_concat
 *
 * Concatenates tlist2 to the end of tlist1 adjusting the resno values
 * of tlist2 so that the resulting entries have resno = position+1.
 * The resno values of tlist1 must be dense from 1 to the length of
 * the list.  (They are sequential by position, though this is not
 * strictly required.
 *
 * May modify tlist1 in place (to adjust last link and length).  Does not
 * modify tlist2, but the result shares structure below the TargetEntry
 * nodes.
 */
List *
seq_tlist_concat(List *tlist1, List *tlist2)
{
	ListCell   *lc;
	AttrNumber	high_attno = list_length(tlist1);

	foreach(lc, tlist2)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);
		TargetEntry *new_tle = (TargetEntry *) makeNode(TargetEntry);

		memcpy(new_tle, tle, sizeof(TargetEntry));
		new_tle->resno = ++high_attno;
		tlist1 = lappend(tlist1, new_tle);
	}
	return tlist1;
}

/* Function finalize_split_expr
 *
 * Note: Only called on the top of the "join" tree, so all D_i are
 *       included in attribute offset calculations.
 */
Node *
finalize_split_expr(Node *expr, MppGroupContext *ctx)
{
	return finalize_split_expr_mutator(expr, ctx);
}

/* Mutator subroutine for finalize_split_expr() replaces pseudo Var nodes
 * produced by split_aggref() with the similarly typed expression found in
 * the top-level targetlist, ctx->top_tlist,  being finalized.
 *
 * For example, a pseudo Var node that represents the 3rd DQA for the
 * 2nd DQA argument will be replaced by the targetlist expression that
 * corresponds to that DQA.
 */
Node *
finalize_split_expr_mutator(Node *node, MppGroupContext *ctx)
{
	if (node == NULL)
		return NULL;

	if (IsA(node, Var))
	{
		AttrNumber	attrno = (AttrNumber) 0;
		TargetEntry *tle;

		Var		   *pseudoVar = (Var *) node;

		if (pseudoVar->varno == grp_varno)
		{
			attrno = pseudoVar->varattno;
		}
		else if (pseudoVar->varno == ref_varno)
		{
			if (ctx->use_dqa_pruning)
			{
				attrno = ctx->dqa_offsets[ctx->numDistinctCols]
					+ pseudoVar->varattno;
			}
			else
			{
				attrno = ctx->numGroupCols + pseudoVar->varattno;
			}
		}
		else if (pseudoVar->varno >= dqa_base_varno && ctx->use_dqa_pruning)
		{
			int			i = pseudoVar->varno - dqa_base_varno;

			attrno = ctx->dqa_offsets[i] + pseudoVar->varattno;
		}
		else
		{
			elog(ERROR, "Unexpected failure of multi-phase aggregation planning");
		}

		tle = (TargetEntry *) list_nth(ctx->top_tlist, attrno - 1);

		return (Node *) tle->expr;
	}

	return expression_tree_mutator(node,
								   finalize_split_expr_mutator,
								   (void *) ctx);
}


/* Function lookup_agg_transtype
 *
 * Return the transition type Oid of the given aggregate fuction or throw
 * an error, if none.
 */
Oid
lookup_agg_transtype(Aggref *aggref)
{
	Oid			aggid = aggref->aggfnoid;
	Oid			result;
	HeapTuple	tuple;

	/* XXX: would have been get_agg_transtype() */
	tuple = SearchSysCache1(AGGFNOID, ObjectIdGetDatum(aggid));
	if (!tuple)
		elog(ERROR, "cache lookup failed for aggregate %u", aggid);

	result = ((Form_pg_aggregate) GETSTRUCT(tuple))->aggtranstype;

	ReleaseSysCache(tuple);

	return result;
}

/**
 * Update the scatter clause before a query tree's targetlist is about to
 * be modified. The scatter clause of a query tree will correspond to
 * old targetlist entries. If the query tree is modified and the targetlist
 * is to be modified, we must call this method to ensure that the scatter clause
 * is kept in sync with the new targetlist.
 */
void
UpdateScatterClause(Query *query, List *newtlist)
{
	Assert(query);
	Assert(query->targetList);
	Assert(newtlist);

	if (query->scatterClause
		&& list_nth(query->scatterClause, 0) != NULL	/* scattered randomly */
		)
	{
		Assert(list_length(query->targetList) == list_length(newtlist));
		List	   *scatterClause = NIL;
		ListCell   *lc = NULL;

		foreach(lc, query->scatterClause)
		{
			Expr	   *o = (Expr *) lfirst(lc);

			Assert(o);
			TargetEntry *tle = tlist_member((Node *) o, query->targetList);

			Assert(tle);
			TargetEntry *ntle = list_nth(newtlist, tle->resno - 1);

			scatterClause = lappend(scatterClause, copyObject(ntle->expr));
		}
		query->scatterClause = scatterClause;
	}
}

/*
 * Function: add_second_stage_agg
 *
 * Add an Agg/Group node on top of an Agg/Group node. We add a SubqueryScan
 * node on top of the existing Agg/Group node before adding the new Agg/Group
 * node.
 *
 * Params:
 *  is_agg -- indicate to add an Agg or a Group node.
 *  prelim_tlist -- the targetlist for the existing Agg/Group node.
 *  final_tlist -- the targetlist for the new Agg/Group node.
 */
Plan *
add_second_stage_agg(PlannerInfo *root,
					 bool is_agg,
					 List *lower_tlist,
					 List *upper_tlist,
					 List *upper_qual,
					 AggStrategy aggstrategy,
					 int numGroupCols,
					 AttrNumber *prelimGroupColIdx,
					 Oid *prelimGroupOperators,
					 int num_nullcols,
					 uint64 input_grouping,
					 uint64 grouping,
					 int rollup_gs_times,
					 double numGroups,
					 int numAggs,
					 int transSpace,
					 const char *alias,
					 List **p_current_pathkeys,
					 Plan *result_plan,
					 bool use_root,
					 bool adjust_scatter)
{
	Query	   *parse = root->parse;
	Query	   *subquery;
	List	   *newrtable;
	RangeTblEntry *newrte;
	RangeTblRef *newrtref;
	Plan	   *agg_node;

	/*
	 * Add a SubqueryScan node to renumber the range of the query.
	 *
	 * The result of the preliminary aggregation (represented by lower_tlist)
	 * may contain targets with no representatives in the range of its outer
	 * relation.  We resolve this by treating the preliminary aggregation as a
	 * subquery.
	 *
	 * However, this breaks the correspondence between the Plan tree and the
	 * Query tree that is assumed by the later call to set_plan_references as
	 * well as by the deparse processing used (e.g.) in EXPLAIN.
	 *
	 * So we also push the Query node from the root structure down into a new
	 * subquery RTE and scribble over the original Query node to make it into
	 * a simple SELECT * FROM a Subquery RTE.
	 *
	 * Note that the Agg phase we add below will refer to the attributes of
	 * the result of this new SubqueryScan plan node.  It is up to the caller
	 * to set up upper_tlist and upper_qual accordingly.
	 */

	/*
	 * Flat-copy the root query into a newly allocated Query node and adjust
	 * its target list and having qual to match the lower (existing) Agg plan
	 * we're about to make into a SubqueryScan.
	 */
	subquery = copyObject(parse);

	subquery->targetList = copyObject(lower_tlist);
	subquery->havingQual = NULL;

	/*
	 * Subquery attributes shouldn't be marked as junk, else they'll be
	 * skipped by addRangeTableEntryForSubquery.
	 */
	{
		ListCell   *cell;

		foreach(cell, subquery->targetList)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(cell);

			tle->resjunk = false;
			if (tle->resname == NULL)
			{
				if (use_root && IsA(tle->expr, Var))
				{
					Var		   *var = (Var *) tle->expr;
					RangeTblEntry *rte = rt_fetch(var->varno, root->parse->rtable);

					tle->resname = pstrdup(get_rte_attribute_name(rte, var->varattno));
				}
				else
				{
					const char *fmt = "unnamed_attr_%d";
					char		buf[32];	/* big enough for fmt */

					sprintf(buf, fmt, tle->resno);
					tle->resname = pstrdup(buf);
				}
			}
		}
	}

	/*
	 * Ensure that the plan we're going to attach to the subquery scan has all
	 * the parameter fields figured out.
	 */
	SS_finalize_plan(root, result_plan, false);

	/* Construct a range table entry referring to it. */
	newrte = addRangeTableEntryForSubquery(NULL,
										   subquery,
										   makeAlias(alias, NULL),
										   TRUE);
	newrtable = list_make1(newrte);

	/*
	 * Modify the root query in place to look like its range table is a simple
	 * Subquery.
	 */
	parse->querySource = QSRC_PLANNER;	/* but remember it's really ours  */
	parse->rtable = newrtable;
	parse->jointree = makeNode(FromExpr);
	newrtref = makeNode(RangeTblRef);
	newrtref->rtindex = 1;
	parse->jointree->fromlist = list_make1(newrtref);
	parse->jointree->quals = NULL;
	parse->rowMarks = NIL;

	/*
	 * <EXECUTE s> uses parse->targetList to derive the portal's tupDesc, so
	 * when use_root is true, the caller owns the responsibility to make sure
	 * it ends up in an appropriate form at the end of planning.
	 */
	if (use_root)
	{
		if (adjust_scatter)
		{
			UpdateScatterClause(parse, upper_tlist);
		}
		parse->targetList = copyObject(upper_tlist);	/* Match range. */
	}

	result_plan = add_subqueryscan(root, p_current_pathkeys,
								   1, subquery, result_plan);

	/* Add an Agg node */
	/* convert current_numGroups to long int */
	long		lNumGroups = (long) Min(numGroups, (double) LONG_MAX);

	agg_node = (Plan *) make_agg(root,
								 upper_tlist,
								 upper_qual,
								 aggstrategy, false,
								 numGroupCols,
								 prelimGroupColIdx,
								 prelimGroupOperators,
								 lNumGroups,
								 num_nullcols,
								 input_grouping,
								 grouping,
								 rollup_gs_times,
								 numAggs,
								 transSpace,
								 result_plan);

	/*
	 * Agg will not change the sort order unless it is hashed.
	 */
	agg_node->flow = pull_up_Flow(agg_node, agg_node->lefttree);

	/*
	 * Since the rtable has changed, we had better recreate a RelOptInfo entry
	 * for it. Make a copy of the groupClause since freeing the arrays can
	 * pull out references still in use from underneath it.
	 */
	root->parse->groupClause = copyObject(root->parse->groupClause);

	if (root->simple_rel_array)
		pfree(root->simple_rel_array);
	if (root->simple_rte_array)
		pfree(root->simple_rte_array);

	rebuild_simple_rel_and_rte(root);

	return agg_node;
}


/*
 * Add a SubqueryScan node to the input plan and maintain the given
 * pathkeys by making adjustments to them and to the equivalence class
 * information in root.
 *
 * Note that submerging a plan into a subquery scan will require changes
 * to the range table and to any expressions above the new scan node.
 * This is the caller's responsibility since the nature of the changes
 * depends on the context in which the subquery is used.
 */
Plan *
add_subqueryscan(PlannerInfo *root, List **p_pathkeys,
				 Index varno, Query *subquery, Plan *subplan)
{
	List	   *subplan_tlist;
	int		   *resno_map;

	subplan_tlist = generate_subquery_tlist(varno, subquery->targetList,
											false, &resno_map);

	subplan = (Plan *) make_subqueryscan(root, subplan_tlist,
										 NIL,
										 varno, /* scanrelid (= varno) */
										 subplan,
										 subquery->rtable);

	mark_passthru_locus(subplan, true, true);

	if (p_pathkeys != NULL)
	{
		*p_pathkeys = reconstruct_pathkeys(root, *p_pathkeys, resno_map,
										   subquery->targetList, subplan_tlist);
	}

	pfree(resno_map);

	return subplan;
}


/*
 * hash_safe_type - is the given type hashable?
 *
 * Modelled on function hash_safe_grouping in planner.c, we assume
 * the type is hashable if its equality operator is marked hashjoinable.
 */
static bool
hash_safe_type(Oid type)
{
	Oid			eq_opr;

	get_sort_group_operators(type,
							 false, false, false,
							 NULL, &eq_opr, NULL);

	if (!OidIsValid(eq_opr))
		return false;

	return op_hashjoinable(eq_opr);
}

/*
 * sorting_prefixes_grouping - is the result ordered on a grouping key prefix?
 *
 * If so, then we might prefer a pre-ordered grouping result to one that would
 * need sorting after the fact.
 */
static bool
sorting_prefixes_grouping(PlannerInfo *root)
{
	return root->sort_pathkeys != NIL
		&& pathkeys_contained_in(root->sort_pathkeys, root->group_pathkeys);
}

/*
 * gp_hash_safe_grouping - are grouping operators GP hashable for
 * redistribution motion nodes?
 */
static bool
gp_hash_safe_grouping(PlannerInfo *root)
{
	List	   *grouptles;
	List	   *sortops;
	List	   *eqops;
	ListCell   *glc;

	get_sortgroupclauses_tles(root->parse->groupClause,
							  root->parse->targetList,
							  &grouptles, &sortops, &eqops);
	foreach(glc, grouptles)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(glc);
		bool		canhash;

		canhash = isGreenplumDbHashable(exprType((Node *) tle->expr));
		if (!canhash)
			return false;
	}
	return true;
}

/*
 * reconstruct_pathkeys
 *
 * Reconstruct the given pathkeys based on the given mapping from the original
 * targetlist to a new targetlist.
 */
List *
reconstruct_pathkeys(PlannerInfo *root, List *pathkeys, int *resno_map,
					 List *orig_tlist, List *new_tlist)
{
	List	   *new_pathkeys;
	ListCell   *i,
			   *j;

	new_pathkeys = NIL;
	foreach(i, pathkeys)
	{
		PathKey    *pathkey = (PathKey *) lfirst(i);
		bool		found = false;

		foreach(j, pathkey->pk_eclass->ec_members)
		{
			EquivalenceMember *em = (EquivalenceMember *) lfirst(j);
			TargetEntry *tle = tlist_member((Node *) em->em_expr, orig_tlist);

			if (tle)
			{
				TargetEntry *new_tle;
				EquivalenceClass *new_eclass;
				PathKey    *new_pathkey;

				new_tle = get_tle_by_resno(new_tlist, resno_map[tle->resno - 1]);
				if (!new_tle)
					elog(ERROR, "could not find path key expression in constructed subquery's target list");

				new_eclass = get_eclass_for_sort_expr(root, new_tle->expr,
													  em->em_datatype,
													  pathkey->pk_eclass->ec_opfamilies, 0);
				new_pathkey = makePathKey(new_eclass, pathkey->pk_opfamily, pathkey->pk_strategy,
										  pathkey->pk_nulls_first);

				new_pathkeys = lappend(new_pathkeys, new_pathkey);
				found = true;
				break;
			}
		}
		if (!found)
		{
			new_pathkeys = lappend(new_pathkeys, copyObject(pathkey));
		}
	}

	new_pathkeys = canonicalize_pathkeys(root, new_pathkeys);

	return new_pathkeys;
}


/* cost_common_agg -- Estimate the cost of executing the common subquery
 * for an aggregation plan.  Assumes that the AggPlanInfo contains the
 * correct Path as input_path.
 *
 * Returns the total cost and, more importantly, populates the given
 * dummy Plan node with cost information
 */
Cost
cost_common_agg(PlannerInfo *root, MppGroupContext *ctx, AggPlanInfo *info, Plan *dummy)
{
	QualCost	tlist_cost;
	Cost		startup_cost;
	Cost		total_cost;
	double		input_rows;
	int			input_width;
	int			n;

	Assert(dummy != NULL);

	input_rows = info->input_path->parent->rows;
	input_width = info->input_path->parent->width;
	/* Path input width isn't correct for ctx->sub_tlist so we guess. */
	n = 32 * list_length(ctx->sub_tlist);
	input_width = (input_width < n) ? input_width : n;

	/* Estimate cost of evaluation of the sub_tlist. */
	cost_qual_eval(&tlist_cost, ctx->sub_tlist, root);
	startup_cost = info->input_path->startup_cost + tlist_cost.startup;
	total_cost = info->input_path->total_cost + tlist_cost.startup +
		tlist_cost.per_tuple * input_rows;

	memset(dummy, 0, sizeof(Plan));
	dummy->type = info->input_path->type;
	dummy->startup_cost = startup_cost;
	dummy->total_cost = total_cost;
	dummy->plan_rows = input_rows;
	dummy->plan_width = input_width;

	return dummy->total_cost;
}



/* Function cost_1phase_aggregation
 *
 * May be used for 1 phase aggregation costing with or without DQAs.
 * Corresponds to make_one_stage_agg_plan and must be maintained in sync
 * with it.
 */
Cost
cost_1phase_aggregation(PlannerInfo *root, MppGroupContext *ctx, AggPlanInfo *info)
{
	Plan		input_dummy;
	bool		is_sorted;
	long		numGroups = (*(ctx->p_dNumGroups) < 0) ? 0 :
	(*(ctx->p_dNumGroups) > LONG_MAX) ? LONG_MAX :
	(long) *(ctx->p_dNumGroups);

	cost_common_agg(root, ctx, info, &input_dummy);

	is_sorted = pathkeys_contained_in(root->group_pathkeys, info->input_path->pathkeys);

	/* Collocation cost (Motion). */
	switch (info->group_prep)
	{
		case MPP_GRP_PREP_HASH_GROUPS:
			is_sorted = false;
			input_dummy.total_cost +=
				incremental_motion_cost(input_dummy.plan_rows,
										input_dummy.plan_rows);
			break;
		case MPP_GRP_PREP_FOCUS_QE:
		case MPP_GRP_PREP_FOCUS_QD:
			input_dummy.total_cost +=
				incremental_motion_cost(input_dummy.plan_rows,
										input_dummy.plan_rows * root->config->cdbpath_segments);
			input_dummy.plan_rows = input_dummy.plan_rows * root->config->cdbpath_segments;
			break;
		default:
			break;
	}

	/*
	 * NB: We don't need to calculate grouping extension costs here because
	 * grouping extensions are planned elsewhere.
	 */
	if (ctx->use_hashed_grouping)
	{
		/* HashAgg */
		Assert(ctx->numDistinctCols == 0);

		add_agg_cost(NULL, &input_dummy,
					 ctx->sub_tlist, (List *) root->parse->havingQual,
					 AGG_HASHED, false,
					 ctx->numGroupCols, ctx->groupColIdx,
					 numGroups, 0, ctx->agg_counts->numAggs,
					 ctx->agg_counts->transitionSpace);
	}
	else
	{
		if (ctx->numGroupCols == 0)
		{
			/* PlainAgg */
			add_agg_cost(NULL, &input_dummy,
						 ctx->sub_tlist, (List *) root->parse->havingQual,
						 AGG_PLAIN, false,
						 0, NULL,
						 1, 0, ctx->agg_counts->numAggs,
						 ctx->agg_counts->transitionSpace);
		}
		else
		{
			/* GroupAgg */
			if (!is_sorted)
			{
				add_sort_cost(NULL, &input_dummy, ctx->numGroupCols, NULL, NULL, -1.0);
			}
			add_agg_cost(NULL, &input_dummy,
						 ctx->sub_tlist, (List *) root->parse->havingQual,
						 AGG_SORTED, false,
						 ctx->numGroupCols, ctx->groupColIdx,
						 numGroups, 0, ctx->agg_counts->numAggs,
						 ctx->agg_counts->transitionSpace);
		}

		/*
		 * Beware: AGG_PLAIN and AGG_GROUPED may be used with DQAs, however,
		 * the function cost_agg doesn't distinguish DQAs so it consistently
		 * under estimates the cost in these cases.
		 */
		if (ctx->numDistinctCols > 0)
		{
			Path		path_dummy;
			double		ngrps = *(ctx->p_dNumGroups);
			double		nsorts = ngrps * ctx->numDistinctCols;
			double		avgsize = input_dummy.plan_rows / ngrps;

			cost_sort(&path_dummy, NULL, NIL, 0.0, avgsize, 32, -1);
			input_dummy.total_cost += nsorts * path_dummy.total_cost;
		}
	}
	info->plan_cost = root->config->gp_eager_one_phase_agg ? (Cost) 0.0 : input_dummy.total_cost;
	info->valid = true;
	info->join_strategy = DqaJoinNone;
	info->use_sharing = false;

	info->plan_cost *= gp_coefficient_1phase_agg;
	return info->plan_cost;
}


/* Function cost_2phase_aggregation
 *
 * May be used for 2 phase costing with 0 or 1 DQAs.
 * Corresponds to make_two_stage_agg_plan and must be maintained in sync
 * with it.
 */
Cost
cost_2phase_aggregation(PlannerInfo *root, MppGroupContext *ctx, AggPlanInfo *info)
{
	Plan		input_dummy;
	bool		is_sorted;
	long		numGroups = (*(ctx->p_dNumGroups) < 0) ? 0 :
	(*(ctx->p_dNumGroups) > LONG_MAX) ? LONG_MAX :
	(long) *(ctx->p_dNumGroups);
	double		input_rows;
	double		streaming_fudge = 1.3;

	cost_common_agg(root, ctx, info, &input_dummy);
	input_rows = input_dummy.plan_rows;

	is_sorted = pathkeys_contained_in(root->group_pathkeys, info->input_path->pathkeys);

	/* Precondition Input */

	switch (info->group_prep)
	{
		case MPP_GRP_PREP_HASH_DISTINCT:
			input_dummy.total_cost +=
				incremental_motion_cost(input_dummy.plan_rows,
										input_dummy.plan_rows);
			is_sorted = false;
			break;
		case MPP_GRP_PREP_NONE:
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_CDB_INTERNAL_ERROR),
					 errmsg("unexpected call for two-stage aggregation")));
			break;				/* Never */
	}

	/* Preliminary Aggregation */

	if (ctx->use_hashed_grouping)
	{
		/* Preliminary HashAgg */
		add_agg_cost(NULL, &input_dummy,
					 NIL, NIL,	/* Don't know preliminary tlist, qual IS NIL */
					 AGG_HASHED, root->config->gp_hashagg_streambottom,
					 ctx->numGroupCols, ctx->groupColIdx,
					 numGroups, 0, ctx->agg_counts->numAggs,
					 ctx->agg_counts->transitionSpace);

		if (gp_hashagg_streambottom)
		{
			input_dummy.plan_rows *= streaming_fudge;
		}
	}
	else
	{
		if (ctx->numGroupCols == 0)
		{
			/* Preliminary PlainAgg */
			add_agg_cost(NULL, &input_dummy,
						 NIL, NIL,	/* Don't know preliminary tlist, qual IS
									 * NIL */
						 AGG_PLAIN, false,
						 0, NULL,
						 1, 0, ctx->agg_counts->numAggs,
						 ctx->agg_counts->transitionSpace);
		}
		else
		{
			/* Preliminary GroupAgg */
			if (!is_sorted)
			{
				add_sort_cost(NULL, &input_dummy, ctx->numGroupCols, NULL, NULL, -1.0);
			}
			add_agg_cost(NULL, &input_dummy,
						 NIL, NIL,	/* Don't know preliminary tlist, qual IS
									 * NIL */
						 AGG_SORTED, false,
						 ctx->numGroupCols, ctx->groupColIdx,
						 numGroups, 0, ctx->agg_counts->numAggs,
						 ctx->agg_counts->transitionSpace);
		}

		/*
		 * Beware: AGG_PLAIN and AGG_GROUPED may be used with DQAs, however,
		 * the function cost_agg doesn't distinguish DQAs so it consistently
		 * under estimates the cost in these cases.
		 */
		if (ctx->numDistinctCols > 0)
		{
			Path		path_dummy;
			Cost		run_cost;
			double		ngrps = *(ctx->p_dNumGroups);
			double		avgsize = input_rows / ngrps;

			Assert(ctx->numDistinctCols == 1);

			cost_sort(&path_dummy, NULL, NIL, input_dummy.total_cost, avgsize, 32, -1.0);
			run_cost = path_dummy.total_cost - path_dummy.startup_cost;
			input_dummy.total_cost += path_dummy.startup_cost + ngrps * run_cost;
		}

	}

	/* Collocate groups */
	switch (info->group_type)
	{
		case MPP_GRP_TYPE_GROUPED_2STAGE:	/* Redistribute */
			input_dummy.total_cost +=
				incremental_motion_cost(input_dummy.plan_rows,
										input_dummy.plan_rows);
			break;
		case MPP_GRP_TYPE_PLAIN_2STAGE: /* Gather */
			input_dummy.total_cost +=
				incremental_motion_cost(input_dummy.plan_rows,
										input_dummy.plan_rows * root->config->cdbpath_segments);
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_CDB_INTERNAL_ERROR),
					 errmsg("unexpected call for two-stage aggregation")));
			break;				/* Never */
	}

	/* Final Aggregation */

	if (ctx->use_hashed_grouping)
	{
		/* HashAgg */
		add_agg_cost(NULL, &input_dummy,
					 NIL, NIL,	/* Don't know tlist or qual */
					 AGG_HASHED, false,
					 ctx->numGroupCols, ctx->groupColIdx,
					 numGroups, 0, ctx->agg_counts->numAggs,
					 ctx->agg_counts->transitionSpace);
	}
	else
	{
		if (ctx->numGroupCols == 0)
		{
			/* PlainAgg */
			add_agg_cost(NULL, &input_dummy,
						 NIL, NIL,	/* Don't know tlist or qual */
						 AGG_PLAIN, false,
						 0, NULL,
						 1, 0, ctx->agg_counts->numAggs,
						 ctx->agg_counts->transitionSpace);
		}
		else
		{
			/* GroupAgg */
			add_sort_cost(NULL, &input_dummy, ctx->numGroupCols, NULL, NULL, -1.0);
			add_agg_cost(NULL, &input_dummy,
						 NIL, NIL,	/* Don't know tlist or qual */
						 AGG_SORTED, false,
						 ctx->numGroupCols, ctx->groupColIdx,
						 numGroups, 0, ctx->agg_counts->numAggs,
						 ctx->agg_counts->transitionSpace);
		}
	}

	info->plan_cost = root->config->gp_eager_two_phase_agg ? (Cost) 0.0 : input_dummy.total_cost;
	info->valid = true;
	info->join_strategy = DqaJoinNone;
	info->use_sharing = false;

	info->plan_cost *= gp_coefficient_2phase_agg;
	return info->plan_cost;
}


/* Function cost_3phase_aggregation
 *
 * Only used for costing with 2 or more DQAs.
 * Corresponds to make_three_stage_agg_plan and must be maintained in sync
 * with it.
 *
 * This function assumes the enviroment established by planDqaJoinOrder()
 * and set_coplan_strategies().
 */
Cost
cost_3phase_aggregation(PlannerInfo *root, MppGroupContext *ctx, AggPlanInfo *info)
{
	Plan		dummy;
	Cost		total_cost;
	Cost		share_cost;
	Cost		reexec_cost;
	int			i;
	bool		use_sharing = false;
	DqaJoinStrategy join_strategy = DqaJoinUndefined;
	Cost		input_cost = 0.0;
	Cost		cost_coplan_cheapest = 0.0;
	Cost		cost_coplan_sorted = 0.0;
	Cost		cost_hashjoin = 0.0;
	Cost		cost_mergejoin = 0.0;
	Cost		cost_crossjoin = 0.0;

	cost_common_agg(root, ctx, info, &dummy);

	total_cost = dummy.total_cost;

	Assert(ctx->numDistinctCols == list_length(ctx->agg_counts->dqaArgs));

	/*
	 * Note that join order has already been established by an earlier call to
	 * planDqaJoinOrder.  Here we just use that order, but we need to decide
	 * on the join type.
	 */
	if (list_length(ctx->agg_counts->dqaArgs) < 2)
	{
		join_strategy = DqaJoinNone;
	}
	else if (ctx->numGroupCols == 0)
	{
		join_strategy = DqaJoinCross;
	}
	else if (sorting_prefixes_grouping(root))
	{
		/* Cheapest of merge join of sorted input or sorted hash join */
		join_strategy = DqaJoinSorted;
	}
	else
	{
		/* Cheapest of merge join of sorted input or hash join */
		join_strategy = DqaJoinCheapest;
	}

	/* Determine whether to use input sharing. */
	if (ctx->numDistinctCols < 2)
	{
		reexec_cost = total_cost;
		use_sharing = false;
	}
	else
	{
		/*
		 * Decide based on apparent costs. XXX Do we need to override this if
		 * there are  volatile functions in the common plan?  Is this known,
		 * or do we need to search?
		 */
		share_cost = cost_share_plan(&dummy, root, ctx->numDistinctCols);
		reexec_cost = ctx->numDistinctCols * total_cost;
		use_sharing = share_cost < reexec_cost;
	}
	input_cost = use_sharing ? share_cost : reexec_cost;

	/* Get costs for the ctx->numDistinctCols coplans. */
	cost_coplan_cheapest = cost_coplan_sorted = 0;
	for (i = 0; i < ctx->numDistinctCols; i++)
	{
		DqaInfo    *arg = ctx->dqaArgs + i;

		cost_coplan_cheapest += arg->cost_cheapest;
		cost_coplan_sorted += arg->cost_sorted;
	}

	/* Get costs to join the coplan results. */
	switch (join_strategy)
	{
		case DqaJoinNone:
			break;

		case DqaJoinCross:
			cost_crossjoin = (ctx->numDistinctCols - 1) * 2 * cpu_tuple_cost;
			break;

		case DqaJoinSorted:
		case DqaJoinCheapest:
			set_cost_of_join_strategies(ctx, &cost_hashjoin, &cost_mergejoin);

			if (join_strategy == DqaJoinSorted)
				cost_hashjoin += incremental_sort_cost(*ctx->p_dNumGroups, 100, ctx->numDistinctCols);

			cost_hashjoin += cost_coplan_cheapest;
			cost_mergejoin += cost_coplan_sorted;

			if (cost_hashjoin > 0.0 && cost_hashjoin <= cost_mergejoin)
			{
				join_strategy = DqaJoinHash;
			}
			else
			{
				join_strategy = DqaJoinMerge;
			}
			break;

		default:
			elog(ERROR, "unexpected join strategy during DQA planning");
	}

	/* Compare costs choose cheapest. */
	switch (join_strategy)
	{
		case DqaJoinNone:
			total_cost = input_cost + cost_coplan_cheapest;
			break;

		case DqaJoinCross:
			total_cost = input_cost + cost_coplan_cheapest + cost_crossjoin;
			break;

		case DqaJoinHash:
			total_cost = input_cost + cost_coplan_cheapest + cost_hashjoin;
			break;

		case DqaJoinMerge:
			total_cost = input_cost + cost_coplan_cheapest + cost_mergejoin;
			break;

		default:
			elog(ERROR, "unexpected join strategy during DQA planning");
	}

	info->plan_cost = root->config->gp_eager_dqa_pruning ? (Cost) 0.0 : total_cost;
	info->valid = true;
	info->join_strategy = join_strategy;
	info->use_sharing = use_sharing;

	info->plan_cost *= gp_coefficient_3phase_agg;
	return info->plan_cost;
}


/* Estimate the costs of
 * 		1. HashJoin of cheapest inputs, and
 *		2. MergeJoin of sorted input.
 *
 * If result should be ordered, compare a Sort of 1 with 2.
 * Else compare 1 with 2.
 */
void
set_cost_of_join_strategies(MppGroupContext *ctx, Cost *hashjoin_cost, Cost *mergejoin_cost)
{
	Cost		hj_cost;
	Cost		mj_cost;
	List	   *mergeclauses = NIL;
	List	   *hashclauses = NIL;

	double		rows;
	int			gk_width;
	int			outer_width;
	bool		try_hashed = true;
	AttrNumber	attrno;
	Index		outer_varno = 1;
	int			i;

	rows = *ctx->p_dNumGroups;

	/* Widths are wild speculation, but good enough, we hope. */
	gk_width = 32 * ctx->numGroupCols;
	outer_width = 32;			/* DQA transition values for first DQA arg. */
	outer_width += 64;			/* Ordinary aggregate transition values. */

	/* We need join clauses for costing. */
	for (i = 0; i < ctx->numGroupCols; i++)
	{
		Expr	   *qual;
		Var		   *outer_var;
		Var		   *inner_var;
		AttrNumber	resno = ctx->groupColIdx[i];
		Index		inner_varno = 1 + (i + 1);
		TargetEntry *tle = get_tle_by_resno(ctx->sub_tlist, resno);

		Assert(tle != NULL);

		outer_var = makeVar(outer_varno, resno,
							exprType((Node *) tle->expr),
							exprTypmod((Node *) tle->expr), 0);

		inner_var = makeVar(inner_varno, resno,
							exprType((Node *) tle->expr),
							exprTypmod((Node *) tle->expr), 0);

		/* outer should always be on the left */
		qual = make_op(NULL, list_make1(makeString("=")),
					   (Node *) outer_var,
					   (Node *) inner_var, -1);

		/* If the grouping column is not hashable, do not try hashing. */
		if (!hash_safe_type(exprType((Node *) tle->expr)))
			try_hashed = false;

		if (try_hashed)
		{
			hashclauses = lappend(hashclauses, copyObject(qual));
		}

		qual->type = T_DistinctExpr;
		qual = make_notclause(qual);
		mergeclauses = lappend(mergeclauses, qual);
	}

	/* Estimate the incremental join costs. */
	hj_cost = mj_cost = 0;
	for (attrno = 1; attrno < ctx->numDistinctCols; attrno++)
	{
		int			dqa_width = 32;
		int			inner_width = gk_width + dqa_width;

		mj_cost += incremental_mergejoin_cost(rows, mergeclauses, ctx->root);
		if (try_hashed)
			hj_cost += incremental_hashjoin_cost(rows, inner_width, outer_width, hashclauses, ctx->root);

		outer_width += dqa_width;
	}

	*mergejoin_cost = mj_cost;
	*hashjoin_cost = try_hashed ? hj_cost : 0.0;
}

/* Set up basic structure content.  Caller to fill in.
 */
static
void
initAggPlanInfo(AggPlanInfo *info, Path *input_path, Plan *input_plan)
{
	info->input_path = input_path;
	info->input_plan = input_plan;

	if (input_path != NULL)
		info->input_locus = input_path->locus;
	else
		CdbPathLocus_MakeNull(&info->input_locus);

	info->group_type = MPP_GRP_TYPE_BASEPLAN;
	info->group_prep = MPP_GRP_PREP_NONE;
	CdbPathLocus_MakeNull(&info->output_locus);
	info->distinctkey_collocate = false;

	info->valid = false;
	info->plan_cost = 0;
	info->join_strategy = DqaJoinUndefined;
	info->use_sharing = false;
}


/* set_coplan_strategies
 *
 * Determine and cache in the given DqaInfo structure the cheapest
 * strategy that computes the answer and the cheapest strategy that
 * computes the answer in grouping key order.
 *
 * Below, the result cardinalities are shown as <-n- where
 *
 *   x (input_rows) is the input cardinality which is usually about
 *     equal to #segments * #distinct(grouping key, DQA arg)
 *
 *   d (darg_rows) is #distinct(grouping key, DQA arg)
 *
 *   g (group_rows) is #distinct(grouping key)
 *
 * The coplan through the Motion that collocates tuples on the
 * grouping key is planned independently and will be one of
 *
 *    <- Motion <-x- HashAgg <-t- R
 *    <- Motion <-x- GroupAgg <- Sort <-t- R
 *
 * which is encoded in DqaInfo by the flag use_hashed_preliminary.
 *
 * The possible post-Motion strategies are encoded as enum values of
 * type DqaCoplanType and indicate all the required plan nodes.
 *
 * Vector aggregation strategies that produce a result ordered on the
 * grouping key are:
 *
 * DQACOPLAN_GGS:	<-g- GroupAgg <-d- GroupAgg <- Sort <-x-
 * DQACOPLAN_GSH:	<-g- GroupAgg <- Sort <-d- HashAgg <-x-
 * DQACOPLAN_SHH:	<- Sort <-g-  HashAgg <-d- HashAgg <-x-
 *
 * In addition, the vector aggreagation strategy
 *
 * DQACOPLAN_HH:	<-g- HashAgg <-d- HashAgg <-x- R
 *
 * produces an unordered result.
 *
 * Scalar aggregation strategies are:
 *
 * DQACOPLAN_PGS:	<-1- PlainAgg <-d- GroupAgg <- Sort <-x- R
 * DQACOPLAN_PH:	<-1- PlainAgg <-d- HashedAgg <-x- R
 *
 */
void
set_coplan_strategies(PlannerInfo *root, MppGroupContext *ctx, DqaInfo *dqaArg, Path *input)
{
	double		input_rows = input->parent->rows;
	int			input_width = input->parent->width;
	double		darg_rows = dqaArg->num_rows;
	double		group_rows = *ctx->p_dNumGroups;
	long		numGroups = (group_rows < 0) ? 0 :
	(group_rows > LONG_MAX) ? LONG_MAX :
	(long) group_rows;
	// GPDB_84_MERGE_FIXME: choose_hashed_grouping no longer tracks these separately.
	//bool		can_hash_group_key = ctx->agg_counts->canHashAgg;
	//bool		can_hash_dqa_arg = dqaArg->can_hash;
	bool can_hash_group_key = true;
	bool can_hash_dqa_arg = true;
	bool		use_hashed_preliminary = false;

	Cost		sort_input = incremental_sort_cost(input_rows, input_width,
												   ctx->numGroupCols + 1);
	Cost		sort_dargs = incremental_sort_cost(darg_rows, input_width,
												   ctx->numGroupCols);
	Cost		sort_groups = incremental_sort_cost(group_rows, input_width,
													ctx->numGroupCols);
	Cost		gagg_input = incremental_agg_cost(input_rows, input_width,
												  AGG_SORTED, ctx->numGroupCols + 1,
												  numGroups, ctx->agg_counts->numAggs,
												  ctx->agg_counts->transitionSpace);
	Cost		gagg_dargs = incremental_agg_cost(darg_rows, input_width,
												  AGG_SORTED, ctx->numGroupCols,
												  numGroups, ctx->agg_counts->numAggs,
												  ctx->agg_counts->transitionSpace);
	Cost		hagg_input = incremental_agg_cost(input_rows, input_width,
												  AGG_HASHED, ctx->numGroupCols + 1,
												  numGroups, ctx->agg_counts->numAggs,
												  ctx->agg_counts->transitionSpace);
	Cost		hagg_dargs = incremental_agg_cost(darg_rows, input_width,
												  AGG_HASHED, ctx->numGroupCols,
												  numGroups, ctx->agg_counts->numAggs,
												  ctx->agg_counts->transitionSpace);
	Cost		cost_base;
	Cost		cost_sorted;
	Cost		cost_cheapest;
	DqaCoplanType type_sorted;
	DqaCoplanType type_cheapest;
	Cost		trial;

	/* Preliminary aggregation */
	use_hashed_preliminary = (can_hash_group_key || ctx->numGroupCols == 0)
		&& can_hash_dqa_arg;
	if (use_hashed_preliminary)
	{
		cost_base = hagg_input;
	}
	else
	{
		cost_base = sort_input + gagg_input;
	}

	/* Collocating motion */
	cost_base += incremental_motion_cost(darg_rows, darg_rows);

	/* Post-motion processing is more complex. */

	if (ctx->numGroupCols == 0) /* scalar agg */
	{
		Cost		pagg_dargs = incremental_agg_cost(darg_rows, input_width,
													  AGG_PLAIN, 0,
													  1, ctx->agg_counts->numAggs,
													  ctx->agg_counts->transitionSpace);

		type_sorted = type_cheapest = DQACOPLAN_PGS;
		cost_sorted = cost_cheapest = sort_input + gagg_input + pagg_dargs;

		trial = hagg_input + pagg_dargs;
		if (trial < cost_cheapest)
		{
			cost_cheapest = trial;
			type_cheapest = DQACOPLAN_PH;
		}
	}
	else						/* vector agg */
	{
		type_sorted = type_cheapest = DQACOPLAN_GGS;
		cost_sorted = cost_cheapest = sort_input + gagg_input + gagg_dargs;

		if (can_hash_dqa_arg)
		{
			trial = hagg_input + sort_dargs + gagg_input;

			if (trial < cost_cheapest)
			{
				cost_cheapest = trial;
				type_cheapest = DQACOPLAN_GSH;
			}

			if (trial < cost_sorted)
			{
				cost_sorted = trial;
				type_sorted = DQACOPLAN_GSH;
			}
		}

		if (can_hash_group_key && can_hash_dqa_arg)
		{
			trial = hagg_input + hagg_dargs;

			if (trial < cost_cheapest)
			{
				cost_cheapest = trial;
				type_cheapest = DQACOPLAN_HH;
			}

			trial += sort_groups;

			if (trial < cost_sorted)
			{
				cost_sorted = trial;
				type_sorted = DQACOPLAN_SHH;
			}
		}
	}

	dqaArg->use_hashed_preliminary = use_hashed_preliminary;
	dqaArg->cost_sorted = cost_base + cost_sorted;
	dqaArg->coplan_type_sorted = type_sorted;
	dqaArg->cost_cheapest = cost_base + cost_cheapest;
	dqaArg->coplan_type_cheapest = type_cheapest;
	dqaArg->distinctkey_collocate = false;
}


/* incremental_sort_cost -- helper for set_coplan_strategies
 */
Cost
incremental_sort_cost(double rows, int width, int numKeyCols)
{
	Plan		dummy;

	memset(&dummy, 0, sizeof(dummy));
	dummy.plan_rows = rows;
	dummy.plan_width = width;

	add_sort_cost(NULL, &dummy, numKeyCols, NULL, NULL, -1.0);

	return dummy.total_cost;
}

/* incremental_agg_cost -- helper for set_coplan_strategies
 */
Cost
incremental_agg_cost(double rows, int width, AggStrategy strategy,
					 int numGroupCols, double numGroups,
					 int numAggs, int transSpace)
{
	Plan		dummy;

	memset(&dummy, 0, sizeof(dummy));
	dummy.plan_rows = rows;
	dummy.plan_width = width;

	add_agg_cost(NULL, &dummy,
				 NULL, NULL,
				 strategy, false,
				 numGroupCols, NULL,
				 numGroups, 0, numAggs, transSpace);

	return dummy.total_cost;
}


/*  incremental_motion_cost -- helper for set_coplan_strategies
 */
Cost
incremental_motion_cost(double sendrows, double recvrows)
{
	Cost		cost_per_row = (gp_motion_cost_per_row > 0.0)
	? gp_motion_cost_per_row
	: 2.0 * cpu_tuple_cost;

	return cost_per_row * 0.5 * (sendrows + recvrows);
}

/*
 * rebuilt_simple_rel_and_rte
 *
 * Rebuild arrays for RelOptInfo and RangeTblEntry for the PlannerInfo when
 * the underlying range tables are transformed.  They can be rebuilt from
 * parse->rtable, and will be used in later than here by processes like
 * distinctClause planning.  We never pfree the original array, since
 * it's potentially used by other PlannerInfo which this is copied from.
 */
static void
rebuild_simple_rel_and_rte(PlannerInfo *root)
{
	int			i;
	int			array_size;
	ListCell   *l;

	array_size = list_length(root->parse->rtable) + 1;
	root->simple_rel_array_size = array_size;
	root->simple_rel_array =
		(RelOptInfo **) palloc0(sizeof(RelOptInfo *) * array_size);
	root->simple_rte_array =
		(RangeTblEntry **) palloc0(sizeof(RangeTblEntry *) * array_size);
	i = 1;
	foreach(l, root->parse->rtable)
	{
		root->simple_rte_array[i] = lfirst(l);
		i++;
	}
	i = 1;
	foreach(l, root->parse->rtable)
	{
		(void) build_simple_rel(root, i, RELOPT_BASEREL);
		i++;
	}
}

Plan *
add_motion_to_dqa_child(Plan *plan, PlannerInfo *root, bool *motion_added)
{
	Plan	   *result = plan;

	*motion_added = false;

	List	   *pathkeys = make_pathkeys_for_groupclause(root, root->parse->groupClause, plan->targetlist);
	CdbPathLocus locus = cdbpathlocus_from_flow(plan->flow);

	if (CdbPathLocus_IsPartitioned(locus) && NIL != plan->flow->hashExpr)
	{
		locus = cdbpathlocus_from_exprs(root, plan->flow->hashExpr);
	}

	if (!cdbpathlocus_collocates(root, locus, pathkeys, true /* exact_match */ ))
	{
		/*
		 * MPP-22413: join requires exact distribution match for collocation
		 * purposes, which may not be provided by the underlying group by, as
		 * computing the group by only requires relaxed distribution
		 * collocation
		 */
		List	   *groupExprs = get_sortgrouplist_exprs(root->parse->groupClause,
														 plan->targetlist);

		result = (Plan *) make_motion_hash(root, plan, groupExprs);
		result->total_cost += incremental_motion_cost(plan->plan_rows, plan->plan_rows);
		*motion_added = true;
	}

	return result;
}


static bool
contain_aggfilters_walker(Node *node, void *context)
{
	if (node == NULL)
		return false;

	if (IsA(node, Aggref))
	{
		Aggref	   *aggref = (Aggref *) node;

		if (aggref->aggfilter)
			return true;
	}
	return expression_tree_walker(node, contain_aggfilters_walker, NULL);
}

static bool
contain_aggfilters(Node *node)
{
	return contain_aggfilters_walker(node, NULL);
}
