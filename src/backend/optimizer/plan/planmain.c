/*-------------------------------------------------------------------------
 *
 * planmain.c
 *	  Routines to plan a single query
 *
 * What's in a name, anyway?  The top-level entry point of the planner/
 * optimizer is over in planner.c, not here as you might think from the
 * file name.  But this is the main code for planning a basic join operation,
 * shorn of features like subselects, inheritance, aggregates, grouping,
 * and so on.  (Those are the things planner.c deals with.)
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/plan/planmain.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "optimizer/orclauses.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/placeholder.h"
#include "optimizer/planmain.h"

#include "catalog/pg_proc.h"
#include "cdb/cdbpath.h"        /* cdbpath_rows() */
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "optimizer/cost.h"

static Bitmapset *distcols_in_groupclause(List *gc, Bitmapset *bms);

/*
 * query_planner
 *	  Generate a path (that is, a simplified plan) for a basic query,
 *	  which may involve joins but not any fancier features.
 *
 * Since query_planner does not handle the toplevel processing (grouping,
 * sorting, etc) it cannot select the best path by itself.  Instead, it
 * returns the RelOptInfo for the top level of joining, and the caller
 * (grouping_planner) can choose one of the surviving paths for the rel.
 * Normally it would choose either the rel's cheapest path, or the cheapest
 * path for the desired sort order.
 *
 * root describes the query to plan
 * tlist is the target list the query should produce
 *		(this is NOT necessarily root->parse->targetList!)
 * qp_callback is a function to compute query_pathkeys once it's safe to do so
 * qp_extra is optional extra data to pass to qp_callback
 *
 * Note: the PlannerInfo node also includes a query_pathkeys field, which
 * tells query_planner the sort order that is desired in the final output
 * plan.  This value is *not* available at call time, but is computed by
 * qp_callback once we have completed merging the query's equivalence classes.
 * (We cannot construct canonical pathkeys until that's done.)
 */
RelOptInfo *
query_planner(PlannerInfo *root, List *tlist,
			  query_pathkeys_callback qp_callback, void *qp_extra)
{
	Query	   *parse = root->parse;
	List	   *joinlist;
	RelOptInfo *final_rel;
	Index		rti;
	double		total_pages;

	/*
	 * If the query has an empty join tree, then it's something easy like
	 * "SELECT 2+2;" or "INSERT ... VALUES()".  Fall through quickly.
	 */
	if (parse->jointree->fromlist == NIL)
	{
		Path	   *result_path;

		/* We need a dummy joinrel to describe the empty set of baserels */
		final_rel = build_empty_join_rel(root);

		/* The only path for it is a trivial Result path */
		result_path = (Path *) create_result_path((List *) parse->jointree->quals);
		add_path(final_rel, result_path);

		/* Select cheapest path (pretty easy in this case...) */
		set_cheapest(final_rel);

		/*
		 * We still are required to call qp_callback, in case it's something
		 * like "SELECT 2+2 ORDER BY 1".
		 */
		root->canon_pathkeys = NIL;
		(*qp_callback) (root, qp_extra);

		{
			char		exec_location;

			exec_location = check_execute_on_functions((Node *) parse->targetList);

			if (exec_location == PROEXECLOCATION_MASTER)
				CdbPathLocus_MakeEntry(&result_path->locus);
			else if (exec_location == PROEXECLOCATION_ALL_SEGMENTS)
				CdbPathLocus_MakeStrewn(&result_path->locus,
										GP_POLICY_ALL_NUMSEGMENTS);
		}

		return final_rel;
	}

	/*
	 * Init planner lists to empty.
	 *
	 * NOTE: append_rel_list was set up by subquery_planner, so do not touch
	 * here; eq_classes and minmax_aggs may contain data already, too.
	 */
	root->join_rel_list = NIL;
	root->join_rel_hash = NULL;
	root->join_rel_level = NULL;
	root->join_cur_level = 0;
	root->canon_pathkeys = NIL;
	root->left_join_clauses = NIL;
	root->right_join_clauses = NIL;
	root->full_join_clauses = NIL;
	root->join_info_list = NIL;
	root->lateral_info_list = NIL;
	root->placeholder_list = NIL;
	root->initial_rels = NIL;

	/*
	 * Make a flattened version of the rangetable for faster access (this is
	 * OK because the rangetable won't change any more), and set up an empty
	 * array for indexing base relations.
	 */
	setup_simple_rel_arrays(root);

	/*
	 * Construct RelOptInfo nodes for all base relations in query, and
	 * indirectly for all appendrel member relations ("other rels").  This
	 * will give us a RelOptInfo for every "simple" (non-join) rel involved in
	 * the query.
	 *
	 * Note: the reason we find the rels by searching the jointree and
	 * appendrel list, rather than just scanning the rangetable, is that the
	 * rangetable may contain RTEs for rels not actively part of the query,
	 * for example views.  We don't want to make RelOptInfos for them.
	 */
	add_base_rels_to_query(root, (Node *) parse->jointree);

	/*
	 * Examine the targetlist and join tree, adding entries to baserel
	 * targetlists for all referenced Vars, and generating PlaceHolderInfo
	 * entries for all referenced PlaceHolderVars.  Restrict and join clauses
	 * are added to appropriate lists belonging to the mentioned relations. We
	 * also build EquivalenceClasses for provably equivalent expressions. The
	 * SpecialJoinInfo list is also built to hold information about join order
	 * restrictions.  Finally, we form a target joinlist for make_one_rel() to
	 * work from.
	 */
	build_base_rel_tlists(root, tlist);

	find_placeholders_in_jointree(root);

	find_lateral_references(root);

	joinlist = deconstruct_jointree(root);

	/*
	 * Reconsider any postponed outer-join quals now that we have built up
	 * equivalence classes.  (This could result in further additions or
	 * mergings of classes.)
	 */
	reconsider_outer_join_clauses(root);

	/**
	 * Use the list of equijoined keys to transfer quals between relations.  For example,
	 *   A=B AND f(A) implies A=B AND f(A) and f(B), under some restrictions on f.
	 */
	generate_implied_quals(root);

	/*
	 * If we formed any equivalence classes, generate additional restriction
	 * clauses as appropriate.  (Implied join clauses are formed on-the-fly
	 * later.)
	 */
	generate_base_implied_equalities(root);

	/*
	 * We have completed merging equivalence sets, so it's now possible to
	 * generate pathkeys in canonical form; so compute query_pathkeys and
	 * other pathkeys fields in PlannerInfo.
	 */
	(*qp_callback) (root, qp_extra);

	/*
	 * Examine any "placeholder" expressions generated during subquery pullup.
	 * Make sure that the Vars they need are marked as needed at the relevant
	 * join level.  This must be done before join removal because it might
	 * cause Vars or placeholders to be needed above a join when they weren't
	 * so marked before.
	 */
	fix_placeholder_input_needed_levels(root);

	/*
	 * Remove any useless outer joins.  Ideally this would be done during
	 * jointree preprocessing, but the necessary information isn't available
	 * until we've built baserel data structures and classified qual clauses.
	 */
	joinlist = remove_useless_joins(root, joinlist);

	/*
	 * Now distribute "placeholders" to base rels as needed.  This has to be
	 * done after join removal because removal could change whether a
	 * placeholder is evaluable at a base rel.
	 */
	add_placeholders_to_base_rels(root);

	/*
	 * Create the LateralJoinInfo list now that we have finalized
	 * PlaceHolderVar eval levels and made any necessary additions to the
	 * lateral_vars lists for lateral references within PlaceHolderVars.
	 */
	create_lateral_join_info(root);

	/*
	 * Look for join OR clauses that we can extract single-relation
	 * restriction OR clauses from.
	 */
	extract_restriction_or_clauses(root);

	/*
	 * We should now have size estimates for every actual table involved in
	 * the query, and we also know which if any have been deleted from the
	 * query by join removal; so we can compute total_table_pages.
	 *
	 * Note that appendrels are not double-counted here, even though we don't
	 * bother to distinguish RelOptInfos for appendrel parents, because the
	 * parents will still have size zero.
	 *
	 * XXX if a table is self-joined, we will count it once per appearance,
	 * which perhaps is the wrong thing ... but that's not completely clear,
	 * and detecting self-joins here is difficult, so ignore it for now.
	 */
	total_pages = 0;
	for (rti = 1; rti < root->simple_rel_array_size; rti++)
	{
		RelOptInfo *brel = root->simple_rel_array[rti];

		if (brel == NULL)
			continue;

		Assert(brel->relid == rti);		/* sanity check on array */

		if (brel->reloptkind == RELOPT_BASEREL ||
			brel->reloptkind == RELOPT_OTHER_MEMBER_REL)
			total_pages += (double) brel->pages;
	}
	root->total_table_pages = total_pages;

	/*
	 * Ready to do the primary planning.
	 */
	final_rel = make_one_rel(root, joinlist);

	/* Check that we got at least one usable path */
	if (!final_rel || !final_rel->cheapest_total_path ||
		final_rel->cheapest_total_path->param_info != NULL)
		elog(ERROR, "failed to construct the join relation");
	Insist(final_rel->cheapest_startup_path);

	return final_rel;
}

/*
 * distcols_in_groupclause -
 *     Return all distinct tleSortGroupRef values in a GROUP BY clause.
 *
 * If this is a GROUPING_SET, this function is called recursively to
 * find the tleSortGroupRef values for underlying grouping columns.
 */
static Bitmapset *
distcols_in_groupclause(List *gc, Bitmapset *bms)
{
	ListCell *l;

	foreach(l, gc)
	{
		Node *node = lfirst(l);

		if (node == NULL)
			continue;

		Assert(IsA(node, SortGroupClause) ||
			   IsA(node, List) ||
			   IsA(node, GroupingClause));

		if (IsA(node, SortGroupClause))
		{
			bms = bms_add_member(bms, ((SortGroupClause *) node)->tleSortGroupRef);
		}

		else if (IsA(node, List))
		{
			bms = distcols_in_groupclause((List *)node, bms);
		}

		else if (IsA(node, GroupingClause))
		{
			List *groupsets = ((GroupingClause *)node)->groupsets;
			bms = distcols_in_groupclause(groupsets, bms);
		}
	}

	return bms;
}

/*
 * num_distcols_in_grouplist -
 *      Return number of distinct columns/expressions that appeared in
 *      a list of GroupClauses or GroupingClauses.
 */
int
num_distcols_in_grouplist(List *gc)
{
	Bitmapset *bms = NULL;
	int num_cols;

	bms = distcols_in_groupclause(gc, bms);

	num_cols = bms_num_members(bms);
	bms_free(bms);

	return num_cols;
}

/**
 * Planner configuration related
 */

/**
 * Default configuration information
 */
PlannerConfig *DefaultPlannerConfig(void)
{
	PlannerConfig *c1 = (PlannerConfig *) palloc(sizeof(PlannerConfig));
	c1->cdbpath_segments = planner_segment_count(NULL);
	c1->enable_seqscan = enable_seqscan;
	c1->enable_indexscan = enable_indexscan;
	c1->enable_bitmapscan = enable_bitmapscan;
	c1->enable_tidscan = enable_tidscan;
	c1->enable_sort = enable_sort;
	c1->enable_hashagg = enable_hashagg;
	c1->enable_groupagg = enable_groupagg;
	c1->enable_nestloop = enable_nestloop;
	c1->enable_mergejoin = enable_mergejoin;
	c1->enable_hashjoin = enable_hashjoin;
	c1->gp_enable_hashjoin_size_heuristic = gp_enable_hashjoin_size_heuristic;
	c1->gp_enable_predicate_propagation = gp_enable_predicate_propagation;
	c1->constraint_exclusion = constraint_exclusion;

	c1->gp_enable_minmax_optimization = gp_enable_minmax_optimization;
	c1->gp_enable_multiphase_agg = gp_enable_multiphase_agg;
	c1->gp_enable_preunique = gp_enable_preunique;
	c1->gp_eager_preunique = gp_eager_preunique;
	c1->gp_hashagg_streambottom = gp_hashagg_streambottom;
	c1->gp_enable_agg_distinct = gp_enable_agg_distinct;
	c1->gp_enable_dqa_pruning = gp_enable_dqa_pruning;
	c1->gp_eager_dqa_pruning = gp_eager_dqa_pruning;
	c1->gp_eager_one_phase_agg = gp_eager_one_phase_agg;
	c1->gp_eager_two_phase_agg = gp_eager_two_phase_agg;
	c1->gp_enable_groupext_distinct_pruning = gp_enable_groupext_distinct_pruning;
	c1->gp_enable_groupext_distinct_gather = gp_enable_groupext_distinct_gather;
	c1->gp_enable_sort_limit = gp_enable_sort_limit;
	c1->gp_enable_sort_distinct = gp_enable_sort_distinct;

	c1->gp_enable_direct_dispatch = gp_enable_direct_dispatch;
	c1->gp_dynamic_partition_pruning = gp_dynamic_partition_pruning;

	c1->gp_cte_sharing = gp_cte_sharing;

	c1->honor_order_by = true;

	return c1;
}

/*
 * Copy configuration information
 */
PlannerConfig *
CopyPlannerConfig(const PlannerConfig *c1)
{
	PlannerConfig *c2 = (PlannerConfig *) palloc(sizeof(PlannerConfig));

	memcpy(c2, c1, sizeof(PlannerConfig));
	return c2;
}
