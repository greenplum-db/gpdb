/*-------------------------------------------------------------------------
 *
 * pathnode.c
 *	  Routines to manipulate pathlists and create path nodes
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/util/pathnode.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/tlist.h"
#include "parser/parsetree.h"
#include "utils/faultinjector.h"
#include "utils/lsyscache.h"
#include "utils/selfuncs.h"

#include "catalog/pg_proc.h"
#include "cdb/cdbhash.h"        /* cdb_default_distribution_opfamily_for_type() */
#include "cdb/cdbpath.h"        /* cdb_create_motion_path() etc */
#include "cdb/cdbutil.h"		/* getgpsegmentCount() */
#include "cdb/cdbvars.h"
#include "executor/nodeHash.h"

typedef enum
{
	COSTS_EQUAL,				/* path costs are fuzzily equal */
	COSTS_BETTER1,				/* first path is cheaper than second */
	COSTS_BETTER2,				/* second path is cheaper than first */
	COSTS_DIFFERENT				/* neither path dominates the other on cost */
} PathCostComparison;

static List *translate_sub_tlist(List *tlist, int relid);
static bool query_is_distinct_for(Query *query, List *colnos, List *opids);
static Oid	distinct_col_search(int colno, List *colnos, List *opids);

static void set_append_path_locus(PlannerInfo *root, Path *pathnode, RelOptInfo *rel,
					  List *pathkeys);

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
	CdbVisitOpt whatnext;

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
	ListCell   *cell;
	Path	   *path;
	CdbVisitOpt v = CdbVisit_Walk;

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
		case T_ForeignScan:
		case T_IndexScan:
		case T_IndexOnlyScan:
		case T_TidScan:
		case T_SubqueryScan:
		case T_FunctionScan:
		case T_ValuesScan:
		case T_CteScan:
		case T_WorkTableScan:
		case T_TableFunctionScan:
		case T_Result:
			return CdbVisit_Walk;
		case T_BitmapHeapScan:
			v = pathnode_walk_node(((BitmapHeapPath *)path)->bitmapqual, walker, context);
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
		case T_MergeAppend:
			v = pathnode_walk_list(((MergeAppendPath *)path)->subpaths, walker, context);
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
 * compare_path_costs_fuzzily
 *	  Compare the costs of two paths to see if either can be said to
 *	  dominate the other.
 *
 * We use fuzzy comparisons so that add_path() can avoid keeping both of
 * a pair of paths that really have insignificantly different cost.
 *
 * The fuzz_factor argument must be 1.0 plus delta, where delta is the
 * fraction of the smaller cost that is considered to be a significant
 * difference.  For example, fuzz_factor = 1.01 makes the fuzziness limit
 * be 1% of the smaller cost.
 *
 * The two paths are said to have "equal" costs if both startup and total
 * costs are fuzzily the same.  Path1 is said to be better than path2 if
 * it has fuzzily better startup cost and fuzzily no worse total cost,
 * or if it has fuzzily better total cost and fuzzily no worse startup cost.
 * Path2 is better than path1 if the reverse holds.  Finally, if one path
 * is fuzzily better than the other on startup cost and fuzzily worse on
 * total cost, we just say that their costs are "different", since neither
 * dominates the other across the whole performance spectrum.
 *
 * This function also enforces a policy rule that paths for which the relevant
 * one of parent->consider_startup and parent->consider_param_startup is false
 * cannot win comparisons on the grounds of good startup cost, so we never
 * return COSTS_DIFFERENT when that is true for the total-cost loser.
 */
static PathCostComparison
compare_path_costs_fuzzily(Path *path1, Path *path2, double fuzz_factor)
{
#define CONSIDER_PATH_STARTUP_COST(p)  \
	((p)->param_info == NULL ? (p)->parent->consider_startup : (p)->parent->consider_param_startup)

	/*
	 * Check total cost first since it's more likely to be different; many
	 * paths have zero startup cost.
	 */
	if (path1->total_cost > path2->total_cost * fuzz_factor)
	{
		/* path1 fuzzily worse on total cost */
		if (CONSIDER_PATH_STARTUP_COST(path1) &&
			path2->startup_cost > path1->startup_cost * fuzz_factor)
		{
			/* ... but path2 fuzzily worse on startup, so DIFFERENT */
			return COSTS_DIFFERENT;
		}
		/* else path2 dominates */
		return COSTS_BETTER2;
	}
	if (path2->total_cost > path1->total_cost * fuzz_factor)
	{
		/* path2 fuzzily worse on total cost */
		if (CONSIDER_PATH_STARTUP_COST(path2) &&
			path1->startup_cost > path2->startup_cost * fuzz_factor)
		{
			/* ... but path1 fuzzily worse on startup, so DIFFERENT */
			return COSTS_DIFFERENT;
		}
		/* else path1 dominates */
		return COSTS_BETTER1;
	}

	/*
	 * Fuzzily the same on total cost (so we might as well compare startup
	 * cost, even when that would otherwise be uninteresting; but
	 * parameterized paths aren't allowed to win this way, we'd rather move on
	 * to other comparison heuristics)
	 */
	if (path1->startup_cost > path2->startup_cost * fuzz_factor &&
		path2->param_info == NULL)
	{
		/* ... but path1 fuzzily worse on startup, so path2 wins */
		return COSTS_BETTER2;
	}
	if (path2->startup_cost > path1->startup_cost * fuzz_factor &&
		path1->param_info == NULL)
	{
		/* ... but path2 fuzzily worse on startup, so path1 wins */
		return COSTS_BETTER1;
	}
	/* fuzzily the same on both costs */
	return COSTS_EQUAL;

#undef CONSIDER_PATH_STARTUP_COST
}

/*
 * set_cheapest
 *	  Find the minimum-cost paths from among a relation's paths,
 *	  and save them in the rel's cheapest-path fields.
 *
 * cheapest_total_path is normally the cheapest-total-cost unparameterized
 * path; but if there are no unparameterized paths, we assign it to be the
 * best (cheapest least-parameterized) parameterized path.  However, only
 * unparameterized paths are considered candidates for cheapest_startup_path,
 * so that will be NULL if there are no unparameterized paths.
 *
 * The cheapest_parameterized_paths list collects all parameterized paths
 * that have survived the add_path() tournament for this relation.  (Since
 * add_path ignores pathkeys for a parameterized path, these will be paths
 * that have best cost or best row count for their parameterization.)
 * cheapest_parameterized_paths always includes the cheapest-total
 * unparameterized path, too, if there is one; the users of that list find
 * it more convenient if that's included.
 *
 * This is normally called only after we've finished constructing the path
 * list for the rel node.
 */
void
set_cheapest(RelOptInfo *parent_rel)
{
	Path	   *cheapest_startup_path;
	Path	   *cheapest_total_path;
	Path	   *best_param_path;
	List	   *parameterized_paths;
	ListCell   *p;

	Assert(IsA(parent_rel, RelOptInfo));

	if (parent_rel->pathlist == NIL)
		elog(ERROR, "could not devise a query plan for the given query");

	cheapest_startup_path = cheapest_total_path = best_param_path = NULL;
	parameterized_paths = NIL;

	foreach(p, parent_rel->pathlist)
	{
		Path	   *path = (Path *) lfirst(p);
		int			cmp;

		if (path->param_info)
		{
			/* Parameterized path, so add it to parameterized_paths */
			parameterized_paths = lappend(parameterized_paths, path);

			/*
			 * If we have an unparameterized cheapest-total, we no longer care
			 * about finding the best parameterized path, so move on.
			 */
			if (cheapest_total_path)
				continue;

			/*
			 * Otherwise, track the best parameterized path, which is the one
			 * with least total cost among those of the minimum
			 * parameterization.
			 */
			if (best_param_path == NULL)
				best_param_path = path;
			else
			{
				switch (bms_subset_compare(PATH_REQ_OUTER(path),
										   PATH_REQ_OUTER(best_param_path)))
				{
					case BMS_EQUAL:
						/* keep the cheaper one */
						if (compare_path_costs(path, best_param_path,
											   TOTAL_COST) < 0)
							best_param_path = path;
						break;
					case BMS_SUBSET1:
						/* new path is less-parameterized */
						best_param_path = path;
						break;
					case BMS_SUBSET2:
						/* old path is less-parameterized, keep it */
						break;
					case BMS_DIFFERENT:

						/*
						 * This means that neither path has the least possible
						 * parameterization for the rel.  We'll sit on the old
						 * path until something better comes along.
						 */
						break;
				}
			}
		}
		else
		{
			/* Unparameterized path, so consider it for cheapest slots */
			if (cheapest_total_path == NULL)
			{
				cheapest_startup_path = cheapest_total_path = path;
				continue;
			}

			/*
			 * If we find two paths of identical costs, try to keep the
			 * better-sorted one.  The paths might have unrelated sort
			 * orderings, in which case we can only guess which might be
			 * better to keep, but if one is superior then we definitely
			 * should keep that one.
			 */
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
	}

	/* Add cheapest unparameterized path, if any, to parameterized_paths */
	if (cheapest_total_path)
		parameterized_paths = lcons(cheapest_total_path, parameterized_paths);

	/*
	 * If there is no unparameterized path, use the best parameterized path as
	 * cheapest_total_path (but not as cheapest_startup_path).
	 */
	if (cheapest_total_path == NULL)
		cheapest_total_path = best_param_path;
	Assert(cheapest_total_path != NULL);

	parent_rel->cheapest_startup_path = cheapest_startup_path;
	parent_rel->cheapest_total_path = cheapest_total_path;
	parent_rel->cheapest_unique_path = NULL;	/* computed only if needed */
	parent_rel->cheapest_parameterized_paths = parameterized_paths;
}

/*
 * add_path
 *	  Consider a potential implementation path for the specified parent rel,
 *	  and add it to the rel's pathlist if it is worthy of consideration.
 *	  A path is worthy if it has a better sort order (better pathkeys) or
 *	  cheaper cost (on either dimension), or generates fewer rows, than any
 *	  existing path that has the same or superset parameterization rels.
 *
 *	  We also remove from the rel's pathlist any old paths that are dominated
 *	  by new_path --- that is, new_path is cheaper, at least as well ordered,
 *	  generates no more rows, and requires no outer rels not required by the
 *	  old path.
 *
 *	  In most cases, a path with a superset parameterization will generate
 *	  fewer rows (since it has more join clauses to apply), so that those two
 *	  figures of merit move in opposite directions; this means that a path of
 *	  one parameterization can seldom dominate a path of another.  But such
 *	  cases do arise, so we make the full set of checks anyway.
 *
 *	  There are two policy decisions embedded in this function, along with
 *	  its sibling add_path_precheck.  First, we treat all parameterized paths
 *	  as having NIL pathkeys, so that they cannot win comparisons on the
 *	  basis of sort order.  This is to reduce the number of parameterized
 *	  paths that are kept; see discussion in src/backend/optimizer/README.
 *
 *	  Second, we only consider cheap startup cost to be interesting if
 *	  parent_rel->consider_startup is true for an unparameterized path, or
 *	  parent_rel->consider_param_startup is true for a parameterized one.
 *	  Again, this allows discarding useless paths sooner.
 *
 *	  The pathlist is kept sorted by total_cost, with cheaper paths
 *	  at the front.  Within this routine, that's simply a speed hack:
 *	  doing it that way makes it more likely that we will reject an inferior
 *	  path after a few comparisons, rather than many comparisons.
 *	  However, add_path_precheck relies on this ordering to exit early
 *	  when possible.
 *
 *	  NOTE: discarded Path objects are immediately pfree'd to reduce planner
 *	  memory consumption.  We dare not try to free the substructure of a Path,
 *	  since much of it may be shared with other Paths or the query tree itself;
 *	  but just recycling discarded Path nodes is a very useful savings in
 *	  a large join tree.  We can recycle the List nodes of pathlist, too.
 *
 *	  NB: The Path that is passed to add_path() must be considered invalid
 *	  upon return, and not touched again by the caller, because we free it
 *	  if we already know of a better path.  Likewise, a Path that is passed
 *	  to add_path() must not be shared as a subpath of any other Path of the
 *	  same join level.
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
add_path(RelOptInfo *parent_rel, Path *new_path)
{
	bool		accept_new = true;		/* unless we find a superior old path */
	ListCell   *insert_after = NULL;	/* where to insert new item */
	List	   *new_path_pathkeys;
	ListCell   *p1;
	ListCell   *p1_prev;
	ListCell   *p1_next;

	/*
	 * This is a convenient place to check for query cancel --- no part of the
	 * planner goes very long without calling add_path().
	 */
	CHECK_FOR_INTERRUPTS();

	if (!new_path)
		return;

	Assert(cdbpathlocus_is_valid(new_path->locus));

	/* Pretend parameterized paths have no pathkeys, per comment above */
	new_path_pathkeys = new_path->param_info ? NIL : new_path->pathkeys;

	/*
	 * Loop to check proposed new path against old paths.  Note it is possible
	 * for more than one old path to be tossed out because new_path dominates
	 * it.
	 *
	 * We can't use foreach here because the loop body may delete the current
	 * list cell.
	 */
	p1_prev = NULL;
	for (p1 = list_head(parent_rel->pathlist); p1 != NULL; p1 = p1_next)
	{
		Path	   *old_path = (Path *) lfirst(p1);
		bool		remove_old = false; /* unless new proves superior */
		PathCostComparison costcmp;
		PathKeysComparison keyscmp;
		BMS_Comparison outercmp;

		p1_next = lnext(p1);

		/*
		 * Do a fuzzy cost comparison with 1% fuzziness limit.  (XXX does this
		 * percentage need to be user-configurable?)
		 */
		costcmp = compare_path_costs_fuzzily(new_path, old_path, 1.01);

		/*
		 * If the two paths compare differently for startup and total cost,
		 * then we want to keep both, and we can skip comparing pathkeys and
		 * required_outer rels.  If they compare the same, proceed with the
		 * other comparisons.  Row count is checked last.  (We make the tests
		 * in this order because the cost comparison is most likely to turn
		 * out "different", and the pathkeys comparison next most likely.  As
		 * explained above, row count very seldom makes a difference, so even
		 * though it's cheap to compare there's not much point in checking it
		 * earlier.)
		 */
		if (costcmp != COSTS_DIFFERENT)
		{
			/* Similarly check to see if either dominates on pathkeys */
			List	   *old_path_pathkeys;

			old_path_pathkeys = old_path->param_info ? NIL : old_path->pathkeys;
			keyscmp = compare_pathkeys(new_path_pathkeys,
									   old_path_pathkeys);
			if (keyscmp != PATHKEYS_DIFFERENT)
			{
				switch (costcmp)
				{
					case COSTS_EQUAL:
						outercmp = bms_subset_compare(PATH_REQ_OUTER(new_path),
												   PATH_REQ_OUTER(old_path));
						if (keyscmp == PATHKEYS_BETTER1)
						{
							if ((outercmp == BMS_EQUAL ||
								 outercmp == BMS_SUBSET1) &&
								new_path->rows <= old_path->rows)
								remove_old = true;		/* new dominates old */
						}
						else if (keyscmp == PATHKEYS_BETTER2)
						{
							if ((outercmp == BMS_EQUAL ||
								 outercmp == BMS_SUBSET2) &&
								new_path->rows >= old_path->rows)
								accept_new = false;		/* old dominates new */
						}
						else	/* keyscmp == PATHKEYS_EQUAL */
						{
							if (outercmp == BMS_EQUAL)
							{
								/*
								 * Same pathkeys and outer rels, and fuzzily
								 * the same cost, so keep just one; to decide
								 * which, first check rows and then do a fuzzy
								 * cost comparison with very small fuzz limit.
								 * (We used to do an exact cost comparison,
								 * but that results in annoying
								 * platform-specific plan variations due to
								 * roundoff in the cost estimates.)  If things
								 * are still tied, arbitrarily keep only the
								 * old path.  Notice that we will keep only
								 * the old path even if the less-fuzzy
								 * comparison decides the startup and total
								 * costs compare differently.
								 */
								if (new_path->rows < old_path->rows)
									remove_old = true;	/* new dominates old */
								else if (new_path->rows > old_path->rows)
									accept_new = false; /* old dominates new */
								else if (compare_path_costs_fuzzily(new_path,
																	old_path,
											  1.0000000001) == COSTS_BETTER1)
									remove_old = true;	/* new dominates old */
								else
									accept_new = false; /* old equals or
														 * dominates new */
							}
							else if (outercmp == BMS_SUBSET1 &&
									 new_path->rows <= old_path->rows)
								remove_old = true;		/* new dominates old */
							else if (outercmp == BMS_SUBSET2 &&
									 new_path->rows >= old_path->rows)
								accept_new = false;		/* old dominates new */
							/* else different parameterizations, keep both */
						}
						break;
					case COSTS_BETTER1:
						if (keyscmp != PATHKEYS_BETTER2)
						{
							outercmp = bms_subset_compare(PATH_REQ_OUTER(new_path),
												   PATH_REQ_OUTER(old_path));
							if ((outercmp == BMS_EQUAL ||
								 outercmp == BMS_SUBSET1) &&
								new_path->rows <= old_path->rows)
								remove_old = true;		/* new dominates old */
						}
						break;
					case COSTS_BETTER2:
						if (keyscmp != PATHKEYS_BETTER1)
						{
							outercmp = bms_subset_compare(PATH_REQ_OUTER(new_path),
												   PATH_REQ_OUTER(old_path));
							if ((outercmp == BMS_EQUAL ||
								 outercmp == BMS_SUBSET2) &&
								new_path->rows >= old_path->rows)
								accept_new = false;		/* old dominates new */
						}
						break;
					case COSTS_DIFFERENT:

						/*
						 * can't get here, but keep this case to keep compiler
						 * quiet
						 */
						break;
				}
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
			/* p1_prev does not advance */
		}
		else
		{
			/* new belongs after this old path if it has cost >= old's */
			if (new_path->total_cost >= old_path->total_cost)
				insert_after = p1;
			/* p1_prev advances */
			p1_prev = p1;
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
				  Relids required_outer, JoinPath *new_path)
{
	Path	   *path = (Path *) new_path;

	if (!new_path)
		return;

	if (orig_jointype == JOIN_DEDUP_SEMI)
	{
		Assert(new_path->jointype == JOIN_INNER);

		/*
		 * Skip rowid unique path if distinct rels are replicated tables
		 * The reason is ctid + gp_segment_id can not identify a logical
		 * row of replicated table.
		 *
		 * TODO: add a motion on top of segmentGeneral node to support
		 * rowid unique.
		 */
		if (CdbPathLocus_IsPartitioned(path->locus) &&
		    CdbPathLocus_IsSegmentGeneral(((JoinPath *)path)->outerjoinpath->locus))
			return;

		if (!root->disallow_unique_rowid_path)
		{
			path = (Path *) create_unique_rowid_path(root,
													 parent_rel,
													 (Path *) new_path,
													 new_path->outerjoinpath->parent->relids,
													 required_outer);
		}
		else
			return;
	}
	else if (orig_jointype == JOIN_DEDUP_SEMI_REVERSE)
	{
		Assert(new_path->jointype == JOIN_INNER);

		if (CdbPathLocus_IsPartitioned(path->locus) &&
		    CdbPathLocus_IsSegmentGeneral(((JoinPath *)path)->innerjoinpath->locus))
			return;

		if (!root->disallow_unique_rowid_path)
		{
			path = (Path *) create_unique_rowid_path(root,
													 parent_rel,
													 (Path *) new_path,
													 new_path->innerjoinpath->parent->relids,
													 required_outer);
		}
		else
			return;
	}

	add_path(parent_rel, path);
}

/*
 * add_path_precheck
 *	  Check whether a proposed new path could possibly get accepted.
 *	  We assume we know the path's pathkeys and parameterization accurately,
 *	  and have lower bounds for its costs.
 *
 * Note that we do not know the path's rowcount, since getting an estimate for
 * that is too expensive to do before prechecking.  We assume here that paths
 * of a superset parameterization will generate fewer rows; if that holds,
 * then paths with different parameterizations cannot dominate each other
 * and so we can simply ignore existing paths of another parameterization.
 * (In the infrequent cases where that rule of thumb fails, add_path will
 * get rid of the inferior path.)
 *
 * At the time this is called, we haven't actually built a Path structure,
 * so the required information has to be passed piecemeal.
 */
bool
add_path_precheck(RelOptInfo *parent_rel,
				  Cost startup_cost, Cost total_cost,
				  List *pathkeys, Relids required_outer)
{
	List	   *new_path_pathkeys;
	ListCell   *p1;

	/* Pretend parameterized paths have no pathkeys, per add_path policy */
	new_path_pathkeys = required_outer ? NIL : pathkeys;

	foreach(p1, parent_rel->pathlist)
	{
		Path	   *old_path = (Path *) lfirst(p1);
		PathKeysComparison keyscmp;

		/*
		 * We are looking for an old_path with the same parameterization (and
		 * by assumption the same rowcount) that dominates the new path on
		 * pathkeys as well as both cost metrics.  If we find one, we can
		 * reject the new path.
		 *
		 * For speed, we make exact rather than fuzzy cost comparisons. If an
		 * old path dominates the new path exactly on both costs, it will
		 * surely do so fuzzily.
		 */
		if (total_cost >= old_path->total_cost)
		{
			/* can win on startup cost only if unparameterized */
			if (startup_cost >= old_path->startup_cost || required_outer)
			{
				/* new path does not win on cost, so check pathkeys... */
				List	   *old_path_pathkeys;

				old_path_pathkeys = old_path->param_info ? NIL : old_path->pathkeys;
				keyscmp = compare_pathkeys(new_path_pathkeys,
										   old_path_pathkeys);
				if (keyscmp == PATHKEYS_EQUAL ||
					keyscmp == PATHKEYS_BETTER2)
				{
					/* new path does not win on pathkeys... */
					if (bms_equal(required_outer, PATH_REQ_OUTER(old_path)))
					{
						/* Found an old path that dominates the new one */
						return false;
					}
				}
			}
		}
		else
		{
			/*
			 * Since the pathlist is sorted by total_cost, we can stop looking
			 * once we reach a path with a total_cost larger than the new
			 * path's.
			 */
			break;
		}
	}

	return true;
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
create_seqscan_path(PlannerInfo *root, RelOptInfo *rel, Relids required_outer)
{
	Path	   *pathnode = makeNode(Path);

	pathnode->pathtype = T_SeqScan;
	pathnode->parent = rel;
	pathnode->param_info = get_baserel_parampathinfo(root, rel,
													 required_outer);
	pathnode->pathkeys = NIL;	/* seqscan has unordered result */

	pathnode->locus = cdbpathlocus_from_baserel(root, rel);
	pathnode->motionHazard = false;
	pathnode->rescannable = true;
	pathnode->sameslice_relids = rel->relids;

	cost_seqscan(pathnode, root, rel, pathnode->param_info);

	return pathnode;
}

/*
* Create a path for scanning an external table
 */
ExternalPath *
create_external_path(PlannerInfo *root, RelOptInfo *rel, Relids required_outer)
{
	ExternalPath   *pathnode = makeNode(ExternalPath);

	pathnode->path.pathtype = T_ExternalScan;
	pathnode->path.parent = rel;
	pathnode->path.param_info = get_baserel_parampathinfo(root, rel,
													 required_outer);
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

	cost_externalscan(pathnode, root, rel, pathnode->path.param_info);

	return pathnode;
}


/*
 * create_index_path
 *	  Creates a path node for an index scan.
 *
 * 'index' is a usable index.
 * 'indexclauses' is a list of RestrictInfo nodes representing clauses
 *			to be used as index qual conditions in the scan.
 * 'indexclausecols' is an integer list of index column numbers (zero based)
 *			the indexclauses can be used with.
 * 'indexorderbys' is a list of bare expressions (no RestrictInfos)
 *			to be used as index ordering operators in the scan.
 * 'indexorderbycols' is an integer list of index column numbers (zero based)
 *			the ordering operators can be used with.
 * 'pathkeys' describes the ordering of the path.
 * 'indexscandir' is ForwardScanDirection or BackwardScanDirection
 *			for an ordered index, or NoMovementScanDirection for
 *			an unordered index.
 * 'indexonly' is true if an index-only scan is wanted.
 * 'required_outer' is the set of outer relids for a parameterized path.
 * 'loop_count' is the number of repetitions of the indexscan to factor into
 *		estimates of caching behavior.
 *
 * Returns the new path node.
 */
IndexPath *
create_index_path(PlannerInfo *root,
				  IndexOptInfo *index,
				  List *indexclauses,
				  List *indexclausecols,
				  List *indexorderbys,
				  List *indexorderbycols,
				  List *pathkeys,
				  ScanDirection indexscandir,
				  bool indexonly,
				  Relids required_outer,
				  double loop_count)
{
	IndexPath  *pathnode = makeNode(IndexPath);
	RelOptInfo *rel = index->rel;
	List	   *indexquals,
			   *indexqualcols;

	/*
	 * Greenplum specific behavior
	 * Greenplum has a specical path to handle semjoin,
	 * planner might add unique_row_id path to first inner join
	 * and then de-duplicate. The logic is added into
	 * Greenplum long before, but when merging more code from
	 * upstream, the old logic does not consider new paths,
	 * for indexonly path, the var should be changed to the Index's,
	 * maybe we should enhance the old logic in cdbpath_dedup_fixup,
	 * but at a stable branch 6X, it seems risky. Here we introduce
	 * a switch to turn off it when seeing indexonly path.
	 *
	 * create_index_path happens when setting base rel, and unique
	 * rowid path happens when setting joining rels, so the turn-off
	 * method works.
	 */
	if (indexonly)
		root->disallow_unique_rowid_path = true;

	pathnode->path.pathtype = indexonly ? T_IndexOnlyScan : T_IndexScan;
	pathnode->path.parent = rel;
	pathnode->path.param_info = get_baserel_parampathinfo(root, rel,
														  required_outer);
	pathnode->path.pathkeys = pathkeys;

	/* Convert clauses to indexquals the executor can handle */
	expand_indexqual_conditions(index, indexclauses, indexclausecols,
								&indexquals, &indexqualcols);

	/* Fill in the pathnode */
	pathnode->indexinfo = index;
	pathnode->indexclauses = indexclauses;
	pathnode->indexquals = indexquals;
	pathnode->indexqualcols = indexqualcols;
	pathnode->indexorderbys = indexorderbys;
	pathnode->indexorderbycols = indexorderbycols;
	pathnode->indexscandir = indexscandir;

	/* Distribution is same as the base table. */
	pathnode->path.locus = cdbpathlocus_from_baserel(root, rel);
	pathnode->path.motionHazard = false;
	pathnode->path.rescannable = true;
	pathnode->path.sameslice_relids = rel->relids;

	cost_index(pathnode, root, loop_count);

	return pathnode;
}

/*
 * create_bitmap_heap_path
 *	  Creates a path node for a bitmap scan.
 *
 * 'bitmapqual' is a tree of IndexPath, BitmapAndPath, and BitmapOrPath nodes.
 * 'required_outer' is the set of outer relids for a parameterized path.
 * 'loop_count' is the number of repetitions of the indexscan to factor into
 *		estimates of caching behavior.
 *
 * loop_count should match the value used when creating the component
 * IndexPaths.
 */
BitmapHeapPath *
create_bitmap_heap_path(PlannerInfo *root,
						RelOptInfo *rel,
						Path *bitmapqual,
						Relids required_outer,
						double loop_count)
{
	BitmapHeapPath *pathnode = makeNode(BitmapHeapPath);

	pathnode->path.pathtype = T_BitmapHeapScan;
	pathnode->path.parent = rel;
	pathnode->path.param_info = get_baserel_parampathinfo(root, rel,
														  required_outer);
	pathnode->path.pathkeys = NIL;		/* always unordered */

	/* Distribution is same as the base table. */
	pathnode->path.locus = cdbpathlocus_from_baserel(root, rel);
	pathnode->path.motionHazard = false;
	pathnode->path.rescannable = true;
	pathnode->path.sameslice_relids = rel->relids;

	pathnode->bitmapqual = bitmapqual;

	cost_bitmap_heap_scan(&pathnode->path, root, rel,
						  pathnode->path.param_info,
						  bitmapqual, loop_count);

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
	pathnode->path.param_info = NULL;	/* not used in bitmap trees */
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
	pathnode->path.param_info = NULL;	/* not used in bitmap trees */
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
create_tidscan_path(PlannerInfo *root, RelOptInfo *rel, List *tidquals,
					Relids required_outer)
{
	TidPath    *pathnode = makeNode(TidPath);

	pathnode->path.pathtype = T_TidScan;
	pathnode->path.parent = rel;
	pathnode->path.param_info = get_baserel_parampathinfo(root, rel,
														  required_outer);
	pathnode->path.pathkeys = NIL;		/* always unordered */

	pathnode->tidquals = tidquals;

	/* Distribution is same as the base table. */
	pathnode->path.locus = cdbpathlocus_from_baserel(root, rel);
	pathnode->path.motionHazard = false;
	pathnode->path.rescannable = true;
	pathnode->path.sameslice_relids = rel->relids;

	cost_tidscan(&pathnode->path, root, rel, tidquals,
				 pathnode->path.param_info);

	return pathnode;
}

/*
 * create_append_path
 *	  Creates a path corresponding to an Append plan, returning the
 *	  pathnode.
 *
 * Note that we must handle subpaths = NIL, representing a dummy access path.
 */
AppendPath *
create_append_path(PlannerInfo *root, RelOptInfo *rel, List *subpaths, Relids required_outer)
{
	AppendPath *pathnode = makeNode(AppendPath);
	ListCell   *l;

	pathnode->path.pathtype = T_Append;
	pathnode->path.parent = rel;
	pathnode->path.param_info = get_appendrel_parampathinfo(rel,
															required_outer);
	pathnode->path.pathkeys = NIL;		/* result is always considered
										 * unsorted */
	pathnode->subpaths = subpaths;

	pathnode->path.motionHazard = false;
	pathnode->path.rescannable = true;

	/*
	 * We don't bother with inventing a cost_append(), but just do it here.
	 *
	 * Compute rows and costs as sums of subplan rows and costs.  We charge
	 * nothing extra for the Append itself, which perhaps is too optimistic,
	 * but since it doesn't do any selection or projection, it is a pretty
	 * cheap node.  If you change this, see also make_append().
	 */
	pathnode->path.rows = 0;
	pathnode->path.startup_cost = 0;
	pathnode->path.total_cost = 0;

	set_append_path_locus(root, (Path *) pathnode, rel, NIL);

	foreach(l, subpaths)
	{
		Path	   *subpath = (Path *) lfirst(l);

		pathnode->path.rows += subpath->rows;

		if (l == list_head(subpaths))	/* first node? */
			pathnode->path.startup_cost = subpath->startup_cost;
		pathnode->path.total_cost += subpath->total_cost;

		/* All child paths must have same parameterization */
		Assert(bms_equal(PATH_REQ_OUTER(subpath), required_outer));
	}

	/*
	 * CDB: If there is exactly one subpath, its ordering is preserved.
	 * Child rel's pathkey exprs are already expressed in terms of the
	 * columns of the parent appendrel.  See find_usable_indexes().
	 */
	if (list_length(subpaths) == 1)
		pathnode->path.pathkeys = ((Path *) linitial(subpaths))->pathkeys;

	return pathnode;
}

/*
 * create_merge_append_path
 *	  Creates a path corresponding to a MergeAppend plan, returning the
 *	  pathnode.
 */
MergeAppendPath *
create_merge_append_path(PlannerInfo *root,
						 RelOptInfo *rel,
						 List *subpaths,
						 List *pathkeys,
						 Relids required_outer)
{
	MergeAppendPath *pathnode = makeNode(MergeAppendPath);
	Cost		input_startup_cost;
	Cost		input_total_cost;
	ListCell   *l;

	pathnode->path.pathtype = T_MergeAppend;
	pathnode->path.parent = rel;
	pathnode->path.param_info = get_appendrel_parampathinfo(rel,
															required_outer);
	pathnode->path.pathkeys = pathkeys;
	pathnode->subpaths = subpaths;

	/*
	 * Apply query-wide LIMIT if known and path is for sole base relation.
	 * (Handling this at this low level is a bit klugy.)
	 */
	if (bms_equal(rel->relids, root->all_baserels))
		pathnode->limit_tuples = root->limit_tuples;
	else
		pathnode->limit_tuples = -1.0;

	/*
	 * Add up the sizes and costs of the input paths.
	 */
	pathnode->path.rows = 0;
	input_startup_cost = 0;
	input_total_cost = 0;
	foreach(l, subpaths)
	{
		Path	   *subpath = (Path *) lfirst(l);

		pathnode->path.rows += subpath->rows;

		if (pathkeys_contained_in(pathkeys, subpath->pathkeys))
		{
			/* Subpath is adequately ordered, we won't need to sort it */
			input_startup_cost += subpath->startup_cost;
			input_total_cost += subpath->total_cost;
		}
		else
		{
			/* We'll need to insert a Sort node, so include cost for that */
			Path		sort_path;		/* dummy for result of cost_sort */

			cost_sort(&sort_path,
					  root,
					  pathkeys,
					  subpath->total_cost,
					  subpath->parent->tuples,
					  subpath->parent->width,
					  0.0,
					  work_mem,
					  pathnode->limit_tuples);
			input_startup_cost += sort_path.startup_cost;
			input_total_cost += sort_path.total_cost;
		}

		/* All child paths must have same parameterization */
		Assert(bms_equal(PATH_REQ_OUTER(subpath), required_outer));
	}

	/* Now we can compute total costs of the MergeAppend */
	cost_merge_append(&pathnode->path, root,
					  pathkeys, list_length(subpaths),
					  input_startup_cost, input_total_cost,
					  rel->tuples);

	set_append_path_locus(root, (Path *) pathnode, rel, pathkeys);

	return pathnode;
}

/*
 * Set the locus of an Append or MergeAppend path.
 *
 * This modifies the 'subpaths', costs fields, and locus of 'pathnode'.
 */
static void
set_append_path_locus(PlannerInfo *root, Path *pathnode, RelOptInfo *rel,
					  List *pathkeys)
{
	ListCell   *l;
	CdbLocusType targetlocustype;
	CdbPathLocus targetlocus;
	int			 numsegments;
	List	   *subpaths;
	List	  **subpaths_out;
	List	   *new_subpaths;

	/*
	 * Init max_numsegments to slient compiler.
	 * This variable is only used when result
	 * locus is partitioned.
	 */
	int			max_numsegments = -1;

	if (IsA(pathnode, AppendPath))
		subpaths_out = &((AppendPath *) pathnode)->subpaths;
	else if (IsA(pathnode, MergeAppendPath))
		subpaths_out = &((MergeAppendPath *) pathnode)->subpaths;
	else
		elog(ERROR, "unexpected append path type: %d", nodeTag(pathnode));
	subpaths = *subpaths_out;
	*subpaths_out = NIL;

	/* If no subpath, any worker can execute this Append.  Result has 0 rows. */
	if (!subpaths)
	{
		CdbPathLocus_MakeGeneral(&pathnode->locus,
								 getgpsegmentCount());
		return;
	}

	/*
	 * Do a first pass over the children to determine what locus the result
	 * should have, based on the loci of the children.
	 *
	 * We only determine the target locus type here, the number of segments is
	 * figured out later. We treat also all partitioned types the same for now,
	 * using Strewn to represent them all, and figure out later if we can mark
	 * it hashed, or if have to leave it strewn.
	 *
	 * We will record the max number of segments of each subpath here for later
	 * use.
	 */
	static const struct
	{
		CdbLocusType a;
		CdbLocusType b;
		CdbLocusType result;
	} append_locus_compatibility_table[] =
	{
		/*
		 * If any of the children have 'entry' locus, bring all the subpaths
		 * to the entry db.
		 */
		{ CdbLocusType_Entry, CdbLocusType_Entry,          CdbLocusType_Entry },
		{ CdbLocusType_Entry, CdbLocusType_SingleQE,       CdbLocusType_Entry },
		{ CdbLocusType_Entry, CdbLocusType_Strewn,         CdbLocusType_Entry },
		{ CdbLocusType_Entry, CdbLocusType_Replicated,     CdbLocusType_Entry },
		{ CdbLocusType_Entry, CdbLocusType_SegmentGeneral, CdbLocusType_Entry },
		{ CdbLocusType_Entry, CdbLocusType_General,        CdbLocusType_Entry },

		/* similarly, if there are single QE children, bring everything to a single QE */
		{ CdbLocusType_SingleQE, CdbLocusType_SingleQE,       CdbLocusType_SingleQE },
		{ CdbLocusType_SingleQE, CdbLocusType_Strewn,         CdbLocusType_SingleQE },
		{ CdbLocusType_SingleQE, CdbLocusType_Replicated,     CdbLocusType_SingleQE },
		{ CdbLocusType_SingleQE, CdbLocusType_SegmentGeneral, CdbLocusType_SingleQE },
		{ CdbLocusType_SingleQE, CdbLocusType_General,        CdbLocusType_SingleQE },

		/*
		 * If everything is partitioned, then the result can be partitioned, too.
		 * But if it's a mix of partitioned and replicated, then we have to bring
		 * everything to a single QE. Otherwise, the replicated (or general) children
		 * will contribute rows on every QE. XXX: it would be nice to force the child
		 * to be executed on a single QE, but I couldn't figure out how to do that.
		 * A motion from General to SingleQE is not possible.
		 */
		{ CdbLocusType_Strewn, CdbLocusType_Strewn,         CdbLocusType_Strewn },
		{ CdbLocusType_Strewn, CdbLocusType_Replicated,     CdbLocusType_SingleQE },
		{ CdbLocusType_Strewn, CdbLocusType_SegmentGeneral, CdbLocusType_SingleQE },
		{ CdbLocusType_Strewn, CdbLocusType_General,        CdbLocusType_SingleQE },

		{ CdbLocusType_Replicated, CdbLocusType_Replicated, CdbLocusType_Replicated },
		{ CdbLocusType_Replicated, CdbLocusType_SegmentGeneral, CdbLocusType_Replicated },
		{ CdbLocusType_Replicated, CdbLocusType_General,        CdbLocusType_Replicated },

		{ CdbLocusType_SegmentGeneral, CdbLocusType_SegmentGeneral, CdbLocusType_SegmentGeneral },
		{ CdbLocusType_SegmentGeneral, CdbLocusType_General,        CdbLocusType_SegmentGeneral },

		{ CdbLocusType_General, CdbLocusType_General,        CdbLocusType_General },
	};
	targetlocustype = CdbLocusType_General;
	foreach(l, subpaths)
	{
		Path	   *subpath = (Path *) lfirst(l);
		CdbLocusType subtype;
		int			i;

		if (CdbPathLocus_IsPartitioned(subpath->locus))
			subtype = CdbLocusType_Strewn;
		else
			subtype = subpath->locus.locustype;

		if (l == list_head(subpaths))
		{
			targetlocustype = subtype;
			max_numsegments = CdbPathLocus_NumSegments(subpath->locus);
			continue;
		}

		max_numsegments = Max(max_numsegments,
							  CdbPathLocus_NumSegments(subpath->locus));

		for (i = 0; i < lengthof(append_locus_compatibility_table); i++)
		{
			if ((append_locus_compatibility_table[i].a == targetlocustype &&
				 append_locus_compatibility_table[i].b == subtype) ||
				(append_locus_compatibility_table[i].a == subtype &&
				 append_locus_compatibility_table[i].b == targetlocustype))
			{
				targetlocustype = append_locus_compatibility_table[i].result;
				break;
			}
		}
		if (i == lengthof(append_locus_compatibility_table))
			elog(ERROR, "could not determine target locus for Append");
	}

	/*
	 * Now compute the 'numsegments', and the hash keys if it's a partitioned
	 * type.
	 */
	if (targetlocustype == CdbLocusType_Entry)
	{
		/* nothing more to do */
		CdbPathLocus_MakeEntry(&targetlocus);
	}
	else if (targetlocustype == CdbLocusType_SingleQE ||
			 targetlocustype == CdbLocusType_Replicated ||
			 targetlocustype == CdbLocusType_SegmentGeneral ||
			 targetlocustype == CdbLocusType_General)
	{
		/* By default put Append node on all the segments */
		numsegments = getgpsegmentCount();
		foreach(l, subpaths)
		{
			Path	   *subpath = (Path *) lfirst(l);

			/*
			 * Align numsegments to be the common segments among the children.
			 * Partitioned children will need to be motioned, so ignore them.
			 */
			if (!CdbPathLocus_IsPartitioned(subpath->locus))
			{
				/* When there are multiple SingleQE, use the common segments */
				numsegments = Min(numsegments,
								  CdbPathLocus_NumSegments(subpath->locus));
			}
		}
		CdbPathLocus_MakeSimple(&targetlocus, targetlocustype, numsegments);
	}
	else if (targetlocustype == CdbLocusType_Strewn)
	{
		bool		isfirst = true;

		/* By default put Append node on all the segments */
		numsegments = getgpsegmentCount();
		CdbPathLocus_MakeNull(&targetlocus, 0);
		foreach(l, subpaths)
		{
			Path	   *subpath = (Path *) lfirst(l);
			CdbPathLocus projectedlocus;

			Assert(CdbPathLocus_IsPartitioned(subpath->locus));

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
			if (isfirst)
			{
				targetlocus = projectedlocus;
				isfirst = false;
			}
			else if (cdbpathlocus_equal(targetlocus, projectedlocus))
			{
				/* compatible */
			}
			else
			{
				/*
				 * subpaths have different distributed policy, mark it as random
				 * distributed and set the numsegments to the maximum of all
				 * subpaths to not missing any tuples.
				 *
				 * max_numsegments is computed in the first deduction loop,
				 * even here we use projectdlocus, the numsegments never change.
				 */
				CdbPathLocus_MakeStrewn(&targetlocus, max_numsegments);
				break;
			}
		}
	}
	else
		elog(ERROR, "unexpected Append target locus type");

	/* Ok, we now know the target locus. Add Motions to any subpaths that need it */
	new_subpaths = NIL;
	foreach(l, subpaths)
	{
		Path	   *subpath = (Path *) lfirst(l);

		if (CdbPathLocus_IsPartitioned(targetlocus))
		{
			/* we already determined that all the loci are compatible */
			Assert(CdbPathLocus_IsPartitioned(subpath->locus));
		}
		else
		{
			subpath = cdbpath_create_motion_path(root, subpath, subpath->pathkeys, false, targetlocus);
			if (!subpath)
				elog(ERROR, "Can't create valid motion for Append node.");
		}

		pathnode->sameslice_relids = bms_union(pathnode->sameslice_relids, subpath->sameslice_relids);

		if (subpath->motionHazard)
			pathnode->motionHazard = true;

		if (!subpath->rescannable)
			pathnode->rescannable = false;

		new_subpaths = lappend(new_subpaths, subpath);
	}
	pathnode->locus = targetlocus;

	*subpaths_out = new_subpaths;
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
	pathnode->path.param_info = NULL;	/* there are no other rels... */
	pathnode->path.pathkeys = NIL;
	pathnode->quals = quals;

	/* Hardly worth defining a cost_result() function ... just do it */
	pathnode->path.rows = 1;
	pathnode->path.startup_cost = 0;
	pathnode->path.total_cost = cpu_tuple_cost;

	/* Result can be on any segments */
	CdbPathLocus_MakeGeneral(&pathnode->path.locus, getgpsegmentCount());
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

	Assert(subpath->parent == rel);

	pathnode->path.pathtype = T_Material;
	pathnode->path.parent = rel;
	pathnode->path.param_info = subpath->param_info;
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
				  subpath->rows,
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
	bool		add_motion = false;

	/* Caller made a mistake if subpath isn't cheapest_total ... */
	Assert(subpath == rel->cheapest_total_path);
	Assert(subpath->parent == rel);
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
		Oid			opinputtype;

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
		opinputtype = exprType(left_expr);

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
			if (!op_mergejoinable(opno, opinputtype) ||
				get_mergejoin_opfamilies(opno) == NIL)
				all_btree = false;
		}
		if (all_hash)
		{
			/* ... but oprcanhash had better be correct */
			if (!op_hashjoinable(opno, opinputtype))
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
		!cdbpathlocus_is_hashed_on_exprs(subpath->locus, uniq_exprs, false))
	{
		int			numsegments = CdbPathLocus_NumSegments(subpath->locus);

		List	   *opfamilies = NIL;
		ListCell   *lc;

		foreach(lc, uniq_exprs)
		{
			Node	   *expr = lfirst(lc);
			Oid			opfamily;

			opfamily = cdb_default_distribution_opfamily_for_type(exprType(expr));
			opfamilies = lappend_oid(opfamilies, opfamily);
		}

		locus = cdbpathlocus_from_exprs(root, uniq_exprs, opfamilies, numsegments);
        subpath = cdbpath_create_motion_path(root, subpath, NIL, false, locus);
		/*
		 * We probably add agg/sort node above the added motion node, but it is
		 * possible to add an agg/sort node below this motion node also,
		 * which might be optimal in some cases?
		 */
		add_motion = true;
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
	pathnode->path.param_info = subpath->param_info;

	/*
	 * Assume the output is unsorted, since we don't necessarily have pathkeys
	 * to represent it.  (This might get overridden below.)
	 */
	pathnode->path.pathkeys = NIL;

	pathnode->subpath = subpath;
	pathnode->in_operators = in_operators;
	pathnode->uniq_exprs = uniq_exprs;

	/*
	 * If the input is a relation and it has a unique index that proves the
	 * uniq_exprs are unique, then we don't need to do anything.  Note that
	 * relation_has_unique_index_for automatically considers restriction
	 * clauses for the rel, as well.
	 */
	if (rel->rtekind == RTE_RELATION && all_btree &&
		relation_has_unique_index_for(root, rel, NIL,
									  uniq_exprs, in_operators))
	{
		/*
		 * For UNIQUE_PATH_NOOP, it is possible that subpath could be a
		 * motion node. It is not allowed to add a motion node above a
		 * motion node so we simply disallow this unique path although
		 * in theory we could improve this.
		 */
		if (add_motion)
			goto no_unique_path;
		pathnode->umethod = UNIQUE_PATH_NOOP;
		pathnode->path.rows = rel->rows;
		pathnode->path.startup_cost = subpath->startup_cost;
		pathnode->path.total_cost = subpath->total_cost;
		pathnode->path.pathkeys = subpath->pathkeys;

		rel->cheapest_unique_path = (Path *) pathnode;

		MemoryContextSwitchTo(oldcontext);

		return pathnode;
	}

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
			/* Subpath node could be a motion. See previous comment for details. */
			if (add_motion)
				goto no_unique_path;
			pathnode->umethod = UNIQUE_PATH_NOOP;
			pathnode->path.rows = rel->rows;
			pathnode->path.startup_cost = subpath->startup_cost;
			pathnode->path.total_cost = subpath->total_cost;
			pathnode->path.pathkeys = subpath->pathkeys;

			rel->cheapest_unique_path = (Path *) pathnode;

			MemoryContextSwitchTo(oldcontext);

			return pathnode;
		}
	}

	/* Estimate number of output rows */
	pathnode->path.rows = estimate_num_groups(root, uniq_exprs, rel->rows);
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
				  0.0,
				  work_mem,
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

		if (hashentrysize * pathnode->path.rows > work_mem * 1024L)
			all_hash = false;	/* don't try to hash */
		else
			cost_agg(&agg_path, root,
					 AGG_HASHED, NULL,
					 numCols, pathnode->path.rows,
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
 *  Optimizer status: Postgres query optimizer
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
                         Relids       distinct_relids,
						 Relids       required_outer)
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

	pathnode->path.param_info = get_baserel_parampathinfo(root, rel,
													 required_outer);
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
	((Path*)pathnode)->rows = rel->rows;

	if (all_btree)
	{
		/*
		 * Estimate cost for sort+unique implementation
		 */
		cost_sort(&sort_path, root, NIL,
				  subpath->total_cost,
				  rel->rows,
				  rel->width,
				  0, work_mem,
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

		if (hashentrysize * ((Path*)pathnode)->rows > work_mem * 1024L)
			all_hash = false;	/* don't try to hash */
		else
			cost_agg(&agg_path, root,
					 AGG_HASHED, 0,
					 numCols, ((Path*)pathnode)->rows,
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
        CdbPathLocus_MakeStrewn(&pathnode->path.locus,
								CdbPathLocus_NumSegments(subpath->locus));
		pathnode->path.sameslice_relids = NULL;

        /* Estimate repartitioning cost. */
        memset(&motionpath, 0, sizeof(motionpath));
        motionpath.path.type = T_CdbMotionPath;
        motionpath.path.parent = subpath->parent;
        motionpath.path.locus = pathnode->path.locus;
        motionpath.path.rows = subpath->rows;
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

#ifdef FAULT_INJECTOR
	if (SIMPLE_FAULT_INJECTOR("low_unique_rowid_path_cost") == FaultInjectorTypeSkip)
	{
		/*
		 * Inject a fault to set a very low cost for unique rowid path so that
		 * planner will choose this if it is in pathlist.
		 */
		pathnode->path.total_cost = 1.0;
	}
#endif

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
 * to be distinct.  "Distinctness" is defined according to whether the
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
 * else return InvalidOid.  (We expect colnos does not contain duplicates,
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

static bool
subquery_motionHazard_walker(Plan *node)
{
	List       *planlist = NIL;

	if (node == NULL)
		return false;

	if (IsA(node, Motion))
		return true;
	else if (IsA(node, SubqueryScan))
		return subquery_motionHazard_walker(((SubqueryScan *) node)->subplan);
	else if (IsA(node, Append))
		planlist = ((Append *) node)->appendplans;
	else if (IsA(node, MergeAppend))
		planlist = ((MergeAppend *) node)->mergeplans;
	else if (IsA(node, BitmapAnd))
		planlist = ((BitmapAnd *) node)->bitmapplans;
	else if (IsA(node, BitmapOr))
		planlist = ((BitmapOr *) node)->bitmapplans;
	else if (IsA(node, Sequence))
		planlist = ((Sequence *) node)->subplans;
	else if (IsA(node, ModifyTable))
		planlist = ((ModifyTable *) node)->plans;

	/* Handle plan lists */
	ListCell   *l;
	foreach(l, planlist)
	{
		if (subquery_motionHazard_walker((Plan *) lfirst(l)))
			return true;
	}

	/* left tree and right tree */
	if (subquery_motionHazard_walker(node->lefttree) ||
		subquery_motionHazard_walker(node->righttree))
		return true;

	return false;
}

/*
 * create_subqueryscan_path
 *	  Creates a path corresponding to a sequential scan of a subquery,
 *	  returning the pathnode.
 */
Path *
create_subqueryscan_path(PlannerInfo *root, RelOptInfo *rel,
						 List *pathkeys, Relids required_outer)
{
	Path	   *pathnode = makeNode(Path);

	pathnode->pathtype = T_SubqueryScan;
	pathnode->parent = rel;
	pathnode->param_info = get_baserel_parampathinfo(root, rel,
													 required_outer);
	pathnode->pathkeys = pathkeys;

	pathnode->locus = cdbpathlocus_from_subquery(root, rel->subplan, rel->relid);
	pathnode->motionHazard = subquery_motionHazard_walker(rel->subplan);
	pathnode->rescannable = false;
	pathnode->sameslice_relids = NULL;

	cost_subqueryscan(pathnode, root, rel, pathnode->param_info);

	return pathnode;
}

/*
 * create_functionscan_path
 *	  Creates a path corresponding to a sequential scan of a function,
 *	  returning the pathnode.
 */
Path *
create_functionscan_path(PlannerInfo *root, RelOptInfo *rel,
						 RangeTblEntry *rte,
						 List *pathkeys, Relids required_outer)
{
	Path	   *pathnode = makeNode(Path);
	ListCell   *lc;

	pathnode->pathtype = T_FunctionScan;
	pathnode->parent = rel;
	pathnode->param_info = get_baserel_parampathinfo(root, rel,
													 required_outer);
	pathnode->pathkeys = pathkeys;

	/*
	 * Decide where to execute the FunctionScan.
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		char		exec_location = PROEXECLOCATION_ANY;
		bool		contain_mutables = false;

		/*
		 * If the function desires to run on segments, mark randomly-distributed.
		 * If expression contains mutable functions, evaluate it on entry db.
		 * Otherwise let it be evaluated in the same slice as its parent operator.
		 */
		Assert(rte->rtekind == RTE_FUNCTION);

		foreach (lc, rte->functions)
		{
			RangeTblFunction *rtfunc = (RangeTblFunction *) lfirst(lc);

			if (rtfunc->funcexpr && IsA(rtfunc->funcexpr, FuncExpr))
			{
				FuncExpr   *funcexpr = (FuncExpr *) rtfunc->funcexpr;
				char		this_exec_location;

				this_exec_location = func_exec_location(funcexpr->funcid);

				switch (this_exec_location)
				{
					case PROEXECLOCATION_ANY:
						/*
						 * This can be executed anywhere. Remember if it was
						 * mutable (or contained any mutable arguments), that
						 * will affect the decision after this loop on where
						 * to actually execute it.
						 */
						if (!contain_mutables)
							contain_mutables = contain_mutable_functions((Node *) funcexpr);
						break;
					case PROEXECLOCATION_MASTER:
						/*
						 * This function forces the execution to master.
						 */
						if (exec_location == PROEXECLOCATION_ALL_SEGMENTS)
						{
							ereport(ERROR,
									(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
									 (errmsg("cannot mix EXECUTE ON MASTER and ALL SEGMENTS functions in same function scan"))));
						}
						exec_location = PROEXECLOCATION_MASTER;
						break;
					case PROEXECLOCATION_INITPLAN:
						/*
						 * This function forces the execution to master.
						 */
						if (exec_location == PROEXECLOCATION_ALL_SEGMENTS)
						{
							ereport(ERROR,
									(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
									 (errmsg("cannot mix EXECUTE ON INITPLAN and ALL SEGMENTS functions in same function scan"))));
						}
						exec_location = PROEXECLOCATION_INITPLAN;
						break;
					case PROEXECLOCATION_ALL_SEGMENTS:
						/*
						 * This function forces the execution to segments.
						 */
						if (exec_location == PROEXECLOCATION_MASTER)
						{
							ereport(ERROR,
									(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
									 (errmsg("cannot mix EXECUTE ON MASTER and ALL SEGMENTS functions in same function scan"))));
						}
						exec_location = PROEXECLOCATION_ALL_SEGMENTS;
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
			}
		}

		switch (exec_location)
		{
			case PROEXECLOCATION_ANY:
				/*
				 * If all the functions are ON ANY, we presumably could execute
				 * the function scan anywhere. However, historically, before the
				 * EXECUTE ON syntax was introduced, we always executed
				 * non-IMMUTABLE functions on the master. Keep that behavior
				 * for backwards compatibility.
				 */
				if (contain_mutables)
					CdbPathLocus_MakeEntry(&pathnode->locus);
				else
					CdbPathLocus_MakeGeneral(&pathnode->locus,
											 getgpsegmentCount());
				break;
			case PROEXECLOCATION_MASTER:
				CdbPathLocus_MakeEntry(&pathnode->locus);
				break;
			case PROEXECLOCATION_INITPLAN:
				CdbPathLocus_MakeEntry(&pathnode->locus);
				break;
			case PROEXECLOCATION_ALL_SEGMENTS:
				CdbPathLocus_MakeStrewn(&pathnode->locus,
										getgpsegmentCount());
				break;
			default:
				elog(ERROR, "unrecognized proexeclocation '%c'", exec_location);
		}
	}
	else
		CdbPathLocus_MakeEntry(&pathnode->locus);

	pathnode->motionHazard = false;

	/*
	 * FunctionScan is always rescannable. It uses a tuplestore to
	 * materialize the results all by itself.
	 */
	pathnode->rescannable = true;

	pathnode->sameslice_relids = NULL;

	cost_functionscan(pathnode, root, rel, pathnode->param_info);

	return pathnode;
}

/*
 * create_tablefunction_path
 *	  Creates a path corresponding to a sequential scan of a table function,
 *	  returning the pathnode.
 */
Path *
create_tablefunction_path(PlannerInfo *root, RelOptInfo *rel,
						  RangeTblEntry *rte,
						  Relids required_outer)
{
	Path	   *pathnode = makeNode(Path);

	Assert(rte->rtekind == RTE_TABLEFUNCTION);

	/*
	 * Greenplum specific behavior
	 *
	 * Greenplum has a special path to handle semjoin, the planner might add a
	 * unique_row_id path to the first inner join and then de-duplicate.
	 *
	 * Table function scan has no corresponding dedup workflow. Here we
	 * introduce a switch to turn off it when there is a table function scan.
	 */
	root->disallow_unique_rowid_path = true;

	/* Setup the basics of the TableFunction path */
	pathnode->pathtype	   = T_TableFunctionScan;
	pathnode->parent	   = rel;
	pathnode->param_info = get_baserel_parampathinfo(root, rel,
													 required_outer);
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
		CdbPathLocus_MakeStrewn(&pathnode->locus,
								CdbPathLocus_NumSegments(pathnode->locus));
	pathnode->sameslice_relids = NULL;

	cost_tablefunction(pathnode, root, rel, pathnode->param_info);

	return pathnode;
}

/*
 * create_valuesscan_path
 *	  Creates a path corresponding to a scan of a VALUES list,
 *	  returning the pathnode.
 */
Path *
create_valuesscan_path(PlannerInfo *root, RelOptInfo *rel,
					   RangeTblEntry *rte,
					   Relids required_outer)
{
	Path	   *pathnode = makeNode(Path);

	pathnode->pathtype = T_ValuesScan;
	pathnode->parent = rel;
	pathnode->param_info = get_baserel_parampathinfo(root, rel,
													 required_outer);
	pathnode->pathkeys = NIL;	/* result is always unordered */

	/*
	 * CDB: If VALUES list contains mutable functions, evaluate it on entry db.
	 * Otherwise let it be evaluated in the same slice as its parent operator.
	 */
	Assert(rte->rtekind == RTE_VALUES);
	if (contain_mutable_functions((Node *)rte->values_lists))
		CdbPathLocus_MakeEntry(&pathnode->locus);
	else
		/*
		 * ValuesScan can be on any segment.
		 */
		CdbPathLocus_MakeGeneral(&pathnode->locus, getgpsegmentCount());

	pathnode->motionHazard = false;
	pathnode->rescannable = true;
	pathnode->sameslice_relids = NULL;

	cost_valuesscan(pathnode, root, rel, pathnode->param_info);

	return pathnode;
}

/*
 * create_ctescan_path
 *	  Creates a path corresponding to a scan of a non-self-reference CTE,
 *	  returning the pathnode.
 */
Path *
create_ctescan_path(PlannerInfo *root, RelOptInfo *rel, List *pathkeys,
					Relids required_outer)
{
	Path	   *pathnode = makeNode(Path);

	pathnode->pathtype = T_CteScan;
	pathnode->parent = rel;
	pathnode->param_info = get_baserel_parampathinfo(root, rel,
													 required_outer);
	pathnode->pathkeys = pathkeys;

	pathnode->locus = cdbpathlocus_from_subquery(root, rel->subplan, rel->relid);

	/*
	 * We can't extract these two values from the subplan, so we simple set
	 * them to their worst case here.
	 */
	pathnode->motionHazard = true;
	pathnode->rescannable = false;
	pathnode->sameslice_relids = NULL;

	cost_ctescan(pathnode, root, rel, pathnode->param_info);

	return pathnode;
}

/*
 * create_worktablescan_path
 *	  Creates a path corresponding to a scan of a self-reference CTE,
 *	  returning the pathnode.
 */
Path *
create_worktablescan_path(PlannerInfo *root, RelOptInfo *rel,
						  CdbLocusType ctelocus,
						  Relids required_outer)
{
	Path	   *pathnode = makeNode(Path);
	CdbPathLocus result;
	int			numsegments;

	if (rel->cdbpolicy)
		numsegments = rel->cdbpolicy->numsegments;
	else
		numsegments = getgpsegmentCount(); /* FIXME */

	if (ctelocus == CdbLocusType_Entry)
		CdbPathLocus_MakeEntry(&result);
	else if (ctelocus == CdbLocusType_SingleQE)
		CdbPathLocus_MakeSingleQE(&result, numsegments);
	else if (ctelocus == CdbLocusType_General)
		CdbPathLocus_MakeGeneral(&result, numsegments);
	else if (ctelocus == CdbLocusType_SegmentGeneral)
	{
		/* See comments in set_worktable_pathlist */
		elog(ERROR,
			 "worktable scan path can never have "
			 "segmentgeneral locus.");
	}
	else
		CdbPathLocus_MakeStrewn(&result, numsegments);

	pathnode->pathtype = T_WorkTableScan;
	pathnode->parent = rel;
	pathnode->param_info = get_baserel_parampathinfo(root, rel,
													 required_outer);
	pathnode->pathkeys = NIL;	/* result is always unordered */

	pathnode->locus = result;
	pathnode->motionHazard = false;
	pathnode->rescannable = true;
	pathnode->sameslice_relids = rel->relids;

	/* Cost is the same as for a regular CTE scan */
	cost_ctescan(pathnode, root, rel, pathnode->param_info);

	return pathnode;
}

/*
 * GPDB_92_MERGE_FIXME:Please check why isjoininner is removed.
 */

bool
path_contains_inner_index(Path *path)
{

	if (IsA(path, IndexPath))
		return true;
	else if (IsA(path, BitmapHeapPath))
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
			Path	   *subpath = (Path *)lfirst(l);

			if (path_contains_inner_index(subpath))
				return true;
		}
	}

	return false;
}

/*
 * create_foreignscan_path
 *	  Creates a path corresponding to a scan of a foreign table,
 *	  returning the pathnode.
 *
 * This function is never called from core Postgres; rather, it's expected
 * to be called by the GetForeignPaths function of a foreign data wrapper.
 * We make the FDW supply all fields of the path, since we do not have any
 * way to calculate them in core.
 */
ForeignPath *
create_foreignscan_path(PlannerInfo *root, RelOptInfo *rel,
						double rows, Cost startup_cost, Cost total_cost,
						List *pathkeys,
						Relids required_outer,
						List *fdw_private)
{
	ForeignPath *pathnode = makeNode(ForeignPath);

	/*
	 * Since the path's required_outer should always include all the rel's
	 * lateral_relids, forcibly add those if necessary.  This is a bit of a
	 * hack, but up till early 2019 the contrib FDWs failed to ensure that,
	 * and it's likely that the same error has propagated into many external
	 * FDWs.  Don't risk modifying the passed-in relid set here.
	 */
	if (rel->lateral_relids && !bms_is_subset(rel->lateral_relids,
											  required_outer))
		required_outer = bms_union(required_outer, rel->lateral_relids);

	/*
	 * Although this function is only designed to be used for scans of
	 * baserels, before v12 postgres_fdw abused it to make paths for join and
	 * upper rels.  It will work for such cases as long as required_outer is
	 * empty (otherwise get_baserel_parampathinfo does the wrong thing), which
	 * fortunately is the expected case for now.
	 */
	if (!bms_is_empty(required_outer) &&
		!(rel->reloptkind == RELOPT_BASEREL ||
		  rel->reloptkind == RELOPT_OTHER_MEMBER_REL))
		elog(ERROR, "parameterized foreign joins are not supported yet");

	pathnode->path.pathtype = T_ForeignScan;
	pathnode->path.parent = rel;
	pathnode->path.param_info = get_baserel_parampathinfo(root, rel,
														  required_outer);
	pathnode->path.rows = rows;
	pathnode->path.startup_cost = startup_cost;
	pathnode->path.total_cost = total_cost;
	pathnode->path.pathkeys = pathkeys;

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		switch (rel->ftEntry->exec_location)
		{
		case FTEXECLOCATION_ANY:
			CdbPathLocus_MakeGeneral(&(pathnode->path.locus), getgpsegmentCount());
			break;
		case FTEXECLOCATION_ALL_SEGMENTS:
			if (rel->ftEntry)
			{
				ForeignServer *server = GetForeignServer(rel->ftEntry->serverid);
				CdbPathLocus_MakeStrewn(&(pathnode->path.locus), server->num_segments);
			}
			else
				CdbPathLocus_MakeStrewn(&(pathnode->path.locus), getgpsegmentCount());
			break;
		case FTEXECLOCATION_MASTER:
			CdbPathLocus_MakeEntry(&(pathnode->path.locus));
			break;
		default:
			elog(ERROR, "unrecognized exec_location '%c'", rel->ftEntry->exec_location);
		}
	}
	else
	{
		/* make entry locus for utility role */
		CdbPathLocus_MakeEntry(&(pathnode->path.locus));
	}

	pathnode->fdw_private = fdw_private;

	return pathnode;
}

/*
 * calc_nestloop_required_outer
 *	  Compute the required_outer set for a nestloop join path
 *
 * Note: result must not share storage with either input
 */
Relids
calc_nestloop_required_outer(Path *outer_path, Path *inner_path)
{
	Relids		outer_paramrels = PATH_REQ_OUTER(outer_path);
	Relids		inner_paramrels = PATH_REQ_OUTER(inner_path);
	Relids		required_outer;

	/* inner_path can require rels from outer path, but not vice versa */
	Assert(!bms_overlap(outer_paramrels, inner_path->parent->relids));
	/* easy case if inner path is not parameterized */
	if (!inner_paramrels)
		return bms_copy(outer_paramrels);
	/* else, form the union ... */
	required_outer = bms_union(outer_paramrels, inner_paramrels);
	/* ... and remove any mention of now-satisfied outer rels */
	required_outer = bms_del_members(required_outer,
									 outer_path->parent->relids);
	/* maintain invariant that required_outer is exactly NULL if empty */
	if (bms_is_empty(required_outer))
	{
		bms_free(required_outer);
		required_outer = NULL;
	}
	return required_outer;
}

/*
 * calc_non_nestloop_required_outer
 *	  Compute the required_outer set for a merge or hash join path
 *
 * Note: result must not share storage with either input
 */
Relids
calc_non_nestloop_required_outer(Path *outer_path, Path *inner_path)
{
	Relids		outer_paramrels = PATH_REQ_OUTER(outer_path);
	Relids		inner_paramrels = PATH_REQ_OUTER(inner_path);
	Relids		required_outer;

	/* neither path can require rels from the other */
	Assert(!bms_overlap(outer_paramrels, inner_path->parent->relids));
	Assert(!bms_overlap(inner_paramrels, outer_path->parent->relids));
	/* form the union ... */
	required_outer = bms_union(outer_paramrels, inner_paramrels);
	/* we do not need an explicit test for empty; bms_union gets it right */
	return required_outer;
}

/*
 * create_nestloop_path
 *	  Creates a pathnode corresponding to a nestloop join between two
 *	  relations.
 *
 * 'joinrel' is the join relation.
 * 'jointype' is the type of join required
 * 'workspace' is the result from initial_cost_nestloop
 * 'sjinfo' is extra info about the join for selectivity estimation
 * 'semifactors' contains valid data if jointype is SEMI or ANTI
 * 'outer_path' is the outer path
 * 'inner_path' is the inner path
 * 'restrict_clauses' are the RestrictInfo nodes to apply at the join
 * 'pathkeys' are the path keys of the new join path
 * 'required_outer' is the set of required outer rels
 *
 * Returns the resulting path node.
 */
Path *
create_nestloop_path(PlannerInfo *root,
					 RelOptInfo *joinrel,
					 JoinType jointype,
					 JoinCostWorkspace *workspace,
					 SpecialJoinInfo *sjinfo,
					 SemiAntiJoinFactors *semifactors,
					 Path *outer_path,
					 Path *inner_path,
					 List *restrict_clauses,
					 List *redistribution_clauses,    /*CDB*/
					 List *pathkeys,
					 Relids required_outer)
{
	NestPath   *pathnode;
	CdbPathLocus join_locus;
	Relids		outer_req_outer = PATH_REQ_OUTER(outer_path);
	bool		outer_must_be_local = !bms_is_empty(outer_req_outer);
	Relids		inner_req_outer = PATH_REQ_OUTER(inner_path);
	bool		inner_must_be_local = !bms_is_empty(inner_req_outer);

	/* Add motion nodes above subpaths and decide where to join. */
	join_locus = cdbpath_motion_for_join(root,
										 jointype,
										 &outer_path,       /* INOUT */
										 &inner_path,       /* INOUT */
										 redistribution_clauses,
										 pathkeys,
										 NIL,
										 outer_must_be_local,
										 inner_must_be_local);
	if (CdbPathLocus_IsNull(join_locus))
		return NULL;

	/* Outer might not be ordered anymore after motion. */
	if (!outer_path->pathkeys)
		pathkeys = NIL;

	/*
	 * If this join path is parameterized by a parameter above this path, then
	 * this path needs to be rescannable. A NestLoop is rescannable, when both
	 * outer and inner paths rescannable, so make them both rescannable.
	 */
	if (!outer_path->rescannable && !bms_is_empty(required_outer))
	{
		MaterialPath *matouter = create_material_path(root, outer_path->parent, outer_path);

		matouter->cdb_shield_child_from_rescans = true;

		outer_path = (Path *) matouter;
	}

	/*
	 * If outer has at most one row, NJ will make at most one pass over inner.
	 * Else materialize inner rel after motion so NJ can loop over results.
	 */
	if (!inner_path->rescannable &&
		(!outer_path->parent->onerow || !bms_is_empty(required_outer)))
	{
		/*
		 * NLs potentially rescan the inner; if our inner path
		 * isn't rescannable we have to add a materialize node
		 */
		MaterialPath *matinner = create_material_path(root, inner_path->parent, inner_path);

		matinner->cdb_shield_child_from_rescans = true;

		/*
		 * If we have motion on the outer, to avoid a deadlock; we
		 * need to set cdb_strict. In order for materialize to
		 * fully fetch the underlying (required to avoid our
		 * deadlock hazard) we must set cdb_strict!
		 */
		if (inner_path->motionHazard && outer_path->motionHazard)
		{
			matinner->cdb_strict = true;
			matinner->path.motionHazard = false;
		}

		inner_path = (Path *) matinner;
	}

	/*
	 * If the inner path is parameterized by the outer, we must drop any
	 * restrict_clauses that are due to be moved into the inner path.  We have
	 * to do this now, rather than postpone the work till createplan time,
	 * because the restrict_clauses list can affect the size and cost
	 * estimates for this path.
	 */
	if (bms_overlap(inner_req_outer, outer_path->parent->relids))
	{
		Relids		inner_and_outer = bms_union(inner_path->parent->relids,
												inner_req_outer);
		List	   *jclauses = NIL;
		ListCell   *lc;

		foreach(lc, restrict_clauses)
		{
			RestrictInfo *rinfo = (RestrictInfo *) lfirst(lc);

			if (!join_clause_is_movable_into(rinfo,
											 inner_path->parent->relids,
											 inner_and_outer))
				jclauses = lappend(jclauses, rinfo);
		}
		restrict_clauses = jclauses;
	}


	pathnode = makeNode(NestPath);
	pathnode->path.pathtype = T_NestLoop;
	pathnode->path.parent = joinrel;
	pathnode->path.param_info =
		get_joinrel_parampathinfo(root,
								  joinrel,
								  outer_path,
								  inner_path,
								  sjinfo,
								  required_outer,
								  &restrict_clauses);
	pathnode->path.pathkeys = pathkeys;
	pathnode->jointype = jointype;
	pathnode->outerjoinpath = outer_path;
	pathnode->innerjoinpath = inner_path;
	pathnode->joinrestrictinfo = restrict_clauses;

	pathnode->path.locus = join_locus;
	pathnode->path.motionHazard = outer_path->motionHazard || inner_path->motionHazard;

	/* we're only as rescannable as our child plans */
	pathnode->path.rescannable = outer_path->rescannable && inner_path->rescannable;

	pathnode->path.sameslice_relids = bms_union(inner_path->sameslice_relids, outer_path->sameslice_relids);

	/*
	 * inner_path & outer_path are possibly modified above. Let's recalculate
	 * the initial cost.
	 */
	initial_cost_nestloop(root, workspace, jointype,
						  outer_path, inner_path,
						  sjinfo, semifactors);

	final_cost_nestloop(root, pathnode, workspace, sjinfo, semifactors);

	/*
	 * Greenplum specific behavior:
	 * If we find the join locus is general or segmentgeneral,
	 * we should check the joinqual, if it contains volatile functions
	 * we have to turn the join path to singleQE.
	 *
	 * NB: we do not add this logic in the above create_unique_rowid_path
	 * code block, the reason is:
	 *   create_unique_rowid_path is a technique to implement semi join
	 *   using normal join, it can only happens for sublink query:
	 *   1. if the sublink query contains volatile target list or havingQual
	 *      it cannot be pulled up in pull_up_subquery, so it will be a
	 *      subselect and be handled in the function set_subquery_pathlist
	 *   2. if the sublink query contains volatile functions in joinqual
	 *      or where clause, it will be handled in set_rel_pathlist and
	 *      here.
	 */
	return turn_volatile_seggen_to_singleqe(root,
											(Path *) pathnode,
											(Node *) (pathnode->joinrestrictinfo));	
}

/*
 * create_mergejoin_path
 *	  Creates a pathnode corresponding to a mergejoin join between
 *	  two relations
 *
 * 'joinrel' is the join relation
 * 'jointype' is the type of join required
 * 'workspace' is the result from initial_cost_mergejoin
 * 'sjinfo' is extra info about the join for selectivity estimation
 * 'outer_path' is the outer path
 * 'inner_path' is the inner path
 * 'restrict_clauses' are the RestrictInfo nodes to apply at the join
 * 'pathkeys' are the path keys of the new join path
 * 'required_outer' is the set of required outer rels
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
Path *
create_mergejoin_path(PlannerInfo *root,
					  RelOptInfo *joinrel,
					  JoinType jointype,
					  JoinCostWorkspace *workspace,
					  SpecialJoinInfo *sjinfo,
					  Path *outer_path,
					  Path *inner_path,
					  List *restrict_clauses,
					  List *pathkeys,
					  Relids required_outer,
					  List *mergeclauses,
					  List *redistribution_clauses,    /*CDB*/
					  List *outersortkeys,
					  List *innersortkeys)
{
	MergePath  *pathnode = makeNode(MergePath);
	CdbPathLocus join_locus;
	List	   *outermotionkeys;
	List	   *innermotionkeys;
	bool		preserve_outer_ordering;
	bool		preserve_inner_ordering;

	/*
	 * GPDB_92_MERGE_FIXME: Should we keep the pathkeys_contained_in calls?
	 */
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
	 *
	 * If we're explicitly sorting one or both sides of the join, don't choose
	 * a Motion that would break that ordering again. But as a special case,
	 * if there are no merge clauses, then there is no join order that would need
	 * preserving. That case can occur with a query like "a FULL JOIN b ON true"
	 */
	if (mergeclauses)
	{
		preserve_outer_ordering = (outersortkeys == NIL);
		preserve_inner_ordering = (innersortkeys == NIL);
	}
	else
		preserve_outer_ordering = preserve_inner_ordering = false;

	preserve_outer_ordering = preserve_outer_ordering || !bms_is_empty(PATH_REQ_OUTER(outer_path));
	preserve_inner_ordering = preserve_inner_ordering || !bms_is_empty(PATH_REQ_OUTER(inner_path));

	join_locus = cdbpath_motion_for_join(root,
										 jointype,
										 &outer_path,       /* INOUT */
										 &inner_path,       /* INOUT */
										 redistribution_clauses,
										 outermotionkeys,
										 innermotionkeys,
										 preserve_outer_ordering,
										 preserve_inner_ordering);
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

	pathnode->jpath.path.pathtype = T_MergeJoin;
	pathnode->jpath.path.parent = joinrel;
	pathnode->jpath.path.param_info =
		get_joinrel_parampathinfo(root,
								  joinrel,
								  outer_path,
								  inner_path,
								  sjinfo,
								  required_outer,
								  &restrict_clauses);
	pathnode->jpath.path.pathkeys = pathkeys;

	pathnode->jpath.path.locus = join_locus;

	pathnode->jpath.path.motionHazard = outer_path->motionHazard || inner_path->motionHazard;
	pathnode->jpath.path.rescannable = outer_path->rescannable && inner_path->rescannable;
	pathnode->jpath.path.sameslice_relids = bms_union(inner_path->sameslice_relids, outer_path->sameslice_relids);

	pathnode->jpath.jointype = jointype;
	pathnode->jpath.outerjoinpath = outer_path;
	pathnode->jpath.innerjoinpath = inner_path;
	pathnode->jpath.joinrestrictinfo = restrict_clauses;
	pathnode->path_mergeclauses = mergeclauses;
	pathnode->outersortkeys = outersortkeys;
	pathnode->innersortkeys = innersortkeys;
	/* pathnode->materialize_inner will be set by final_cost_mergejoin */

	/*
	 * inner_path & outer_path are possibly modified above. Let's recalculate
	 * the initial cost.
	 */
	initial_cost_mergejoin(root, workspace, jointype, mergeclauses,
						   outer_path, inner_path,
						   outersortkeys, innersortkeys,
						   sjinfo);

	final_cost_mergejoin(root, pathnode, workspace, sjinfo);

	/*
	 * See the comments at the end of create_nestloop_path.
	 */
	return turn_volatile_seggen_to_singleqe(root,
											(Path *) pathnode,
											(Node *) (pathnode->jpath.joinrestrictinfo));	
}

/*
 * create_hashjoin_path
 *	  Creates a pathnode corresponding to a hash join between two relations.
 *
 * 'joinrel' is the join relation
 * 'jointype' is the type of join required
 * 'workspace' is the result from initial_cost_hashjoin
 * 'sjinfo' is extra info about the join for selectivity estimation
 * 'semifactors' contains valid data if jointype is SEMI or ANTI
 * 'outer_path' is the cheapest outer path
 * 'inner_path' is the cheapest inner path
 * 'restrict_clauses' are the RestrictInfo nodes to apply at the join
 * 'required_outer' is the set of required outer rels
 * 'hashclauses' are the RestrictInfo nodes to use as hash clauses
 *		(this should be a subset of the restrict_clauses list)
 */
Path *
create_hashjoin_path(PlannerInfo *root,
					 RelOptInfo *joinrel,
					 JoinType jointype,
					 JoinCostWorkspace *workspace,
					 SpecialJoinInfo *sjinfo,
					 SemiAntiJoinFactors *semifactors,
					 Path *outer_path,
					 Path *inner_path,
					 List *restrict_clauses,
					 Relids required_outer,
					 List *redistribution_clauses,    /*CDB*/
					 List *hashclauses)
{
	HashPath   *pathnode;
	CdbPathLocus join_locus;
	bool		outer_must_be_local = !bms_is_empty(PATH_REQ_OUTER(outer_path));
	bool		inner_must_be_local = !bms_is_empty(PATH_REQ_OUTER(inner_path));

	/* Add motion nodes above subpaths and decide where to join. */
	join_locus = cdbpath_motion_for_join(root,
										 jointype,
										 &outer_path,       /* INOUT */
										 &inner_path,       /* INOUT */
										 redistribution_clauses,
										 NIL,   /* don't care about ordering */
										 NIL,
										 outer_must_be_local,
										 inner_must_be_local);
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
		root->config->gp_enable_hashjoin_size_heuristic)
	{
		double		outersize;
		double		innersize;

		outersize = ExecHashRowSize(outer_path->parent->width) *
			outer_path->rows;
		innersize = ExecHashRowSize(inner_path->parent->width) *
			inner_path->rows;

		if (innersize > outersize)
			return NULL;
	}

	pathnode = makeNode(HashPath);

	pathnode->jpath.path.pathtype = T_HashJoin;
	pathnode->jpath.path.parent = joinrel;
	pathnode->jpath.path.param_info =
		get_joinrel_parampathinfo(root,
								  joinrel,
								  outer_path,
								  inner_path,
								  sjinfo,
								  required_outer,
								  &restrict_clauses);

	/*
	 * A hashjoin never has pathkeys, since its output ordering is
	 * unpredictable due to possible batching.  XXX If the inner relation is
	 * small enough, we could instruct the executor that it must not batch,
	 * and then we could assume that the output inherits the outer relation's
	 * ordering, which might save a sort step.  However there is considerable
	 * downside if our estimate of the inner relation size is badly off. For
	 * the moment we don't risk it.  (Note also that if we wanted to take this
	 * seriously, joinpath.c would have to consider many more paths for the
	 * outer rel than it does now.)
	 */
	pathnode->jpath.path.pathkeys = NIL;
	pathnode->jpath.path.locus = join_locus;

	pathnode->jpath.jointype = jointype;
	pathnode->jpath.outerjoinpath = outer_path;
	pathnode->jpath.innerjoinpath = inner_path;
	pathnode->jpath.joinrestrictinfo = restrict_clauses;
	pathnode->path_hashclauses = hashclauses;
	/* final_cost_hashjoin will fill in pathnode->num_batches */

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

	/*
	 * inner_path & outer_path are possibly modified above. Let's recalculate
	 * the initial cost.
	 */
	initial_cost_hashjoin(root, workspace, jointype, hashclauses,
						  outer_path, inner_path,
						  sjinfo, semifactors);

	final_cost_hashjoin(root, pathnode, workspace, sjinfo, semifactors);

	/*
	 * See the comments at the end of create_nestloop_path.
	 */
	return turn_volatile_seggen_to_singleqe(root,
											(Path *) pathnode,
											(Node *) (pathnode->jpath.joinrestrictinfo));	
}

/*
 * reparameterize_path
 *		Attempt to modify a Path to have greater parameterization
 *
 * We use this to attempt to bring all child paths of an appendrel to the
 * same parameterization level, ensuring that they all enforce the same set
 * of join quals (and thus that that parameterization can be attributed to
 * an append path built from such paths).  Currently, only a few path types
 * are supported here, though more could be added at need.  We return NULL
 * if we can't reparameterize the given path.
 *
 * Note: we intentionally do not pass created paths to add_path(); it would
 * possibly try to delete them on the grounds of being cost-inferior to the
 * paths they were made from, and we don't want that.  Paths made here are
 * not necessarily of general-purpose usefulness, but they can be useful
 * as members of an append path.
 */
Path *
reparameterize_path(PlannerInfo *root, Path *path,
					Relids required_outer,
					double loop_count)
{
	RelOptInfo *rel = path->parent;

	/* Can only increase, not decrease, path's parameterization */
	if (!bms_is_subset(PATH_REQ_OUTER(path), required_outer))
		return NULL;
	switch (path->pathtype)
	{
		case T_SeqScan:
			return create_seqscan_path(root, rel, required_outer);
		case T_IndexScan:
		case T_IndexOnlyScan:
			{
				IndexPath  *ipath = (IndexPath *) path;
				IndexPath  *newpath = makeNode(IndexPath);

				/*
				 * We can't use create_index_path directly, and would not want
				 * to because it would re-compute the indexqual conditions
				 * which is wasted effort.  Instead we hack things a bit:
				 * flat-copy the path node, revise its param_info, and redo
				 * the cost estimate.
				 */
				memcpy(newpath, ipath, sizeof(IndexPath));
				newpath->path.param_info =
					get_baserel_parampathinfo(root, rel, required_outer);
				cost_index(newpath, root, loop_count);
				return (Path *) newpath;
			}
		case T_BitmapHeapScan:
			{
				BitmapHeapPath *bpath = (BitmapHeapPath *) path;

				return (Path *) create_bitmap_heap_path(root,
														rel,
														bpath->bitmapqual,
														required_outer,
														loop_count);
			}
		case T_SubqueryScan:
			return create_subqueryscan_path(root, rel, path->pathkeys,
											required_outer);
		case T_Append:
			{
				AppendPath *apath = (AppendPath *) path;
				List	   *childpaths = NIL;
				ListCell   *lc;

				/* Reparameterize the children */
				foreach(lc, apath->subpaths)
				{
					Path	   *spath = (Path *) lfirst(lc);

					spath = reparameterize_path(root, spath,
												required_outer,
												loop_count);
					if (spath == NULL)
						return NULL;
					childpaths = lappend(childpaths, spath);
				}
				return (Path *)
					create_append_path(root, rel, childpaths,
									   required_outer);
			}
		default:
			break;
	}
	return NULL;
}

/*
 * create_projection_path_with_quals
 *	  Creates a pathnode that represents performing a projection and filter.
 *
 * 'rel' is the parent relation associated with the result
 * 'subpath' is the path representing the source of data
 */
ProjectionPath *
create_projection_path_with_quals(PlannerInfo *root,
								  RelOptInfo *rel,
								  Path *subpath,
								  List *restrict_clauses)
{
	ProjectionPath *pathnode = makeNode(ProjectionPath);

	pathnode->path.pathtype = T_Result;
	pathnode->path.parent = rel;
	/* For now, assume we are above any joins, so no parameterization */
	pathnode->path.param_info = NULL;
	/* Projection does not change the sort order */
	pathnode->path.pathkeys = subpath->pathkeys;

	pathnode->subpath = subpath;

	/*
	 * We might not need a separate Result node.  If the input plan node type
	 * can project, we can just tell it to project something else.  Or, if it
	 * can't project but the desired target has the same expression list as
	 * what the input will produce anyway, we can still give it the desired
	 * tlist (possibly changing its ressortgroupref labels, but nothing else).
	 * Note: in the latter case, create_projection_plan has to recheck our
	 * conclusion; see comments therein.
	 *
	 * GPDB: The 'restrict_clauses' is a GPDB addition. If the subpath supports
	 * Filters, we could push them down too. But currently this is only used on
	 * top of Material paths, which don't support it, so it doesn't matter.
	 *
	 * GPDB_96_MERGE_FIXME: Until 9.6, this isn't used in any situation where
	 * we wouldn't need a Result. And we don't have is_projection_capable_path()
	 * yet.
	 */
#if 0
	if (!restrict_clauses &&
		(is_projection_capable_path(subpath) ||
		 equal(oldtarget->exprs, target->exprs)))
	{
		/* No separate Result node needed */
		pathnode->dummypp = true;

		/*
		 * Set cost of plan as subpath's cost, adjusted for tlist replacement.
		 */
		pathnode->path.rows = subpath->rows;
		pathnode->path.startup_cost = subpath->startup_cost +
			(target->cost.startup - oldtarget->cost.startup);
		pathnode->path.total_cost = subpath->total_cost +
			(target->cost.startup - oldtarget->cost.startup) +
			(target->cost.per_tuple - oldtarget->cost.per_tuple) * subpath->rows;
	}
	else
#endif
	{
		/* We really do need the Result node */
		pathnode->dummypp = false;

		pathnode->path.rows = subpath->rows;
		pathnode->path.startup_cost = subpath->startup_cost;
		pathnode->path.total_cost = subpath->total_cost;

		pathnode->cdb_restrict_clauses = restrict_clauses;
	}

	pathnode->path.locus = subpath->locus;

	return pathnode;
}
