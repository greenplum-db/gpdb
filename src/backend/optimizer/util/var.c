/*-------------------------------------------------------------------------
 *
 * var.c
 *	  Var node manipulation routines
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/optimizer/util/var.c,v 1.84 2009/02/25 03:30:37 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup.h"
#include "optimizer/clauses.h"
#include "optimizer/prep.h"
#include "optimizer/var.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"
#include "access/sysattr.h"


typedef struct
{
	Relids		varnos;
} pull_varnos_context;

typedef struct
{
	int			var_location;
	int			sublevels_up;
} locate_var_of_level_context;

typedef struct
{
	Index		varno;
	int			varattno;
} contain_var_reference_context;

typedef struct
{
	int			min_varlevel;
} find_minimum_var_level_context;

typedef struct
{
	List	   *varlist;
	bool		includeUpperVars;
} pull_var_clause_context;

typedef struct
{
	PlannerInfo *root;
	int			sublevels_up;
	bool		possible_sublink;		/* could aliases include a SubLink? */
	bool		inserted_sublink;		/* have we inserted a SubLink? */
} flatten_join_alias_vars_context;

static bool pull_varattnos_walker(Node *node, Bitmapset **varattnos);

static bool contain_var_clause_walker(Node *node, void *context);
static bool contain_vars_of_level_walker(Node *node, int *sublevels_up);
static bool locate_var_of_level_walker(Node *node,
						   locate_var_of_level_context *context);
static bool pull_var_clause_walker(Node *node,
					   pull_var_clause_context *context);
static Node *flatten_join_alias_vars_mutator(Node *node,
								flatten_join_alias_vars_context *context);


/*
 * cdb_walk_vars
 *	  Invoke callback function on each Var and/or Aggref node in an expression.
 *    If a callback returns true, no further nodes are visited, and true is
 *    returned.  Otherwise after visiting all nodes, false is returned.
 *
 * Will recurse into sublinks.	Also, may be invoked directly on a Query.
 */
typedef struct Cdb_walk_vars_context
{
    Cdb_walk_vars_callback_Var      	callback_var;
    Cdb_walk_vars_callback_Aggref   	callback_aggref;
    Cdb_walk_vars_callback_CurrentOf    callback_currentof;
    void                           	   *context;
    int                             	sublevelsup;
} Cdb_walk_vars_context;

static bool
cdb_walk_vars_walker(Node *node, void *wvwcontext)
{
    Cdb_walk_vars_context  *ctx = (Cdb_walk_vars_context *)wvwcontext;

	if (node == NULL)
		return false;

    if (IsA(node, Var) &&
        ctx->callback_var != NULL)
		return ctx->callback_var((Var *)node, ctx->context, ctx->sublevelsup);

    if (IsA(node, Aggref) &&
        ctx->callback_aggref != NULL)
        return ctx->callback_aggref((Aggref *)node, ctx->context, ctx->sublevelsup);

    if (IsA(node, CurrentOfExpr) &&
        ctx->callback_currentof != NULL)
        return ctx->callback_currentof((CurrentOfExpr *)node, ctx->context, ctx->sublevelsup);

    if (IsA(node, Query))
	{
		bool    b;

		/* Recurse into subselects */
		ctx->sublevelsup++;
		b = query_tree_walker((Query *)node, cdb_walk_vars_walker, ctx, 0);
		ctx->sublevelsup--;
		return b;
	}
	return expression_tree_walker(node, cdb_walk_vars_walker, ctx);
}                               /* cdb_walk_vars_walker */

bool
cdb_walk_vars(Node                         *node,
              Cdb_walk_vars_callback_Var    callback_var,
              Cdb_walk_vars_callback_Aggref callback_aggref,
              Cdb_walk_vars_callback_CurrentOf callback_currentof,
              void                         *context,
              int                           levelsup)
{
	Cdb_walk_vars_context   ctx;

    ctx.callback_var = callback_var;
    ctx.callback_aggref = callback_aggref;
    ctx.callback_currentof = callback_currentof;
    ctx.context = context;
    ctx.sublevelsup = levelsup;

	/*
	 * Must be prepared to start with a Query or a bare expression tree; if
	 * it's a Query, we don't want to increment levelsdown.
	 */
	return query_or_expression_tree_walker(node, cdb_walk_vars_walker, &ctx, 0);
}                               /* cdb_walk_vars */


/*
 * pull_varnos
 *		Create a set of all the distinct varnos present in a parsetree.
 *		Only varnos that reference level-zero rtable entries are considered.
 *
 * NOTE: this is used on not-yet-planned expressions.  It may therefore find
 * bare SubLinks, and if so it needs to recurse into them to look for uplevel
 * references to the desired rtable level!	But when we find a completed
 * SubPlan, we only need to look at the parameters passed to the subplan.
 */
static bool
pull_varnos_cbVar(Var *var, void *context, int sublevelsup)
{
    pull_varnos_context *ctx = (pull_varnos_context *)context;

	if ((int)var->varlevelsup == sublevelsup)
		ctx->varnos = bms_add_member(ctx->varnos, var->varno);
	return false;
}

static bool
pull_varnos_cbCurrentOf(CurrentOfExpr *expr, void *context, int sublevelsup)
{
	Assert(sublevelsup == 0);
	pull_varnos_context *ctx = (pull_varnos_context *)context;
	ctx->varnos = bms_add_member(ctx->varnos, expr->cvarno);
	return false;
}

static inline Relids
pull_varnos_of_level(Node *node, int levelsup)      /*CDB*/
{
	pull_varnos_context context;

	context.varnos = NULL;
    cdb_walk_vars(node, pull_varnos_cbVar, NULL, pull_varnos_cbCurrentOf, &context, levelsup);
	return context.varnos;
}                               /* pull_varnos_of_level */

Relids
pull_varnos(Node *node)
{
	return pull_varnos_of_level(node, 0);
}

/*
 * pull_varattnos
 *		Find all the distinct attribute numbers present in an expression tree,
 *		and add them to the initial contents of *varattnos.
 *		Only Vars that reference RTE 1 of rtable level zero are considered.
 *
 * Attribute numbers are offset by FirstLowInvalidHeapAttributeNumber so that
 * we can include system attributes (e.g., OID) in the bitmap representation.
 *
 * Currently, this does not support subqueries nor expressions containing
 * references to multiple tables; not needed since it's only applied to
 * index expressions and predicates.
 */
void
pull_varattnos(Node *node, Bitmapset **varattnos)
{
	(void) pull_varattnos_walker(node, varattnos);
}

static bool
pull_varattnos_walker(Node *node, Bitmapset **varattnos)
{
	if (node == NULL)
		return false;
	if (IsA(node, Var))
	{
		Var		   *var = (Var *) node;

		Assert(var->varno == 1);
		*varattnos = bms_add_member(*varattnos,
						 var->varattno - FirstLowInvalidHeapAttributeNumber);
		return false;
	}
	/* Should not find a subquery or subplan */
	Assert(!IsA(node, Query));
	Assert(!is_subplan(node));

	return expression_tree_walker(node, pull_varattnos_walker,
								  (void *) varattnos);
}


/*
 *		contain_var_reference
 *
 *		Detect whether a parsetree contains any references to a specified
 *		attribute of a specified rtable entry.
 *
 * NOTE: this is used on not-yet-planned expressions.  It may therefore find
 * bare SubLinks, and if so it needs to recurse into them to look for uplevel
 * references to the desired rtable entry!	But when we find a completed
 * SubPlan, we only need to look at the parameters passed to the subplan.
 */
static bool
contain_var_reference_cbVar(Var    *var,
							void   *context,
                            int     sublevelsup)
{
    contain_var_reference_context *ctx = (contain_var_reference_context *)context;

    if (var->varno == ctx->varno &&
		var->varattno == ctx->varattno &&
		(int)var->varlevelsup == sublevelsup)
		return true;
	return false;
}

static bool
contain_var_reference(Node *node, int varno, int varattno, int levelsup)
{
	contain_var_reference_context context;

	context.varno = varno;
	context.varattno = varattno;

	return cdb_walk_vars(node, contain_var_reference_cbVar, NULL, NULL, &context, levelsup);
}

bool
contain_ctid_var_reference(Scan *scan)
{
	/* Check if targetlist contains a var node referencing the ctid column */
	bool want_ctid_in_targetlist =
			contain_var_reference((Node *) scan->plan.targetlist,
			scan->scanrelid,
			SelfItemPointerAttributeNumber,
			0);
	/* Check if qual contains a var node referencing the ctid column */
	bool want_ctid_in_qual =
			contain_var_reference((Node *) scan->plan.qual,
			scan->scanrelid,
			SelfItemPointerAttributeNumber,
			0);

	return want_ctid_in_targetlist || want_ctid_in_qual;
}

/*
 * contain_var_clause
 *	  Recursively scan a clause to discover whether it contains any Var nodes
 *	  (of the current query level).
 *
 *	  Returns true if any varnode found.
 *
 * Does not examine subqueries, therefore must only be used after reduction
 * of sublinks to subplans!
 */
bool
contain_var_clause(Node *node)
{
	return contain_var_clause_walker(node, NULL);
}

static bool
contain_var_clause_walker(Node *node, void *context)
{
	if (node == NULL)
		return false;
	if (IsA(node, Var))
	{
		if (((Var *) node)->varlevelsup == 0)
			return true;		/* abort the tree traversal and return true */
		return false;
	}
	if (IsA(node, CurrentOfExpr))
		return true;
	return expression_tree_walker(node, contain_var_clause_walker, context);
}

/*
 * contain_vars_of_level
 *	  Recursively scan a clause to discover whether it contains any Var nodes
 *	  of the specified query level.
 *
 *	  Returns true if any such Var found.
 *
 * Will recurse into sublinks.	Also, may be invoked directly on a Query.
 */
bool
contain_vars_of_level(Node *node, int levelsup)
{
	int			sublevels_up = levelsup;

	return query_or_expression_tree_walker(node,
										   contain_vars_of_level_walker,
										   (void *) &sublevels_up,
										   0);
}

static bool
contain_vars_of_level_walker(Node *node, int *sublevels_up)
{
	if (node == NULL)
		return false;
	if (IsA(node, Var))
	{
		if (((Var *) node)->varlevelsup == *sublevels_up)
			return true;		/* abort tree traversal and return true */
		return false;
	}
	if (IsA(node, CurrentOfExpr))
	{
		if (*sublevels_up == 0)
			return true;
		return false;
	}
	if (IsA(node, Query))
	{
		/* Recurse into subselects */
		bool		result;

		(*sublevels_up)++;
		result = query_tree_walker((Query *) node,
								   contain_vars_of_level_walker,
								   (void *) sublevels_up,
								   0);
		(*sublevels_up)--;
		return result;
	}
	return expression_tree_walker(node,
								  contain_vars_of_level_walker,
								  (void *) sublevels_up);
}

/*
 * locate_var_of_level
 *	  Find the parse location of any Var of the specified query level.
 *
 * Returns -1 if no such Var is in the querytree, or if they all have
 * unknown parse location.  (The former case is probably caller error,
 * but we don't bother to distinguish it from the latter case.)
 *
 * Will recurse into sublinks.  Also, may be invoked directly on a Query.
 *
 * Note: it might seem appropriate to merge this functionality into
 * contain_vars_of_level, but that would complicate that function's API.
 * Currently, the only uses of this function are for error reporting,
 * and so shaving cycles probably isn't very important.
 */
int
locate_var_of_level(Node *node, int levelsup)
{
	locate_var_of_level_context context;

	context.var_location = -1;	/* in case we find nothing */
	context.sublevels_up = levelsup;

	(void) query_or_expression_tree_walker(node,
										   locate_var_of_level_walker,
										   (void *) &context,
										   0);

	return context.var_location;
}

static bool
locate_var_of_level_walker(Node *node,
						   locate_var_of_level_context *context)
{
	if (node == NULL)
		return false;
	if (IsA(node, Var))
	{
		Var		   *var = (Var *) node;

		if (var->varlevelsup == context->sublevels_up &&
			var->location >= 0)
		{
			context->var_location = var->location;
			return true;		/* abort tree traversal and return true */
		}
		return false;
	}
	if (IsA(node, CurrentOfExpr))
	{
		/* since CurrentOfExpr doesn't carry location, nothing we can do */
		return false;
	}
	/* No extra code needed for PlaceHolderVar; just look in contained expr */
	if (IsA(node, Query))
	{
		/* Recurse into subselects */
		bool		result;

		context->sublevels_up++;
		result = query_tree_walker((Query *) node,
								   locate_var_of_level_walker,
								   (void *) context,
								   0);
		context->sublevels_up--;
		return result;
	}
	return expression_tree_walker(node,
								  locate_var_of_level_walker,
								  (void *) context);
}

/*
 * contain_vars_of_level_or_above
 *	  Recursively scan a clause to discover whether it contains any Var or
 *    Aggref nodes of the specified query level or above.  For example,
 *    pass 1 to detect all nonlocal Vars.
 *
 *	  Returns true if any such Var found.
 *
 * Will recurse into sublinks.	Also, may be invoked directly on a Query.
 */
static bool
contain_vars_of_level_or_above_cbVar(Var *var, void *unused, int sublevelsup)
{
	if ((int)var->varlevelsup >= sublevelsup)
		return true;		    /* abort tree traversal and return true */
    return false;
}

static bool
contain_vars_of_level_or_above_cbAggref(Aggref *aggref, void *unused, int sublevelsup)
{
	if ((int)aggref->agglevelsup >= sublevelsup)
        return true;

    /* visit aggregate's args */
	return cdb_walk_vars((Node *)aggref->args,
                         contain_vars_of_level_or_above_cbVar,
                         contain_vars_of_level_or_above_cbAggref,
                         NULL,
                         NULL,
                         sublevelsup);
}

bool
contain_vars_of_level_or_above(Node *node, int levelsup)
{
	return cdb_walk_vars(node,
                         contain_vars_of_level_or_above_cbVar,
                         contain_vars_of_level_or_above_cbAggref,
                         NULL,
                         NULL,
                         levelsup);
}


/*
 * find_minimum_var_level
 *	  Recursively scan a clause to find the lowest variable level it
 *	  contains --- for example, zero is returned if there are any local
 *	  variables, one if there are no local variables but there are
 *	  one-level-up outer references, etc.  Subqueries are scanned to see
 *	  if they possess relevant outer references.  (But any local variables
 *	  within subqueries are not relevant.)
 *
 *	  -1 is returned if the clause has no variables at all.
 *
 * Will recurse into sublinks.	Also, may be invoked directly on a Query.
 */
static bool
find_minimum_var_level_cbVar(Var   *var,
							 void  *context,
                             int    sublevelsup)
{
    find_minimum_var_level_context *ctx = (find_minimum_var_level_context *)context;
    int			varlevelsup = var->varlevelsup;

	/* convert levelsup to frame of reference of original query */
	varlevelsup -= sublevelsup;
	/* ignore local vars of subqueries */
	if (varlevelsup >= 0)
	{
		if (ctx->min_varlevel < 0 ||
			ctx->min_varlevel > varlevelsup)
		{
			ctx->min_varlevel = varlevelsup;

			/*
			 * As soon as we find a local variable, we can abort the tree
			 * traversal, since min_varlevel is then certainly 0.
			 */
			if (varlevelsup == 0)
				return true;
		}
	}
    return false;
}

static bool
find_minimum_var_level_cbAggref(Aggref *aggref,
						        void   *context,
                                int     sublevelsup)
{
	/*
	 * An Aggref must be treated like a Var of its level.  Normally we'd get
	 * the same result from looking at the Vars in the aggregate's argument,
	 * but this fails in the case of a Var-less aggregate call (COUNT(*)).
	 */
    find_minimum_var_level_context *ctx = (find_minimum_var_level_context *)context;
    int			agglevelsup = aggref->agglevelsup;

	/* convert levelsup to frame of reference of original query */
	agglevelsup -= sublevelsup;
	/* ignore local aggs of subqueries */
	if (agglevelsup >= 0)
	{
		if (ctx->min_varlevel < 0 ||
			ctx->min_varlevel > agglevelsup)
		{
			ctx->min_varlevel = agglevelsup;

			/*
			 * As soon as we find a local aggregate, we can abort the tree
			 * traversal, since min_varlevel is then certainly 0.
			 */
			if (agglevelsup == 0)
				return true;
		}
	}

    /* visit aggregate's args */
	return cdb_walk_vars((Node *)aggref->args,
                         find_minimum_var_level_cbVar,
                         find_minimum_var_level_cbAggref,
                         NULL,
                         ctx,
                         sublevelsup);
}

int
find_minimum_var_level(Node *node)
{
	find_minimum_var_level_context context;

	context.min_varlevel = -1;	/* signifies nothing found yet */

	cdb_walk_vars(node,
                  find_minimum_var_level_cbVar,
                  find_minimum_var_level_cbAggref,
                  NULL,
                  &context,
                  0);

	return context.min_varlevel;
}


/*
 * pull_var_clause
 *	  Recursively pulls all var nodes from an expression clause.
 *
 *	  Upper-level vars (with varlevelsup > 0) are included only
 *	  if includeUpperVars is true.	Most callers probably want
 *	  to ignore upper-level vars.
 *
 *	  Returns list of varnodes found.  Note the varnodes themselves are not
 *	  copied, only referenced.
 *
 * Does not examine subqueries, therefore must only be used after reduction
 * of sublinks to subplans!
 */
List *
pull_var_clause(Node *node, bool includeUpperVars)
{
	pull_var_clause_context context;

	context.varlist = NIL;
	context.includeUpperVars = includeUpperVars;

	pull_var_clause_walker(node, &context);
	return context.varlist;
}

static bool
pull_var_clause_walker(Node *node, pull_var_clause_context *context)
{
	if (node == NULL)
		return false;
	if (IsA(node, Var))
	{
		if (((Var *) node)->varlevelsup == 0 || context->includeUpperVars)
			context->varlist = lappend(context->varlist, node);
		return false;
	}
	return expression_tree_walker(node, pull_var_clause_walker,
								  (void *) context);
}


/*
 * flatten_join_alias_vars
 *	  Replace Vars that reference JOIN outputs with references to the original
 *	  relation variables instead.  This allows quals involving such vars to be
 *	  pushed down.	Whole-row Vars that reference JOIN relations are expanded
 *	  into RowExpr constructs that name the individual output Vars.  This
 *	  is necessary since we will not scan the JOIN as a base relation, which
 *	  is the only way that the executor can directly handle whole-row Vars.
 *
 * If a JOIN contains sub-selects that have been flattened, its join alias
 * entries might now be arbitrary expressions, not just Vars.  This affects
 * this function in one important way: we might find ourselves inserting
 * SubLink expressions into subqueries, and we must make sure that their
 * Query.hasSubLinks fields get set to TRUE if so.  If there are any
 * SubLinks in the join alias lists, the outer Query should already have
 * hasSubLinks = TRUE, so this is only relevant to un-flattened subqueries.
 *
 * NOTE: this is used on not-yet-planned expressions.  We do not expect it
 * to be applied directly to a Query node.
 */
Node *
flatten_join_alias_vars(PlannerInfo *root, Node *node)
{
	flatten_join_alias_vars_context context;

	context.root = root;
	context.sublevels_up = 0;
	/* flag whether join aliases could possibly contain SubLinks */
	context.possible_sublink = root->parse->hasSubLinks;
	/* if hasSubLinks is already true, no need to work hard */
	context.inserted_sublink = root->parse->hasSubLinks;

	return flatten_join_alias_vars_mutator(node, &context);
}

static Node *
flatten_join_alias_vars_mutator(Node *node,
								flatten_join_alias_vars_context *context)
{
	if (node == NULL)
		return NULL;
	if (IsA(node, Var))
	{
		Var		   *var = (Var *) node;
		RangeTblEntry *rte;
		Node	   *newvar;

		/* No change unless Var belongs to a JOIN of the target level */
		if ((int)var->varlevelsup != context->sublevels_up)
			return node;		/* no need to copy, really */
		rte = rt_fetch(var->varno, context->root->parse->rtable);
		if (rte->rtekind != RTE_JOIN)
			return node;
		if (var->varattno == InvalidAttrNumber)
		{
			/* Must expand whole-row reference */
			RowExpr    *rowexpr;
			List	   *fields = NIL;
			AttrNumber	attnum;
			ListCell   *l;

			attnum = 0;
			foreach(l, rte->joinaliasvars)
			{
				newvar = (Node *) lfirst(l);
				attnum++;
				/* Ignore dropped columns */
				if (IsA(newvar, Const))
					continue;

				/*
				 * If we are expanding an alias carried down from an upper
				 * query, must adjust its varlevelsup fields.
				 */
				if (context->sublevels_up != 0)
				{
					newvar = copyObject(newvar);
					IncrementVarSublevelsUp(newvar, context->sublevels_up, 0);
				}
				/* Recurse in case join input is itself a join */
				/* (also takes care of setting inserted_sublink if needed) */
				newvar = flatten_join_alias_vars_mutator(newvar, context);
				fields = lappend(fields, newvar);
			}
			rowexpr = makeNode(RowExpr);
			rowexpr->args = fields;
			rowexpr->row_typeid = var->vartype;
			rowexpr->row_format = COERCE_IMPLICIT_CAST;

			return (Node *) rowexpr;
		}

		/* Expand join alias reference */
		Assert(var->varattno > 0);
		newvar = (Node *) list_nth(rte->joinaliasvars, var->varattno - 1);

		/*
		 * If we are expanding an alias carried down from an upper query, must
		 * adjust its varlevelsup fields.
		 */
		if (context->sublevels_up != 0)
		{
			newvar = copyObject(newvar);
			IncrementVarSublevelsUp(newvar, context->sublevels_up, 0);
		}

		/* Recurse in case join input is itself a join */
		newvar = flatten_join_alias_vars_mutator(newvar, context);

		/* Detect if we are adding a sublink to query */
		if (context->possible_sublink && !context->inserted_sublink)
			context->inserted_sublink = checkExprHasSubLink(newvar);

		return newvar;
	}

	if (IsA(node, Query))
	{
		/* Recurse into RTE subquery or not-yet-planned sublink subquery */
		Query	   *newnode;
		bool		save_inserted_sublink;

		context->sublevels_up++;
		save_inserted_sublink = context->inserted_sublink;
		context->inserted_sublink = ((Query *) node)->hasSubLinks;
		newnode = query_tree_mutator((Query *) node,
									 flatten_join_alias_vars_mutator,
									 (void *) context,
									 QTW_IGNORE_JOINALIASES);
		newnode->hasSubLinks |= context->inserted_sublink;
		context->inserted_sublink = save_inserted_sublink;
		context->sublevels_up--;
		return (Node *) newnode;
	}
	/* Already-planned tree not supported */
	Assert(!is_subplan(node));

	return expression_tree_mutator(node, flatten_join_alias_vars_mutator,
								   (void *) context);
}
