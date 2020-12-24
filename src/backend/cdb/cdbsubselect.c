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
	Query	   *query;			/* Query in question. */
	List	   *nonNullableVars;	/* Known non-nullable vars */
} NonNullableVarsContext;


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

static bool find_nonnullable_vars_walker(Node *node, NonNullableVarsContext *context);
static bool is_attribute_nonnullable(Oid relationOid, AttrNumber attrNumber);
static bool is_targetlist_nullable(Query *subq);

#define DUMMY_COLUMN_NAME "zero"

/*
 * cdbsubselect_drop_distinct
 *
 * In an IN, EXISTS, NOT IN or NOT EXISTS subquery, any duplicates in the
 * subselect will not affect the overall result, so we can throw away any
 * DISTINCT clause. Unless there's a LIMIT.
 */
void
cdbsubselect_drop_distinct(Query *subselect)
{
	if (subselect->limitCount == NULL &&
		subselect->limitOffset == NULL)
	{
		/* Delete DISTINCT. */
		subselect->distinctClause = NIL;

		/* Delete GROUP BY if subquery has no aggregates and no HAVING. */
		if (!subselect->hasAggs &&
			subselect->havingQual == NULL)
			subselect->groupClause = NIL;
	}
}								/* cdbsubselect_drop_distinct */


/*
 * cdbsubselect_drop_orderby
 *
 * In a subquery, the order of the rows subselect's results won't make a
 * difference to the overall result, so we can throw away any ORDER BY.
 * Unless there's a LIMIT.
 */
void
cdbsubselect_drop_orderby(Query *subselect)
{
	if (subselect->limitCount == NULL &&
		subselect->limitOffset == NULL)
	{
		/* Delete ORDER BY. */
		subselect->sortClause = NIL;
	}
}								/* cdbsubselect_drop_orderby */


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
 */
static bool
IsCorrelatedEqualityOpExpr(OpExpr *opexp, Expr **innerExpr, Oid *eqOp, Oid *sortOp, bool *hashable)
{
	Oid			opfamily;
	Oid			ltype;
	Oid			rtype;
	List	   *l;

	Assert(opexp);
	Assert(list_length(opexp->args) > 1);
	Assert(innerExpr);
	Assert(eqOp);
	Assert(sortOp);

	/*
	 * If this is an expression of the form a = b, then we want to know about
	 * the vars involved.
	 */
	if (!op_mergejoinable(opexp->opno, exprType(linitial(opexp->args))))
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
	 * Look up the correct sort operator from the chosen opfamily.
	 */
	ltype = exprType(linitial(opexp->args));
	rtype = exprType(lsecond(opexp->args));
	*eqOp = get_opfamily_member(opfamily, ltype, rtype, BTEqualStrategyNumber);
	if (!OidIsValid(*eqOp))	/* should not happen */
		elog(ERROR, "could not find member %d(%u,%u) of opfamily %u",
			 BTEqualStrategyNumber, ltype, rtype, opfamily);

	*sortOp = get_opfamily_member(opfamily, ltype, rtype, BTLessStrategyNumber);
	if (!OidIsValid(*sortOp))	/* should not happen */
		elog(ERROR, "could not find member %d(%u,%u) of opfamily %u",
			 BTLessStrategyNumber, ltype, rtype, opfamily);

	*hashable = op_hashjoinable(*eqOp, ltype);

	if (!IsCorrelatedOpExpr(opexp, innerExpr))
		return false;

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
	 * If deeply correlated, don't bother.
	 */
	if (IsSubqueryMultiLevelCorrelated(subselect))
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
		 * Don't care about order by clause
		 */
		cdbsubselect_drop_orderby(subselect);

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
find_nonnullable_vars_walker(Node *node, NonNullableVarsContext *context)
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
					c1.nonNullableVars = NIL;
					ListCell   *lc = NULL;
					int			orArgNum = 0;

					foreach(lc, expr->args)
					{
						Node	   *orArg = lfirst(lc);

						NonNullableVarsContext c2;

						c2.query = context->query;
						c2.nonNullableVars = NIL;
						expression_tree_walker(orArg, find_nonnullable_vars_walker, &c2);

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

					foreach(lc, context->query->targetList)
					{
						TargetEntry *tle = (TargetEntry *) lfirst(lc);

						Assert(tle->expr);

						if (nodeTag(tle->expr) == T_Var)
						{
							Var		   *var = (Var *) tle->expr;

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
	return expression_tree_walker(node, find_nonnullable_vars_walker, context);
}


/*
 * This function is used to determine whether the parameters of an expression in
 * ALL Sublink can be NULL.
 */
static bool
is_param_nullable(Node *node, Query *query, Value *oprname)
{
	bool result = false;
	NonNullableVarsContext context;
	Expr *expr;
	ListCell *lc;
	Expr *arg;

	Assert(query);
	context.query = query;
	context.nonNullableVars = NIL;

	/* Find nullable vars in the jointree */
	expression_tree_walker((Node *) query->jointree, find_nonnullable_vars_walker, &context);

	/*
	 * A null value "not in / > all / < all" a non-empty set, the result is
	 * always false, but a null value "not in / > all / < all" a empty set, the
	 * result is always true. So if the param is nullable, we should not make
	 * the locus as "Partitioned".
	 * If the sql is "... a not in (select ...)", the node should be a BoolExpr.
	 * if the sql is "... a < all (select ...), the node should be a OpExpr"
	 */
	if (nodeTag(node) == T_BoolExpr)
	{
		if(((BoolExpr *) node)->boolop != NOT_EXPR)
			return false;
		expr = lfirst(list_head(((BoolExpr*) node)->args));
	}
	else if (nodeTag(node) == T_OpExpr)
	{
		if(strcmp(oprname->val.str, "=") == 0)
			return false;
		expr = (Expr *) node;
	}
	else
		return true;

	if (nodeTag(expr) != T_OpExpr)
		return true;

	foreach(lc, ((OpExpr*)expr)->args)
	{
		arg = lfirst(lc);

		if (nodeTag(arg) == T_RelabelType)
			arg = ((RelabelType*)arg)->arg;

		if (nodeTag(arg) == T_Param)
			continue;
		else if (nodeTag(arg) == T_Const)
		{
			/*
			 * Is the constant entry in the targetlist null?
			 */
			Const	   *constant = (Const *) arg;

			/*
			 * Note: the 'dummy' column is not NULL, so we don't need any special handling for it
			 */
			if (constant->constisnull == true)
				result = true;
		}
		else if (nodeTag(arg) == T_Var)
		{
			Var		   *var = (Var *) arg;

			/* Was this var determined to be non-nullable? */
			if (!list_member(context.nonNullableVars, var))
			{
				result = true;
			}
		}
		else
			result = true;
	}

	return result;
}

/**
 * This method determines if the targetlist of a query is nullable.
 * Consider a query of the form: select t1.x, t2.y from t1, t2 where t1.x > 5
 * This method simply determines if the targetlist i.e. (t1.x, t2.y) is nullable.
 * A targetlist is "nullable" if all entries in the targetlist
 * cannot be proven to be non-nullable.
 *
 * We don't use NULL for the 'dummy' column, because is_targetlist_nullable() 
 * would then treat the target list as nullable
 */
static bool
is_targetlist_nullable(Query *subq)
{
	Assert(subq);

	if (subq->setOperations)
	{
		/* TODO: support setops in subselect someday */
		return false;
	}

	/**
	 * Find all non-nullable vars in the query i.e. the set of vars,
	 * if part of the targetlist, that would be non-nullable. E.g.
	 * select x from t1 where x is not null
	 * would produce {x}.
	 */
	NonNullableVarsContext context;

	context.query = subq;
	context.nonNullableVars = NIL;
	expression_tree_walker((Node *) subq->jointree, find_nonnullable_vars_walker, &context);

	bool		result = false;

	/**
	 * Now cross-check with the actual targetlist of the query.
	 */
	ListCell   *lc = NULL;

	foreach(lc, subq->targetList)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);

		if (tle->resjunk)
		{
			/**
			 * Be conservative.
			 */
			result = true;
		}

		if (nodeTag(tle->expr) == T_Const)
		{
			/**
			 * Is the constant entry in the targetlist null?
			 */
			Const	   *constant = (Const *) tle->expr;

			/**
			 *  Note: the 'dummy' column is not NULL, so we don't need any special handling for it 
			 */	
			if (constant->constisnull == true)
			{
				result = true;
			}
		}
		else if (nodeTag(tle->expr) == T_Var)
		{
			Var		   *var = (Var *) tle->expr;

			/* Was this var determined to be non-nullable? */
			if (!list_member(context.nonNullableVars, var))
			{
				result = true;
			}
		}
		else
		{
			/**
			 * Be conservative.
			 */
			result = true;
		}
	}

	return result;
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
		/* Delete ORDER BY and DISTINCT. */
		cdbsubselect_drop_orderby(subselect);
		cdbsubselect_drop_distinct(subselect);

		int			subq_indx = add_notin_subquery_rte(parse, subselect);
		bool		inner_nullable = is_targetlist_nullable(subselect);

		ListCell   *lc = list_head(sublink->operName);
		bool		outer_nullable = is_param_nullable(sublink->testexpr,
										   root->parse,
										   lc? list_head(sublink->operName)->data.ptr_value : NULL);
		JoinExpr   *join_expr = make_join_expr(NULL, subq_indx, JOIN_LASJ_NOTIN);

		join_expr->quals = make_lasj_quals(root, sublink, subq_indx);

		if (inner_nullable || outer_nullable)
		{
			join_expr->quals = add_null_match_clause(join_expr->quals);
		}

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
