/*-------------------------------------------------------------------------
 *
 * transform.c
 *	  This file contains methods to transform the query tree
 *
 * Portions Copyright (c) 2011, EMC Greenplum
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/plan/transform.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "nodes/parsenodes.h"
#include "nodes/makefuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/transform.h"
#include "optimizer/var.h"
#include "utils/lsyscache.h"
#include "catalog/pg_proc.h"
#include "catalog/namespace.h"
#include "parser/parse_oper.h"
#include "parser/parse_coerce.h"
#include "lib/stringinfo.h"
#include "catalog/pg_operator.h"
#include "utils/fmgroids.h"

/**
 * Static declarations
 */
static Node* normalize_query_jointree(Node *node);
static Node *normalize_query_jointree_mutator(Node *node, void *context);
static Node *replace_sirv_functions_mutator(Node *node, void *context);
static void replace_sirvf_tle(Query *query, int tleOffset);
static void replace_sirvf_rte(Query *query, int rteIndex);
static Node *replace_sirvf_tle_expr_mutator(Node *node, void *context);
static SubLink *make_sirvf_subselect(FuncExpr *fe);
static Query *make_sirvf_subquery(FuncExpr *fe);
static bool safe_to_replace_sirvf_tle(Query *query);
static bool safe_to_replace_sirvf_rte(Query *query);
static void wrap_vars_with_fieldselect(List *targetlist, int varno, Oid newvartype);

/**
 * Preprocess query structure for consumption by the optimizer
 */
Query *
preprocess_query_optimizer(PlannerGlobal *glob, Query *query, ParamListInfo boundParams)
{
#ifdef USE_ASSERT_CHECKING
	Query *qcopy = (Query *) copyObject(query);
#endif

	/* fold all constant expressions */
	Query *res = fold_constants(glob, query, boundParams, GPOPT_MAX_FOLDED_CONSTANT_SIZE);

#ifdef USE_ASSERT_CHECKING
	Assert(equal(qcopy, query) && "Preprocessing should not modify original query object");
#endif

	return res;

}

/**
 * Normalize query before planning.
 */
Query *normalize_query(Query *query)
{
#ifdef USE_ASSERT_CHECKING
	Query *qcopy = (Query *) copyObject(query);
#endif

	/**
	 * Normalize the jointree
	 */
	Query *res = (Query *) normalize_query_jointree_mutator((Node *) query, NULL);

	/**
	 * MPP-12635 Replace all instances of single row returning volatile (sirv) functions
	 */
	res = (Query *) replace_sirv_functions_mutator((Node *) res, NULL);

#ifdef USE_ASSERT_CHECKING
	Assert(equal(qcopy, query) && "Normalization should not modify original query object");
#endif

	return res;
}

/**
 * This method takes a join tree that contains from expr and translates them to
 * explicit join expressions.
 * For example:
 * select * from t1, t2, t3 where pred(t1.a,t2.b,t3.c);
 * is translated to
 * select * from t1 cross join t2 cross join t3 where pred(t1.a,t2.b,t3.c)
 *
 * Note that this does not use the generic expression mutator framework because
 * the recursion is highly specialized.
 */
static Node* normalize_query_jointree(Node *node)
{
	Node *result = NULL;

	if (!node)
		return NULL;

#ifdef USE_ASSERT_CHECKING
	Node *exprCopy = copyObject(node);
#endif

	switch(nodeTag(node))
	{
		case T_FromExpr:
			{
				FromExpr *oldFrom = (FromExpr *) node;
				FromExpr *from = (FromExpr *) copyObject(oldFrom);
				if (oldFrom->fromlist)
				{
					Node *newFromExprEntry = (Node *) normalize_query_jointree((Node *) oldFrom->fromlist);
					from->fromlist = list_make1(newFromExprEntry);
				}
				Assert(equal(from->quals, oldFrom->quals));
				result = (Node *) from;
				break;
			}
		case T_List:
			{
				List *crossJoinList = (List *) copyObject(node);
				if (list_length(crossJoinList) == 1)
				{
					Node *entry = lfirst(list_head(crossJoinList));
					result = normalize_query_jointree(entry);
				}
				else
				{
					Node *larg = lfirst(list_head(crossJoinList));
					Assert(larg);
					larg = normalize_query_jointree(larg);
					List *rest = list_delete_first(crossJoinList);
					Node *rarg = normalize_query_jointree((Node *) rest);
					JoinExpr *join = makeNode(JoinExpr);
					join->jointype = JOIN_INNER;
					join->isNatural = false;
					join->larg = larg;
					join->rarg = rarg;
					join->quals = NULL; /* Cross product */
					join->rtindex = 0;
					join->usingClause = NIL;
					result = (Node *) join;
				}
				break;
			}
		case T_JoinExpr:
			{
				JoinExpr *join = (JoinExpr *) copyObject(node);
				join->larg = normalize_query_jointree(join->larg);
				join->rarg = normalize_query_jointree(join->rarg);
				result = (Node *) join;
				break;
			}
		case T_RangeTblRef:
			{
				result = (Node *) node;
				break;
			}
		default:
			Assert(false && "Unrecognized entry in jointree");
			break;
	}

	Assert(result);

	/* Assert that the input is unmodified */
#ifdef USE_ASSERT_CHECKING
	Assert(equal(node, exprCopy));
#endif

	return result;
}

/**
 * This method walks through a query tree, finds the join tree and normalizes them.
 * It also walks sublinks and subqueries and normalizes their jointrees as well.
 * E.g. a query of the form SELECT x, y FROM t1, t2, t3 where x = y and y = z is transformed to:
 * SELECT x,y FROM (t1 INNER JOIN (t2 INNER JOIN t3)) where x = y
 *
 */
static Node *normalize_query_jointree_mutator(Node *node, void *context)
{
	Assert(context == NULL);

	if (!node)
	{
		return NULL;
	}

	switch(nodeTag(node))
	{
		case T_Query:
			{
				Query *newQuery = (Query *) copyObject(node);
				newQuery->jointree = (FromExpr*) normalize_query_jointree((Node *) newQuery->jointree);
				newQuery = (Query *) query_tree_mutator(newQuery, normalize_query_jointree_mutator, context, 0);
				return (Node *) newQuery;
			}
		default:
			break;
	}

	return expression_tree_mutator(node, normalize_query_jointree_mutator, context);
}

/**
 * Mutator that walks the query tree and replaces single row returning volatile (sirv) functions with a more complicated
 * construct so that it may be evaluated in a initplan subsequently. Reason for this is that this sirv function may perform
 * DDL/DML/dispatching and it can do all this only if it is evaluated on the master as an initplan. See MPP-12635 for details.
 */
static Node *replace_sirv_functions_mutator(Node *node, void *context)
{
	Assert(context == NULL);

	if (!node)
	{
		return NULL;
	}

	/**
	 * Do not recurse into sublinks
	 */
	if (IsA(node, SubLink))
	{
		return node;
	}

	if (IsA(node, Query))
	{
		/**
		 * Find sirv functions in the targetlist and replace them
		 */
		Query *query = (Query *) copyObject(node);

		if (safe_to_replace_sirvf_tle(query))
		{
			for (int tleOffset = 0; tleOffset < list_length(query->targetList); tleOffset++)
			{
				replace_sirvf_tle(query, tleOffset + 1);
			}
		}

		/**
		 * Find sirv functions in the range table entries and replace them
		 */
		if (safe_to_replace_sirvf_rte(query))
		{
			for (int rteOffset = 0; rteOffset < list_length(query->rtable); rteOffset++)
			{
				replace_sirvf_rte(query, rteOffset + 1);
			}
		}

		return (Node *) query_tree_mutator(query, replace_sirv_functions_mutator, context, 0);
	}

	return expression_tree_mutator(node, replace_sirv_functions_mutator, context);
}

/**
 * Replace all sirv function references in a TLE (see replace_sirv_functions_mutator for an explanation)
 */
static void replace_sirvf_tle(Query *query, int tleOffset)
{
	Assert(query);

	Assert(safe_to_replace_sirvf_tle(query));

	TargetEntry *tle = list_nth(query->targetList, tleOffset - 1);

	tle->expr = (Expr *) replace_sirvf_tle_expr_mutator((Node *) tle->expr, NULL);

	/**
	 * If the resulting targetlist entry has a sublink, the query's flag must be set
	 */
	if (contain_subplans((Node *) tle->expr))
	{
		query->hasSubLinks = true;
	}

}

/**
 * Is a function expression a sirvf?
 */
static bool is_sirv_funcexpr(FuncExpr *fe)
{
	bool res = (!fe->funcretset /* Set returning functions cannot become initplans */
			&& !fe->is_tablefunc /* Ignore table functions */
			&& !contain_vars_of_level_or_above((Node *) fe->args, 0) /* Must be variable free */
			&& !contain_subplans((Node *) fe->args) /* Must not contain sublinks */
			&& func_volatile(fe->funcid) == PROVOLATILE_VOLATILE /* Must be a volatile function */
			&& fe->funcresulttype != RECORDOID /* Record types cannot be handled currently */
			);

	/**
	 * Function cannot be sequence related
	 */
	Oid funcid = fe->funcid;
	res = res && !(funcid == F_NEXTVAL_OID || funcid == F_CURRVAL_OID || funcid == F_SETVAL_OID);

	return res;
}

/**
 * This mutator replaces all instances of a function that is a sirvf with a subselect.
 * Note that this must only work on expressions that occur in the targetlist.
 */
static Node *replace_sirvf_tle_expr_mutator(Node *node, void *context)
{
	Assert(context == NULL);

	if (!node)
		return NULL;

	if (IsA(node, FuncExpr)
			&& is_sirv_funcexpr((FuncExpr *) node))
	{
		node = (Node *) make_sirvf_subselect((FuncExpr *) node);
	}

	return expression_tree_mutator(node, replace_sirvf_tle_expr_mutator, context);
}

/**
 * Given a sirv function, wrap it up in a subselect and return the sublink node
 */
static SubLink *make_sirvf_subselect(FuncExpr *fe)
{
	Assert(is_sirv_funcexpr(fe));

	Query *query = makeNode(Query);
	query->commandType = CMD_SELECT;
	query->querySource = QSRC_PLANNER;
	query->jointree = makeNode(FromExpr);

	TargetEntry *tle = (TargetEntry *) makeTargetEntry((Expr *) fe, 1, "sirvf", false);
	query->targetList = list_make1(tle);

	SubLink *sublink = makeNode(SubLink);
	sublink->location = -1;
	sublink->subLinkType = EXPR_SUBLINK;
	sublink->subselect = (Node *) query;

	return sublink;
}

/**
 * Given a sirv function expression, create a subquery (derived table) from it.
 */
static Query *make_sirvf_subquery(FuncExpr *fe)
{
	SubLink *sl = make_sirvf_subselect(fe);

	Query *sq = makeNode(Query);
	sq->commandType = CMD_SELECT;
	sq->querySource = QSRC_PLANNER;
	sq->jointree = makeNode(FromExpr);

	TargetEntry *tle = (TargetEntry *) makeTargetEntry((Expr *) sl, 1, "sirvf_sq", false);
	sq->targetList = list_make1(tle);
	sq->hasSubLinks = true;

	return sq;
}

/**
 * Is it safe to replace tle's in this query? Very conservatively, the jointree must be empty
 */
static bool safe_to_replace_sirvf_tle(Query *query)
{
	return (query->jointree->fromlist == NIL);
}


/**
 * Does a query return utmost single row?
 */
static bool single_row_query(Query *query)
{
	/**
	 * If targetlist has a SRF, then no guarantee
	 */
	if (expression_returns_set( (Node *) query->targetList))
	{
		return false;
	}

	/**
	 * Every range table entry must return utmost one row.
	 */
	ListCell *lcrte = NULL;
	foreach (lcrte, query->rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(lcrte);

		switch(rte->rtekind)
		{
			case RTE_FUNCTION:
			{
				FuncExpr *fe = (FuncExpr *) rte->funcexpr;
				if (fe->funcretset)
				{
					/* SRF in FROM clause */
					return false;
				}
				break;
			}
			case RTE_SUBQUERY:
			{
				Query *sq = (Query *) rte->subquery;
				/**
				 * Recurse into subquery to see if it returns
				 * utmost one row.
				 */
				if (!single_row_query(sq))
				{
					return false;
				}
				break;
			}
			default:
			{
				return false;
			}
		}
	}

	return true;
}

/**
 * Is it safe to replace rte's in this query? This is the
 * case if the query returns utmost one row.
 */
static bool safe_to_replace_sirvf_rte(Query *query)
{
	return single_row_query(query);
}

/**
 * If a range table entry contains a sirv function, this must be replaced with a derived table (subquery)
 * with a sublink - this will eventually be turned into an initplan.
 * Conceptually, SELECT * FROM FOO(1) t1 is turned into SELECT * FROM (SELECT (SELECT FOO(1))) t1.
 */
static void replace_sirvf_rte(Query *query, int rteIndex)
{
	Assert(query);
	Assert(safe_to_replace_sirvf_rte(query));
	RangeTblEntry *rte = (RangeTblEntry *) list_nth(query->rtable, rteIndex - 1);
	if (rte->rtekind == RTE_FUNCTION)
	{
		FuncExpr *fe = (FuncExpr *) rte->funcexpr;
		Assert(fe);

		/**
		 * Transform function expression's inputs
		 */
		fe->args = (List *) replace_sirvf_tle_expr_mutator((Node *) fe->args, NULL);

		/**
		 * If the resulting targetlist entry has a sublink, the query's flag must be set
		 */
		if (contain_subplans((Node *) fe->args))
		{
			query->hasSubLinks = true;
		}

		/**
		 * If function expression is a SIRV, then further transformations
		 * need to happen
		 */
		if (is_sirv_funcexpr(fe))
		{
			bool returns_record = (get_typtype(fe->funcresulttype) == 'c');

			if (returns_record)
			{
				/**
				 * Need to extract out relevant vars using fieldselect
				 */
				wrap_vars_with_fieldselect(query->targetList,
						rteIndex,
						fe->funcresulttype
				);
			}

			Query *subquery = make_sirvf_subquery(fe);

			rte->funcexpr = NULL;
			rte->funccoltypes = NIL;
			rte->funcuserdata = NULL;
			rte->funccoltypmods = NIL;

			/**
			 * Turn the range table entry to the kind RTE_SUBQUERY
			 */
			rte->rtekind = RTE_SUBQUERY;
			rte->subquery = subquery;
		}
	}

}

/**
 * Context for mutator
 */
typedef struct fieldselect_mutator_context
{
	plan_tree_base_prefix base;
	int varno;		/* What is the relid of vars that are to be changed? */
	Oid recordtype;

} fieldselect_mutator_context;

static Node *wrap_vars_with_fieldselect_mutator(Node *node, fieldselect_mutator_context *ctx);

/**
 * Iterate over expression and wrap vars with specific varno
 * with a fieldselect
 */
static Node *wrap_vars_with_fieldselect_mutator(Node *node, fieldselect_mutator_context *ctx)
{
	Assert(ctx);
	if (node == NULL)
	{
		return NULL;
	}

	if (IsA(node, Var))
	{
		Var *v = (Var *) node;

		if (v->varno == ctx->varno)
		{
			Var *v1 = (Var *) copyObject(v);
			v1->varattno = 1;	/* Attribute is a record */
			v1->vartype = ctx->recordtype;	/* What is the composite type */
			v1->vartypmod = -1;
			FieldSelect *fs = (FieldSelect *) makeNode(FieldSelect);
			fs->arg = (Expr *) v1;
			fs->fieldnum = v->varattno;
			fs->resulttype = v->vartype;
			fs->resulttypmod = v->vartypmod;
			return (Node *) fs;
		}
		return (Node *) v;
	}

	return expression_tree_mutator(node, wrap_vars_with_fieldselect_mutator, ctx);
}

/**
 * Wrap vars with specified varno with a fieldselect.
 */
static void wrap_vars_with_fieldselect(List *targetlist, int varno, Oid recordtype)
{
	fieldselect_mutator_context ctx;
	ctx.base.node = NULL;
	ctx.varno = varno;
	ctx.recordtype = recordtype;

	ListCell *lc = NULL;
	int tleOffset = 0;
	foreach_with_count(lc, targetlist, tleOffset)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);
		lfirst(lc) = (TargetEntry *) wrap_vars_with_fieldselect_mutator((Node *) tle, &ctx);
	}
}

