/*-------------------------------------------------------------------------
 *
 * joinpath.c
 *	  Routines to find all possible paths for processing a set of joins
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/path/joinpath.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "executor/executor.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"
#include "utils/lsyscache.h"

#include "executor/nodeHash.h"                  /* ExecHashRowSize() */
#include "cdb/cdbpath.h"                        /* cdbpath_rows() */


#define PATH_PARAM_BY_REL(path, rel)  \
	((path)->param_info && bms_overlap(PATH_REQ_OUTER(path), (rel)->relids))

static void sort_inner_and_outer(PlannerInfo *root, RelOptInfo *joinrel,
					 RelOptInfo *outerrel, RelOptInfo *innerrel,
					 List *restrictlist, List *redistribution_clauses,
					 List *mergeclause_list,
					 JoinType jointype, SpecialJoinInfo *sjinfo,
					 Relids param_source_rels);
static void match_unsorted_outer(PlannerInfo *root, RelOptInfo *joinrel,
					 RelOptInfo *outerrel, RelOptInfo *innerrel,
					 List *restrictlist, List *redistribution_clauses,
					 List *mergeclause_list,
					 JoinType jointype, SpecialJoinInfo *sjinfo,
					 SemiAntiJoinFactors *semifactors,
					 Relids param_source_rels);
static void hash_inner_and_outer(PlannerInfo *root, RelOptInfo *joinrel,
					 RelOptInfo *outerrel, RelOptInfo *innerrel,
					 List *restrictlist,
					 JoinType jointype, SpecialJoinInfo *sjinfo,
					 SemiAntiJoinFactors *semifactors,
					 Relids param_source_rels,
					 List *redistribution_clauses /*CDB*/);
static List *select_mergejoin_clauses(PlannerInfo *root,
						 RelOptInfo *joinrel,
						 RelOptInfo *outerrel,
						 RelOptInfo *innerrel,
						 List *restrictlist,
						 JoinType jointype,
						 bool *mergejoin_allowed);
static List *select_cdb_redistribute_clauses(PlannerInfo *root,
											 RelOptInfo *joinrel,
											 RelOptInfo *outerrel,
											 RelOptInfo *innerrel,
											 List *restrictlist,
											 JoinType jointype);

/*
 * add_paths_to_joinrel
 *	  Given a join relation and two component rels from which it can be made,
 *	  consider all possible paths that use the two component rels as outer
 *	  and inner rel respectively.  Add these paths to the join rel's pathlist
 *	  if they survive comparison with other paths (and remove any existing
 *	  paths that are dominated by these paths).
 *
 * Modifies the pathlist field of the joinrel node to contain the best
 * paths found so far.
 *
 * jointype is not necessarily the same as sjinfo->jointype; it might be
 * "flipped around" if we are considering joining the rels in the opposite
 * direction from what's indicated in sjinfo.
 *
 * Also, this routine and others in this module accept the special JoinTypes
 * JOIN_UNIQUE_OUTER and JOIN_UNIQUE_INNER to indicate that we should
 * unique-ify the outer or inner relation and then apply a regular inner
 * join.  These values are not allowed to propagate outside this module,
 * however.  Path cost estimation code may need to recognize that it's
 * dealing with such a case --- the combination of nominal jointype INNER
 * with sjinfo->jointype == JOIN_SEMI indicates that.
 */
void
add_paths_to_joinrel(PlannerInfo *root,
					 RelOptInfo *joinrel,
					 RelOptInfo *outerrel,
					 RelOptInfo *innerrel,
					 JoinType jointype,
					 SpecialJoinInfo *sjinfo,
					 List *restrictlist)
{
	List	   *mergeclause_list = NIL;
	List	   *redistribution_clauses = NIL;
	bool		mergejoin_allowed = true;
	SemiAntiJoinFactors semifactors;
	Relids		param_source_rels = NULL;
	ListCell   *lc;

    Assert(outerrel->pathlist &&
           outerrel->cheapest_total_path);
    Assert(innerrel->pathlist &&
           innerrel->cheapest_total_path);

	/* Don't consider paths that have WorkTableScan as inner rel */
	if (innerrel->cheapest_startup_path && cdbpath_contains_wts(innerrel->cheapest_startup_path))
		return;
	if (cdbpath_contains_wts(innerrel->cheapest_total_path))
		return;

	/*
	 * Find potential mergejoin clauses.  We can skip this if we are not
	 * interested in doing a mergejoin.  However, mergejoin may be our only
	 * way of implementing a full outer join, so override enable_mergejoin if
	 * it's a full join.
	 *
	 * CDB: Always build mergeclause_list.  We need it for motion planning.
	 */
	redistribution_clauses = select_cdb_redistribute_clauses(root,
															 joinrel,
															 outerrel,
															 innerrel,
															 restrictlist,
															 jointype);

	/*
	 * If it's SEMI or ANTI join, compute correction factors for cost
	 * estimation.  These will be the same for all paths.
	 */
	if (jointype == JOIN_SEMI || jointype == JOIN_ANTI)
		compute_semi_anti_join_factors(root, outerrel, innerrel,
									   jointype, sjinfo, restrictlist,
									   &semifactors);

	/*
	 * Decide whether it's sensible to generate parameterized paths for this
	 * joinrel, and if so, which relations such paths should require.  There
	 * is usually no need to create a parameterized result path unless there
	 * is a join order restriction that prevents joining one of our input rels
	 * directly to the parameter source rel instead of joining to the other
	 * input rel.  (But see allow_star_schema_join().)	This restriction
	 * reduces the number of parameterized paths we have to deal with at
	 * higher join levels, without compromising the quality of the resulting
	 * plan.  We express the restriction as a Relids set that must overlap the
	 * parameterization of any proposed join path.
	 */
	foreach(lc, root->join_info_list)
	{
		SpecialJoinInfo *sjinfo = (SpecialJoinInfo *) lfirst(lc);

		/*
		 * SJ is relevant to this join if we have some part of its RHS
		 * (possibly not all of it), and haven't yet joined to its LHS.  (This
		 * test is pretty simplistic, but should be sufficient considering the
		 * join has already been proven legal.)  If the SJ is relevant, it
		 * presents constraints for joining to anything not in its RHS.
		 */
		if (bms_overlap(joinrel->relids, sjinfo->min_righthand) &&
			!bms_overlap(joinrel->relids, sjinfo->min_lefthand))
			param_source_rels = bms_join(param_source_rels,
										 bms_difference(root->all_baserels,
													 sjinfo->min_righthand));

		/* full joins constrain both sides symmetrically */
		if (sjinfo->jointype == JOIN_FULL &&
			bms_overlap(joinrel->relids, sjinfo->min_lefthand) &&
			!bms_overlap(joinrel->relids, sjinfo->min_righthand))
			param_source_rels = bms_join(param_source_rels,
										 bms_difference(root->all_baserels,
													  sjinfo->min_lefthand));
	}

	/*
	 * However, when a LATERAL subquery is involved, there will simply not be
	 * any paths for the joinrel that aren't parameterized by whatever the
	 * subquery is parameterized by, unless its parameterization is resolved
	 * within the joinrel.  So we might as well allow additional dependencies
	 * on whatever residual lateral dependencies the joinrel will have.
	 */
	param_source_rels = bms_add_members(param_source_rels,
										joinrel->lateral_relids);

	/*
	 * 1. Consider mergejoin paths where both relations must be explicitly
	 * sorted.  Skip this if we can't mergejoin.
	 */
	mergeclause_list = select_mergejoin_clauses(root,
												joinrel,
												outerrel,
												innerrel,
												restrictlist,
												jointype,
												&mergejoin_allowed);
	if (mergejoin_allowed && jointype != JOIN_LASJ_NOTIN)
		sort_inner_and_outer(root, joinrel, outerrel, innerrel,
							 restrictlist, redistribution_clauses,
							 mergeclause_list, jointype,
							 sjinfo,
							 param_source_rels);

	/*
	 * 2. Consider paths where the outer relation need not be explicitly
	 * sorted. This includes both nestloops and mergejoins where the outer
	 * path is already ordered.  Again, skip this if we can't mergejoin.
	 * (That's okay because we know that nestloop can't handle right/full
	 * joins at all, so it wouldn't work in the prohibited cases either.)
	 */
	if (mergejoin_allowed)
		match_unsorted_outer(root, joinrel, outerrel, innerrel,
							 restrictlist, redistribution_clauses,
							 mergeclause_list, jointype,
							 sjinfo, &semifactors,
							 param_source_rels);

#ifdef NOT_USED

	/*
	 * 3. Consider paths where the inner relation need not be explicitly
	 * sorted.  This includes mergejoins only (nestloops were already built in
	 * match_unsorted_outer).
	 *
	 * Diked out as redundant 2/13/2000 -- tgl.  There isn't any really
	 * significant difference between the inner and outer side of a mergejoin,
	 * so match_unsorted_inner creates no paths that aren't equivalent to
	 * those made by match_unsorted_outer when add_paths_to_joinrel() is
	 * invoked with the two rels given in the other order.
	 */
	if (mergejoin_allowed)
		match_unsorted_inner(root, joinrel, outerrel, innerrel,
							 restrictlist, redistribution_clauses,
							 mergeclause_list, jointype,
							 sjinfo, &semifactors,
							 param_source_rels);
#endif

	/*
	 * 4. Consider paths where both outer and inner relations must be hashed
	 * before being joined.  As above, disregard enable_hashjoin for full
	 * joins, because there may be no other alternative.
     *
	 * We consider both the cheapest-total-cost and cheapest-startup-cost
	 * outer paths.  There's no need to consider any but the
	 * cheapest-total-cost inner path, however.
	 */
	if (root->config->enable_hashjoin || jointype == JOIN_FULL)
		hash_inner_and_outer(root, joinrel, outerrel, innerrel,
							 restrictlist, jointype,
							 sjinfo, &semifactors,
							 param_source_rels,
							 redistribution_clauses);
}

/*
 * We override the param_source_rels heuristic to accept nestloop paths in
 * which the outer rel satisfies some but not all of the inner path's
 * parameterization.  This is necessary to get good plans for star-schema
 * scenarios, in which a parameterized path for a large table may require
 * parameters from multiple small tables that will not get joined directly to
 * each other.  We can handle that by stacking nestloops that have the small
 * tables on the outside; but this breaks the rule the param_source_rels
 * heuristic is based on, namely that parameters should not be passed down
 * across joins unless there's a join-order-constraint-based reason to do so.
 * So we ignore the param_source_rels restriction when this case applies.
 *
 * allow_star_schema_join() returns TRUE if the param_source_rels restriction
 * should be overridden, ie, it's okay to perform this join.
 */
static inline bool
allow_star_schema_join(PlannerInfo *root,
					   Path *outer_path,
					   Path *inner_path)
{
	Relids		innerparams = PATH_REQ_OUTER(inner_path);
	Relids		outerrelids = outer_path->parent->relids;

	/*
	 * It's a star-schema case if the outer rel provides some but not all of
	 * the inner rel's parameterization.
	 */
	return (bms_overlap(innerparams, outerrelids) &&
			bms_nonempty_difference(innerparams, outerrelids));
}

/*
 * try_nestloop_path
 *	  Consider a nestloop join path; if it appears useful, push it into
 *	  the joinrel's pathlist via add_path().
 */
static void
try_nestloop_path(PlannerInfo *root,
				  RelOptInfo *joinrel,
				  JoinType orig_jointype,	/* CDB */
				  JoinType jointype,
				  SpecialJoinInfo *sjinfo,
				  SemiAntiJoinFactors *semifactors,
				  Relids param_source_rels,
				  Path *outer_path,
				  Path *inner_path,
				  List *restrict_clauses,
				  List *redistribution_clauses,    /*CDB*/
				  List *pathkeys)
{
	Relids		required_outer;
	JoinCostWorkspace workspace;

	/*
	 * Check to see if proposed path is still parameterized, and reject if the
	 * parameterization wouldn't be sensible --- unless allow_star_schema_join
	 * says to allow it anyway.  Also, we must reject if have_dangerous_phv
	 * doesn't like the look of it, which could only happen if the nestloop is
	 * still parameterized.
	 */
	required_outer = calc_nestloop_required_outer(outer_path,
												  inner_path);
	if (required_outer &&
		((!bms_overlap(required_outer, param_source_rels) &&
		  !allow_star_schema_join(root, outer_path, inner_path)) ||
		 have_dangerous_phv(root,
							outer_path->parent->relids,
							PATH_REQ_OUTER(inner_path))))
	{
		/* Waste no memory when we reject a path here */
		bms_free(required_outer);
		return;
	}

	/*
	 * Do a precheck to quickly eliminate obviously-inferior paths.  We
	 * calculate a cheap lower bound on the path's cost and then use
	 * add_path_precheck() to see if the path is clearly going to be dominated
	 * by some existing path for the joinrel.  If not, do the full pushup with
	 * creating a fully valid path structure and submitting it to add_path().
	 * The latter two steps are expensive enough to make this two-phase
	 * methodology worthwhile.
	 */
	initial_cost_nestloop(root, &workspace, jointype,
						  outer_path, inner_path,
						  sjinfo, semifactors);

	if (add_path_precheck(joinrel,
						  workspace.startup_cost, workspace.total_cost,
						  pathkeys, required_outer))
	{
		cdb_add_join_path(root, joinrel, orig_jointype, required_outer, (JoinPath *)
				 create_nestloop_path(root,
									  joinrel,
									  jointype,
									  &workspace,
									  sjinfo,
									  semifactors,
									  outer_path,
									  inner_path,
									  restrict_clauses,
									  redistribution_clauses,
									  pathkeys,
									  required_outer));
	}
	else
	{
		/* Waste no memory when we reject a path here */
		bms_free(required_outer);
	}
}

/*
 * try_mergejoin_path
 *	  Consider a merge join path; if it appears useful, push it into
 *	  the joinrel's pathlist via add_path().
 */
static void
try_mergejoin_path(PlannerInfo *root,
				   RelOptInfo *joinrel,
				   JoinType orig_jointype,
				   JoinType jointype,
				   SpecialJoinInfo *sjinfo,
				   Relids param_source_rels,
				   Path *outer_path,
				   Path *inner_path,
				   List *restrict_clauses,
				   List *pathkeys,
				   List *mergeclauses,
				   List *redistribution_clauses,
				   List *outersortkeys,
				   List *innersortkeys)
{
	Relids		required_outer;
	JoinCostWorkspace workspace;

	/*
	 * Check to see if proposed path is still parameterized, and reject if the
	 * parameterization wouldn't be sensible.
	 */
	required_outer = calc_non_nestloop_required_outer(outer_path,
													  inner_path);
	if (required_outer &&
		!bms_overlap(required_outer, param_source_rels))
	{
		/* Waste no memory when we reject a path here */
		bms_free(required_outer);
		return;
	}

	/*
	 * If the given paths are already well enough ordered, we can skip doing
	 * an explicit sort.
	 */
	if (outersortkeys &&
		pathkeys_contained_in(outersortkeys, outer_path->pathkeys))
		outersortkeys = NIL;
	if (innersortkeys &&
		pathkeys_contained_in(innersortkeys, inner_path->pathkeys))
		innersortkeys = NIL;

	/*
	 * See comments in try_nestloop_path().
	 */
	initial_cost_mergejoin(root, &workspace, jointype, mergeclauses,
						   outer_path, inner_path,
						   outersortkeys, innersortkeys,
						   sjinfo);

	if (add_path_precheck(joinrel,
						  workspace.startup_cost, workspace.total_cost,
						  pathkeys, required_outer))
	{
		cdb_add_join_path(root, joinrel, orig_jointype, required_outer, (JoinPath *)
				 create_mergejoin_path(root,
									   joinrel,
									   jointype,
									   &workspace,
									   sjinfo,
									   outer_path,
									   inner_path,
									   restrict_clauses,
									   pathkeys,
									   required_outer,
									   mergeclauses,
									   redistribution_clauses,
									   outersortkeys,
									   innersortkeys));
	}
	else
	{
		/* Waste no memory when we reject a path here */
		bms_free(required_outer);
	}
}

/*
 * try_hashjoin_path
 *	  Consider a hash join path; if it appears useful, push it into
 *	  the joinrel's pathlist via add_path().
 */
static void
try_hashjoin_path(PlannerInfo *root,
				  RelOptInfo *joinrel,
				  JoinType orig_jointype,
				  JoinType jointype,
				  SpecialJoinInfo *sjinfo,
				  SemiAntiJoinFactors *semifactors,
				  Relids param_source_rels,
				  Path *outer_path,
				  Path *inner_path,
				  List *restrict_clauses,
				  List *redistribution_clauses,    /*CDB*/
				  List *hashclauses)
{
	Relids		required_outer;
	JoinCostWorkspace workspace;

	/*
	 * Check to see if proposed path is still parameterized, and reject if the
	 * parameterization wouldn't be sensible.
	 */
	required_outer = calc_non_nestloop_required_outer(outer_path,
													  inner_path);
	if (required_outer &&
		!bms_overlap(required_outer, param_source_rels))
	{
		/* Waste no memory when we reject a path here */
		bms_free(required_outer);
		return;
	}

	/*
	 * See comments in try_nestloop_path().  Also note that hashjoin paths
	 * never have any output pathkeys, per comments in create_hashjoin_path.
	 */
	initial_cost_hashjoin(root, &workspace, jointype, hashclauses,
						  outer_path, inner_path,
						  sjinfo, semifactors);

	if (add_path_precheck(joinrel,
						  workspace.startup_cost, workspace.total_cost,
						  NIL, required_outer))
	{
		cdb_add_join_path(root, joinrel, orig_jointype, required_outer, (JoinPath *)
				 create_hashjoin_path(root,
									  joinrel,
									  jointype,
									  &workspace,
									  sjinfo,
									  semifactors,
									  outer_path,
									  inner_path,
									  restrict_clauses,
									  required_outer,
									  redistribution_clauses,
									  hashclauses));
	}
	else
	{
		/* Waste no memory when we reject a path here */
		bms_free(required_outer);
	}
}

/*
 * clause_sides_match_join
 *	  Determine whether a join clause is of the right form to use in this join.
 *
 * We already know that the clause is a binary opclause referencing only the
 * rels in the current join.  The point here is to check whether it has the
 * form "outerrel_expr op innerrel_expr" or "innerrel_expr op outerrel_expr",
 * rather than mixing outer and inner vars on either side.  If it matches,
 * we set the transient flag outer_is_left to identify which side is which.
 */
static inline bool
clause_sides_match_join(RestrictInfo *rinfo, RelOptInfo *outerrel,
						RelOptInfo *innerrel)
{
	if (bms_is_subset(rinfo->left_relids, outerrel->relids) &&
		bms_is_subset(rinfo->right_relids, innerrel->relids))
	{
		/* lefthand side is outer */
		rinfo->outer_is_left = true;
		return true;
	}
	else if (bms_is_subset(rinfo->left_relids, innerrel->relids) &&
			 bms_is_subset(rinfo->right_relids, outerrel->relids))
	{
		/* righthand side is outer */
		rinfo->outer_is_left = false;
		return true;
	}
	return false;				/* no good for these input relations */
}

/*
 * sort_inner_and_outer
 *	  Create mergejoin join paths by explicitly sorting both the outer and
 *	  inner join relations on each available merge ordering.
 *
 * 'joinrel' is the join relation
 * 'outerrel' is the outer join relation
 * 'innerrel' is the inner join relation
 * 'restrictlist' contains all of the RestrictInfo nodes for restriction
 *		clauses that apply to this join
 * 'mergeclause_list' is a list of RestrictInfo nodes for available
 *		mergejoin clauses in this join
 * 'jointype' is the type of join to do
 * 'sjinfo' is extra info about the join for selectivity estimation
 * 'param_source_rels' are OK targets for parameterization of result paths
 */
static void
sort_inner_and_outer(PlannerInfo *root,
					 RelOptInfo *joinrel,
					 RelOptInfo *outerrel,
					 RelOptInfo *innerrel,
					 List *restrictlist,
					 List *redistribution_clauses,
					 List *mergeclause_list,
					 JoinType jointype,
					 SpecialJoinInfo *sjinfo,
					 Relids param_source_rels)
{
	Path	   *outer_path;
	Path	   *inner_path;
	List	   *all_pathkeys;
	ListCell   *l;
	JoinType	save_jointype = jointype;

	if (jointype == JOIN_DEDUP_SEMI || jointype == JOIN_DEDUP_SEMI_REVERSE)
		jointype = JOIN_INNER;

	/*
	 * We only consider the cheapest-total-cost input paths, since we are
	 * assuming here that a sort is required.  We will consider
	 * cheapest-startup-cost input paths later, and only if they don't need a
	 * sort.
	 *
	 * This function intentionally does not consider parameterized input
	 * paths, except when the cheapest-total is parameterized.  If we did so,
	 * we'd have a combinatorial explosion of mergejoin paths of dubious
	 * value.  This interacts with decisions elsewhere that also discriminate
	 * against mergejoins with parameterized inputs; see comments in
	 * src/backend/optimizer/README.
	 */
	outer_path = outerrel->cheapest_total_path;
	inner_path = innerrel->cheapest_total_path;

	/*
	 * If either cheapest-total path is parameterized by the other rel, we
	 * can't use a mergejoin.  (There's no use looking for alternative input
	 * paths, since these should already be the least-parameterized available
	 * paths.)
	 */
	if (PATH_PARAM_BY_REL(outer_path, innerrel) ||
		PATH_PARAM_BY_REL(inner_path, outerrel))
		return;

	/*
	 * If unique-ification is requested, do it and then handle as a plain
	 * inner join.
	 */
	if (jointype == JOIN_UNIQUE_OUTER)
	{
		outer_path = (Path *) create_unique_path(root, outerrel,
												 outer_path, sjinfo);
		Assert(outer_path);
		jointype = JOIN_INNER;
	}
	else if (jointype == JOIN_UNIQUE_INNER)
	{
		inner_path = (Path *) create_unique_path(root, innerrel,
												 inner_path, sjinfo);
		Assert(inner_path);
		jointype = JOIN_INNER;
	}

	/*
	 * Each possible ordering of the available mergejoin clauses will generate
	 * a differently-sorted result path at essentially the same cost.  We have
	 * no basis for choosing one over another at this level of joining, but
	 * some sort orders may be more useful than others for higher-level
	 * mergejoins, so it's worth considering multiple orderings.
	 *
	 * Actually, it's not quite true that every mergeclause ordering will
	 * generate a different path order, because some of the clauses may be
	 * partially redundant (refer to the same EquivalenceClasses).  Therefore,
	 * what we do is convert the mergeclause list to a list of canonical
	 * pathkeys, and then consider different orderings of the pathkeys.
	 *
	 * Generating a path for *every* permutation of the pathkeys doesn't seem
	 * like a winning strategy; the cost in planning time is too high. For
	 * now, we generate one path for each pathkey, listing that pathkey first
	 * and the rest in random order.  This should allow at least a one-clause
	 * mergejoin without re-sorting against any other possible mergejoin
	 * partner path.  But if we've not guessed the right ordering of secondary
	 * keys, we may end up evaluating clauses as qpquals when they could have
	 * been done as mergeclauses.  (In practice, it's rare that there's more
	 * than two or three mergeclauses, so expending a huge amount of thought
	 * on that is probably not worth it.)
	 *
	 * The pathkey order returned by select_outer_pathkeys_for_merge() has
	 * some heuristics behind it (see that function), so be sure to try it
	 * exactly as-is as well as making variants.
	 */
	all_pathkeys = select_outer_pathkeys_for_merge(root,
												   mergeclause_list,
												   joinrel);

	foreach(l, all_pathkeys)
	{
		List	   *front_pathkey = (List *) lfirst(l);
		List	   *cur_mergeclauses;
		List	   *outerkeys;
		List	   *innerkeys;
		List	   *merge_pathkeys;

		/* Make a pathkey list with this guy first */
		if (l != list_head(all_pathkeys))
			outerkeys = lcons(front_pathkey,
							  list_delete_ptr(list_copy(all_pathkeys),
											  front_pathkey));
		else
			outerkeys = all_pathkeys;	/* no work at first one... */

		/* Sort the mergeclauses into the corresponding ordering */
		cur_mergeclauses =
			find_mergeclauses_for_outer_pathkeys(root,
												 outerkeys,
												 mergeclause_list);

		/* Should have used them all... */
		Assert(list_length(cur_mergeclauses) == list_length(mergeclause_list));

		/* Build sort pathkeys for the inner side */
		innerkeys = make_inner_pathkeys_for_merge(root,
												  cur_mergeclauses,
												  outerkeys);

		/* Build pathkeys representing output sort order */
		merge_pathkeys = build_join_pathkeys(root, joinrel, jointype,
											 outerkeys);

		/*
		 * And now we can make the path.
		 *
		 * Note: it's possible that the cheapest paths will already be sorted
		 * properly.  try_mergejoin_path will detect that case and suppress an
		 * explicit sort step, so we needn't do so here.
		 */
		try_mergejoin_path(root,
						   joinrel,
						   save_jointype,
						   jointype,
						   sjinfo,
						   param_source_rels,
						   outer_path,
						   inner_path,
						   restrictlist,
						   merge_pathkeys,
						   cur_mergeclauses,
						   redistribution_clauses,
						   outerkeys,
						   innerkeys);
	}
}

/*
 * match_unsorted_outer
 *	  Creates possible join paths for processing a single join relation
 *	  'joinrel' by employing either iterative substitution or
 *	  mergejoining on each of its possible outer paths (considering
 *	  only outer paths that are already ordered well enough for merging).
 *
 * We always generate a nestloop path for each available outer path.
 * In fact we may generate as many as five: one on the cheapest-total-cost
 * inner path, one on the same with materialization, one on the
 * cheapest-startup-cost inner path (if different), one on the
 * cheapest-total inner-indexscan path (if any), and one on the
 * cheapest-startup inner-indexscan path (if different).
 *
 * We also consider mergejoins if mergejoin clauses are available.  We have
 * two ways to generate the inner path for a mergejoin: sort the cheapest
 * inner path, or use an inner path that is already suitably ordered for the
 * merge.  If we have several mergeclauses, it could be that there is no inner
 * path (or only a very expensive one) for the full list of mergeclauses, but
 * better paths exist if we truncate the mergeclause list (thereby discarding
 * some sort key requirements).  So, we consider truncations of the
 * mergeclause list as well as the full list.  (Ideally we'd consider all
 * subsets of the mergeclause list, but that seems way too expensive.)
 *
 * 'joinrel' is the join relation
 * 'outerrel' is the outer join relation
 * 'innerrel' is the inner join relation
 * 'restrictlist' contains all of the RestrictInfo nodes for restriction
 *		clauses that apply to this join
 * 'mergeclause_list' is a list of RestrictInfo nodes for available
 *		mergejoin clauses in this join
 * 'jointype' is the type of join to do
 * 'sjinfo' is extra info about the join for selectivity estimation
 * 'semifactors' contains valid data if jointype is SEMI or ANTI
 * 'param_source_rels' are OK targets for parameterization of result paths
 */
static void
match_unsorted_outer(PlannerInfo *root,
					 RelOptInfo *joinrel,
					 RelOptInfo *outerrel,
					 RelOptInfo *innerrel,
					 List *restrictlist,
					 List *redistribution_clauses,
					 List *mergeclause_list,
					 JoinType jointype,
					 SpecialJoinInfo *sjinfo,
					 SemiAntiJoinFactors *semifactors,
					 Relids param_source_rels)
{
	JoinType	save_jointype = jointype;
	bool		nestjoinOK;
	bool		useallclauses;
	Path	   *inner_cheapest_total = innerrel->cheapest_total_path;
	Path	   *matpath = NULL;
	ListCell   *lc1;

	if (jointype == JOIN_DEDUP_SEMI || jointype == JOIN_DEDUP_SEMI_REVERSE)
		jointype = JOIN_INNER;

	/*
	 * Nestloop only supports inner, left, semi, and anti joins.  Also, if we
	 * are doing a right or full mergejoin, we must use *all* the mergeclauses
	 * as join clauses, else we will not have a valid plan.  (Although these
	 * two flags are currently inverses, keep them separate for clarity and
	 * possible future changes.)
	 */
	switch (jointype)
	{
		case JOIN_INNER:
		case JOIN_LEFT:
		case JOIN_SEMI:
		case JOIN_ANTI:
		case JOIN_LASJ_NOTIN:
			nestjoinOK = true;
			useallclauses = false;
			break;
		case JOIN_RIGHT:
		case JOIN_FULL:
			nestjoinOK = false;
			useallclauses = true;
			break;
		case JOIN_UNIQUE_OUTER:
		case JOIN_UNIQUE_INNER:
			jointype = JOIN_INNER;
			nestjoinOK = true;
			useallclauses = false;
			break;
		default:
			elog(ERROR, "unrecognized join type: %d",
				 (int) jointype);
			nestjoinOK = false; /* keep compiler quiet */
			useallclauses = false;
			break;
	}

	/*
	 * If inner_cheapest_total is parameterized by the outer rel, ignore it;
	 * we will consider it below as a member of cheapest_parameterized_paths,
	 * but the other possibilities considered in this routine aren't usable.
	 */
	if (PATH_PARAM_BY_REL(inner_cheapest_total, outerrel))
		inner_cheapest_total = NULL;

	/*
	 * If we need to unique-ify the inner path, we will consider only the
	 * cheapest-total inner.
	 */
	if (save_jointype == JOIN_UNIQUE_INNER)
	{
		/* No way to do this with an inner path parameterized by outer rel */
		if (inner_cheapest_total == NULL)
			return;
		inner_cheapest_total = (Path *)
			create_unique_path(root, innerrel, inner_cheapest_total, sjinfo);
		Assert(inner_cheapest_total);
	}
	else if (nestjoinOK)
	{
		/*
		 * Consider materializing the cheapest inner path, unless
		 * enable_material is off or the path in question materializes its
		 * output anyway.
		 */
		if (enable_material && inner_cheapest_total != NULL &&
			!ExecMaterializesOutput(inner_cheapest_total->pathtype))
			matpath = (Path *)
				create_material_path(root, innerrel, inner_cheapest_total);
	}

	foreach(lc1, outerrel->pathlist)
	{
		Path	   *outerpath = (Path *) lfirst(lc1);
		List	   *merge_pathkeys;
		List	   *mergeclauses;
		List	   *innersortkeys;
		List	   *trialsortkeys;
		Path	   *cheapest_startup_inner;
		Path	   *cheapest_total_inner;
		int			num_sortkeys;
		int			sortkeycnt;

		/*
		 * We cannot use an outer path that is parameterized by the inner rel.
		 */
		if (PATH_PARAM_BY_REL(outerpath, innerrel))
			continue;

		/*
		 * If we need to unique-ify the outer path, it's pointless to consider
		 * any but the cheapest outer.  (XXX we don't consider parameterized
		 * outers, nor inners, for unique-ified cases.  Should we?)
		 */
		if (save_jointype == JOIN_UNIQUE_OUTER)
		{
			if (outerpath != outerrel->cheapest_total_path)
				continue;
			outerpath = (Path *) create_unique_path(root, outerrel,
													outerpath, sjinfo);
			Assert(outerpath);
		}

		/*
		 * The result will have this sort order (even if it is implemented as
		 * a nestloop, and even if some of the mergeclauses are implemented by
		 * qpquals rather than as true mergeclauses):
		 */
		merge_pathkeys = build_join_pathkeys(root, joinrel, jointype,
											 outerpath->pathkeys);

		if (save_jointype == JOIN_UNIQUE_INNER)
		{
			/*
			 * Consider nestloop join, but only with the unique-ified cheapest
			 * inner path
			 */
			try_nestloop_path(root,
							  joinrel,
							  save_jointype,
							  jointype,
							  sjinfo,
							  semifactors,
							  param_source_rels,
							  outerpath,
							  inner_cheapest_total,
							  restrictlist,
							  redistribution_clauses,
							  merge_pathkeys);
		}
		else if (nestjoinOK)
		{
			/*
			 * Consider nestloop joins using this outer path and various
			 * available paths for the inner relation.  We consider the
			 * cheapest-total paths for each available parameterization of the
			 * inner relation, including the unparameterized case.
			 */
			ListCell   *lc2;

			foreach(lc2, innerrel->cheapest_parameterized_paths)
			{
				Path	   *innerpath = (Path *) lfirst(lc2);

				try_nestloop_path(root,
								  joinrel,
								  save_jointype,
								  jointype,
								  sjinfo,
								  semifactors,
								  param_source_rels,
								  outerpath,
								  innerpath,
								  restrictlist,
								  redistribution_clauses,
								  merge_pathkeys);
			}

			/* Also consider materialized form of the cheapest inner path */
			if (matpath != NULL)
				try_nestloop_path(root,
								  joinrel,
								  save_jointype,
								  jointype,
								  sjinfo,
								  semifactors,
								  param_source_rels,
								  outerpath,
								  matpath,
								  restrictlist,
								  redistribution_clauses,
								  merge_pathkeys);
		}

		/* Can't do anything else if outer path needs to be unique'd */
		if (save_jointype == JOIN_UNIQUE_OUTER)
			continue;

		/* Can't do anything else if inner rel is parameterized by outer */
		if (inner_cheapest_total == NULL)
			continue;

		/* Look for useful mergeclauses (if any) */
		mergeclauses =
			find_mergeclauses_for_outer_pathkeys(root,
												 outerpath->pathkeys,
												 mergeclause_list);

		/*
		 * Done with this outer path if no chance for a mergejoin.
		 *
		 * Special corner case: for "x FULL JOIN y ON true", there will be no
		 * join clauses at all.  Ordinarily we'd generate a clauseless
		 * nestloop path, but since mergejoin is our only join type that
		 * supports FULL JOIN without any join clauses, it's necessary to
		 * generate a clauseless mergejoin path instead.
		 */
		if (mergeclauses == NIL)
		{
			if (jointype == JOIN_FULL)
				 /* okay to try for mergejoin */ ;
			else
				continue;
		}
		if (useallclauses && list_length(mergeclauses) != list_length(mergeclause_list))
			continue;

		/* The merge join executor code doesn't support LASJ_NOTIN */
		if (jointype == JOIN_LASJ_NOTIN)
			continue;

		/* Compute the required ordering of the inner path */
		innersortkeys = make_inner_pathkeys_for_merge(root,
													  mergeclauses,
													  outerpath->pathkeys);

		/*
		 * Generate a mergejoin on the basis of sorting the cheapest inner.
		 * Since a sort will be needed, only cheapest total cost matters. (But
		 * try_mergejoin_path will do the right thing if inner_cheapest_total
		 * is already correctly sorted.)
		 */
		try_mergejoin_path(root,
						   joinrel,
						   save_jointype,
						   jointype,
						   sjinfo,
						   param_source_rels,
						   outerpath,
						   inner_cheapest_total,
						   restrictlist,
						   merge_pathkeys,
						   mergeclauses,
						   redistribution_clauses,
						   NIL,
						   innersortkeys);

		/* Can't do anything else if inner path needs to be unique'd */
		if (save_jointype == JOIN_UNIQUE_INNER)
			continue;

		/*
		 * Look for presorted inner paths that satisfy the innersortkey list
		 * --- or any truncation thereof, if we are allowed to build a
		 * mergejoin using a subset of the merge clauses.  Here, we consider
		 * both cheap startup cost and cheap total cost.
		 *
		 * Currently we do not consider parameterized inner paths here. This
		 * interacts with decisions elsewhere that also discriminate against
		 * mergejoins with parameterized inputs; see comments in
		 * src/backend/optimizer/README.
		 *
		 * As we shorten the sortkey list, we should consider only paths that
		 * are strictly cheaper than (in particular, not the same as) any path
		 * found in an earlier iteration.  Otherwise we'd be intentionally
		 * using fewer merge keys than a given path allows (treating the rest
		 * as plain joinquals), which is unlikely to be a good idea.  Also,
		 * eliminating paths here on the basis of compare_path_costs is a lot
		 * cheaper than building the mergejoin path only to throw it away.
		 *
		 * If inner_cheapest_total is well enough sorted to have not required
		 * a sort in the path made above, we shouldn't make a duplicate path
		 * with it, either.  We handle that case with the same logic that
		 * handles the previous consideration, by initializing the variables
		 * that track cheapest-so-far properly.  Note that we do NOT reject
		 * inner_cheapest_total if we find it matches some shorter set of
		 * pathkeys.  That case corresponds to using fewer mergekeys to avoid
		 * sorting inner_cheapest_total, whereas we did sort it above, so the
		 * plans being considered are different.
		 */
		if (pathkeys_contained_in(innersortkeys,
								  inner_cheapest_total->pathkeys))
		{
			/* inner_cheapest_total didn't require a sort */
			cheapest_startup_inner = inner_cheapest_total;
			cheapest_total_inner = inner_cheapest_total;
		}
		else
		{
			/* it did require a sort, at least for the full set of keys */
			cheapest_startup_inner = NULL;
			cheapest_total_inner = NULL;
		}
		num_sortkeys = list_length(innersortkeys);
		if (num_sortkeys > 1 && !useallclauses)
			trialsortkeys = list_copy(innersortkeys);	/* need modifiable copy */
		else
			trialsortkeys = innersortkeys;		/* won't really truncate */

		for (sortkeycnt = num_sortkeys; sortkeycnt > 0; sortkeycnt--)
		{
			Path	   *innerpath;
			List	   *newclauses = NIL;

			/*
			 * Look for an inner path ordered well enough for the first
			 * 'sortkeycnt' innersortkeys.  NB: trialsortkeys list is modified
			 * destructively, which is why we made a copy...
			 */
			trialsortkeys = list_truncate(trialsortkeys, sortkeycnt);
			innerpath = get_cheapest_path_for_pathkeys(innerrel->pathlist,
													   trialsortkeys,
													   NULL,
													   TOTAL_COST);
			if (innerpath != NULL &&
				(cheapest_total_inner == NULL ||
				 compare_path_costs(innerpath, cheapest_total_inner,
									TOTAL_COST) < 0))
			{
				/* Found a cheap (or even-cheaper) sorted path */
				/* Select the right mergeclauses, if we didn't already */
				if (sortkeycnt < num_sortkeys)
				{
					newclauses =
						trim_mergeclauses_for_inner_pathkeys(root,
															 mergeclauses,
															 trialsortkeys);
					Assert(newclauses != NIL);
				}
				else
					newclauses = mergeclauses;
				try_mergejoin_path(root,
								   joinrel,
								   save_jointype,
								   jointype,
								   sjinfo,
								   param_source_rels,
								   outerpath,
								   innerpath,
								   restrictlist,
								   merge_pathkeys,
								   newclauses,
								   redistribution_clauses,
								   NIL,
								   NIL);
				cheapest_total_inner = innerpath;
			}
			/* Same on the basis of cheapest startup cost ... */
			innerpath = get_cheapest_path_for_pathkeys(innerrel->pathlist,
													   trialsortkeys,
													   NULL,
													   STARTUP_COST);
			if (innerpath != NULL &&
				(cheapest_startup_inner == NULL ||
				 compare_path_costs(innerpath, cheapest_startup_inner,
									STARTUP_COST) < 0))
			{
				/* Found a cheap (or even-cheaper) sorted path */
				if (innerpath != cheapest_total_inner)
				{
					/*
					 * Avoid rebuilding clause list if we already made one;
					 * saves memory in big join trees...
					 */
					if (newclauses == NIL)
					{
						if (sortkeycnt < num_sortkeys)
						{
							newclauses =
								trim_mergeclauses_for_inner_pathkeys(root,
																mergeclauses,
															  trialsortkeys);
							Assert(newclauses != NIL);
						}
						else
							newclauses = mergeclauses;
					}
					try_mergejoin_path(root,
									   joinrel,
									   save_jointype,
									   jointype,
									   sjinfo,
									   param_source_rels,
									   outerpath,
									   innerpath,
									   restrictlist,
									   merge_pathkeys,
									   newclauses,
									   redistribution_clauses,
									   NIL,
									   NIL);
				}
				cheapest_startup_inner = innerpath;
			}

			/*
			 * Don't consider truncated sortkeys if we need all clauses.
			 */
			if (useallclauses)
				break;
		}
	}
}

/*
 * hash_inner_and_outer
 *	  Create hashjoin join paths by explicitly hashing both the outer and
 *	  inner keys of each available hash clause.
 *
 * 'joinrel' is the join relation
 * 'outerrel' is the outer join relation
 * 'innerrel' is the inner join relation
 * 'restrictlist' contains all of the RestrictInfo nodes for restriction
 *		clauses that apply to this join
 * 'jointype' is the type of join to do
 * 'sjinfo' is extra info about the join for selectivity estimation
 * 'semifactors' contains valid data if jointype is SEMI or ANTI
 * 'param_source_rels' are OK targets for parameterization of result paths
 */
static void
hash_inner_and_outer(PlannerInfo *root,
					 RelOptInfo *joinrel,
					 RelOptInfo *outerrel,
					 RelOptInfo *innerrel,
					 List *restrictlist,
					 JoinType jointype,
					 SpecialJoinInfo *sjinfo,
					 SemiAntiJoinFactors *semifactors,
					 Relids param_source_rels,
					 List *redistribution_clauses     /*CDB*/)
{
	bool		isouterjoin = IS_OUTER_JOIN(jointype);
	List	   *hashclauses;
	ListCell   *l;
	JoinType	save_jointype = jointype;

	if (jointype == JOIN_DEDUP_SEMI || jointype == JOIN_DEDUP_SEMI_REVERSE)
		jointype = JOIN_INNER;

	/*
	 * We need to build only one hashclauses list for any given pair of outer
	 * and inner relations; all of the hashable clauses will be used as keys.
	 *
	 * Scan the join's restrictinfo list to find hashjoinable clauses that are
	 * usable with this pair of sub-relations.
	 */
	hashclauses = NIL;
	foreach(l, restrictlist)
	{
		RestrictInfo *restrictinfo = (RestrictInfo *) lfirst(l);

		/*
		 * A qual like "(a = b) IS NOT FALSE" is treated as hashable in
		 * check_hashjoinable(), for the benefit of LASJ joins. It will be
		 * hashed like "a = b", but the special LASJ handlng in the hash join
		 * executor node will ensure that NULLs are treated correctly. For
		 * other kinds of joins, we cannot use "(a = b) IS NOT FALSE" as a
		 * hash qual.
		 */
		if (jointype != JOIN_LASJ_NOTIN && IsA(restrictinfo->clause, BooleanTest))
			continue;

		/*
		 * If processing an outer join, only use its own join clauses for
		 * hashing.  For inner joins we need not be so picky.
		 */
		if (isouterjoin && RINFO_IS_PUSHED_DOWN(restrictinfo, joinrel->relids))
			continue;

		if (!restrictinfo->can_join ||
			restrictinfo->hashjoinoperator == InvalidOid)
			continue;			/* not hashjoinable */

		/*
		 * Check if clause has the form "outer op inner" or "inner op outer".
		 */
		if (!clause_sides_match_join(restrictinfo, outerrel, innerrel))
			continue;			/* no good for these input relations */

		hashclauses = lappend(hashclauses, restrictinfo);
	}

	/* If we found any usable hashclauses, make paths */
	if (hashclauses)
	{
		/*
		 * We consider both the cheapest-total-cost and cheapest-startup-cost
		 * outer paths.  There's no need to consider any but the
		 * cheapest-total-cost inner path, however.
		 */
		Path	   *cheapest_startup_outer = outerrel->cheapest_startup_path;
		Path	   *cheapest_total_outer = outerrel->cheapest_total_path;
		Path	   *cheapest_total_inner = innerrel->cheapest_total_path;

		/*
		 * If either cheapest-total path is parameterized by the other rel, we
		 * can't use a hashjoin.  (There's no use looking for alternative
		 * input paths, since these should already be the least-parameterized
		 * available paths.)
		 */
		if (PATH_PARAM_BY_REL(cheapest_total_outer, innerrel) ||
			PATH_PARAM_BY_REL(cheapest_total_inner, outerrel))
			return;

		/* Unique-ify if need be; we ignore parameterized possibilities */
		if (jointype == JOIN_UNIQUE_OUTER)
		{
			cheapest_total_outer = (Path *)
				create_unique_path(root, outerrel,
								   cheapest_total_outer, sjinfo);
			Assert(cheapest_total_outer);
			jointype = JOIN_INNER;
			try_hashjoin_path(root,
							  joinrel,
							  save_jointype,
							  jointype,
							  sjinfo,
							  semifactors,
							  param_source_rels,
							  cheapest_total_outer,
							  cheapest_total_inner,
							  restrictlist,
							  redistribution_clauses,
							  hashclauses);
			/* no possibility of cheap startup here */
		}
		else if (jointype == JOIN_UNIQUE_INNER)
		{
			cheapest_total_inner = (Path *)
				create_unique_path(root, innerrel,
								   cheapest_total_inner, sjinfo);
			Assert(cheapest_total_inner);
			jointype = JOIN_INNER;
			try_hashjoin_path(root,
							  joinrel,
							  save_jointype,
							  jointype,
							  sjinfo,
							  semifactors,
							  param_source_rels,
							  cheapest_total_outer,
							  cheapest_total_inner,
							  restrictlist,
							  redistribution_clauses,
							  hashclauses);
			if (cheapest_startup_outer != NULL &&
				cheapest_startup_outer != cheapest_total_outer)
				try_hashjoin_path(root,
								  joinrel,
								  save_jointype,
								  jointype,
								  sjinfo,
								  semifactors,
								  param_source_rels,
								  cheapest_startup_outer,
								  cheapest_total_inner,
								  restrictlist,
								  redistribution_clauses,
								  hashclauses);
		}
		else
		{
			/*
			 * For other jointypes, we consider the cheapest startup outer
			 * together with the cheapest total inner, and then consider
			 * pairings of cheapest-total paths including parameterized ones.
			 * There is no use in generating parameterized paths on the basis
			 * of possibly cheap startup cost, so this is sufficient.
			 */
			ListCell   *lc1;
			ListCell   *lc2;

			if (cheapest_startup_outer != NULL)
				try_hashjoin_path(root,
								  joinrel,
								  save_jointype,
								  jointype,
								  sjinfo,
								  semifactors,
								  param_source_rels,
								  cheapest_startup_outer,
								  cheapest_total_inner,
								  restrictlist,
								  redistribution_clauses,
								  hashclauses);

			foreach(lc1, outerrel->cheapest_parameterized_paths)
			{
				Path	   *outerpath = (Path *) lfirst(lc1);

				/*
				 * We cannot use an outer path that is parameterized by the
				 * inner rel.
				 */
				if (PATH_PARAM_BY_REL(outerpath, innerrel))
					continue;

				foreach(lc2, innerrel->cheapest_parameterized_paths)
				{
					Path	   *innerpath = (Path *) lfirst(lc2);

					/*
					 * We cannot use an inner path that is parameterized by
					 * the outer rel, either.
					 */
					if (PATH_PARAM_BY_REL(innerpath, outerrel))
						continue;

					if (outerpath == cheapest_startup_outer &&
						innerpath == cheapest_total_inner)
						continue;		/* already tried it */

					try_hashjoin_path(root,
									  joinrel,
									  save_jointype,
									  jointype,
									  sjinfo,
									  semifactors,
									  param_source_rels,
									  outerpath,
									  innerpath,
									  restrictlist,
									  redistribution_clauses,
									  hashclauses);
				}
			}
		}
	}
}

/*
 * select_mergejoin_clauses
 *	  Select mergejoin clauses that are usable for a particular join.
 *	  Returns a list of RestrictInfo nodes for those clauses.
 *
 * *mergejoin_allowed is normally set to TRUE, but it is set to FALSE if
 * this is a right/full join and there are nonmergejoinable join clauses.
 * The executor's mergejoin machinery cannot handle such cases, so we have
 * to avoid generating a mergejoin plan.  (Note that this flag does NOT
 * consider whether there are actually any mergejoinable clauses.  This is
 * correct because in some cases we need to build a clauseless mergejoin.
 * Simply returning NIL is therefore not enough to distinguish safe from
 * unsafe cases.)
 *
 * We also mark each selected RestrictInfo to show which side is currently
 * being considered as outer.  These are transient markings that are only
 * good for the duration of the current add_paths_to_joinrel() call!
 *
 * We examine each restrictinfo clause known for the join to see
 * if it is mergejoinable and involves vars from the two sub-relations
 * currently of interest.
 */
static List *
select_mergejoin_clauses(PlannerInfo *root,
						 RelOptInfo *joinrel,
						 RelOptInfo *outerrel,
						 RelOptInfo *innerrel,
						 List *restrictlist,
						 JoinType jointype,
						 bool *mergejoin_allowed)
{
	List	   *result_list = NIL;
	bool		isouterjoin = IS_OUTER_JOIN(jointype);
	bool		have_nonmergeable_joinclause = false;
	ListCell   *l;

	foreach(l, restrictlist)
	{
		RestrictInfo *restrictinfo = (RestrictInfo *) lfirst(l);

		/*
		 * If processing an outer join, only use its own join clauses in the
		 * merge.  For inner joins we can use pushed-down clauses too. (Note:
		 * we don't set have_nonmergeable_joinclause here because pushed-down
		 * clauses will become otherquals not joinquals.)
		 */
		if (isouterjoin && RINFO_IS_PUSHED_DOWN(restrictinfo, joinrel->relids))
			continue;

		/* Check that clause is a mergeable operator clause */
		if (!restrictinfo->can_join ||
			restrictinfo->mergeopfamilies == NIL)
		{
			/*
			 * The executor can handle extra joinquals that are constants, but
			 * not anything else, when doing right/full merge join.  (The
			 * reason to support constants is so we can do FULL JOIN ON
			 * FALSE.)
			 */
			if (!restrictinfo->clause || !IsA(restrictinfo->clause, Const))
				have_nonmergeable_joinclause = true;
			continue;			/* not mergejoinable */
		}

		if (!OidIsValid(get_commutator(((OpExpr *)(restrictinfo->clause))->opno)))
			continue;

		/*
		 * Check if clause has the form "outer op inner" or "inner op outer".
		 */
		if (!clause_sides_match_join(restrictinfo, outerrel, innerrel))
		{
			have_nonmergeable_joinclause = true;
			continue;			/* no good for these input relations */
		}

		/*
		 * Insist that each side have a non-redundant eclass.  This
		 * restriction is needed because various bits of the planner expect
		 * that each clause in a merge be associable with some pathkey in a
		 * canonical pathkey list, but redundant eclasses can't appear in
		 * canonical sort orderings.  (XXX it might be worth relaxing this,
		 * but not enough time to address it for 8.3.)
		 *
		 * Note: it would be bad if this condition failed for an otherwise
		 * mergejoinable FULL JOIN clause, since that would result in
		 * undesirable planner failure.  I believe that is not possible
		 * however; a variable involved in a full join could only appear in
		 * below_outer_join eclasses, which aren't considered redundant.
		 *
		 * This case *can* happen for left/right join clauses: the outer-side
		 * variable could be equated to a constant.  Because we will propagate
		 * that constant across the join clause, the loss of ability to do a
		 * mergejoin is not really all that big a deal, and so it's not clear
		 * that improving this is important.
		 */
		update_mergeclause_eclasses(root, restrictinfo);

		if (EC_MUST_BE_REDUNDANT(restrictinfo->left_ec) ||
			EC_MUST_BE_REDUNDANT(restrictinfo->right_ec))
		{
			have_nonmergeable_joinclause = true;
			continue;			/* can't handle redundant eclasses */
		}

		result_list = lappend(result_list, restrictinfo);
	}

	/*
	 * Report whether mergejoin is allowed (see comment at top of function).
	 */
	switch (jointype)
	{
		case JOIN_RIGHT:
		case JOIN_FULL:
			*mergejoin_allowed = !have_nonmergeable_joinclause;
			break;
		default:
			*mergejoin_allowed = true;
			break;
	}

	return result_list;
}


/*
 * select_cdb_redistribute_clauses
 *	  Select redistribute clauses that are usable for a particular join.
 *	  Returns a list of RestrictInfo nodes for those clauses.
 *
 * The result of this function is a subset of mergejoin_clauses. Also
 * verify that the operator can be cdbhash.
 */
static List *
select_cdb_redistribute_clauses(PlannerInfo *root,
								RelOptInfo *joinrel,
								RelOptInfo *outerrel,
								RelOptInfo *innerrel,
								List *restrictlist,
								JoinType jointype)
{
	List	   *result_list = NIL;
	bool		isouterjoin = IS_OUTER_JOIN(jointype);
	bool		have_nonmergeable_joinclause = false;
	ListCell   *l;

	foreach(l, restrictlist)
	{
		RestrictInfo *restrictinfo = (RestrictInfo *) lfirst(l);

		/*
		 * If processing an outer join, only use its own join clauses in the
		 * merge.  For inner joins we can use pushed-down clauses too. (Note:
		 * we don't set have_nonmergeable_joinclause here because pushed-down
		 * clauses will become otherquals not joinquals.)
		 */
		if (isouterjoin && restrictinfo->is_pushed_down)
			continue;

		if (!has_redistributable_clause(restrictinfo))
			continue;

		/* Check that clause is a mergeable operator clause */
		if (!restrictinfo->can_join ||
			restrictinfo->mergeopfamilies == NIL)
		{
			/*
			 * The executor can handle extra joinquals that are constants, but
			 * not anything else, when doing right/full merge join.  (The
			 * reason to support constants is so we can do FULL JOIN ON
			 * FALSE.)
			 */
			if (!restrictinfo->clause || !IsA(restrictinfo->clause, Const))
				have_nonmergeable_joinclause = true;
			continue;			/* not mergejoinable */
		}

		/*
		 * Check if clause has the form "outer op inner" or "inner op outer".
		 */
		if (!clause_sides_match_join(restrictinfo, outerrel, innerrel))
		{
			have_nonmergeable_joinclause = true;
			continue;			/* no good for these input relations */
		}

		/*
		 * Insist that each side have a non-redundant eclass.  This
		 * restriction is needed because various bits of the planner expect
		 * that each clause in a merge be associatable with some pathkey in a
		 * canonical pathkey list, but redundant eclasses can't appear in
		 * canonical sort orderings.  (XXX it might be worth relaxing this,
		 * but not enough time to address it for 8.3.)
		 *
		 * Note: it would be bad if this condition failed for an otherwise
		 * mergejoinable FULL JOIN clause, since that would result in
		 * undesirable planner failure.  I believe that is not possible
		 * however; a variable involved in a full join could only appear in
		 * below_outer_join eclasses, which aren't considered redundant.
		 *
		 * This case *can* happen for left/right join clauses: the outer-side
		 * variable could be equated to a constant.  Because we will propagate
		 * that constant across the join clause, the loss of ability to do a
		 * mergejoin is not really all that big a deal, and so it's not clear
		 * that improving this is important.
		 */
		update_mergeclause_eclasses(root, restrictinfo);

		if (EC_MUST_BE_REDUNDANT(restrictinfo->left_ec) ||
			EC_MUST_BE_REDUNDANT(restrictinfo->right_ec))
		{
			have_nonmergeable_joinclause = true;
			continue;			/* can't handle redundant eclasses */
		}

		result_list = lappend(result_list, restrictinfo);
	}

	return result_list;
}
