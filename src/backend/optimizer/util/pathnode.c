/*-------------------------------------------------------------------------
 *
 * pathnode.c
 *	  Routines to manipulate pathlists and create path nodes
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/optimizer/util/pathnode.c,v 1.155 2009/11/15 02:45:35 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "executor/executor.h"
#include "executor/nodeHash.h"
#include "miscadmin.h"
#include "optimizer/clauses.h"
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

static List *translate_sub_tlist(List *tlist, int relid);
static bool query_is_distinct_for(Query *query, List *colnos, List *opids);
static Oid	distinct_col_search(int colno, List *colnos, List *opids);

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
	List	   *pathlist = parent_rel->pathlist;
	ListCell   *p;
	Path	   *cheapest_startup_path;
	Path	   *cheapest_total_path;

	Assert(IsA(parent_rel, RelOptInfo));

	/* CDB: Empty pathlist is possible if user set some enable_xxx = off. */
	if (pathlist == NIL)
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
	parent_rel->cheapest_unique_path = NULL;	/* computed only if needed */
}

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

	/*
	 * This is a convenient place to check for query cancel --- no part of the
	 * planner goes very long without calling add_path().
	 */
	CHECK_FOR_INTERRUPTS();

    if (!new_path)
        return;

    Assert(cdbpathlocus_is_valid(new_path->locus));

	/*
	 * Loop to check proposed new path against old paths.  Note it is possible
	 * for more than one old path to be tossed out because new_path dominates
	 * it.
	 */
	p1 = list_head(parent_rel->pathlist);		/* cannot use foreach here */
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
			parent_rel->pathlist = list_delete_cell(parent_rel->pathlist,
													p1, p1_prev);

			/*
			 * Delete the data pointed-to by the deleted cell, if possible
			 */
			if (!IsA(old_path, IndexPath))
				pfree(old_path);

			/* Advance list pointer */
			if (p1_prev)
				p1 = lnext(p1_prev);
			else
				p1 = list_head(parent_rel->pathlist);
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
			lappend_cell(parent_rel->pathlist, insert_after, new_path);
		else
			parent_rel->pathlist = lcons(new_path, parent_rel->pathlist);
	}
	else
	{
		/* Reject and recycle the new path */
		if (!IsA(new_path, IndexPath))
			pfree(new_path);
	}
}                               /* add_path */


/*
 * Wrapper around add_path(), for join paths.
 *
 * If the join was originally a semi-join, that's been implemented as an
 * inner-join, followed by removing duplicates, adds the UniquePath on
 * top of the join. Otherwise, just passes through the Path to add_path().
 */
void
cdb_add_join_path(PlannerInfo *root, RelOptInfo *parent_rel, JoinType orig_jointype,
				  JoinPath *new_path)
{
	Path	   *path = (Path *) new_path;

	if (!new_path)
		return;

	if (orig_jointype == JOIN_DEDUP_SEMI)
	{
		Assert(new_path->jointype == JOIN_INNER);
		path = (Path *) create_unique_rowid_path(root,
												 parent_rel,
												 (Path *) new_path,
												 new_path->outerjoinpath->parent->relids);
	}
	else if (orig_jointype == JOIN_DEDUP_SEMI_REVERSE)
	{
		Assert(new_path->jointype == JOIN_INNER);
		path = (Path *) create_unique_rowid_path(root,
												 parent_rel,
												 (Path *) new_path,
												 new_path->innerjoinpath->parent->relids);
	}

	add_path(root, parent_rel, path);
}


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
				  subpath->startup_cost,
				  subpath->total_cost,
				  cdbpath_rows(root, subpath),
				  rel->width);

	return pathnode;
}


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
create_unique_path(PlannerInfo *root, RelOptInfo *rel, Path *subpath,
				   SpecialJoinInfo *sjinfo)
{
	UniquePath *pathnode;
	Path		sort_path;		/* dummy for result of cost_sort */
	Path		agg_path;		/* dummy for result of cost_agg */
	MemoryContext oldcontext;
	List	   *in_operators;
	List	   *uniq_exprs;
	bool		all_btree;
	bool		all_hash;
	int			numCols;
	ListCell   *lc;
	CdbPathLocus locus;

	/* Caller made a mistake if subpath isn't cheapest_total ... */
	Assert(subpath == rel->cheapest_total_path);
	/* ... or if SpecialJoinInfo is the wrong one */
	Assert(sjinfo->jointype == JOIN_SEMI);
	Assert(bms_equal(rel->relids, sjinfo->syn_righthand));

	/* If result already cached, return it */
	if (rel->cheapest_unique_path)
		return (UniquePath *) rel->cheapest_unique_path;

	/* If we previously failed, return NULL quickly */
	if (sjinfo->join_quals == NIL)
		return NULL;

	/*
	 * We must ensure path struct and subsidiary data are allocated in main
	 * planning context; otherwise GEQO memory management causes trouble.
	 * (Compare best_inner_indexscan().)
	 */
	oldcontext = MemoryContextSwitchTo(root->planner_cxt);

	/*----------
	 * Look to see whether the semijoin's join quals consist of AND'ed
	 * equality operators, with (only) RHS variables on only one side of
	 * each one.  If so, we can figure out how to enforce uniqueness for
	 * the RHS.
	 *
	 * Note that the input join_quals list is the list of quals that are
	 * *syntactically* associated with the semijoin, which in practice means
	 * the synthesized comparison list for an IN or the WHERE of an EXISTS.
	 * Particularly in the latter case, it might contain clauses that aren't
	 * *semantically* associated with the join, but refer to just one side or
	 * the other.  We can ignore such clauses here, as they will just drop
	 * down to be processed within one side or the other.  (It is okay to
	 * consider only the syntactically-associated clauses here because for a
	 * semijoin, no higher-level quals could refer to the RHS, and so there
	 * can be no other quals that are semantically associated with this join.
	 * We do things this way because it is useful to be able to run this test
	 * before we have extracted the list of quals that are actually
	 * semantically associated with the particular join.)
	 *
	 * Note that the in_operators list consists of the joinqual operators
	 * themselves (but commuted if needed to put the RHS value on the right).
	 * These could be cross-type operators, in which case the operator
	 * actually needed for uniqueness is a related single-type operator.
	 * We assume here that that operator will be available from the btree
	 * or hash opclass when the time comes ... if not, create_unique_plan()
	 * will fail.
	 *----------
	 */
	in_operators = NIL;
	uniq_exprs = NIL;
	all_btree = true;
	all_hash = enable_hashagg;	/* don't consider hash if not enabled */
	foreach(lc, sjinfo->join_quals)
	{
		OpExpr	   *op = (OpExpr *) lfirst(lc);
		Oid			opno;
		Node	   *left_expr;
		Node	   *right_expr;
		Relids		left_varnos;
		Relids		right_varnos;
		Relids		all_varnos;

		/* Is it a binary opclause? */
		if (!IsA(op, OpExpr) ||
			list_length(op->args) != 2)
		{
			/* No, but does it reference both sides? */
			all_varnos = pull_varnos((Node *) op);
			if (!bms_overlap(all_varnos, sjinfo->syn_righthand) ||
				bms_is_subset(all_varnos, sjinfo->syn_righthand))
			{
				/*
				 * Clause refers to only one rel, so ignore it --- unless it
				 * contains volatile functions, in which case we'd better
				 * punt.
				 */
				if (contain_volatile_functions((Node *) op))
					goto no_unique_path;
				continue;
			}
			/* Non-operator clause referencing both sides, must punt */
			goto no_unique_path;
		}

		/* Extract data from binary opclause */
		opno = op->opno;
		left_expr = linitial(op->args);
		right_expr = lsecond(op->args);
		left_varnos = pull_varnos(left_expr);
		right_varnos = pull_varnos(right_expr);
		all_varnos = bms_union(left_varnos, right_varnos);

		/* Does it reference both sides? */
		if (!bms_overlap(all_varnos, sjinfo->syn_righthand) ||
			bms_is_subset(all_varnos, sjinfo->syn_righthand))
		{
			/*
			 * Clause refers to only one rel, so ignore it --- unless it
			 * contains volatile functions, in which case we'd better punt.
			 */
			if (contain_volatile_functions((Node *) op))
				goto no_unique_path;
			continue;
		}

		/* check rel membership of arguments */
		if (!bms_is_empty(right_varnos) &&
			bms_is_subset(right_varnos, sjinfo->syn_righthand) &&
			!bms_overlap(left_varnos, sjinfo->syn_righthand))
		{
			/* typical case, right_expr is RHS variable */
		}
		else if (!bms_is_empty(left_varnos) &&
				 bms_is_subset(left_varnos, sjinfo->syn_righthand) &&
				 !bms_overlap(right_varnos, sjinfo->syn_righthand))
		{
			/* flipped case, left_expr is RHS variable */
			opno = get_commutator(opno);
			if (!OidIsValid(opno))
				goto no_unique_path;
			right_expr = left_expr;
		}
		else
			goto no_unique_path;

		/* all operators must be btree equality or hash equality */
		if (all_btree)
		{
			/* oprcanmerge is considered a hint... */
			if (!op_mergejoinable(opno) ||
				get_mergejoin_opfamilies(opno) == NIL)
				all_btree = false;
		}
		if (all_hash)
		{
			/* ... but oprcanhash had better be correct */
			if (!op_hashjoinable(opno))
				all_hash = false;
		}
		if (!(all_btree || all_hash))
			goto no_unique_path;

		/* so far so good, keep building lists */
		in_operators = lappend_oid(in_operators, opno);
		uniq_exprs = lappend(uniq_exprs, copyObject(right_expr));
	}

	/* Punt if we didn't find at least one column to unique-ify */
	if (uniq_exprs == NIL)
		goto no_unique_path;

	/*
	 * The expressions we'd need to unique-ify mustn't be volatile.
	 */
	if (contain_volatile_functions((Node *) uniq_exprs))
		goto no_unique_path;

    /* Repartition first if duplicates might be on different QEs. */
    if (!CdbPathLocus_IsBottleneck(subpath->locus) &&
		!cdbpathlocus_is_hashed_on_exprs(subpath->locus, uniq_exprs))
	{
		goto no_unique_path;
        locus = cdbpathlocus_from_exprs(root, uniq_exprs);
        subpath = cdbpath_create_motion_path(root, subpath, NIL, false, locus);
        Insist(subpath);
	}
	else
		locus = subpath->locus;

	/*
	 * If we get here, we can unique-ify using at least one of sorting and
	 * hashing.  Start building the result Path object.
	 */
	pathnode = makeNode(UniquePath);

	pathnode->path.pathtype = T_Unique;
	pathnode->path.parent = rel;
	pathnode->path.locus = locus;

	/*
	 * Treat the output as always unsorted, since we don't necessarily have
	 * pathkeys to represent it.
	 */
	pathnode->path.pathkeys = NIL;

	pathnode->subpath = subpath;
	pathnode->in_operators = in_operators;
	pathnode->uniq_exprs = uniq_exprs;

	/*
	 * If the input is a subquery whose output must be unique already, then we
	 * don't need to do anything.  The test for uniqueness has to consider
	 * exactly which columns we are extracting; for example "SELECT DISTINCT
	 * x,y" doesn't guarantee that x alone is distinct. So we cannot check for
	 * this optimization unless uniq_exprs consists only of simple Vars
	 * referencing subquery outputs.  (Possibly we could do something with
	 * expressions in the subquery outputs, too, but for now keep it simple.)
	 */
	if (rel->rtekind == RTE_SUBQUERY)
	{
		RangeTblEntry *rte = planner_rt_fetch(rel->relid, root);
		List	   *sub_tlist_colnos;

		sub_tlist_colnos = translate_sub_tlist(uniq_exprs, rel->relid);

		if (sub_tlist_colnos &&
			query_is_distinct_for(rte->subquery,
								  sub_tlist_colnos, in_operators))
		{
			pathnode->umethod = UNIQUE_PATH_NOOP;
			pathnode->rows = rel->rows;
			pathnode->path.startup_cost = subpath->startup_cost;
			pathnode->path.total_cost = subpath->total_cost;
			pathnode->path.pathkeys = subpath->pathkeys;

			rel->cheapest_unique_path = (Path *) pathnode;

			MemoryContextSwitchTo(oldcontext);

			return pathnode;
		}
	}

	/* Estimate number of output rows */
	pathnode->rows = estimate_num_groups(root, uniq_exprs, rel->rows);
	numCols = list_length(uniq_exprs);

	// FIXME?
    //subpath_rows = cdbpath_rows(root, subpath);

	if (all_btree)
	{
		/*
		 * Estimate cost for sort+unique implementation
		 */
		cost_sort(&sort_path, root, NIL,
				  subpath->total_cost,
				  rel->rows,
				  rel->width,
				  -1.0);

		/*
		 * Charge one cpu_operator_cost per comparison per input tuple. We
		 * assume all columns get compared at most of the tuples. (XXX
		 * probably this is an overestimate.)  This should agree with
		 * make_unique.
		 */
		sort_path.total_cost += cpu_operator_cost * rel->rows * numCols;
	}

	if (all_hash)
	{
		/*
		 * Estimate the overhead per hashtable entry at 64 bytes (same as in
		 * planner.c).
		 */
		int			hashentrysize = rel->width + 64;

		if (hashentrysize * pathnode->rows > work_mem * 1024L)
			all_hash = false;	/* don't try to hash */
		else
			cost_agg(&agg_path, root,
					 AGG_HASHED, 0,
					 numCols, pathnode->rows,
					 subpath->startup_cost,
					 subpath->total_cost,
					 rel->rows,
					 0, /* input_width */
					 0, /* hash_batches */
					 0, /* hashentry_width */
					 false /* streaming */
				);
	}

	if (all_btree && all_hash)
	{
		if (agg_path.total_cost < sort_path.total_cost)
			pathnode->umethod = UNIQUE_PATH_HASH;
		else
			pathnode->umethod = UNIQUE_PATH_SORT;
	}
	else if (all_btree)
		pathnode->umethod = UNIQUE_PATH_SORT;
	else if (all_hash)
		pathnode->umethod = UNIQUE_PATH_HASH;
	else
		goto no_unique_path;

	if (pathnode->umethod == UNIQUE_PATH_HASH)
	{
		pathnode->path.startup_cost = agg_path.startup_cost;
		pathnode->path.total_cost = agg_path.total_cost;
	}
	else
	{
		pathnode->path.startup_cost = sort_path.startup_cost;
		pathnode->path.total_cost = sort_path.total_cost;
	}

	rel->cheapest_unique_path = (Path *) pathnode;

	MemoryContextSwitchTo(oldcontext);

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

no_unique_path:			/* failure exit */

	/* Mark the SpecialJoinInfo as not unique-able */
	sjinfo->join_quals = NIL;

	MemoryContextSwitchTo(oldcontext);

	return NULL;
}

/*
 * create_unique_rowid_path (GPDB)
 *
 * Create a UniquePath to deduplicate based on the ctid and gp_segment_id,
 * or some other columns that uniquely identify a row. This is used as part
 * of implementing semi-joins (such as "x IN (SELECT ...)").
 *
 * In PostgreSQL, semi-joins are implemented with JOIN_SEMI join types, or
 * by first eliminating duplicates from the inner side, and then performing
 * normal inner join (that's JOIN_UNIQUE_OUTER and JOIN_UNIQUE_INNER). GPDB
 * has a third way to implement them: Perform an inner join first, and then
 * eliminate duplicates from the result. The JOIN_DEDUP_SEMI and
 * JOIN_DEDUP_SEMI_REVERSE join types indicate such plans.
 *
 * The JOIN_DEDUP_SEMI plan will look something like this:
 *
 * postgres=# explain select * from s where exists (select 1 from r where s.a = r.b);
 *                                                       QUERY PLAN                                                      
 * ----------------------------------------------------------------------------------------------------------------------
 *  Gather Motion 3:1  (slice3; segments: 3)  (cost=189.75..190.75 rows=100 width=18)
 *    ->  HashAggregate  (cost=189.75..190.75 rows=34 width=18)
 *          Group By: s.ctid::bigint, s.gp_segment_id
 *          ->  Result  (cost=11.75..189.25 rows=34 width=18)
 *                ->  Redistribute Motion 3:3  (slice2; segments: 3)  (cost=11.75..189.25 rows=34 width=18)
 *                      Hash Key: s.ctid
 *                      ->  Hash Join  (cost=11.75..187.25 rows=34 width=18)
 *                            Hash Cond: r.b = s.a
 *                            ->  Seq Scan on r  (cost=0.00..112.00 rows=3334 width=4)
 *                            ->  Hash  (cost=8.00..8.00 rows=100 width=18)
 *                                  ->  Broadcast Motion 3:3  (slice1; segments: 3)  (cost=0.00..8.00 rows=100 width=18)
 *                                        ->  Seq Scan on s  (cost=0.00..4.00 rows=34 width=18)
 *  Settings:  optimizer=off
 *  Optimizer status: legacy query optimizer
 * (14 rows)
 *
 *
 * In PostgreSQL, this is never better than doing a JOIN_SEMI directly.
 * But it can be a win in GPDB, if the distribution of the outer and inner
 * relations don't match, and the outer relation is much larger than the
 * inner relation. In the above example, a normal semi-join would have to
 * have 's' on the outer side, and 'r' on the inner side. A hash semi-join
 * can't be performed the other way 'round, because the duplicate
 * elimination in a semi-join is done when building the hash table.
 * Furthermore, you can't have a Broadcast motion on the outer side of
 * a semi-join, because that could also generate duplicates. That leaves
 * the planner no choice, but to redistribute the larger 'r' relation,
 * in a JOIN_SEMI plan.
 *
 * So in GPDB, we try to implement semi-joins as a inner joins, followed
 * by an explicit UniquePath to eliminate the duplicates. That allows the
 * above plan, where the smaller 's' relation is Broadcast to all the
 * segments, and the duplicates that can arise from doing that are eliminated
 * above the join. You get one more Motion than with a JOIN_SEMI plan, but
 * each Motion has to move much fewer rows.
 *
 * The role of this function is to insert the UniquePath to represent
 * the deduplication above the join. Returns a UniquePath node representing
 * a "DISTINCT ON r1,...,rn" operator, where (r1,...,rn) represents a unique
 * identifier for each row of the cross product of the tables specified by
 * the 'distinct_relids' parameter.
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
UniquePath *
create_unique_rowid_path(PlannerInfo *root,
						 RelOptInfo *rel,
                         Path        *subpath,
                         Relids       distinct_relids)
{
	UniquePath *pathnode;
	CdbPathLocus locus;
	Path		sort_path;		/* dummy for result of cost_sort */
	Path		agg_path;		/* dummy for result of cost_agg */
	int			numCols;
	bool		all_btree;
	bool		all_hash;

    Assert(!bms_is_empty(distinct_relids));

	/*
	 * For easier merging (albeit it's going to manual), keep this function
	 * similar to create_unique_path(). In this function, we deduplicate based
	 * on ctid and gp_segment_id, or other unique identifiers that we generate
	 * on the fly. Sorting and hashing are both possible, but we keep these
	 * as variables to resemble create_unique_path().
	 */
	all_btree = true;
	all_hash = true;

	locus = subpath->locus;

	/*
	 * Start building the result Path object.
	 */
	pathnode = makeNode(UniquePath);

	pathnode->path.pathtype = T_Unique;
	pathnode->path.parent = rel;
	pathnode->path.locus = locus;

	/*
	 * Treat the output as always unsorted, since we don't necessarily have
	 * pathkeys to represent it.
	 */
	pathnode->path.pathkeys = NIL;

	pathnode->subpath = subpath;
	pathnode->in_operators = NIL;
	pathnode->uniq_exprs = NIL;
	pathnode->distinct_on_rowid_relids = distinct_relids;

	/*
	 * For cost estimation purposes, assume we'll deduplicate based on ctid and
	 * gp_segment_id. If the outer side of the join is a join relation itself,
	 * we'll need to deduplicate based on gp_segment_id and ctid of all the
	 * involved base tables, or other identifiers. See cdbpath_dedup_fixup()
	 * for the details, but here, for cost estimation purposes, just assume
	 * it's going to be two columns.
	 */
	numCols	= 2;
	pathnode->rows = rel->rows;

	if (all_btree)
	{
		/*
		 * Estimate cost for sort+unique implementation
		 */
		cost_sort(&sort_path, root, NIL,
				  subpath->total_cost,
				  rel->rows,
				  rel->width,
				  -1.0);

		/*
		 * Charge one cpu_operator_cost per comparison per input tuple. We
		 * assume all columns get compared at most of the tuples. (XXX
		 * probably this is an overestimate.)  This should agree with
		 * make_unique.
		 */
		sort_path.total_cost += cpu_operator_cost * rel->rows * numCols;
	}

	if (all_hash)
	{
		/*
		 * Estimate the overhead per hashtable entry at 64 bytes (same as in
		 * planner.c).
		 */
		int			hashentrysize = rel->width + 64;

		if (hashentrysize * pathnode->rows > work_mem * 1024L)
			all_hash = false;	/* don't try to hash */
		else
			cost_agg(&agg_path, root,
					 AGG_HASHED, 0,
					 numCols, pathnode->rows,
					 subpath->startup_cost,
					 subpath->total_cost,
					 rel->rows,
					 0, /* input_width */
					 0, /* hash_batches */
					 0, /* hashentry_width */
					 false /* streaming */
				);
	}

	if (all_btree && all_hash)
	{
		if (agg_path.total_cost < sort_path.total_cost)
			pathnode->umethod = UNIQUE_PATH_HASH;
		else
			pathnode->umethod = UNIQUE_PATH_SORT;
	}
	else if (all_btree)
		pathnode->umethod = UNIQUE_PATH_SORT;
	else if (all_hash)
		pathnode->umethod = UNIQUE_PATH_HASH;
	else
	{
		Assert(false);
	}

	if (pathnode->umethod == UNIQUE_PATH_HASH)
	{
		pathnode->path.startup_cost = agg_path.startup_cost;
		pathnode->path.total_cost = agg_path.total_cost;
	}
	else
	{
		pathnode->path.startup_cost = sort_path.startup_cost;
		pathnode->path.total_cost = sort_path.total_cost;
	}

    /* Add repartitioning cost if duplicates might be on different QEs. */
    if (!CdbPathLocus_IsBottleneck(subpath->locus) &&
        !cdbpathlocus_is_hashed_on_relids(subpath->locus, distinct_relids))
    {
        CdbMotionPath   motionpath;     /* dummy for cost estimate */
        Cost            repartition_cost;

        /* Tell create_unique_plan() to insert Motion operator atop subpath. */
        pathnode->must_repartition = true;

        /* Set a fake locus.  Repartitioning key won't be built until later. */
        CdbPathLocus_MakeStrewn(&pathnode->path.locus);
		pathnode->path.sameslice_relids = NULL;

        /* Estimate repartitioning cost. */
        memset(&motionpath, 0, sizeof(motionpath));
        motionpath.path.type = T_CdbMotionPath;
        motionpath.path.parent = subpath->parent;
        motionpath.path.locus = pathnode->path.locus;
        motionpath.subpath = subpath;
        cdbpath_cost_motion(root, &motionpath);

        /* Add MotionPath cost to UniquePath cost. */
        repartition_cost = motionpath.path.total_cost - subpath->total_cost;
        pathnode->path.total_cost += repartition_cost;
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
}                               /* create_unique_rowid_path */

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
	/* pathnode->materialize_inner will be set by cost_mergejoin */

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

	/*
	 * CDB: If gp_enable_hashjoin_size_heuristic is set, disallow inner
	 * joins where the inner rel is the larger of the two inputs.
	 *
	 * Note cdbpath_motion_for_join() has to precede this so we can get
	 * the right row count, in case Broadcast Motion is inserted above an
	 * input path.
	 */
	if (jointype == JOIN_INNER &&
		root->config->gp_enable_hashjoin_size_heuristic &&
		!root->config->mpp_trying_fallback_plan)
	{
		double		outersize;
		double		innersize;

		outersize = ExecHashRowSize(outer_path->parent->width) *
			cdbpath_rows(root, outer_path);
		innersize = ExecHashRowSize(inner_path->parent->width) *
			cdbpath_rows(root, inner_path);

		if (innersize > outersize)
			return NULL;
	}

	pathnode = makeNode(HashPath);

	pathnode->jpath.path.pathtype = T_HashJoin;
	pathnode->jpath.path.parent = joinrel;
	pathnode->jpath.jointype = jointype;
	pathnode->jpath.outerjoinpath = outer_path;
	pathnode->jpath.innerjoinpath = inner_path;
	pathnode->jpath.joinrestrictinfo = restrict_clauses;

	/*
	 * A hashjoin never has pathkeys, since its output ordering is
	 * unpredictable due to possible batching.	XXX If the inner relation is
	 * small enough, we could instruct the executor that it must not batch,
	 * and then we could assume that the output inherits the outer relation's
	 * ordering, which might save a sort step.	However there is considerable
	 * downside if our estimate of the inner relation size is badly off. For
	 * the moment we don't risk it.  (Note also that if we wanted to take this
	 * seriously, joinpath.c would have to consider many more paths for the
	 * outer rel than it does now.)
	 */
	pathnode->jpath.path.pathkeys = NIL;
    pathnode->jpath.path.locus = join_locus;

	pathnode->path_hashclauses = hashclauses;
	/* cost_hashjoin will fill in pathnode->num_batches */

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
