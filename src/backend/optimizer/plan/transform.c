/*-------------------------------------------------------------------------
 *
 * transform.c
 *	  This file contains methods to transform the query tree
 *
 * Portions Copyright (c) 2011, EMC Greenplum
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/plan/transform.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "funcapi.h"
#include "nodes/parsenodes.h"
#include "nodes/makefuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/optimizer.h"
#include "optimizer/transform.h"
#include "utils/lsyscache.h"
#include "catalog/pg_proc.h"
#include "parser/parse_oper.h"
#include "parser/parse_coerce.h"
#include "parser/parse_relation.h"
#include "utils/fmgroids.h"

/**
 * Static declarations
 */
static Node *replace_sirv_functions_mutator(Node *node, void *context);
static void replace_sirvf_tle(Query *query, int tleOffset);
static RangeTblEntry *replace_sirvf_rte(Query *query, RangeTblEntry *rte);
static Node *replace_sirvf_tle_expr_mutator(Node *node, void *context);
static SubLink *make_sirvf_subselect(FuncExpr *fe);
static Query *make_sirvf_subquery(FuncExpr *fe);
static bool safe_to_replace_sirvf_tle(Query *query);
static bool safe_to_replace_sirvf_rte(Query *query);

/**
 * Normalize query before planning.
 */
Query *
normalize_query(Query *query)
{
	bool		copied = false;
	Query	   *res = query;
#ifdef USE_ASSERT_CHECKING
	Query	   *qcopy = (Query *) copyObject(query);
#endif

	/*
	 * MPP-12635 Replace all instances of single row returning volatile (sirv)
	 * functions.
	 *
	 * Only do the transformation on the target list for INSERT/UPDATE/DELETE
	 * and CREATE TABLE AS commands; there's no need to complicate simple
	 * queries like "SELECT function()", which would be executed on the QD
	 * anyway.
	 */
	if (res->commandType != CMD_SELECT || res->parentStmtType != PARENTSTMTTYPE_NONE)
	{
		if (safe_to_replace_sirvf_tle(res))
		{
			if (!copied)
			{
				res = (Query *) copyObject(query);
				copied = true;
			}
			for (int tleOffset = 0; tleOffset < list_length(res->targetList); tleOffset++)
			{
				replace_sirvf_tle(res, tleOffset + 1);
			}
		}
	}

	/*
	 * Find sirv functions in the range table entries and replace them
	 */
	if (safe_to_replace_sirvf_rte(res))
	{
		ListCell   *lc;

		if (!copied)
		{
			res = (Query *) copyObject(query);
			copied = true;
		}

		foreach(lc, res->rtable)
		{
			RangeTblEntry *rte = (RangeTblEntry *) lfirst(lc);

			replace_sirvf_rte(res, rte);
		}
	}

	res = query_tree_mutator(res, replace_sirv_functions_mutator, NULL, 0);

#ifdef USE_ASSERT_CHECKING
	Assert(equal(qcopy, query) && "Normalization should not modify original query object");
#endif

	return res;
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
			ListCell *lc;

			foreach(lc, query->rtable)
			{
				RangeTblEntry *rte = (RangeTblEntry *) lfirst(lc);

				rte = replace_sirvf_rte(query, rte);

				lfirst(lc) = rte;
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
static bool
is_sirv_funcexpr(FuncExpr *fe)
{
	if (fe->funcretset)
		return false;	/* Set returning functions cannot become initplans */

	if (fe->is_tablefunc)
		return false;	/* Ignore table functions */

	if (contain_vars_of_level_or_above((Node *) fe->args, 0))
		return false;	/* Must be variable free */

	if (contain_subplans((Node *) fe->args))
		return false;	/* Must not contain sublinks */

	if (func_volatile(fe->funcid) != PROVOLATILE_VOLATILE)
		return false;	/* Must be a volatile function */

	if (fe->funcresulttype == RECORDOID)
		return false;	/* Record types cannot be handled currently */

	if (fe->funcid == F_NEXTVAL_OID || fe->funcid == F_CURRVAL_OID || fe-> funcid == F_SETVAL_OID)
		return false;	/* Function cannot be sequence related */

	return true;
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
 *
 * For example, in a query like:
 *
 * SELECT * FROM func();
 *
 * The 'fe' argument represents the function call. The function returns a
 * subquery to replace the func() RTE.
 *
 * If the function returns a scalar type, it's transformed into:
 *
 * (SELECT func())
 *
 * If it returns a composite (or record) type, we need to work a bit harder, to
 * get a subquery with a targetlist that's compatible with the original one.
 * It's transformed into:
 *
 * (SELECT (func).col1, (func).col2, ... FROM (SELECT func()) AS func)
 */
static Query *
make_sirvf_subquery(FuncExpr *fe)
{
	SubLink	   *sl;
	TargetEntry *tle;
	TypeFuncClass funcclass;
	Oid			resultTypeId;
	TupleDesc	resultTupleDesc;
	Query	   *sq;
	char	   *funcname = get_func_name(fe->funcid);

	sq = makeNode(Query);
	sq->commandType = CMD_SELECT;
	sq->querySource = QSRC_PLANNER;
	sq->jointree = makeNode(FromExpr);

	sl = make_sirvf_subselect(fe);
	tle = (TargetEntry *) makeTargetEntry((Expr *) sl, 1, funcname, false);
	sq->targetList = list_make1(tle);
	sq->hasSubLinks = true;

	funcclass = get_expr_result_type((Node *) fe, &resultTypeId, &resultTupleDesc);

	if (funcclass == TYPEFUNC_COMPOSITE ||
		funcclass == TYPEFUNC_COMPOSITE_DOMAIN ||
		funcclass == TYPEFUNC_RECORD)
	{
		Query	   *sub_sq = sq;
		RangeTblEntry *rte;
		RangeTblRef *rtref;
		int			attno;
		Var		   *var;

		// FIXME: does this need to be lateral?
		rte = addRangeTableEntryForSubquery(NULL,
											sub_sq,
											makeAlias("sirvf_sq", NIL),
											false, /* isLateral? */
											true);
		rtref = makeNode(RangeTblRef);
		rtref->rtindex = 1;

		sq = makeNode(Query);
		sq->commandType = CMD_SELECT;
		sq->querySource = QSRC_PLANNER;
		sq->rtable = list_make1(rte);
		sq->jointree = makeFromExpr(list_make1(rtref), NULL);
		/*
		 * XXX: I'm not sure if we need so set this in this middle subquery.
		 * This query doesn't have SubLinks, but its subquery does.
		 */
		sq->hasSubLinks = true;

		var = makeVar(1, 1, exprType((Node *) sl), -1, 0, 0);

		for (attno = 1; attno <= resultTupleDesc->natts; attno++)
		{
			Form_pg_attribute attr = TupleDescAttr(resultTupleDesc, attno - 1);
			FieldSelect *fs;

			fs = (FieldSelect *) makeNode(FieldSelect);
			fs->arg = (Expr *) var;
			fs->fieldnum = attno;
			fs->resulttype = attr->atttypid;
			fs->resulttypmod = attr->atttypmod;
			fs->resultcollid = attr->attcollation;

			tle = (TargetEntry *) makeTargetEntry((Expr *) fs, attno, NameStr(attr->attname), false);
			sq->targetList = lappend(sq->targetList, tle);
		}
	}

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
				ListCell *lcrtfunc;

				/* GPDB_94_MERGE_FIXME: The SIRV transformation can't handle WITH ORDINALITY
				 * currently */
				if (rte->funcordinality)
					return false;

				foreach(lcrtfunc, rte->functions)
				{
					RangeTblFunction *rtfunc = (RangeTblFunction *) lfirst(lcrtfunc);

					/*
					 * This runs before const-evaluation, so the expressions should
					 * be FuncExprs still. But better safe than sorry.
					 */
					if (!IsA(rtfunc->funcexpr, FuncExpr))
						return false;

					if (((FuncExpr *) rtfunc->funcexpr)->funcretset)
					{
						/* SRF in FROM clause */
						return false;
					}
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
 * If a range table entry contains a sirv function, this must be replaced
 * with a derived table (subquery) with a sublink - this will eventually be
 * turned into an initplan.
 *
 * Conceptually,
 *
 * SELECT * FROM FOO(1) t1
 *
 * is turned into
 *
 * SELECT * FROM (SELECT FOO(1)) t1
 */
static RangeTblEntry *
replace_sirvf_rte(Query *query, RangeTblEntry *rte)
{
	Assert(query);
	Assert(safe_to_replace_sirvf_rte(query));

	if (rte->rtekind == RTE_FUNCTION)
	{
		/*
		 * We only deal with the simple cases, with a single function.
		 * I.e. not ROWS FROM() with multiple functions.
		 */
		if (list_length(rte->functions) == 1)
		{
			RangeTblFunction *rtfunc = (RangeTblFunction *) linitial(rte->functions);
			FuncExpr *fe = (FuncExpr *) rtfunc->funcexpr;
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
				Query *subquery = make_sirvf_subquery(fe);

				rte->functions = NIL;

				/**
				 * Turn the range table entry to the kind RTE_SUBQUERY
				 */
				rte->rtekind = RTE_SUBQUERY;
				rte->subquery = subquery;
			}
		}
	}

	return rte;
}
