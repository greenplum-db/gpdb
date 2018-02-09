/*-------------------------------------------------------------------------
 *
 * analyze.c
 *	  transform the raw parse tree into a query tree
 *
 * For optimizable statements, we are careful to obtain a suitable lock on
 * each referenced table, and other modules of the backend preserve or
 * re-obtain these locks before depending on the results.  It is therefore
 * okay to do significant semantic analysis of these statements.  For
 * utility commands, no locks are obtained here (and if they were, we could
 * not be sure we'd still have them at execution).  Hence the general rule
 * for utility commands is to just dump them into a Query node untransformed.
 * DECLARE CURSOR and EXPLAIN are exceptions because they contain
 * optimizable statements.
 *
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *	$PostgreSQL: pgsql/src/backend/parser/analyze.c,v 1.398 2009/12/16 22:24:13 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/sysattr.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/plancat.h"
#include "optimizer/tlist.h"
#include "optimizer/var.h"
#include "parser/analyze.h"
#include "parser/parse_agg.h"
#include "parser/parse_clause.h"
#include "parser/parse_coerce.h"
#include "parser/parse_cte.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "parser/parse_param.h"
#include "parser/parse_relation.h"
#include "parser/parse_target.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"
#include "utils/rel.h"

#include "cdb/cdbvars.h"
#include "catalog/gp_policy.h"
#include "optimizer/clauses.h"
#include "optimizer/tlist.h"
#include "parser/parse_func.h"
#include "utils/lsyscache.h"


/* Context for transformGroupedWindows() which mutates components
 * of a query that mixes windowing and aggregation or grouping.  It
 * accumulates context for eventual construction of a subquery (the
 * grouping query) during mutation of components of the outer query
 * (the windowing query).
 */
typedef struct
{
	List *subtlist; /* target list for subquery */
	List *subgroupClause; /* group clause for subquery */
	List *windowClause; /* window clause for outer query*/

	/* Scratch area for init_grouped_window context and map_sgr_mutator.
	 */
	Index *sgr_map;

	/* Scratch area for grouped_window_mutator and var_for_gw_expr.
	 */
	List *subrtable;
	int call_depth;
	TargetEntry *tle;
} grouped_window_ctx;

/* Working state for transformSetOperationTree_internal */
typedef struct
{
	int			ncols;
	List	  **leafinfos;
} setop_types_ctx;

static Query *transformDeleteStmt(ParseState *pstate, DeleteStmt *stmt);
static Query *transformInsertStmt(ParseState *pstate, InsertStmt *stmt);
static List *transformInsertRow(ParseState *pstate, List *exprlist,
				   List *stmtcols, List *icolumns, List *attrnos);

/*
 * MPP-2506 [insert/update/delete] RETURNING clause not supported:
 *   We have problems processing the returning clause, so for now we have
 *   simply removed it and replaced it with an error message.
 */
#define MPP_RETURNING_NOT_SUPPORTED
#ifndef MPP_RETURNING_NOT_SUPPORTED
static List *transformReturningList(ParseState *pstate, List *returningList);
#endif

static Query *transformSelectStmt(ParseState *pstate, SelectStmt *stmt);
static Query *transformValuesClause(ParseState *pstate, SelectStmt *stmt);
static Query *transformSetOperationStmt(ParseState *pstate, SelectStmt *stmt);
static Node *transformSetOperationTree(ParseState *pstate, SelectStmt *stmt,
						  bool isTopLevel, List **colInfo);
static Node *transformSetOperationTree_internal(ParseState *pstate, SelectStmt *stmt,
												bool isTopLevel, setop_types_ctx *setop_types);
static void coerceSetOpTypes(ParseState *pstate, Node *sop,
							 List *coltypes, List *coltypmods,
							 List **colInfo);
static void select_setop_types(ParseState *pstate, setop_types_ctx *ctx, SetOperation op, List **selected_types, List **selected_typmods);
static void determineRecursiveColTypes(ParseState *pstate,
									   Node *larg, List *lcolinfo);
static void applyColumnNames(List *dst, List *src);
static Query *transformUpdateStmt(ParseState *pstate, UpdateStmt *stmt);
static Query *transformDeclareCursorStmt(ParseState *pstate,
						   DeclareCursorStmt *stmt);
static Query *transformExplainStmt(ParseState *pstate,
					 ExplainStmt *stmt);
static void transformLockingClause(ParseState *pstate, Query *qry,
								   LockingClause *lc, bool pushedDown);

static void setQryDistributionPolicy(SelectStmt *stmt, Query *qry);

static Query *transformGroupedWindows(Query *qry);
static void init_grouped_window_context(grouped_window_ctx *ctx, Query *qry);
static Var *var_for_gw_expr(grouped_window_ctx *ctx, Node *expr, bool force);
static void discard_grouped_window_context(grouped_window_ctx *ctx);
static Node *map_sgr_mutator(Node *node, void *context);
static Node *grouped_window_mutator(Node *node, void *context);
static Alias *make_replacement_alias(Query *qry, const char *aname);
static char *generate_positional_name(AttrNumber attrno);
static List*generate_alternate_vars(Var *var, grouped_window_ctx *ctx);

/*
 * parse_analyze
 *		Analyze a raw parse tree and transform it to Query form.
 *
 * Optionally, information about $n parameter types can be supplied.
 * References to $n indexes not defined by paramTypes[] are disallowed.
 *
 * The result is a Query node.	Optimizable statements require considerable
 * transformation, while utility-type statements are simply hung off
 * a dummy CMD_UTILITY Query node.
 */
Query *
parse_analyze(Node *parseTree, const char *sourceText,
			  Oid *paramTypes, int numParams)
{
	ParseState *pstate = make_parsestate(NULL);
	Query	   *query;

	Assert(sourceText != NULL); /* required as of 8.4 */

	pstate->p_sourcetext = sourceText;

	if (numParams > 0)
		parse_fixed_parameters(pstate, paramTypes, numParams);

	query = transformStmt(pstate, parseTree);

	free_parsestate(pstate);

	return query;
}

/*
 * parse_analyze_varparams
 *
 * This variant is used when it's okay to deduce information about $n
 * symbol datatypes from context.  The passed-in paramTypes[] array can
 * be modified or enlarged (via repalloc).
 */
Query *
parse_analyze_varparams(Node *parseTree, const char *sourceText,
						Oid **paramTypes, int *numParams)
{
	ParseState *pstate = make_parsestate(NULL);
	Query	   *query;

	Assert(sourceText != NULL); /* required as of 8.4 */

	pstate->p_sourcetext = sourceText;

	parse_variable_parameters(pstate, paramTypes, numParams);

	query = transformStmt(pstate, parseTree);

	/* make sure all is well with parameter types */
	check_variable_parameters(pstate, query);

	free_parsestate(pstate);

	return query;
}

/*
 * parse_sub_analyze
 *		Entry point for recursively analyzing a sub-statement.
 */
Query *
parse_sub_analyze(Node *parseTree, ParseState *parentParseState,
				  CommonTableExpr *parentCTE,
				  LockingClause *lockclause_from_parent)
{
	ParseState *pstate = make_parsestate(parentParseState);
	Query	   *query;

	pstate->p_parent_cte = parentCTE;
	pstate->p_lockclause_from_parent = lockclause_from_parent;

	query = transformStmt(pstate, parseTree);

	free_parsestate(pstate);

	return query;
}

/*
 * transformStmt -
 *	  transform a Parse tree into a Query tree.
 */
Query *
transformStmt(ParseState *pstate, Node *parseTree)
{
	Query	   *result;

	switch (nodeTag(parseTree))
	{
			/*
			 * Optimizable statements
			 */
		case T_InsertStmt:
			result = transformInsertStmt(pstate, (InsertStmt *) parseTree);
			break;

		case T_DeleteStmt:
			result = transformDeleteStmt(pstate, (DeleteStmt *) parseTree);
			break;

		case T_UpdateStmt:
			result = transformUpdateStmt(pstate, (UpdateStmt *) parseTree);
			break;

		case T_SelectStmt:
			{
				SelectStmt *n = (SelectStmt *) parseTree;

				if (n->valuesLists)
					result = transformValuesClause(pstate, n);
				else if (n->op == SETOP_NONE)
					result = transformSelectStmt(pstate, n);
				else
					result = transformSetOperationStmt(pstate, n);
			}
			break;

			/*
			 * Special cases
			 */
		case T_DeclareCursorStmt:
			result = transformDeclareCursorStmt(pstate,
											(DeclareCursorStmt *) parseTree);
			break;

		case T_ExplainStmt:
			result = transformExplainStmt(pstate,
										  (ExplainStmt *) parseTree);
			break;

		default:

			/*
			 * other statements don't require any transformation; just return
			 * the original parsetree with a Query node plastered on top.
			 */
			result = makeNode(Query);
			result->commandType = CMD_UTILITY;
			result->utilityStmt = (Node *) parseTree;
			break;
	}

	/* Mark as original query until we learn differently */
	result->querySource = QSRC_ORIGINAL;
	result->canSetTag = true;

	if (pstate->p_hasDynamicFunction)
		result->hasDynamicFunctions = true;

	return result;
}

/*
 * analyze_requires_snapshot
 *		Returns true if a snapshot must be set before doing parse analysis
 *		on the given raw parse tree.
 *
 * Classification here should match transformStmt(); but we also have to
 * allow a NULL input (for Parse/Bind of an empty query string).
 */
bool
analyze_requires_snapshot(Node *parseTree)
{
	bool		result;

	if (parseTree == NULL)
		return false;

	switch (nodeTag(parseTree))
	{
			/*
			 * Optimizable statements
			 */
		case T_InsertStmt:
		case T_DeleteStmt:
		case T_UpdateStmt:
		case T_SelectStmt:
			result = true;
			break;

			/*
			 * Special cases
			 */
		case T_DeclareCursorStmt:
			/* yes, because it's analyzed just like SELECT */
			result = true;
			break;

		case T_ExplainStmt:

			/*
			 * We only need a snapshot in varparams case, but it doesn't seem
			 * worth complicating this function's API to distinguish that.
			 */
			result = true;
			break;

		default:
			/* utility statements don't have any active parse analysis */
			result = false;
			break;
	}

	return result;
}

/*
 * transformDeleteStmt -
 *	  transforms a Delete Statement
 */
static Query *
transformDeleteStmt(ParseState *pstate, DeleteStmt *stmt)
{
	Query	   *qry = makeNode(Query);
	Node	   *qual;

	qry->commandType = CMD_DELETE;

	/* set up range table with just the result rel */
	qry->resultRelation = setTargetTable(pstate, stmt->relation,
								  interpretInhOption(stmt->relation->inhOpt),
										 true,
										 ACL_DELETE);

	qry->distinctClause = NIL;

	/*
	 * The USING clause is non-standard SQL syntax, and is equivalent in
	 * functionality to the FROM list that can be specified for UPDATE. The
	 * USING keyword is used rather than FROM because FROM is already a
	 * keyword in the DELETE syntax.
	 */
	transformFromClause(pstate, stmt->usingClause);

	qual = transformWhereClause(pstate, stmt->whereClause,
								EXPR_KIND_WHERE, "WHERE");

	/*
	 * MPP-2506 [insert/update/delete] RETURNING clause not supported:
	 *   We have problems processing the returning clause, so for now we have
	 *   simply removed it and replaced it with an error message.
	 */
#ifdef MPP_RETURNING_NOT_SUPPORTED
	if (stmt->returningList)
	{
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("The RETURNING clause of the DELETE statement is not "
						"supported in this version of Greenplum Database.")));
	}
#else
	qry->returningList = transformReturningList(pstate, stmt->returningList);
#endif

	/* done building the range table and jointree */
	qry->rtable = pstate->p_rtable;
	qry->jointree = makeFromExpr(pstate->p_joinlist, qual);

	qry->hasSubLinks = pstate->p_hasSubLinks;
	qry->hasWindowFuncs = pstate->p_hasWindowFuncs;
	qry->hasAggs = pstate->p_hasAggs;
	qry->hasFuncsWithExecRestrictions = pstate->p_hasFuncsWithExecRestrictions;
	if (pstate->p_hasAggs)
		parseCheckAggregates(pstate, qry);
	if (pstate->p_hasTblValueExpr)
		parseCheckTableFunctions(pstate, qry);

	return qry;
}

/*
 * transformInsertStmt -
 *	  transform an Insert Statement
 */
static Query *
transformInsertStmt(ParseState *pstate, InsertStmt *stmt)
{
	Query	   *qry = makeNode(Query);
	SelectStmt *selectStmt = (SelectStmt *) stmt->selectStmt;
	List	   *exprList = NIL;
	bool		isGeneralSelect;
	List	   *sub_rtable;
	List	   *sub_relnamespace;
	List	   *sub_varnamespace;
	List	   *icolumns;
	List	   *attrnos;
	RangeTblEntry *rte;
	RangeTblRef *rtr;
	ListCell   *icols;
	ListCell   *attnos;
	ListCell   *lc;

	qry->commandType = CMD_INSERT;
	pstate->p_is_insert = true;

	/*
	 * We have three cases to deal with: DEFAULT VALUES (selectStmt == NULL),
	 * VALUES list, or general SELECT input.  We special-case VALUES, both for
	 * efficiency and so we can handle DEFAULT specifications.
	 *
	 * The grammar allows attaching ORDER BY, LIMIT, or FOR UPDATE to a
	 * VALUES clause.  If we have any of those, treat it as a general SELECT;
	 * so it will work, but you can't use DEFAULT items together with those.
	 */
	isGeneralSelect = (selectStmt && (selectStmt->valuesLists == NIL ||
									  selectStmt->sortClause != NIL ||
									  selectStmt->limitOffset != NULL ||
									  selectStmt->limitCount != NULL ||
									  selectStmt->lockingClause != NIL));

	/*
	 * If a non-nil rangetable/namespace was passed in, and we are doing
	 * INSERT/SELECT, arrange to pass the rangetable/namespace down to the
	 * SELECT.	This can only happen if we are inside a CREATE RULE, and in
	 * that case we want the rule's OLD and NEW rtable entries to appear as
	 * part of the SELECT's rtable, not as outer references for it.  (Kluge!)
	 * The SELECT's joinlist is not affected however.  We must do this before
	 * adding the target table to the INSERT's rtable.
	 */
	if (isGeneralSelect)
	{
		sub_rtable = pstate->p_rtable;
		pstate->p_rtable = NIL;
		sub_relnamespace = pstate->p_relnamespace;
		pstate->p_relnamespace = NIL;
		sub_varnamespace = pstate->p_varnamespace;
		pstate->p_varnamespace = NIL;
		/* There can't be any outer WITH to worry about */
		Assert(pstate->p_ctenamespace == NIL);
	}
	else
	{
		sub_rtable = NIL;		/* not used, but keep compiler quiet */
		sub_relnamespace = NIL;
		sub_varnamespace = NIL;
	}

	/*
	 * Must get write lock on INSERT target table before scanning SELECT, else
	 * we will grab the wrong kind of initial lock if the target table is also
	 * mentioned in the SELECT part.  Note that the target table is not added
	 * to the joinlist or namespace.
	 */
	qry->resultRelation = setTargetTable(pstate, stmt->relation,
										 false, false, ACL_INSERT);

	/* Validate stmt->cols list, or build default list if no list given */
	icolumns = checkInsertTargets(pstate, stmt->cols, &attrnos);
	Assert(list_length(icolumns) == list_length(attrnos));

	/*
	 * Determine which variant of INSERT we have.
	 */
	if (selectStmt == NULL)
	{
		/*
		 * We have INSERT ... DEFAULT VALUES.  We can handle this case by
		 * emitting an empty targetlist --- all columns will be defaulted when
		 * the planner expands the targetlist.
		 */
		exprList = NIL;
	}
	else if (isGeneralSelect)
	{
		/*
		 * We make the sub-pstate a child of the outer pstate so that it can
		 * see any Param definitions supplied from above.  Since the outer
		 * pstate's rtable and namespace are presently empty, there are no
		 * side-effects of exposing names the sub-SELECT shouldn't be able to
		 * see.
		 */
		ParseState *sub_pstate = make_parsestate(pstate);
		Query	   *selectQuery;

		/*
		 * Process the source SELECT.
		 *
		 * It is important that this be handled just like a standalone SELECT;
		 * otherwise the behavior of SELECT within INSERT might be different
		 * from a stand-alone SELECT. (Indeed, Postgres up through 6.5 had
		 * bugs of just that nature...)
		 */
		sub_pstate->p_rtable = sub_rtable;
		sub_pstate->p_joinexprs = NIL;	/* sub_rtable has no joins */
		sub_pstate->p_relnamespace = sub_relnamespace;
		sub_pstate->p_varnamespace = sub_varnamespace;

		selectQuery = transformStmt(sub_pstate, stmt->selectStmt);

		free_parsestate(sub_pstate);

		/* The grammar should have produced a SELECT, but it might have INTO */
		if (!IsA(selectQuery, Query) ||
			selectQuery->commandType != CMD_SELECT ||
			selectQuery->utilityStmt != NULL)
			elog(ERROR, "unexpected non-SELECT command in INSERT ... SELECT");
		if (selectQuery->intoClause)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("INSERT ... SELECT cannot specify INTO"),
					 parser_errposition(pstate,
						   exprLocation((Node *) selectQuery->intoClause))));

		/*
		 * Make the source be a subquery in the INSERT's rangetable, and add
		 * it to the INSERT's joinlist.
		 */
		rte = addRangeTableEntryForSubquery(pstate,
											selectQuery,
											makeAlias("*SELECT*", NIL),
											false);
		rtr = makeNode(RangeTblRef);
		/* assume new rte is at end */
		rtr->rtindex = list_length(pstate->p_rtable);
		Assert(rte == rt_fetch(rtr->rtindex, pstate->p_rtable));
		pstate->p_joinlist = lappend(pstate->p_joinlist, rtr);

		/*----------
		 * Generate an expression list for the INSERT that selects all the
		 * non-resjunk columns from the subquery.  (INSERT's tlist must be
		 * separate from the subquery's tlist because we may add columns,
		 * insert datatype coercions, etc.)
		 *
		 * Const and Param nodes of type UNKNOWN in the SELECT's targetlist
		 * no longer need special treatment here.  They'll be assigned proper
         * types later by coerce_type() upon assignment to the target columns.
		 * Otherwise this fails:  INSERT INTO foo SELECT 'bar', ... FROM baz
		 *----------
		 */
		expandRTE(rte, rtr->rtindex, 0, -1, false, NULL, &exprList);

		/* Prepare row for assignment to target table */
		exprList = transformInsertRow(pstate, exprList,
									  stmt->cols,
									  icolumns, attrnos);
	}
	else if (list_length(selectStmt->valuesLists) > 1)
	{
		/*
		 * Process INSERT ... VALUES with multiple VALUES sublists. We
		 * generate a VALUES RTE holding the transformed expression lists, and
		 * build up a targetlist containing Vars that reference the VALUES
		 * RTE.
		 */
		List	   *exprsLists = NIL;
		int			sublist_length = -1;

		/* process the WITH clause */
		if (selectStmt->withClause)
		{
			qry->hasRecursive = selectStmt->withClause->recursive;
			qry->cteList = transformWithClause(pstate, selectStmt->withClause);
		}

		foreach(lc, selectStmt->valuesLists)
		{
			List	   *sublist = (List *) lfirst(lc);

			/* Do basic expression transformation (same as a ROW() expr) */
			sublist = transformExpressionList(pstate, sublist, EXPR_KIND_VALUES);

			/*
			 * All the sublists must be the same length, *after*
			 * transformation (which might expand '*' into multiple items).
			 * The VALUES RTE can't handle anything different.
			 */
			if (sublist_length < 0)
			{
				/* Remember post-transformation length of first sublist */
				sublist_length = list_length(sublist);
			}
			else if (sublist_length != list_length(sublist))
			{
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("VALUES lists must all be the same length"),
						 parser_errposition(pstate,
											exprLocation((Node *) sublist))));
			}

			/* Prepare row for assignment to target table */
			sublist = transformInsertRow(pstate, sublist,
										 stmt->cols,
										 icolumns, attrnos);

			exprsLists = lappend(exprsLists, sublist);
		}

		/*
		 * There mustn't have been any table references in the expressions,
		 * else strange things would happen, like Cartesian products of those
		 * tables with the VALUES list ...
		 */
		if (pstate->p_joinlist != NIL)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("VALUES must not contain table references"),
					 parser_errposition(pstate,
							  locate_var_of_level((Node *) exprsLists, 0))));

		/*
		 * Another thing we can't currently support is NEW/OLD references in
		 * rules --- seems we'd need something like SQL99's LATERAL construct
		 * to ensure that the values would be available while evaluating the
		 * VALUES RTE.	This is a shame.  FIXME
		 */
		if (list_length(pstate->p_rtable) != 1 &&
			contain_vars_of_level((Node *) exprsLists, 0))
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("VALUES must not contain OLD or NEW references"),
					 errhint("Use SELECT ... UNION ALL ... instead."),
					 parser_errposition(pstate,
							  locate_var_of_level((Node *) exprsLists, 0))));

		/*
		 * Generate the VALUES RTE
		 */
		rte = addRangeTableEntryForValues(pstate, exprsLists, NULL, true);
		rtr = makeNode(RangeTblRef);
		/* assume new rte is at end */
		rtr->rtindex = list_length(pstate->p_rtable);
		Assert(rte == rt_fetch(rtr->rtindex, pstate->p_rtable));
		pstate->p_joinlist = lappend(pstate->p_joinlist, rtr);

		/*
		 * Generate list of Vars referencing the RTE
		 */
		expandRTE(rte, rtr->rtindex, 0, -1, false, NULL, &exprList);
	}
	else
	{
		/*
		 * Process INSERT ... VALUES with a single VALUES sublist.  We treat
		 * this case separately for efficiency.  The sublist is just computed
		 * directly as the Query's targetlist, with no VALUES RTE.  So it
		 * works just like a SELECT without any FROM.
		 */
		List	   *valuesLists = selectStmt->valuesLists;

		Assert(list_length(valuesLists) == 1);

		/* process the WITH clause */
		if (selectStmt->withClause)
		{
			qry->hasRecursive = selectStmt->withClause->recursive;
			qry->cteList = transformWithClause(pstate, selectStmt->withClause);
		}

		/* Do basic expression transformation (same as a ROW() expr) */
		exprList = transformExpressionList(pstate,
										   (List *) linitial(valuesLists),
										   EXPR_KIND_VALUES);

		/* Prepare row for assignment to target table */
		exprList = transformInsertRow(pstate, exprList,
									  stmt->cols,
									  icolumns, attrnos);
	}

	/*
	 * Generate query's target list using the computed list of expressions.
	 * Also, mark all the target columns as needing insert permissions.
	 */
	rte = pstate->p_target_rangetblentry;
	qry->targetList = NIL;
	icols = list_head(icolumns);
	attnos = list_head(attrnos);
	foreach(lc, exprList)
	{
		Expr	   *expr = (Expr *) lfirst(lc);
		ResTarget  *col;
		AttrNumber	attr_num;
		TargetEntry *tle;

		col = (ResTarget *) lfirst(icols);
		Assert(IsA(col, ResTarget));
		attr_num = (AttrNumber) lfirst_int(attnos);

		tle = makeTargetEntry(expr,
							  attr_num,
							  col->name,
							  false);
		qry->targetList = lappend(qry->targetList, tle);

		rte->modifiedCols = bms_add_member(rte->modifiedCols,
							  attr_num - FirstLowInvalidHeapAttributeNumber);

		icols = lnext(icols);
		attnos = lnext(attnos);
	}


	/*
	 * MPP-2506 [insert/update/delete] RETURNING clause not supported:
	 *   We have problems processing the returning clause, so for now we have
	 *   simply removed it and replaced it with an error message.
	 */
#ifdef MPP_RETURNING_NOT_SUPPORTED
	if (stmt->returningList)
	{
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("The RETURNING clause of the INSERT statement is not "
						"supported in this version of Greenplum Database.")));
	}
#else
	/*
	 * If we have a RETURNING clause, we need to add the target relation to
	 * the query namespace before processing it, so that Var references in
	 * RETURNING will work.  Also, remove any namespace entries added in a
	 * sub-SELECT or VALUES list.
	 */
	if (stmt->returningList)
	{
		pstate->p_relnamespace = NIL;
		pstate->p_varnamespace = NIL;
		addRTEtoQuery(pstate, pstate->p_target_rangetblentry,
					  false, true, true);
		qry->returningList = transformReturningList(pstate,
													stmt->returningList);
	}
#endif

	/* done building the range table and jointree */
	qry->rtable = pstate->p_rtable;
	qry->jointree = makeFromExpr(pstate->p_joinlist, NULL);

	qry->hasSubLinks = pstate->p_hasSubLinks;
	qry->hasFuncsWithExecRestrictions = pstate->p_hasFuncsWithExecRestrictions;

	return qry;
}

/*
 * Prepare an INSERT row for assignment to the target table.
 *
 * The row might be either a VALUES row, or variables referencing a
 * sub-SELECT output.
 */
static List *
transformInsertRow(ParseState *pstate, List *exprlist,
				   List *stmtcols, List *icolumns, List *attrnos)
{
	List	   *result;
	ListCell   *lc;
	ListCell   *icols;
	ListCell   *attnos;

	/*
	 * Check length of expr list.  It must not have more expressions than
	 * there are target columns.  We allow fewer, but only if no explicit
	 * columns list was given (the remaining columns are implicitly
	 * defaulted).	Note we must check this *after* transformation because
	 * that could expand '*' into multiple items.
	 */
	if (list_length(exprlist) > list_length(icolumns))
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("INSERT has more expressions than target columns"),
				 parser_errposition(pstate,
									exprLocation(list_nth(exprlist,
												  list_length(icolumns))))));
	if (stmtcols != NIL &&
		list_length(exprlist) < list_length(icolumns))
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("INSERT has more target columns than expressions"),
				 parser_errposition(pstate,
									exprLocation(list_nth(icolumns,
												  list_length(exprlist))))));

	/*
	 * Prepare columns for assignment to target table.
	 */
	result = NIL;
	icols = list_head(icolumns);
	attnos = list_head(attrnos);
	foreach(lc, exprlist)
	{
		Expr	   *expr = (Expr *) lfirst(lc);
		ResTarget  *col;

		col = (ResTarget *) lfirst(icols);
		Assert(IsA(col, ResTarget));

		expr = transformAssignedExpr(pstate, expr,
									 EXPR_KIND_INSERT_TARGET,
									 col->name,
									 lfirst_int(attnos),
									 col->indirection,
									 col->location);

		result = lappend(result, expr);

		icols = lnext(icols);
		attnos = lnext(attnos);
	}

	return result;
}

/*
 * If an input query (Q) mixes window functions with aggregate
 * functions or grouping, then (per SQL:2003) we need to divide
 * it into an outer query, Q', that contains no aggregate calls
 * or grouping and an inner query, Q'', that contains no window
 * calls.
 *
 * Q' will have a 1-entry range table whose entry corresponds to
 * the results of Q''.
 *
 * Q'' will have the same range as Q and will be pushed down into
 * a subquery range table entry in Q'.
 *
 * As a result, the depth of outer references in Q'' and below
 * will increase, so we need to adjust non-zero xxxlevelsup fields
 * (Var, Aggref, and WindowFunc nodes) in Q'' and below.  At the end,
 * there will be no levelsup items referring to Q'.  Prior references
 * to Q will now refer to Q''; prior references to blocks above Q will
 * refer to the same blocks above Q'.)
 *
 * We do all this by creating a new Query node, subq, for Q''.  We
 * modify the input Query node, qry, in place for Q'.  (Since qry is
 * also the input, Q, be careful not to destroy values before we're
 * done with them.
 */
static Query *
transformGroupedWindows(Query *qry)
{
	Query *subq;
	RangeTblEntry *rte;
	RangeTblRef *ref;
	Alias *alias;
	bool hadSubLinks = qry->hasSubLinks;

	grouped_window_ctx ctx;

	Assert(qry->commandType == CMD_SELECT);
	Assert(!PointerIsValid(qry->utilityStmt));
	Assert(qry->returningList == NIL);

	if ( !qry->hasWindowFuncs || !(qry->groupClause || qry->hasAggs) )
		return qry;

	/* Make the new subquery (Q'').  Note that (per SQL:2003) there
	 * can't be any window functions called in the WHERE, GROUP BY,
	 * or HAVING clauses.
	 */
	subq = makeNode(Query);
	subq->commandType = CMD_SELECT;
	subq->querySource = QSRC_PARSER;
	subq->canSetTag = true;
	subq->utilityStmt = NULL;
	subq->resultRelation = 0;
	subq->intoClause = NULL;
	subq->hasAggs = qry->hasAggs;
	subq->hasWindowFuncs = false; /* reevaluate later */
	subq->hasSubLinks = qry->hasSubLinks; /* reevaluate later */

	/* Core of subquery input table expression: */
	subq->rtable = qry->rtable; /* before windowing */
	subq->jointree = qry->jointree; /* before windowing */
	subq->targetList = NIL; /* fill in later */

	subq->returningList = NIL;
	subq->groupClause = qry->groupClause; /* before windowing */
	subq->havingQual = qry->havingQual; /* before windowing */
	subq->windowClause = NIL; /* by construction */
	subq->distinctClause = NIL; /* after windowing */
	subq->sortClause = NIL; /* after windowing */
	subq->limitOffset = NULL; /* after windowing */
	subq->limitCount = NULL; /* after windowing */
	subq->rowMarks = NIL;
	subq->setOperations = NULL;

	/* Check if there is a window function in the join tree. If so
	 * we must mark hasWindowFuncs in the sub query as well.
	 */
	if (contain_window_function((Node *)subq->jointree))
		subq->hasWindowFuncs = true;

	/* Make the single range table entry for the outer query Q' as
	 * a wrapper for the subquery (Q'') currently under construction.
	 */
	rte = makeNode(RangeTblEntry);
	rte->rtekind = RTE_SUBQUERY;
	rte->subquery = subq;
	rte->alias = NULL; /* fill in later */
	rte->eref = NULL; /* fill in later */
	rte->inFromCl = true;
	rte->requiredPerms = ACL_SELECT;
	/* Default?
	 * rte->inh = 0;
	 * rte->checkAsUser = 0;
	 * rte->pseudocols = 0;
	*/

	/* Make a reference to the new range table entry .
	 */
	ref = makeNode(RangeTblRef);
	ref->rtindex = 1;

	/* Set up context for mutating the target list.  Careful.
	 * This is trickier than it looks.  The context will be
	 * "primed" with grouping targets.
	 */
	init_grouped_window_context(&ctx, qry);

    /* Begin rewriting the outer query in place.
     */
	qry->hasAggs = false; /* by constuction */
	/* qry->hasSubLinks -- reevaluate later. */

	/* Core of outer query input table expression: */
	qry->rtable = list_make1(rte);
	qry->jointree = (FromExpr *)makeNode(FromExpr);
	qry->jointree->fromlist = list_make1(ref);
	qry->jointree->quals = NULL;
	/* qry->targetList -- to be mutated from Q to Q' below */

	qry->groupClause = NIL; /* by construction */
	qry->havingQual = NULL; /* by construction */

	/* Mutate the Q target list and windowClauses for use in Q' and, at the
	 * same time, update state with info needed to assemble the target list
	 * for the subquery (Q'').
	 */
	qry->targetList = (List*)grouped_window_mutator((Node*)qry->targetList, &ctx);
	qry->windowClause = (List*)grouped_window_mutator((Node*)qry->windowClause, &ctx);
	qry->hasSubLinks = checkExprHasSubLink((Node*)qry->targetList);

	/* New subquery fields
	 */
	subq->targetList = ctx.subtlist;
	subq->groupClause = ctx.subgroupClause;

	/* We always need an eref, but we shouldn't really need a filled in alias.
	 * However, view deparse (or at least the fix for MPP-2189) wants one.
	 */
	alias = make_replacement_alias(subq, "Window");
	rte->eref = copyObject(alias);
	rte->alias = alias;

	/* Accomodate depth change in new subquery, Q''.
	 */
	IncrementVarSublevelsUpInTransformGroupedWindows((Node*)subq, 1, 1);

	/* Might have changed. */
	subq->hasSubLinks = checkExprHasSubLink((Node*)subq);

	Assert(PointerIsValid(qry->targetList));
	Assert(IsA(qry->targetList, List));
	/* Use error instead of assertion to "use" hadSubLinks and keep compiler happy. */
	if (hadSubLinks != (qry->hasSubLinks || subq->hasSubLinks))
		elog(ERROR, "inconsistency detected in internal grouped windows transformation");

	discard_grouped_window_context(&ctx);

	return qry;
}


/* Helper for transformGroupedWindows:
 *
 * Prime the subquery target list in the context with the grouping
 * and windowing attributes from the given query and adjust the
 * subquery group clauses in the context to agree.
 *
 * Note that we arrange dense sortgroupref values and stash the
 * referents on the front of the subquery target list.  This may
 * be over-kill, but the grouping extension code seems to like it
 * this way.
 *
 * Note that we only transfer sortgrpref values associated with
 * grouping and windowing to the subquery context.  The subquery
 * shouldn't care about ordering, etc. XXX
 */
static void
init_grouped_window_context(grouped_window_ctx *ctx, Query *qry)
{
	List *grp_tles;
	List *grp_sortops;
	List *grp_eqops;
	ListCell *lc = NULL;
	Index maxsgr = 0;

	get_sortgroupclauses_tles(qry->groupClause, qry->targetList,
							  &grp_tles, &grp_sortops, &grp_eqops);
	list_free(grp_sortops);
	maxsgr = maxSortGroupRef(grp_tles, true);

	ctx->subtlist = NIL;
	ctx->subgroupClause = NIL;

	/* Set up scratch space.
	 */

	ctx->subrtable = qry->rtable;

	/* Map input = outer query sortgroupref values to subquery values while building the
	 * subquery target list prefix. */
	ctx->sgr_map = palloc0((maxsgr+1)*sizeof(ctx->sgr_map[0]));
	foreach (lc, grp_tles)
	{
	    TargetEntry *tle;
	    Index old_sgr;

	    tle = (TargetEntry*)copyObject(lfirst(lc));
	    old_sgr = tle->ressortgroupref;

	    ctx->subtlist = lappend(ctx->subtlist, tle);
		tle->resno = list_length(ctx->subtlist);
		tle->ressortgroupref = tle->resno;
		tle->resjunk = false;

		ctx->sgr_map[old_sgr] = tle->ressortgroupref;
	}

	/* Miscellaneous scratch area. */
	ctx->call_depth = 0;
	ctx->tle = NULL;

	/* Revise grouping into ctx->subgroupClause */
	ctx->subgroupClause = (List*)map_sgr_mutator((Node*)qry->groupClause, ctx);
}


/* Helper for transformGroupedWindows */
static void
discard_grouped_window_context(grouped_window_ctx *ctx)
{
    ctx->subtlist = NIL;
    ctx->subgroupClause = NIL;
    ctx->tle = NULL;
	if (ctx->sgr_map)
		pfree(ctx->sgr_map);
	ctx->sgr_map = NULL;
	ctx->subrtable = NULL;
}


/* Helper for transformGroupedWindows:
 *
 * Look for the given expression in the context's subtlist.  If
 * none is found and the force argument is true, add a target
 * for it.  Make and return a variable referring to the target
 * with the matching expression, or return NULL, if no target
 * was found/added.
 */
static Var *
var_for_gw_expr(grouped_window_ctx *ctx, Node *expr, bool force)
{
	Var *var = NULL;
	TargetEntry *tle = tlist_member(expr, ctx->subtlist);

	if ( tle == NULL && force )
	{
		tle = makeNode(TargetEntry);
		ctx->subtlist = lappend(ctx->subtlist, tle);
		tle->expr = (Expr*)expr;
		tle->resno = list_length(ctx->subtlist);
		/* See comment in grouped_window_mutator for why level 3 is appropriate. */
		if ( ctx->call_depth == 3 && ctx->tle != NULL && ctx->tle->resname != NULL )
		{
			tle->resname = pstrdup(ctx->tle->resname);
		}
		else
		{
			tle->resname = generate_positional_name(tle->resno);
		}
		tle->ressortgroupref = 0;
		tle->resorigtbl = 0;
		tle->resorigcol = 0;
		tle->resjunk = false;
	}

	if (tle != NULL)
	{
		var = makeNode(Var);
		var->varno = 1; /* one and only */
		var->varattno = tle->resno; /* by construction */
		var->vartype = exprType((Node*)tle->expr);
		var->vartypmod = exprTypmod((Node*)tle->expr);
		var->varlevelsup = 0;
		var->varnoold = 1;
		var->varoattno = tle->resno;
		var->location = 0;
	}

	return var;
}


/* Helper for transformGroupedWindows:
 *
 * Mutator for subquery groupingClause to adjust sortgrpref values
 * based on map developed while priming context target list.
 */
static Node*
map_sgr_mutator(Node *node, void *context)
{
	grouped_window_ctx *ctx = (grouped_window_ctx*)context;

	if (!node)
		return NULL;

	if (IsA(node, List))
	{
		ListCell *lc;
		List *new_lst = NIL;

		foreach ( lc, (List *)node)
		{
			Node *newnode = lfirst(lc);
			newnode = map_sgr_mutator(newnode, ctx);
			new_lst = lappend(new_lst, newnode);
		}
		return (Node*)new_lst;
	}
	else if (IsA(node, SortGroupClause))
	{
		SortGroupClause *g = (SortGroupClause *) node;
		SortGroupClause *new_g = makeNode(SortGroupClause);
		memcpy(new_g, g, sizeof(SortGroupClause));
		new_g->tleSortGroupRef = ctx->sgr_map[g->tleSortGroupRef];
		return (Node*)new_g;
	}
	else if (IsA(node, GroupingClause))
	{
		GroupingClause *gc = (GroupingClause*)node;
		GroupingClause *new_gc = makeNode(GroupingClause);
		memcpy(new_gc, gc, sizeof(GroupingClause));
		new_gc->groupsets = (List*)map_sgr_mutator((Node*)gc->groupsets, ctx);
		return (Node*)new_gc;
	}

	return NULL; /* Never happens */
}




/*
 * Helper for transformGroupedWindows:
 *
 * Transform targets from Q into targets for Q' and place information
 * needed to eventually construct the target list for the subquery Q''
 * in the context structure.
 *
 * The general idea is to add expressions that must be evaluated in the
 * subquery to the subquery target list (in the context) and to replace
 * them with Var nodes in the outer query.
 *
 * If there are any Agg nodes in the Q'' target list, arrange
 * to set hasAggs to true in the subquery. (This should already be
 * done, though).
 *
 * If we're pushing down an entire TLE that has a resname, use
 * it as an alias in the upper TLE, too.  Facilitate this by copying
 * down the resname from an immediately enclosing TargetEntry, if any.
 *
 * The algorithm repeatedly searches the subquery target list under
 * construction (quadric), however we don't expect many targets so
 * we don't optimize this.  (Could, for example, use a hash or divide
 * the target list into var, expr, and group/aggregate function lists.)
 */

static Node* grouped_window_mutator(Node *node, void *context)
{
	Node *result = NULL;

	grouped_window_ctx *ctx = (grouped_window_ctx*)context;

	if (!node)
		return result;

	ctx->call_depth++;

	if (IsA(node, TargetEntry))
	{
		TargetEntry *tle = (TargetEntry *)node;
		TargetEntry *new_tle = makeNode(TargetEntry);

		/* Copy the target entry. */
		new_tle->resno = tle->resno;
		if (tle->resname == NULL )
		{
			new_tle->resname = generate_positional_name(new_tle->resno);
		}
		else
		{
			new_tle->resname = pstrdup(tle->resname);
		}
		new_tle->ressortgroupref = tle->ressortgroupref;
		new_tle->resorigtbl = InvalidOid;
		new_tle->resorigcol = 0;
		new_tle->resjunk = tle->resjunk;

		/* This is pretty shady, but we know our call pattern.  The target
		 * list is at level 1, so we're interested in target entries at level
		 * 2.  We record them in context so var_for_gw_expr can maybe make a better
		 * than default choice of alias.
		 */
		if (ctx->call_depth == 2 )
		{
			ctx->tle = tle;
		}
		else
		{
			ctx->tle = NULL;
		}

		new_tle->expr = (Expr*)grouped_window_mutator((Node*)tle->expr, ctx);

		ctx->tle = NULL;
		result = (Node*)new_tle;
	}
	else if (IsA(node, Aggref) ||
			 IsA(node, GroupingFunc) ||
			 IsA(node, GroupId) )
	{
		/* Aggregation expression */
		result = (Node*) var_for_gw_expr(ctx, node, true);
	}
	else if (IsA(node, Var))
	{
		Var *var = (Var*)node;

		/* Since this is a Var (leaf node), we must be able to mutate it,
		 * else we can't finish the transformation and must give up.
		 */
		result = (Node*) var_for_gw_expr(ctx, node, false);

		if ( !result )
		{
			List *altvars = generate_alternate_vars(var, ctx);
			ListCell *lc;
			foreach(lc, altvars)
			{
				result = (Node*) var_for_gw_expr(ctx, lfirst(lc), false);
				if ( result )
					break;
			}
		}

		if ( ! result )
			ereport(ERROR,
					(errcode(ERRCODE_WINDOWING_ERROR),
					 errmsg("unresolved grouping key in window query"),
					 errhint("You might need to use explicit aliases and/or to refer to grouping keys in the same way throughout the query.")));
	}
	else
	{
		/* Grouping expression; may not find one. */
		result = (Node*) var_for_gw_expr(ctx, node, false);
	}


	if ( !result )
	{
		result = expression_tree_mutator(node, grouped_window_mutator, ctx);
	}

	ctx->call_depth--;
	return result;
}

/*
 * Helper for transformGroupedWindows:
 *
 * Build an Alias for a subquery RTE representing the given Query.
 * The input string aname is the name for the overall Alias. The
 * attribute names are all found or made up.
 */
static Alias *
make_replacement_alias(Query *qry, const char *aname)
{
	ListCell *lc = NULL;
	 char *name = NULL;
	Alias *alias = makeNode(Alias);
	AttrNumber attrno = 0;

	alias->aliasname = pstrdup(aname);
	alias->colnames = NIL;

	foreach(lc, qry->targetList)
	{
		TargetEntry *tle = (TargetEntry*)lfirst(lc);
		attrno++;

		if (tle->resname)
		{
			/* Prefer the target's resname. */
			name = pstrdup(tle->resname);
		}
		else if ( IsA(tle->expr, Var) )
		{
			/* If the target expression is a Var, use the name of the
			 * attribute in the query's range table. */
			Var *var = (Var*)tle->expr;
			RangeTblEntry *rte = rt_fetch(var->varno, qry->rtable);
			name = pstrdup(get_rte_attribute_name(rte, var->varattno));
		}
		else
		{
			/* If all else, fails, generate a name based on position. */
			name = generate_positional_name(attrno);
		}

		alias->colnames = lappend(alias->colnames, makeString(name));
	}
	return alias;
}

/*
 * Helper for transformGroupedWindows:
 *
 * Make a palloc'd C-string named for the input attribute number.
 */
static char *
generate_positional_name(AttrNumber attrno)
{
	int rc = 0;
	char buf[NAMEDATALEN];

	rc = snprintf(buf, sizeof(buf),
				  "att_%d", attrno );
	if ( rc == EOF || rc < 0 || rc >=sizeof(buf) )
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("can't generate internal attribute name")));
	}
	return pstrdup(buf);
}

/*
 * Helper for transformGroupedWindows:
 *
 * Find alternate Vars on the range of the input query that are aliases
 * (modulo ANSI join) of the input Var on the range and that occur in the
 * target list of the input query.
 *
 * If the input Var references a join result, there will be a single
 * alias.  If not, we need to search the range table for occurrences
 * of the input Var in some join result's RTE and add a Var referring
 * to the appropriate attribute of the join RTE to the list.
 *
 * This is not efficient, but the need is rare (MPP-12082) so we don't
 * bother to precompute this.
 */
static List*
generate_alternate_vars(Var *invar, grouped_window_ctx *ctx)
{
	List *rtable = ctx->subrtable;
	RangeTblEntry *inrte;
	List *alternates = NIL;

	Assert(IsA(invar, Var));

	inrte = rt_fetch(invar->varno, rtable);

	if ( inrte->rtekind == RTE_JOIN )
	{
		Node *ja = list_nth(inrte->joinaliasvars, invar->varattno-1);

		/* Though Node types other than Var (e.g., CoalesceExpr or Const) may occur
		 * as joinaliasvars, we ignore them.
		 */
		if ( IsA(ja, Var) )
		{
			alternates = lappend(alternates, copyObject(ja));
		}
	}
	else
	{
		ListCell *jlc;
		Index varno = 0;

		foreach (jlc, rtable)
		{
			RangeTblEntry *rte = (RangeTblEntry*)lfirst(jlc);

			varno++; /* This RTE's varno */

			if ( rte->rtekind == RTE_JOIN )
			{
				ListCell *alc;
				AttrNumber attno = 0;

				foreach (alc, rte->joinaliasvars)
				{
					ListCell *tlc;
					Node *altnode = lfirst(alc);
					Var *altvar = (Var*)altnode;

					attno++; /* This attribute's attno in its join RTE */

					if ( !IsA(altvar, Var) || !equal(invar, altvar) )
						continue;

					/* Look for a matching Var in the target list. */

					foreach(tlc, ctx->subtlist)
					{
						TargetEntry *tle = (TargetEntry*)lfirst(tlc);
						Var *v = (Var*)tle->expr;

						if ( IsA(v, Var) && v->varno == varno && v->varattno == attno )
						{
							alternates = lappend(alternates, tle->expr);
						}
					}
				}
			}
		}
	}
	return alternates;
}



/*
 * transformSelectStmt -
 *	  transforms a Select Statement
 *
 * Note: this covers only cases with no set operations and no VALUES lists;
 * see below for the other cases.
 */
static Query *
transformSelectStmt(ParseState *pstate, SelectStmt *stmt)
{
	Query	   *qry = makeNode(Query);
	Node	   *qual;
	ListCell   *l;

	qry->commandType = CMD_SELECT;

	/* make FOR UPDATE/FOR SHARE info available to addRangeTableEntry */
	pstate->p_locking_clause = stmt->lockingClause;

	/*
	 * Put WINDOW clause data into pstate so that window references know
	 * about them.
	 */
	pstate->p_windowdefs = stmt->windowClause;

	/* process the WITH clause */
	if (stmt->withClause)
	{
		qry->hasRecursive = stmt->withClause->recursive;
		qry->cteList = transformWithClause(pstate, stmt->withClause);
	}

	/* process the FROM clause */
	transformFromClause(pstate, stmt->fromClause);

	/* transform targetlist */
	qry->targetList = transformTargetList(pstate, stmt->targetList,
										  EXPR_KIND_SELECT_TARGET);

	/* mark column origins */
	markTargetListOrigins(pstate, qry->targetList);

	/* transform WHERE */
	qual = transformWhereClause(pstate, stmt->whereClause,
								EXPR_KIND_WHERE, "WHERE");

	/*
	 * Initial processing of HAVING clause is just like WHERE clause.
	 */
	qry->havingQual = transformWhereClause(pstate, stmt->havingClause,
										   EXPR_KIND_HAVING, "HAVING");

    /*
     * CDB: Untyped Const or Param nodes in a subquery in the FROM clause
     * might have been assigned proper types when we transformed the WHERE
     * clause, targetlist, etc.  Bring targetlist Var types up to date.
     */
    fixup_unknown_vars_in_targetlist(pstate, qry->targetList);

	/*
	 * Transform sorting/grouping stuff.  Do ORDER BY first because both
	 * transformGroupClause and transformDistinctClause need the results. Note
	 * that these functions can also change the targetList, so it's passed to
	 * them by reference.
	 */
	qry->sortClause = transformSortClause(pstate,
										  stmt->sortClause,
										  &qry->targetList,
										  EXPR_KIND_ORDER_BY,
										  true, /* fix unknowns */
                                          false /* allow SQL92 rules */);

	qry->groupClause = transformGroupClause(pstate,
											stmt->groupClause,
											&qry->targetList,
											qry->sortClause,
											EXPR_KIND_GROUP_BY,
                                            false /* allow SQL92 rules */);

	/*
	 * SCATTER BY clause on a table function TableValueExpr subquery.
	 *
	 * Note: a given subquery cannot have both a SCATTER clause and an INTO
	 * clause, because both of those control distribution.  This should not
	 * possible due to grammar restrictions on where a SCATTER clause is
	 * allowed.
	 */
	Insist(!(stmt->scatterClause && stmt->intoClause));
	qry->scatterClause = transformScatterClause(pstate,
												stmt->scatterClause,
												&qry->targetList);

	if (stmt->distinctClause == NIL)
	{
		qry->distinctClause = NIL;
		qry->hasDistinctOn = false;
	}
	else if (linitial(stmt->distinctClause) == NULL)
	{
		/* We had SELECT DISTINCT */
		if (!pstate->p_hasAggs && !pstate->p_hasWindowFuncs && qry->groupClause == NIL)
		{
			/*
			 * MPP-15040
			 * turn distinct clause into grouping clause to make both sort-based
			 * and hash-based grouping implementations viable plan options
			 */
			qry->distinctClause = transformDistinctToGroupBy(pstate,
															 &qry->targetList,
															 &qry->sortClause,
															 &qry->groupClause);
		}
		else
		{
			qry->distinctClause = transformDistinctClause(pstate,
														  &qry->targetList,
														  qry->sortClause,
														  false);
		}
		qry->hasDistinctOn = false;
	}
	else
	{
		/* We had SELECT DISTINCT ON */
		qry->distinctClause = transformDistinctOnClause(pstate,
														stmt->distinctClause,
														&qry->targetList,
														qry->sortClause);
		qry->hasDistinctOn = true;
	}

	/* transform LIMIT */
	qry->limitOffset = transformLimitClause(pstate, stmt->limitOffset,
											EXPR_KIND_OFFSET, "OFFSET");
	qry->limitCount = transformLimitClause(pstate, stmt->limitCount,
										   EXPR_KIND_LIMIT, "LIMIT");

	/* transform window clauses after we have seen all window functions */
	qry->windowClause = transformWindowDefinitions(pstate,
												   pstate->p_windowdefs,
												   &qry->targetList);

	processExtendedGrouping(pstate, qry->havingQual, qry->windowClause, qry->targetList);

	/* handle any SELECT INTO/CREATE TABLE AS spec */
	qry->intoClause = NULL;
	if (stmt->intoClause)
	{
		qry->intoClause = stmt->intoClause;
		if (stmt->intoClause->colNames)
			applyColumnNames(qry->targetList, stmt->intoClause->colNames);
		/* XXX XXX:		qry->partitionBy = stmt->partitionBy; */
	}

	/*
	 * Generally, we'll only have a distributedBy clause if stmt->into is set,
	 * with the exception of set op queries, since transformSetOperationStmt()
	 * sets stmt->into to NULL to avoid complications elsewhere.
	 */
	if (stmt->distributedBy && Gp_role == GP_ROLE_DISPATCH)
		setQryDistributionPolicy(stmt, qry);

	qry->rtable = pstate->p_rtable;
	qry->jointree = makeFromExpr(pstate->p_joinlist, qual);

	qry->hasSubLinks = pstate->p_hasSubLinks;
	qry->hasWindowFuncs = pstate->p_hasWindowFuncs;
	qry->hasFuncsWithExecRestrictions = pstate->p_hasFuncsWithExecRestrictions;
	qry->hasAggs = pstate->p_hasAggs;
	if (pstate->p_hasAggs || qry->groupClause || qry->havingQual)
		parseCheckAggregates(pstate, qry);

	if (pstate->p_hasTblValueExpr)
		parseCheckTableFunctions(pstate, qry);

	foreach(l, stmt->lockingClause)
	{
		transformLockingClause(pstate, qry,
							   (LockingClause *) lfirst(l), false);
	}

	/*
	 * If the query mixes window functions and aggregates, we need to
	 * transform it such that the grouped query appears as a subquery
	 */
	if (qry->hasWindowFuncs && (qry->groupClause || qry->hasAggs))
		transformGroupedWindows(qry);

	return qry;
}

/*
 * transformValuesClause -
 *	  transforms a VALUES clause that's being used as a standalone SELECT
 *
 * We build a Query containing a VALUES RTE, rather as if one had written
 *			SELECT * FROM (VALUES ...) AS "*VALUES*"
 */
static Query *
transformValuesClause(ParseState *pstate, SelectStmt *stmt)
{
	Query	   *qry = makeNode(Query);
	List	   *exprsLists = NIL;
	List	  **colexprs = NULL;
	Oid		   *coltypes = NULL;
	int			sublist_length = -1;
	List	   *newExprsLists;
	RangeTblEntry *rte;
	RangeTblRef *rtr;
	ListCell   *lc;
	ListCell   *lc2;
	int			i;

	qry->commandType = CMD_SELECT;

	/* Most SELECT stuff doesn't apply in a VALUES clause */
	Assert(stmt->distinctClause == NIL);
	Assert(stmt->targetList == NIL);
	Assert(stmt->fromClause == NIL);
	Assert(stmt->whereClause == NULL);
	Assert(stmt->groupClause == NIL);
	Assert(stmt->havingClause == NULL);
	Assert(stmt->scatterClause == NIL);
	Assert(stmt->op == SETOP_NONE);

	/* process the WITH clause */
	if (stmt->withClause)
	{
		qry->hasRecursive = stmt->withClause->recursive;
		qry->cteList = transformWithClause(pstate, stmt->withClause);
	}

	/*
	 * For each row of VALUES, transform the raw expressions and gather type
	 * information.  This is also a handy place to reject DEFAULT nodes, which
	 * the grammar allows for simplicity.
	 */
	foreach(lc, stmt->valuesLists)
	{
		List	   *sublist = (List *) lfirst(lc);

		/* Do basic expression transformation (same as a ROW() expr) */
		sublist = transformExpressionList(pstate, sublist, EXPR_KIND_VALUES);

		/*
		 * All the sublists must be the same length, *after* transformation
		 * (which might expand '*' into multiple items).  The VALUES RTE can't
		 * handle anything different.
		 */
		if (sublist_length < 0)
		{
			/* Remember post-transformation length of first sublist */
			sublist_length = list_length(sublist);
			/* and allocate arrays for per-column info */
			colexprs = (List **) palloc0(sublist_length * sizeof(List *));
			coltypes = (Oid *) palloc0(sublist_length * sizeof(Oid));
		}
		else if (sublist_length != list_length(sublist))
		{
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("VALUES lists must all be the same length"),
					 parser_errposition(pstate,
										exprLocation((Node *) sublist))));
		}

		exprsLists = lappend(exprsLists, sublist);

		/* Check for DEFAULT and build per-column expression lists */
		i = 0;
		foreach(lc2, sublist)
		{
			Node	   *col = (Node *) lfirst(lc2);

			if (IsA(col, SetToDefault))
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("DEFAULT can only appear in a VALUES list within INSERT"),
						 parser_errposition(pstate, exprLocation(col))));
			colexprs[i] = lappend(colexprs[i], col);
			i++;
		}
	}

	/*
	 * Now resolve the common types of the columns, and coerce everything to
	 * those types.
	 */
	for (i = 0; i < sublist_length; i++)
	{
		coltypes[i] = select_common_type(pstate, colexprs[i], "VALUES", NULL);
	}

	newExprsLists = NIL;
	foreach(lc, exprsLists)
	{
		List	   *sublist = (List *) lfirst(lc);
		List	   *newsublist = NIL;

		i = 0;
		foreach(lc2, sublist)
		{
			Node	   *col = (Node *) lfirst(lc2);

			col = coerce_to_common_type(pstate, col, coltypes[i], "VALUES");
			newsublist = lappend(newsublist, col);
			i++;
		}

		newExprsLists = lappend(newExprsLists, newsublist);
	}

	/*
	 * Generate the VALUES RTE
	 */
	rte = addRangeTableEntryForValues(pstate, newExprsLists, NULL, true);
	rtr = makeNode(RangeTblRef);
	/* assume new rte is at end */
	rtr->rtindex = list_length(pstate->p_rtable);
	Assert(rte == rt_fetch(rtr->rtindex, pstate->p_rtable));
	pstate->p_joinlist = lappend(pstate->p_joinlist, rtr);
	pstate->p_relnamespace = lappend(pstate->p_relnamespace, rte);
	pstate->p_varnamespace = lappend(pstate->p_varnamespace, rte);

	/*
	 * Generate a targetlist as though expanding "*"
	 */
	Assert(pstate->p_next_resno == 1);
	qry->targetList = expandRelAttrs(pstate, rte, rtr->rtindex, 0, -1);

	/*
	 * The grammar allows attaching ORDER BY, LIMIT, and FOR UPDATE to a
	 * VALUES, so cope.
	 */
	qry->sortClause = transformSortClause(pstate,
										  stmt->sortClause,
										  &qry->targetList,
										  EXPR_KIND_ORDER_BY,
										  true, /* fix unknowns */
                                          false /* allow SQL92 rules */);

	qry->limitOffset = transformLimitClause(pstate, stmt->limitOffset,
											EXPR_KIND_OFFSET, "OFFSET");
	qry->limitCount = transformLimitClause(pstate, stmt->limitCount,
										   EXPR_KIND_LIMIT, "LIMIT");

	if (stmt->lockingClause)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("SELECT FOR UPDATE/SHARE cannot be applied to VALUES")));

	/* handle any CREATE TABLE AS spec */
	qry->intoClause = NULL;
	if (stmt->intoClause)
	{
		qry->intoClause = stmt->intoClause;
		if (stmt->intoClause->colNames)
			applyColumnNames(qry->targetList, stmt->intoClause->colNames);
	}

	if (stmt->distributedBy && Gp_role == GP_ROLE_DISPATCH)
		setQryDistributionPolicy(stmt, qry);

	/*
	 * There mustn't have been any table references in the expressions, else
	 * strange things would happen, like Cartesian products of those tables
	 * with the VALUES list.  We have to check this after parsing ORDER BY et
	 * al since those could insert more junk.
	 */
	if (list_length(pstate->p_joinlist) != 1)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("VALUES must not contain table references"),
				 parser_errposition(pstate,
						   locate_var_of_level((Node *) newExprsLists, 0))));

	/*
	 * Another thing we can't currently support is NEW/OLD references in rules
	 * --- seems we'd need something like SQL99's LATERAL construct to ensure
	 * that the values would be available while evaluating the VALUES RTE.
	 * This is a shame.  FIXME
	 */
	if (list_length(pstate->p_rtable) != 1 &&
		contain_vars_of_level((Node *) newExprsLists, 0))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("VALUES must not contain OLD or NEW references"),
				 errhint("Use SELECT ... UNION ALL ... instead."),
				 parser_errposition(pstate,
						   locate_var_of_level((Node *) newExprsLists, 0))));

	qry->rtable = pstate->p_rtable;
	qry->jointree = makeFromExpr(pstate->p_joinlist, NULL);

	qry->hasSubLinks = pstate->p_hasSubLinks;
	qry->hasFuncsWithExecRestrictions = pstate->p_hasFuncsWithExecRestrictions;

	return qry;
}

/*
 * transformSetOperationStmt -
 *	  transforms a set-operations tree
 *
 * A set-operation tree is just a SELECT, but with UNION/INTERSECT/EXCEPT
 * structure to it.  We must transform each leaf SELECT and build up a top-
 * level Query that contains the leaf SELECTs as subqueries in its rangetable.
 * The tree of set operations is converted into the setOperations field of
 * the top-level Query.
 */
static Query *
transformSetOperationStmt(ParseState *pstate, SelectStmt *stmt)
{
	Query	   *qry = makeNode(Query);
	SelectStmt *leftmostSelect;
	int			leftmostRTI;
	Query	   *leftmostQuery;
	WithClause *withClause;
	SetOperationStmt *sostmt;
	List	   *socolinfo;
	List	   *intoColNames = NIL;
	List	   *sortClause;
	Node	   *limitOffset;
	Node	   *limitCount;
	List	   *lockingClause;
	Node	   *node;
	ListCell   *left_tlist,
			   *lct,
			   *lcm,
			   *l;
	List	   *targetvars,
			   *targetnames,
			   *sv_relnamespace,
			   *sv_varnamespace;
	int			sv_rtable_length;
	RangeTblEntry *jrte;
	int			tllen;

	qry->commandType = CMD_SELECT;

	/*
	 * Find leftmost leaf SelectStmt; extract the one-time-only items from it
	 * and from the top-level node.
	 */
	leftmostSelect = stmt->larg;
	while (leftmostSelect && leftmostSelect->op != SETOP_NONE)
		leftmostSelect = leftmostSelect->larg;
	Assert(leftmostSelect && IsA(leftmostSelect, SelectStmt) &&
		   leftmostSelect->larg == NULL);
	qry->intoClause = NULL;
	if (leftmostSelect->intoClause)
	{
		qry->intoClause = leftmostSelect->intoClause;
		intoColNames = leftmostSelect->intoClause->colNames;
	}

	/* clear this to prevent complaints in transformSetOperationTree() */
	leftmostSelect->intoClause = NULL;

	/*
	 * These are not one-time, exactly, but we want to process them here and
	 * not let transformSetOperationTree() see them --- else it'll just
	 * recurse right back here!
	 */
	sortClause = stmt->sortClause;
	limitOffset = stmt->limitOffset;
	limitCount = stmt->limitCount;
	lockingClause = stmt->lockingClause;
	withClause = stmt->withClause;

	stmt->sortClause = NIL;
	stmt->limitOffset = NULL;
	stmt->limitCount = NULL;
	stmt->lockingClause = NIL;
	stmt->withClause = NULL;

	/* We don't support FOR UPDATE/SHARE with set ops at the moment. */
	if (lockingClause)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("SELECT FOR UPDATE/SHARE is not allowed with UNION/INTERSECT/EXCEPT")));

	/* process the WITH clause */
	if (withClause)
	{
		qry->hasRecursive = withClause->recursive;
		qry->cteList = transformWithClause(pstate, withClause);
	}

	/*
	 * Recursively transform the components of the tree.
	 */
	sostmt = (SetOperationStmt *) transformSetOperationTree(pstate, stmt,
															true,
															&socolinfo);
	Assert(sostmt && IsA(sostmt, SetOperationStmt));
	qry->setOperations = (Node *) sostmt;

	/*
	 * Re-find leftmost SELECT (now it's a sub-query in rangetable)
	 */
	node = sostmt->larg;
	while (node && IsA(node, SetOperationStmt))
		node = ((SetOperationStmt *) node)->larg;
	Assert(node && IsA(node, RangeTblRef));
	leftmostRTI = ((RangeTblRef *) node)->rtindex;
	leftmostQuery = rt_fetch(leftmostRTI, pstate->p_rtable)->subquery;
	Assert(leftmostQuery != NULL);

	/* Copy transformed distribution policy to query */
	if (qry->intoClause)
		qry->intoPolicy = leftmostQuery->intoPolicy;

	/*
	 * Generate dummy targetlist for outer query using column names of
	 * leftmost select and common datatypes of topmost set operation. Also
	 * make lists of the dummy vars and their names for use in parsing ORDER
	 * BY.
	 *
	 * Note: we use leftmostRTI as the varno of the dummy variables. It
	 * shouldn't matter too much which RT index they have, as long as they
	 * have one that corresponds to a real RT entry; else funny things may
	 * happen when the tree is mashed by rule rewriting.
	 */
	qry->targetList = NIL;
	targetvars = NIL;
	targetnames = NIL;
	left_tlist = list_head(leftmostQuery->targetList);

	forboth(lct, sostmt->colTypes, lcm, sostmt->colTypmods)
	{
		Oid			colType = lfirst_oid(lct);
		int32		colTypmod = lfirst_int(lcm);
		TargetEntry *lefttle = (TargetEntry *) lfirst(left_tlist);
		char	   *colName;
		TargetEntry *tle;
		Var		   *var;

		Assert(!lefttle->resjunk);
		colName = pstrdup(lefttle->resname);
		var = makeVar(leftmostRTI,
					  lefttle->resno,
					  colType,
					  colTypmod,
					  0);
		var->location = exprLocation((Node *) lefttle->expr);
		tle = makeTargetEntry((Expr *) var,
							  (AttrNumber) pstate->p_next_resno++,
							  colName,
							  false);
		qry->targetList = lappend(qry->targetList, tle);
		targetvars = lappend(targetvars, var);
		targetnames = lappend(targetnames, makeString(colName));
		left_tlist = lnext(left_tlist);
	}

	/*
	 * Coerce the UNKNOWN type for target entries to its right type here.
	 */
	fixup_unknown_vars_in_setop(pstate, sostmt);

	/*
	 * As a first step towards supporting sort clauses that are expressions
	 * using the output columns, generate a varnamespace entry that makes the
	 * output columns visible.	A Join RTE node is handy for this, since we
	 * can easily control the Vars generated upon matches.
	 *
	 * Note: we don't yet do anything useful with such cases, but at least
	 * "ORDER BY upper(foo)" will draw the right error message rather than
	 * "foo not found".
	 */
	sv_rtable_length = list_length(pstate->p_rtable);

	jrte = addRangeTableEntryForJoin(pstate,
									 targetnames,
									 JOIN_INNER,
									 targetvars,
									 NULL,
									 false);

	sv_relnamespace = pstate->p_relnamespace;
	pstate->p_relnamespace = NIL;		/* no qualified names allowed */

	sv_varnamespace = pstate->p_varnamespace;
	pstate->p_varnamespace = list_make1(jrte);

	/*
	 * For now, we don't support resjunk sort clauses on the output of a
	 * setOperation tree --- you can only use the SQL92-spec options of
	 * selecting an output column by name or number.  Enforce by checking that
	 * transformSortClause doesn't add any items to tlist.
	 */
	tllen = list_length(qry->targetList);

	qry->sortClause = transformSortClause(pstate,
										  sortClause,
										  &qry->targetList,
										  EXPR_KIND_ORDER_BY,
										  false /* no unknowns expected */,
                                          false /* allow SQL92 rules */ );

	pstate->p_rtable = list_truncate(pstate->p_rtable, sv_rtable_length);
	pstate->p_relnamespace = sv_relnamespace;
	pstate->p_varnamespace = sv_varnamespace;

	if (tllen != list_length(qry->targetList))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("invalid UNION/INTERSECT/EXCEPT ORDER BY clause"),
				 errdetail("Only result column names can be used, not expressions or functions."),
				 errhint("Add the expression/function to every SELECT, or move the UNION into a FROM clause."),
				 parser_errposition(pstate,
						   exprLocation(list_nth(qry->targetList, tllen)))));

	qry->limitOffset = transformLimitClause(pstate, limitOffset,
											EXPR_KIND_OFFSET, "OFFSET");
	qry->limitCount = transformLimitClause(pstate, limitCount,
										   EXPR_KIND_LIMIT, "LIMIT");

	/*
	 * Handle SELECT INTO/CREATE TABLE AS.
	 *
	 * Any column names from CREATE TABLE AS need to be attached to both the
	 * top level and the leftmost subquery.  We do not do this earlier because
	 * we do *not* want sortClause processing to be affected.
	 */
	if (intoColNames)
	{
		applyColumnNames(qry->targetList, intoColNames);
		applyColumnNames(leftmostQuery->targetList, intoColNames);
	}

	qry->rtable = pstate->p_rtable;
	qry->jointree = makeFromExpr(pstate->p_joinlist, NULL);

	qry->hasSubLinks = pstate->p_hasSubLinks;
	qry->hasWindowFuncs = pstate->p_hasWindowFuncs;
	qry->hasFuncsWithExecRestrictions = pstate->p_hasFuncsWithExecRestrictions;
	qry->hasWindowFuncs = pstate->p_hasWindowFuncs;
	qry->hasAggs = pstate->p_hasAggs;
	if (pstate->p_hasAggs || qry->groupClause || qry->havingQual)
		parseCheckAggregates(pstate, qry);

	if (pstate->p_hasTblValueExpr)
		parseCheckTableFunctions(pstate, qry);

	foreach(l, lockingClause)
	{
		transformLockingClause(pstate, qry,
							   (LockingClause *) lfirst(l), false);
	}

	return qry;
}

/*
 * transformSetOperationTree
 *		Recursively transform leaves and internal nodes of a set-op tree
 *
 * In addition to returning the transformed node, we return a list of
 * expression nodes showing the type, typmod, and location (for error messages)
 * of each output column of the set-op node.  This is used only during the
 * internal recursion of this function.  At the upper levels we use
 * SetToDefault nodes for this purpose, since they carry exactly the fields
 * needed, but any other expression node type would do as well.
 */
static Node *
transformSetOperationTree(ParseState *pstate, SelectStmt *stmt,
						  bool isTopLevel, List **colInfo)
{
	setop_types_ctx ctx;
	Node	   *top;
	List	   *selected_types;
	List	   *selected_typmods;

	/*
	 * Transform all the subtrees.
	 */
	ctx.ncols = -1;
	ctx.leafinfos = NULL;
	top = transformSetOperationTree_internal(pstate, stmt, isTopLevel, &ctx);
	Assert(ctx.ncols >= 0);

	/*
	 * We have now transformed all the subtrees, and collected all the
	 * data types and typmods of the columns from each leaf node.
	 *
	 * In PostgreSQL, we also choose the result type for each subtree as we
	 * recurse, but in GPDB, we do that here as a separate pass. That way, we
	 * have can make the decision globally based on every leaf, rather
	 * separately for each subtree.
	 *
	 * There are also some hacks to more leniently coerce between types, to
	 * make some cases not error out.
	 */
	select_setop_types(pstate, &ctx, stmt->op, &selected_types, &selected_typmods);

	coerceSetOpTypes(pstate, top, selected_types, selected_typmods, colInfo);

	return top;
}

static void
select_setop_types(ParseState *pstate, setop_types_ctx *ctx, SetOperation op, List **selected_types, List **selected_typmods)
{
	int			i;

	*selected_types = NIL;
	*selected_typmods = NIL;
	for (i = 0; i < ctx->ncols; i++)
	{
		List	   *typinfos = ctx->leafinfos[i];
		ListCell   *lci2;
		Oid			ptype;
		int32		ptypmod;
		Oid			restype;
		int32		restypmod;
		bool		allsame, hasnontext;
		char	   *context;

		context = (op == SETOP_UNION ? "UNION" :
				   op == SETOP_INTERSECT ? "INTERSECT" :
				   "EXCEPT");
		allsame = true;
		hasnontext = false;
		ptype = exprType(linitial(typinfos));
		ptypmod = exprTypmod(linitial(typinfos));
		foreach (lci2, typinfos)
		{
			Oid			ntype = exprType(lfirst(lci2));
			int32		ntypmod = exprTypmod(lfirst(lci2));

			/*
			 * In the first iteration, ntype and ptype is the same element,
			 * but we ignore it as it's not a big problem here.
			 */
			if (!(ntype == ptype && ntypmod == ptypmod))
			{
				/* if any is different, false */
				allsame = false;
			}
			/*
			 * MPP-15619 - backwards compatibility with existing view definitions.
			 *
			 * Historically we would cast UNKNOWN to text for most union queries,
			 * but there are many union cases where this historical behavior
			 * resulted in unacceptable errors (MPP-11377).
			 * To handle this we added additional code to resolve to a
			 * consistent cast for unions, which is generally better and
			 * handles more cases.  However, in order to deal with backwards
			 * compatibility we have to deliberately hamstring this code and
			 * cast UNKNOWN to text if the other colums are STRING_TYPE
			 * even when some other datatype (such as name) might actually
			 * be more natural.  This captures the set of views that
			 * we previously supported prior to the fix for MPP-11377 and
			 * thus is the set of views that we must not treat differently.
			 * This might be removed when we are ready to change view definition.
			 */
			if (ntype != UNKNOWNOID &&
				TYPCATEGORY_STRING != TypeCategory(getBaseType(ntype)))
				hasnontext = true;
		}

		/*
		 * Backward compatibility; Unfortunately, we cannot change
		 * the old behavior of the part which was working without ERROR,
		 * mostly for the view definition. See comments above for detail.
		 * Setting InvalidOid for this column, the column type resolution
		 * will be falling back to the old process.
		 */
		if (!hasnontext)
		{
			restype = InvalidOid;
			restypmod = -1;
		}
		else
		{
			/*
			 * Even if the types are all the same, we resolve the type
			 * by select_common_type(), which casts domains to base types.
			 * Ideally, the domain types should be preserved, but to keep
			 * compatibility with older GPDB views, currently we don't change it.
			 * This restriction will be solved once upgrade/view issues get clean.
			 * See MPP-7509 for the issue.
			 */
			restype = select_common_type(pstate, typinfos, context, NULL);
			/*
			 * If there's no common type, the last resort is TEXT.
			 * See also select_common_type().
			 */
			if (restype == UNKNOWNOID)
			{
				restype = TEXTOID;
				restypmod = -1;
			}
			else
			{
				/*
				 * Essentially we preserve typmod only when all elements
				 * are identical, otherwise default (-1).
				 */
				if (allsame)
					restypmod = ptypmod;
				else
					restypmod = -1;
			}
		}

		*selected_types = lappend_oid(*selected_types, restype);
		*selected_typmods = lappend_int(*selected_typmods, restypmod);
	}
}




static Node *
transformSetOperationTree_internal(ParseState *pstate, SelectStmt *stmt,
								   bool isTopLevel, setop_types_ctx *setop_types)
{
	bool		isLeaf;

	Assert(stmt && IsA(stmt, SelectStmt));

	/* Guard against stack overflow due to overly complex set-expressions */
	check_stack_depth();

	/*
	 * Validity-check both leaf and internal SELECTs for disallowed ops.
	 */
	if (stmt->intoClause)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("INTO is only allowed on first SELECT of UNION/INTERSECT/EXCEPT"),
				 parser_errposition(pstate,
								  exprLocation((Node *) stmt->intoClause))));

	/* We don't support FOR UPDATE/SHARE with set ops at the moment. */
	if (stmt->lockingClause)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("SELECT FOR UPDATE/SHARE is not allowed with UNION/INTERSECT/EXCEPT")));

	/*
	 * If an internal node of a set-op tree has ORDER BY, LIMIT, or FOR UPDATE
	 * clauses attached, we need to treat it like a leaf node to generate an
	 * independent sub-Query tree.	Otherwise, it can be represented by a
	 * SetOperationStmt node underneath the parent Query.
	 */
	if (stmt->op == SETOP_NONE)
	{
		Assert(stmt->larg == NULL && stmt->rarg == NULL);
		isLeaf = true;
	}
	else
	{
		Assert(stmt->larg != NULL && stmt->rarg != NULL);
		if (stmt->sortClause || stmt->limitOffset || stmt->limitCount ||
			stmt->lockingClause || stmt->withClause)
			isLeaf = true;
		else
			isLeaf = false;
	}

	if (isLeaf)
	{
		/* Process leaf SELECT */
		Query	   *selectQuery;
		char		selectName[32];
		RangeTblEntry *rte;
		RangeTblRef *rtr;
		ListCell   *tl;
		int			numCols;

		/*
		 * Transform SelectStmt into a Query.
		 *
		 * Note: previously transformed sub-queries don't affect the parsing
		 * of this sub-query, because they are not in the toplevel pstate's
		 * namespace list.
		 */
		selectQuery = parse_sub_analyze((Node *) stmt, pstate, NULL, NULL);

		/*
		 * Check for bogus references to Vars on the current query level (but
		 * upper-level references are okay). Normally this can't happen
		 * because the namespace will be empty, but it could happen if we are
		 * inside a rule.
		 */
		if (pstate->p_relnamespace || pstate->p_varnamespace)
		{
			if (contain_vars_of_level((Node *) selectQuery, 1))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
						 errmsg("UNION/INTERSECT/EXCEPT member statement cannot refer to other relations of same query level"),
						 parser_errposition(pstate,
							 locate_var_of_level((Node *) selectQuery, 1))));
		}

		/*
		 * Extract a list of the result expressions for upper-level checking.
		 */
		numCols = 0;
		foreach(tl, selectQuery->targetList)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(tl);

			if (!tle->resjunk)
				numCols++;
		}

		/*
		 * Also remember the datatype of each column to the lists in
		 * 'setop_types'.
		 */
		{
			int			i;

			if (setop_types->ncols == -1)
			{
				setop_types->ncols = numCols;
				setop_types->leafinfos = (List **) palloc0(setop_types->ncols * sizeof(List *));
			}
			i = 0;
			foreach(tl, selectQuery->targetList)
			{
				TargetEntry *tle = (TargetEntry *) lfirst(tl);

				if (tle->resjunk)
					continue;

				setop_types->leafinfos[i] = lappend(setop_types->leafinfos[i],
													(Node *) tle->expr);
				i++;

				/*
				 * It's possible that this leaf query has a differnet number
				 * of columns than the previous ones. That's an error, but
				 * we don't throw it here because we don't have the context
				 * needed for a good error message. We don't know which
				 * operation of the setop tree is the one where the number
				 * of columns between the left and right branches differ.
				 * Therefore, just return here as if nothing happened, and
				 * we'll catch that error in the parent instead.
				 */
				if (i == setop_types->ncols)
					break;
			}
		}

		/*
		 * Make the leaf query be a subquery in the top-level rangetable.
		 */
		snprintf(selectName, sizeof(selectName), "*SELECT* %d",
				 list_length(pstate->p_rtable) + 1);
		rte = addRangeTableEntryForSubquery(pstate,
											selectQuery,
											makeAlias(selectName, NIL),
											false);

		/*
		 * Return a RangeTblRef to replace the SelectStmt in the set-op tree.
		 */
		rtr = makeNode(RangeTblRef);
		/* assume new rte is at end */
		rtr->rtindex = list_length(pstate->p_rtable);
		Assert(rte == rt_fetch(rtr->rtindex, pstate->p_rtable));
		return (Node *) rtr;
	}
	else
	{
		/* Process an internal node (set operation node) */
		SetOperationStmt *op = makeNode(SetOperationStmt);
		const char *context;

		context = (stmt->op == SETOP_UNION ? "UNION" :
				   (stmt->op == SETOP_INTERSECT ? "INTERSECT" :
					"EXCEPT"));

		op->op = stmt->op;
		op->all = stmt->all;

		/*
		 * Recursively transform the left child node.
		 */
		op->larg = transformSetOperationTree_internal(pstate, stmt->larg,
													  false, setop_types);

		/*
		 * If we are processing a recursive union query, now is the time
		 * to examine the non-recursive term's output columns and mark the
		 * containing CTE as having those result columns.  We should do this
		 * only at the topmost setop of the CTE, of course.
		 *
		 * In PostgreSQL, transformSetOperationTree() runs as a single pass,
		 * and we coerce the column types as we go. In GDPB, it's a two-pass
		 * process. This function is part of the first pass, where we just
		 * collect datatype information, and in the second pass we coerce
		 * the targetlist of each branch of the setop tree to have compatible
		 * types. Unfortunately, WITH RECURSIVE puts a fly in the ointment.
		 * In order to make the columns of the WITH RECURSIVE itself visible
		 * to the second branch of the UNION, we must fully process the first
		 * branch before the second branch. So if this is WITH RECURSIVE,
		 * proceed with the type coercion after processing the first branch.
		 * We will do another coercion at the top, after processing the second
		 * branch.
		 */
		if (isTopLevel &&
			pstate->p_parent_cte &&
			pstate->p_parent_cte->cterecursive)
		{
			List *lcolinfo;
			List *selected_types;
			List *selected_typmods;

			select_setop_types(pstate, setop_types, stmt->op, &selected_types, &selected_typmods);

			coerceSetOpTypes(pstate, op->larg, selected_types, selected_typmods, &lcolinfo);

			determineRecursiveColTypes(pstate, op->larg, lcolinfo);
		}

		/*
		 * Recursively transform the right child node.
		 */
		op->rarg = transformSetOperationTree_internal(pstate, stmt->rarg,
													  false, setop_types);

		/*
		 * In PostgreSQL, we select the common type for each column here.
		 * In GPDB, we do that as a separate pass, after we have collected
		 * information on the types of each leaf node first.
		 */

		return (Node *) op;
	}
}

/*
 * Label every SetOperationStmt in the tree with the given datatypes.
 */
static void
coerceSetOpTypes(ParseState *pstate, Node *sop,
				 List *preselected_coltypes, List *preselected_coltypmods,
				 List **colInfo)
{
	if (IsA(sop, RangeTblRef))
	{
		RangeTblEntry *rte = rt_fetch((((RangeTblRef *) sop)->rtindex), pstate->p_rtable);
		Query	   *selectQuery = rte->subquery;
		ListCell   *tl;

		/*
		 * Extract a list of the result expressions. This is the same we did in
		 * the first pass, in transformSetOperationTree_internal().
		 */
		*colInfo = NIL;
		foreach(tl, selectQuery->targetList)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(tl);

			if (!tle->resjunk)
				*colInfo = lappend(*colInfo, tle->expr);
		}
		return;
	}
	else
	{
		SetOperationStmt *op = (SetOperationStmt *) sop;
		List	   *lcolinfo;
		List	   *rcolinfo;
		ListCell   *lci;
		ListCell   *rci;
		ListCell   *pct;
		ListCell   *pcm;
		const char *context;

		Assert(IsA(op, SetOperationStmt));

		context = (op->op == SETOP_UNION ? "UNION" :
				   op->op == SETOP_INTERSECT ? "INTERSECT" :
				   "EXCEPT");

		/* Recurse to determine the children's types first */
		coerceSetOpTypes(pstate, op->larg,
						 preselected_coltypes, preselected_coltypmods,
						 &lcolinfo);

		coerceSetOpTypes(pstate, op->rarg,
						 preselected_coltypes, preselected_coltypmods,
						 &rcolinfo);

		/*
		 * Verify that the two children have the same number of non-junk
		 * columns, and determine the types of the merged output columns.
		 */
		if (list_length(lcolinfo) != list_length(rcolinfo))
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("each %s query must have the same number of columns",
						context),
					 parser_errposition(pstate,
										exprLocation((Node *) rcolinfo))));

		Assert(list_length(preselected_coltypes) == list_length(preselected_coltypmods));

		*colInfo = NIL;
		op->colTypes = NIL;
		op->colTypmods = NIL;
		/* don't have a "foreach4", so chase two of the lists by hand */
		pct = list_head(preselected_coltypes);
		pcm = list_head(preselected_coltypmods);
		forboth(lci, lcolinfo, rci, rcolinfo)
		{
			Node	   *lcolnode = (Node *) lfirst(lci);
			Node	   *rcolnode = (Node *) lfirst(rci);
			Oid			lcoltype = exprType(lcolnode);
			Oid			rcoltype = exprType(rcolnode);
			int32		lcoltypmod = exprTypmod(lcolnode);
			int32		rcoltypmod = exprTypmod(rcolnode);
			Node       *bestexpr = NULL;
			SetToDefault *rescolnode;
			Oid			rescoltype = pct ? lfirst_oid(pct) : InvalidOid;
			int32		rescoltypmod = pcm ? lfirst_int(pcm) : -1;

			/*
			 * If the preprocessed coltype is InvalidOid, we fall back
			 * to the old style type resolution for backward
			 * compatibility. See transformSetOperationStmt for the reason.
			 */
			if (!OidIsValid(rescoltype))
			{
				/* select common type, same as CASE et al */
				rescoltype = select_common_type(pstate,
												list_make2(lcolnode, rcolnode),
												context,
												&bestexpr);
				/* if same type and same typmod, use typmod; else default */
				if (lcoltype == rcoltype && lcoltypmod == rcoltypmod)
					rescoltypmod = lcoltypmod;
			}
			else
			{
				/*
				 * If we used the preselected type, arbitrarily use the left
				 * query's expression for error reporting purposes.
				 */
				bestexpr = lcolnode;
			}

			/*
			 * Verify the coercions are actually possible.  If not, we'd
			 * fail later anyway, but we want to fail now while we have
			 * sufficient context to produce an error cursor position.
			 *
			 * The if-tests might look wrong, but they are correct: we should
			 * verify if the input is non-UNKNOWN *or* if it is an UNKNOWN
			 * Const (to verify the literal is valid for the target data type)
			 * or Param (to possibly resolve the Param's type).  We should do
			 * nothing if the input is say an UNKNOWN Var, which can happen in
			 * some cases.  The planner is sometimes able to fold the Var to a
			 * constant before it has to coerce the type, so failing now would
			 * just break cases that might work.
			 */
			if (lcoltype != UNKNOWNOID ||
				IsA(lcolnode, Const) || IsA(lcolnode, Param))
				(void) coerce_to_common_type(pstate, lcolnode,
											 rescoltype, context);
			if (rcoltype != UNKNOWNOID ||
				IsA(rcolnode, Const) || IsA(rcolnode, Param))
				(void) coerce_to_common_type(pstate, rcolnode,
											 rescoltype, context);

			/* emit results */
			rescolnode = makeNode(SetToDefault);
			rescolnode->typeId = rescoltype;
			rescolnode->typeMod = rescoltypmod;
			rescolnode->location = exprLocation(bestexpr);
			*colInfo = lappend(*colInfo, rescolnode);

			/* Set final decision */
			op->colTypes = lappend_oid(op->colTypes, rescoltype);
			op->colTypmods = lappend_int(op->colTypmods, rescoltypmod);

			/*
			 * For all cases except UNION ALL, identify the grouping operators
			 * (and, if available, sorting operators) that will be used to
			 * eliminate duplicates.
			 *
			 * A more logical place for this would be in the first pass, but we
			 * can't do this until we've decided the datatypes.
			 */
			if (op->op != SETOP_UNION || !op->all)
			{
				SortGroupClause *grpcl = makeNode(SortGroupClause);
				Oid			sortop;
				Oid			eqop;
				ParseCallbackState pcbstate;

				setup_parser_errposition_callback(&pcbstate, pstate,
												  rescolnode->location);

				/* determine the eqop and optional sortop */
				get_sort_group_operators(rescoltype,
										 false, true, false,
										 &sortop, &eqop, NULL);

				cancel_parser_errposition_callback(&pcbstate);

				/* we don't have a tlist yet, so can't assign sortgrouprefs */
				grpcl->tleSortGroupRef = 0;
				grpcl->eqop = eqop;
				grpcl->sortop = sortop;
				grpcl->nulls_first = false;		/* OK with or without sortop */

				op->groupClauses = lappend(op->groupClauses, grpcl);
			}

			pct = pct ? lnext(pct) : NULL;
			pcm = pcm ? lnext(pcm) : NULL;
		}
	}
}

/*
 * Process the outputs of the non-recursive term of a recursive union
 * to set up the parent CTE's columns
 */
static void
determineRecursiveColTypes(ParseState *pstate, Node *larg, List *lcolinfo)
{
	Node	   *node;
	int			leftmostRTI;
	Query	   *leftmostQuery;
	List	   *targetList;
	ListCell   *left_tlist;
	ListCell   *lci;
	int			next_resno;

	/*
	 * Find leftmost leaf SELECT
	 */
	node = larg;
	while (node && IsA(node, SetOperationStmt))
		node = ((SetOperationStmt *) node)->larg;
	Assert(node && IsA(node, RangeTblRef));
	leftmostRTI = ((RangeTblRef *) node)->rtindex;
	leftmostQuery = rt_fetch(leftmostRTI, pstate->p_rtable)->subquery;
	Assert(leftmostQuery != NULL);

	/*
	 * Generate dummy targetlist using column names of leftmost select
	 * and dummy result expressions of the non-recursive term.
	 */
	targetList = NIL;
	left_tlist = list_head(leftmostQuery->targetList);
	next_resno = 1;

	foreach(lci, lcolinfo)
	{
		Expr	   *lcolexpr = (Expr *) lfirst(lci);
		TargetEntry *lefttle = (TargetEntry *) lfirst(left_tlist);
		char	   *colName;
		TargetEntry *tle;

		Assert(!lefttle->resjunk);
		colName = pstrdup(lefttle->resname);
		tle = makeTargetEntry(lcolexpr,
							  next_resno++,
							  colName,
							  false);
		targetList = lappend(targetList, tle);
		left_tlist = lnext(left_tlist);
	}

	/* Now build CTE's output column info using dummy targetlist */
	analyzeCTETargetList(pstate, pstate->p_parent_cte, targetList);
}

/*
 * Attach column names from a ColumnDef list to a TargetEntry list
 * (for CREATE TABLE AS)
 */
static void
applyColumnNames(List *dst, List *src)
{
	ListCell   *dst_item;
	ListCell   *src_item;

	src_item = list_head(src);

	foreach(dst_item, dst)
	{
		TargetEntry *d = (TargetEntry *) lfirst(dst_item);
		ColumnDef  *s;

		/* junk targets don't count */
		if (d->resjunk)
			continue;

		/* fewer ColumnDefs than target entries is OK */
		if (src_item == NULL)
			break;

		s = (ColumnDef *) lfirst(src_item);
		src_item = lnext(src_item);

		d->resname = pstrdup(s->colname);
	}

	/* more ColumnDefs than target entries is not OK */
	if (src_item != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("CREATE TABLE AS specifies too many column names")));
}


/*
 * transformUpdateStmt -
 *	  transforms an update statement
 */
static Query *
transformUpdateStmt(ParseState *pstate, UpdateStmt *stmt)
{
	Query	   *qry = makeNode(Query);
	RangeTblEntry *target_rte;
	Node	   *qual;
	ListCell   *origTargetList;
	ListCell   *tl;

	qry->commandType = CMD_UPDATE;
	pstate->p_is_update = true;

	qry->resultRelation = setTargetTable(pstate, stmt->relation,
								  interpretInhOption(stmt->relation->inhOpt),
										 true,
										 ACL_UPDATE);

	/*
	 * the FROM clause is non-standard SQL syntax. We used to be able to do
	 * this with REPLACE in POSTQUEL so we keep the feature.
	 */
	transformFromClause(pstate, stmt->fromClause);

	qry->targetList = transformTargetList(pstate, stmt->targetList,
										  EXPR_KIND_UPDATE_SOURCE);

	qual = transformWhereClause(pstate, stmt->whereClause,
								EXPR_KIND_WHERE, "WHERE");

	/*
	 * MPP-2506 [insert/update/delete] RETURNING clause not supported:
	 *   We have problems processing the returning clause, so for now we have
	 *   simply removed it and replaced it with an error message.
	 */
#ifdef MPP_RETURNING_NOT_SUPPORTED
	if (stmt->returningList)
	{
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("The RETURNING clause of the UPDATE statement is not "
						"supported in this version of Greenplum Database.")));
	}
#else
	qry->returningList = transformReturningList(pstate, stmt->returningList);
#endif

    /*
     * CDB: Untyped Const or Param nodes in a subquery in the FROM clause
     * could have been assigned proper types when we transformed the WHERE
     * clause or targetlist above.  Bring targetlist Var types up to date.
     */
    if (stmt->fromClause)
    {
        fixup_unknown_vars_in_targetlist(pstate, qry->targetList);
        fixup_unknown_vars_in_targetlist(pstate, qry->returningList);
    }

	qry->rtable = pstate->p_rtable;
	qry->jointree = makeFromExpr(pstate->p_joinlist, qual);

	qry->hasSubLinks = pstate->p_hasSubLinks;
	qry->hasFuncsWithExecRestrictions = pstate->p_hasFuncsWithExecRestrictions;

	/*
	 * Now we are done with SELECT-like processing, and can get on with
	 * transforming the target list to match the UPDATE target columns.
	 */

	/* Prepare to assign non-conflicting resnos to resjunk attributes */
	if (pstate->p_next_resno <= pstate->p_target_relation->rd_rel->relnatts)
		pstate->p_next_resno = pstate->p_target_relation->rd_rel->relnatts + 1;

	/* Prepare non-junk columns for assignment to target table */
	target_rte = pstate->p_target_rangetblentry;
	origTargetList = list_head(stmt->targetList);

	foreach(tl, qry->targetList)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(tl);
		ResTarget  *origTarget;
		int			attrno;

		if (tle->resjunk)
		{
			/*
			 * Resjunk nodes need no additional processing, but be sure they
			 * have resnos that do not match any target columns; else rewriter
			 * or planner might get confused.  They don't need a resname
			 * either.
			 */
			tle->resno = (AttrNumber) pstate->p_next_resno++;
			tle->resname = NULL;
			continue;
		}
		if (origTargetList == NULL)
			elog(ERROR, "UPDATE target count mismatch --- internal error");
		origTarget = (ResTarget *) lfirst(origTargetList);
		Assert(IsA(origTarget, ResTarget));

		attrno = attnameAttNum(pstate->p_target_relation,
							   origTarget->name, true);
		if (attrno == InvalidAttrNumber)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_COLUMN),
					 errmsg("column \"%s\" of relation \"%s\" does not exist",
							origTarget->name,
						 RelationGetRelationName(pstate->p_target_relation)),
					 parser_errposition(pstate, origTarget->location)));

		updateTargetListEntry(pstate, tle, origTarget->name,
							  attrno,
							  origTarget->indirection,
							  origTarget->location);

		/* Mark the target column as requiring update permissions */
		target_rte->modifiedCols = bms_add_member(target_rte->modifiedCols,
								attrno - FirstLowInvalidHeapAttributeNumber);

		origTargetList = lnext(origTargetList);
	}
	if (origTargetList != NULL)
		elog(ERROR, "UPDATE target count mismatch --- internal error");

	return qry;
}


/*
 * MPP-2506 [insert/update/delete] RETURNING clause not supported:
 *   We have problems processing the returning clause, so for now we have
 *   simply removed it and replaced it with an error message.
 */
#ifndef MPP_RETURNING_NOT_SUPPORTED
/*
 * transformReturningList -
 *	handle a RETURNING clause in INSERT/UPDATE/DELETE
 */
static List *
transformReturningList(ParseState *pstate, List *returningList)
{
	List	   *rlist;
	int			save_next_resno;
	int			length_rtable;

	if (returningList == NIL)
		return NIL;				/* nothing to do */

	/*
	 * We need to assign resnos starting at one in the RETURNING list. Save
	 * and restore the main tlist's value of p_next_resno, just in case
	 * someone looks at it later (probably won't happen).
	 */
	save_next_resno = pstate->p_next_resno;
	pstate->p_next_resno = 1;

	/* transform RETURNING identically to a SELECT targetlist */
	rlist = transformTargetList(pstate, returningList, EXPR_KIND_RETURNING);

	/* no new relation references please */
	if (list_length(pstate->p_rtable) != length_rtable)
	{
		int			vlocation = -1;
		int			relid;

		/* try to locate such a reference to point to */
		for (relid = length_rtable + 1; relid <= list_length(pstate->p_rtable); relid++)
		{
			vlocation = locate_var_of_relation((Node *) rlist, relid, 0);
			if (vlocation >= 0)
				break;
		}
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			errmsg("RETURNING cannot contain references to other relations"),
				 parser_errposition(pstate, vlocation)));
	}

	/* mark column origins */
	markTargetListOrigins(pstate, rlist);

	/* restore state */
	pstate->p_next_resno = save_next_resno;

	return rlist;
}
#endif


/*
 * transformDeclareCursorStmt -
 *	transform a DECLARE CURSOR Statement
 *
 * DECLARE CURSOR is a hybrid case: it's an optimizable statement (in fact not
 * significantly different from a SELECT) as far as parsing/rewriting/planning
 * are concerned, but it's not passed to the executor and so in that sense is
 * a utility statement.  We transform it into a Query exactly as if it were
 * a SELECT, then stick the original DeclareCursorStmt into the utilityStmt
 * field to carry the cursor name and options.
 */
static Query *
transformDeclareCursorStmt(ParseState *pstate, DeclareCursorStmt *stmt)
{
	Query	   *result;

	/*
	 * Don't allow both SCROLL and NO SCROLL to be specified
	 */
	if ((stmt->options & CURSOR_OPT_SCROLL) &&
		(stmt->options & CURSOR_OPT_NO_SCROLL))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_CURSOR_DEFINITION),
				 errmsg("cannot specify both SCROLL and NO SCROLL")));

	result = transformStmt(pstate, stmt->query);

	/* Grammar should not have allowed anything but SELECT */
	if (!IsA(result, Query) ||
		result->commandType != CMD_SELECT ||
		result->utilityStmt != NULL)
		elog(ERROR, "unexpected non-SELECT command in DECLARE CURSOR");

	/* But we must explicitly disallow DECLARE CURSOR ... SELECT INTO */
	if (result->intoClause)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_CURSOR_DEFINITION),
				 errmsg("DECLARE CURSOR cannot specify INTO"),
				 parser_errposition(pstate,
								exprLocation((Node *) result->intoClause))));

	/* FOR UPDATE and WITH HOLD are not compatible */
	if (result->rowMarks != NIL && (stmt->options & CURSOR_OPT_HOLD))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("DECLARE CURSOR WITH HOLD ... FOR UPDATE/SHARE is not supported"),
				 errdetail("Holdable cursors must be READ ONLY.")));

	/* FOR UPDATE and SCROLL are not compatible */
	if (result->rowMarks != NIL && (stmt->options & CURSOR_OPT_SCROLL))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		errmsg("DECLARE SCROLL CURSOR ... FOR UPDATE/SHARE is not supported"),
				 errdetail("Scrollable cursors must be READ ONLY.")));

	/* FOR UPDATE and INSENSITIVE are not compatible */
	if (result->rowMarks != NIL && (stmt->options & CURSOR_OPT_INSENSITIVE))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("DECLARE INSENSITIVE CURSOR ... FOR UPDATE/SHARE is not supported"),
				 errdetail("Insensitive cursors must be READ ONLY.")));

	/* We won't need the raw querytree any more */
	stmt->query = NULL;

	result->utilityStmt = (Node *) stmt;

	return result;
}

/*
 * transformExplainStmt -
 *	transform an EXPLAIN Statement
 *
 * EXPLAIN is just like other utility statements in that we emit it as a
 * CMD_UTILITY Query node with no transformation of the raw parse tree.
 * However, if p_coerce_param_hook is set, it could be that the client is
 * expecting us to resolve parameter types in something like
 *		EXPLAIN SELECT * FROM tab WHERE col = $1
 * To deal with such cases, we run parse analysis and throw away the result;
 * this is a bit grotty but not worth contorting the rest of the system for.
 * (The approach we use for DECLARE CURSOR won't work because the statement
 * being explained isn't necessarily a SELECT, and in particular might rewrite
 * to multiple parsetrees.)
 */
static Query *
transformExplainStmt(ParseState *pstate, ExplainStmt *stmt)
{
	Query	   *result;

	if (pstate->p_coerce_param_hook != NULL)
	{
		/* Since parse analysis scribbles on its input, copy the tree first! */
		(void) transformStmt(pstate, copyObject(stmt->query));
	}

	/* Now return the untransformed command as a utility Query */
	result = makeNode(Query);
	result->commandType = CMD_UTILITY;
	result->utilityStmt = (Node *) stmt;

	return result;
}


/*
 * Check for features that are not supported together with FOR UPDATE/SHARE.
 *
 * exported so planner can check again after rewriting, query pullup, etc
 */
void
CheckSelectLocking(Query *qry)
{
	if (qry->setOperations)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("SELECT FOR UPDATE/SHARE is not allowed with UNION/INTERSECT/EXCEPT")));
	if (qry->distinctClause != NIL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("SELECT FOR UPDATE/SHARE is not allowed with DISTINCT clause")));
	if (qry->groupClause != NIL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("SELECT FOR UPDATE/SHARE is not allowed with GROUP BY clause")));
	if (qry->havingQual != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		errmsg("SELECT FOR UPDATE/SHARE is not allowed with HAVING clause")));
	if (qry->hasAggs)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("SELECT FOR UPDATE/SHARE is not allowed with aggregate functions")));
	if (qry->hasWindowFuncs)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("SELECT FOR UPDATE/SHARE is not allowed with window functions")));
	if (expression_returns_set((Node *) qry->targetList))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("SELECT FOR UPDATE/SHARE is not allowed with set-returning functions in the target list")));
}

/*
 * Transform a FOR UPDATE/SHARE clause
 *
 * This basically involves replacing names by integer relids.
 *
 * NB: if you need to change this, see also markQueryForLocking()
 * in rewriteHandler.c, and isLockedRefname() in parse_relation.c.
 */
static void
transformLockingClause(ParseState *pstate, Query *qry, LockingClause *lc,
					   bool pushedDown)
{
	List	   *lockedRels = lc->lockedRels;
	ListCell   *l;
	ListCell   *rt;
	Index		i;
	LockingClause *allrels;

	CheckSelectLocking(qry);

	/* make a clause we can pass down to subqueries to select all rels */
	allrels = makeNode(LockingClause);
	allrels->lockedRels = NIL;	/* indicates all rels */
	allrels->forUpdate = lc->forUpdate;
	allrels->noWait = lc->noWait;

	if (lockedRels == NIL)
	{
		/* all regular tables used in query */
		i = 0;
		foreach(rt, qry->rtable)
		{
			RangeTblEntry *rte = (RangeTblEntry *) lfirst(rt);

			++i;
			switch (rte->rtekind)
			{
				case RTE_RELATION:
					if(get_rel_relstorage(rte->relid) == RELSTORAGE_EXTERNAL)
						ereport(ERROR,
								(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
								 errmsg("SELECT FOR UPDATE/SHARE cannot be applied to external tables")));

					applyLockingClause(qry, i,
									   lc->forUpdate, lc->noWait, pushedDown);
					rte->requiredPerms |= ACL_SELECT_FOR_UPDATE;
					break;
				case RTE_SUBQUERY:
					applyLockingClause(qry, i,
									   lc->forUpdate, lc->noWait, pushedDown);
					/*
					 * FOR UPDATE/SHARE of subquery is propagated to all of
					 * subquery's rels, too.  We could do this later (based
					 * on the marking of the subquery RTE) but it is convenient
					 * to have local knowledge in each query level about
					 * which rels need to be opened with RowShareLock.
					 */
					transformLockingClause(pstate, rte->subquery,
										   allrels, true);
					break;
				default:
					/* ignore JOIN, SPECIAL, FUNCTION, VALUES, CTE RTEs */
					break;
			}
		}
	}
	else
	{
		/* just the named tables */
		foreach(l, lockedRels)
		{
			RangeVar   *thisrel = (RangeVar *) lfirst(l);

			/* For simplicity we insist on unqualified alias names here */
			if (thisrel->catalogname || thisrel->schemaname)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("SELECT FOR UPDATE/SHARE must specify unqualified relation names"),
						 parser_errposition(pstate, thisrel->location)));

			i = 0;
			foreach(rt, qry->rtable)
			{
				RangeTblEntry *rte = (RangeTblEntry *) lfirst(rt);

				++i;
				if (strcmp(rte->eref->aliasname, thisrel->relname) == 0)
				{
					switch (rte->rtekind)
					{
						case RTE_RELATION:
							if(get_rel_relstorage(rte->relid) == RELSTORAGE_EXTERNAL)
								ereport(ERROR,
										(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
										 errmsg("SELECT FOR UPDATE/SHARE cannot be applied to external tables")));

							applyLockingClause(qry, i,
											   lc->forUpdate, lc->noWait,
											   pushedDown);
							rte->requiredPerms |= ACL_SELECT_FOR_UPDATE;
							break;
						case RTE_SUBQUERY:
							applyLockingClause(qry, i,
											   lc->forUpdate, lc->noWait,
											   pushedDown);
							/* see comment above */
							transformLockingClause(pstate, rte->subquery,
												   allrels, true);
							break;
						case RTE_JOIN:
							ereport(ERROR,
									(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
									 errmsg("SELECT FOR UPDATE/SHARE cannot be applied to a join"),
							 parser_errposition(pstate, thisrel->location)));
							break;
						case RTE_SPECIAL:
							ereport(ERROR,
									(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
									 errmsg("SELECT FOR UPDATE/SHARE cannot be applied to NEW or OLD"),
							 parser_errposition(pstate, thisrel->location)));
							break;
						case RTE_FUNCTION:
							ereport(ERROR,
									(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
									 errmsg("SELECT FOR UPDATE/SHARE cannot be applied to a function"),
							 parser_errposition(pstate, thisrel->location)));
							break;
						case RTE_TABLEFUNCTION:
							ereport(ERROR,
									(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
									 errmsg("SELECT FOR UPDATE/SHARE cannot be applied to a table function")));
							break;
						case RTE_VALUES:
							ereport(ERROR,
									(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
									 errmsg("SELECT FOR UPDATE/SHARE cannot be applied to VALUES"),
							 parser_errposition(pstate, thisrel->location)));
							break;
						case RTE_CTE:
							ereport(ERROR,
									(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
									 errmsg("SELECT FOR UPDATE/SHARE cannot be applied to a WITH query"),
							 parser_errposition(pstate, thisrel->location)));
							break;
						default:
							elog(ERROR, "unrecognized RTE type: %d",
								 (int) rte->rtekind);
							break;
					}
					break;		/* out of foreach loop */
				}
			}
			if (rt == NULL)
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_TABLE),
						 errmsg("relation \"%s\" in FOR UPDATE/SHARE clause not found in FROM clause",
								thisrel->relname),
						 parser_errposition(pstate, thisrel->location)));
		}
	}
}

/*
 * Record locking info for a single rangetable item
 */
void
applyLockingClause(Query *qry, Index rtindex,
				   bool forUpdate, bool noWait, bool pushedDown)
{
	RowMarkClause *rc;

	/* If it's an explicit clause, make sure hasForUpdate gets set */
	if (!pushedDown)
		qry->hasForUpdate = true;

	/* Check for pre-existing entry for same rtindex */
	if ((rc = get_parse_rowmark(qry, rtindex)) != NULL)
	{
		/*
		 * If the same RTE is specified both FOR UPDATE and FOR SHARE, treat
		 * it as FOR UPDATE.  (Reasonable, since you can't take both a shared
		 * and exclusive lock at the same time; it'll end up being exclusive
		 * anyway.)
		 *
		 * We also consider that NOWAIT wins if it's specified both ways. This
		 * is a bit more debatable but raising an error doesn't seem helpful.
		 * (Consider for instance SELECT FOR UPDATE NOWAIT from a view that
		 * internally contains a plain FOR UPDATE spec.)
		 *
		 * And of course pushedDown becomes false if any clause is explicit.
		 */
		rc->forUpdate |= forUpdate;
		rc->noWait |= noWait;
		rc->pushedDown &= pushedDown;
		return;
	}

	/* Make a new RowMarkClause */
	rc = makeNode(RowMarkClause);
	rc->rti = rtindex;
	rc->forUpdate = forUpdate;
	rc->noWait = noWait;
	rc->pushedDown = pushedDown;
	qry->rowMarks = lappend(qry->rowMarks, rc);
}

static void
setQryDistributionPolicy(SelectStmt *stmt, Query *qry)
{
	ListCell   *keys = NULL;

	GpPolicy  *policy = NULL;
	int			colindex = 0;

	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	Assert(stmt->distributedBy);

	if (stmt->distributedBy)
	{
		/*
		 * We have a DISTRIBUTED BY column list specified by the user
		 * Process it now and set the distribution policy.
		 */
		if (list_length(stmt->distributedBy) > MaxPolicyAttributeNumber)
			ereport(ERROR,
					(errcode(ERRCODE_TOO_MANY_COLUMNS),
					 errmsg("number of distributed by columns exceeds limit (%d)",
							MaxPolicyAttributeNumber)));

		policy = (GpPolicy *) palloc0(sizeof(GpPolicy) - sizeof(policy->attrs)
								+ list_length(stmt->distributedBy) * sizeof(policy->attrs[0]));
		policy->ptype = POLICYTYPE_PARTITIONED;
		policy->nattrs = 0;

		if (stmt->distributedBy->length == 1 && (list_head(stmt->distributedBy) == NULL || linitial(stmt->distributedBy) == NULL))
		{
			/* distributed randomly */
			qry->intoPolicy = policy;
		}
		else
		{
			foreach(keys, stmt->distributedBy)
			{
				char	   *key = strVal(lfirst(keys));
				bool		found = false;
				AttrNumber	n;

				for (n=1; n <= list_length(qry->targetList); n++)
				{
					TargetEntry *target = get_tle_by_resno(qry->targetList, n);
					colindex = n;

					if (target->resname && strcmp(target->resname, key) == 0)
					{
						found = true;
						break;
					}
				}

				if (!found)
					ereport(ERROR,
							(errcode(ERRCODE_UNDEFINED_COLUMN),
							 errmsg("column \"%s\" named in DISTRIBUTED BY "
									"clause does not exist",
									key)));
	
				policy->attrs[policy->nattrs++] = colindex;
	
			}
			qry->intoPolicy = policy;
		}
	}
}
