/*-------------------------------------------------------------------------
 *
 * pathnode.c
 *	  Routines to manipulate pathlists and create path nodes
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/optimizer/util/pathnode.c,v 1.145 2008/08/07 01:11:50 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "optimizer/clauses.h"              /* contain_mutable_functions() */
#include "nodes/nodeFuncs.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/tlist.h"
#include "optimizer/var.h"
#include "parser/parse_expr.h"
#include "parser/parsetree.h"
#include "utils/memutils.h"
#include "utils/selfuncs.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

#include "cdb/cdbpath.h"        /* cdb_create_motion_path() etc */

static void     cdb_set_cheapest_dedup(PlannerInfo *root, RelOptInfo *rel);
static bool     cdb_is_path_deduped(Path *path);
static bool     cdb_subpath_tried_postjoin_dedup(Path *path, Relids subqrelids);
static UniquePath  *create_limit1_path(PlannerInfo *root, Path *subpath);
static UniquePath *create_unique_exprlist_path(PlannerInfo *root,
							Path *subpath, List *distinct_on_exprs,
							List *distinct_on_operators);
static UniquePath *create_unique_rowid_path(PlannerInfo *root,
						 Path *subpath, Relids dedup_relids);
static UniquePath  *make_limit1_path(Path *subpath);
static UniquePath  *make_unique_path(Path *subpath);
static List *translate_sub_tlist(List *tlist, int relid);
static bool query_is_distinct_for(Query *query, List *colnos, List *opids);
static Oid	distinct_col_search(int colno, List *colnos, List *opids);
static bool hash_safe_operators(List *opids);

static CdbVisitOpt pathnode_walk_list(List *pathlist,
				   CdbVisitOpt (*walker)(Path *path, void *context),
				   void *context);
static CdbVisitOpt pathnode_walk_kids(Path *path,
				   CdbVisitOpt (*walker)(Path *path, void *context),
				   void *context);

/*
 * pathnode_walk_node
 *    Calls a 'walker' function for the given Path node; or returns
 *    CdbVisit_Walk if 'path' is NULL.
 *
 *    If 'walker' returns CdbVisit_Walk, then this function calls
 *    pathnode_walk_kids() to visit the node's children, and returns
 *    the result.
 *
 *    If 'walker' returns CdbVisit_Skip, then this function immediately
 *    returns CdbVisit_Walk and does not visit the node's children.
 *
 *    If 'walker' returns CdbVisit_Stop or another value, then this function
 *    immediately returns that value and does not visit the node's children.
 *
 * pathnode_walk_list
 *    Calls pathnode_walk_node() for each Path node in the given List.
 *
 *    Quits if the result of pathnode_walk_node() is CdbVisit_Stop or another
 *    value other than CdbVisit_Walk, and returns that result without visiting
 *    any more nodes.
 *
 *    Returns CdbVisit_Walk if all of the subtrees return CdbVisit_Walk, or
 *    if the list is empty.
 *
 *    Note that this function never returns CdbVisit_Skip to its caller.
 *    Only the 'walker' can return CdbVisit_Skip.
 *
 * pathnode_walk_kids
 *    Calls pathnode_walk_node() for each child of the given Path node.
 *
 *    Quits if the result of pathnode_walk_node() is CdbVisit_Stop or another
 *    value other than CdbVisit_Walk, and returns that result without visiting
 *    any more nodes.
 *
 *    Returns CdbVisit_Walk if all of the children return CdbVisit_Walk, or
 *    if there are no children.
 *
 *    Note that this function never returns CdbVisit_Skip to its caller.
 *    Only the 'walker' can return CdbVisit_Skip.
 *
 * NB: All CdbVisitOpt values other than CdbVisit_Walk or CdbVisit_Skip are
 * treated as equivalent to CdbVisit_Stop.  Thus the walker can break out
 * of a traversal and at the same time return a smidgen of information to the
 * caller, perhaps to indicate the reason for termination.  For convenience,
 * a couple of alternative stopping codes are predefined for walkers to use at
 * their discretion: CdbVisit_Failure and CdbVisit_Success.
 */
CdbVisitOpt
pathnode_walk_node(Path            *path,
			       CdbVisitOpt    (*walker)(Path *path, void *context),
			       void            *context)
{
    CdbVisitOpt     whatnext;

    if (path == NULL)
        whatnext = CdbVisit_Walk;
    else
    {
        whatnext = walker(path, context);
        if (whatnext == CdbVisit_Walk)
            whatnext = pathnode_walk_kids(path, walker, context);
        else if (whatnext == CdbVisit_Skip)
            whatnext = CdbVisit_Walk;
    }
    Assert(whatnext != CdbVisit_Skip);
    return whatnext;
}	                            /* pathnode_walk_node */

static CdbVisitOpt
pathnode_walk_list(List            *pathlist,
			       CdbVisitOpt    (*walker)(Path *path, void *context),
			       void            *context)
{
    ListCell       *cell;
    Path           *path;
    CdbVisitOpt     v = CdbVisit_Walk;

	foreach(cell, pathlist)
	{
        path = (Path *)lfirst(cell);
        v = pathnode_walk_node(path, walker, context);
        if (v != CdbVisit_Walk) /* stop */
            break;
	}
	return v;
}	                            /* pathnode_walk_list */

static CdbVisitOpt
pathnode_walk_kids(Path            *path,
			       CdbVisitOpt    (*walker)(Path *path, void *context),
			       void            *context)
{
    CdbVisitOpt v;

    Assert(path != NULL);

    switch (path->pathtype)
    {
            case T_SeqScan:
            case T_ExternalScan:
            case T_AppendOnlyScan:
            case T_AOCSScan:
            case T_IndexScan:
            case T_TidScan:
            case T_SubqueryScan:
            case T_FunctionScan:
            case T_ValuesScan:
            case T_CteScan:
            case T_TableFunctionScan:
            case T_Result:
                    return CdbVisit_Walk;
            case T_BitmapHeapScan:
                    v = pathnode_walk_node(((BitmapHeapPath *)path)->bitmapqual, walker, context);
                    break;
			case T_BitmapAppendOnlyScan:
					v = pathnode_walk_node(((BitmapAppendOnlyPath *)path)->bitmapqual, walker, context);
					break;
            case T_BitmapAnd:
                    v = pathnode_walk_list(((BitmapAndPath *)path)->bitmapquals, walker, context);
                    break;
            case T_BitmapOr:
                    v = pathnode_walk_list(((BitmapOrPath *)path)->bitmapquals, walker, context);
                    break;
            case T_HashJoin:
            case T_MergeJoin:
                    v = pathnode_walk_node(((JoinPath *)path)->outerjoinpath, walker, context);
                    if (v != CdbVisit_Walk)     /* stop */
                            break;
                    v = pathnode_walk_node(((JoinPath *)path)->innerjoinpath, walker, context);
                    break;
            case T_NestLoop:
                    {
                            NestPath   *nestpath = (NestPath *)path;

                            v = pathnode_walk_node(nestpath->outerjoinpath, walker, context);
                            if (v != CdbVisit_Walk)     /* stop */
                                    break;
                            v = pathnode_walk_node(nestpath->innerjoinpath, walker, context);
                            if (v != CdbVisit_Walk)     /* stop */
                                    break;
                    }
                    break;
            case T_Append:
                    v = pathnode_walk_list(((AppendPath *)path)->subpaths, walker, context);
                    break;
            case T_Material:
                    v = pathnode_walk_node(((MaterialPath *)path)->subpath, walker, context);
                    break;
            case T_Unique:
                    v = pathnode_walk_node(((UniquePath *)path)->subpath, walker, context);
                    break;
            case T_Motion:
                    v = pathnode_walk_node(((CdbMotionPath *)path)->subpath, walker, context);
                    break;
            default:
                    v = CdbVisit_Walk;  /* keep compiler quiet */
                    elog(ERROR, "unrecognized path type: %d", (int)path->pathtype);
    }
    return v;
}	                            /* pathnode_walk_kids */


/*****************************************************************************
 *		MISC. PATH UTILITIES
 *****************************************************************************/

/*
 * compare_path_costs
 *	  Return -1, 0, or +1 according as path1 is cheaper, the same cost,
 *	  or more expensive than path2 for the specified criterion.
 */
int
compare_path_costs(Path *path1, Path *path2, CostSelector criterion)
{
	if (criterion == STARTUP_COST)
	{
		if (path1->startup_cost < path2->startup_cost)
			return -1;
		if (path1->startup_cost > path2->startup_cost)
			return +1;

		/*
		 * If paths have the same startup cost (not at all unlikely), order
		 * them by total cost.
		 */
		if (path1->total_cost < path2->total_cost)
			return -1;
		if (path1->total_cost > path2->total_cost)
			return +1;
	}
	else
	{
		if (path1->total_cost < path2->total_cost)
			return -1;
		if (path1->total_cost > path2->total_cost)
			return +1;

		/*
		 * If paths have the same total cost, order them by startup cost.
		 */
		if (path1->startup_cost < path2->startup_cost)
			return -1;
		if (path1->startup_cost > path2->startup_cost)
			return +1;
	}
	return 0;
}

/*
 * compare_recursive_path_costs
 *   JoinPath that has WorkTableScan as outer child is always cheaper.
 *   If both paths are JointPath and only path1 has outer WTS return -1.
 *   If both paths are JointPath and only path2 has outer WTS return +1.
 *   Otherwise return 0.
 */
static int
compare_recursive_path_costs(Path *path1, Path *path2)
{
	bool	isWTpath1;
	bool	isWTpath2;

	if (!IsJoinPath(path1) || !IsJoinPath(path2))
		return 0;

	isWTpath1 = ((JoinPath *) path1)->outerjoinpath->pathtype == T_WorkTableScan;
	isWTpath2 = ((JoinPath *) path2)->outerjoinpath->pathtype == T_WorkTableScan;

	if (isWTpath1 && isWTpath2)
		return 0;
	else if (isWTpath1)
		return -1;
	else if (isWTpath2)
		return +1;
	else
		return 0;
}

/*
 * compare_fuzzy_path_costs
 *	  Return -1, 0, or +1 according as path1 is cheaper, the same cost,
 *	  or more expensive than path2 for the specified criterion.
 *
 * This differs from compare_path_costs in that we consider the costs the
 * same if they agree to within a "fuzz factor".  This is used by add_path
 * to avoid keeping both of a pair of paths that really have insignificantly
 * different cost.
 */
static int
compare_fuzzy_path_costs(Path *path1, Path *path2, CostSelector criterion)
{
	int		cmp;

	cmp = compare_recursive_path_costs(path1, path2);
	if (cmp != 0)
		return cmp;

	/*
	 * We use a fuzz factor of 1% of the smaller cost.
	 *
	 * XXX does this percentage need to be user-configurable?
	 */
	if (criterion == STARTUP_COST)
	{
		if (path1->startup_cost > path2->startup_cost * 1.01)
			return +1;
		if (path2->startup_cost > path1->startup_cost * 1.01)
			return -1;

		/*
		 * If paths have the same startup cost (not at all unlikely), order
		 * them by total cost.
		 */
		if (path1->total_cost > path2->total_cost * 1.01)
			return +1;
		if (path2->total_cost > path1->total_cost * 1.01)
			return -1;
	}
	else
	{
		if (path1->total_cost > path2->total_cost * 1.01)
			return +1;
		if (path2->total_cost > path1->total_cost * 1.01)
			return -1;

		/*
		 * If paths have the same total cost, order them by startup cost.
		 */
		if (path1->startup_cost > path2->startup_cost * 1.01)
			return +1;
		if (path2->startup_cost > path1->startup_cost * 1.01)
			return -1;
	}
	return 0;
}

/*
 * compare_path_fractional_costs
 *	  Return -1, 0, or +1 according as path1 is cheaper, the same cost,
 *	  or more expensive than path2 for fetching the specified fraction
 *	  of the total tuples.
 *
 * If fraction is <= 0 or > 1, we interpret it as 1, ie, we select the
 * path with the cheaper total_cost.
 */
int
compare_fractional_path_costs(Path *path1, Path *path2,
							  double fraction)
{
	Cost		cost1,
				cost2;

	if (fraction <= 0.0 || fraction >= 1.0)
		return compare_path_costs(path1, path2, TOTAL_COST);
	cost1 = path1->startup_cost +
		fraction * (path1->total_cost - path1->startup_cost);
	cost2 = path2->startup_cost +
		fraction * (path2->total_cost - path2->startup_cost);
	if (cost1 < cost2)
		return -1;
	if (cost1 > cost2)
		return +1;
	return 0;
}

/*
 * set_cheapest
 *	  Find the minimum-cost paths from among a relation's paths,
 *	  and save them in the rel's cheapest-path fields.
 *
 * This is normally called only after we've finished constructing the path
 * list for the rel node.
 *
 * If we find two paths of identical costs, try to keep the better-sorted one.
 * The paths might have unrelated sort orderings, in which case we can only
 * guess which might be better to keep, but if one is superior then we
 * definitely should keep it.
 */
void
set_cheapest(PlannerInfo *root, RelOptInfo *parent_rel)
{
	ListCell   *p;
	Path	   *cheapest_startup_path;
	Path	   *cheapest_total_path;
    CdbRelDedupInfo    *dedup_info = parent_rel->dedup_info;

	Assert(IsA(parent_rel, RelOptInfo));

    /* CDB: Add paths incorporating subquery duplicate suppression. */
    if (dedup_info &&
        dedup_info->later_dedup_pathlist)
    {
        cdb_set_cheapest_dedup(root, parent_rel);

        /* If main pathlist is empty, put the later_dedup paths there. */
        if (!parent_rel->pathlist &&
            dedup_info->later_dedup_pathlist)
        {
            parent_rel->pathlist = dedup_info->later_dedup_pathlist;
            parent_rel->cheapest_startup_path = dedup_info->cheapest_startup_path;
            parent_rel->cheapest_total_path = dedup_info->cheapest_total_path;
            dedup_info->later_dedup_pathlist = NIL;
            dedup_info->cheapest_startup_path = NULL;
            dedup_info->cheapest_total_path = NULL;
            Assert(parent_rel->cheapest_total_path);
            return;
        }
    }

    /* CDB: Empty pathlist is possible if user set some enable_xxx = off. */
    if (!parent_rel->pathlist)
    {
        parent_rel->cheapest_startup_path = parent_rel->cheapest_total_path = NULL;
        return;
    }

    cheapest_startup_path = cheapest_total_path = (Path *) linitial(parent_rel->pathlist);

	for_each_cell(p, lnext(list_head(parent_rel->pathlist)))
	{
		Path	   *path = (Path *) lfirst(p);
		int			cmp;

		cmp = compare_path_costs(cheapest_startup_path, path, STARTUP_COST);
		if (cmp > 0 ||
			(cmp == 0 &&
			 compare_pathkeys(cheapest_startup_path->pathkeys,
							  path->pathkeys) == PATHKEYS_BETTER2))
			cheapest_startup_path = path;

		cmp = compare_path_costs(cheapest_total_path, path, TOTAL_COST);
		if (cmp > 0 ||
			(cmp == 0 &&
			 compare_pathkeys(cheapest_total_path->pathkeys,
							  path->pathkeys) == PATHKEYS_BETTER2))
			cheapest_total_path = path;
	}

	parent_rel->cheapest_startup_path = cheapest_startup_path;
	parent_rel->cheapest_total_path = cheapest_total_path;

}                               /* set_cheapest */


/*
 * cdb_set_cheapest_dedup
 *
 * Called by set_cheapest() to augment a RelOptInfo with paths that
 * provide for subquery duplicate suppression.
 */
static void
cdb_set_cheapest_dedup(PlannerInfo *root, RelOptInfo *rel)
{
    CdbRelDedupInfo    *dedup = rel->dedup_info;
    ListCell           *cell;
	List               *save_pathlist;
	Path               *save_cheapest_startup;
	Path               *save_cheapest_total;
    Path               *subpath;
    UniquePath         *upath = NULL;

    /*
     * If rel has only 1 row, joining with it cannot produce duplicates.
     *
     * Some paths could have been added to the later_dedup_pathlist before
     * the discovery that at most one row can satisfy the predicates.
     * Transfer any such to the main pathlist.
     */
    if (rel->onerow)
    {
        /* Seize the later_dedup_pathlist. */
        save_pathlist = dedup->later_dedup_pathlist;
        dedup->later_dedup_pathlist = NIL;

        /* Add its paths to the main pathlist. */
        foreach(cell, save_pathlist)
            add_path(root, rel, (Path *)lfirst(cell));

        /* Verify that the paths weren't added to the wrong list. */
        Insist(!dedup->later_dedup_pathlist);
        return;
    }

    /*
     * Pick out the lowest-cost paths in later_dedup_pathlist.
     *
     * The choice is made by a recursive call to set_cheapest(), for which we
     * momentarily hijack the main pathlist fields in RelOptInfo.
     */
    save_pathlist = rel->pathlist;
    save_cheapest_startup = rel->cheapest_startup_path;
    save_cheapest_total = rel->cheapest_total_path;
    rel->pathlist = dedup->later_dedup_pathlist;
    rel->cheapest_startup_path = NULL;
    rel->cheapest_total_path = NULL;
    dedup->later_dedup_pathlist = NIL;  /* to stop the recursion */

    set_cheapest(root, rel);

    dedup->later_dedup_pathlist = rel->pathlist;
    dedup->cheapest_startup_path = rel->cheapest_startup_path;
    dedup->cheapest_total_path = rel->cheapest_total_path;
    rel->pathlist = save_pathlist;
    rel->cheapest_startup_path = save_cheapest_startup;
    rel->cheapest_total_path = save_cheapest_total;

    /*
     * If rel is the final result of a pulled-up uncorrelated "= ANY" subquery,
     * consider a path that yields the distinct rows of the subquery.
     *
     * For an uncorrelated "= ANY" subquery, we'll consider plans in which
     * we remove duplicates from the rel containing the subquery result,
     * before that is joined with any other rels.  An "=" predicate (which
     * was created by convert_IN_to_join()) ensures that at most one row of
     * the duplicate-free subquery result can join with any one row of the
     * subquery's parent query.
     *
     * (Formerly in PostgreSQL this pre-join duplicate suppression was invoked
     * via special jointype codes: JOIN_UNIQUE_OUTER and JOIN_UNIQUE_INNER.)
     *
     * CDB TODO: We wouldn't have to do anything special if we knew the
     * subquery result rows are distinct because of a UNIQUE constraint.
     *
     * CDB TODO: Correlated subquery could use JOIN_UNIQUE if all outer
     * refs can be pulled up above the UniquePath operator, or if umethod
     * is UNIQUE_PATH_NOOP.
     *
     * CDB TODO: Avoid sorting when the subpath is ordered on the
     * sub_targetlist columns.
     */
    if (dedup->join_unique_ininfo)
    {
		Assert(dedup->join_unique_ininfo->semi_rhs_exprs);
		/* Top off the subpath with DISTINCT ON the result columns. */
		upath = create_unique_exprlist_path(root,
											dedup->cheapest_total_path,
											dedup->join_unique_ininfo->semi_rhs_exprs,
											dedup->join_unique_ininfo->semi_operators);
		/* Add to rel's main pathlist. */
		add_path(root, rel, (Path *)upath);
    }

    /*
     * Consider post-join duplicate removal.
     *
     * Post-join duplicate removal is done by inserting a Unique operator that
     * is DISTINCT ON the unique row identifiers of those relids that do *not*
     * derive from flattened subqueries.
     *
     * If there is at least one subquery whose required inputs are all present
     * in this rel, and no subquery having some inputs present and some inputs
     * absent, then cdb_make_rel_dedup_info() has set the try_postjoin_dedup
     * flag and filled in the prejoin_dedup_subqrelids field giving the relids
     * of those subqueries' own tables.  Those subqueries are candidates for
     * late duplicate removal.
     *
     * NB: "Required inputs" means the sublink's lefthand relids, the
     * subquery's own tables (the sublink's righthand relids), and the
     * relids of outer references.
     */
    else if (dedup->try_postjoin_dedup)
    {
        Relids  distinct_relids = NULL;

        Assert(dedup->prejoin_dedup_subqrelids);

        /* hide later_dedup_pathlist while we walk it, so add_path can't upd. */
        save_pathlist = dedup->later_dedup_pathlist;
        dedup->later_dedup_pathlist = NIL;

        foreach(cell, save_pathlist)
        {
            subpath = (Path *)lfirst(cell);
            Assert(!subpath->subq_complete);

            /*
             * When one subplan of a join is the source of all of the relids in
             * postjoin_dedup_allrelids, we can assume post-join duplicate
             * removal has already been considered upstream.  We could keep on
             * considering it after every subsequent join, to insert the Unique
             * op at the point where it is cheapest.  But we do not attempt
             * that, because at present the join result size estimates are
             * untrustworthy.
             *
             * CDB TODO: Could remove this after fixing the join selectivity.
             */
            if (cdb_subpath_tried_postjoin_dedup(subpath,
                                                 dedup->prejoin_dedup_subqrelids))
                continue;

            /*
             * Build set of tables whose row ids will be the DISTINCT ON keys.
             * The bitmapset object will be shared; don't free it!
             */
            if (!distinct_relids)
                distinct_relids =
                    bms_difference(rel->relids, dedup->prejoin_dedup_subqrelids);

            /*
             * Top off the subpath with DISTINCT ON the non-subquery row ids.
             * Add to rel's main pathlist.
             */
            upath = create_unique_rowid_path(root, subpath, distinct_relids);
            add_path(root, rel, (Path *)upath);
        }

        /* Verify that our new paths haven't gone into the wrong pathlist. */
        Insist(!dedup->later_dedup_pathlist);
        dedup->later_dedup_pathlist = save_pathlist;
    }

    /*
     * Don't consider plans that further delay duplicate suppression
     * after all subqueries have been fully evaluated.  (If a later join
     * happens to reduce the amount of data, duplicate removal could be
     * cheaper afterward and it would be better to wait.  However, we
     * don't currently trust the join result size estimates that much.)
     *
     * CDB TODO: If current query level has GROUP BY with only MIN/MAX aggs,
     * DISTINCT or LIMIT 1, retain these paths... but move them into the rel's
     * main pathlist since no extra step will be needed downstream for dedup.
     */
    if (dedup->no_more_subqueries)
        dedup->later_dedup_pathlist = NIL;

}                             /* cdb_set_cheapest_dedup */


/*
 * cdb_is_path_deduped
 *
 * Returns true if there is no flattened subquery whose tables are all present,
 * or if the given path takes care of all the duplicate suppression for such
 * subqueries, so that they require no further action downstream.
 *
 * Returns false if some flattened subquery has all tables present but still
 * needs duplicate suppression to be done downstream.
 *
 * Not affected by subqueries which have some but not all tables present.
 */
typedef struct CdbIsPathDedupedCtx
{
    Relids          all_subqrelids;
    Relids          deduped_relids;
    bool            deduped_unshared;
} CdbIsPathDedupedCtx;

static CdbVisitOpt
cdb_is_path_deduped_walker(Path *path, void* context)
{
    CdbIsPathDedupedCtx    *ctx = (CdbIsPathDedupedCtx *)context;
    RelOptInfo             *rel = path->parent;
    Relids                  deduped;

    /* Skip path unless rel includes all relids of some flattened subquery. */
    if (!rel->dedup_info ||
        !rel->dedup_info->prejoin_dedup_subqrelids)
        return CdbVisit_Skip;       /* no relids to add; don't visit children */

    /* Dedup usually wraps up all subqueries whose tables are all present. */
    deduped = rel->dedup_info->prejoin_dedup_subqrelids;

    /* Already satisfied? */
    if (path->subq_complete)
        goto put_deduped_result;

    /* If rel has only 1 row, joining with it cannot produce duplicates. */
    if (rel->onerow)
        goto put_deduped_result;

    switch (path->pathtype)
    {
        /*
         * Unique op is used only for subquery duplicate suppression, at present.
         * Subqueries whose relids are covered by a Unique op don't require
         * further duplicate suppression downstream.
         */
        case T_Unique:
            goto put_deduped_result;

        /*
         * Join with jointype JOIN_SEMI will suppress duplicates for all
         * subqueries whose relids are covered by the join's inner rel.
         */
        case T_HashJoin:
	    case T_MergeJoin:
	    case T_NestLoop:
        {
            JoinPath   *joinpath = (JoinPath *)path;
            RelOptInfo *inner_rel = joinpath->innerjoinpath->parent;
            CdbVisitOpt status;

            /* Visit the outer subpath. */
            status = pathnode_walk_node(joinpath->outerjoinpath,
                                        cdb_is_path_deduped_walker,
                                        ctx);
            if (status != CdbVisit_Walk)
                return status;

            /* Subqueries on inner side of JOIN_SEMI can't cause duplicates. */
            if (joinpath->jointype == JOIN_SEMI)
            {
                if (!inner_rel->dedup_info ||
                    !inner_rel->dedup_info->prejoin_dedup_subqrelids)
                    return CdbVisit_Walk;
                deduped = inner_rel->dedup_info->prejoin_dedup_subqrelids;
                goto put_deduped_result;
            }

            /* Visit the inner subpath. */
            return pathnode_walk_node(joinpath->innerjoinpath,
                                      cdb_is_path_deduped_walker,
                                      ctx);
        }

        default:
            break;
    }
    return CdbVisit_Walk;       /* onward to visit children */

    /*
     * Add this Path node's deduped relids to set.
     * To reduce allocations, try to return an existing bitmapset.
     */
put_deduped_result:
    if (!ctx->deduped_relids)
    {
        ctx->deduped_relids = deduped;
        ctx->deduped_unshared = false;
    }
    else if (ctx->deduped_unshared)
        ctx->deduped_relids = bms_add_members(ctx->deduped_relids, deduped);
    else
    {
        ctx->deduped_relids = bms_union(ctx->deduped_relids, deduped);
        ctx->deduped_unshared = true;
    }

    /*
     * All found?  Then stop.
     */
    if (bms_equal(ctx->deduped_relids, ctx->all_subqrelids))
        return CdbVisit_Success;

    /* Done with this branch; don't visit children, but proceed to next. */
    return CdbVisit_Skip;
}                               /* cdb_is_path_deduped_walker */

static bool
cdb_is_path_deduped(Path *path)
{
    CdbIsPathDedupedCtx context;
    CdbVisitOpt         status;

    if (!path->parent->dedup_info ||
        !path->parent->dedup_info->prejoin_dedup_subqrelids)
        return true;

    context.all_subqrelids = path->parent->dedup_info->prejoin_dedup_subqrelids;
    context.deduped_relids = NULL;
    context.deduped_unshared = false;

    status = pathnode_walk_node(path, cdb_is_path_deduped_walker, &context);

    if (context.deduped_unshared)
        bms_free(context.deduped_relids);

    return status == CdbVisit_Success;
}                               /* cdb_is_path_deduped */


/*
 * cdb_subpath_tried_postjoin_dedup
 *
 * Returns true if the given Path has a subpath in which post-join duplicate
 * suppression can be assumed to have been considered already for exactly the
 * given set of subquery relids.
 */
typedef struct CdbSubpathTriedPostjoinDedupCtx
{
    RelOptInfo     *given_rel;
    Relids          subqrelids;
} CdbSubpathTriedPostjoinDedupCtx;

static CdbVisitOpt
cdb_subpath_tried_postjoin_dedup_walker(Path *path, void* context)
{
    CdbSubpathTriedPostjoinDedupCtx *ctx = (CdbSubpathTriedPostjoinDedupCtx *)context;
    RelOptInfo *rel = path->parent;

    /* Descend thru parent_rel's Path nodes to top Path node of an input rel. */
    if (rel == ctx->given_rel)
        return CdbVisit_Walk;

    /* If none of the given relids come from this input rel, advance to next. */
    if (!bms_overlap(ctx->subqrelids, rel->relids))
        return CdbVisit_Skip;

    /* Succeed if rel is marked for post-join dedup of given subqueries. */
    if (rel->dedup_info &&
        rel->dedup_info->try_postjoin_dedup &&
        bms_equal(rel->dedup_info->prejoin_dedup_subqrelids, ctx->subqrelids))
        return CdbVisit_Success;

    return CdbVisit_Failure;
}                               /* cdb_subpath_tried_postjoin_dedup_walker */

static bool
cdb_subpath_tried_postjoin_dedup(Path *path, Relids subqrelids)
{
    CdbSubpathTriedPostjoinDedupCtx context;
    CdbVisitOpt             status;

    context.given_rel = path->parent;
    context.subqrelids = subqrelids;

    status = pathnode_walk_kids(path, cdb_subpath_tried_postjoin_dedup_walker, &context);

    return (status == CdbVisit_Success);
}                               /* cdb_subpath_tried_postjoin_dedup */


/*
 * add_path
 *	  Consider a potential implementation path for the specified parent rel,
 *	  and add it to the rel's pathlist if it is worthy of consideration.
 *	  A path is worthy if it has either a better sort order (better pathkeys)
 *	  or cheaper cost (on either dimension) than any of the existing old paths.
 *
 *	  We also remove from the rel's pathlist any old paths that are dominated
 *	  by new_path --- that is, new_path is both cheaper and at least as well
 *	  ordered.
 *
 *	  The pathlist is kept sorted by TOTAL_COST metric, with cheaper paths
 *	  at the front.  No code depends on that for correctness; it's simply
 *	  a speed hack within this routine.  Doing it that way makes it more
 *	  likely that we will reject an inferior path after a few comparisons,
 *	  rather than many comparisons.
 *
 *	  NOTE: discarded Path objects are immediately pfree'd to reduce planner
 *	  memory consumption.  We dare not try to free the substructure of a Path,
 *	  since much of it may be shared with other Paths or the query tree itself;
 *	  but just recycling discarded Path nodes is a very useful savings in
 *	  a large join tree.  We can recycle the List nodes of pathlist, too.
 *
 *    NB: The Path that is passed to add_path() must be considered invalid
 *    upon return, and not touched again by the caller, because we free it
 *    if we already know of a better path.  Likewise, a Path that is passed
 *    to add_path() must not be shared as a subpath of any other Path of the
 *    same join level.
 *
 *	  BUT: we do not pfree IndexPath objects, since they may be referenced as
 *	  children of BitmapHeapPaths as well as being paths in their own right.
 *
 * 'parent_rel' is the relation entry to which the path corresponds.
 * 'new_path' is a potential path for parent_rel.
 *
 * Returns nothing, but modifies parent_rel->pathlist.
 */
void
add_path(PlannerInfo *root, RelOptInfo *parent_rel, Path *new_path)
{
	bool		accept_new = true;		/* unless we find a superior old path */
	ListCell   *insert_after = NULL;	/* where to insert new item */
	ListCell   *p1_prev = NULL;
	ListCell   *p1;
    List      **which_pathlist;
    List       *pathlist;

	/*
	 * This is a convenient place to check for query cancel --- no part of the
	 * planner goes very long without calling add_path().
	 */
	CHECK_FOR_INTERRUPTS();

    if (!new_path)
        return;

    Assert(cdbpathlocus_is_valid(new_path->locus));

   /*
     * CDB: Can we limit the rel to 1 row without affecting the query result?
     *
     * If rel's targetlist is empty, then only the rel's row count affects
     * downstream processing.  And if the rel consists only of subquery tables
     * which have not been joined with tables of the current query level, then
     * all that matters is whether the number of result rows is zero or nonzero.
     * We'll insert LIMIT 1 on top of every path.
     *
     * This situation won't come up often.  Flattened uncorrelated EXISTS
     * subqueries would be a typical example, if not for the fact that they
     * are pulled out for separate optimization as InitPlans.
     */
    if (!parent_rel->reltargetlist &&
        !parent_rel->onerow &&
        !bms_overlap(parent_rel->relids, root->currlevel_relids))
        new_path = (Path *)create_limit1_path(root, new_path);

    /*
     * CDB: Note whether duplicate suppression has been completed for all
     * flattened subqueries whose tables are all present.
     *
     * Paths that await later addition of duplicate suppression are kept in a
     * separate pathlist.
     *
     * NB: We set subq_complete false if all tables of a not-deduped subquery
     * are present, even if the not-deduped subquery is joined with a deduped
     * one.  The earlier deduping can't help the later one, except perhaps by
     * reducing the number of rows; so don't bother to keep flags about it.
     */
    Assert(!new_path->subq_complete);
    if (!parent_rel->dedup_info ||
        cdb_is_path_deduped(new_path))
    {
        new_path->subq_complete = true;
        which_pathlist = &parent_rel->pathlist;
        pathlist = *which_pathlist;
    }
    else
    {
        new_path->subq_complete = false;
        which_pathlist = &parent_rel->dedup_info->later_dedup_pathlist;
        pathlist = *which_pathlist;
    }

	/*
	 * Loop to check proposed new path against old paths.  Note it is possible
	 * for more than one old path to be tossed out because new_path dominates
	 * it.
	 */
	p1 = list_head(pathlist);		    /* cannot use foreach here */
	while (p1 != NULL)
	{
		Path	   *old_path = (Path *) lfirst(p1);
		bool		remove_old = false; /* unless new proves superior */
		int			costcmp;

		/*
		 * As of Postgres 8.0, we use fuzzy cost comparison to avoid wasting
		 * cycles keeping paths that are really not significantly different in
		 * cost.
		 */
		costcmp = compare_fuzzy_path_costs(new_path, old_path, TOTAL_COST);

		/*
		 * If the two paths compare differently for startup and total cost,
		 * then we want to keep both, and we can skip the (much slower)
		 * comparison of pathkeys.	If they compare the same, proceed with the
		 * pathkeys comparison.  Note: this test relies on the fact that
		 * compare_fuzzy_path_costs will only return 0 if both costs are
		 * effectively equal (and, therefore, there's no need to call it twice
		 * in that case).
		 */
		if (costcmp == 0 ||
			costcmp == compare_fuzzy_path_costs(new_path, old_path,
												STARTUP_COST))
		{
			/* Still a tie?  See which path has better pathkeys. */
			switch (compare_pathkeys(new_path->pathkeys, old_path->pathkeys))
			{
				case PATHKEYS_EQUAL:
					if (costcmp < 0)
						remove_old = true;		/* new dominates old */
					else if (costcmp > 0)
						accept_new = false;		/* old dominates new */
					else
					{
						/*
						 * Same pathkeys, and fuzzily the same cost, so keep
						 * just one --- but we'll do an exact cost comparison
						 * to decide which.
						 */
						if (compare_path_costs(new_path, old_path,
											   TOTAL_COST) < 0)
							remove_old = true;	/* new dominates old */
						else
							accept_new = false; /* old equals or dominates new */
					}
					break;
				case PATHKEYS_BETTER1:
					if (costcmp <= 0)
						remove_old = true;		/* new dominates old */
					break;
				case PATHKEYS_BETTER2:
					if (costcmp >= 0)
						accept_new = false;		/* old dominates new */
					break;
				case PATHKEYS_DIFFERENT:
					/* keep both paths, since they have different ordering */
					break;
			}
		}

		/*
		 * Remove current element from pathlist if dominated by new.
		 */
		if (remove_old)
		{
			pathlist = list_delete_cell(pathlist, p1, p1_prev);

			/*
			 * Delete the data pointed-to by the deleted cell, if possible
			 */
			if (!IsA(old_path, IndexPath))
				pfree(old_path);

			/* Advance list pointer */
			if (p1_prev)
				p1 = lnext(p1_prev);
			else
				p1 = list_head(pathlist);
		}
		else
		{
			/* new belongs after this old path if it has cost >= old's */
			if (costcmp >= 0)
				insert_after = p1;
			/* Advance list pointers */
			p1_prev = p1;
			p1 = lnext(p1);
		}

		/*
		 * If we found an old path that dominates new_path, we can quit
		 * scanning the pathlist; we will not add new_path, and we assume
		 * new_path cannot dominate any other elements of the pathlist.
		 */
		if (!accept_new)
			break;
	}

	if (accept_new)
	{
		/* Accept the new path: insert it at proper place in pathlist */
		if (insert_after)
			lappend_cell(pathlist, insert_after, new_path);
		else
			pathlist = lcons(new_path, pathlist);
	}
	else
	{
		/* Reject and recycle the new path */
		if (!IsA(new_path, IndexPath))
			pfree(new_path);
	}

    /* Put back the updated pathlist ptr. */
    *which_pathlist = pathlist;
}                               /* add_path */


/*****************************************************************************
 *		PATH NODE CREATION ROUTINES
 *****************************************************************************/

/*
 * create_seqscan_path
 *	  Creates a path corresponding to a sequential scan, returning the
 *	  pathnode.
 */
Path *
create_seqscan_path(PlannerInfo *root, RelOptInfo *rel)
{
	Path	   *pathnode = makeNode(Path);

	pathnode->pathtype = T_SeqScan;
	pathnode->parent = rel;
	pathnode->pathkeys = NIL;	/* seqscan has unordered result */

    pathnode->locus = cdbpathlocus_from_baserel(root, rel);
    pathnode->motionHazard = false;
	pathnode->rescannable = true;
	pathnode->sameslice_relids = rel->relids;

	cost_seqscan(pathnode, root, rel);

	return pathnode;
}

/* 
 * Create a path for scanning an append-only table
 */
AppendOnlyPath *
create_appendonly_path(PlannerInfo *root, RelOptInfo *rel)
{
	AppendOnlyPath	   *pathnode = makeNode(AppendOnlyPath);

	pathnode->path.pathtype = T_AppendOnlyScan;
	pathnode->path.parent = rel;
	pathnode->path.pathkeys = NIL;	/* seqscan has unordered result */

    pathnode->path.locus = cdbpathlocus_from_baserel(root, rel);
    pathnode->path.motionHazard = false;
	pathnode->path.rescannable = true;
	pathnode->path.sameslice_relids = rel->relids;

	cost_appendonlyscan(pathnode, root, rel);

	return pathnode;
}

/* 
 * Create a path for scanning an append-only table
 */
AOCSPath *
create_aocs_path(PlannerInfo *root, RelOptInfo *rel)
{
	AOCSPath	   *pathnode = makeNode(AOCSPath);
	
	pathnode->path.pathtype = T_AOCSScan;
	pathnode->path.parent = rel;
	pathnode->path.pathkeys = NIL;	/* seqscan has unordered result */

	pathnode->path.locus = cdbpathlocus_from_baserel(root, rel);
	pathnode->path.motionHazard = false;
	pathnode->path.rescannable = true;
	pathnode->path.sameslice_relids = rel->relids;
	
	cost_aocsscan(pathnode, root, rel);
	return pathnode;
}
/* 
* Create a path for scanning an external table
 */
ExternalPath *
create_external_path(PlannerInfo *root, RelOptInfo *rel)
{
	ExternalPath   *pathnode = makeNode(ExternalPath);
	
	pathnode->path.pathtype = T_ExternalScan;
	pathnode->path.parent = rel;
	pathnode->path.pathkeys = NIL;	/* external scan has unordered result */
	
    pathnode->path.locus = cdbpathlocus_from_baserel(root, rel); 
    pathnode->path.motionHazard = false;

	/*
	 * Mark external tables as non-rescannable. While rescan is possible,
	 * it can lead to surprising results if the external table produces
	 * different results when invoked twice.
	 */
	pathnode->path.rescannable = false;
	pathnode->path.sameslice_relids = rel->relids;

	cost_externalscan(pathnode, root, rel);
	
	return pathnode;
}


/*
 * create_index_path
 *	  Creates a path node for an index scan.
 *
 * 'index' is a usable index.
 * 'clause_groups' is a list of lists of RestrictInfo nodes
 *			to be used as index qual conditions in the scan.
 * 'pathkeys' describes the ordering of the path.
 * 'indexscandir' is ForwardScanDirection or BackwardScanDirection
 *			for an ordered index, or NoMovementScanDirection for
 *			an unordered index.
 * 'outer_rel' is the outer relation if this is a join inner indexscan path.
 *			(pathkeys and indexscandir are ignored if so.)	NULL if not.
 *
 * Returns the new path node.
 */
IndexPath *
create_index_path(PlannerInfo *root,
				  IndexOptInfo *index,
				  List *clause_groups,
				  List *pathkeys,
				  ScanDirection indexscandir,
				  RelOptInfo *outer_rel)
{
	IndexPath  *pathnode = makeNode(IndexPath);
	RelOptInfo *rel = index->rel;
	List	   *indexquals,
			   *allclauses;

	/*
	 * For a join inner scan, there's no point in marking the path with any
	 * pathkeys, since it will only ever be used as the inner path of a
	 * nestloop, and so its ordering does not matter.  For the same reason we
	 * don't really care what order it's scanned in.  (We could expect the
	 * caller to supply the correct values, but it's easier to force it here.)
	 */
	if (outer_rel != NULL)
	{
		pathkeys = NIL;
		indexscandir = NoMovementScanDirection;
	}

	pathnode->path.pathtype = T_IndexScan;
	pathnode->path.parent = rel;
	pathnode->path.pathkeys = pathkeys;

	/* Convert clauses to indexquals the executor can handle */
	indexquals = expand_indexqual_conditions(index, clause_groups);

	/* Flatten the clause-groups list to produce indexclauses list */
	allclauses = flatten_clausegroups_list(clause_groups);

	/* Fill in the pathnode */
	pathnode->indexinfo = index;
	pathnode->indexclauses = allclauses;
	pathnode->indexquals = indexquals;

	pathnode->isjoininner = (outer_rel != NULL);
	pathnode->indexscandir = indexscandir;

	if (outer_rel != NULL)
	{
		/*
		 * We must compute the estimated number of output rows for the
		 * indexscan.  This is less than rel->rows because of the additional
		 * selectivity of the join clauses.  Since clause_groups may contain
		 * both restriction and join clauses, we have to do a set union to get
		 * the full set of clauses that must be considered to compute the
		 * correct selectivity.  (Without the union operation, we might have
		 * some restriction clauses appearing twice, which'd mislead
		 * clauselist_selectivity into double-counting their selectivity.
		 * However, since RestrictInfo nodes aren't copied when linking them
		 * into different lists, it should be sufficient to use pointer
		 * comparison to remove duplicates.)
		 *
		 * Note that we force the clauses to be treated as non-join clauses
		 * during selectivity estimation.
		 */
		allclauses = list_union_ptr(rel->baserestrictinfo, allclauses);
		pathnode->rows = rel->tuples *
			clauselist_selectivity(root,
								   allclauses,
								   rel->relid,	/* do not use 0! */
								   JOIN_INNER,
								   NULL,
								   false /* use_damping */);
		/* Like costsize.c, force estimate to be at least one row */
		pathnode->rows = clamp_row_est(pathnode->rows);
	}
	else
	{
		/*
		 * The number of rows is the same as the parent rel's estimate, since
		 * this isn't a join inner indexscan.
		 */
		pathnode->rows = rel->rows;
	}

	/* Distribution is same as the base table. */
	pathnode->path.locus = cdbpathlocus_from_baserel(root, rel);
	pathnode->path.motionHazard = false;
	pathnode->path.rescannable = true;
	pathnode->path.sameslice_relids = rel->relids;

	cost_index(pathnode, root, index, indexquals, outer_rel);

	return pathnode;
}

/*
 * create_bitmap_heap_path
 *	  Creates a path node for a bitmap scan.
 *
 * 'bitmapqual' is a tree of IndexPath, BitmapAndPath, and BitmapOrPath nodes.
 *
 * If this is a join inner indexscan path, 'outer_rel' is the outer relation,
 * and all the component IndexPaths should have been costed accordingly.
 */
BitmapHeapPath *
create_bitmap_heap_path(PlannerInfo *root,
						RelOptInfo *rel,
						Path *bitmapqual,
						RelOptInfo *outer_rel)
{
	BitmapHeapPath *pathnode = makeNode(BitmapHeapPath);

	pathnode->path.pathtype = T_BitmapHeapScan;
	pathnode->path.parent = rel;
	pathnode->path.pathkeys = NIL;		/* always unordered */

    /* Distribution is same as the base table. */
    pathnode->path.locus = cdbpathlocus_from_baserel(root, rel);
    pathnode->path.motionHazard = false;
    pathnode->path.rescannable = true;
	pathnode->path.sameslice_relids = rel->relids;

	pathnode->bitmapqual = bitmapqual;
	pathnode->isjoininner = (outer_rel != NULL);

	if (pathnode->isjoininner)
	{
		/*
		 * We must compute the estimated number of output rows for the
		 * indexscan.  This is less than rel->rows because of the additional
		 * selectivity of the join clauses.  We make use of the selectivity
		 * estimated for the bitmap to do this; this isn't really quite right
		 * since there may be restriction conditions not included in the
		 * bitmap ...
		 */
		Cost		indexTotalCost;
		Selectivity indexSelectivity;

		cost_bitmap_tree_node(bitmapqual, &indexTotalCost, &indexSelectivity);
		pathnode->rows = rel->tuples * indexSelectivity;
		if (pathnode->rows > rel->rows)
			pathnode->rows = rel->rows;
		/* Like costsize.c, force estimate to be at least one row */
		pathnode->rows = clamp_row_est(pathnode->rows);
	}
	else
	{
		/*
		 * The number of rows is the same as the parent rel's estimate, since
		 * this isn't a join inner indexscan.
		 */
		pathnode->rows = rel->rows;
	}

	cost_bitmap_heap_scan(&pathnode->path, root, rel, bitmapqual, outer_rel);

	return pathnode;
}

/*
 * create_bitmap_appendonly_path
 *	  Creates a path node for a bitmap Append-Only row table scan.
 *
 * 'bitmapqual' is a tree of IndexPath, BitmapAndPath, and BitmapOrPath nodes.
 *
 * If this is a join inner indexscan path, 'outer_rel' is the outer relation,
 * and all the component IndexPaths should have been costed accordingly.
 *
 * NOTE: This is mostly the same as the create_bitmap_heap_path routine.
 */
BitmapAppendOnlyPath *
create_bitmap_appendonly_path(PlannerInfo *root,
							  RelOptInfo *rel,
							  Path *bitmapqual,
							  RelOptInfo *outer_rel,
							  bool isAORow)
{
	BitmapAppendOnlyPath *pathnode = makeNode(BitmapAppendOnlyPath);

	pathnode->path.pathtype = T_BitmapAppendOnlyScan;
	pathnode->path.parent = rel;
	pathnode->path.pathkeys = NIL;		/* always unordered */

    /* Distribution is same as the base table. */
    pathnode->path.locus = cdbpathlocus_from_baserel(root, rel);
    pathnode->path.motionHazard = false;
    pathnode->path.rescannable = true;
	pathnode->path.sameslice_relids = rel->relids;

	pathnode->bitmapqual = bitmapqual;
	pathnode->isjoininner = (outer_rel != NULL);
	pathnode->isAORow = isAORow;

	if (pathnode->isjoininner)
	{
		/*
		 * We must compute the estimated number of output rows for the
		 * indexscan.  This is less than rel->rows because of the additional
		 * selectivity of the join clauses.  We make use of the selectivity
		 * estimated for the bitmap to do this; this isn't really quite right
		 * since there may be restriction conditions not included in the
		 * bitmap ...
		 */
		Cost		indexTotalCost;
		Selectivity indexSelectivity;

		cost_bitmap_tree_node(bitmapqual, &indexTotalCost, &indexSelectivity);
		pathnode->rows = rel->tuples * indexSelectivity;
		if (pathnode->rows > rel->rows)
			pathnode->rows = rel->rows;
		/* Like costsize.c, force estimate to be at least one row */
		pathnode->rows = clamp_row_est(pathnode->rows);
	}
	else
	{
		/*
		 * The number of rows is the same as the parent rel's estimate, since
		 * this isn't a join inner indexscan.
		 */
		pathnode->rows = rel->rows;
	}

	cost_bitmap_appendonly_scan(&pathnode->path, root, rel, bitmapqual, outer_rel);

	return pathnode;
}

/*
 * create_bitmap_and_path
 *	  Creates a path node representing a BitmapAnd.
 */
BitmapAndPath *
create_bitmap_and_path(PlannerInfo *root,
					   RelOptInfo *rel,
					   List *bitmapquals)
{
	BitmapAndPath *pathnode = makeNode(BitmapAndPath);

	pathnode->path.pathtype = T_BitmapAnd;
	pathnode->path.parent = rel;
	pathnode->path.pathkeys = NIL;		/* always unordered */

	pathnode->bitmapquals = bitmapquals;

	/* this sets bitmapselectivity as well as the regular cost fields: */
	cost_bitmap_and_node(pathnode, root);

	return pathnode;
}

/*
 * create_bitmap_or_path
 *	  Creates a path node representing a BitmapOr.
 */
BitmapOrPath *
create_bitmap_or_path(PlannerInfo *root,
					  RelOptInfo *rel,
					  List *bitmapquals)
{
	BitmapOrPath *pathnode = makeNode(BitmapOrPath);

	pathnode->path.pathtype = T_BitmapOr;
	pathnode->path.parent = rel;
	pathnode->path.pathkeys = NIL;		/* always unordered */

	pathnode->bitmapquals = bitmapquals;

	/* this sets bitmapselectivity as well as the regular cost fields: */
	cost_bitmap_or_node(pathnode, root);

	return pathnode;
}

/*
 * create_tidscan_path
 *	  Creates a path corresponding to a scan by TID, returning the pathnode.
 */
TidPath *
create_tidscan_path(PlannerInfo *root, RelOptInfo *rel, List *tidquals)
{
	TidPath    *pathnode = makeNode(TidPath);

	pathnode->path.pathtype = T_TidScan;
	pathnode->path.parent = rel;
	pathnode->path.pathkeys = NIL;

	pathnode->tidquals = tidquals;

    /* Distribution is same as the base table. */
    pathnode->path.locus = cdbpathlocus_from_baserel(root, rel);
    pathnode->path.motionHazard = false;
    pathnode->path.rescannable = true;
	pathnode->path.sameslice_relids = rel->relids;

	cost_tidscan(&pathnode->path, root, rel, tidquals);

	return pathnode;
}

/*
 * create_append_path
 *	  Creates a path corresponding to an Append plan, returning the
 *	  pathnode.
 */
AppendPath *
create_append_path(PlannerInfo *root, RelOptInfo *rel, List *subpaths)
{
	AppendPath *pathnode = makeNode(AppendPath);
	ListCell   *l;

	pathnode->path.pathtype = T_Append;
	pathnode->path.parent = rel;
	pathnode->path.pathkeys = NIL;		/* result is always considered
										 * unsorted */
	pathnode->subpaths = NIL;

    pathnode->path.motionHazard = false;
    pathnode->path.rescannable = true;

	pathnode->path.startup_cost = 0;
	pathnode->path.total_cost = 0;

    /* If no subpath, any worker can execute this Append.  Result has 0 rows. */
    if (!subpaths)
        CdbPathLocus_MakeGeneral(&pathnode->path.locus);
    else
	{
		bool fIsNotPartitioned = false;
		bool fIsPartitionInEntry = false;

		/*
		 * Do a first pass over the children to determine if
		 * there's any child which is not partitioned, i.e. a bottleneck or
		 * replicated.
		 */
		foreach(l, subpaths)
		{
			Path *subpath = (Path *) lfirst(l);

			if (CdbPathLocus_IsBottleneck(subpath->locus) ||
				CdbPathLocus_IsReplicated(subpath->locus))
			{
				fIsNotPartitioned = true;

				/* check whether any partition is on entry db */
				if (CdbPathLocus_IsEntry(subpath->locus))
				{
					fIsPartitionInEntry = true;
					break;
				}
			}
		}

		foreach(l, subpaths)
		{
			Path	       *subpath = (Path *) lfirst(l);
			CdbPathLocus    projectedlocus;

			/*
			 * In case any of the children is not partitioned convert all
			 * children to have singleQE locus
			 */
			if (fIsNotPartitioned)
			{
				/*
				 * if any partition is on entry db, we should gather all the
				 * partitions to QD to do the append
				 */
				if (fIsPartitionInEntry)
				{
					if (!CdbPathLocus_IsEntry(subpath->locus))
					{
						CdbPathLocus singleEntry;
						CdbPathLocus_MakeEntry(&singleEntry);

						subpath = cdbpath_create_motion_path(root, subpath, NIL, false, singleEntry);
					}
				}
				else /* fIsNotPartitioned true, fIsPartitionInEntry false */
				{
					if (!CdbPathLocus_IsSingleQE(subpath->locus))
					{
						CdbPathLocus    singleQE;
						CdbPathLocus_MakeSingleQE(&singleQE);

						subpath = cdbpath_create_motion_path(root, subpath, NIL, false, singleQE);
					}
				}
			}

			/* Transform subpath locus into the appendrel's space for comparison. */
			if (subpath->parent == rel ||
				subpath->parent->reloptkind != RELOPT_OTHER_MEMBER_REL)
				projectedlocus = subpath->locus;
			else
				projectedlocus =
					cdbpathlocus_pull_above_projection(root,
													   subpath->locus,
													   subpath->parent->relids,
													   subpath->parent->reltargetlist,
													   rel->reltargetlist,
													   rel->relid);

			if (l == list_head(subpaths))	/* first node? */
				pathnode->path.startup_cost = subpath->startup_cost;
			pathnode->path.total_cost += subpath->total_cost;

			/*
			 * CDB: If all the scans are distributed alike, set
			 * the result locus to match.  Otherwise, if all are partitioned,
			 * set it to strewn.  A mixture of partitioned and non-partitioned
			 * scans should not occur after above correction;
			 *
			 * CDB TODO: When the scans are not all partitioned alike, and the
			 * result is joined with another rel, consider pushing the join
			 * below the Append so that child tables that are properly
			 * distributed can be joined in place.
			 */
			if (l == list_head(subpaths))
				pathnode->path.locus = projectedlocus;
			else if (cdbpathlocus_compare(CdbPathLocus_Comparison_Equal,
										  pathnode->path.locus, projectedlocus))
			{}
			else if (CdbPathLocus_IsPartitioned(pathnode->path.locus) &&
					 CdbPathLocus_IsPartitioned(projectedlocus))
				CdbPathLocus_MakeStrewn(&pathnode->path.locus);
			else
				ereport(ERROR, (errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
								errmsg_internal("Cannot append paths with "
												"incompatible distribution")));

			pathnode->path.sameslice_relids = bms_union(pathnode->path.sameslice_relids, subpath->sameslice_relids);

			if (subpath->motionHazard)
				pathnode->path.motionHazard = true;

			if (!subpath->rescannable)
				pathnode->path.rescannable = false;

			pathnode->subpaths = lappend(pathnode->subpaths, subpath);
		}

		/*
		 * CDB: If there is exactly one subpath, its ordering is preserved.
		 * Child rel's pathkey exprs are already expressed in terms of the
		 * columns of the parent appendrel.  See find_usable_indexes().
		 */
		if (list_length(subpaths) == 1)
			pathnode->path.pathkeys = ((Path *)linitial(subpaths))->pathkeys;
	}
	return pathnode;
}

/*
 * create_result_path
 *	  Creates a path representing a Result-and-nothing-else plan.
 *	  This is only used for the case of a query with an empty jointree.
 */
ResultPath *
create_result_path(List *quals)
{
	ResultPath *pathnode = makeNode(ResultPath);

	pathnode->path.pathtype = T_Result;
	pathnode->path.parent = NULL;
	pathnode->path.pathkeys = NIL;
	pathnode->quals = quals;

	/* Ideally should define cost_result(), but I'm too lazy */
	pathnode->path.startup_cost = 0;
	pathnode->path.total_cost = cpu_tuple_cost;

	CdbPathLocus_MakeGeneral(&pathnode->path.locus);
	pathnode->path.motionHazard = false;
	pathnode->path.rescannable = true;

	/*
	 * In theory we should include the qual eval cost as well, but at present
	 * that doesn't accomplish much except duplicate work that will be done
	 * again in make_result; since this is only used for degenerate cases,
	 * nothing interesting will be done with the path cost values...
	 */

	return pathnode;
}

/*
 * create_material_path
 *	  Creates a path corresponding to a Material plan, returning the
 *	  pathnode.
 */
MaterialPath *
create_material_path(PlannerInfo *root, RelOptInfo *rel, Path *subpath)
{
	MaterialPath *pathnode = makeNode(MaterialPath);

	pathnode->path.pathtype = T_Material;
	pathnode->path.parent = rel;

	pathnode->path.pathkeys = subpath->pathkeys;

    pathnode->path.locus = subpath->locus;
    pathnode->path.motionHazard = subpath->motionHazard;
    pathnode->cdb_strict = false;
    pathnode->path.rescannable = true; /* Independent of sub-path */
	pathnode->path.sameslice_relids = subpath->sameslice_relids;

	pathnode->subpath = subpath;

	cost_material(&pathnode->path,
				  root,
				  subpath->total_cost,
				  cdbpath_rows(root, subpath),
				  rel->width);

	return pathnode;
}


/* GPDB_84_MERGE_NOTE: Before commit e006a24a, this function used in_info_list
 * to compute in_operators and uniq_exprs. After this commit, this function used sjinfo instead.
 * But in GPDB, this function receives distinct_on_operators (in_operators) and distinct_on_exprs (uniq_exprs)
 * hence we don't need to make any changes here. */

/*
 * create_unique_path
 *	  Creates a path representing elimination of distinct rows from the
 *	  input data.  Distinct-ness is defined according to the needs of the
 *	  semijoin represented by sjinfo.  If it is not possible to identify
 *	  how to make the data unique, NULL is returned.
 *
 * If used at all, this is likely to be called repeatedly on the same rel;
 * and the input subpath should always be the same (the cheapest_total path
 * for the rel).  So we cache the result.
 */
UniquePath *
create_unique_path(PlannerInfo *root,
                   Path        *subpath,
                   List        *distinct_on_exprs,
				   List		   *distinct_on_operators,
                   Relids       distinct_on_rowid_relids)
{
	UniquePath *pathnode;
	Path		sort_path;		/* dummy for result of cost_sort */
	Path		agg_path;		/* dummy for result of cost_agg */
    RelOptInfo *rel = subpath->parent;
	int			numCols;
    double      subpath_rows;
    bool        hashable = false;

    subpath_rows = cdbpath_rows(root, subpath);

    /* Allocate and partially initialize a UniquePath node. */
    pathnode = make_unique_path(subpath);

    /* Share caller's expr list and operators, and relids. */
    pathnode->distinct_on_exprs = distinct_on_exprs;
	pathnode->distinct_on_eq_operators = distinct_on_operators;
    pathnode->distinct_on_rowid_relids = distinct_on_rowid_relids;

	/*
	 * Treat the output as always unsorted, since we don't necessarily have
	 * pathkeys to represent it.
	 */
	pathnode->path.pathkeys = NIL;

	/*
	 * If we know the targetlist, try to estimate number of result rows;
	 * otherwise punt.
	 */
	if (distinct_on_exprs)
	{
		pathnode->rows = estimate_num_groups(root, distinct_on_exprs, subpath_rows);
		numCols = list_length(distinct_on_exprs);
	}
	else
	{
		pathnode->rows = subpath_rows;
		numCols = 1;
	}

	/*
	 * Estimate cost for sort+unique implementation
	 */
	cost_sort(&sort_path, root, NIL,
			  subpath->total_cost,
			  subpath_rows,
			  rel->width,
			  -1.0);

	/*
	 * Charge one cpu_operator_cost per comparison per input tuple. We assume
	 * all columns get compared at most of the tuples.	(XXX probably this is
	 * an overestimate.)  This should agree with make_unique.
	 */
	sort_path.total_cost += cpu_operator_cost * subpath_rows * numCols;

	/*
	 * Is it safe to use a hashed implementation?  If so, estimate and compare
	 * costs.  We only try this if we know the targetlist for sure (else we
	 * can't be sure about the datatypes involved).
     *
     * CDB: For dedup, the distinct_on_exprs list isn't built until later,
     * but we know the data types will be hashable.
	 */
	pathnode->umethod = UNIQUE_PATH_SORT;
	pathnode->path.startup_cost = sort_path.startup_cost;
	pathnode->path.total_cost = sort_path.total_cost;

    if (distinct_on_exprs && hash_safe_operators(distinct_on_operators))
        hashable = true;
    if (distinct_on_rowid_relids)
        hashable = true;
    if (hashable
    		&& (root->config->enable_hashagg
    				|| root->config->mpp_trying_fallback_plan
    		)
    )
	{
		/*
		 * Estimate the overhead per hashtable entry at 64 bytes (same as in
		 * planner.c).
		 */
		int			hashentrysize = rel->width + 64;

        /*
         * CDB TODO: Hybrid hashed aggregation is not limited by work_mem.
         */
        if (hashentrysize * pathnode->rows <= global_work_mem(root))
		{
			cost_agg(&agg_path, root,
					 AGG_HASHED, 0,
					 numCols, pathnode->rows,
					 subpath->startup_cost,
					 subpath->total_cost,
					 rel->rows, 0.0, 0.0, hashentrysize, false);
			if (agg_path.total_cost < sort_path.total_cost || 
                (!root->config->enable_sort && !root->config->mpp_trying_fallback_plan))
            {
                pathnode->umethod = UNIQUE_PATH_HASH;
		        pathnode->path.startup_cost = agg_path.startup_cost;
		        pathnode->path.total_cost = agg_path.total_cost;
            }
		}
	}

	/* see MPP-1140 */
	if (pathnode->umethod == UNIQUE_PATH_HASH)
	{
		/* hybrid hash agg is not rescannable, and may present a motion hazard */
		pathnode->path.motionHazard = subpath->motionHazard;
		pathnode->path.rescannable = false;
	}
	else
	{
		/* sort or plain implies materialization and breaks deadlock cycle.
		 *  (NB: Must not reset motionHazard when sort is eliminated due to
		 *  existing ordering; but Unique sort is never optimized away at present.)
		 */
		pathnode->path.motionHazard = subpath->motionHazard;

		/* Same reasoning applies to rescanablilty.  If no actual sort is placed
		 * in the plan, then rescannable is set correctly to the subpath value.
		 * If sort intervenes, it should be set to true.  We depend
		 * on the above claim that sort will always intervene.
		 */
		pathnode->path.rescannable = true;
	}

	return pathnode;
}                               /* create_unique_path */


/*
 * create_unique_exprlist_path
 *	  Creates a path representing elimination of distinct rows from the
 *	  input data.
 *
 * Returns a UniquePath node representing a "DISTINCT ON e1,...,en" operator,
 * where e1,...,en are the expressions in the 'distinct_on_exprs' list.
 *
 * NB: The returned node shares the 'distinct_on_exprs' list given by the
 * caller; the list and its members must not be changed or freed during the
 * node's lifetime.
 *
 * If a row's duplicates might occur in more than one partition, a Motion
 * operator will be needed to bring them together.  Since this path might
 * not be chosen, we won't take the time to create a CdbMotionPath node here.
 * Just estimate what the cost would be, and assign a dummy locus; leave
 * the real work for create_plan().
 */
static UniquePath *
create_unique_exprlist_path(PlannerInfo *root, Path *subpath,
                            List *distinct_on_exprs,
							List *distinct_on_operators)
{
	UniquePath *uniquepath;
    RelOptInfo *rel = subpath->parent;

    Assert(distinct_on_exprs);
    Assert(distinct_on_operators);

	/*
	 * If the input is a subquery whose output must be unique already, then we
	 * don't need to do anything.  The test for uniqueness has to consider
	 * exactly which columns we are extracting; for example "SELECT DISTINCT
	 * x,y" doesn't guarantee that x alone is distinct. So we cannot check for
	 * this optimization unless we found our own targetlist above, and it
	 * consists only of simple Vars referencing subquery outputs.  (Possibly
	 * we could do something with expressions in the subquery outputs, too,
	 * but for now keep it simple.)
	 */
	if (rel->rtekind == RTE_SUBQUERY)
	{
		RangeTblEntry *rte = planner_rt_fetch(rel->relid, root);
		List	   *sub_tlist_colnos;

		sub_tlist_colnos = translate_sub_tlist(distinct_on_exprs, rel->relid);

		if (sub_tlist_colnos &&
			query_is_distinct_for(rte->subquery, sub_tlist_colnos, distinct_on_operators))
		{
            uniquepath = make_unique_path(subpath);
			uniquepath->umethod = UNIQUE_PATH_NOOP;
			uniquepath->rows = cdbpath_rows(root, subpath);
            uniquepath->distinct_on_exprs = distinct_on_exprs;  /* share list */
			uniquepath->distinct_on_eq_operators = distinct_on_operators;
			return uniquepath;
		}
	}

    /* Repartition first if duplicates might be on different QEs. */
    if (!CdbPathLocus_IsBottleneck(subpath->locus) &&
        !cdbpathlocus_is_hashed_on_exprs(subpath->locus, distinct_on_exprs))
    {
        CdbPathLocus    locus;

        locus = cdbpathlocus_from_exprs(root, distinct_on_exprs);
        subpath = cdbpath_create_motion_path(root, subpath, NIL, false, locus);
        Insist(subpath);
    }

    /* Create the UniquePath node.  (Might return NULL if enable_sort = off.) */
    uniquepath = create_unique_path(root, subpath,
									distinct_on_exprs, distinct_on_operators,
									NULL);

	return uniquepath;
}                               /* create_unique_exprlist_path */


/*
 * create_unique_rowid_path
 *
 * CDB: Used when a subquery predicate (such as "x IN (SELECT ...)") has
 * been flattened into a join with the tables of the current query.  A row
 * of the cross-product of the current query's tables might join with more
 * than one row from the subquery and thus be duplicated.  Removing such
 * duplicates after the join is called deduping.
 *
 * Returns a UniquePath node representing a "DISTINCT ON r1,...,rn" operator,
 * where (r1,...,rn) represents a unique identifier for each row of the cross
 * product of the tables specified by the 'distinct_relids' parameter.
 *
 * NB: The returned node shares the given 'distinct_relids' bitmapset object;
 * so the caller must not free or modify it during the node's lifetime.
 *
 * If a row's duplicates might occur in more than one partition, a Motion
 * operator will be needed to bring them together.  Since this path might
 * not be chosen, we won't take the time to create a CdbMotionPath node here.
 * Just estimate what the cost would be, and assign a dummy locus; leave
 * the real work for create_plan().
 */
static UniquePath *
create_unique_rowid_path(PlannerInfo *root,
                         Path        *subpath,
                         Relids       distinct_relids)
{
	UniquePath *uniquepath;

    Assert(!bms_is_empty(distinct_relids));

    /* Create the UniquePath node.  (Might return NULL if enable_sort = off.) */
    uniquepath = create_unique_path(root, subpath, NIL, NIL, distinct_relids);
    if (!uniquepath)
        return NULL;

    /* Add repartitioning cost if duplicates might be on different QEs. */
    if (!CdbPathLocus_IsBottleneck(subpath->locus) &&
        !cdbpathlocus_is_hashed_on_relids(subpath->locus, distinct_relids))
    {
        CdbMotionPath   motionpath;     /* dummy for cost estimate */
        Cost            repartition_cost;

        /* Tell create_unique_plan() to insert Motion operator atop subpath. */
        uniquepath->must_repartition = true;

        /* Set a fake locus.  Repartitioning key won't be built until later. */
        CdbPathLocus_MakeStrewn(&uniquepath->path.locus);
		uniquepath->path.sameslice_relids = NULL;

        /* Estimate repartitioning cost. */
        memset(&motionpath, 0, sizeof(motionpath));
        motionpath.path.type = T_CdbMotionPath;
        motionpath.path.parent = subpath->parent;
        motionpath.path.locus = uniquepath->path.locus;
        motionpath.subpath = subpath;
        cdbpath_cost_motion(root, &motionpath);

        /* Add MotionPath cost to UniquePath cost. */
        repartition_cost = motionpath.path.total_cost - subpath->total_cost;
        uniquepath->path.total_cost += repartition_cost;
    }

	return uniquepath;
}                               /* create_unique_rowid_path */


/*
 * create_limit1_path
 *
 * CDB: Add operators on top of the path to ensure uniqueness by
 * discarding all but the first row of the given subpath's result.
 *
 * If a row's duplicates might occur in more than one partition, a Motion
 * operator will be inserted to bring them together.
 */
UniquePath *
create_limit1_path(PlannerInfo *root, Path *subpath)
{
    UniquePath     *uniquepath;
    CdbPathLocus    singleQE;

    /* Add LIMIT 1 operator. */
    uniquepath = make_limit1_path(subpath);

    /* Add Motion operator to gather to a single qExec, then LIMIT 1 again. */
    if (CdbPathLocus_IsPartitioned(subpath->locus))
    {
        CdbPathLocus_MakeSingleQE(&singleQE);
        subpath = cdbpath_create_motion_path(root, (Path *)uniquepath, NIL,
                                             false, singleQE);

        /* Add another LIMIT 1 on top. */
        uniquepath = make_limit1_path(subpath);

		uniquepath->path.motionHazard = subpath->motionHazard;
    }
	return uniquepath;
}                               /* create_limit1_path */


/*
 * make_limit1_path
 *
 * CDB: Returns a UniquePath which ensures uniqueness by producing at most
 * one row of the given subpath's result.
 */
UniquePath *
make_limit1_path(Path *subpath)
{
    UniquePath *uniquepath = make_unique_path(subpath);

    uniquepath->umethod = UNIQUE_PATH_LIMIT1;
    uniquepath->rows = 1;

    /* Cost to get a single row is same as startup cost (from subpath). */
    uniquepath->path.total_cost = uniquepath->path.startup_cost;

    return uniquepath;
}                               /* make_limit1_path */


/*
 * make_unique_path
 *    Allocates and partially initializes a UniquePath node to be completed
 *    by the caller.
 */
UniquePath *
make_unique_path(Path *subpath)
{
	UniquePath     *pathnode;
    MemoryContext   oldcontext;

	/* Allocate in same context as parent rel in case GEQO is ever used. */
	oldcontext = MemoryContextSwitchTo(GetMemoryChunkContext(subpath->parent));

    /* Allocate the UniquePath node. */
	pathnode = makeNode(UniquePath);

    /* Copy Path header from subpath. */
    pathnode->path = *subpath;

    /* Fix up the Path header. */
    pathnode->path.type = T_UniquePath;
    pathnode->path.pathtype = T_Unique;

    /* Initialize added UniquePath fields. */
    pathnode->subpath = subpath;
    pathnode->distinct_on_exprs = NIL;
    pathnode->distinct_on_eq_operators = NIL;
    pathnode->distinct_on_rowid_relids = NULL;
    pathnode->must_repartition = false;

	/* Restore caller's allocation context. */
	MemoryContextSwitchTo(oldcontext);

    return pathnode;
}                               /* make_unique_path */


/*
 * translate_sub_tlist - get subquery column numbers represented by tlist
 *
 * The given targetlist usually contains only Vars referencing the given relid.
 * Extract their varattnos (ie, the column numbers of the subquery) and return
 * as an integer List.
 *
 * If any of the tlist items is not a simple Var, we cannot determine whether
 * the subquery's uniqueness condition (if any) matches ours, so punt and
 * return NIL.
 */
static List *
translate_sub_tlist(List *tlist, int relid)
{
	List	   *result = NIL;
	ListCell   *l;

	foreach(l, tlist)
	{
		Var		   *var = (Var *) lfirst(l);

		if (!var || !IsA(var, Var) ||
			var->varno != relid)
			return NIL;			/* punt */

		result = lappend_int(result, var->varattno);
	}
	return result;
}

/*
 * query_is_distinct_for - does query never return duplicates of the
 *		specified columns?
 *
 * colnos is an integer list of output column numbers (resno's).  We are
 * interested in whether rows consisting of just these columns are certain
 * to be distinct.	"Distinctness" is defined according to whether the
 * corresponding upper-level equality operators listed in opids would think
 * the values are distinct.  (Note: the opids entries could be cross-type
 * operators, and thus not exactly the equality operators that the subquery
 * would use itself.  We use equality_ops_are_compatible() to check
 * compatibility.  That looks at btree or hash opfamily membership, and so
 * should give trustworthy answers for all operators that we might need
 * to deal with here.)
 */
static bool
query_is_distinct_for(Query *query, List *colnos, List *opids)
{
	ListCell   *l;
	Oid			opid;

	Assert(list_length(colnos) == list_length(opids));

	/*
	 * A set-returning function in the query's targetlist can result in
	 * returning duplicate rows, if the SRF is evaluated after the
	 * de-duplication step; so we play it safe and say "no" if there are any
	 * SRFs.  (We could be certain that it's okay if SRFs appear only in the
	 * specified columns, since those must be evaluated before de-duplication;
	 * but it doesn't presently seem worth the complication to check that.)
	 */
	if (expression_returns_set((Node *) query->targetList))
		return false;

	/*
	 * DISTINCT (including DISTINCT ON) guarantees uniqueness if all the
	 * columns in the DISTINCT clause appear in colnos and operator semantics
	 * match.
	 */
	if (query->distinctClause)
	{
		foreach(l, query->distinctClause)
		{
			SortGroupClause *sgc = (SortGroupClause *) lfirst(l);
			TargetEntry *tle = get_sortgroupclause_tle(sgc,
													   query->targetList);

			opid = distinct_col_search(tle->resno, colnos, opids);
			if (!OidIsValid(opid) ||
				!equality_ops_are_compatible(opid, sgc->eqop))
				break;			/* exit early if no match */
		}
		if (l == NULL)			/* had matches for all? */
			return true;
	}

	/*
	 * Similarly, GROUP BY guarantees uniqueness if all the grouped columns
	 * appear in colnos and operator semantics match.
	 */
	if (query->groupClause)
	{
		List	   *grouptles;
		List	   *sortops;
		List	   *eqops;
		ListCell   *l_eqop;

		get_sortgroupclauses_tles(query->groupClause, query->targetList,
								  &grouptles, &sortops, &eqops);

		forboth(l, grouptles, l_eqop, eqops)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(l);

			opid = distinct_col_search(tle->resno, colnos, opids);
			if (!OidIsValid(opid) ||
				!equality_ops_are_compatible(opid, lfirst_oid(l_eqop)))
				break;			/* exit early if no match */
		}
		if (l == NULL)			/* had matches for all? */
			return true;
	}
	else
	{
		/*
		 * If we have no GROUP BY, but do have aggregates or HAVING, then the
		 * result is at most one row so it's surely unique, for any operators.
		 */
		if (query->hasAggs || query->havingQual)
			return true;
	}

	/*
	 * UNION, INTERSECT, EXCEPT guarantee uniqueness of the whole output row,
	 * except with ALL.
	 */
	if (query->setOperations)
	{
		SetOperationStmt *topop = (SetOperationStmt *) query->setOperations;

		Assert(IsA(topop, SetOperationStmt));
		Assert(topop->op != SETOP_NONE);

		if (!topop->all)
		{
			ListCell   *lg;

			/* We're good if all the nonjunk output columns are in colnos */
			lg = list_head(topop->groupClauses);
			foreach(l, query->targetList)
			{
				TargetEntry *tle = (TargetEntry *) lfirst(l);
				SortGroupClause *sgc;

				if (tle->resjunk)
					continue;	/* ignore resjunk columns */

				/* non-resjunk columns should have grouping clauses */
				Assert(lg != NULL);
				sgc = (SortGroupClause *) lfirst(lg);
				lg = lnext(lg);

				opid = distinct_col_search(tle->resno, colnos, opids);
				if (!OidIsValid(opid) ||
					!equality_ops_are_compatible(opid, sgc->eqop))
					break;		/* exit early if no match */
			}
			if (l == NULL)		/* had matches for all? */
				return true;
		}
	}

	/*
	 * XXX Are there any other cases in which we can easily see the result
	 * must be distinct?
	 */

	return false;
}

/*
 * distinct_col_search - subroutine for query_is_distinct_for
 *
 * If colno is in colnos, return the corresponding element of opids,
 * else return InvalidOid.	(We expect colnos does not contain duplicates,
 * so the result is well-defined.)
 */
static Oid
distinct_col_search(int colno, List *colnos, List *opids)
{
	ListCell   *lc1,
			   *lc2;

	forboth(lc1, colnos, lc2, opids)
	{
		if (colno == lfirst_int(lc1))
			return lfirst_oid(lc2);
	}
	return InvalidOid;
}

/*
 * hash_safe_operators - can all the specified IN operators be hashed?
 *
 * We assume hashed aggregation will work if each IN operator is marked
 * hashjoinable.  If the IN operators are cross-type, this could conceivably
 * fail: the aggregation will need a hashable equality operator for the RHS
 * datatype --- but it's pretty hard to conceive of a hash opfamily that has
 * cross-type hashing without support for hashing the individual types, so
 * we don't expend cycles here to support the case.  We could check
 * get_compatible_hash_operator() instead of just op_hashjoinable(), but the
 * former is a significantly more expensive test.
 */
static bool
hash_safe_operators(List *opids)
{
	ListCell   *lc;

	foreach(lc, opids)
	{
		if (!op_hashjoinable(lfirst_oid(lc)))
			return false;
	}
	return true;
}

/*
 * create_subqueryscan_path
 *	  Creates a path corresponding to a sequential scan of a subquery,
 *	  returning the pathnode.
 */
Path *
create_subqueryscan_path(PlannerInfo *root, RelOptInfo *rel, List *pathkeys)
{
	Path	   *pathnode = makeNode(Path);

	pathnode->pathtype = T_SubqueryScan;
	pathnode->parent = rel;
	pathnode->pathkeys = pathkeys;

    pathnode->locus = cdbpathlocus_from_subquery(root, rel->subplan, rel->relid);
    pathnode->motionHazard = true;          /* better safe than sorry */
    pathnode->rescannable = false;
	pathnode->sameslice_relids = NULL;

	cost_subqueryscan(pathnode, rel);

	return pathnode;
}

/*
 * create_functionscan_path
 *	  Creates a path corresponding to a sequential scan of a function,
 *	  returning the pathnode.
 */
Path *
create_functionscan_path(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
	Path	   *pathnode = makeNode(Path);

	pathnode->pathtype = T_FunctionScan;
	pathnode->parent = rel;
	pathnode->pathkeys = NIL;	/* for now, assume unordered result */

	/*
	 * If the function desires to run on segments, mark randomly-distributed.
	 * If expression contains mutable functions, evaluate it on entry db.
	 * Otherwise let it be evaluated in the same slice as its parent operator.
	 */
	Assert(rte->rtekind == RTE_FUNCTION);

	if (rte->funcexpr && IsA(rte->funcexpr, FuncExpr))
	{
		char		exec_location;

		exec_location = func_exec_location(((FuncExpr *) rte->funcexpr)->funcid);

		switch (exec_location)
		{
			case PROEXECLOCATION_ANY:
				CdbPathLocus_MakeGeneral(&pathnode->locus);

				/*
				 * If the function is ON ANY, we presumably could execute the
				 * function anywhere. However, historically, before the
				 * EXECUTE ON syntax was introduced, we always executed
				 * non-IMMUTABLE functions on the master. Keep that behavior
				 * for backwards compatibility.
				 */
				if (contain_mutable_functions(rte->funcexpr))
					CdbPathLocus_MakeEntry(&pathnode->locus);
				else
					CdbPathLocus_MakeGeneral(&pathnode->locus);
				break;
			case PROEXECLOCATION_MASTER:
				CdbPathLocus_MakeEntry(&pathnode->locus);
				break;
			case PROEXECLOCATION_ALL_SEGMENTS:
				CdbPathLocus_MakeStrewn(&pathnode->locus);
				break;
			default:
				elog(ERROR, "unrecognized proexeclocation '%c'", exec_location);
		}
	}
	else
	{
		/*
		 * The expression might've been simplified into a Const. Which can
		 * be executed anywhere.
		 */
		/* The default behavior is */
		if (contain_mutable_functions(rte->funcexpr))
			CdbPathLocus_MakeEntry(&pathnode->locus);
		else
			CdbPathLocus_MakeGeneral(&pathnode->locus);
	}

	pathnode->motionHazard = false;

	/* For now, be conservative. */
	pathnode->rescannable = false;
	pathnode->sameslice_relids = NULL;

	cost_functionscan(pathnode, root, rel);

	return pathnode;
}

/*
 * create_tablefunction_path
 *	  Creates a path corresponding to a sequential scan of a table function,
 *	  returning the pathnode.
 */
Path *
create_tablefunction_path(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
	Path	   *pathnode = makeNode(Path);

	Assert(rte->rtekind == RTE_TABLEFUNCTION);

	/* Setup the basics of the TableFunction path */
	pathnode->pathtype	   = T_TableFunctionScan;
	pathnode->parent	   = rel;
	pathnode->pathkeys	   = NIL;		/* no way to specify output ordering */
	pathnode->motionHazard = true;      /* better safe than sorry */
	pathnode->rescannable  = false;     /* better safe than sorry */

	/* 
	 * Inherit the locus of the input subquery.  This is necessary to handle the
	 * case of a General locus, e.g. if all the data has been concentrated to a
	 * single segment then the output will all be on that segment, otherwise the
	 * output must be declared as randomly distributed because we do not know
	 * what relationship, if any, there is between the input data and the output
	 * data.
	 */
	pathnode->locus = cdbpathlocus_from_subquery(root, rel->subplan, rel->relid);

	/* Mark the output as random if the input is partitioned */
	if (CdbPathLocus_IsPartitioned(pathnode->locus))
		CdbPathLocus_MakeStrewn(&pathnode->locus);
	pathnode->sameslice_relids = NULL;

	cost_tablefunction(pathnode, root, rel);

	return pathnode;
}

/*
 * create_valuesscan_path
 *	  Creates a path corresponding to a scan of a VALUES list,
 *	  returning the pathnode.
 */
Path *
create_valuesscan_path(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
	Path	   *pathnode = makeNode(Path);

	pathnode->pathtype = T_ValuesScan;
	pathnode->parent = rel;
	pathnode->pathkeys = NIL;	/* result is always unordered */

    /*
     * CDB: If VALUES list contains mutable functions, evaluate it on entry db.
     * Otherwise let it be evaluated in the same slice as its parent operator.
     */
    Assert(rte->rtekind == RTE_VALUES);
    if (contain_mutable_functions((Node *)rte->values_lists))
        CdbPathLocus_MakeEntry(&pathnode->locus);
    else
        CdbPathLocus_MakeGeneral(&pathnode->locus);

    pathnode->motionHazard = false;
	pathnode->rescannable = true;
	pathnode->sameslice_relids = NULL;

	cost_valuesscan(pathnode, root, rel);

	return pathnode;
}

/*
 * create_ctescan_path
 *	  Creates a path corresponding to a scan of a non-self-reference CTE,
 *	  returning the pathnode.
 */
Path *
create_ctescan_path(PlannerInfo *root, RelOptInfo *rel, List *pathkeys)
{
	Path	   *pathnode = makeNode(Path);

	pathnode->pathtype = T_CteScan;
	pathnode->parent = rel;
	pathnode->pathkeys = pathkeys;

	pathnode->locus = cdbpathlocus_from_subquery(root, rel->subplan, rel->relid);

	/*
	 * We can't extract these two values from the subplan, so we simple set
	 * them to their worst case here.
	 */
	pathnode->motionHazard = true;
	pathnode->rescannable = false;
	pathnode->sameslice_relids = NULL;


	cost_ctescan(pathnode, root, rel);

	return pathnode;
}

/*
 * create_worktablescan_path
 *	  Creates a path corresponding to a scan of a self-reference CTE,
 *	  returning the pathnode.
 */
Path *
create_worktablescan_path(PlannerInfo *root, RelOptInfo *rel, CdbLocusType ctelocus)
{
	Path	   *pathnode = makeNode(Path);
	CdbPathLocus result;

	if (ctelocus == CdbLocusType_Entry)
		CdbPathLocus_MakeEntry(&result);
	else if (ctelocus == CdbLocusType_SingleQE)
		CdbPathLocus_MakeSingleQE(&result);
	else if (ctelocus == CdbLocusType_General)
		CdbPathLocus_MakeGeneral(&result);
	else
		CdbPathLocus_MakeStrewn(&result);

	pathnode->pathtype = T_WorkTableScan;
	pathnode->parent = rel;
	pathnode->pathkeys = NIL;	/* result is always unordered */

	pathnode->locus = result;
	pathnode->motionHazard = false;
	pathnode->rescannable = true;
	pathnode->sameslice_relids = rel->relids;

	/* Cost is the same as for a regular CTE scan */
	cost_ctescan(pathnode, root, rel);

	return pathnode;
}

/*
 * cdb_jointype_to_join_in
 *    Returns JOIN_SEMI if the jointype should be changed from JOIN_INNER to
 *    JOIN_SEMI so as to produce at most one matching inner row per outer row.
 *    Else returns the given jointype.
 *
 * CDB TODO: Allow outer joins to use the JOIN_SEMI technique.
 * CDB TODO: Occasionally, symmetric JOIN_SEMI might be useful (aka 'match join').
 */
static inline JoinType
cdb_jointype_to_join_semi(RelOptInfo *joinrel, JoinType jointype, Path *inner_path)
{
    CdbRelDedupInfo    *dedup = joinrel->dedup_info;

    if (dedup &&
        dedup->spent_subqrelids &&
        bms_is_subset(inner_path->parent->relids, dedup->spent_subqrelids) &&
        !IsA(inner_path, UniquePath) &&
        jointype == JOIN_INNER)
    {
        jointype = JOIN_SEMI;
    }
    return jointype;
}                               /* cdb_jointype_to_join_semi */

bool
path_contains_inner_index(Path *path)
{
    if (IsA(path, IndexPath) &&
        ((IndexPath *)path)->isjoininner)
		return true;
    else if (IsA(path, BitmapHeapPath) &&
             ((BitmapHeapPath *)path)->isjoininner)
		return true;
	else if (IsA(path, BitmapAppendOnlyPath) &&
			 ((BitmapAppendOnlyPath *)path)->isjoininner)
		return true;
	else if (IsA(path, AppendPath))
	{
		/* MPP-2377: Append paths may conceal inner-index scans, if
		 * any of the subpaths are indexpaths or bitmapheap-paths we
		 * have to do more checking */
		ListCell   *l;

		/* scan the subpaths of the Append */
		foreach(l, ((AppendPath *)path)->subpaths)
		{
			Path *subpath = (Path *)lfirst(l);

			if (path_contains_inner_index(subpath))
				return true;
		}
	}

	return false;
}

/*
 * create_nestloop_path
 *	  Creates a pathnode corresponding to a nestloop join between two
 *	  relations.
 *
 * 'joinrel' is the join relation.
 * 'jointype' is the type of join required
 * 'sjinfo' is extra info about the join for selectivity estimation
 * 'outer_path' is the outer path
 * 'inner_path' is the inner path
 * 'restrict_clauses' are the RestrictInfo nodes to apply at the join
 * 'pathkeys' are the path keys of the new join path
 *
 * Returns the resulting path node.
 */
NestPath *
create_nestloop_path(PlannerInfo *root,
					 RelOptInfo *joinrel,
					 JoinType jointype,
					 SpecialJoinInfo *sjinfo,
					 Path *outer_path,
					 Path *inner_path,
					 List *restrict_clauses,
                     List *mergeclause_list,    /*CDB*/
					 List *pathkeys)
{
	NestPath       *pathnode;
    CdbPathLocus    join_locus;
    bool            inner_must_be_local = false;

    /* CDB: Change jointype to JOIN_SEMI from JOIN_INNER (if eligible). */
    if (joinrel->dedup_info)
    {
        jointype = cdb_jointype_to_join_semi(joinrel, jointype, inner_path);
        sjinfo->jointype = jointype;
    }

    /* CDB: Inner indexpath must execute in the same backend as the
     * nested join to receive input values from the outer rel.
     */
	inner_must_be_local = path_contains_inner_index(inner_path);

    /* Add motion nodes above subpaths and decide where to join. */
    join_locus = cdbpath_motion_for_join(root,
                                         jointype,
                                         &outer_path,       /* INOUT */
                                         &inner_path,       /* INOUT */
                                         mergeclause_list,
                                         pathkeys,
                                         NIL,
                                         false,
                                         inner_must_be_local);
    if (CdbPathLocus_IsNull(join_locus))
        return NULL;

    /* Outer might not be ordered anymore after motion. */
    if (!outer_path->pathkeys)
        pathkeys = NIL;

    /*
     * If outer has at most one row, NJ will make at most one pass over inner.
     * Else materialize inner rel after motion so NJ can loop over results.
     * Skip if an intervening Unique operator will materialize.
     */
    if (!outer_path->parent->onerow)
    {
        if (!inner_path->rescannable)
		{
			/* NLs potentially rescan the inner; if our inner path
			 * isn't rescannable we have to add a materialize node */
			inner_path = (Path *)create_material_path(root, inner_path->parent, inner_path);

			/* If we have motion on the outer, to avoid a deadlock; we
			 * need to set cdb_strict. In order for materialize to
			 * fully fetch the underlying (required to avoid our
			 * deadlock hazard) we must set cdb_strict! */
			if (inner_path->motionHazard && outer_path->motionHazard)
			{
				((MaterialPath *)inner_path)->cdb_strict = true;
				inner_path->motionHazard = false;
			}
		}
    }

    pathnode = makeNode(NestPath);
	pathnode->path.pathtype = T_NestLoop;
	pathnode->path.parent = joinrel;
	pathnode->jointype = jointype;
	pathnode->outerjoinpath = outer_path;
	pathnode->innerjoinpath = inner_path;
	pathnode->joinrestrictinfo = restrict_clauses;
	pathnode->path.pathkeys = pathkeys;

    pathnode->path.locus = join_locus;
    pathnode->path.motionHazard = outer_path->motionHazard || inner_path->motionHazard;

	/* we're only as rescannable as our child plans */
    pathnode->path.rescannable = outer_path->rescannable && inner_path->rescannable;

	pathnode->path.sameslice_relids = bms_union(inner_path->sameslice_relids, outer_path->sameslice_relids);

	cost_nestloop(pathnode, root, sjinfo);

	return pathnode;
}

/*
 * create_mergejoin_path
 *	  Creates a pathnode corresponding to a mergejoin join between
 *	  two relations
 *
 * 'joinrel' is the join relation
 * 'jointype' is the type of join required
 * 'sjinfo' is extra info about the join for selectivity estimation
 * 'outer_path' is the outer path
 * 'inner_path' is the inner path
 * 'restrict_clauses' are the RestrictInfo nodes to apply at the join
 * 'pathkeys' are the path keys of the new join path
 * 'mergeclauses' are the RestrictInfo nodes to use as merge clauses
 *		(this should be a subset of the restrict_clauses list)
 * 'allmergeclauses' are the RestrictInfo nodes that are of the form
 *      required of merge clauses (equijoin between outer and inner rel).
 *      Consists of the ones to be used for merging ('mergeclauses') plus
 *      any others in 'restrict_clauses' that are to be applied after the
 *      merge.  We use them for motion planning.  (CDB)

 * 'outersortkeys' are the sort varkeys for the outer relation
 *      or NIL to use existing ordering
 * 'innersortkeys' are the sort varkeys for the inner relation
 *      or NIL to use existing ordering
 */
MergePath *
create_mergejoin_path(PlannerInfo *root,
					  RelOptInfo *joinrel,
					  JoinType jointype,
					  SpecialJoinInfo *sjinfo,
					  Path *outer_path,
					  Path *inner_path,
					  List *restrict_clauses,
					  List *pathkeys,
					  List *mergeclauses,
                      List *allmergeclauses,    /*CDB*/
					  List *outersortkeys,
					  List *innersortkeys)
{
    MergePath      *pathnode;
    CdbPathLocus    join_locus;
    List           *outermotionkeys;
    List           *innermotionkeys;

    /* CDB: Change jointype to JOIN_SEMI from JOIN_INNER (if eligible). */
    if (joinrel->dedup_info)
    {
        jointype = cdb_jointype_to_join_semi(joinrel, jointype, inner_path);
        sjinfo->jointype = jointype;
    }

    /*
     * Do subpaths have useful ordering?
     */
    if (outersortkeys == NIL)           /* must preserve existing ordering */
        outermotionkeys = outer_path->pathkeys;
    else if (pathkeys_contained_in(outersortkeys, outer_path->pathkeys))
        outermotionkeys = outersortkeys;/* lucky coincidence, already ordered */
    else                                /* existing order useless; must sort */
        outermotionkeys = NIL;

    if (innersortkeys == NIL)
        innermotionkeys = inner_path->pathkeys;
    else if (pathkeys_contained_in(innersortkeys, inner_path->pathkeys))
        innermotionkeys = innersortkeys;
    else
        innermotionkeys = NIL;

    /*
     * Add motion nodes above subpaths and decide where to join.
     */
    join_locus = cdbpath_motion_for_join(root,
                                         jointype,
                                         &outer_path,       /* INOUT */
                                         &inner_path,       /* INOUT */
                                         allmergeclauses,
                                         outermotionkeys,
                                         innermotionkeys,
                                         outersortkeys == NIL,
                                         innersortkeys == NIL);
    if (CdbPathLocus_IsNull(join_locus))
        return NULL;

	/*
	 * Sort is not needed if subpath is already well enough ordered and a
     * disordering motion node (with pathkeys == NIL) hasn't been added.
	 */
	if (outermotionkeys &&
		outer_path->pathkeys)
		outersortkeys = NIL;
	if (innermotionkeys &&
		inner_path->pathkeys)
		innersortkeys = NIL;

    /* If user doesn't want sort, but this MJ requires a sort, fail. */
    if (!root->config->enable_sort &&
        !root->config->mpp_trying_fallback_plan)
    {
        if (outersortkeys || innersortkeys)
            return NULL;
    }

    pathnode = makeNode(MergePath);

	/*
	 * If we are not sorting the inner path, we may need a materialize node to
	 * ensure it can be marked/restored.  (Sort does support mark/restore, so
	 * no materialize is needed in that case.)
	 *
	 * Since the inner side must be ordered, and only Sorts and IndexScans can
	 * create order to begin with, you might think there's no problem --- but
	 * you'd be wrong.  Nestloop and merge joins can *preserve* the order of
	 * their inputs, so they can be selected as the input of a mergejoin, and
	 * they don't support mark/restore at present.
	 */
	if (!ExecSupportsMarkRestore(inner_path->pathtype))
	{
		/*
		 * The inner side does not support mark/restore capability.
		 * Check whether a sort node will be inserted later, and if that is not the case,
		 * add a materialize node.
		 * */
		bool need_sort = false;
		if (innersortkeys != NIL)
		{
			/* Check whether all sort keys are constants. If that is the case,
			 * no sort node is needed.
			 * The check is essentially the same as the one performed later in the
			 * make_sort_from_pathkeys() function (optimizer/plan/createplan.c),
			 * which decides whether a sort node is to be added.
			*/
			ListCell   *sortkeycell;

			foreach(sortkeycell, innersortkeys)
			{
				PathKey	   *keysublist = (PathKey *) lfirst(sortkeycell);

			    if (!CdbPathkeyEqualsConstant(keysublist))
			    {
			    	/* sort key is not a constant - sort will be added later */
			    	need_sort = true;
			    	break;
			    }
			}
		}
		// else: innersortkeys == NIL -> no sort node will be added
		
		if (!need_sort)
		{
			/* no sort node will be added - add a materialize node */
			inner_path = (Path *)
							create_material_path(root, inner_path->parent, inner_path);
		}
	}

	pathnode->jpath.path.pathtype = T_MergeJoin;
	pathnode->jpath.path.parent = joinrel;
	pathnode->jpath.jointype = jointype;
	pathnode->jpath.outerjoinpath = outer_path;
	pathnode->jpath.innerjoinpath = inner_path;
	pathnode->jpath.joinrestrictinfo = restrict_clauses;
	pathnode->jpath.path.pathkeys = pathkeys;
    pathnode->jpath.path.locus = join_locus;

	pathnode->jpath.path.motionHazard = outer_path->motionHazard || inner_path->motionHazard;
	pathnode->jpath.path.rescannable = outer_path->rescannable && inner_path->rescannable;
	pathnode->jpath.path.sameslice_relids = bms_union(inner_path->sameslice_relids, outer_path->sameslice_relids);

	pathnode->path_mergeclauses = mergeclauses;
	pathnode->outersortkeys = outersortkeys;
	pathnode->innersortkeys = innersortkeys;

	cost_mergejoin(pathnode, root, sjinfo);

	return pathnode;
}

/*
 * create_hashjoin_path
 *	  Creates a pathnode corresponding to a hash join between two relations.
 *
 * 'joinrel' is the join relation
 * 'jointype' is the type of join required
 * 'sjinfo' is extra info about the join for selectivity estimation
 * 'outer_path' is the cheapest outer path
 * 'inner_path' is the cheapest inner path
 * 'restrict_clauses' are the RestrictInfo nodes to apply at the join
 * 'hashclauses' are the RestrictInfo nodes to use as hash clauses
 *		(this should be a subset of the restrict_clauses list)
 */
HashPath *
create_hashjoin_path(PlannerInfo *root,
					 RelOptInfo *joinrel,
					 JoinType jointype,
					 SpecialJoinInfo *sjinfo,
					 Path *outer_path,
					 Path *inner_path,
					 List *restrict_clauses,
                     List *mergeclause_list,    /*CDB*/
					 List *hashclauses)
{
	HashPath       *pathnode;
	CdbPathLocus    join_locus;

	/* CDB: Change jointype to JOIN_SEMI from JOIN_INNER (if eligible). */
	if (joinrel->dedup_info)
	{
		jointype = cdb_jointype_to_join_semi(joinrel, jointype, inner_path);
		sjinfo->jointype = jointype;
	}

	/* Add motion nodes above subpaths and decide where to join. */
	join_locus = cdbpath_motion_for_join(root,
										 jointype,
										 &outer_path,       /* INOUT */
										 &inner_path,       /* INOUT */
										 mergeclause_list,
										 NIL,   /* don't care about ordering */
										 NIL,
										 false,
										 false);
	if (CdbPathLocus_IsNull(join_locus))
		return NULL;

	pathnode = makeNode(HashPath);

	pathnode->jpath.path.pathtype = T_HashJoin;
	pathnode->jpath.path.parent = joinrel;
	pathnode->jpath.jointype = jointype;
	pathnode->jpath.outerjoinpath = outer_path;
	pathnode->jpath.innerjoinpath = inner_path;
	pathnode->jpath.joinrestrictinfo = restrict_clauses;
	/* A hashjoin never has pathkeys, since its ordering is unpredictable */
	pathnode->jpath.path.pathkeys = NIL;
    pathnode->jpath.path.locus = join_locus;

	pathnode->path_hashclauses = hashclauses;

    /*
     * If hash table overflows to disk, and an ancestor node requests rescan
     * (e.g. because the HJ is in the inner subtree of a NJ), then the HJ has
     * to be redone, including rescanning the inner rel in order to rebuild
     * the hash table.
     */
    pathnode->jpath.path.rescannable = outer_path->rescannable && inner_path->rescannable;

	/* see the comment above; we may have a motion hazard on our inner ?! */
	if (pathnode->jpath.path.rescannable)
		pathnode->jpath.path.motionHazard = outer_path->motionHazard;
	else
		pathnode->jpath.path.motionHazard = outer_path->motionHazard || inner_path->motionHazard;
	pathnode->jpath.path.sameslice_relids = bms_union(inner_path->sameslice_relids, outer_path->sameslice_relids);

	cost_hashjoin(pathnode, root, sjinfo);

	return pathnode;
}
