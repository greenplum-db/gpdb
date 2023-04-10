/*-------------------------------------------------------------------------
 *
 * cdbsubselect.c
 *	  Flattens subqueries, transforms them to joins.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbsubselect.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/skey.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/optimizer.h"
#include "optimizer/subselect.h"	/* convert_testexpr() */
#include "optimizer/tlist.h"
#include "optimizer/prep.h"		/* canonicalize_qual() */
#include "parser/parse_oper.h"	/* make_op() */
#include "parser/parse_expr.h"
#include "parser/parse_relation.h"	/* addRangeTableEntryForSubquery() */
#include "parser/parsetree.h"	/* rt_fetch() */
#include "rewrite/rewriteManip.h"
#include "utils/lsyscache.h"	/* get_op_btree_interpretation() */
#include "utils/syscache.h"
#include "cdb/cdbsubselect.h"	/* me */
#include "lib/stringinfo.h"
#include "cdb/cdbpullup.h"

static int	add_expr_subquery_rte(Query *parse, Query *subselect);

static JoinExpr *make_join_expr(Node *larg, int r_rtindex, int join_type);

static Node *make_lasj_quals(PlannerInfo *root, SubLink *sublink, int subquery_indx);

static Node *add_null_match_clause(Node *clause);

typedef struct NonNullableVarsContext
{
	Query	   *query;              /* Query in question. */
	List       *varsToCheck;        /* Vars to check when walking to RTE */
	List	   *nonNullableVars;	/* Known non-nullable vars */
} NonNullableVarsContext;

typedef struct FindAllVarsContext
{
	List       *rtable;
	List       *vars;
} FindAllVarsContext;


/**
 * Walker that performs the following tasks:
 * - It checks if a given expr is "safe" to be pulled up to be a join
 * - Extracts out the vars from the outer query in the qual in order
 * - Extracts out the vars from the inner query in the qual in order
 */
typedef struct ConvertSubqueryToJoinContext
{
	bool		safeToConvert;	/* Can correlated expression subquery be
								 * pulled up? */
	Node	   *joinQual;		/* Qual to employ to join subquery */
	Node	   *innerQual;		/* Qual to leave behind in subquery */
	List	   *targetList;		/* targetlist for subquery */
	List	   *groupClause;	/* grouping clause for subquery */
} ConvertSubqueryToJoinContext;

static void ProcessSubqueryToJoin(Query *subselect, ConvertSubqueryToJoinContext *context);
static void ProcessSubqueryToJoin_walker(Node *jtree, ConvertSubqueryToJoinContext *context);
static void SubqueryToJoinWalker(Node *node, ConvertSubqueryToJoinContext *context);
static void RemoveInnerJoinQuals(Query *subselect);
static void RemoveInnerJoinQuals_walker(Node *jtree);

static bool cdb_find_nonnullable_vars_walker(Node *node, NonNullableVarsContext *context);
static bool is_attribute_nonnullable(Oid relationOid, AttrNumber attrNumber);
static List *fetch_targetlist_exprs(List *targetlist);
static List *fetch_outer_exprs(Node *testexpr);
static bool  is_exprs_nullable(Node *exprs, Query *query);
static bool  is_exprs_nullable_internal(Node *exprs, List *nonnullable_vars, List *rtable);
static List *cdb_find_all_vars(Node *exprs, List *rtable);
static bool  cdb_find_all_vars_walker(Node *node, FindAllVarsContext *context);
static Var *cdb_map_to_base_var(Var *var, List *rtable);

#define DUMMY_COLUMN_NAME "zero"

/**
 * Initialize context.
 */
static void
InitConvertSubqueryToJoinContext(ConvertSubqueryToJoinContext *ctx)
{
	Assert(ctx);
	ctx->safeToConvert = true;
	ctx->joinQual = NULL;
	ctx->innerQual = NULL;
	ctx->groupClause = NIL;
	ctx->targetList = NIL;
}

/**
 * Process correlated opexpr of the form foo(outer.var) OP bar(inner.var). Extracts
 * bar(inner.var) as innerExpr.
 * Returns true, if this is not a compatible correlated opexpr.
 */
static bool
IsCorrelatedOpExpr(OpExpr *opexp, Expr **innerExpr)
{
	Expr	   *e1;
	Expr	   *e2;

	if (list_length(opexp->args) != 2)
		return false;

	e1 = (Expr *) list_nth(opexp->args, 0);
	e2 = (Expr *) list_nth(opexp->args, 1);

	/*
	 * One of the vars must be outer, and other must be inner.
	 */
	if (contain_vars_of_level((Node *) e1, 1) &&
			!contain_vars_of_level((Node *) e1, 0) &&
			contain_vars_of_level((Node *) e2, 0) &&
			!contain_vars_of_level((Node *) e2, 1))
	{
		*innerExpr = (Expr *) copyObject(e2);

		return true;
	}

	if (contain_vars_of_level((Node *) e1, 0) &&
			!contain_vars_of_level((Node *) e1, 1) &&
			contain_vars_of_level((Node *) e2, 1) &&
			!contain_vars_of_level((Node *) e2, 0))
	{
		*innerExpr = (Expr *) copyObject(e1);

		return true;
	}

	return false;
}

/**
 * Checks if an opexpression is of the form (foo(outervar) = bar(innervar))
 * Input:
 *	opexp - op expression
 * Output:
 *	returns true if correlated equality condition
 *	*innerExpr - points to the inner expr i.e. bar(innervar) in the condition
 *	*eqOp and *sortOp - equality and < operators, to implement the condition as a mergejoin.
 *  The *eqOp and *sortOp should be determined according to innervar's type.
 */
static bool
IsCorrelatedEqualityOpExpr(OpExpr *opexp, Expr **innerExpr, Oid *eqOp, Oid *sortOp, bool *hashable)
{
	Oid			opfamily;
	Oid			innerExprType;
	List	   *l;

	Assert(opexp);
	Assert(list_length(opexp->args) > 1);
	Assert(innerExpr);
	Assert(eqOp);
	Assert(sortOp);

	if (!IsCorrelatedOpExpr(opexp, innerExpr))
		return false;

	Assert(*innerExpr);
	innerExprType = exprType((Node *)*innerExpr);

	/*
	 * If this is an expression of the form a = b, then we want to know about
	 * the vars involved.
	 */
	if (!op_mergejoinable(opexp->opno, innerExprType))
		return false;

	/*
	 * Arbitrarily use the first operator family containing the operator that
	 * we can find.
	 */
	l = get_mergejoin_opfamilies(opexp->opno);
	if (l == NIL)
		return false;

	opfamily = linitial_oid(l);
	list_free(l);

	/*
	 * Look up the correct equility/sort operators from the chosen opfamily.
	 */
	*eqOp = get_opfamily_member(opfamily, innerExprType, innerExprType, BTEqualStrategyNumber);
	if (!OidIsValid(*eqOp))	/* should not happen */
		elog(ERROR, "could not find member %d(%u,%u) of opfamily %u",
			 BTEqualStrategyNumber, innerExprType, innerExprType, opfamily);

	*sortOp = get_opfamily_member(opfamily, innerExprType, innerExprType, BTLessStrategyNumber);
	if (!OidIsValid(*sortOp))	/* should not happen */
		elog(ERROR, "could not find member %d(%u,%u) of opfamily %u",
			 BTLessStrategyNumber, innerExprType, innerExprType, opfamily);

	*hashable = op_hashjoinable(*eqOp, innerExprType);

	return true;
}

/**
 * Process subquery to extract useful information to be able to convert it to
 * a join.
 *
 * This scans the join tree, and verifies that it consists entirely of inner
 * joins. The inner joins can be represented as explicit JOIN_INNER JoinExprs*
 * or as FromExprs. All the join quals are collected in context->innerQual.
 *
 * context->safeToConvert must be 'true' on entry. This sets it to false if
 * there are any non-inner joins in the tree.
 */
static void
ProcessSubqueryToJoin(Query *subselect, ConvertSubqueryToJoinContext *context)
{
	Assert(context);
	Assert(context->safeToConvert);
	Assert(subselect);

	ProcessSubqueryToJoin_walker((Node *) subselect->jointree, context);
}

static void
ProcessSubqueryToJoin_walker(Node *jtree, ConvertSubqueryToJoinContext *context)
{
	if (IsA(jtree, JoinExpr))
	{
		JoinExpr   *je = (JoinExpr *) jtree;

		/*
		 * If subselect's join tree is not a plain relation or an inner join,
		 * we refuse to convert.
		 */
		if (je->jointype != JOIN_INNER)
		{
			context->safeToConvert = false;
			return;
		}

		ProcessSubqueryToJoin_walker(je->larg, context);
		if (!context->safeToConvert)
			return;
		ProcessSubqueryToJoin_walker(je->rarg, context);
		if (!context->safeToConvert)
			return;

		SubqueryToJoinWalker(je->quals, context);
	}
	else if (IsA(jtree, FromExpr))
	{
		FromExpr   *fe = (FromExpr *) jtree;
		ListCell   *lc;

		foreach(lc, fe->fromlist)
		{
			ProcessSubqueryToJoin_walker(lfirst(lc), context);
			if (!context->safeToConvert)
				return;
		}

		SubqueryToJoinWalker(fe->quals, context);
	}
	else if (IsA(jtree, RangeTblRef))
	{
		/* nothing to do */
	}
	else
	{
		elog(ERROR, "unexpected node of type %d in join tree", jtree->type);
	}
}

/**
 * Wipe out join quals i.e. top-level where clause and any quals in the top-level inner join.
 */
static void
RemoveInnerJoinQuals(Query *subselect)
{
	RemoveInnerJoinQuals_walker((Node *) subselect->jointree);
}

static void
RemoveInnerJoinQuals_walker(Node *jtree)
{
	if (IsA(jtree, JoinExpr))
	{
		JoinExpr   *je = (JoinExpr *) jtree;

		/*
		 * We already checked in ProcessSubqueryToJoin() that there
		 * are no outer joins, but doesn't hurt to check again.
		 */
		if (je->jointype != JOIN_INNER)
		{
			elog(ERROR, "unexpected join type encountered while converting subquery to join");
		}

		RemoveInnerJoinQuals_walker(je->larg);
		RemoveInnerJoinQuals_walker(je->rarg);

		je->quals = NULL;
	}
	else if (IsA(jtree, FromExpr))
	{
		FromExpr   *fe = (FromExpr *) jtree;
		ListCell   *lc;

		foreach(lc, fe->fromlist)
		{
			RemoveInnerJoinQuals_walker(lfirst(lc));
		}

		fe->quals = NULL;
	}
	else if (IsA(jtree, RangeTblRef))
	{
		/* nothing to do */
	}
	else
	{
		elog(ERROR, "unexpected node of type %d in join tree", jtree->type);
	}
}

/**
 * This method recursively walks down the quals of an expression subquery to see if it can be pulled up to a join
 * and constructs the pieces necessary to perform the pullup.
 * E.g. SELECT * FROM outer o WHERE o.a < (SELECT avg(i.x) FROM inner i WHERE o.b = i.y)
 * This extracts interesting pieces of the subquery so as to create SELECT i.y, avg(i.x) from inner i GROUP by i.y
 */
static void
SubqueryToJoinWalker(Node *node, ConvertSubqueryToJoinContext *context)
{
	Assert(context);
	Assert(context->safeToConvert);

	if (node == NULL)
	{
		return;
	}

	if (IsA(node, BoolExpr))
	{

		/**
		 * Be extremely conservative. If there are any outer vars under an or or a not expression, then give up.
		 */
		if (is_notclause(node)
			|| is_orclause(node))
		{
			if (contain_vars_of_level_or_above(node, 1))
			{
				context->safeToConvert = false;
				return;
			}
			context->innerQual = make_and_qual(context->innerQual, node);
			return;
		}

		Assert(is_andclause(node));

		BoolExpr   *bexp = (BoolExpr *) node;
		ListCell   *lc = NULL;

		foreach(lc, bexp->args)
		{
			Node	   *arg = (Node *) lfirst(lc);

			/**
			 * If there is an outer var anywhere in the boolean expression, walk recursively.
			 */
			if (contain_vars_of_level_or_above(arg, 1))
			{
				SubqueryToJoinWalker(arg, context);

				if (!context->safeToConvert)
				{
					return;
				}
			}
			else
			{
				/**
				 * This qual should be part of the subquery's inner qual.
				 */
				context->innerQual = make_and_qual(context->innerQual, arg);
			}
		}
		return;
	}

	/**
	 * If this is a correlated opexpression, we'd need to look inside.
	 */
	else if (contain_vars_of_level_or_above(node, 1) && IsA(node, OpExpr))
	{
		OpExpr	   *opexp = (OpExpr *) node;

		/**
		 * If this is an expression of the form foo(outervar) = bar(innervar), then we want to know about the inner expression.
		 */
		Oid			eqOp = InvalidOid;
		Oid			sortOp = InvalidOid;
		bool		hashable = false;
		Expr	   *innerExpr = NULL;
		bool		considerOpExpr = false;

		considerOpExpr = IsCorrelatedEqualityOpExpr(opexp, &innerExpr, &eqOp, &sortOp, &hashable);

		if (considerOpExpr)
		{
			TargetEntry *tle;
			SortGroupClause *gc;

			tle = makeTargetEntry(innerExpr,
											   list_length(context->targetList) + 1,
											   NULL,
											   false);
			tle->ressortgroupref = list_length(context->targetList) + 1;
			context->targetList = lappend(context->targetList, tle);

			gc = makeNode(SortGroupClause);
			gc->tleSortGroupRef = list_length(context->groupClause) + 1;
			gc->eqop = eqOp;
			gc->sortop = sortOp;
			gc->hashable = hashable;
			context->groupClause = lappend(context->groupClause, gc);

			Assert(list_length(context->groupClause) == list_length(context->targetList));

			context->joinQual = make_and_qual(context->joinQual, (Node *) opexp);

			return;
		}

		/**
		 * Correlated join expression contains incompatible operators. Not safe to convert.
		 */
		context->safeToConvert = false;
	}
	else if (contain_vars_of_level_or_above(node, 1))
	{
		/*
		 * This is a correlated expression, but we don't know how to deal with
		 * it. Give up.
		 */
		context->safeToConvert = false;
	}

	return;
}

/*
 * cdbsubselect_drop_distinct
 */
void
cdbsubselect_drop_distinct(Query *subselect)
{
	if (subselect->limitCount == NULL &&
		subselect->limitOffset == NULL)
	{
		/* Delete DISTINCT. */
		if (!subselect->hasDistinctOn ||
			list_length(subselect->distinctClause) == list_length(subselect->targetList))
			subselect->distinctClause = NIL;

		/* Delete GROUP BY if subquery has no aggregates and no HAVING. */
		if (!subselect->hasAggs &&
			subselect->havingQual == NULL)
			subselect->groupClause = NIL;
	}
}	/* cdbsubselect_drop_distinct */

/*
 * cdbsubselect_drop_orderby
 */
void
cdbsubselect_drop_orderby(Query *subselect)
{
	if (subselect->limitCount == NULL &&
		subselect->limitOffset == NULL)
	{
		/* Delete ORDER BY. */
		if (!subselect->hasDistinctOn ||
			list_length(subselect->distinctClause) == list_length(subselect->targetList))
			subselect->sortClause = NIL;
	}
}	/* cdbsubselect_drop_orderby */

/**
 * Safe to convert expr sublink to a join
 */
static bool
safe_to_convert_EXPR(SubLink *sublink, ConvertSubqueryToJoinContext *ctx1)
{
	Assert(ctx1);

	Query	   *subselect = (Query *) sublink->subselect;

	if (subselect->jointree->fromlist == NULL)
		return false;

	if (expression_returns_set((Node *) subselect->targetList))
		return false;

	/* No set operations in the subquery */
	if (subselect->setOperations)
		return false;

	/**
	 * If there are no correlations in the WHERE clause, then don't bother.
	 */
	if (!IsSubqueryCorrelated(subselect))
		return false;

	/**
	 * If there are correlations in a func expr in the from clause, then don't bother.
	 */
	if (has_correlation_in_funcexpr_rte(subselect->rtable))
	{
		return false;
	}

	/**
	 * If there is a having qual, then don't bother.
	 */
	if (subselect->havingQual != NULL)
		return false;

	/**
	 * If it does not have aggs, then don't bother. This could result in a run-time error.
	 */
	if (!subselect->hasAggs)
		return false;

	/**
	 * A LIMIT or OFFSET could interfere with the transformation of the
	 * correlated qual to GROUP BY. (LIMIT >0 in a subquery that contains a
	 * plain aggregate is actually a no-op, so we could try to remove it,
	 * but it doesn't seem worth the trouble to optimize queries with
	 * pointless limits like that.)
	 */
	if (subselect->limitOffset || subselect->limitCount)
		return false;

	/**
	 * Cannot support grouping clause in subselect.
	 */
	if (subselect->groupClause)
		return false;

	/**
	 * If targetlist of the subquery does not contain exactly one element, don't bother.
	 */
	if (list_length(subselect->targetList) != 1)
		return false;


	/**
	 * Walk the quals of the subquery to do a more fine grained check as to whether this subquery
	 * may be pulled up. Identify useful fragments to construct join condition if possible to pullup.
	 */
	ProcessSubqueryToJoin(subselect, ctx1);

	/**
	 * There should be no outer vars in innerQual
	 */
	Assert(!contain_vars_of_level_or_above(ctx1->innerQual, 1));

	return ctx1->safeToConvert;
}


/**
 * convert_EXPR_to_join
 *
 * Method attempts to convert an EXPR_SUBLINK of the form select * from T where a > (select 10*avg(x) from R where T.b=R.y)
 */
JoinExpr *
convert_EXPR_to_join(PlannerInfo *root, OpExpr *opexp)
{
	Assert(root);
	Assert(list_length(opexp->args) == 2);
	Node	   *rarg = list_nth(opexp->args, 1);

	Assert(IsA(rarg, SubLink));
	SubLink    *sublink = (SubLink *) rarg;

	ConvertSubqueryToJoinContext ctx1;

	InitConvertSubqueryToJoinContext(&ctx1);

	if (safe_to_convert_EXPR(sublink, &ctx1))
	{
		Query	   *subselect = (Query *) copyObject(sublink->subselect);

		Assert(IsA(subselect, Query));

		/**
		 * Original subselect must have a single output column (e.g. 10*avg(x) )
		 */
		Assert(list_length(subselect->targetList) == 1);

		/**
		 * To pull up the subquery, we need to construct a new "Query" object that has grouping
		 * columns extracted from the correlated join predicate and the extra column from the subquery's
		 * targetlist.
		 */
		TargetEntry *origSubqueryTLE = (TargetEntry *) list_nth(subselect->targetList, 0);

		List	   *subselectTargetList = (List *) copyObject(ctx1.targetList);

		subselectTargetList = add_to_flat_tlist(subselectTargetList, list_make1(origSubqueryTLE->expr));
		subselect->targetList = subselectTargetList;
		subselect->groupClause = ctx1.groupClause;

		RemoveInnerJoinQuals(subselect);

		subselect->jointree->quals = ctx1.innerQual;

		/**
		 * Construct a new range table entry for the new pulled up subquery.
		 */
		int			rteIndex = add_expr_subquery_rte(root->parse, subselect);

		Assert(rteIndex > 0);

		/**
		 * Construct the join expression involving the new pulled up subselect.
		 */
		JoinExpr   *join_expr = make_join_expr(NULL, rteIndex, JOIN_INNER);
		Node	   *joinQual = ctx1.joinQual;

		/**
		 * Make outer ones regular and regular ones correspond to rteIndex
		 */
		joinQual = (Node *) cdbpullup_expr((Expr *) joinQual, subselect->targetList, NULL, rteIndex);
		IncrementVarSublevelsUp(joinQual, -1, 1);

		join_expr->quals = joinQual;

		TargetEntry *subselectAggTLE = (TargetEntry *) list_nth(subselect->targetList, list_length(subselect->targetList) - 1);

		/**
		 *	modify the op expr to involve the column that has the computed aggregate that needs to compared.
		 */
		Var		   *aggVar = (Var *) makeVar(rteIndex,
											 subselectAggTLE->resno,
											 exprType((Node *) subselectAggTLE->expr),
											 exprTypmod((Node *) subselectAggTLE->expr),
											 exprCollation((Node *) subselectAggTLE->expr),
											 0);

		list_nth_replace(opexp->args, 1, aggVar);

		return join_expr;
	}

	return NULL;
}

/* NOTIN subquery transformation -start */

/* check if NOT IN conversion to antijoin is possible */
static bool
safe_to_convert_NOTIN(SubLink *sublink, Relids available_rels)
{
	Query	   *subselect = (Query *) sublink->subselect;
	Relids		left_varnos;

	/* cases we don't currently handle are listed below. */

	/* ARRAY sublinks have empty test expressions */
	if (sublink->testexpr == NULL)
		return false;

	/* No volatile functions in the subquery */
	if (contain_volatile_functions(sublink->testexpr))
		return false;

	/**
	 * If there are correlations in a func expr in the from clause, then don't bother.
	 */
	if (has_correlation_in_funcexpr_rte(subselect->rtable))
	{
		return false;
	}

	/* Left-hand expressions must contain some Vars of the current */
	left_varnos = pull_varnos(sublink->testexpr);
	if (bms_is_empty(left_varnos))
		return false;

	/*
	 * However, it can't refer to anything outside available_rels.
	 */
	if (!bms_is_subset(left_varnos, available_rels))
		return false;

	/* Correlation - subquery referencing Vars of parent not handled */
	if (contain_vars_of_level((Node *) subselect, 1))
		return false;

	/* No set operations in the subquery */
	if (subselect->setOperations)
		return false;

	return true;
}

/*
 * Find if the supplied targetlist has any resjunk
 * entries. We only have to check the tail since
 * resjunks (if any) can only appear in the end.
 */
inline static bool
has_resjunk(List *tlist)
{
	bool		resjunk = false;
	Node	   *tlnode = (Node *) (lfirst(tlist->tail));

	if (IsA(tlnode, TargetEntry))
	{
		TargetEntry *te = (TargetEntry *) tlnode;

		if (te->resjunk)
			resjunk = true;
	}
	return resjunk;
}

/* add a dummy constant var to the end of the supplied list */
static List *
add_dummy_const(List *tlist)
{
	TargetEntry *dummy;
	Const	   *zconst;
	int			resno;

	zconst = makeConst(INT4OID, -1, InvalidOid, sizeof(int32), (Datum) 0,
					   false, true);	/* isnull, byval */
	resno = list_length(tlist) + 1;
	dummy = makeTargetEntry((Expr *) zconst,
							resno,
							DUMMY_COLUMN_NAME,
							false /* resjunk */ );

	if (tlist == NIL)
		tlist = list_make1(dummy);
	else
		tlist = lappend(tlist, dummy);

	return tlist;
}

/* Add a dummy variable to the supplied target list. The
 * variable is added to end of targetlist but before all
 * resjunk vars (if any). The caller should make use of
 * the returned targetlist since this code might modify
 * the list in-place.
 */
static List *
mutate_targetlist(List *tlist)
{
	List	   *new_list = NIL;

	if (has_resjunk(tlist))
	{
		ListCell   *curr = NULL;
		bool		junk = false;

		foreach(curr, tlist)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(curr);

			if (tle->resjunk)
			{
				tle->resno = tle->resno + 1;
				if (!junk)
				{
					junk = true;
					new_list = add_dummy_const(new_list);
				}
			}
			new_list = lappend(new_list, tle);
		}
	}
	else
	{
		new_list = tlist;
		new_list = add_dummy_const(new_list);
	}
	return new_list;
}

/* Pulls up the subquery into the top-level range table.
 * Before that add a dummy column zero to the target list
 * of the subquery.
 */
static int
add_notin_subquery_rte(Query *parse, Query *subselect)
{
	RangeTblEntry *subq_rte;
	int			subq_indx;

	/*
	 * Create a RTE entry in the parent query for the subquery.
	 * It is marked as lateral, because any correlation quals will
	 * refer to other RTEs in the parent query.
	 */
	subselect->targetList = mutate_targetlist(subselect->targetList);
	subq_rte = addRangeTableEntryForSubquery(NULL,	/* pstate */
											 subselect,
											 makeAlias("NotIn_SUBQUERY", NIL),
											 false, /* not lateral */
											 false /* inFromClause */ );
	parse->rtable = lappend(parse->rtable, subq_rte);

	/* assume new rte is at end */
	subq_indx = list_length(parse->rtable);
	Assert(subq_rte == rt_fetch(subq_indx, parse->rtable));

	return subq_indx;
}

/*
 * Pulls up the expr sublink subquery into the top-level range table.
 */
static int
add_expr_subquery_rte(Query *parse, Query *subselect)
{
	RangeTblEntry *subq_rte;
	int			subq_indx;

	/**
	 * Generate column names.
	 * TODO: improve this to keep old names around
	 */
	ListCell   *lc = NULL;
	int			teNum = 0;

	foreach(lc, subselect->targetList)
	{
		TargetEntry *te = (TargetEntry *) lfirst(lc);

		te->resname = psprintf("csq_c%d", teNum);
		teNum++;
	}

	/*
	 * Create a RTE entry in the parent query for the subquery.
	 * It is marked as lateral, because any correlation quals will
	 * refer to other RTEs in the parent query.
	 */
	subq_rte = addRangeTableEntryForSubquery(NULL,	/* pstate */
											 subselect,
											 makeAlias("Expr_SUBQUERY", NIL),
											 true, /* lateral */
											 false /* inFromClause */ );
	parse->rtable = lappend(parse->rtable, subq_rte);

	/* assume new rte is at end */
	subq_indx = list_length(parse->rtable);
	Assert(subq_rte == rt_fetch(subq_indx, parse->rtable));

	return subq_indx;
}


/* Create a join expression linking the supplied larg node
 * with the pulled up NOT IN subquery located at r_rtindex
 * in the range table. The appropriate JOIN_RTE has already
 * been created by the caller and can be located at j_rtindex
 */
static JoinExpr *
make_join_expr(Node *larg, int r_rtindex, int join_type)
{
	JoinExpr   *jexpr;
	RangeTblRef *rhs;

	rhs = makeNode(RangeTblRef);
	rhs->rtindex = r_rtindex;

	jexpr = makeNode(JoinExpr);
	jexpr->jointype = join_type;
	jexpr->isNatural = false;
	jexpr->larg = larg;
	jexpr->rarg = (Node *) rhs;
	jexpr->rtindex = 0;

	return jexpr;
}

/*
 * Convert subquery's test expr to a suitable predicate.
 * If we wanted to add correlated subquery support, this would be the place to do it.
 */
static Node *
make_lasj_quals(PlannerInfo *root, SubLink *sublink, int subquery_indx)
{
	Query	   *subselect = (Query *) sublink->subselect;
	Expr	   *join_pred;
	List	   *subquery_vars;

	Assert(sublink->subLinkType == ALL_SUBLINK);

	/*
	 * Build a list of Vars representing the subselect outputs.
	 */
	subquery_vars = generate_subquery_vars(root,
										   subselect->targetList,
										   subquery_indx);

	/*
	 * Build the result qual expression, replacing Params with these Vars.
	 */
	join_pred = (Expr *) convert_testexpr(root,
										  sublink->testexpr,
										  subquery_vars);

	join_pred = canonicalize_qual(make_notclause(join_pred), false);

	Assert(join_pred != NULL);
	return (Node *) join_pred;
}

/* add IS NOT FALSE clause on top of the clause */
static Node *
add_null_match_clause(Node *clause)
{
	BooleanTest *btest;

	Assert(clause != NULL);
	btest = makeNode(BooleanTest);
	btest->arg = (Expr *) clause;
	btest->booltesttype = IS_NOT_FALSE;
	return (Node *) btest;
}


/**
 * Is the attribute of a base relation non-nullable?
 * Input:
 *	relationOid
 *	attributeNumber
 * Output:
 *	true if the attribute is non-nullable
 */
static bool
is_attribute_nonnullable(Oid relationOid, AttrNumber attrNumber)
{
	HeapTuple	attributeTuple;
	Form_pg_attribute attribute;
	bool		result = true;

	attributeTuple = SearchSysCache2(ATTNUM,
									 ObjectIdGetDatum(relationOid),
									 Int16GetDatum(attrNumber));
	if (!HeapTupleIsValid(attributeTuple))
		return false;

	attribute = (Form_pg_attribute) GETSTRUCT(attributeTuple);

	if (attribute->attisdropped)
		result = false;

	if (!attribute->attnotnull)
		result = false;

	ReleaseSysCache(attributeTuple);

	return result;
}


/**
 * This walker goes through a query's join-tree to determine the set of non-nullable
 * vars. E.g.
 * select x from t1, t2 where x=y .. the walker determines that x and y are involved in an inner join
 * and therefore are non-nullable.
 * select x from t1 where x > 20 .. the walker determines that the quals ensures that x is non-nullable
 *
 */
static bool
cdb_find_nonnullable_vars_walker(Node *node, NonNullableVarsContext *context)
{
	Assert(context);
	Assert(context->query);

	if (node == NULL)
		return false;

	switch (nodeTag(node))
	{
		case T_Var:
			{
				Var		   *var = (Var *) node;

				if (var->varlevelsup == 0)
				{
					context->nonNullableVars = list_append_unique(context->nonNullableVars, var);
				}
				return false;
			}
		case T_FuncExpr:
			{
				FuncExpr   *expr = (FuncExpr *) node;

				if (!func_strict(expr->funcid))
				{
					/*
					 * If a function is not strict, it can return non-null
					 * values for null inputs. Thus, input vars can be null
					 * and sneak through. Therefore, ignore all vars
					 * underneath.
					 */
					return false;
				}
				break;
			}
		case T_OpExpr:
			{
				OpExpr	   *expr = (OpExpr *) node;

				if (!op_strict(expr->opno))
				{
					/*
					 * If an op is not strict, it can return non-null values
					 * for null inputs. Ignore all vars underneath.
					 */
					return false;
				}

				break;
			}
		case T_BoolExpr:
			{
				BoolExpr   *expr = (BoolExpr *) node;

				if (expr->boolop == NOT_EXPR)
				{
					/**
					 * Not negates all conditions underneath. We choose to not handle
					 * this situation.
					 */
					return false;
				}
				else if (expr->boolop == OR_EXPR)
				{
					/**
					 * We add the intersection of variables determined to be
					 * non-nullable by each arg to the OR expression.
					 */
					NonNullableVarsContext c1;

					c1.query = context->query;
					c1.varsToCheck = context->varsToCheck;
					c1.nonNullableVars = NIL;
					ListCell   *lc = NULL;
					int			orArgNum = 0;

					foreach(lc, expr->args)
					{
						Node	   *orArg = lfirst(lc);

						NonNullableVarsContext c2;

						c2.query = context->query;
						c2.varsToCheck = context->varsToCheck;
						c2.nonNullableVars = NIL;
						expression_tree_walker(orArg, cdb_find_nonnullable_vars_walker, &c2);

						if (orArgNum == 0)
						{
							Assert(c1.nonNullableVars == NIL);
							c1.nonNullableVars = c2.nonNullableVars;
						}
						else
						{
							c1.nonNullableVars = list_intersection(c1.nonNullableVars, c2.nonNullableVars);
						}
						orArgNum++;
					}

					context->nonNullableVars = list_concat_unique(context->nonNullableVars, c1.nonNullableVars);
					return false;
				}

				Assert(expr->boolop == AND_EXPR);

				/**
				 * AND_EXPR is automatically handled by the walking algorithm.
				 */
				break;
			}
		case T_NullTest:
			{
				NullTest   *expr = (NullTest *) node;

				if (expr->nulltesttype != IS_NOT_NULL)
				{
					return false;
				}

				break;
			}
		case T_BooleanTest:
			{
				BooleanTest *expr = (BooleanTest *) node;

				if (!(expr->booltesttype == IS_NOT_UNKNOWN
					  || expr->booltesttype == IS_TRUE
					  || expr->booltesttype == IS_FALSE))
				{
					/* Other tests may allow a null value to pass through. */
					return false;
				}
				break;
			}
		case T_JoinExpr:
			{
				JoinExpr   *expr = (JoinExpr *) node;

				if (expr->jointype != JOIN_INNER)
				{
					/* Do not descend below any other join type */
					return false;
				}
				break;
			}
		case T_FromExpr:
		case T_List:
			{
				/*
				 * Top-level where clause is fine -- equivalent to an inner
				 * join
				 */
				break;
			}
		case T_RangeTblRef:
			{
				/*
				 * If we've gotten this far, then we can look for non-null
				 * constraints on the vars in the query's targetlist.
				 */
				RangeTblRef *rtf = (RangeTblRef *) node;
				RangeTblEntry *rte = rt_fetch(rtf->rtindex, context->query->rtable);

				if (rte->rtekind == RTE_RELATION)
				{
					/*
					 * Find all vars in the query's targetlist that are from
					 * this relation and check if the attribute is
					 * non-nullable by base table constraint.
					 */

					ListCell   *lc = NULL;

					foreach(lc, context->varsToCheck)
					{
						Var		   *var = (Var *) lfirst(lc);

						if (var->varno == rtf->rtindex)
						{
							int			attNum = var->varattno;
							int			relOid = rte->relid;

							Assert(relOid != InvalidOid);

							if (is_attribute_nonnullable(relOid, attNum))
							{
								/*
								 * Base table constraint on the var. Add
								 * it to the list!
								 */
								context->nonNullableVars = list_append_unique(context->nonNullableVars, var);
							}
						}
					}
				}
				else if (rte->rtekind == RTE_SUBQUERY)
				{
					/*
					 * When the RTE is a subquery, the algorithm to extend non-nullable Vars is:
					 *   1. based on the interested VarsToCheck, find those pointing to this RTE,
					 *      and build a list of exprs containing the corresponding target entry's
					 *      expr.
					 *   2. recusrively invoke is_exprs_nullable for The new list of exprs and the
					 *      subquery:
					 *        a. if they are non-nullable, let's expand the knowledage databse
					 *           using the corresponding Vars in original VarsToCheck
					 *        b. otherwise, we know the result is nullable, terminate the walk.
					 */
					ListCell *lc = NULL;
					List     *exprs = NIL;
					foreach(lc, context->varsToCheck)
					{
						Var *var = (Var *) lfirst(lc);
						if (var->varno == rtf->rtindex)
						{
							TargetEntry *tle;
							tle = list_nth(rte->subquery->targetList, var->varattno-1);
							exprs = lappend(exprs, tle->expr);
						}
					}

					/*
					 * The vars fetched from special RTEs (RTE_JOIN ) are mapped to those in base RTEs,
					 * so when the walker reach these special RTEs, the exprs can be NIL.
					 * Empty exprs means no check is needed in this step and the walker should continue
					 * scanning other parts of the jointree.
					 * NOTE: break is also needed to avoid call the function 'is_exprs_nullable' on NIL exprs,
					 * which reports NIL as nullable and terminates the walker
					 */
					if (exprs == NIL)
						break;

					if (is_exprs_nullable((Node *) exprs, rte->subquery))
					{
						/*
						 * The VarsTocheck must be nullable, terminate here.
						 * Since we are sure the nullable check will fail, so
						 * set the knowledage database to NIL here.
						 * */
						context->nonNullableVars = NIL;
						return true;
					}

					foreach(lc, context->varsToCheck)
					{
						Var *var = (Var *) lfirst(lc);
						if (var->varno == rtf->rtindex)
						{
							context->nonNullableVars = list_append_unique(
											context->nonNullableVars, var);
						}
					}

				}
				else if (rte->rtekind == RTE_VALUES)
				{
					/*
					 * TODO: make this work for values scan someday.
					 */
				}
				return false;
			}
		case T_PlaceHolderVar:
			{
				/*
				 * GPDB_84_MERGE_FIXME: Confirm if we need to do special
				 * handling for PlaceHolderVar. Currently we are just fall
				 * through the mutator.
				 */
				break;
			}
		default:
			{
				/* Do not descend beyond any other node */
				return false;
			}
	}
	return expression_tree_walker(node, cdb_find_nonnullable_vars_walker, context);
}

static List *
fetch_targetlist_exprs(List *targetlist)
{
	List        *exprs = NIL;
	ListCell    *lc    = NULL;

	foreach(lc, targetlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);

		if (tle->resjunk)
		{
			/*
			 * Previously, we take it nullable when we
			 * see any resjunk target entry. I think it
			 * is safe to just ignore them.
			 */
			continue;
		}

		exprs = lappend(exprs, tle->expr);
	}

	return exprs;
}

/*
 * fetch_outer_exprs
 *   @param testexpr: the NOT-IN sublink's test exprs
 *
 *   For a two-col NOT_IN query: select * from t1 where (a,b) not in (select a,b from t2)
 *   this testexpr should be:
 *   BoolExpr [boolop=NOT_EXPR]
 *      BoolExpr [boolop=AND_EXPR]
 *        OpExpr [opno=96 opfuncid=65 opresulttype=16 opretset=false]
 *                Var [varno=1 varattno=1 vartype=23 varnoold=1 varoattno=1]
 *                Param [paramkind=PARAM_SUBLINK paramid=1 paramtype=23]
 *        OpExpr [opno=96 opfuncid=65 opresulttype=16 opretset=false]
 *                Var [varno=1 varattno=2 vartype=23 varnoold=1 varoattno=2]
 *                Param [paramkind=PARAM_SUBLINK paramid=2 paramtype=23]
 *
 *  For a two-col <> ALL query: select * from t1 where (a,b) <> (select a,b from t2)
 *  this testexpr should be:
 *  BoolExpr [boolop=OR_EXPR]
 *     OpExpr [opno=518 opfuncid=144 opresulttype=16 opretset=false]
 *              Var [varno=1 varattno=1 vartype=23 varnoold=1 varoattno=1]
 *              Param [paramkind=PARAM_SUBLINK paramid=1 paramtype=23]
 *      OpExpr [opno=518 opfuncid=144 opresulttype=16 opretset=false]
 *              Var [varno=1 varattno=2 vartype=23 varnoold=1 varoattno=2]
 *              Param [paramkind=PARAM_SUBLINK paramid=2 paramtype=23]
 *
 * This function fetches all the outer parts and put them in a list as the
 * result.
 *
 * NOTE: we want to be conservative for cases we are not interested or
 * we are not sure. Returning a NIL is conservative policy here since
 * is_exprs_nullable will return true for NULL input.
 */
static List *
fetch_outer_exprs(Node *testexpr)
{
	if (testexpr == NULL)
		return NIL;

	if (IsA(testexpr, BoolExpr))
	{
		BoolExpr *be = (BoolExpr *) testexpr;
		bool      seen_not_atop;
		Node     *expr;
		seen_not_atop = be->boolop == NOT_EXPR;

		/* strip off the top NOT */
		if (seen_not_atop)
			expr = linitial(be->args);
		else
			expr = (Node *) be;

		/*
		 * The above expr should be a single OpExpr when single-column not-in,
		 * or a BoolExpr of AND when multi-column not-in. We are not interested
		 * in other cases.
		 */
		if (IsA(expr, BoolExpr))
		{
			BoolExpr *be    = (BoolExpr *) expr;
			List     *exprs = NIL;
			ListCell *lc;

			/*
			 * The following cases should not happen, instead of
			 * erroring out, let's be conservative by returning NIL.
			 */
			if (be->boolop == AND_EXPR && !seen_not_atop)
				return NIL;
			if (be->boolop == OR_EXPR && seen_not_atop)
				return NIL;
			if (be->boolop != OR_EXPR && be->boolop != AND_EXPR)
				return NIL;

			foreach(lc, be->args)
			{
				OpExpr *op_expr = (OpExpr *) lfirst(lc);
				if (!IsA(op_expr, OpExpr))
					return NIL;
				exprs = lappend(exprs, linitial(op_expr->args));
			}
			return exprs;
		}
		else if (IsA(expr, OpExpr))
			return list_make1(linitial(((OpExpr *)expr)->args));
		else
			return NIL;
	}
	else if (IsA(testexpr, OpExpr))
		return list_make1(linitial(((OpExpr *)testexpr)->args));
	else
		return NIL;
}

/*
 * is_exprs_nullable
 *   Return true if any of the exprs might be null, otherwise false.
 *   We want to be conservative for those cases either we are not
 *   interested or not sure.
 */
static bool
is_exprs_nullable(Node *exprs, Query *query)
{
	NonNullableVarsContext context;
	context.query           = query;
	context.varsToCheck     = cdb_find_all_vars(exprs, query->rtable);
	context.nonNullableVars = NIL;

	/* Find nullable vars in the jointree */
	(void) expression_tree_walker((Node *) query->jointree,
								  cdb_find_nonnullable_vars_walker, &context);

	return is_exprs_nullable_internal(exprs, context.nonNullableVars, query->rtable);
}

static bool
is_exprs_nullable_internal(Node *exprs, List *nonnullable_vars, List *rtable)
{
	if (exprs == NULL)
	{
		/*
		 * Be conservative when input is Empty. Keep consistent
		 * with fetch_outer_exprs and fetch_targetlist_exprs.
		 */
		return true;
	}

	if (IsA(exprs, Var))
	{
		Var *tmpvar = (Var *)exprs;

		/* params treat as nullable exprs */
		if (tmpvar->varlevelsup != 0)
			return true;

		Var		   *var = cdb_map_to_base_var((Var *) exprs, rtable);

		/* once not found RTE of var, return as nullable expr */
		if (var == NULL)
			return true;

		return !list_member(nonnullable_vars, var);
	}
	else if (IsA(exprs, List))
	{
		ListCell *lc;
		foreach(lc, (List *) exprs)
		{
			if (is_exprs_nullable_internal((Node *) lfirst(lc),
										   nonnullable_vars, rtable))
				return true;
		}
		return false;
	}
	else if (IsA(exprs, Const))
	{
		Const	   *constant = (Const *) exprs;
		return constant->constisnull;
	}
	else if (IsA(exprs, RelabelType))
	{
		RelabelType    *rt = (RelabelType *) exprs;
		return is_exprs_nullable_internal((Node *) rt->arg, nonnullable_vars, rtable);
	}
	else if (IsA(exprs, OpExpr))
	{
		OpExpr   *op_expr = (OpExpr *) exprs;
		ListCell *lc;
		foreach(lc, op_expr->args)
		{
			if (is_exprs_nullable_internal((Node *) lfirst(lc),
										   nonnullable_vars, rtable))
				return true;
		}
		return false;
	}
	else
	{
		/* Be conservative here */
		return true;
	}
}

/*
 * convert_IN_to_antijoin: can we convert an ALL SubLink to join style?
 * If not appropriate to process this SubLink, return it as it is.
 * Side effects of a successful conversion include adding the SubLink's
 * subselect to the top-level rangetable, adding a JOIN RTE linking the outer
 * query with the subselect and setting up the qualifiers correctly.
 *
 * The transformation is to rewrite a query of the form:
 *		select c1 from t1 where c1 NOT IN (select c2 from t2);
 *						(to)
 *		select c1 from t1 left anti semi join (select 0 as zero, c2 from t2) foo
 *						  ON (c1 = c2) IS NOT FALSE where zero is NULL;
 *
 * The pseudoconstant column zero is needed to correctly pipe in the NULLs
 * from the subselect upstream.
 *
 * The current implementation assumes that the sublink expression occurs
 * in a top-level where clause (or through a series of inner joins).
 */
JoinExpr *
convert_IN_to_antijoin(PlannerInfo *root, SubLink *sublink,
					   Relids available_rels)
{
	Query	   *parse = root->parse;
	Query	   *subselect = (Query *) sublink->subselect;

	if (safe_to_convert_NOTIN(sublink, available_rels))
	{
		/* Delete ORDER BY and DISTINCT.
		 *
		 * There is no need to do the group-by or order-by inside the
		 * subquery, if we have decided to pull up the sublink. For the
		 * group-by case, after the sublink pull-up, there will be a semi-join
		 * plan node generated in top level, which will weed out duplicate
		 * tuples naturally. For the order-by case, after the sublink pull-up,
		 * the subquery will become a jointree, inside which the tuples' order
		 * doesn't matter. In a summary, it's safe to elimate the group-by or
		 * order-by causes here.
		 */
		cdbsubselect_drop_orderby(subselect);
		cdbsubselect_drop_distinct(subselect);

		int			subq_indx      = add_notin_subquery_rte(parse, subselect);
		List       *inner_exprs    = NIL;
		List       *outer_exprs    = NIL;
		bool        nullable       = true;
		JoinExpr   *join_expr      = make_join_expr(NULL, subq_indx, JOIN_LASJ_NOTIN);

		join_expr->quals = make_lasj_quals(root, sublink, subq_indx);

		inner_exprs = fetch_targetlist_exprs(subselect->targetList);
		nullable = is_exprs_nullable((Node *) inner_exprs, subselect);
		if (!nullable)
		{
			outer_exprs = fetch_outer_exprs(sublink->testexpr);
			nullable = is_exprs_nullable((Node *) outer_exprs, parse);
		}

		if (nullable)
			join_expr->quals = add_null_match_clause(join_expr->quals);

		return join_expr;
	}
	/* Not safe to perform transformation. */
	return NULL;
}

/*
 * Check if there is a range table entry of type func expr whose arguments
 * are correlated
 */
bool
has_correlation_in_funcexpr_rte(List *rtable)
{
	/*
	 * check if correlation occurs in a func expr in the from clause of the
	 * subselect
	 */
	ListCell   *lc_rte;

	foreach(lc_rte, rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(lc_rte);

		if (rte->functions  && contain_vars_of_level_or_above((Node *) rte->functions, 1))
		{
			return true;
		}
	}
	return false;
}

static List *
cdb_find_all_vars(Node *exprs, List *rtable)
{
	FindAllVarsContext    context;

	context.rtable = rtable;
	context.vars = NIL;

	expression_tree_walker(exprs, cdb_find_all_vars_walker, (void *) &context);

	return context.vars;
}

static bool
cdb_find_all_vars_walker(Node *node, FindAllVarsContext *context)
{
	if (node == NULL)
		return false;

	if (IsA(node, Var))
	{
		Var *var = (Var *)node;

		if (var->varlevelsup != 0)
			return false;

		/*
		 * The vars fetched from targetList/testexpr.. can be from virtual range table (RTE_JOIN),
		 * which do not directly match base vars fetched by cdb_find_nonnullable_vars_walker, aligning
		 * them to base vars is needed before check nullable.
		 */
		var = cdb_map_to_base_var((Var *) node, context->rtable);

		if (var != NULL)
			context->vars = list_append_unique(context->vars, var);

		return false;
	}

	return expression_tree_walker(node, cdb_find_all_vars_walker, context);
}

static Var *
cdb_map_to_base_var(Var *var, List *rtable)
{
	RangeTblEntry *rte    = rt_fetch(var->varno, rtable);

	while (rte != NULL && rte->rtekind == RTE_JOIN && rte->joinaliasvars)
	{
		Node *node = list_nth(rte->joinaliasvars, var->varattno-1);
		/*
		 * Per the comments of the field joinaliasvars of struct RangeTblEntry,
		 * it might be Var or COALESCE expr or NULL pointer. For cases other than
		 * a simple Var, return NULL is a safe choice. See Github Issue
		 * https://github.com/greenplum-db/gpdb/issues/14858 for details.
		 */
		if (node != NULL && IsA(node, Var))
		{
			var = (Var *) node;
			rte = rt_fetch(var->varno, rtable);
		}
		else
			return NULL;
	}

	/* not found RTE in current level rtable */
	if (rte == NULL)
		return NULL;

	return var;
}
