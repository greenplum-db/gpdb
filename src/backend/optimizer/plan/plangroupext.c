/*-------------------------------------------------------------------------
 *
 * plangroupext.c
 *    Planning routines for grouping extensions.
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *    src/backend/optimizer/plan/plangroupext.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "nodes/makefuncs.h"
#include "parser/parse_expr.h"
#include "parser/parse_clause.h"   /* assignSortGroupRef */
#include "parser/parse_oper.h"     /* ordering_oper_opid */
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/cost.h"
#include "optimizer/clauses.h"
#include "optimizer/tlist.h"
#include "optimizer/paths.h"
#include "optimizer/planshare.h"
#include "parser/parse_relation.h" /* addRangeTableEntryForSubquery */
#include "parser/parsetree.h"  /* get_tle_by_resno */

#include "cdb/cdbsetop.h"					/* make_motion... routines */
#include "cdb/cdbvars.h"
#include "cdb/cdbpathlocus.h"
#include "cdb/cdbgroup.h" 					/* generate_three_tlists */
#include "cdb/cdbllize.h"                   /* pull_up_Flow */
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"                /* INT4OID */
#include "utils/lsyscache.h"                /* get_typavgwidth */

/*
 * Define a canonical form of a ROLLUP.
 *
 * This can be used to represent a complete or partial ROLLUP, from
 * the user requested ROLLUP clause, or part of user requested CUBE
 * or GROUPING SETS clauses.
 */
typedef struct CanonicalRollup
{
	/* Define the number of columns in a rollup. The maximum number of
	 * grouping sets in a rollup is (numcols+1).
	 */
	int numcols;

	/* The column index for this rollup */
	AttrNumber *colIdx;
	Oid		   *operators;

	/* This rollup may be part of a GROUPING SETS or a CUBE. This value
	 * is used to record the relative position of columns in the original
	 * grouping extension clause.
	 *
	 * For example, the original grouping extension is
	 *
	 *    CUBE (a,b,c)
	 *
	 * which can be converted into the following rollups:
	 *    (a,b,c) ==> (a,b) ==> (a) ==> ()
	 *    (c,a) ==> (c)
	 *    (b,a) ==> (b)
	 *
	 * Assume that this canonical rollup wants to represent the second
	 * rollup "(c,a) ==> (c)". We want to record the position of "c" and
	 * "a" in the original CUBE clause. This is used to calculate
	 * the GROUPING value.
	 *
	 * Note that the value for positions starts with 0.
	 */
	int *grping_pos;

	/* Since this rollup can be a partial rollup, we record the number
	 * of grouping sets that are really appeared in this rollup.
	 */
	int ngrpsets;

	/* Represent the number of times that each grouping set appears
	 * in a rollup. There should be 'numcols+1' elements in this array.
	 *
	 * Note that this should be the last one in this structure.
	 */
	int grpset_counts[1];
} CanonicalRollup;

typedef struct GroupExtContext
{
	Path *path; /* the path before grouping planning */
	
	double tuple_fraction;
	bool use_hashed_grouping;
	List *tlist;
	List *sub_tlist;
	List *qual;

	AggStrategy aggstrategy;
	int numGroupCols;
	AttrNumber *grpColIdx;
	Oid		   *grpOperators;
	int numDistinctCols;
	AttrNumber *distinctColIdx;
	AggClauseCounts *agg_counts;
	double *p_dNumGroups;
	List *current_pathkeys;
	
	bool is_agg; /* indicate whether to create Agg/Group nodes. */
	bool twostage; /* indicate if this appears in a two-stage aggregation. */
	bool need_repeat_node; /* indicate if a Repeat node is needed in a two-stage aggregation. */
	bool querynode_changed; /* indicate if root->parse is changed. */

	List *canonical_rollups; /* list of CanonicalRollups */

	/* the rollup which is currently being processed */
	CanonicalRollup *current_rollup;

	/* The current grouping set sequence number */
	int curr_grpset_no;

	/* The following are intended for input sharing */
	Plan *subplan;
	CdbPathLocus input_locus;
	/* also the above p_current_pathkeys */
	/* requested sort order for each Agg node using this shared input */
	List *pathkeys;
} GroupExtContext;

static void destroyGroupExtContext(GroupExtContext *context);
static List *convert_gs_to_rollups(AttrNumber *grpColIdx, Oid *grpOperators,
								   int numGroupCols,
								   CanonicalGroupingSets *canonical_grpsets,
								   List *canonical_rollups,
								   List *sub_tlist,
								   List *tlist);
static Plan *make_aggs_for_rollup(PlannerInfo *root,
								  GroupExtContext *context,
								  Plan *lefttree);
static Plan *make_list_aggs_for_rollup(PlannerInfo *root,
									   GroupExtContext *context,
									   Plan *lefttree);
static Plan*make_append_aggs_for_rollup(PlannerInfo *root,
										GroupExtContext *context,
										Plan *lefttree);
static Plan *plan_append_aggs_with_rewrite(PlannerInfo *root,
										   GroupExtContext *context,
										   Plan *lefttree);
static Plan *plan_append_aggs_with_gather(PlannerInfo *root,
										  GroupExtContext *context,
										  Plan *lefttree);
static Plan *add_first_agg(PlannerInfo *root,
						   GroupExtContext *context,
						   int num_nullcols,
						   uint64 input_grouping,
						   uint64 grouping,
						   int rollup_gs_times,
						   int numGroupCols,
						   AttrNumber *groupColIdx,
						   Oid *groupOperators,
						   List *current_tlist,
						   List *current_qual,
						   Plan *current_lefttree);
static List *generate_list_subplans(PlannerInfo *root,
									int num_subplans,
									GroupExtContext *context);
static List *add_grouping_groupid(List *tlist);
static void append_colIdx(AttrNumber *colIdx, Oid *operators, int numcols, int colno,
						  int *grping_pos, Bitmapset *bms, AttrNumber *grpColIdx, Oid *grpOperators,
						  Index *sortrefs_to_resnos, int max_sortgroupref);
static Plan *plan_list_rollup_plans(PlannerInfo *root,
									GroupExtContext *context,
									Plan *lefttree);
static Node *replace_grouping_columns(Node *node,
									  List *sub_tlist,
									  AttrNumber *grpColIdx,
									  int start_colno,
									  int end_colno,
									  bool is_targetList);
static bool contain_groupingfunc(Node *node);
static void checkGroupExtensionQuery(CanonicalGroupingSets *cgs, List *targetList);

/**
 * These functions are to be used for debugging purpose only.
 */
#ifdef DEBUG_GROUPING_SETS
char *canonicalRollupListToString(List *crl);
char *canonicalRollupToString(CanonicalRollup *cr);
char *canonicalGroupingSetsToString(CanonicalGroupingSets *cgs);
char *bitmapsetToString(Bitmapset *bms);
#endif

/**
 * Method returns the target entry from list which matches the ressortgroupref.
 * It returns NULL if no match is found.
 */
static TargetEntry *tlist_member_by_ressortgroupref(List *targetList, int ressortgroupref)
{
	ListCell *lc = NULL;
	foreach (lc, targetList)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);
		if (tle->ressortgroupref == ressortgroupref)
		{
			return tle;
		}
	}
	return NULL;
}


/*
 * Reorder a path key, using given column mappings.
 */
static List *
reorder_pathkeys(PlannerInfo *root,
				 List *pathkeys,
				 AttrNumber *orig_grpColIdx,
				 AttrNumber *new_grpColIdx)
{
	List	   *result = NIL;
	int			grpno;
	int			numgrpkeys = list_length(pathkeys);

	for (grpno = 0; grpno < numgrpkeys; grpno++)
	{
		PathKey	   *pk;
		int			pos;

		/*
		 * Find the index position in orig_grpColIdx, in which the element is
		 * equal to new_grpColIdx[grpno]. This index position points to the
		 * place that the corresponding PathKey in pathkeys.
		 */
		for (pos = 0; pos < numgrpkeys; pos++)
		{
			if (orig_grpColIdx[pos] == new_grpColIdx[grpno])
				break;
		}

		Assert(pos < numgrpkeys);

		pk = (PathKey *) list_nth(pathkeys, pos);

		result = lappend(result, pk);
	}

	return result;
}

/**
 * Grouping extension planner does not correctly support a scenario where multiple grouping sets contain
 * references to expressions where one of which subsumes another in the select targetlist.
 * For example:
 * SELECT (sale.qty)  as a1,(sale.qty) as a2 FROM sale GROUP BY GROUPING SETS((1),(2));
 * SELECT (sale.qty)  as a1,(sale.qty + 100) as a2 FROM sale GROUP BY GROUPING SETS((1),(2));
 * 
 * See MPP-8358 for details about this problem. For this release, we identify and disable these cases using
 * this check.
 */
static void checkGroupExtensionQuery(CanonicalGroupingSets *cgs, List *targetList)
{
	Assert(cgs);
	
	if (cgs->ngrpsets < 2)
	{
		return;
	}

	Assert(cgs->grpsets);
	
	int i = 0;
	int j = 0;
	
	for (i=0;i<cgs->ngrpsets-1;i++)
	{
		Bitmapset *bms1 = bms_copy(cgs->grpsets[i]);

		/**
		 * Reconstruct targetlist based on this bitmap set.
		 */
		List *bms1TargetList = NIL;
		ListCell *lc = NULL;
		foreach (lc, targetList)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(lc);
			if (bms_is_member(tle->ressortgroupref, bms1))
			{
				bms1TargetList = lappend(bms1TargetList, tle);
			}
		}
		
		for (j=i+1;j<cgs->ngrpsets;j++)
		{
			Bitmapset *bms2 = bms_copy(cgs->grpsets[j]);
			
			/* We are only interested in elements in bms2 that are not present in bms1. Compute the diff. */
			
			Bitmapset *bmsdiff = bms_difference(bms2, bms1);
			
			/* Look at the target entries corresponding to bmsdiff. If a duplicate entry exists in the target list corresponding to bms1, we do not support the case */
			int bms2Entry = -1;
			while ((bms2Entry = bms_first_member(bmsdiff)) >= 0)
			{
				TargetEntry *tle2 = tlist_member_by_ressortgroupref(targetList, bms2Entry);
				Assert(tle2);
				
				/**
				 * We need to check if any target entry expression from bms1TargetList subsumes tle2's expression or vice versa. In this case, we return false.
				 */
				lc = NULL;
				foreach (lc, bms1TargetList)
				{
					TargetEntry *tle1 = lfirst(lc);
					Expr *expr1 = tle1->expr;
					Expr *expr2 = tle2->expr;
					
					/**
					 * If expr1 subsumes expr2 or vice versa, we return false.
					 */
					if (subexpression_match(expr1, expr2) || subexpression_match(expr2, expr1))
					{
						ereport(ERROR, (errcode(ERRCODE_CDB_FEATURE_NOT_YET),
								errmsg("GROUPING SETS / ROLLUP / CUBE columns ambiguous.")
						));
					}
				}
			}
			
			bms_free(bms2);
			bms_free(bmsdiff);
		} /* for j */
		
		bms_free(bms1);
	} /* for i */

	return;
}

/*
 * Create the plan for grouping extensions
 *
 * Most of the arguments are straightforward and similar to make_agg().
 * Exceptions are twostage, is_agg, p_tlist and sub_tlist.
 *
 * 'twostage' is used to tell if this function is called within a
 * two-stage aggregation.  If this is the case, we want to make sure
 * all targets in the targetlists generated here are not junk.
 *
 * 'is_agg' indicates to construct an Agg or Group node.
 *
 * 'p_tlist' is a pointer to the final target list for this sequence of Agg nodes.
 * We need to pass the final target list out because the final stage of two-stage
 * aggregation needs this.
 *
 * 'sub_tlist' is the subplan targetlist generated from '*p_tlist'.
 */
Plan *
plan_grouping_extension(PlannerInfo *root,
						Path *path,
						double tuple_fraction,
						bool use_hashed_grouping,
						List **p_tlist, List *sub_tlist,
						bool is_agg, bool twostage,
						List *qual,
						int *p_numGroupCols, AttrNumber **p_grpColIdx, Oid **p_grpOperators,
						AggClauseCounts *agg_counts,
						CanonicalGroupingSets *canonical_grpsets,
						double *p_dNumGroups,
						bool *querynode_changed,
						List **p_current_pathkeys,
						Plan *lefttree)
{
	GroupExtContext context;
	Plan *result_plan;
	List *tlist = copyObject(*p_tlist);

	context.path = path;
	context.tuple_fraction = tuple_fraction;
	context.use_hashed_grouping = use_hashed_grouping;
	context.tlist = tlist;
	context.sub_tlist = copyObject(sub_tlist);
	context.qual = qual;
	context.numGroupCols = *p_numGroupCols;
	context.grpColIdx = palloc0(context.numGroupCols * sizeof(AttrNumber));
	memcpy(context.grpColIdx, *p_grpColIdx,
		   context.numGroupCols * sizeof(AttrNumber));
	context.grpOperators = palloc0(context.numGroupCols * sizeof(Oid));
	memcpy(context.grpOperators, *p_grpOperators,
		   context.numGroupCols * sizeof(Oid));
	context.numDistinctCols = 0;
	context.distinctColIdx = NULL;
	context.agg_counts = agg_counts;
	context.p_dNumGroups = p_dNumGroups;
	context.current_pathkeys = *p_current_pathkeys;
	context.is_agg = is_agg;
	context.twostage = twostage;
	context.need_repeat_node = false;
	context.querynode_changed = false;
	context.canonical_rollups = NIL;
	context.curr_grpset_no = 0;

	/**
	 * MPP-8358 - we don't handle certain situations in group extension correctly.
	 */
	checkGroupExtensionQuery(canonical_grpsets, tlist);

	/*
	 * Convert a list of grouping sets into multiple rollups.
	 */
	context.canonical_rollups =
		convert_gs_to_rollups(context.grpColIdx, context.grpOperators, context.numGroupCols,
							  canonical_grpsets, context.canonical_rollups,
							  context.sub_tlist, context.tlist);

	/* Construct a plan for a list of rollups. */
	result_plan = plan_list_rollup_plans(root, &context, lefttree);

	/* destroy the context */
	destroyGroupExtContext(&context);

	/* Set the proper resnos for Grouping/GroupId columns in *p_grpColIdx.
	 */
	if (*p_grpColIdx != NULL)
	{
		*p_grpColIdx =
			(AttrNumber *)repalloc(*p_grpColIdx,
								   (*p_numGroupCols + 2) * sizeof(AttrNumber));
		*p_grpOperators =
			(Oid *) repalloc(*p_grpOperators,
							 (*p_numGroupCols + 2) * sizeof(Oid));
	}
	else
	{
		Assert(*p_numGroupCols == 0);
		
		*p_grpColIdx = 
			(AttrNumber *)palloc0((*p_numGroupCols + 2) * sizeof(AttrNumber));
		*p_grpOperators =
			(Oid *) palloc0((*p_numGroupCols + 2) * sizeof(Oid));
	}
	
	(*p_numGroupCols) += 2;

	*p_tlist = context.tlist;
	(*p_grpColIdx)[(*p_numGroupCols)-2] = list_length(*p_tlist) - 1;
	(*p_grpColIdx)[(*p_numGroupCols)-1] = list_length(*p_tlist);

	/* GROUPING column is an int8 */
	(*p_grpOperators)[(*p_numGroupCols) - 2] = Int8EqualOperator;
	/* GROUP_ID column is an int4 */
	(*p_grpOperators)[(*p_numGroupCols) - 1] = Int4EqualOperator;

	*p_current_pathkeys = context.current_pathkeys;

	*querynode_changed = context.querynode_changed;

	return result_plan;
}

void
free_canonical_groupingsets(CanonicalGroupingSets *canonical_grpsets)
{
	int grpset_no;
	for (grpset_no=0; grpset_no<canonical_grpsets->ngrpsets; grpset_no++)
	{
		bms_free(canonical_grpsets->grpsets[grpset_no]);
	}
	
	if (canonical_grpsets)
	{
		if (canonical_grpsets->grpsets)
			pfree(canonical_grpsets->grpsets);
		if (canonical_grpsets->grpset_counts)
			pfree(canonical_grpsets->grpset_counts);
		pfree(canonical_grpsets);
	}
}

/*
 * Create an agg plan for a complete or partial ROLLUP clause,
 * defined by context->current_rollup->grpset_counts.
 */
static Plan *
make_aggs_for_rollup(PlannerInfo *root,
					 GroupExtContext *context,
					 Plan *lefttree)
{
	Plan *result_plan;

	/*
	 * We always try to create a sequence of Agg/Group nodes one after
	 * the other whenever possible. Currently, this includes non-distinct
	 * qualified queries and there are defined prelimfuncs for requested
	 * aggregates. Otherwise, we fall back to use
	 * Append-Aggs.
	 */
	if (context->agg_counts->numDistinctAggs == 0 &&
		!context->agg_counts->missing_prelimfunc)
		result_plan = make_list_aggs_for_rollup(root, context, lefttree);
	else
		result_plan = make_append_aggs_for_rollup(root, context, lefttree);

	return result_plan;
}

/*
 * Create a sequence of Agg/Group nodes one after the other.
 *
 * The upper Agg/Group nodes aggregate over the results from the lower
 * Agg/Group nodes. The upper Agg/Group nodes not only return their own
 * aggregate results, but also pass through the aggregate results from
 * all lower nodes.
 *
 * Note that this is not suitable for DISTINCT-qualified rollup queries.
 */
static Plan *
make_list_aggs_for_rollup(PlannerInfo *root,
						  GroupExtContext *context,
						  Plan *lefttree)
{
	int group_no;
	Plan *current_lefttree = lefttree;
	Plan *agg_node = NULL;
	AttrNumber *prelimGroupColIdx;
	Oid		   *prelimGroupOperators;

	List *tlist1 = NIL;
	List *tlist2 = NIL;
	List *tlist3 = NIL;
	List *qual2 = NIL;
	List *qual3 = NIL;
	int last_group_no = 0;

	AttrNumber *orig_grpColIdx;
	Oid		   *orig_grpOperators;
	AttrNumber *groupColIdx = NULL;
	Oid		   *groupOperators = NULL;
	List *current_tlist;
	List *current_qual;
	double current_numGroups;
	uint64 grouping = 0;
	double numGroups = *context->p_dNumGroups; /* make a copy of the original number */
	/* The query has a non-empty set group clause */
	bool has_groups = (root->parse->groupClause != NIL);
	bool need_repeat_node = false;
	List	   *group_pathkeys;

	TargetEntry *tle;
	uint64    input_grouping = 0;
	bool need_finalagg = !context->twostage;

	generate_three_tlists(context->tlist, context->twostage,
						  context->sub_tlist, (Node *)context->qual,
						  context->numGroupCols,
						  context->grpColIdx,
						  context->grpOperators,
						  &tlist1, &tlist2, &tlist3, &qual3);

	orig_grpColIdx = context->grpColIdx;
	orig_grpOperators = context->grpOperators;
	context->grpColIdx = context->current_rollup->colIdx;
	context->grpOperators = context->current_rollup->operators;

	/* Even though we use context->grpColIdx to generate
	 * these tlists, but the order of grouping columns are defined in
	 * context->current_rollup->colIdx. We reflect this
	 * in constructing prelimGroupColIdx.
	 */
	prelimGroupColIdx =
		(AttrNumber*)palloc(context->numGroupCols * sizeof(AttrNumber));
	prelimGroupOperators =
		(Oid *) palloc(context->numGroupCols * sizeof(Oid));

	for (group_no = 0; group_no < context->numGroupCols; group_no++ )
	{
		int resno = context->current_rollup->colIdx[group_no];
		int pos;

		for (pos=0; pos<context->numGroupCols; pos++)
		{
			if (orig_grpColIdx[pos] == resno)
				break;
		}

		if (pos == context->numGroupCols)
			elog(ERROR, "could not find rollup column %u in original group by clause", group_no);

		prelimGroupColIdx[group_no] = pos + 1;
		prelimGroupOperators[group_no] = orig_grpOperators[pos];
	}

	/*
	 * We always append Grouping and group_id to all three tlists.
	 * Grouping is also served as an additional grouping column. So
	 * we add it to both context->grpColIdx, and prelimGroupColIdx.
	 */
	tlist1 = add_grouping_groupid(tlist1);
	tlist2 = add_grouping_groupid(tlist2);
	tlist3 = add_grouping_groupid(tlist3);

	/* Also add GROUPING, GROUP_ID into sub_tlist. */
	context->sub_tlist = add_grouping_groupid(context->sub_tlist);

	context->numGroupCols += 2;
	context->grpColIdx =
		(AttrNumber *)repalloc(context->grpColIdx,
							   context->numGroupCols * sizeof(AttrNumber));
	context->grpOperators =
		(Oid *)repalloc(context->grpOperators,
							   context->numGroupCols * sizeof(Oid));
	tle = list_nth(context->sub_tlist, list_length(context->sub_tlist) - 2);
	Assert(IsA(tle, TargetEntry));
	context->grpColIdx[context->numGroupCols - 2] = tle->resno;
	tle = list_nth(context->sub_tlist, list_length(context->sub_tlist) - 1);
	Assert(IsA(tle, TargetEntry));
	context->grpColIdx[context->numGroupCols - 1] = tle->resno;
	/* GROUPING column is an int8 */
	context->grpOperators[context->numGroupCols - 2] = Int8EqualOperator;
	/* GROUP_ID column is an int4 */
	context->grpOperators[context->numGroupCols - 1] = Int4EqualOperator;

	prelimGroupColIdx = (AttrNumber *)
		repalloc(prelimGroupColIdx, context->numGroupCols * sizeof(AttrNumber));
	prelimGroupOperators = (Oid *)
		repalloc(prelimGroupOperators, context->numGroupCols * sizeof(Oid));
	prelimGroupColIdx[context->numGroupCols - 1] = list_length(tlist2);
	prelimGroupColIdx[context->numGroupCols - 2] = list_length(tlist2) - 1;
	/* GROUPING column is an int8 */
	prelimGroupOperators[context->numGroupCols - 2] = Int8EqualOperator;
	/* GROUP_ID column is an int4 */
	prelimGroupOperators[context->numGroupCols - 1] = Int4EqualOperator;

	/*
	 * Find the index position for the last rollup level.
	 */
	for (group_no=0; group_no < context->current_rollup->numcols + 1; group_no++)
	{
		if (context->current_rollup->grpset_counts[group_no] > 0)
		{
			last_group_no = group_no;
			if (context->current_rollup->grpset_counts[group_no] > 1)
				need_repeat_node = true;
		}
	}

	if (context->twostage && context->need_repeat_node)
		need_repeat_node = true;

	/* Generate the bottom-level Agg node */
	for (group_no=0; group_no <= last_group_no; group_no++)
	{
		if (group_no > 0)
			grouping += ( ((uint64)1) << context->current_rollup->grping_pos[
							 context->current_rollup->numcols - group_no]);

		if (context->current_rollup->grpset_counts[group_no] > 0)
		{
			AggStrategy aggstrategy;
			groupColIdx = context->grpColIdx;
			groupOperators = context->grpOperators;

			/* Add sort node if needed, and set AggStrategy */
			if (root->parse->groupClause)
			{
				group_pathkeys = reorder_pathkeys(root, root->group_pathkeys,
												  orig_grpColIdx,
												  groupColIdx);
				if (!pathkeys_contained_in(group_pathkeys,
										   context->current_pathkeys))
				{
					current_lefttree = (Plan *)
						make_sort_from_pathkeys(root, current_lefttree, group_pathkeys,
												-1, false);
					context->current_pathkeys = group_pathkeys;
					mark_sort_locus(current_lefttree);

					if (!context->is_agg)
					{
						/* CDB */ /* pass DISTINCT to sort */
						if (IsA(current_lefttree, Sort) && gp_enable_sort_distinct)
						{
							Sort* pSort = (Sort*)current_lefttree;
							pSort->noduplicates = true;
						}
					}
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
				context->current_pathkeys = NIL;
			}

			context->aggstrategy = aggstrategy;

			if (current_lefttree->flow != NULL &&
				current_lefttree->flow->flotype == FLOW_SINGLETON &&
				has_groups)
				need_finalagg = false;

			if (!need_finalagg && group_no == last_group_no)
			{
				current_qual = context->qual;
				context->tlist = add_grouping_groupid(context->tlist);
				current_tlist = copyObject(context->tlist);
				prelimGroupColIdx[context->numGroupCols-1] = list_length(current_tlist) - 1;
				prelimGroupColIdx[context->numGroupCols-2] = list_length(current_tlist) - 2;
			}
			else
			{
				current_qual = NIL;
				current_tlist = tlist1;
			}

			/* Add an Agg node */
			agg_node = add_first_agg(root, context,
									 group_no,
									 0,
									 grouping,
									 (need_repeat_node ?
									  context->current_rollup->grpset_counts[group_no] : 0),
									 context->numGroupCols - 2, /* ignore GROUPING, GROUP_ID */
									 groupColIdx,
									 groupOperators,
									 current_tlist,
									 current_qual,
									 current_lefttree);

			current_lefttree = agg_node;
			context->tlist = copyObject(current_tlist);

			input_grouping = grouping;
			group_no++;

			break;
		}
	}

	/*
	 * Reset groupColIdx to point to the one for the newly-generated
	 * Agg node.
	 */
	groupColIdx = prelimGroupColIdx;
	groupOperators = prelimGroupOperators;

	/* Construct the rest Agg nodes. */
	for (; group_no <= last_group_no; group_no++)
	{
		AttrNumber *new_grpColIdx;
		Oid		   *new_grpOperators;
		AttrNumber last_grpCol, last_grpidCol;
		Oid			last_grpOperator, last_grpidOperator;

		if (group_no > 0)
			grouping += ( ((uint64)1) << context->current_rollup->grping_pos[
							 context->current_rollup->numcols - group_no]);

		if (context->current_rollup->grpset_counts[group_no] <= 0)
			continue;

		current_tlist = tlist2;
		current_qual = qual2;

		if (!need_finalagg && group_no == last_group_no)
		{
			current_tlist = tlist3;
			current_qual = qual3;
		}

		current_numGroups =
			(((double)(context->numGroupCols-group_no))/((double)(context->numGroupCols))) *
			numGroups;
		if (current_numGroups == 0)
			current_numGroups = 1;

		*context->p_dNumGroups += current_numGroups;

		/*
		 * Re-construct the grouping columns index array. The last two columns
		 * GROUPING, GROUP_ID are switched to its rightful position.
		 */
		new_grpColIdx = palloc0(context->numGroupCols * sizeof(AttrNumber));
		new_grpOperators = palloc0(context->numGroupCols * sizeof(Oid));
		memcpy(new_grpColIdx, groupColIdx, context->numGroupCols * sizeof(AttrNumber));
		memcpy(new_grpOperators, groupOperators, context->numGroupCols * sizeof(Oid));

		last_grpCol = new_grpColIdx[context->numGroupCols - 2];
		last_grpOperator = new_grpOperators[context->numGroupCols - 2];
		last_grpidCol = new_grpColIdx[context->numGroupCols - 1];
		last_grpidOperator = new_grpOperators[context->numGroupCols - 1];
		new_grpColIdx[context->numGroupCols - 2] =
			new_grpColIdx[context->numGroupCols - group_no - 2];
		new_grpOperators[context->numGroupCols - 2] =
			new_grpOperators[context->numGroupCols - group_no - 2];
		new_grpColIdx[context->numGroupCols - 1] =
			new_grpColIdx[context->numGroupCols - group_no - 1];
		new_grpOperators[context->numGroupCols - 1] =
			new_grpOperators[context->numGroupCols - group_no - 1];
		new_grpColIdx[context->numGroupCols - group_no - 2] = last_grpCol;
		new_grpOperators[context->numGroupCols - group_no - 2] = last_grpOperator;
		new_grpColIdx[context->numGroupCols - group_no - 1] = last_grpidCol;
		new_grpOperators[context->numGroupCols - group_no - 1] = last_grpidOperator;
		
		agg_node = add_second_stage_agg(root, context->is_agg, context->tlist, current_tlist,
										current_qual, context->aggstrategy,
										context->numGroupCols, new_grpColIdx, new_grpOperators,
										group_no, input_grouping, grouping,
										(need_repeat_node ?
										 context->current_rollup->grpset_counts[group_no] : 0),
										current_numGroups,
										context->agg_counts->numAggs,
										context->agg_counts->transitionSpace,
										"rollup",
										&context->current_pathkeys,
										agg_node, false, false);

		/* Set inputHasGrouping */
		((Agg *)agg_node)->inputHasGrouping = true;

		context->querynode_changed = true;

		current_lefttree = agg_node;
		context->tlist = copyObject(current_tlist);
		input_grouping = grouping;
	}
	
	/*
	 * If we need an additional Agg node, we add it here. If the flow
	 * is not a SINGLETON, we redistribute the data and add the last
	 * Agg node to do the final stage of aggregation.
	 */
	if (need_finalagg)
	{
		long lNumGroups;
		int key_no;
		List *groupExprs = NIL;

		Path dummy_path;
		RelOptInfo dummy_parent;

		if (agg_node->flow != NULL &&
			agg_node->flow->flotype != FLOW_SINGLETON)
		{
			double motion_cost_per_row = (gp_motion_cost_per_row > 0.0) ?
				gp_motion_cost_per_row :
				2.0 * cpu_tuple_cost;

			/* Build the redistribution hash keys */
			for (key_no=0; key_no<context->numGroupCols; key_no++)
			{
				TargetEntry *tle = get_tle_by_resno(agg_node->targetlist,
													prelimGroupColIdx[key_no]);
				groupExprs = lappend(groupExprs, copyObject(tle->expr));
			}
		
			agg_node = (Plan *)make_motion_hash(root, agg_node, groupExprs);
			agg_node->total_cost += motion_cost_per_row * agg_node->plan_rows;
		}

		/*
		 * Determine if we want to use hash-based aggregation or
		 * sort-based aggregation. 
		 *
		 * First of all, construct a dummy path and its parent RelOptInfo to
		 * be passed into choose_hashed_grouping.
		 */
		dummy_path.parent = &dummy_parent;
		dummy_path.parent->rows = agg_node->plan_rows;
		dummy_path.parent->width = agg_node->plan_width;
		dummy_path.startup_cost = agg_node->startup_cost;
		dummy_path.total_cost = agg_node->total_cost;
		dummy_path.pathkeys = NIL;
		if (choose_hashed_grouping(root, context->tuple_fraction, -1.0, &dummy_path,
								   NULL, NULL, 0, *context->p_dNumGroups,
								   context->agg_counts))
			context->aggstrategy = AGG_HASHED;

		/* Add a sort node */
		if (context->aggstrategy == AGG_SORTED)
		{
			Oid		   *cmpOperators = palloc(context->numGroupCols * sizeof(Oid));
			bool	   *nullsFirst = palloc(context->numGroupCols * sizeof(bool));
			int			i;

			for (i = 0; i < context->numGroupCols; i++)
			{
				cmpOperators[i] = get_ordering_op_for_equality_op(prelimGroupOperators[i], false);
				nullsFirst[i] = false;
			}

			agg_node = (Plan *) make_sort(root, agg_node, context->numGroupCols,
										  prelimGroupColIdx,
										  cmpOperators,
										  nullsFirst, -1.0);
			mark_sort_locus(agg_node);
		}

		/* Convert # groups to long int --- but 'ware overflow! */
		lNumGroups = (long) Min(*context->p_dNumGroups, (double) LONG_MAX);

		/* Add a Agg/Group node */
		agg_node = add_second_stage_agg(root, context->is_agg, context->tlist, tlist3,
										qual3, context->aggstrategy,
										context->numGroupCols,
										prelimGroupColIdx, prelimGroupOperators,
										0,  /* num_nullcols */
										0, /* input_grouping */
										0, /* grouping */
										(need_repeat_node ? 1 : 0), /* rollup_gs_times */
										lNumGroups, context->agg_counts->numAggs,
										context->agg_counts->transitionSpace,
										"rollup", &context->current_pathkeys,
										agg_node,
										false,
										false);

		if (context->aggstrategy == AGG_HASHED)
			context->current_pathkeys = NIL; /* No longer sorted */

		((Agg *)agg_node)->inputHasGrouping = true;

		context->querynode_changed = true;
		context->tlist = copyObject(tlist3);
	}

	if (!context->twostage)
	{
		((Agg *)agg_node)->lastAgg = true;

		if (need_repeat_node)
			agg_node = add_repeat_node(agg_node, 0, 0);
	}

	pfree(orig_grpColIdx);
	pfree(orig_grpOperators);

	return agg_node;
}

/*
 * Add an Append node for a given list of subplans.
 *
 * If there is only 1 subplan in the given list, no Append node is
 * added and the first subplan is returned.
 */
static Plan *
add_append_node(PlannerInfo *root, List *subplans, GroupExtContext *context)
{
	Plan *result_plan;
	GpSetOpType optype = PSETOP_NONE;
	
	/* Add 'Append' if there are more than one subplan. */
	if (list_length(subplans) > 1)
	{
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			optype = choose_setop_type(subplans);
			adjust_setop_arguments(root, subplans, optype);
		}
		
		/* Append all agg_plans together */
		result_plan = (Plan *)make_append(subplans, false, context->tlist);
		mark_append_locus(result_plan, optype); /* Mark the plan result locus. */

		/* set the final pathkey to NIL */
		context->current_pathkeys = NIL;
	}
	
	else
	{
		result_plan = (Plan *)linitial(subplans);
	}

	return result_plan;
}
				

/*
 * Create an agg plan by using an Append node to glue together a list
 * of Agg/Group nodes, each of which represents an Agg/Group node in a
 * rollup level.
 *
 * This function generates two different Agg/Group plan for each
 * rollup level: one is by bringing all data into a single QE and
 * let it handle all distinct-qualified aggregates; the other is
 * by calling cdb_grouping_planner to obtain a plan for rewritten
 * distinct-qualified aggregates. This function chooses a cheaper
 * one from these two. The former plan is also the fallback plan.
 */
static Plan*
make_append_aggs_for_rollup(PlannerInfo *root,
							GroupExtContext *context,
							Plan *lefttree)
{
	Plan *result_plan = NULL;
	Plan *rewrite_agg_plan = NULL;
	Plan *gather_agg_plan = NULL;
	Query *query_for_gather = NULL;
	List *pathkeys_for_gather = NULL;
	GroupExtContext context_copy = { };
	double numGroups = *(context->p_dNumGroups);
	double numGroups_for_gather = 0;
	bool has_ordered_aggs = list_length(context->agg_counts->aggOrder) > 0;

	root->group_pathkeys = canonicalize_pathkeys(root, root->group_pathkeys);

	/* Plan a list of plans, one for each rollup level. */
	if (root->config->gp_enable_groupext_distinct_gather ||
		!root->config->gp_enable_groupext_distinct_pruning ||
		has_ordered_aggs )
	{
		memcpy(&context_copy, context, sizeof(GroupExtContext));
		gather_agg_plan =
			plan_append_aggs_with_gather(root, &context_copy, lefttree);
		if (gp_enable_groupext_distinct_pruning)
		{
			query_for_gather = copyObject(root->parse);
			pathkeys_for_gather = context->current_pathkeys;
			numGroups_for_gather = *(context->p_dNumGroups);
		}
	}
	
	if (gp_enable_groupext_distinct_pruning && !has_ordered_aggs)
	{
		*(context->p_dNumGroups) = numGroups;
		rewrite_agg_plan =
			plan_append_aggs_with_rewrite(root, context, lefttree);
	}

	result_plan = gather_agg_plan;
	if (result_plan == NULL)
	{
		result_plan = rewrite_agg_plan;
	}
	
	else if (rewrite_agg_plan != NULL)
	{
		/* Choose a cheaper plan */
		if (!root->config->enable_groupagg
				|| rewrite_agg_plan->total_cost < gather_agg_plan->total_cost)
		{
			result_plan = rewrite_agg_plan;
		}
		else if (rewrite_agg_plan->total_cost == gather_agg_plan->total_cost
				&& rewrite_agg_plan->startup_cost < rewrite_agg_plan->startup_cost)
		{
			result_plan = rewrite_agg_plan;
		}
	}

	if (result_plan == gather_agg_plan &&
		rewrite_agg_plan != NULL)
	{
		root->parse = query_for_gather;
		memcpy(context, &context_copy, sizeof(GroupExtContext));
		*(context->p_dNumGroups) = numGroups_for_gather;
		context->current_pathkeys = pathkeys_for_gather;
	}

	return result_plan;
}

/*
 * Construct a given grouping set into the form of a canonical grouping
 * sets.
 */
static CanonicalGroupingSets *
reconstruct_groupingsets(CanonicalRollup *rollup, int grpset_no)
{
	CanonicalGroupingSets *grpsets = palloc(sizeof(CanonicalGroupingSets));
	int group_no;
	
	grpsets->num_distcols = rollup->numcols - grpset_no;
	grpsets->ngrpsets = 1;
	grpsets->grpsets = (Bitmapset **)palloc0(sizeof(Bitmapset*));
	grpsets->grpset_counts = (int *)palloc0(sizeof(int));
	grpsets->grpset_counts[0] =	rollup->grpset_counts[grpset_no];

	for (group_no = 0; group_no < rollup->numcols - grpset_no; group_no++)
		grpsets->grpsets[0] = bms_add_member(grpsets->grpsets[0], rollup->colIdx[group_no]);
			
	return grpsets;
}

/*
 * Add the cost of computing DISTINCTs inside an Agg node.
 */
static void
add_distinct_cost(Plan *agg_plan, GroupExtContext *context)
{
	ListCell *dqa_lc;
	
	double num_groups = *context->p_dNumGroups;
	double avgsize = agg_plan->plan_rows / num_groups;
	foreach (dqa_lc, context->agg_counts->dqaArgs)
	{
		Path path_dummy;
		Node *dqa_expr = (Node *)lfirst(dqa_lc);
		
		cost_sort(&path_dummy, NULL, NIL, 0.0, avgsize,
				  get_typavgwidth(exprType(dqa_expr), exprTypmod(dqa_expr)), -1.0);
		agg_plan->total_cost += path_dummy.total_cost;
	}
}

/*
 * Generate a plan for a non-extension distinct-qualified query.
 *
 * We calls cdb_grouping_planner to generate a one, two, three-stage
 * plan for this query. When it is not necessary to generate such a plan, we fall
 * back to the sequential planning -- a simple Agg node.
 */
static Plan *
generate_dqa_plan(PlannerInfo *root,
				  GroupExtContext *context,
				  int rollup_level,
				  uint64 grouping,
				  List *subplans,
				  int subplan_no)
{
	Plan *agg_plan = NULL;
	Query *orig_query = root->parse;
		
	List *pathkeys = (List *)list_nth(context->pathkeys, subplan_no);
	List *pathkeys_copy = canonicalize_pathkeys(root, copyObject(pathkeys));
	List *orig_groupClause = root->parse->groupClause;
	
	Plan *subplan = (Plan *)list_nth(subplans, subplan_no);
	CanonicalGroupingSets *canonical_grpsets;
	List *new_groupClause;
	List *new_tlist = copyObject(context->tlist);
	List *new_qual = copyObject(context->qual);
	GroupContext group_context;
	List *orig_group_pathkeys = root->group_pathkeys;
	
	root->parse = copyObject(orig_query);

	/*
	 * Use cdb_grouping_planner to generate a plan for this
	 * rollup level.
	 */
	canonical_grpsets =
		reconstruct_groupingsets(context->current_rollup,
								 rollup_level);

	/*
	 * Reconstruct the root->groupClause for this rollup.
	 */
	new_groupClause =
		reconstruct_group_clause(orig_groupClause, context->sub_tlist,
								 context->grpColIdx,
								 context->numGroupCols - rollup_level);
	root->parse->groupClause = new_groupClause;

	/*
	 * Rewrite the targetlist and qual by replacing the removed grouping
	 * columns to NULLs.
	 */
	if (rollup_level > 0)
	{
		new_tlist =	(List *)replace_grouping_columns((Node *)new_tlist,
													 context->sub_tlist,
													 context->grpColIdx,
													 context->numGroupCols - rollup_level,
													 context->numGroupCols - 1, true);
		Assert(IsA(new_tlist, List));
		
		new_qual = (List *)replace_grouping_columns((Node *)new_qual,
													context->sub_tlist,
													context->grpColIdx,
													context->numGroupCols - rollup_level,
													context->numGroupCols - 1, false);
		Assert(new_qual == NULL || IsA(new_qual, List));
		
		root->group_pathkeys =
			make_pathkeys_for_groupclause(root, root->parse->groupClause,
										  context->sub_tlist);
		root->group_pathkeys = canonicalize_pathkeys(root, root->group_pathkeys);
	}

	root->parse->havingQual = (Node *)new_qual;
	
	group_context.best_path = context->path;
	group_context.cheapest_path = context->path;
	group_context.subplan = subplan;
	group_context.tlist = new_tlist;
	group_context.use_hashed_grouping = context->use_hashed_grouping;
	group_context.tuple_fraction = context->tuple_fraction;
	group_context.canonical_grpsets = canonical_grpsets;
	group_context.grouping = grouping;
	group_context.numGroupCols = context->numGroupCols - rollup_level;
	group_context.groupColIdx = context->grpColIdx;
	group_context.groupOperators = context->grpOperators;
	group_context.numDistinctCols = context->numDistinctCols;
	group_context.distinctColIdx = context->distinctColIdx;
	group_context.p_dNumGroups = context->p_dNumGroups;
	group_context.pcurrent_pathkeys = &pathkeys;
	group_context.querynode_changed = &(context->querynode_changed);
	agg_plan = cdb_grouping_planner(root, context->agg_counts, &group_context);

	if (agg_plan == NULL)
	{
		root->parse = orig_query;
		root->parse->groupClause = new_groupClause;
		root->parse->havingQual = (Node *)new_qual;

		/* Add an explicit sort if we couldn't make the path
		 * come out the way the Agg/Group node needs it.
		 */
		if (!pathkeys_contained_in(root->group_pathkeys,
								   pathkeys_copy))
		{
			subplan = (Plan *)
				make_sort_from_pathkeys(root, subplan, root->group_pathkeys,
										-1, false);
			pathkeys_copy = root->group_pathkeys;
			mark_sort_locus(subplan);
			
			/* If this is a Group node, pass DISTINCT to sort */
			if (!context->is_agg &&
				IsA(subplan, Sort) &&
				gp_enable_sort_distinct)
			{
				Sort* pSort = (Sort*)subplan;
				pSort->noduplicates = true;
			}
		}
		
		context->aggstrategy = AGG_SORTED;

		/* Add an Agg node */
		agg_plan = add_first_agg(root, context,
								 rollup_level,
								 0,
								 grouping,
								 ((context->current_rollup->grpset_counts[rollup_level] > 1) ?
								  context->current_rollup->grpset_counts[rollup_level] : 0),
								 context->numGroupCols,
								 context->grpColIdx,
								 context->grpOperators,
								 new_tlist,
								 context->qual,
								 subplan);

		Assert(agg_plan != NULL);
		Assert(canonical_grpsets->ngrpsets == 1);

		/*
		 * The function cost_agg does not cost DISTINCT portion of the work, when
		 * the Agg node also handles the distinct-qualified aggregates. We add
		 * this portion of cost here.
		 */
		add_distinct_cost(agg_plan, context);
		
		if (canonical_grpsets->grpset_counts != NULL &&
			canonical_grpsets->grpset_counts[0] > 1)
		{
			agg_plan = add_repeat_node(agg_plan,
									   canonical_grpsets->grpset_counts[0],
									   0);
		}

		context->current_pathkeys = pathkeys_copy;
	}
	else 
	{
		/*
		 * Remove entries in the flow->hashExpr that do not appear in
		 * the target list. These entries should be those replaced grouping
		 * columns. In the other words, they should appear in the original
		 * target list.
		 */
		if (agg_plan->flow != NULL &&
			agg_plan->flow->hashExpr != NULL)
		{
			ListCell *hashexpr_lc;
			List *new_hashExpr = NIL;
			
			foreach (hashexpr_lc, agg_plan->flow->hashExpr)
			{
				Node *hashexpr = lfirst(hashexpr_lc);
				
				if (tlist_member(hashexpr, agg_plan->targetlist))
				{
					new_hashExpr = lappend(new_hashExpr, hashexpr);
				}
				else
				{
					Assert(tlist_member(hashexpr, context->tlist));
				}
			}

			list_free(agg_plan->flow->hashExpr);
			agg_plan->flow->hashExpr = new_hashExpr;
		}

		/*
		 * Add Repeat node if needed. Note that if the agg_plan is not
		 * an Agg node, and its target list or qual contains GroupingFunc,
		 * we have to compute GroupingFunc inside the Repeat node. So tell
		 * the Repeat node about that.
		 */
		if (!IsA(agg_plan, Agg) &&
			(grouping > 0 &&
			 (contain_groupingfunc((Node *)agg_plan->targetlist) ||
			  contain_groupingfunc((Node *)agg_plan->qual))))
			agg_plan = add_repeat_node(agg_plan,
									   canonical_grpsets->grpset_counts[0],
									   grouping);
		else if (canonical_grpsets->grpset_counts != NULL &&
				 canonical_grpsets->grpset_counts[0] > 1)
		{
			agg_plan = add_repeat_node(agg_plan,
									   canonical_grpsets->grpset_counts[0],
									   0);
		}

		context->current_pathkeys = pathkeys;
	}

	free_canonical_groupingsets(canonical_grpsets);

	root->group_pathkeys = orig_group_pathkeys;
	return agg_plan;
}

/*
 * Generate a list of Agg/Group nodes, all of which share the same 
 * source plan that has a Gather motion on top to bring all data
 * to a single QE to compute distinct-qualified aggregates.
 */
static Plan *
plan_append_aggs_with_gather(PlannerInfo *root,
							 GroupExtContext *context,
							 Plan *lefttree)
{
	Plan *result_plan;
	Plan *gather_subplan = NULL;
	List *agg_plans = NIL;
	int group_no;
	uint64 grouping = 0;
	List *subplans = NIL;
	int num_subplans = 0;
	int subplan_no;
	AttrNumber *orig_grpColIdx;
	Oid		   *orig_grpOperators;
	double      motion_cost_per_row;
	List	   *group_pathkeys;

	orig_grpColIdx = context->grpColIdx;
	orig_grpOperators = context->grpOperators;
	context->grpColIdx = context->current_rollup->colIdx;
	context->grpOperators = context->current_rollup->operators;

	group_pathkeys = reorder_pathkeys(root,
									  root->group_pathkeys,
									  orig_grpColIdx,
									  context->grpColIdx);

	/* Add an explicit sort if we couldn't make the path
	 * come out the way the Agg/Group node needs it.
	 */
	if (!pathkeys_contained_in(group_pathkeys,
							   context->current_pathkeys))
	{
		lefttree = (Plan *)
			make_sort_from_pathkeys(root, lefttree, group_pathkeys,
									-1, false);
		context->current_pathkeys = group_pathkeys;
		mark_sort_locus(lefttree);

		/* If this is a Group node, pass DISTINCT to sort */
		if (!context->is_agg &&
			IsA(lefttree, Sort) &&
			gp_enable_sort_distinct)
		{
			Sort* pSort = (Sort*)lefttree;
			pSort->noduplicates = true;
		}
	}

	/* Precondition the input by adjusting its locus prior to adding
	 * the Agg or Group node to the base plan, if needed.
	 *
	 * In accounting for cost of motion, our rule is no increase in startup
	 * cost (doesn't sound right, but that's what to use for now) and a
	 * per row cost added to total cost (also doubtful).
	 */
	motion_cost_per_row = (gp_motion_cost_per_row > 0.0) ?
		gp_motion_cost_per_row :
		2.0 * cpu_tuple_cost;
	
	/* The common source plan is a Gather merge node on top of the given
	 * subplan
	 */
	gather_subplan = lefttree;
	Assert(lefttree->flow != NULL);
	if (lefttree->flow->flotype != FLOW_SINGLETON)
	{
		gather_subplan =
			(Plan *)make_motion_gather_to_QE(root, lefttree, context->current_pathkeys);
		gather_subplan->total_cost +=
			motion_cost_per_row * gather_subplan->plan_rows;
	}
	
	context->aggstrategy = AGG_SORTED; /* set the strategy to AGG_SORTED */

	/*
	 * Generate subplans for Agg nodes in each requested rollup level.
	 * Input sharing should be applied here by constructing appropriate
	 * subplans to be used.
	 */
	context->subplan = gather_subplan;
	context->path->locus.partkey_h = copyObject(context->input_locus.partkey_h);
	context->path->locus.partkey_oj = copyObject(context->input_locus.partkey_oj);
	context->pathkeys = NIL;

	/* Compute how many number of subplans is needed. */
	for (group_no=0, subplan_no=0; group_no<context->current_rollup->numcols + 1; group_no++)
	{
		if (context->current_rollup->grpset_counts[group_no] > 0)
			num_subplans++;
	}
		
	subplans = generate_list_subplans(root, num_subplans, context);

	/* Append Grouping and GroupId into targetlist */
	context->tlist = add_grouping_groupid(context->tlist);

	/* Generate an Agg/Group node for each rollup level */
	for (group_no=0, subplan_no=0; group_no<context->current_rollup->numcols + 1; group_no++)
	{
		if (group_no > 0)
			grouping += ( ((uint64)1) << context->current_rollup->grping_pos[
							 context->current_rollup->numcols - group_no]);

		if (context->current_rollup->grpset_counts[group_no] > 0)
		{
			Plan *agg_plan ;
			Plan *subplan = (Plan *)list_nth(subplans, subplan_no);
			List *pathkeys = (List *)list_nth(context->pathkeys, subplan_no);
			subplan_no++;

			/* Add an explicit sort if we couldn't make the path
			 * come out the way the Agg/Group node needs it.
			 */
			if (!pathkeys_contained_in(group_pathkeys,
									   pathkeys))
			{
				subplan = (Plan *)
					make_sort_from_pathkeys(root, subplan, group_pathkeys,
											-1, false);
				mark_sort_locus(subplan);

				/* If this is a Group node, pass DISTINCT to sort */
				if (!context->is_agg &&
					IsA(subplan, Sort) &&
					gp_enable_sort_distinct)
				{
					Sort* pSort = (Sort*)subplan;
					pSort->noduplicates = true;
				}
			}

			/* Add an Agg node */
			agg_plan = add_first_agg(root, context,
									 group_no,
									 0,
									 grouping,
									 ((context->current_rollup->grpset_counts[group_no] > 1) ?
									  context->current_rollup->grpset_counts[group_no] : 0),
									 context->numGroupCols,
									 context->grpColIdx,
									 context->grpOperators,
									 copyObject(context->tlist),
									 context->qual,
									 subplan);

			/*
			 * The function cost_agg does not cost DISTINCT portion of the work, when
			 * the Agg node also handles the distinct-qualified aggregates. We add
			 * this portion of cost here.
			 */
			add_distinct_cost(agg_plan, context);

			if (context->current_rollup->grpset_counts != NULL &&
				context->current_rollup->grpset_counts[group_no] > 1)
			{
				agg_plan = add_repeat_node(agg_plan,
										   context->current_rollup->grpset_counts[group_no],
										   0);
			}

			agg_plans = lappend(agg_plans, agg_plan);
			context->current_pathkeys = pathkeys;
		}
	}

	result_plan = add_append_node(root, agg_plans, context);

	return result_plan;
}


/*
 * Generate a plan which appends subplans for each individual
 * rollup level. Each of the subplans is generated through
 * the rewrite of distinct-qualified queries.
 */
static Plan *
plan_append_aggs_with_rewrite(PlannerInfo *root,
							  GroupExtContext *context,
							  Plan *lefttree)
{
	Plan *result_plan;
	List *agg_plans = NIL;
	int group_no;
	List *subplans = NIL;
	Query *orig_query = root->parse;
	struct RelOptInfo **orig_rel_array = root->simple_rel_array;
	int orig_rel_array_size = root->simple_rel_array_size;
	Query *final_query = NULL;
	uint64 grouping = 0;
	int num_subplans = 0;
	int subplan_no = 0;
	double orig_numGroups = *context->p_dNumGroups;
	double new_numGroups = 0;

	context->grpColIdx = context->current_rollup->colIdx;
	context->grpOperators = context->current_rollup->operators;

	/*
	 * Generate subplans for Agg nodes in each requested rollup level.
	 * Input sharing should be applied here by constructing appropriate
	 * subplans to be used.
	 */
	context->subplan = lefttree;
	context->path->locus.partkey_h = copyObject(context->input_locus.partkey_h);
	context->path->locus.partkey_oj = copyObject(context->input_locus.partkey_oj);
	context->pathkeys = NIL;

	/* Compute how many subplans is needed. */
	for (group_no=0; group_no<context->current_rollup->numcols + 1; group_no++)
	{
		if (context->current_rollup->grpset_counts[group_no] > 0)
			num_subplans++;
	}
	
	subplans = generate_list_subplans(root, num_subplans, context);

	/* Append Grouping and GroupId into context->tlist */
	context->tlist = add_grouping_groupid(context->tlist);

	/* Generate an Agg/Group node for each rollup level */
	for (group_no=0; group_no<context->current_rollup->numcols + 1; group_no++)
	{
		Plan *agg_plan = NULL;
		
		if (group_no > 0)
			grouping += ( ((uint64)1) << context->current_rollup->grping_pos[
							  context->current_rollup->numcols - group_no]);

		if (context->current_rollup->grpset_counts[group_no] <= 0)
			continue;

		context->curr_grpset_no++;

		/* Estimate the number of groups for this rollup level. */
		*context->p_dNumGroups =
			(((double)(context->current_rollup->numcols + 1 - group_no)) /
			 ((double)(context->current_rollup->numcols + 1))) *
			orig_numGroups;
		if (*context->p_dNumGroups == 0)
			*context->p_dNumGroups = 1;

		/* Install a replica of the original query and simple_rel_array
		 * cache.  This addresses failure of calls to generate_dqa_plan() 
		 * after the first as reported in MPP-6756.  Early calls "mess up"
		 * the query and rel array.  Later calls trip over the mess.  (Note 
		 * that they remain "messed up" following the final call.  I don't 
		 * think this will cause a problem.)  - Brian
		 */
		root->parse = copyObject(orig_query);
		if ( orig_rel_array && orig_rel_array_size )
		{
			size_t sz = orig_rel_array_size * sizeof(RelOptInfo *);
			root->simple_rel_array_size = orig_rel_array_size;
			root->simple_rel_array = (RelOptInfo **)palloc0(sz);
			memcpy(root->simple_rel_array, orig_rel_array, sz);
		}
		else
		{
			root->simple_rel_array = orig_rel_array;
			root->simple_rel_array_size = orig_rel_array_size;
		}

		agg_plan = generate_dqa_plan(root, context, group_no, grouping, subplans, subplan_no);
		subplan_no++;
		
		if (agg_plan == NULL)
			continue;
		
		new_numGroups += *context->p_dNumGroups;

		/*
		 * Since the range table for each rollup plan may be different,
		 * we build a new range table to include all range table entries.
		 */
		if (final_query != NULL)
		{
			RangeTblEntry *newrte;
			List *subquery_tlist = NIL;
			ListCell *lc;
			int *resno_map;

			/* addRangeTableEntryForSubquery requires the resnames in 
			 * the targetlist to be not NULL.
			 */
			foreach (lc, root->parse->targetList)
			{
				TargetEntry *tle = (TargetEntry *)lfirst(lc);
				if (tle->resname == NULL)
				{
					const char *fmt = "unnamed_attr_%d";
					char buf[32]; /* big enuf for fmt */
					snprintf(buf,32, fmt, tle->resno);
					tle->resname = pstrdup(buf);
				}
			}

			/* Since the range table for each rollup plan may be different,
			 * we build a new range table to include all range table entries.
			 */
			newrte = addRangeTableEntryForSubquery(NULL, root->parse,
												   makeAlias("rollup", NULL),
												   TRUE);
			final_query->rtable = lappend(final_query->rtable, newrte);
			subquery_tlist = generate_subquery_tlist(list_length(final_query->rtable),
													 agg_plan->targetlist, true, &resno_map);
			agg_plan = (Plan *)make_subqueryscan(root, subquery_tlist,
												 NIL,
												 list_length(final_query->rtable),
												 agg_plan,
												 final_query->rtable);
			mark_passthru_locus(agg_plan, true, true);
			pfree(resno_map);
		}

		else
		{
			final_query = root->parse;
		}

		agg_plans = lappend(agg_plans, agg_plan);
	}

	*context->p_dNumGroups = new_numGroups;

	/* Update the original Query node */
	root->parse = orig_query;
	root->simple_rel_array = orig_rel_array;
	root->simple_rel_array_size = orig_rel_array_size;
	memcpy(root->parse, final_query, sizeof(Query));

	result_plan = add_append_node(root, agg_plans, context);
	
	return result_plan;
}

/*
 * Create the first Agg node whose subplan is not an Agg node.
 *
 * Params:
 *    num_nullcols: the number of grouping colummns should not be considered
 *                  as a grouping column in this Agg node. For an input tuple
 *                  in this Agg node, the values for these grouping columns
 *                  will be set to NULL.
 *    input_grouping: the grouping value for input tuples.
 *    grouping: the grouping value for the Agg node.
 *    rollup_gs_times: the times that each aggregate result is outputed.
 *    groupColIdx: the grouping column.
 *    groupOperators: the sort operators corresponding groupColIdx.
 *    current_tlist: the targetlist for the Agg node to be generated.
 *    current_qual: the qual for the Agg node.
 *    current_lefttree: the subplan for the Agg node.
 *
 * Note that we always assume that groupColIdx includes GROUPING column
 * at the end, and context->numGroupCols specifies the number of grouping
 * columns. However, the result Agg node does not consider GROUPING column
 * because its subplan has no idea about this column.
 */
static Plan *
add_first_agg(PlannerInfo *root,
			  GroupExtContext *context,
			  int num_nullcols,
			  uint64 input_grouping,
			  uint64 grouping,
			  int rollup_gs_times,
			  int numGroupCols,
			  AttrNumber *groupColIdx,
			  Oid *groupOperators,
			  List *current_tlist,
			  List *current_qual,
			  Plan *current_lefttree)
{
	double current_numGroups;
	Plan *agg_node;

	/*
	 * XXX We estimate number of groups for each rollup level using
	 * a "straight-line interpolation" approach. Consider there
	 * are N groups for (a,b,c). We consider that there are (2/3)N
	 * groups for (a,b), (1/3)N groups for (a), and 1 group for ().
	 * We also consider the cost for a pass-through tuple as 0.
	 */
	current_numGroups =
		(((double)(numGroupCols-num_nullcols))/((double)(numGroupCols))) *
		*context->p_dNumGroups;
	if (current_numGroups == 0)
		current_numGroups = 1;

	*context->p_dNumGroups += current_numGroups;

	/* convert current_numGroups to long int */
	long lNumGroups = (long) Min(current_numGroups, (double) LONG_MAX);

	/* For the first Agg node, we don't really want to consider
	 * GROUPING in groupColIdx, which is the last entry in groupColIdx.
	 * The parameter numGroupCols should have excluded the GROUPING column.
	 */
	agg_node = (Plan *)make_agg(root, current_tlist, current_qual, context->aggstrategy, false,
			numGroupCols, groupColIdx, groupOperators,
			lNumGroups, num_nullcols, input_grouping, grouping,
			rollup_gs_times,
			context->agg_counts->numAggs, context->agg_counts->transitionSpace,
			current_lefttree);

	/* Pull up the Flow from the subplan */
	agg_node->flow = pull_up_Flow(agg_node, agg_node->lefttree);

	/* Simply pulling up the Flow from the subplan is not enough.
	 * We set hash->hashExpr to NULL since we can not be sure that
	 * the order of all entries in hash->hashExpr will match with
	 * the agg_node->targetlist, which is required by the upper-level
	 * Agg nodes, see also mark_passthru_locus(). This can not
	 * be done by simplying re-order the entries in flow->hashExpr since
	 * some grouping columns may not be in flow->hashExpr at all.
	 */
	list_free_deep(agg_node->flow->hashExpr);
	agg_node->flow->hashExpr = NULL;

	return agg_node;
}

/*
 * Generate a list of subplans based on context->subplan.
 */
static List *
generate_list_subplans(PlannerInfo *root, int num_subplans,
					   GroupExtContext *context)
{
	int group_no;
	List *subplans = NIL;
	Cost share_cost;
	Cost copy_cost;

	Assert(context->subplan != NULL);
	
	/*
	 * We consider both input sharing plan and a list of common
	 * subplan copies, and choose the one which has the cheaper
	 * cost.
	 */
	copy_cost = num_subplans * context->subplan->total_cost;
	share_cost = cost_share_plan(context->subplan, root, num_subplans);

	if (num_subplans == 1 || copy_cost > share_cost)
		subplans = share_plan(root, context->subplan, num_subplans);
	else
	{
		for (group_no = 0; group_no < num_subplans; group_no++)
		{
			Plan *copy_plan = copyObject(context->subplan);
			subplans = lappend(subplans, copy_plan);
		}
	}

	list_free(context->pathkeys);
	context->pathkeys = NIL;

	for(group_no = 0; group_no < num_subplans; ++group_no)
	{
		List *pathkeys = copyObject(context->current_pathkeys);
		pathkeys = canonicalize_pathkeys(root, pathkeys);
		context->pathkeys = lappend(context->pathkeys, pathkeys);
	}

	return subplans;
}

static void
destroyGroupExtContext(GroupExtContext *context)
{
	if (context->sub_tlist)
		list_free(context->sub_tlist);
	if (context->canonical_rollups)
		list_free_deep(context->canonical_rollups);
}

/*
 * Append Grouping and GroupId into the given target list.
 *
 * This function only appends Grouping and GroupId if they are not
 * the last two in the original targetlist.
 */
static List *
add_grouping_groupid(List *tlist)
{
	TargetEntry *tle;
	Grouping *grping;
	GroupId *grpid;

	if (list_length(tlist) >= 2)
	{
		TargetEntry *te1, *te2;
		te1 = get_tle_by_resno(tlist, list_length(tlist));
		te2 = get_tle_by_resno(tlist, list_length(tlist) - 1);
		
		Assert (te1->expr != NULL && te2->expr != NULL);
		if (IsA(te1->expr, GroupId) &&
			IsA(te2->expr, Grouping))
			return tlist;
	}

	grping = makeNode(Grouping);
	tle = makeTargetEntry((Expr *)grping, list_length(tlist) + 1,
						  "grouping", true);
	tle->ressortgroupref = assignSortGroupRef(tle, tlist);
	tlist = lappend(tlist, tle);

	grpid = makeNode(GroupId);
	tle = makeTargetEntry((Expr *)grpid, list_length(tlist) + 1,
						  "group_id", true);
	tle->ressortgroupref = assignSortGroupRef(tle, tlist);
	tlist = lappend(tlist, tle);

	return tlist;
}

/*
 * Convert a given list of canonical grouping sets into multiple
 * canonical rollups.
 *
 * The input list of canonical grouping sets is sorted.
 */
static List *
convert_gs_to_rollups(AttrNumber *grpColIdx, Oid *grpOperators,
					  int numGroupCols,
					  CanonicalGroupingSets *canonical_grpsets,
					  List *canonical_rollups,
					  List *sub_tlist,
					  List *tlist)
{
	bool *used;
	bool more = true;
	Index *sortrefs_to_resnos;
	ListCell *sub_lc;
	int max_sortgroupref = 0;
	int no;

	if (canonical_grpsets == NULL ||
		canonical_grpsets->ngrpsets == 0)
		return NIL;

	used = (bool *)palloc0(canonical_grpsets->ngrpsets * sizeof(bool));

	/* In canonical grouping sets, we use tleSortGroupRef to represent
	 * a grouping column. These tleSortGroupRefs are also computed
	 * based on both GROUP BY and ORDER BY clauses. However, grpColIdx
	 * stores the resno in the sub_tlist. Here, we construct a 
	 * mapping from the ressortgrouprefs to the resno in the sub_tlist,
	 * so that we could easily construct the grouping column index
	 * for each rollup later. The mapping array indexes represent
	 * the ressortgrouprefs, while the array elements represent the
	 * resno in the sub_tlist.
	 */
	max_sortgroupref = maxSortGroupRef(tlist, true);
	
	sortrefs_to_resnos = (Index *)palloc0(max_sortgroupref * sizeof(Index));
	
	foreach (sub_lc, sub_tlist)
	{
		TargetEntry *tle = (TargetEntry *)lfirst(sub_lc);
		if (tle->ressortgroupref > 0)
		{
			sortrefs_to_resnos[tle->ressortgroupref - 1] = tle->resno;
		}
	}

	/*
	 * In sortrefs_to_resnos, if there are any entries that are not set,
	 * it means that it is a sort key or it is a duplicate grouping key.
	 * We find its target entry in the tlist that has such a ressortgroupref
	 * value, and find its corresponding target entry in the sub_tlist, which
	 * resno is what we need.
	 */
	for (no = 0; no < max_sortgroupref; no++)
	{
		TargetEntry *tle = NULL;
		SortClause sc = { };
		
		if (sortrefs_to_resnos[no] > 0)
			continue;
		/*
		 * Find the target entry in tlist whose ressortgroupref is equal to (no+1),
		 * and find the corresponding target entry in sub_tlist. Fill in the
		 * resno.
		 */
		sc.tleSortGroupRef = no+1;
		tle = get_sortgroupclause_tle(&sc, tlist);
		foreach (sub_lc, sub_tlist)
		{
			TargetEntry *sub_tle = (TargetEntry *)lfirst(sub_lc);
			Assert(sub_tle);
			if (equal(sub_tle->expr, tle->expr))
			{
				Assert(tle->ressortgroupref - 1 == no);
				sortrefs_to_resnos[tle->ressortgroupref - 1] = sub_tle->resno;
				break;
			}
		}
	}

	/*
	 * XXX We use a simple greedy algorithm to make up the list
	 * of rollups. We scan over all grouping sets several times
	 * until there are no more grouping sets left. In each round,
	 * we pick up the maximum length of grouping sets for a
	 * single rollup. The grouping sets have been picked in the
	 * previous rounds are ignored in this round.
	 */
	while (more)
	{
		int grpsetno;
		int prev_grpsetno = -1;
		List *list_grpsetnos = NIL;
		ListCell *lc;
		int colno = 0;
		int size_rollup = sizeof(CanonicalRollup) +
			canonical_grpsets->num_distcols * sizeof(int);

		CanonicalRollup *rollup = (CanonicalRollup *)
			palloc0(size_rollup);
		rollup->numcols = canonical_grpsets->num_distcols;
		rollup->colIdx = (AttrNumber *)palloc0(rollup->numcols * sizeof(AttrNumber));
		rollup->operators = (Oid *)palloc0(rollup->numcols * sizeof(Oid));
		rollup->grping_pos = (int *)palloc0(rollup->numcols * sizeof(int));
		rollup->ngrpsets = 0;

		more = false;

		for (grpsetno=0; grpsetno<canonical_grpsets->ngrpsets; grpsetno++)
		{
			if (used[grpsetno])
				continue;

			if (prev_grpsetno == -1 ||
				bms_is_subset(canonical_grpsets->grpsets[grpsetno],
							  canonical_grpsets->grpsets[prev_grpsetno]))
			{
				int curr_grpset;
				curr_grpset = canonical_grpsets->num_distcols -
					bms_num_members(canonical_grpsets->grpsets[grpsetno]);
				Assert(curr_grpset >= 0);

				rollup->grpset_counts[curr_grpset] =
					canonical_grpsets->grpset_counts[grpsetno];
				rollup->ngrpsets++;
				used[grpsetno] = true;

				list_grpsetnos = lcons_int(grpsetno, list_grpsetnos);

				prev_grpsetno = grpsetno;
			}

			else
				more = true;
		}

		/* Build colIdx based on the grouping sets in the current ROLLUP. */
		prev_grpsetno = -1;
		foreach(lc, list_grpsetnos)
		{
			grpsetno = lfirst_int(lc);

			if (prev_grpsetno == -1)
			{
				append_colIdx(rollup->colIdx, rollup->operators, rollup->numcols, colno,
							  rollup->grping_pos, canonical_grpsets->grpsets[grpsetno],
							  grpColIdx, grpOperators, sortrefs_to_resnos, max_sortgroupref);
				prev_grpsetno = grpsetno;
				colno += bms_num_members(canonical_grpsets->grpsets[grpsetno]);
			}

			else
			{
				Bitmapset *bms_diff =
					bms_difference(canonical_grpsets->grpsets[grpsetno],
								   canonical_grpsets->grpsets[prev_grpsetno]);
				append_colIdx(rollup->colIdx, rollup->operators, rollup->numcols, colno,
							  rollup->grping_pos, bms_diff, grpColIdx, grpOperators,
							  sortrefs_to_resnos, max_sortgroupref);
				colno += bms_num_members(bms_diff);
				prev_grpsetno = grpsetno;
							  
			}
		}

		/* This rollup may not use all grouping columns in the user-defined
		 * grouping sets, we fill the blanks in colIdx with not-used columns.
		 */
		if (colno < rollup->numcols)
		{
			/*
			 * Record the index positions in grpColIdx in which the corresponding
			 * grouping columns are used.
			 */
			Bitmapset *used_poses = NULL;
			
			int rollup_colno;
			int grpcol_no;
			for(rollup_colno = 0; rollup_colno < colno; rollup_colno++)
			{
				for (grpcol_no = 0; grpcol_no < rollup->numcols; grpcol_no++)
				{
					if (grpColIdx[grpcol_no] == rollup->colIdx[rollup_colno] &&
						!bms_is_member(grpcol_no, used_poses))
					{
						used_poses = bms_add_member(used_poses, grpcol_no);
						break;
					}
				}
				Assert(grpcol_no < rollup->numcols);
			}
			
			for (grpcol_no = 0; grpcol_no < rollup->numcols; grpcol_no++)
			{
				if (bms_is_member(grpcol_no, used_poses))
					continue;
				rollup->colIdx[colno] = grpColIdx[grpcol_no];
				rollup->operators[colno] = grpOperators[grpcol_no];
				rollup->grping_pos[colno] = rollup->numcols - 1 - grpcol_no;
				colno++;

				used_poses = bms_add_member(used_poses, grpcol_no);
			}
			Assert(colno == rollup->numcols);

			bms_free(used_poses);
		}

		canonical_rollups = lappend(canonical_rollups, rollup);
	}

	pfree(sortrefs_to_resnos);

	return canonical_rollups;
}

/*
 * append_colIdx - append the elements in a Bitmapset into
 *   a given array, starting from colno.
 */
static void
append_colIdx(AttrNumber *colIdx, Oid *operators, int numcols, int colno,
			  int *grping_pos, Bitmapset *bms, AttrNumber *grpColIdx, Oid *grpOperators,
			  Index *sortrefs_to_resnos, int max_sortgroupref)
{
	int x;
	int pos;
	Assert(colno + bms_num_members(bms) <= numcols);

	/* x is the original tleSortGroupRef */
	x = bms_first_from(bms, 0);
	while (x >= 0)
	{
		Assert(x > 0 && x <= max_sortgroupref);

		/* Find the index position in grpColIdx for
		 * sortrefs_to_resnos[x - 1].
		 */
		for (pos = 0; pos < numcols; pos++)
		{
			if (sortrefs_to_resnos[x - 1] == grpColIdx[pos])
				break;
		}
		Assert(pos < numcols);

		colIdx[colno] = sortrefs_to_resnos[x - 1];
		operators[colno] = grpOperators[pos];
		grping_pos[colno] = numcols - 1 - pos;
		colno++;
		x = bms_first_from(bms, x + 1);
	}
}

static Plan *
plan_list_rollup_plans(PlannerInfo *root,
					   GroupExtContext *context,
					   Plan *lefttree)
{
	Plan *result_plan;
	GpSetOpType optype = PSETOP_NONE;
	List *rollup_plans = NIL;
	int rollup_no;
	List *subplans = NIL;
	List *orig_tlist = context->tlist;
	AttrNumber *orig_grpColIdx = context->grpColIdx;
	Oid *orig_grpOperators = context->grpOperators;
	int orig_numGroupCols = context->numGroupCols;
	double orig_numGroups = *context->p_dNumGroups;
	double new_numGroups = 0;
	Query *orig_query = root->parse;
	Query *final_query = NULL;
	List *pathkeys = NIL;
	bool orig_gp_enable_groupext_distinct_pruning = root->config->gp_enable_groupext_distinct_pruning;

	/*
	 * Augment the targetlist of lefttree with DQA arguments if
	 * any. If lefttree is not a projection capable plan, we insert a
	 * Result on top of it
	 */
	if (context->agg_counts->dqaArgs != NIL)
	{
		if (!is_projection_capable_plan(lefttree))
		{
			Plan *plan = (Plan *) make_result(root, lefttree->targetlist, NULL, lefttree);
			if (lefttree->flow)
				plan->flow = pull_up_Flow(plan, lefttree);
			lefttree = plan;
		}
	
		Assert(is_projection_capable_plan(lefttree));
		lefttree->targetlist = 
			augment_subplan_tlist(lefttree->targetlist,
								  context->agg_counts->dqaArgs,
								  &(context->numDistinctCols),
								  &(context->distinctColIdx),
								  true);
	}

	/* Generate the shared input subplans for all rollups. */
	context->subplan = lefttree;
	context->input_locus = context->path->locus;
	context->input_locus.partkey_h = copyObject(context->path->locus.partkey_h);
	context->input_locus.partkey_oj = copyObject(context->path->locus.partkey_oj);
	context->pathkeys = NIL;

	if (list_length(context->canonical_rollups) > 1)
	{
		subplans = generate_list_subplans(root,
										  list_length(context->canonical_rollups),
										  context);
	}
	else
	{
		subplans = lappend(subplans, lefttree);
		context->pathkeys = lappend(context->pathkeys,
									context->current_pathkeys);
	}

	pathkeys = context->pathkeys;
	context->tlist = NIL;

	/*
	 * If this is in a two-stage aggregation, the Repeat node will be
	 * added in the second stage. We need to set rollup_gs_times based
	 * on all ROLLUPs instead of individual ROLLUP.
	 */
	if (context->twostage)
	{
		for (rollup_no=0; rollup_no < list_length(context->canonical_rollups); rollup_no++)
		{
			CanonicalRollup *rollup = (CanonicalRollup *)
				list_nth(context->canonical_rollups, rollup_no);
			int group_no;
			
			for (group_no=0; group_no < rollup->numcols + 1; group_no++)
			{
				if (rollup->grpset_counts[group_no] > 1)
					context->need_repeat_node = true;
			}
		}
	}

	for (rollup_no=0; rollup_no<list_length(context->canonical_rollups); rollup_no++)
	{
		Plan *subplan = (Plan *)list_nth(subplans, rollup_no);
		Plan *rollup_plan;

		context->current_rollup = (CanonicalRollup *)
			list_nth(context->canonical_rollups, rollup_no);

		context->current_pathkeys = list_nth(pathkeys, rollup_no);

		context->tlist = copyObject(orig_tlist);
		context->numGroupCols = orig_numGroupCols;
		context->grpColIdx =
			(AttrNumber *)palloc0(context->numGroupCols * sizeof(AttrNumber));
		context->grpOperators =
			(Oid *) palloc0(context->numGroupCols * sizeof(Oid));
		memcpy(context->grpColIdx, orig_grpColIdx,
			   context->numGroupCols * sizeof(AttrNumber));
		memcpy(context->grpOperators, orig_grpOperators,
			   context->numGroupCols * sizeof(Oid));
		*context->p_dNumGroups = orig_numGroups;
		root->parse = copyObject(orig_query);

		if (gp_enable_groupext_distinct_pruning &&
			context->agg_counts->numDistinctAggs > 0 &&
			gp_distinct_grouping_sets_threshold > 0)
		{
			/*
			 * XXX The rewrite may generate a plan with a large number of slices, which
			 * can not be run by the existing GPDB architecture. We disable the rewrite
			 * for the rest of grouping sets when the number of grouping sets we have
			 * rewitten is greater than the value specified
			 * by "gp_distinct_grouping_sets_threshold" dividved by the number of
			 * distinct-qualified aggregates.
			 */
			if (context->curr_grpset_no >=
				gp_distinct_grouping_sets_threshold / context->agg_counts->numDistinctAggs)
				gp_enable_groupext_distinct_pruning = false;
		}

		rollup_plan = make_aggs_for_rollup(root, context, subplan);

		new_numGroups += *context->p_dNumGroups;

		if (rollup_no > 0 && list_length(context->canonical_rollups) > 1)
		{
			RangeTblEntry *newrte;
			List *subquery_tlist = NIL;
			ListCell *lc;
			int *resno_map;

			/* addRangeTableEntryForSubquery requires the resnames in 
			 * the targetlist to be not NULL.
			 */
			foreach (lc, root->parse->targetList)
			{
				TargetEntry *tle = (TargetEntry *)lfirst(lc);
				if (tle->resname == NULL)
				{
					const char *fmt = "unnamed_attr_%d";
					char buf[32]; /* big enuf for fmt */
					snprintf(buf,32, fmt, tle->resno);
					tle->resname = pstrdup(buf);
				}
			}

			/* Since the range table for each rollup plan may be different,
			 * we build a new range table to include all range table entries.
			 */
			newrte = addRangeTableEntryForSubquery(NULL, root->parse,
												   makeAlias("rollup", NULL),
												   TRUE);
			final_query->rtable = lappend(final_query->rtable, newrte);
			subquery_tlist = generate_subquery_tlist(list_length(final_query->rtable),
													 rollup_plan->targetlist, true, &resno_map);
			rollup_plan = (Plan *)make_subqueryscan(root, subquery_tlist,
													NIL,
													list_length(final_query->rtable),
													rollup_plan,
													root->parse->rtable);
			mark_passthru_locus(rollup_plan, true, true);
			pfree(resno_map);
		}

		else
		{
			final_query = root->parse;
		}

		rollup_plans = lappend(rollup_plans, rollup_plan);
	}

	if (list_length(rollup_plans) > 1)
	{
		/* Decide on approach, condition argument plans to suit. */
		if ( Gp_role == GP_ROLE_DISPATCH )
		{
			optype = choose_setop_type(rollup_plans);
			adjust_setop_arguments(root, rollup_plans, optype);
		}

		/* Append all rollup_plans together */
		result_plan = (Plan *)make_append(rollup_plans, false, context->tlist);
		mark_append_locus(result_plan, optype); /* Mark the plan result locus. */

		/* set the final pathkey to NIL */
		context->current_pathkeys = NIL;
	}
	
	else
	{
		Assert (list_length(rollup_plans) == 1);
		result_plan = (Plan *)linitial(rollup_plans);
	}

	*context->p_dNumGroups = new_numGroups;
	gp_enable_groupext_distinct_pruning = orig_gp_enable_groupext_distinct_pruning;

	/* Update the original Query node */
	root->parse = orig_query;
	memcpy(root->parse, final_query, sizeof(Query));

	return result_plan;
}

static Node *
replace_grouping_columns_quals(Node *node, void *grpcols)
{
	ListCell *lc = NULL;
	
	if (node == NULL || IsA(node, Const))
		return node;

	Assert(IsA(grpcols, List));

	foreach (lc, (List*)grpcols)
	{
		Node *grpcol = lfirst(lc);
		if (equal(node, grpcol)) {

			/* Generate a NULL constant to replace the node. */
			Const *null = makeNullConst(exprType((Node *)grpcol), -1);
			return (Node *)null;
		}
	}

	return expression_tree_mutator(node, replace_grouping_columns_quals, grpcols);
}

static Node *
replace_grouping_columns_targetlist(Node *node, void *grpcols)
{
	ListCell *lc = NULL;
	
	if (node == NULL)
		return NULL;

	Assert(IsA(grpcols, List));

	foreach (lc, (List*)grpcols)
	{
		Node *grpcol = lfirst(lc);
		if (equal(node, grpcol)) {

			/* Generate a NULL constant to replace the node. */
			Const *null = makeNullConst(exprType((Node *)grpcol), -1);
			return (Node *)null;
		}
	}

	return node;
}


/*
 * Replace grouping columns with NULL constants in the given targetlist/quals
 */
static Node *
replace_grouping_columns(Node *node,
						 List *sub_tlist,
						 AttrNumber *grpColIdx,
						 int start_colno,
						 int end_colno,
						 bool is_targetlist)
{
	int attno;
	List *grpcols = NIL;
	List* new_node = NIL;
	ListCell* lc;

	Assert(start_colno <= end_colno);

	if (node == NULL)
		return NULL;
	
	/*
	 * Compute a list of grouping columns to be replaced
	 */
	for (attno = start_colno; attno <= end_colno; attno++)
	{
		TargetEntry *te = get_tle_by_resno(sub_tlist, grpColIdx[attno]);
		Assert(te != NULL);
		grpcols = lappend(grpcols, te->expr);
	}

	if(is_targetlist)
	{
		foreach(lc, (List*)node)
		{
			TargetEntry *te;
			te = copyObject(lfirst(lc));
			te->expr = (Expr *)replace_grouping_columns_targetlist((Node *)te->expr, grpcols);
			new_node = lappend(new_node, te);
		}
	}
	else
	{
		new_node = (List*)replace_grouping_columns_quals((Node *)node, grpcols);
	}

	list_free(grpcols);

	return (Node*)new_node;
}

typedef struct qual_context
{
	Plan *subplan;
	uint64 grouping;
} qual_context;

/*
 * Push down Aggref/Var in the node to the given subplan.
 *
 * This function makes sure that the qual for the upper node is still valid
 * even if it contains Aggrefs that do not appear in the subplan targetlist.
 * For example,
 *   select cn, count(vn) from sale group by grouping sets((cn),(cn),()) having count(pn) < 2;
 *
 * The 'count(pn)' only appears in the qual.
 */
static Node *
qual_pushdown_mutator(Node *node, void *ctx)
{
	qual_context *qual_ctx = (qual_context *)ctx;
	Plan *subplan = qual_ctx->subplan;
	
	if (node == NULL)
		return NULL;
	if (IsA(node, Var) ||
		IsA(node, Aggref) ||
		(qual_ctx->grouping == 0 && IsA(node, GroupingFunc)))
	{
		ListCell *lc;
		bool found = false;
		
		/*
		 * Search the subplan target list. If this Aggref/Var is not one of
		 * its entry, we push it into the subplan targetlist.
		 */
		foreach (lc, subplan->targetlist)
		{
			TargetEntry *te = (TargetEntry *)lfirst(lc);
			if (equal(node, te->expr))
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			TargetEntry *te = makeTargetEntry((Expr*)node,
											  list_length(subplan->targetlist) + 1,
											  "attr",
											  false);
			subplan->targetlist = lappend(subplan->targetlist, te);
		}

		return copyObject(node);
	}

	return expression_tree_mutator(node, qual_pushdown_mutator, ctx);
}

typedef struct expr_context
{
	List *list_exprs;
} expr_context;

static bool
flatten_expression(Node *expr, void *ctx)
{
	if (expr == NULL)
		return false;
		
	if (IsA(expr, Aggref) ||
		IsA(expr, Var))
	{
		((expr_context *)ctx)->list_exprs = lappend(((expr_context *)ctx)->list_exprs, expr);
	}

	return expression_tree_walker(expr, flatten_expression, ctx);
}


/*
 * Add a Repeat node on top of a given plan.
 *
 * When repeat_count is equal to 1, simple returns the original plan.
 *
 * When repeat_count is greater than 1, it represents each tuple
 * will be repeatly return that many times.
 *
 * When repeat_count is 0, it represents that each tuple will be repeated
 * several times based on an expression. Currently, this expression is
 * defined by the GROUP_ID attribute in the given result_plan targetlist.
 * Note that GROUP_ID may not appears in the given result_plan. However,
 * it sholud appears in a plan node upstream.
 */
Plan *
add_repeat_node(Plan *result_plan, int repeat_count, uint64 grouping)
{
	AttrNumber repeat_count_resno = 0;
	List *targetlist = NIL;
	ListCell *lc;
	List *qual;
	Expr *repeatCountExpr;
	qual_context ctx;

	Assert(result_plan != NULL);
	
	/*
	 * Pull up target entries from result_plan. Remember if we see
	 * any GROUP_ID attribute.
	 */
	foreach (lc, result_plan->targetlist)
	{
		TargetEntry *te = (TargetEntry *)lfirst(lc);
		Expr *new_expr = NULL;
		TargetEntry *new_te;

		new_expr = (Expr *)copyObject(te->expr);

		new_te = makeTargetEntry(new_expr, list_length(targetlist) + 1,
								 te->resname, te->resjunk);
		new_te->ressortgroupref = te->ressortgroupref;
		targetlist = lappend(targetlist, new_te);

		/*
		 * If GROUP_ID exists in the targetlist of result_plan, remember it,
		 * and mark this target entry as not-junk since the Repeat node needs
		 * it.
		 */
		if (repeat_count == 0 &&
			IsA(te->expr, GroupId) &&
			repeat_count_resno == 0)
		{
			repeat_count_resno = te->resno;
			te->resjunk = false;
		}
	}

	if (repeat_count == 0 &&
		repeat_count_resno == 0)
	{
		TargetEntry *te;
		
		/* The last column has to be GROUP_ID. Mark it as not-junk. */
		repeat_count_resno = list_length(result_plan->targetlist);
		te = get_tle_by_resno(result_plan->targetlist, repeat_count_resno);
		te->resjunk = false;
	}

	Assert(repeat_count >= 1 ||
		   repeat_count_resno > 0);

	if (repeat_count >= 1)
	{
		repeatCountExpr = (Expr *)makeConst(INT4OID, -1, sizeof(int32),
											Int32GetDatum(repeat_count),
											false, true);
	}
	else
	{
		repeatCountExpr = (Expr *)makeVar(OUTER,
										  repeat_count_resno,
										  INT4OID,
										  -1,
										  0);
	}

	Assert(exprType((Node *)repeatCountExpr) == INT4OID);

	/*
	 * When GroupingFunc will be evaluated in the Repeat node, we have to
	 * replace its entry in the result_plan targetlist. Otherwise,
	 * the post planning will replace GroupingFuncs in the Repeat node
	 * as Vars.
	 */
	if (grouping > 0)
	{
		foreach (lc, result_plan->targetlist)
		{
			TargetEntry *te = (TargetEntry *)lfirst(lc);
			if (contain_groupingfunc((Node *)te->expr))
			{
				ListCell *expr_lc;
				expr_context exp_ctx;

#ifdef USE_ASSERT_CHECKING
				bool no_exprs;
#endif

				exp_ctx.list_exprs = NIL;

#ifdef USE_ASSERT_CHECKING
				no_exprs =
#endif
				flatten_expression((Node *)te->expr, (void *)&exp_ctx);

				Assert(!no_exprs);
				if (list_length(exp_ctx.list_exprs) == 0)
				{
					te->expr = (Expr *)makeNullConst(exprType((Node *)te->expr), -1);
					continue;
				}

				expr_lc = list_head(exp_ctx.list_exprs);
				te->expr = (Expr *)lfirst(expr_lc);

				expr_lc = lnext(expr_lc);
				while(expr_lc != NULL)
				{
					TargetEntry *new_te =
						makeTargetEntry((Expr *)lfirst(expr_lc),
										list_length(result_plan->targetlist) + 1,
										"attr", false);
					result_plan->targetlist = lappend(result_plan->targetlist, new_te);
					expr_lc = lnext(expr_lc);
				}
			}
		}
	}

	/*
	 * We pull the qual in the result_plan up to the Repeat node, and let it
	 * be evaluated there. However, we may need to push down Aggrefs/Vars that
	 * only appear in the qual, but not the result_plan targetlist.
	 */
	ctx.subplan = result_plan;
	ctx.grouping = grouping;
	qual = (List *)qual_pushdown_mutator((Node *)result_plan->qual, (void *)&ctx);
	result_plan->qual = NULL;
	
	result_plan = (Plan *)
		make_repeat(targetlist, NULL,
					repeatCountExpr,
					grouping,
					result_plan);
	result_plan->qual = qual;
	mark_passthru_locus(result_plan, true, true);

	return result_plan;
}

static bool
contain_groupingfunc_walker(Node *node, void *context)
{
	if (node == NULL)
		return false;
	else if (IsA(node, GroupingFunc))
		return true;
	return expression_tree_walker(node,
								  contain_groupingfunc_walker,
								  context);
}

/*
 * Return true if the given node contains GroupingFunc.
 */
static bool
contain_groupingfunc(Node *node)
{
	return contain_groupingfunc_walker(node, NULL);
}

#ifdef DEBUG_GROUPING_SETS

/**
 * Textual representation of a canonical rollup for debugging purposes.
 */
char *canonicalRollupToString(CanonicalRollup *cr)
{
	StringInfoData str;
	int i = 0;
	
	initStringInfo(&str);
	
	appendStringInfo(&str, "(numcols=%d ", cr->numcols);
	
	appendStringInfo(&str, "colIdx=[");
	for (i=0;i<cr->numcols;i++)
	{
		appendStringInfo(&str, "%d ", cr->colIdx[i]);
	}
	appendStringInfo(&str, "] ");

	appendStringInfo(&str, "operators=[");
	for (i=0;i<cr->numcols;i++)
	{
		appendStringInfo(&str, "%u ", cr->operators[i]);
	}
	appendStringInfo(&str, "] ");

	appendStringInfo(&str, "grping_pos=[");
	for (i=0;i<cr->numcols;i++)
	{
		appendStringInfo(&str, "%d ", cr->grping_pos[i]);
	}
	appendStringInfo(&str, "] ");

	appendStringInfo(&str, "grpset_counts=[");
	for (i=0;i<cr->numcols + 1;i++)
	{
		appendStringInfo(&str, "%d ", cr->grpset_counts[i]);
	}
	appendStringInfo(&str, "] ");

	appendStringInfo(&str, "ngrpsets=%d)", cr->ngrpsets);

	return str.data;
}

/**
 * Textual representation of a list of canonical rollups for debugging purposes.
 */
char *canonicalRollupListToString(List *crl)
{
	StringInfoData str;
	int i = 0;
	ListCell *lc = NULL;
	
	initStringInfo(&str);

	foreach (lc, crl)
	{
		CanonicalRollup *cr = (CanonicalRollup *) lfirst(lc);
		appendStringInfo(&str, "%i: %s ", i, canonicalRollupToString(cr));
		i++;
	}
	
	return str.data;
}

/**
 * Textual representation of a bitmapset for debugging purposes.
 */
char *bitmapsetToString(Bitmapset *bms)
{
	StringInfoData str;
	int x = -1;

	initStringInfo(&str);

	Bitmapset *tmpset = bms_copy(bms);
	
	appendStringInfo(&str, "b(");
	while ((x = bms_first_member(tmpset)) >= 0)
	{
		appendStringInfo(&str, " %d ", x);
	}
	bms_free(tmpset);
	appendStringInfo(&str, ") ");

	return str.data;
}

/**
 * Textual representation of a canonical grouping set for debugging purposes.
 */
char *canonicalGroupingSetsToString(CanonicalGroupingSets *cgs)
{
	StringInfoData str;
	int i = 0;
	initStringInfo(&str);

	appendStringInfo(&str, "(num_distcols=%d ", cgs->num_distcols);
	appendStringInfo(&str, "ngrpsets=%d ", cgs->ngrpsets);

	appendStringInfo(&str, "grpsets=[");
	for (i=0;i<cgs->ngrpsets;i++)
	{
		Bitmapset *bms = cgs->grpsets[i];
		Bitmapset *tmpset = bms_copy(bms);
		int x = -1;
		
		appendStringInfo(&str, "b(");
		while ((x = bms_first_member(tmpset)) >= 0)
		{
			appendStringInfo(&str, " %d ", x);
		}
		bms_free(tmpset);
		appendStringInfo(&str, ") ");
	}
	appendStringInfo(&str, "] ");
	
	appendStringInfo(&str, "grpset_counts=[");
	for (i=0;i<cgs->ngrpsets;i++)
	{
		appendStringInfo(&str, "%d ", cgs->grpset_counts[i]);
	}
	appendStringInfo(&str, "])");
	
	return str.data;
}

#endif
