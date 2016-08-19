/*
 * parse_cte.c
 *    Handle WITH clause in parser.
 *
 * Copyright (c) 2011 - present, EMC Greenplum.
 */
#include "postgres.h"

#include "parser/analyze.h"
#include "parser/parse_node.h"
#include "parser/parse_cte.h"
#include "parser/parse_expr.h"
#include "parser/parse_relation.h"
#include "nodes/parsenodes.h"
#include "nodes/nodeFuncs.h"

static void analyzeCTE(ParseState *pstate, CommonTableExpr *cte);
static void analyzeCTETargetList(ParseState *pstate, CommonTableExpr *cte, List *tlist);

/*
 * transformWithClause -
 *	  Transform the list of WITH clause "common table expressions" into
 *	  Query nodes.
 *
 * The result is the list of transformed CTEs to be put into the output
 * Query.  (This is in fact the same as the ending value of p_ctenamespace,
 * but it seems cleaner to not expose that in the function's API.)
 */
List *
transformWithClause(ParseState *pstate, WithClause *withClause)
{
	ListCell   *lc;

	if (withClause == NULL)
		return NULL;
	
	if (withClause->recursive)
	{
		ereport(ERROR, 
				(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
				 errmsg("RECURSIVE option in WITH clause is not supported")));
	}

	/* Only one WITH clause per query level */
	Assert(pstate->p_ctenamespace == NIL);
	Assert(pstate->p_future_ctes == NIL);

	/*
	 * For either type of WITH, there must not be duplicate CTE names in the
	 * list.  Check this right away so we needn't worry later.
	 *
	 * Also, initialize other variables in CommonTableExpr.
	 */
	foreach(lc, withClause->ctes)
	{
		CommonTableExpr *cte = (CommonTableExpr *) lfirst(lc);
		ListCell   *rest;

		for_each_cell(rest, lnext(lc))
		{
			CommonTableExpr *cte2 = (CommonTableExpr *) lfirst(rest);

			if (strcmp(cte->ctename, cte2->ctename) == 0)
				ereport(ERROR,
						(errcode(ERRCODE_DUPLICATE_ALIAS),
					errmsg("WITH query name \"%s\" specified more than once",
						   cte2->ctename),
						 parser_errposition(pstate, cte2->location)));
		}

		cte->cterecursive = false;
		cte->cterefcount = 0;
	}

	/*
	 * For non-recursive WITH, just analyze each CTE in sequence and then
	 * add it to the ctenamespace.	This corresponds to the spec's
	 * definition of the scope of each WITH name.  However, to allow error
	 * reports to be aware of the possibility of an erroneous reference,
	 * we maintain a list in p_future_ctes of the not-yet-visible CTEs.
	 */
	pstate->p_future_ctes = list_copy(withClause->ctes);

	foreach (lc, withClause->ctes)
	{
		CommonTableExpr *cte = (CommonTableExpr *)lfirst(lc);
		analyzeCTE(pstate, cte);
		pstate->p_ctenamespace = lappend(pstate->p_ctenamespace, cte);
		pstate->p_future_ctes = list_delete_first(pstate->p_future_ctes);
	}

	return pstate->p_ctenamespace;
}

/*
 * GetCTEForRTE
 *    Find CommonTableExpr stored in pstate that is referenced by rte.
 *
 * rtelevelsup is the number of query levels above the given pstate that the
 * RTE came from.  Callers that don't have this information readily available
 * may pass -1 instead.
 *
 * Report error if not such CTE found.
 */
CommonTableExpr *
GetCTEForRTE(ParseState *pstate, RangeTblEntry *rte, int rtelevelsup)
{
	Assert(pstate != NULL && rte != NULL);
	Assert(rte->rtekind == RTE_CTE);

	/* Determine RTE's levelsup if caller didn't know it */
	if (rtelevelsup < 0)
		(void) RTERangeTablePosn(pstate, rte, &rtelevelsup);

	Index levelsup = rte->ctelevelsup + rtelevelsup;
	while (levelsup > 0)
	{
		pstate = pstate->parentParseState;
		Assert(pstate != NULL);
		levelsup--;
	}

	if (pstate->p_ctenamespace == NULL)
		return NULL;
	
	ListCell *lc;
	foreach (lc, pstate->p_ctenamespace)
	{
		CommonTableExpr *cte = (CommonTableExpr *)lfirst(lc);
		if (strcmp(cte->ctename, rte->ctename) == 0)
		{
			return cte;
		}
	}
	
	/* shouldn't happen */
	elog(ERROR, "unexpected error while parsing WITH query \"%s\"", rte->ctename);
	return NULL;
}


/*
 * analyzeCTE
 *    Analyze the given CommonTableExpr.
 *
 * Report errors if the given CTE has one of the following:
 *   (1) not a Query statement.
 *   (2) containing INTO clause.
 */
static void
analyzeCTE(ParseState *pstate, CommonTableExpr *cte)
{
	Query	   *query;

	Assert(cte != NULL);
	Assert(cte->ctequery != NULL);
	Assert(!IsA(cte->ctequery, Query));

	query = parse_sub_analyze(cte->ctequery, pstate);
	cte->ctequery = (Node *) query;

	Assert(IsA(query, Query));

	/* Check if the query is what we expected. */
	if (!IsA(query, Query))
		elog(ERROR, "unexpected non-Query statement in WITH clause");
	if (query->utilityStmt != NULL)
		elog(ERROR, "unexpected utility statement in WITH clause");

	if (query->intoClause)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("query defined in WITH clause cannot have SELECT INTO"),
				 parser_errposition(pstate,
									exprLocation((Node *) query->intoClause))));

	/* CTE queries are always marked as not canSetTag */
	query->canSetTag = false;

	/* Compute the column types, typmods. */
	analyzeCTETargetList(pstate, cte, GetCTETargetList(cte));
}

/*
 * reportDuplicateNames
 *    Report error when a given list of names (in String) contain duplicate values for a given
 * query name.
 */
static void
reportDuplicateNames(const char *queryName, List *names)
{
	if (names == NULL)
		return;

	ListCell *lc;
	foreach (lc, names)
	{
		Value *string = (Value *)lfirst(lc);
		Assert(IsA(string, String));

		ListCell *rest;
		for_each_cell(rest, lnext(lc))
		{
			Value *string2 = (Value *)lfirst(rest);
			Assert(IsA(string, String));
			
			if (strcmp(strVal(string), strVal(string2)) == 0)
			{
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("WITH query \"%s\" must not have duplicate column name: %s",
								queryName, strVal(string)),
						 errhint("Specify a column list without duplicate names")));
			}
		}
	}
}

/*
 * analayzeCTETargetList
 *    Compute colnames, coltypes, and coltypmods from the targetlist generated
 * after analyze.
 */
static void
analyzeCTETargetList(ParseState *pstate, CommonTableExpr *cte, List *tlist)
{
	int			numaliases;
	int			varattno;
	ListCell   *tlistitem;

	/* Not done already ... */
	Assert(cte->ctecolnames == NIL);

	/*
	 * We need to determine column names and types.  The alias column names
	 * override anything coming from the query itself.  (Note: the SQL spec
	 * says that the alias list must be empty or exactly as long as the output
	 * column set. Also, the alias can not have the same name. We report
	 * errors if this is not the case.)
	 */
	cte->ctecolnames = copyObject(cte->aliascolnames);
	cte->ctecoltypes = cte->ctecoltypmods = NIL;
	numaliases = list_length(cte->aliascolnames);
	varattno = 0;
	foreach(tlistitem, tlist)
	{
		TargetEntry *te = (TargetEntry *) lfirst(tlistitem);
		Oid			coltype;
		int32		coltypmod;

		if (te->resjunk)
			continue;
		varattno++;
		Assert(varattno == te->resno);
		if (varattno > numaliases)
		{
			char	   *attrname;

			if (numaliases > 0)
			{
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg(ERRMSG_GP_WITH_COLUMNS_MISMATCH, cte->ctename),
						 parser_errposition(pstate, cte->location)));
			}

			attrname = pstrdup(te->resname);
			cte->ctecolnames = lappend(cte->ctecolnames, makeString(attrname));
		}
		coltype = exprType((Node *) te->expr);
		coltypmod = exprTypmod((Node *) te->expr);

		cte->ctecoltypes = lappend_oid(cte->ctecoltypes, coltype);
		cte->ctecoltypmods = lappend_int(cte->ctecoltypmods, coltypmod);
	}

	if (varattno < numaliases)
	{
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg(ERRMSG_GP_WITH_COLUMNS_MISMATCH, cte->ctename),
				 parser_errposition(pstate, cte->location)));
	}

	reportDuplicateNames(cte->ctename, cte->ctecolnames);
}
