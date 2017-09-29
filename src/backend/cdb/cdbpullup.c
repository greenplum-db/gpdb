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

#include "nodes/makefuncs.h"	/* makeVar() */
#include "nodes/plannodes.h"	/* Plan */
#include "optimizer/clauses.h"	/* expression_tree_walker/mutator */
#include "optimizer/tlist.h"	/* tlist_member() */
#include "parser/parse_expr.h"	/* exprType() and exprTypmod() */
#include "parser/parsetree.h"	/* get_tle_by_resno() */

#include "cdb/cdbpullup.h"		/* me */


/*
 * cdbpullup_colIdx
 *
 * Given a [potentially] projecting Plan operator and a vector
 * of attribute numbers in the plan's single subplan, create a
 * vector of attribute numbers in the plan's targetlist.
 *
 * Parameters:
 *      plan -> the Plan node
 *      subplan -> the Plan node's subplan
 *      numCols = number of colIdx array elements
 *      subplanColIdx (IN) -> array [0..numCols-1] of AttrNumber, designating
 *          entries in the targetlist of the Plan operator's subplan.
 *     *pProjectedColIdx (OUT) -> array [0..n-1] of AttrNumber, palloc'd
 *          by this function, where n is the function's return value; or
 *          NULL when n == 0.
 *
 * Returns the number of leading subplanColIdx entries for which matching
 * Var nodes were found in the plan's targetlist.
 */
int
cdbpullup_colIdx(struct Plan *plan,
				 struct Plan *subplan,
				 int numCols,
				 AttrNumber *subplanColIdx,
				 AttrNumber **pProjectedColIdx)
{
	TargetEntry *tle;
	int			i;
	bool		useExecutorVarFormat;

	Assert(pProjectedColIdx);
	*pProjectedColIdx = NULL;

	/*
	 * Look at an arbitrary Var node in the upper Plan node's targetlist to
	 * see if it has been adjusted by set_plan_references().
	 *
	 * Before set_plan_references(), if Var nodes are equal(), then they are
	 * equivalent in meaning, more or less.  Afterwards that is no longer the
	 * case; each Var node is implicitly tied to a particular evaluation
	 * context (i.e. a certain Plan node's targetlist), which means expr trees
	 * for different contexts are incomparable.
	 */

	useExecutorVarFormat = cdbpullup_exprHasSubplanRef((Expr *) plan->targetlist);

	/*
	 * Translate the column index array.
	 */
	for (i = 0; i < numCols; i++)
	{
		Index		kidTargetIdx = subplanColIdx[i];

		/*
		 * Search our targetlist for a Var that references the specified item
		 * of the subplan's targetlist.  All Var nodes (at least, all that we
		 * expect to encounter here) are converted to this form by
		 * set_plan_references() at the end of optimization.
		 */
		if (useExecutorVarFormat)
			tle = cdbpullup_findSubplanRefInTargetList(kidTargetIdx, plan->targetlist);

		/*
		 * Iff set_plan_references() hasn't been called yet, a different
		 * procedure is necessary: search our own targetlist for a copy of the
		 * designated expr from the subplan targetlist.
		 */
		else
		{
			TargetEntry *subtle;

			/* Get subplan's targetlist entry. */
			subtle = get_tle_by_resno(subplan->targetlist, kidTargetIdx);
			Assert(subtle);

			/* Look for matching expr in the given plan's targetlist. */
			tle = tlist_member_ignore_relabel((Node *) subtle->expr, plan->targetlist);
		}

		/* Quit if the plan's projection doesn't include this column. */
		if (!tle)
			break;

		if (i == 0)
			*pProjectedColIdx = palloc(numCols * sizeof((*pProjectedColIdx)[0]));

		(*pProjectedColIdx)[i] = tle->resno;
	}

	return i;
}								/* cdbpullup_colIdx */


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
 * When calling this function on an expr which has NOT yet been transformed
 * by set_plan_references(), newvarno should be the RTE index assigned to
 * the result of the projection.
 *
 * When calling this function on an expr which HAS been transformed by
 * set_plan_references(), newvarno should usually be OUTER; or 0 if the
 * expr is to be used in the targetlist of an Agg or Group node.
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

			/* After set_plan_references(), search on varattno only. */
			if (var->varno == OUTER ||
				var->varno == INNER ||
				var->varno == 0)
				tle = cdbpullup_findSubplanRefInTargetList(var->varattno,
														   ctx->targetlist);
			/* Before set_plan_references(), search for exact match. */
			else
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
				if (equal(tlistexpr, var))
					break;
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
		Assert(exprType(newnode) == exprType(node) &&
			   exprTypmod(newnode) == exprTypmod(node));

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
		Assert(exprType(newnode) == exprType(node) &&
			   exprTypmod(newnode) == exprTypmod(node));

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
 * cdbpullup_exprHasSubplanRef
 *
 * Returns true if the expr's first Var is a reference to an item in the
 * targetlist of the associated Plan node's subplan.  If so, it can be
 * assumed that the Plan node and associated exprs have been processed
 * by set_plan_references(), and its Var nodes are in executor format.
 *
 * Returns false otherwise, which implies no conclusion about whether or
 * not set_plan_references() has been done.
 *
 * Note that a Var node that belongs to a Scan operator and refers to the
 * Scan's source relation or index, doesn't have its varno changed by
 * set_plan_references() to OUTER/INNER/0.  Think twice about using this
 * unless you know that the expr comes from an upper Plan node.
 */

/* Find any Var in an expr */
static bool
findAnyVar_walker(Node *node, void *ppVar)
{
	if (!node)
		return false;
	if (IsA(node, Var))
	{
		*(Var **) ppVar = (Var *) node;
		return true;
	}
	return expression_tree_walker(node, findAnyVar_walker, ppVar);
}

bool
cdbpullup_exprHasSubplanRef(Expr *expr)
{
	Var		   *var;

	if (!findAnyVar_walker((Node *) expr, &var))
		return false;

	return var->varno == OUTER ||
		var->varno == INNER ||
		var->varno == 0;
}								/* cdbpullup_exprHasSubplanRef */


/*
 * cdbpullup_findPathKeyExprInTargetList
 *
 * Searches the equivalence class of a given PathKey for a member that
 * uses no rels outside the 'relids' set, and either is a member of
 * 'targetlist', or uses no Vars that are not in 'targetlist'.
 *
 * If found, returns the chosen member's expression, otherwise returns
 *  NULL
 *
 * 'item' is a PathKey.
 * 'targetlist' is a List of TargetEntry or merely a List of Expr.
 *
 * NB: We ignore the presence or absence of a RelabelType node atop either
 * expr in determining whether a PathKey expr matches a targetlist expr.
 *
 * (A RelabelType node might have been placed atop a PathKey's expr to
 * match its type to the sortop's input operand type, when the types are
 * binary compatible but not identical... such as VARCHAR and TEXT.  The
 * RelabelType node merely documents the representational equivalence but
 * does not affect the semantics.  A RelabelType node might also be found
 * atop an argument of a function or operator, but generally not atop a
 * targetlist expr.)
 */
Expr *
cdbpullup_findPathKeyExprInTargetList(PathKey *item, List *targetlist)
{
	ListCell   *lc;
	EquivalenceClass *eclass = item->pk_eclass;

	foreach(lc, eclass->ec_members)
	{
		EquivalenceMember *em = (EquivalenceMember *) lfirst(lc);
		Expr	   *key = (Expr *) em->em_expr;

		/* A constant is OK regardless of the target list */
		if (em->em_is_const)
			return key;

		if (targetlist)
		{
			/* Ignore possible RelabelType node atop the PathKey expr. */
			if (IsA(key, RelabelType))
				key = ((RelabelType *) key)->arg;

			/* Check if targetlist is a List of TargetEntry */
			if (IsA(linitial(targetlist), TargetEntry))
			{
				TargetEntry *tle;

				tle = tlist_member_ignore_relabel((Node *) key, targetlist);
				if (tle)
					return key;
			}
			/* Planner's RelOptInfo targetlists don't have TargetEntry nodes */
			else
			{
				ListCell   *tcell;

				foreach(tcell, targetlist)
				{
					Expr	   *expr = (Expr *) lfirst(tcell);

					if (IsA(expr, RelabelType))
						expr = ((RelabelType *) expr)->arg;

					if (equal(expr, key))
						return key;
				}
			}

			/* Return this item if all referenced Vars are in targetlist. */
			if (!IsA(key, Var) &&
				!cdbpullup_missingVarWalker((Node *) key, targetlist))
			{
				return key;
			}
		}
	}

	return NULL;
}


/*
 * cdbpullup_findSubplanRefInTargetList
 *
 * Given a targetlist, returns ptr to first TargetEntry whose expr is a
 * Var node having the specified varattno, and having its varno in executor
 * format (varno is OUTER, INNER, or 0) as set by set_plan_references().
 * Returns NULL if no such TargetEntry is found.
 */
TargetEntry *
cdbpullup_findSubplanRefInTargetList(AttrNumber varattno, List *targetlist)
{
	ListCell   *cell;
	TargetEntry *tle;
	Var		   *var;

	foreach(cell, targetlist)
	{
		tle = (TargetEntry *) lfirst(cell);
		if (IsA(tle->expr, Var))
		{
			var = (Var *) tle->expr;
			if (var->varattno == varattno)
			{
				if (var->varno == OUTER ||
					var->varno == INNER ||
					var->varno == 0)
					return tle;
			}
		}
	}
	return NULL;
}								/* cdbpullup_findSubplanRefInTargetList */


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
bool
cdbpullup_missingVarWalker(Node *node, void *targetlist)
{
	if (!node)
		return false;

	if (IsA(node, Var))
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


/*
 * cdbpullup_targetentry
 *
 * Given a TargetEntry from a subplan's targetlist, this function returns a
 * new TargetEntry that can be used to pull the result value up into the
 * parent plan's projection.
 *
 * Parameters:
 *      subplan_targetentry is an item in the targetlist of the subplan S.
 *      newresno is the attribute number of the new TargetEntry (its
 *          position in P's targetlist).
 *      newvarno should be OUTER; except it should be 0 if P is an Agg or
 *          Group node.  It is ignored if useExecutorVarFormat is false.
 *      useExecutorVarFormat must be true if called after optimization,
 *          when set_plan_references() has been called and Var nodes have
 *          been adjusted to refer to items in the subplan's targetlist.
 *          It must be false before the end of optimization, when Var
 *          nodes are still in parser format, referring to RTE items.
 */
TargetEntry *
cdbpullup_targetentry(TargetEntry *subplan_targetentry,
					  AttrNumber newresno,
					  Index newvarno,
					  bool useExecutorVarFormat)
{
	TargetEntry *kidtle = subplan_targetentry;
	TargetEntry *newtle;

	/* After optimization, a Var's referent depends on the owning Plan node. */
	if (useExecutorVarFormat)
	{
		/* Copy the subplan's TargetEntry, without its expr. */
		newtle = flatCopyTargetEntry(kidtle);

		/* Make a Var node referencing the subplan's targetlist entry. */
		newtle->expr = cdbpullup_make_expr(newvarno,
										   kidtle->resno,
										   kidtle->expr,
										   true);
	}

	/* Until then, a Var's referent is the same anywhere in a Query. */
	else
		newtle = copyObject(kidtle);

	newtle->resno = newresno;
	return newtle;
}								/* cdbpullup_targetentry */


/*
 * cdbpullup_targetlist
 *
 * Return a targetlist that can be attached to a parent plan yielding
 * the same projection as the given subplan.
 */
List *
cdbpullup_targetlist(struct Plan *subplan, bool useExecutorVarFormat)
{
	ListCell   *cell;
	List	   *targetlist = NIL;
	AttrNumber	resno = 1;

	foreach(cell, subplan->targetlist)
	{
		TargetEntry *kidtle = (TargetEntry *) lfirst(cell);
		TargetEntry *newtle = cdbpullup_targetentry(kidtle,
													resno++,
													OUTER,
													useExecutorVarFormat);

		targetlist = lappend(targetlist, newtle);
	}

	return targetlist;
}								/* cdbpullup_targetlist */
