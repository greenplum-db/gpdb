/*-------------------------------------------------------------------------
 *
 * cdbpathlocus.c
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbpathlocus.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/gp_policy.h"	/* GpPolicy */
#include "cdb/cdbdef.h"			/* CdbSwap() */
#include "cdb/cdbpullup.h"		/* cdbpullup_missing_var_walker() */
#include "nodes/makefuncs.h"	/* makeVar() */
#include "nodes/nodeFuncs.h"
#include "nodes/plannodes.h"	/* Plan */
#include "nodes/relation.h"		/* RelOptInfo */
#include "optimizer/pathnode.h" /* Path */
#include "optimizer/paths.h"	/* cdb_build_distribution_pathkeys() */
#include "optimizer/tlist.h"	/* tlist_member() */
#include "parser/parse_expr.h"	/* exprType() and exprTypmod() */

#include "cdb/cdbvars.h"
#include "cdb/cdbpathlocus.h"	/* me */

static List *cdb_build_distribution_pathkeys(PlannerInfo *root,
								RelOptInfo *rel,
								int nattrs,
								AttrNumber *attrs);


/*
 * This flag controls the policy type returned from
 * cdbpathlocus_from_baserel() for non-partitioned tables.
 * It's a kludge put in to allow us to do
 * distributed queries on catalog tables, like pg_class
 * and pg_statistic.
 * To use it, simply set it to true before running a catalog query, then set
 * it back to false.
 */
bool		cdbpathlocus_querysegmentcatalogs = false;

/*
 * Are two pathkeys equal?
 *
 * This is roughly the same as compare_pathkeys, but compare_pathkeys
 * requires the input pathkeys to be canonicalized. The PathKeys we use to
 * represent hashed distribution are not canonicalized, so we can't use it.
 */
static bool
pathkeys_equal(List *apathkey, List *bpathkey)
{
	ListCell   *acell;
	ListCell   *bcell;

	forboth(acell, apathkey, bcell, bpathkey)
	{
		PathKey    *apathkey = (PathKey *) lfirst(acell);
		PathKey    *bpathkey = (PathKey *) lfirst(bcell);

		Assert(IsA(apathkey, PathKey));
		Assert(IsA(bpathkey, PathKey));

		if (apathkey->pk_eclass != bpathkey->pk_eclass)
			return false;
	}
	return true;
}

/*
 * Does given list of path keys contain the given pathkey? "contains" is
 * defined in terms of pathkeys_equal.
 */
static bool
list_contains_pathkey(List *list, List *pathkey)
{
	ListCell   *lc;
	bool		found;

	found = false;
	foreach(lc, list)
	{
		List	   *lpathkey = (List *) lfirst(lc);

		if (pathkeys_equal(pathkey, lpathkey))
		{
			found = true;
			break;
		}
	}
	return found;
}

/*
 * cdbpathlocus_compare
 *
 *    - Returns false if locus a or locus b is "strewn", meaning that it
 *      cannot be determined whether it is equal to another partitioned
 *      distribution.
 *
 *    - Returns true if a and b have the same 'locustype' and 'partkey'.
 *
 *    - Returns true if both a and b are hashed and the set of possible
 *      m-tuples of expressions (e1, e2, ..., em) produced by a's partkey
 *      is equal to (if op == CdbPathLocus_Equal) or a superset of (if
 *      op == CdbPathLocus_Contains) the set produced by b's partkey.
 *
 *    - Returns false otherwise.
 */
bool
cdbpathlocus_compare(CdbPathLocus_Comparison op,
					 CdbPathLocus a,
					 CdbPathLocus b)
{
	ListCell   *acell;
	ListCell   *bcell;
	ListCell   *aequivpathkeycell;
	ListCell   *bequivpathkeycell;

	Assert(op == CdbPathLocus_Comparison_Equal ||
		   op == CdbPathLocus_Comparison_Contains);

	if (CdbPathLocus_IsStrewn(a) ||
		CdbPathLocus_IsStrewn(b))
		return false;

	if (CdbPathLocus_IsEqual(a, b))
		return true;

	if (CdbPathLocus_Degree(a) == 0 ||
		CdbPathLocus_Degree(b) == 0 ||
		CdbPathLocus_Degree(a) != CdbPathLocus_Degree(b))
		return false;

	if (a.locustype == b.locustype)
	{
		if (CdbPathLocus_IsHashed(a))
			return pathkeys_equal(a.partkey_h, b.partkey_h);

		if (CdbPathLocus_IsHashedOJ(a))
		{
			forboth(acell, a.partkey_oj, bcell, b.partkey_oj)
			{
				List	   *aequivpathkeylist = (List *) lfirst(acell);
				List	   *bequivpathkeylist = (List *) lfirst(bcell);

				foreach(bequivpathkeycell, bequivpathkeylist)
				{
					List	   *bpathkey = (List *) lfirst(bequivpathkeycell);

					if (!list_contains_pathkey(aequivpathkeylist, bpathkey))
						return false;
				}
				if (op == CdbPathLocus_Comparison_Equal)
				{
					foreach(aequivpathkeycell, aequivpathkeylist)
					{
						List	   *apathkey = (List *) lfirst(aequivpathkeycell);

						if (!list_contains_pathkey(bequivpathkeylist, apathkey))
							return false;
					}
				}
			}
			return true;
		}
	}

	if (CdbPathLocus_IsHashedOJ(a) &&
		CdbPathLocus_IsHashed(b))
	{
		if (op == CdbPathLocus_Comparison_Equal)
			CdbSwap(CdbPathLocus, a, b);
		else
		{
			forboth(acell, a.partkey_oj, bcell, b.partkey_h)
			{
				List	   *aequivpathkeylist = (List *) lfirst(acell);
				List	   *bpathkey = (List *) lfirst(bcell);

				if (!list_member_ptr(aequivpathkeylist, bpathkey))
					return false;
			}
			return true;
		}
	}

	if (CdbPathLocus_IsHashed(a) &&
		CdbPathLocus_IsHashedOJ(b))
	{
		forboth(acell, a.partkey_h, bcell, b.partkey_oj)
		{
			List	   *apathkey = (List *) lfirst(acell);
			List	   *bequivpathkeylist = (List *) lfirst(bcell);

			foreach(bequivpathkeycell, bequivpathkeylist)
			{
				List	   *bpathkey = (List *) lfirst(bequivpathkeycell);

				if (apathkey != bpathkey)
					return false;
			}
		}
		return true;
	}

	Assert(false);
	return false;
}								/* cdbpathlocus_compare */

/*
 * cdb_build_distribution_pathkeys
 *	  Build canonicalized pathkeys list for given columns of rel.
 *
 *    Returns a List, of length 'nattrs': each of its members is
 *    a List of one or more PathKey nodes.  The returned List
 *    might contain duplicate entries: this occurs when the
 *    corresponding columns are constrained to be equal.
 *
 *    The caller receives ownership of the returned List, freshly
 *    palloc'ed in the caller's context.  The members of the returned
 *    List are shared and might belong to the caller's context or
 *    other contexts.
 */
static List *
cdb_build_distribution_pathkeys(PlannerInfo *root,
								RelOptInfo *rel,
								int nattrs,
								AttrNumber *attrs)
{
	List	   *retval = NIL;
	List	   *eq = list_make1(makeString("="));
	int			i;
	bool		isAppendChildRelation = false;

	isAppendChildRelation = (rel->reloptkind == RELOPT_OTHER_MEMBER_REL);

	for (i = 0; i < nattrs; ++i)
	{
		PathKey    *cpathkey;

		/* Find or create a Var node that references the specified column. */
		Var		   *expr = find_indexkey_var(root, rel, attrs[i]);

		Assert(expr);

		/*
		 * Find or create a pathkey. We distinguish two cases for performance
		 * reasons: 1) If the relation in question is a child relation under
		 * an append node, we don't care about ensuring that we return a
		 * canonicalized version of its pathkey item. Co-location of
		 * joins/group-bys happens at the append relation level. In
		 * create_append_path(), the call to
		 * cdbpathlocus_pull_above_projection() ensures that canonicalized
		 * pathkeys are created at the append relation level. (see MPP-3536).
		 *
		 * 2) For regular relations, we create a canonical pathkey so that we
		 * may identify co-location for joins/group-bys.
		 */
		if (isAppendChildRelation)
		{
			/**
        	 * Append child relation.
        	 */
#ifdef DISTRIBUTION_PATHKEYS_DEBUG
			PathKey    *canonicalPathKeyList = cdb_make_pathkey_for_expr(root, (Node *) expr, eq, true);

			/*
			 * This assert ensures that we should not really find any
			 * equivalent keys during canonicalization for append child
			 * relations.
			 */
			Assert(list_length(canonicalPathKeyList) == 1);
#endif
			cpathkey = cdb_make_pathkey_for_expr(root, (Node *) expr, eq, false);
			Assert(cpathkey);
		}
		else
		{
			/**
        	 * Regular relation.
        	 */
			cpathkey = cdb_make_pathkey_for_expr(root, (Node *) expr, eq, true);
		}
		Assert(cpathkey);

		/* Append to list of pathkeys. */
		retval = lappend(retval, cpathkey);
	}

	list_free_deep(eq);
	return retval;
}								/* cdb_build_distribution_pathkeys */

/*
 * cdbpathlocus_from_baserel
 *
 * Returns a locus describing the distribution of a base relation.
 */
CdbPathLocus
cdbpathlocus_from_baserel(struct PlannerInfo *root,
						  struct RelOptInfo *rel)
{
	CdbPathLocus result;
	GpPolicy   *policy = rel->cdbpolicy;

	if (Gp_role != GP_ROLE_DISPATCH)
	{
		CdbPathLocus_MakeEntry(&result);
		return result;
	}

	if (policy &&
		policy->ptype == POLICYTYPE_PARTITIONED)
	{
		/* Are the rows distributed by hashing on specified columns? */
		if (policy->nattrs > 0)
		{
			List	   *partkey = cdb_build_distribution_pathkeys(root,
																  rel,
																  policy->nattrs,
																  policy->attrs);

			CdbPathLocus_MakeHashed(&result, partkey);
		}

		/* Rows are distributed on an unknown criterion (uniformly, we hope!) */
		else
			CdbPathLocus_MakeStrewn(&result);
	}

	/* Kludge used internally for querying catalogs on segment dbs */
	else if (cdbpathlocus_querysegmentcatalogs)
		CdbPathLocus_MakeStrewn(&result);

	/* Normal catalog access */
	else
		CdbPathLocus_MakeEntry(&result);

	return result;
}								/* cdbpathlocus_from_baserel */


/*
 * cdbpathlocus_from_exprs
 *
 * Returns a locus specifying hashed distribution on a list of exprs.
 */
CdbPathLocus
cdbpathlocus_from_exprs(struct PlannerInfo *root,
						List *hash_on_exprs)
{
	CdbPathLocus locus;
	List	   *partkey = NIL;
	List	   *eq = list_make1(makeString("="));
	ListCell   *cell;

	foreach(cell, hash_on_exprs)
	{
		Node	   *expr = (Node *) lfirst(cell);
		PathKey    *pathkey;

		pathkey = cdb_make_pathkey_for_expr(root, expr, eq, true);
		partkey = lappend(partkey, pathkey);
	}

	CdbPathLocus_MakeHashed(&locus, partkey);
	list_free_deep(eq);
	return locus;
}								/* cdbpathlocus_from_exprs */


/*
 * cdbpathlocus_from_subquery
 *
 * Returns a locus describing the distribution of a subquery.
 * The subquery plan should have been generated already.
 *
 * 'subqplan' is the subquery plan.
 * 'subqrelid' is the subquery RTE index in the current query level, for
 *      building Var nodes that reference the subquery's result columns.
 */
CdbPathLocus
cdbpathlocus_from_subquery(struct PlannerInfo *root,
						   struct Plan *subqplan,
						   Index subqrelid)
{
	CdbPathLocus locus;
	Flow	   *flow = subqplan->flow;

	Insist(flow);

	/* Flow node was made from CdbPathLocus by cdbpathtoplan_create_flow() */
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
			{
				List	   *partkey = NIL;
				ListCell   *hashexprcell;
				List	   *eq = list_make1(makeString("="));

				foreach(hashexprcell, flow->hashExpr)
				{
					Node	   *expr = (Node *) lfirst(hashexprcell);
					TargetEntry *tle;
					Var		   *var;
					PathKey    *pathkey;

					/*
					 * Look for hash key expr among the subquery result
					 * columns.
					 */
					tle = tlist_member_ignore_relabel(expr, subqplan->targetlist);
					if (!tle)
						break;

					Assert(tle->resno >= 1);
					var = makeVar(subqrelid,
								  tle->resno,
								  exprType((Node *) tle->expr),
								  exprTypmod((Node *) tle->expr),
								  0);
					pathkey = cdb_make_pathkey_for_expr(root, (Node *) var, eq, true);
					partkey = lappend(partkey, pathkey);
				}
				if (partkey &&
					!hashexprcell)
					CdbPathLocus_MakeHashed(&locus, partkey);
				else
					CdbPathLocus_MakeStrewn(&locus);
				list_free_deep(eq);
				break;
			}
		default:
			CdbPathLocus_MakeNull(&locus);
			Insist(0);
	}
	return locus;
}								/* cdbpathlocus_from_subquery */


/*
 * cdbpathlocus_get_partkey_exprs
 *
 * Returns a List with one Expr for each partkey column.  Each item either is
 * in the given targetlist, or has no Var nodes that are not in the targetlist;
 * and uses only rels in the given set of relids.  Returns NIL if the
 * partkey cannot be expressed in terms of the given relids and targetlist.
 */
List *
cdbpathlocus_get_partkey_exprs(CdbPathLocus locus,
							   Bitmapset *relids,
							   List *targetlist)
{
	List	   *result = NIL;
	ListCell   *partkeycell;

	Assert(cdbpathlocus_is_valid(locus));

	if (CdbPathLocus_IsHashed(locus))
	{
		foreach(partkeycell, locus.partkey_h)
		{
			PathKey    *pathkey = (PathKey *) lfirst(partkeycell);
			Expr	   *item;

			item = cdbpullup_findPathKeyExprInTargetList(pathkey, targetlist);

			/*
			 * Fail if can't evaluate partkey in the context of this
			 * targetlist.
			 */
			if (!item)
				return NIL;

			result = lappend(result, item);
		}
		return result;
	}
	else if (CdbPathLocus_IsHashedOJ(locus))
	{
		foreach(partkeycell, locus.partkey_oj)
		{
			List	   *pathkeylist = (List *) lfirst(partkeycell);
			ListCell   *pathkeylistcell;
			Expr	   *item = NULL;

			foreach(pathkeylistcell, pathkeylist)
			{
				PathKey    *pathkey = (PathKey *) lfirst(pathkeylistcell);

				item = cdbpullup_findPathKeyExprInTargetList(pathkey, targetlist);

				if (item)
				{
					result = lappend(result, item);
					break;
				}
			}
			if (!item)
				return NIL;
		}
		return result;
	}
	else
		return NIL;
}								/* cdbpathlocus_get_partkey_exprs */


/*
 * cdbpathlocus_pull_above_projection
 *
 * Given a targetlist, and a locus evaluable before the projection is
 * applied, returns an equivalent locus evaluable after the projection.
 * Returns a strewn locus if the necessary inputs are not projected.
 *
 * 'relids' is the set of relids that may occur in the targetlist exprs.
 * 'targetlist' specifies the projection.  It is a List of TargetEntry
 *      or merely a List of Expr.
 * 'newvarlist' is an optional List of Expr, in 1-1 correspondence with
 *      'targetlist'.  If specified, instead of creating a Var node to
 *      reference a targetlist item, we plug in a copy of the corresponding
 *      newvarlist item.
 * 'newrelid' is the RTE index of the projected result, for finding or
 *      building Var nodes that reference the projected columns.
 *      Ignored if 'newvarlist' is specified.
 */
CdbPathLocus
cdbpathlocus_pull_above_projection(struct PlannerInfo *root,
								   CdbPathLocus locus,
								   Bitmapset *relids,
								   List *targetlist,
								   List *newvarlist,
								   Index newrelid)
{
	CdbPathLocus newlocus;
	ListCell   *partkeycell;
	List	   *newpartkey = NIL;

	Assert(cdbpathlocus_is_valid(locus));

	if (CdbPathLocus_IsHashed(locus))
	{
		foreach(partkeycell, locus.partkey_h)
		{
			PathKey    *oldpathkey;
			PathKey    *newpathkey = NULL;

			/* Get pathkey for key expr rewritten in terms of projection cols. */
			oldpathkey = (PathKey *) lfirst(partkeycell);
			newpathkey = cdb_pull_up_pathkey(root,
											 oldpathkey,
											 relids,
											 targetlist,
											 newvarlist,
											 newrelid);

			/*
			 * Fail if can't evaluate partkey in the context of this
			 * targetlist.
			 */
			if (!newpathkey)
			{
				CdbPathLocus_MakeStrewn(&newlocus);
				return newlocus;
			}

			/* Assemble new partkey. */
			newpartkey = lappend(newpartkey, newpathkey);
		}

		/* Build new locus. */
		CdbPathLocus_MakeHashed(&newlocus, newpartkey);
		return newlocus;
	}
	else if (CdbPathLocus_IsHashedOJ(locus))
	{
		/* For each column of the partitioning key... */
		foreach(partkeycell, locus.partkey_oj)
		{
			PathKey    *oldpathkey;
			PathKey    *newpathkey = NULL;

			/* Get pathkey for key expr rewritten in terms of projection cols. */
			List	   *pathkeylist = (List *) lfirst(partkeycell);
			ListCell   *pathkeylistcell;

			foreach(pathkeylistcell, pathkeylist)
			{
				oldpathkey = (PathKey *) lfirst(pathkeylistcell);
				newpathkey = cdb_pull_up_pathkey(root,
												 oldpathkey,
												 relids,
												 targetlist,
												 newvarlist,
												 newrelid);
				if (newpathkey)
					break;
			}

			/*
			 * NB: Targetlist might include columns from both sides of outer
			 * join "=" comparison, in which case cdb_pull_up_pathkey might
			 * succeed on pathkeys from more than one pathkeylist. The
			 * pulled-up locus could then be a HashedOJ locus, perhaps saving
			 * a Motion when an outer join is followed by UNION ALL followed
			 * by a join or aggregate.  For now, don't bother.
			 */

			/*
			 * Fail if can't evaluate partkey in the context of this
			 * targetlist.
			 */
			if (!newpathkey)
			{
				CdbPathLocus_MakeStrewn(&newlocus);
				return newlocus;
			}

			/* Assemble new partkey. */
			newpartkey = lappend(newpartkey, newpathkey);
		}

		/* Build new locus. */
		CdbPathLocus_MakeHashed(&newlocus, newpartkey);
		return newlocus;
	}
	else
		return locus;
}								/* cdbpathlocus_pull_above_projection */


/*
 * cdbpathlocus_join
 *
 * Determine locus to describe the result of a join.  Any necessary Motion has
 * already been applied to the sources.
 */
CdbPathLocus
cdbpathlocus_join(CdbPathLocus a, CdbPathLocus b)
{
	ListCell   *acell;
	ListCell   *bcell;
	List	   *equivpathkeylist;
	CdbPathLocus ojlocus = {0};

	Assert(cdbpathlocus_is_valid(a));
	Assert(cdbpathlocus_is_valid(b));

	/* Do both input rels have same locus? */
	if (cdbpathlocus_compare(CdbPathLocus_Comparison_Equal, a, b))
		return a;

	/* If one rel is general or replicated, result stays with the other rel. */
	if (CdbPathLocus_IsGeneral(a) ||
		CdbPathLocus_IsReplicated(a))
		return b;
	if (CdbPathLocus_IsGeneral(b) ||
		CdbPathLocus_IsReplicated(b))
		return a;

	/* This is an outer join, or one or both inputs are outer join results. */

	Assert(CdbPathLocus_Degree(a) > 0 &&
		   CdbPathLocus_Degree(a) == CdbPathLocus_Degree(b));

	if (CdbPathLocus_IsHashed(a) &&
		CdbPathLocus_IsHashed(b))
	{
		/* Zip the two pathkey lists together to make a HashedOJ locus. */
		List	   *partkey_oj = NIL;

		forboth(acell, a.partkey_h, bcell, b.partkey_h)
		{
			PathKey    *apathkey = (PathKey *) lfirst(acell);
			PathKey    *bpathkey = (PathKey *) lfirst(bcell);

			equivpathkeylist = list_make2(apathkey, bpathkey);
			partkey_oj = lappend(partkey_oj, equivpathkeylist);
		}
		CdbPathLocus_MakeHashedOJ(&ojlocus, partkey_oj);
		Assert(cdbpathlocus_is_valid(ojlocus));
		return ojlocus;
	}

	if (!CdbPathLocus_IsHashedOJ(a))
		CdbSwap(CdbPathLocus, a, b);

	Assert(CdbPathLocus_IsHashedOJ(a));
	Assert(CdbPathLocus_IsHashed(b) ||
		   CdbPathLocus_IsHashedOJ(b));

	if (CdbPathLocus_IsHashed(b))
	{
		List	   *partkey_oj = NIL;

		forboth(acell, a.partkey_oj, bcell, b.partkey_h)
		{
			List	   *aequivpathkeylist = (List *) lfirst(acell);
			PathKey    *bpathkey = (PathKey *) lfirst(bcell);

			equivpathkeylist = lappend(list_copy(aequivpathkeylist), bpathkey);
			partkey_oj = lappend(partkey_oj, equivpathkeylist);
		}
		CdbPathLocus_MakeHashedOJ(&ojlocus, partkey_oj);
	}
	else if (CdbPathLocus_IsHashedOJ(b))
	{
		List	   *partkey_oj = NIL;

		forboth(acell, a.partkey_oj, bcell, b.partkey_oj)
		{
			List	   *aequivpathkeylist = (List *) lfirst(acell);
			List	   *bequivpathkeylist = (List *) lfirst(bcell);

			equivpathkeylist = list_union_ptr(aequivpathkeylist,
											  bequivpathkeylist);
			partkey_oj = lappend(partkey_oj, equivpathkeylist);
		}
		CdbPathLocus_MakeHashedOJ(&ojlocus, partkey_oj);
	}
	Assert(cdbpathlocus_is_valid(ojlocus));
	return ojlocus;
}								/* cdbpathlocus_join */

/*
 * cdbpathlocus_is_hashed_on_exprs
 *
 * This function tests whether grouping on a given set of exprs can be done
 * in place without motion.
 *
 * For a hashed locus, returns false if the partkey has a column whose
 * equivalence class contains no expr belonging to the given list.
 */
bool
cdbpathlocus_is_hashed_on_exprs(CdbPathLocus locus, List *exprlist)
{
	ListCell   *partkeycell;

	Assert(cdbpathlocus_is_valid(locus));

	if (CdbPathLocus_IsHashed(locus))
	{
		foreach(partkeycell, locus.partkey_h)
		{
			bool		found = false;
			ListCell   *i;

			/* Does pathkey have an expr that is equal() to one in exprlist? */
			PathKey    *pathkey = (PathKey *) lfirst(partkeycell);

			Assert(IsA(pathkey, PathKey));

			foreach(i, pathkey->pk_eclass->ec_members)
			{
				EquivalenceMember *em = (EquivalenceMember *) lfirst(i);

				if (list_member(exprlist, em->em_expr))
				{
					found = true;
					break;
				}
			}
			if (!found)
				return false;
		}
		/* Every column of the partkey contains an expr in exprlist. */
		return true;
	}
	else if (CdbPathLocus_IsHashedOJ(locus))
	{
		foreach(partkeycell, locus.partkey_oj)
		{
			List	   *pathkeylist = (List *) lfirst(partkeycell);
			ListCell   *pathkeylistcell;
			bool		found = false;

			foreach(pathkeylistcell, pathkeylist)
			{
				/* Does some expr in pathkey match some item in exprlist? */
				PathKey    *item = (PathKey *) lfirst(pathkeylistcell);
				ListCell   *i;

				Assert(IsA(item, PathKey));

				foreach(i, item->pk_eclass->ec_members)
				{
					EquivalenceMember *em = (EquivalenceMember *) lfirst(i);

					if (list_member(exprlist, em->em_expr))
					{
						found = true;
						break;
					}
				}
				if (found)
					break;
			}
			if (!found)
				return false;
		}
		/* Every column of the partkey contains an expr in exprlist. */
		return true;
	}
	else
		return !CdbPathLocus_IsStrewn(locus);
}								/* cdbpathlocus_is_hashed_on_exprs */

/*
 * cdbpathlocus_is_hashed_on_eclasses
 *
 * This function tests whether grouping on a given set of exprs can be done
 * in place without motion.
 *
 * For a hashed locus, returns false if the partkey has any column whose
 * equivalence class is not in 'eclasses' list.
 *
 * If 'ignore_constants' is true, any constants in the locus are ignored.
 */
bool
cdbpathlocus_is_hashed_on_eclasses(CdbPathLocus locus, List *eclasses,
								   bool ignore_constants)
{
	ListCell   *partkeycell;
	ListCell   *eccell;

	Assert(cdbpathlocus_is_valid(locus));

	if (CdbPathLocus_IsHashed(locus))
	{
		foreach(partkeycell, locus.partkey_h)
		{
			PathKey    *pathkey = (PathKey *) lfirst(partkeycell);
			bool		found = false;
			EquivalenceClass *pk_ec;

			/* Does pathkey have an eclass that's not in 'eclasses'? */
			Assert(IsA(pathkey, PathKey));

			pk_ec = pathkey->pk_eclass;
			while (pk_ec->ec_merged != NULL)
				pk_ec = pk_ec->ec_merged;

			if (ignore_constants && CdbEquivClassIsConstant(pk_ec))
				continue;

			foreach(eccell, eclasses)
			{
				EquivalenceClass *ec = (EquivalenceClass *) lfirst(eccell);

				while (ec->ec_merged != NULL)
					ec = ec->ec_merged;

				if (ec == pk_ec)
				{
					found = true;
					break;
				}
			}
			if (!found)
				return false;
		}
		/* Every column of the partkey contains an expr in exprlist. */
		return true;
	}
	else if (CdbPathLocus_IsHashedOJ(locus))
	{
		foreach(partkeycell, locus.partkey_oj)
		{
			List	   *pathkeylist = (List *) lfirst(partkeycell);
			ListCell   *pathkeylistcell;
			bool		found = false;

			foreach(pathkeylistcell, pathkeylist)
			{
				PathKey    *pathkey = (PathKey *) lfirst(pathkeylistcell);
				EquivalenceClass *pk_ec;

				/* Does pathkey have an eclass that's not in 'eclasses'? */
				Assert(IsA(pathkey, PathKey));

				pk_ec = pathkey->pk_eclass;
				while (pk_ec->ec_merged != NULL)
					pk_ec = pk_ec->ec_merged;

				if (ignore_constants && CdbEquivClassIsConstant(pk_ec))
					continue;

				foreach(eccell, eclasses)
				{
					EquivalenceClass *ec = (EquivalenceClass *) lfirst(eccell);

					while (ec->ec_merged != NULL)
						ec = ec->ec_merged;

					if (ec == pk_ec)
					{
						found = true;
						break;
					}
				}
				if (found)
					break;
			}
			if (!found)
				return false;
		}
		/* Every column of the partkey contains an expr in exprlist. */
		return true;
	}
	else
		return !CdbPathLocus_IsStrewn(locus);
}								/* cdbpathlocus_is_hashed_on_exprs */


/*
 * cdbpathlocus_is_hashed_on_relids
 *
 * Used when a subquery predicate (such as "x IN (SELECT ...)") has been
 * flattened into a join with the tables of the current query.  A row of
 * the cross-product of the current query's tables might join with more
 * than one row from the subquery and thus be duplicated.  Removing such
 * duplicates after the join is called deduping.  If a row's duplicates
 * might occur in more than one partition, a Motion operator will be
 * needed to bring them together.  This function tests whether the join
 * result (whose locus is given) can be deduped in place without motion.
 *
 * For a hashed locus, returns false if the partkey has a column whose
 * equivalence class contains no Var node belonging to the given set of
 * relids.  Caller should specify the relids of the non-subquery tables.
 */
bool
cdbpathlocus_is_hashed_on_relids(CdbPathLocus locus, Bitmapset *relids)
{
	ListCell   *partkeycell;
	ListCell   *pathkeycell;

	Assert(cdbpathlocus_is_valid(locus));

	if (CdbPathLocus_IsHashed(locus))
	{
		foreach(partkeycell, locus.partkey_h)
		{
			bool		found = false;

			/* Does pathkey contain a Var whose varno is in relids? */
			PathKey    *pathkey = (PathKey *) lfirst(partkeycell);

			Assert(IsA(pathkey, PathKey));
			foreach(pathkeycell, pathkey->pk_eclass->ec_members)
			{
				EquivalenceMember *em = (EquivalenceMember *) lfirst(pathkeycell);

				if (IsA(em->em_expr, Var) &&bms_is_subset(em->em_relids, relids))
				{
					found = true;
					break;
				}
			}
			if (!found)
				return false;
		}

		/*
		 * Every column of the partkey contains a Var whose varno is in
		 * relids.
		 */
		return true;
	}
	else if (CdbPathLocus_IsHashedOJ(locus))
	{
		foreach(partkeycell, locus.partkey_oj)
		{
			bool		found = false;
			List	   *pathkeylist = (List *) lfirst(partkeycell);
			ListCell   *pathkeylistcell;

			foreach(pathkeylistcell, pathkeylist)
			{
				/* Does pathkey contain a Var whose varno is in relids? */
				PathKey    *item = (PathKey *) lfirst(pathkeylistcell);
				ListCell   *i;

				Assert(IsA(item, PathKey));

				foreach(i, item->pk_eclass->ec_members)
				{
					EquivalenceMember *em = (EquivalenceMember *) lfirst(i);

					if (IsA(em->em_expr, Var) &&bms_is_subset(em->em_relids, relids))
					{
						found = true;
						break;
					}
				}
				if (found)
					break;
			}
			if (!found)
				return false;
		}

		/*
		 * Every column of the partkey contains a Var whose varno is in
		 * relids.
		 */
		return true;
	}
	else
		return !CdbPathLocus_IsStrewn(locus);
}								/* cdbpathlocus_is_hashed_on_relids */


/*
 * cdbpathlocus_is_valid
 *
 * Returns true if locus appears structurally valid.
 */
bool
cdbpathlocus_is_valid(CdbPathLocus locus)
{
	ListCell   *partkeycell;

	if (!CdbLocusType_IsValid(locus.locustype))
		goto bad;

	if (!CdbPathLocus_IsHashed(locus) && locus.partkey_h != NIL)
		goto bad;
	if (!CdbPathLocus_IsHashedOJ(locus) && locus.partkey_oj != NIL)
		goto bad;

	if (CdbPathLocus_IsHashed(locus))
	{
		if (locus.partkey_h == NIL)
			goto bad;
		if (!IsA(locus.partkey_h, List))
			goto bad;
		foreach(partkeycell, locus.partkey_h)
		{
			PathKey    *item = (PathKey *) lfirst(partkeycell);

			if (!item || !IsA(item, PathKey))
				goto bad;
		}
	}
	if (CdbPathLocus_IsHashedOJ(locus))
	{
		if (locus.partkey_oj == NIL)
			goto bad;
		if (!IsA(locus.partkey_oj, List))
			goto bad;
		foreach(partkeycell, locus.partkey_oj)
		{
			List	   *item = (List *) lfirst(partkeycell);

			if (!item || !IsA(item, List))
				goto bad;
		}
	}
	return true;

bad:
	return false;
}								/* cdbpathlocus_is_valid */
