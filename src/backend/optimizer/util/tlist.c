/*-------------------------------------------------------------------------
 *
 * tlist.c
 *	  Target list manipulation routines
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/optimizer/util/tlist.c,v 1.85 2009/01/01 17:23:45 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/planmain.h"
#include "optimizer/tlist.h"
#include "optimizer/var.h"
#include "utils/lsyscache.h"

typedef struct maxSortGroupRef_context
{
	Index maxsgr;
	bool include_orderedagg;
} maxSortGroupRef_context;

static
bool maxSortGroupRef_walker(Node *node, maxSortGroupRef_context *cxt);

/*****************************************************************************
 *		Target list creation and searching utilities
 *****************************************************************************/

/*
 * tlist_member
 *	  Finds the (first) member of the given tlist whose expression is
 *	  equal() to the given expression.	Result is NULL if no such member.
 */
TargetEntry *
tlist_member(Node *node, List *targetlist)
{
	ListCell   *temp;

	foreach(temp, targetlist)
	{
		TargetEntry *tlentry = (TargetEntry *) lfirst(temp);

        Assert(IsA(tlentry, TargetEntry));

		if (equal(node, tlentry->expr))
			return tlentry;
	}
	return NULL;
}

/*
 * tlist_members
 *	  Finds all members of the given tlist whose expression is
 *	  equal() to the given expression.	Result is NIL if no such member.
 *	  Note: We do not make a copy of the tlist entries that match.
 *	  The caller is responsible for cleaning up the memory allocated
 *	  to the List returned.
 */
List *
tlist_members(Node *node, List *targetlist)
{
	List *tlist = NIL;
	ListCell   *temp = NULL;

	foreach(temp, targetlist)
	{
		TargetEntry *tlentry = (TargetEntry *) lfirst(temp);

        Assert(IsA(tlentry, TargetEntry));

		if (equal(node, tlentry->expr))
		{
			tlist = lappend(tlist, tlentry);
		}
	}

	return tlist;
}

/*
 * tlist_member_ignore_relabel
 *	  Same as above, except that we ignore top-level RelabelType nodes
 *	  while checking for a match.  This is needed for some scenarios
 *	  involving binary-compatible sort operations.
 */
TargetEntry *
tlist_member_ignore_relabel(Node *node, List *targetlist)
{
	ListCell   *temp;

	while (node && IsA(node, RelabelType))
		node = (Node *) ((RelabelType *) node)->arg;

	foreach(temp, targetlist)
	{
		TargetEntry *tlentry = (TargetEntry *) lfirst(temp);
		Expr	   *tlexpr = tlentry->expr;

		while (tlexpr && IsA(tlexpr, RelabelType))
			tlexpr = ((RelabelType *) tlexpr)->arg;

		if (equal(node, tlexpr))
			return tlentry;
	}
	return NULL;
}

/*
 * flatten_tlist
 *	  Create a target list that only contains unique variables.
 *
 * Aggrefs and PlaceHolderVars in the input are treated according to
 * aggbehavior and phbehavior, for which see pull_var_clause().
 *
 * 'tlist' is the current target list
 *
 * Returns the "flattened" new target list.
 *
 * The result is entirely new structure sharing no nodes with the original.
 * Copying the Var nodes is probably overkill, but be safe for now.
 */
List *
flatten_tlist(List *tlist, PVCAggregateBehavior aggbehavior,
			  PVCPlaceHolderBehavior phbehavior)
{
	List	   *vlist = pull_var_clause((Node *) tlist,
										aggbehavior,
										phbehavior);
	List	   *new_tlist;

	new_tlist = add_to_flat_tlist(NIL, vlist);
	list_free(vlist);
	return new_tlist;
}

/*
 * add_to_flat_tlist
 *		Add more items to a flattened tlist (if they're not already in it)
 *
 * 'tlist' is the flattened tlist
 * 'exprs' is a list of expressions (usually, but not necessarily, Vars)
 *
 * Returns the extended tlist.
 */
List *
add_to_flat_tlist(List *tlist, List *exprs)
{
	return add_to_flat_tlist_junk(tlist, exprs, false);
}

List *
add_to_flat_tlist_junk(List *tlist, List *exprs, bool resjunk)
{
	int			next_resno = list_length(tlist) + 1;
	ListCell   *lc;

	foreach(lc, exprs)
	{
		Node	   *expr = (Node *) lfirst(lc);

		if (!tlist_member_ignore_relabel(expr, tlist))
		{
			TargetEntry *tle;

			tle = makeTargetEntry(copyObject(expr),		/* copy needed?? */
								  next_resno++,
								  NULL,
								  resjunk);
			tlist = lappend(tlist, tle);
		}
	}
	return tlist;
}


/*
 * get_tlist_exprs
 *		Get Ejust the expression subtrees of a tlist
 *
 * Resjunk columns are ignored unless includeJunk is true
 */
List *
get_tlist_exprs(List *tlist, bool includeJunk)
{
	List	   *result = NIL;
	ListCell   *l;

	foreach(l, tlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(l);

		if (tle->resjunk && !includeJunk)
			continue;

		result = lappend(result, tle->expr);
	}
	return result;
}


/*
 * Does tlist have same output datatypes as listed in colTypes?
 *
 * Resjunk columns are ignored if junkOK is true; otherwise presence of
 * a resjunk column will always cause a 'false' result.
 *
 * Note: currently no callers care about comparing typmods.
 */
bool
tlist_same_datatypes(List *tlist, List *colTypes, bool junkOK)
{
	ListCell   *l;
	ListCell   *curColType = list_head(colTypes);

	foreach(l, tlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(l);

		if (tle->resjunk)
		{
			if (!junkOK)
				return false;
		}
		else
		{
			if (curColType == NULL)
				return false;	/* tlist longer than colTypes */
			if (exprType((Node *) tle->expr) != lfirst_oid(curColType))
				return false;
			curColType = lnext(curColType);
		}
	}
	if (curColType != NULL)
		return false;			/* tlist shorter than colTypes */
	return true;
}


/*
 * get_sortgroupref_tle
 *		Find the targetlist entry matching the given SortGroupRef index,
 *		and return it.
 */
TargetEntry *
get_sortgroupref_tle(Index sortref, List *targetList)
{
	ListCell   *l;

	foreach(l, targetList)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(l);

		if (tle->ressortgroupref == sortref)
			return tle;
	}

	/*
	 * XXX: we probably should catch this earlier, but we have a
	 * few queries in the regression suite that hit this.
	 */
	ereport(ERROR,
			(errcode(ERRCODE_SYNTAX_ERROR),
			 errmsg("ORDER/GROUP BY expression not found in targetlist")));
	return NULL;				/* keep compiler quiet */
}

/*
 * get_sortgroupclause_tle
 *		Find the targetlist entry matching the given SortGroupClause
 *		by ressortgroupref, and return it.
 */
TargetEntry *
get_sortgroupclause_tle(SortGroupClause *sgClause,
						List *targetList)
{
	return get_sortgroupref_tle(sgClause->tleSortGroupRef, targetList);
}

/*
 * get_sortgroupclauses_tles
 *      Find a list of unique targetlist entries matching the given list of
 *      SortGroupClauses, or GroupingClauses.
 *
 * In each grouping set, targets that do not appear in a GroupingClause
 * will be put in the front of those that appear in a GroupingClauses.
 * The targets within the same clause will be appended in the order
 * of their appearance.
 *
 * The unique targetlist entries are returned in *tles, and the sort
 * and equality operators associated with each tle are returned in
 * *sortops and *eqops.
 */
static void
get_sortgroupclauses_tles_recurse(List *clauses, List *targetList,
								  List **tles, List **sortops, List **eqops)
{
	ListCell   *lc;
	ListCell   *lc_sortop;
	ListCell   *lc_eqop;
	List	   *sub_grouping_tles = NIL;
	List	   *sub_grouping_sortops = NIL;
	List	   *sub_grouping_eqops = NIL;

	foreach(lc, clauses)
	{
		Node *node = lfirst(lc);

		if (node == NULL)
			continue;

		if (IsA(node, SortGroupClause))
		{
			SortGroupClause *sgc = (SortGroupClause *) node;
			TargetEntry *tle = get_sortgroupclause_tle(sgc,
													   targetList);

			if (!list_member(*tles, tle))
			{
				*tles = lappend(*tles, tle);
				*sortops = lappend_oid(*sortops, sgc->sortop);
				*eqops = lappend_oid(*eqops, sgc->eqop);
			}
		}
		else if (IsA(node, List))
		{
			get_sortgroupclauses_tles_recurse((List *) node, targetList,
											  tles, sortops, eqops);
		}
		else if (IsA(node, GroupingClause))
		{
			/* GroupingClauses are collected into separate list */
			get_sortgroupclauses_tles_recurse(((GroupingClause *) node)->groupsets,
											  targetList,
											  &sub_grouping_tles,
											  &sub_grouping_sortops,
											  &sub_grouping_eqops);
		}
		else
			elog(ERROR, "unrecognized node type in list of sort/group clauses: %d",
				 (int) nodeTag(node));
	}

	/*
	 * Put SortGroupClauses before GroupingClauses.
	 */
	forthree(lc, sub_grouping_tles,
			 lc_sortop, sub_grouping_sortops,
			 lc_eqop, sub_grouping_eqops)
	{
		if (!list_member(*tles, lfirst(lc)))
		{
			*tles = lappend(*tles, lfirst(lc));
			*sortops = lappend_oid(*sortops, lfirst_oid(lc_sortop));
			*eqops = lappend_oid(*eqops, lfirst_oid(lc_eqop));
		}
	}
}

void
get_sortgroupclauses_tles(List *clauses, List *targetList,
						  List **tles, List **sortops, List **eqops)
{
	*tles = NIL;
	*sortops = NIL;
	*eqops = NIL;

	get_sortgroupclauses_tles_recurse(clauses, targetList,
									  tles, sortops, eqops);
}

/*
 * get_sortgroupclause_expr
 *		Find the targetlist entry matching the given SortGroupClause
 *		by ressortgroupref, and return its expression.
 */
Node *
get_sortgroupclause_expr(SortGroupClause *sgClause, List *targetList)
{
	TargetEntry *tle = get_sortgroupclause_tle(sgClause, targetList);

	return (Node *) tle->expr;
}

/*
 * get_sortgrouplist_exprs
 *		Given a list of SortGroupClauses, build a list
 *		of the referenced targetlist expressions.
 */
List *
get_sortgrouplist_exprs(List *sgClauses, List *targetList)
{
	List	   *result = NIL;
	ListCell   *l;

	foreach(l, sgClauses)
	{
		SortGroupClause *sortcl = (SortGroupClause *) lfirst(l);
		Node	   *sortexpr;

		sortexpr = get_sortgroupclause_expr(sortcl, targetList);
		result = lappend(result, sortexpr);
	}
	return result;
}

/*
 * get_grouplist_colidx
 *		Given a list of GroupClauses, build an array of the referenced
 *		targetlist resnos and their sort operators.  If numCols is not NULL,
 *		it is filled by the length of returned array.  Results are allocated
 *		by palloc.
 */
void
get_grouplist_colidx(List *groupClauses, List *targetList, int *numCols,
					 AttrNumber **colIdx, Oid **grpOperators)
{
	List	   *tles;
	List	   *sortops;
	List	   *eqops;
	ListCell   *lc_tle;
	ListCell   *lc_eqop;
	int			i,
				len;

	len = num_distcols_in_grouplist(groupClauses);
	if (numCols)
		*numCols = len;

	if (len == 0)
	{
		*colIdx = NULL;
		*grpOperators = NULL;
		return;
	}

	get_sortgroupclauses_tles(groupClauses, targetList, &tles, &sortops, &eqops);

	*colIdx = (AttrNumber *) palloc(sizeof(AttrNumber) * len);
	*grpOperators = (Oid *) palloc(sizeof(Oid) * len);

	i = 0;
	forboth(lc_tle, tles, lc_eqop, eqops)
	{
		TargetEntry	*tle = lfirst(lc_tle);
		Oid			eqop = lfirst_oid(lc_eqop);

		Assert (i < len);

		(*colIdx)[i] = tle->resno;
		(*grpOperators)[i] = eqop;
		if (!OidIsValid((*grpOperators)[i]))		/* shouldn't happen */
			elog(ERROR, "could not find equality operator for ordering operator %u",
				 (*grpOperators)[i]);
		i++;
	}
}

/*
 * get_grouplist_exprs
 *     Find a list of unique referenced targetlist expressions used in a given
 *     list of GroupClauses or a GroupingClauses.
 *
 * All expressions will appear in the same order as they appear in the
 * given list of GroupClauses or a GroupingClauses.
 *
 * Note that the top-level empty sets will be removed here.
 */
List *
get_grouplist_exprs(List *groupClauses, List *targetList)
{
	List *result = NIL;
	ListCell *l;

	foreach (l, groupClauses)
	{
		Node *groupClause = lfirst(l);

		if (groupClause == NULL)
			continue;

		Assert(IsA(groupClause, SortGroupClause) ||
			   IsA(groupClause, GroupingClause) ||
			   IsA(groupClause, List));

		if (IsA(groupClause, SortGroupClause))
		{
			Node *expr = get_sortgroupclause_expr((SortGroupClause *) groupClause,
												  targetList);

			if (!list_member(result, expr))
				result = lappend(result, expr);
		}

		else if (IsA(groupClause, List))
			result = list_concat_unique(result,
								 get_grouplist_exprs((List *)groupClause, targetList));

		else
			result = list_concat_unique(result,
								 get_grouplist_exprs(((GroupingClause*)groupClause)->groupsets,
													 targetList));
	}

	return result;
}


/*****************************************************************************
 *		Functions to extract data from a list of SortGroupClauses
 *
 * These don't really belong in tlist.c, but they are sort of related to the
 * functions just above, and they don't seem to deserve their own file.
 *****************************************************************************/

/*
 * extract_grouping_ops - make an array of the equality operator OIDs
 *		for a SortGroupClause list
 */
Oid *
extract_grouping_ops(List *groupClause)
{
	int			numCols = list_length(groupClause);
	int			colno = 0;
	Oid		   *groupOperators;
	ListCell   *glitem;

	groupOperators = (Oid *) palloc(sizeof(Oid) * numCols);

	foreach(glitem, groupClause)
	{
		SortGroupClause *groupcl = (SortGroupClause *) lfirst(glitem);

		groupOperators[colno] = groupcl->eqop;
		Assert(OidIsValid(groupOperators[colno]));
		colno++;
	}

	return groupOperators;
}

/*
 * extract_grouping_cols - make an array of the grouping column resnos
 *		for a SortGroupClause list
 */
AttrNumber *
extract_grouping_cols(List *groupClause, List *tlist)
{
	AttrNumber *grpColIdx;
	int			numCols = list_length(groupClause);
	int			colno = 0;
	ListCell   *glitem;

	grpColIdx = (AttrNumber *) palloc(sizeof(AttrNumber) * numCols);

	foreach(glitem, groupClause)
	{
		SortGroupClause *groupcl = (SortGroupClause *) lfirst(glitem);
		TargetEntry *tle = get_sortgroupclause_tle(groupcl, tlist);

		grpColIdx[colno++] = tle->resno;
	}

	return grpColIdx;
}

/*
 * grouping_is_sortable - is it possible to implement grouping list by sorting?
 *
 * This is easy since the parser will have included a sortop if one exists.
 */
bool
grouping_is_sortable(List *groupClause)
{
	ListCell   *glitem;

	foreach(glitem, groupClause)
	{
		Node	   *node = lfirst(glitem);

		if (node == NULL)
			continue;

		if (IsA(node, List))
		{
			if (!grouping_is_sortable((List *) node))
				return false;
		}
		else if (IsA(node, GroupingClause))
		{
			if (!grouping_is_sortable(((GroupingClause *) node)->groupsets))
				return false;
		}
		else
		{
			SortGroupClause *groupcl;

			Assert(IsA(node, SortGroupClause));

			groupcl = (SortGroupClause *) node;
			if (!OidIsValid(groupcl->sortop))
				return false;
		}
	}
	return true;
}

/*
 * grouping_is_hashable - is it possible to implement grouping list by hashing?
 *
 * We assume hashing is OK if the equality operators are marked oprcanhash.
 * (If there isn't actually a supporting hash function, the executor will
 * complain at runtime; but this is a misdeclaration of the operator, not
 * a system bug.)
 */
bool
grouping_is_hashable(List *groupClause)
{
	ListCell   *glitem;

	foreach(glitem, groupClause)
	{
		Node	   *node = lfirst(glitem);

		if (node == NULL)
			continue;

		if (IsA(node, List))
		{
			if (!grouping_is_hashable((List *) node))
				return false;
		}
		else if (IsA(node, GroupingClause))
		{
			if (!grouping_is_hashable(((GroupingClause *) node)->groupsets))
				return false;
		}
		else
		{
			SortGroupClause *groupcl = (SortGroupClause *) node;

			if (!op_hashjoinable(groupcl->eqop))
				return false;
		}
	}
	return true;
}


/*
 * Return the largest sortgroupref value in use in the given
 * target list.
 *
 * If include_orderedagg is false, consider only the top-level
 * entries in the target list, i.e., those that might be occur
 * in a groupClause, distinctClause, or sortClause of the Query
 * node that immediately contains the target list.
 *
 * If include_orderedagg is true, also consider AggOrder entries
 * embedded in Aggref nodes within the target list.  Though
 * such entries will only occur in the aggregation sub_tlist
 * (input) they affect sortgroupref numbering for both sub_tlist
 * and tlist (aggregate).
 */
Index maxSortGroupRef(List *targetlist, bool include_orderedagg)
{
	maxSortGroupRef_context context;
	context.maxsgr = 0;
	context.include_orderedagg = include_orderedagg;

	if (targetlist != NIL)
	{
		if ( !IsA(targetlist, List) || !IsA(linitial(targetlist), TargetEntry ) )
			elog(ERROR, "non-targetlist argument supplied");

		maxSortGroupRef_walker((Node*)targetlist, &context);
	}

	return context.maxsgr;
}

bool maxSortGroupRef_walker(Node *node, maxSortGroupRef_context *cxt)
{
	if ( node == NULL )
		return false;

	if ( IsA(node, TargetEntry) )
	{
		TargetEntry *tle = (TargetEntry*)node;
		if ( tle->ressortgroupref > cxt->maxsgr )
			cxt->maxsgr = tle->ressortgroupref;

		return maxSortGroupRef_walker((Node*)tle->expr, cxt);
	}

	/* Aggref nodes don't nest, so we can treat them here without recurring
	 * further.
	 */

	if ( IsA(node, Aggref) )
	{
		Aggref *ref = (Aggref*)node;

		if ( cxt->include_orderedagg )
		{
			ListCell *lc;

			foreach (lc, ref->aggorder)
			{
				SortGroupClause *sort = (SortGroupClause *)lfirst(lc);
				Assert(IsA(sort, SortGroupClause));
				Assert( sort->tleSortGroupRef != 0 );
				if (sort->tleSortGroupRef > cxt->maxsgr )
					cxt->maxsgr = sort->tleSortGroupRef;
			}

		}
		return false;
	}

	return expression_tree_walker(node, maxSortGroupRef_walker, cxt);
}

/**
 * Returns the width of the row by traversing through the
 * target list and adding up the width of each target entry.
 */
int get_row_width(List *tlist)
{
	int width = 0;
	ListCell *plc = NULL;

    foreach(plc, tlist)
    {
    	TargetEntry *pte = (TargetEntry*) lfirst(plc);
	    Expr *pexpr = pte->expr;

	    Assert(NULL != pexpr);

	    Oid oidType = exprType( (Node *) pexpr);
	    int32 iTypmod = exprTypmod( (Node *) pexpr);

	    width += get_typavgwidth(oidType, iTypmod);
	}

    return width;
}
