/*-------------------------------------------------------------------------
 *
 * cdbpullup.c
 *    Provides routines supporting plan tree manipulation.
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbpullup.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/hash.h"		/* HashEqualStrategyNumber */
#include "nodes/makefuncs.h"	/* makeVar() */
#include "nodes/plannodes.h"	/* Plan */
#include "optimizer/clauses.h"	/* expression_tree_walker/mutator */
#include "optimizer/tlist.h"	/* tlist_member() */
#include "nodes/nodeFuncs.h"	/* exprType() and exprTypmod() */
#include "parser/parsetree.h"	/* get_tle_by_resno() */
#include "utils/lsyscache.h"	/* get_opfamily_member() */

#include "cdb/cdbpullup.h"		/* me */


static bool cdbpullup_missingVarWalker(Node *node, void *targetlist);

/*
 * cdbpullup_expr
 *
 * Suppose there is a Plan node 'P' whose projection is defined by
 * a targetlist 'TL', and which has subplan 'S'.  Given TL and an
 * expr 'X0' whose Var nodes reference the result columns of S, this
 * function returns a new expr 'X1' that is a copy of X0 with Var nodes
 * adjusted to reference the columns of S after their passage through P.
 *
 * Parameters:
 *      expr -> X0, the expr in terms of the subplan S's targetlist
 *      targetlist -> TL (a List of TargetEntry), the plan P's targetlist
 *              (or can be a List of Expr)
 *      newvarlist -> an optional List of Expr, in 1-1 correspondence with
 *              targetlist.  If specified, instead of creating a Var node to
 *              reference a targetlist item, we plug in a copy of the
 *              corresponding newvarlist item.
 *      newvarno = varno to be used in new Var nodes.  Ignored if a non-NULL
 *              newvarlist is given.
 *
 * This function cannot be used set_plan_references(). newvarno should be the
 * RTE index assigned to the result of the projection.
 *
 * At present this function doesn't support pull-up from a subquery into a
 * containing query: there is no provision for adjusting the varlevelsup
 * field in Var nodes for outer references.  This could be added if needed.
 *
 * Returns X1, the expr recast in terms of the given targetlist; or
 * NULL if X0 references a column of S that is not projected in TL.
 */
struct pullUpExpr_context
{
	List	   *targetlist;
	List	   *newvarlist;
	Index		newvarno;
	Var		   *notfound;
};

static Node *
pullUpExpr_mutator(Node *node, void *context)
{
	struct pullUpExpr_context *ctx = (struct pullUpExpr_context *) context;
	TargetEntry *tle;
	Node	   *newnode;
	Var		   *var;

	if (!node ||
		ctx->notfound)
		return NULL;

	/* Pull up Var. */
	if (IsA(node, Var))
	{
		var = (Var *) node;

		/* Outer reference?  Just copy it. */
		if (var->varlevelsup > 0)
			return (Node *) copyObject(var);

		if (!ctx->targetlist)
			goto fail;

		/* Is targetlist a List of TargetEntry?  (Plan nodes use this format) */
		if (IsA(linitial(ctx->targetlist), TargetEntry))
		{
			tle = tlist_member((Node *) var, ctx->targetlist);

			/* Fail if P's result does not include this column. */
			if (!tle)
				goto fail;

			/* Substitute the corresponding entry from newvarlist, if given. */
			if (ctx->newvarlist)
				newnode = (Node *) copyObject(list_nth(ctx->newvarlist,
													   tle->resno - 1));

			/* Substitute a Var node referencing the targetlist entry. */
			else
				newnode = (Node *) cdbpullup_make_expr(ctx->newvarno,
													   tle->resno,
													   (Expr *) var,
													   true);
		}

		/* Planner's RelOptInfo targetlists don't have TargetEntry nodes. */
		else
		{
			ListCell   *cell;
			Expr	   *tlistexpr = NULL;
			AttrNumber	targetattno = 1;

			foreach(cell, ctx->targetlist)
			{
				tlistexpr = (Expr *) lfirst(cell);

				/*
				 * We don't use equal(), because we want to ignore typmod.
				 * A projection sometimes loses typmod, and that's OK.
				 */
				if (IsA(tlistexpr, Var))
				{
					Var		   *tlistvar = (Var *) tlistexpr;

					if (var->varno == tlistvar->varno &&
						var->varattno == tlistvar->varattno &&
						var->varlevelsup == tlistvar->varlevelsup)
						break;
				}
				targetattno++;
			}
			if (!cell)
				goto fail;

			/* Substitute the corresponding entry from newvarlist, if given. */
			if (ctx->newvarlist)
				newnode = (Node *) copyObject(list_nth(ctx->newvarlist,
													   targetattno - 1));

			/* Substitute a Var node referencing the targetlist entry. */
			else
				newnode = (Node *) cdbpullup_make_expr(ctx->newvarno,
													   targetattno,
													   tlistexpr, true);
		}

		/* Make sure we haven't inadvertently changed the data type. */
		Assert(exprType(newnode) == exprType(node));

		return newnode;
	}

	/* The whole expr might be in the targetlist of a Plan node. */
	else if (!IsA(node, List) &&
			 ctx->targetlist &&
			 IsA(linitial(ctx->targetlist), TargetEntry) &&
			 NULL != (tle = tlist_member(node, ctx->targetlist)))
	{
		/* Substitute the corresponding entry from newvarlist, if given. */
		if (ctx->newvarlist)
			newnode = (Node *) copyObject(list_nth(ctx->newvarlist,
												   tle->resno - 1));

		/* Replace expr with a Var node referencing the targetlist entry. */
		else
			newnode = (Node *) cdbpullup_make_expr(ctx->newvarno,
												   tle->resno,
												   tle->expr, true);

		/* Make sure we haven't inadvertently changed the data type. */
		Assert(exprType(newnode) == exprType(node));

		return newnode;
	}

	return expression_tree_mutator(node, pullUpExpr_mutator, context);

	/* Fail if P's result does not include a referenced column. */
fail:
	ctx->notfound = var;
	return NULL;
}								/* pullUpExpr_mutator */

Expr *
cdbpullup_expr(Expr *expr, List *targetlist, List *newvarlist, Index newvarno)
{
	Expr	   *newexpr;
	struct pullUpExpr_context ctx;

	Assert(!targetlist || IsA(targetlist, List));
	Assert(!newvarlist || list_length(newvarlist) == list_length(targetlist));

	ctx.targetlist = targetlist;
	ctx.newvarlist = newvarlist;
	ctx.newvarno = newvarno;
	ctx.notfound = NULL;

	newexpr = (Expr *) pullUpExpr_mutator((Node *) expr, &ctx);

	if (ctx.notfound)
		newexpr = NULL;

	return newexpr;
}								/* cdbpullup_expr */

/*
 * cdbpullup_findEclassInTargetList
 *
 * Searches the given equivalence class for a member that uses no rels
 * outside the 'relids' set, and either is a member of 'targetlist', or
 * uses no Vars that are not in 'targetlist'. Furthermore, if
 * 'hashOpFamily' is valid, the member must be hashable using that hash
 * operator family.
 *
 * If found, returns the chosen member's expression, otherwise returns
 * NULL.
 *
 * NB: We ignore the presence or absence of a RelabelType node atop either
 * expr in determining whether an EC member expr matches a targetlist expr.
 *
 * (A RelabelType node might have been placed atop an EC member's expr to
 * match its type to the sortop's input operand type, when the types are
 * binary compatible but not identical... such as VARCHAR and TEXT.  The
 * RelabelType node merely documents the representational equivalence but
 * does not affect the semantics.  A RelabelType node might also be found
 * atop an argument of a function or operator, but generally not atop a
 * targetlist expr.)
 */
Expr *
cdbpullup_findEclassInTargetList(EquivalenceClass *eclass, List *targetlist,
								 Oid hashOpFamily)
{
	ListCell   *lc;

	foreach(lc, eclass->ec_members)
	{
		EquivalenceMember *em = (EquivalenceMember *) lfirst(lc);
		Expr	   *key = (Expr *) em->em_expr;
		ListCell *lc_tle;

		if (OidIsValid(hashOpFamily) &&
			!get_opfamily_member(hashOpFamily, em->em_datatype, em->em_datatype,
								 HTEqualStrategyNumber))
			continue;

		/* A constant is OK regardless of the target list */
		if (em->em_is_const)
			return key;

		/*-------
		 * Try to find this EC member in the target list.
		 *
		 * We do the search in a very lenient way:
		 *
		 * 1. Ignore RelabelType nodes on top of both sides, like
		 *    tlist_member_ignore_relabel() does.
		 * 2. Ignore varnoold/varoattno fields in Var nodes, like
		 *    tlist_member_match_var() does.
		 * 3. Also Accept "naked" targetlists, without TargetEntry nodes
		 *
		 * Unfortunately, neither tlist_member_ignore_relabel() nor
		 * tlist_member_match_var() does exactly what we need.
		 *-------
		 */
		while (IsA(key, RelabelType))
			key = (Expr *) ((RelabelType *) key)->arg;

		foreach(lc_tle, targetlist)
		{
			Node	   *tlexpr = lfirst(lc_tle);
			Node	   *naked_tlexpr;

			/*
			 * Check if targetlist is a List of TargetEntry. (Planner's
			 * RelOptInfo targetlists don't have TargetEntry nodes.)
			 */
			if (IsA(tlexpr, TargetEntry))
				tlexpr = (Node *) ((TargetEntry *) tlexpr)->expr;

			/* ignore RelabelType nodes on both sides */
			naked_tlexpr = tlexpr;
			while (naked_tlexpr && IsA(naked_tlexpr, RelabelType))
				naked_tlexpr = (Node *) ((RelabelType *) naked_tlexpr)->arg;

			if (IsA(key, Var))
			{
				if (IsA(naked_tlexpr, Var))
				{
					Var		   *keyvar = (Var *) key;
					Var		   *tlvar = (Var *) naked_tlexpr;

					if (keyvar->varno == tlvar->varno &&
						keyvar->varattno == tlvar->varattno &&
						keyvar->varlevelsup == tlvar->varlevelsup)
						return (Expr *) tlexpr;
				}
			}
			else
			{
				if (equal(naked_tlexpr, key))
					return (Expr *) tlexpr;
			}
		}

		/* Return this item if all referenced Vars are in targetlist. */
		if (!IsA(key, Var) &&
			!cdbpullup_missingVarWalker((Node *) key, targetlist))
		{
			return key;
		}
	}

	return NULL;
}

/*
 * cdbpullup_truncatePathKeysForTargetList
 *
 * Truncate a list of pathkeys to only those that can be evaluated
 * using the given target list.
 */
List *
cdbpullup_truncatePathKeysForTargetList(List *pathkeys, List *targetlist)
{
	List	   *new_pathkeys = NIL;
	ListCell   *lc;

	foreach(lc, pathkeys)
	{
		PathKey	   *pk = (PathKey *) lfirst(lc);

		if (!cdbpullup_findEclassInTargetList(pk->pk_eclass, targetlist, InvalidOid))
			break;

		new_pathkeys = lappend(new_pathkeys, pk);
	}

	return new_pathkeys;
}

/*
 * cdbpullup_isExprCoveredByTargetlist
 *
 * Returns true if 'expr' is in 'targetlist', or if 'expr' contains no
 * Var node of the current query level that is not in 'targetlist'.
 *
 * If 'expr' is a List, returns false if the above condition is false for
 * some member of the list.
 *
 * 'targetlist' is a List of TargetEntry.
 *
 * NB:  A Var in the expr is considered as matching a Var in the targetlist
 * without regard for whether or not there is a RelabelType node atop the
 * targetlist Var.
 *
 * See also: cdbpullup_missing_var_walker
 */
bool
cdbpullup_isExprCoveredByTargetlist(Expr *expr, List *targetlist)
{
	ListCell   *cell;

	/* List of Expr?  Verify that all items are covered. */
	if (IsA(expr, List))
	{
		foreach(cell, (List *) expr)
		{
			Node	   *item = (Node *) lfirst(cell);

			/* The whole expr or all of its Vars must be in targetlist. */
			if (!tlist_member_ignore_relabel(item, targetlist) &&
				cdbpullup_missingVarWalker(item, targetlist))
				return false;
		}
	}

	/* The whole expr or all of its Vars must be in targetlist. */
	else if (!tlist_member_ignore_relabel((Node *) expr, targetlist) &&
			 cdbpullup_missingVarWalker((Node *) expr, targetlist))
		return false;

	/* expr is evaluable on rows projected thru targetlist */
	return true;
}								/* cdbpullup_isExprCoveredByTlist */


/*
 * cdbpullup_make_var
 *
 * Returns a new Var node with given 'varno' and 'varattno', and varlevelsup=0.
 *
 * The caller must provide an 'oldexpr' from which we obtain the vartype and
 * vartypmod for the new Var node.  If 'oldexpr' is a Var node, all fields are
 * copied except for varno, varattno and varlevelsup.
 *
 * The parameter modifyOld determines if varnoold and varoattno are modified or
 * not. Rule of thumb is to use modifyOld = false if called before setrefs.
 *
 * Copying an existing Var node preserves its varnoold and varoattno fields,
 * which are used by EXPLAIN to display the table and column name.
 * Also these fields are tested by equal(), so they may need to be set
 * correctly for successful lookups by list_member(), tlist_member(),
 * make_canonical_pathkey(), etc.
 */
Expr *
cdbpullup_make_expr(Index varno, AttrNumber varattno, Expr *oldexpr, bool modifyOld)
{
	Assert(oldexpr);

	/* If caller provided an old Var node, copy and modify it. */
	if (IsA(oldexpr, Var))
	{
		Var		   *var = (Var *) copyObject(oldexpr);

		var->varno = varno;
		var->varattno = varattno;
		if (modifyOld)
		{
			var->varoattno = varattno;
			var->varnoold = varno;
		}
		var->varlevelsup = 0;
		return (Expr *) var;
	}
	else if (IsA(oldexpr, Const))
	{
		Const	   *constExpr = copyObject(oldexpr);

		return (Expr *) constExpr;
	}
	/* Make a new Var node. */
	else
	{
		Var		   *var = NULL;

		Assert(!IsA(oldexpr, Const));
		Assert(!IsA(oldexpr, Var));
		var = makeVar(varno,
					  varattno,
					  exprType((Node *) oldexpr),
					  exprTypmod((Node *) oldexpr),
					  exprCollation((Node *) oldexpr),
					  0);
		var->varnoold = varno;	/* assuming it wasn't ever a plain Var */
		var->varoattno = varattno;
		return (Expr *) var;
	}
}								/* cdbpullup_make_var */


/*
 * cdbpullup_missingVarWalker
 *
 * Returns true if some Var in expr is not in targetlist.
 * 'targetlist' is either a List of TargetEntry, or a plain List of Expr.
 *
 * NB:  A Var in the expr is considered as matching a Var in the targetlist
 * without regard for whether or not there is a RelabelType node atop the
 * targetlist Var.
 *
 * See also: cdbpullup_isExprCoveredByTargetlist
 */
static bool
cdbpullup_missingVarWalker(Node *node, void *targetlist)
{
	if (!node)
		return false;

	/*
	 * Should also consider PlaceHolderVar in the targetlist.
	 * See github issue: https://github.com/greenplum-db/gpdb/issues/10315
	 */
	if (IsA(node, Var) || IsA(node, PlaceHolderVar))
	{
		if (!targetlist)
			return true;

		/* is targetlist a List of TargetEntry? */
		if (IsA(linitial(targetlist), TargetEntry))
		{
			if (tlist_member_ignore_relabel(node, (List *) targetlist))
				return false;	/* this Var ok - go on to check rest of expr */
		}

		/* targetlist must be a List of Expr */
		else if (list_member((List *) targetlist, node))
			return false;		/* this Var ok - go on to check rest of expr */

		return true;			/* Var is not in targetlist - quit the walk */
	}

	return expression_tree_walker(node, cdbpullup_missingVarWalker, targetlist);
}								/* cdbpullup_missingVarWalker */
