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
 * DECLARE CURSOR, EXPLAIN, and CREATE TABLE AS are exceptions because they
 * contain optimizable statements, which we should transform.
 *
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *	src/backend/parser/analyze.c
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
#include "parser/parse_collate.h"
#include "parser/parse_cte.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "parser/parse_param.h"
#include "parser/parse_relation.h"
#include "parser/parse_target.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"
#include "utils/rel.h"

#include "cdb/cdbhash.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbutil.h"
#include "catalog/gp_policy.h"
#include "commands/defrem.h"
#include "access/htup_details.h"
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

/* Hook for plugins to get control at end of parse analysis */
post_parse_analyze_hook_type post_parse_analyze_hook = NULL;

static Query *transformDeleteStmt(ParseState *pstate, DeleteStmt *stmt);
static Query *transformInsertStmt(ParseState *pstate, InsertStmt *stmt);
static List *transformInsertRow(ParseState *pstate, List *exprlist,
				   List *stmtcols, List *icolumns, List *attrnos);
static int	count_rowexpr_columns(ParseState *pstate, Node *expr);
static Query *transformSelectStmt(ParseState *pstate, SelectStmt *stmt);
static Query *transformValuesClause(ParseState *pstate, SelectStmt *stmt);
static Query *transformSetOperationStmt(ParseState *pstate, SelectStmt *stmt);
static Node *transformSetOperationTree(ParseState *pstate, SelectStmt *stmt,
									   bool isTopLevel, List **targetlist);
static Node *transformSetOperationTree_internal(ParseState *pstate, SelectStmt *stmt,
												bool isTopLevel, setop_types_ctx *setop_types);
static void coerceSetOpTypes(ParseState *pstate, Node *sop,
							 List *preselected_coltypes, List *preselected_coltypmods,
							 List **targetlist);
static void select_setop_types(ParseState *pstate, setop_types_ctx *ctx, SetOperation op, List **selected_types, List **selected_typmods);
static void determineRecursiveColTypes(ParseState *pstate,
						   Node *larg, List *nrtargetlist);
static Query *transformUpdateStmt(ParseState *pstate, UpdateStmt *stmt);
static List *transformReturningList(ParseState *pstate, List *returningList);
static Query *transformDeclareCursorStmt(ParseState *pstate,
						   DeclareCursorStmt *stmt);
static Query *transformExplainStmt(ParseState *pstate,
					 ExplainStmt *stmt);
static Query *transformCreateTableAsStmt(ParseState *pstate,
						   CreateTableAsStmt *stmt);
static void transformLockingClause(ParseState *pstate, Query *qry,
					   LockingClause *lc, bool pushedDown);

static int get_distkey_by_name(char *key, IntoClause *into, Query *qry, bool *found);
static void setQryDistributionPolicy(IntoClause *into, Query *qry);

static Query *transformGroupedWindows(ParseState *pstate, Query *qry);
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
 * The result is a Query node.  Optimizable statements require considerable
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

	query = transformTopLevelStmt(pstate, parseTree);

	if (post_parse_analyze_hook)
		(*post_parse_analyze_hook) (pstate, query);

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

	query = transformTopLevelStmt(pstate, parseTree);

	/* make sure all is well with parameter types */
	check_variable_parameters(pstate, query);

	if (post_parse_analyze_hook)
		(*post_parse_analyze_hook) (pstate, query);

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
 * transformTopLevelStmt -
 *	  transform a Parse tree into a Query tree.
 *
 * The only thing we do here that we don't do in transformStmt() is to
 * convert SELECT ... INTO into CREATE TABLE AS.  Since utility statements
 * aren't allowed within larger statements, this is only allowed at the top
 * of the parse tree, and so we only try it before entering the recursive
 * transformStmt() processing.
 */
Query *
transformTopLevelStmt(ParseState *pstate, Node *parseTree)
{
	if (IsA(parseTree, SelectStmt))
	{
		SelectStmt *stmt = (SelectStmt *) parseTree;

		/* If it's a set-operation tree, drill down to leftmost SelectStmt */
		while (stmt && stmt->op != SETOP_NONE)
			stmt = stmt->larg;
		Assert(stmt && IsA(stmt, SelectStmt) &&stmt->larg == NULL);

		if (stmt->intoClause)
		{
			CreateTableAsStmt *ctas = makeNode(CreateTableAsStmt);

			ctas->query = parseTree;
			ctas->into = stmt->intoClause;
			ctas->relkind = OBJECT_TABLE;
			ctas->is_select_into = true;

			/*
			 * Remove the intoClause from the SelectStmt.  This makes it safe
			 * for transformSelectStmt to complain if it finds intoClause set
			 * (implying that the INTO appeared in a disallowed place).
			 */
			stmt->intoClause = NULL;

			parseTree = (Node *) ctas;
		}
	}

	return transformStmt(pstate, parseTree);
}

/*
 * transformStmt -
 *	  recursively transform a Parse tree into a Query tree.
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

		case T_CreateTableAsStmt:
			result = transformCreateTableAsStmt(pstate,
											(CreateTableAsStmt *) parseTree);
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
 * Classification here should match transformStmt().
 */
bool
analyze_requires_snapshot(Node *parseTree)
{
	bool		result;

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
		case T_CreateTableAsStmt:
			/* yes, because we must analyze the contained statement */
			result = true;
			break;

		default:
			/* other utility statements don't have any real parse analysis */
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
	ParseNamespaceItem *nsitem;
	Node	   *qual;

	qry->commandType = CMD_DELETE;

	/* process the WITH clause independently of all else */
	if (stmt->withClause)
	{
		qry->hasRecursive = stmt->withClause->recursive;
		qry->cteList = transformWithClause(pstate, stmt->withClause);
		qry->hasModifyingCTE = pstate->p_hasModifyingCTE;

		/*
		 * Since GPDB currently only support a single writer gang, only one
		 * writable clause is permitted per CTE. Once we get flexible gangs
		 * with more than one writer gang we can lift this restriction.
		 */
		if (pstate->p_hasModifyingCTE)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("writable CTE queries cannot be themselves writable"),
					 errdetail("Greenplum Database currently only support CTEs with one writable clause, called in a non-writable context."),
					 errhint("Rewrite the query to only include one writable clause.")));
	}

	/* set up range table with just the result rel */
	qry->resultRelation = setTargetTable(pstate, stmt->relation,
								  interpretInhOption(stmt->relation->inhOpt),
										 true,
										 ACL_DELETE);

	/* grab the namespace item made by setTargetTable */
	nsitem = (ParseNamespaceItem *) llast(pstate->p_namespace);

	/* there's no DISTINCT in DELETE */
	qry->distinctClause = NIL;

	/* subqueries in USING cannot access the result relation */
	nsitem->p_lateral_only = true;
	nsitem->p_lateral_ok = false;

	/*
	 * The USING clause is non-standard SQL syntax, and is equivalent in
	 * functionality to the FROM list that can be specified for UPDATE. The
	 * USING keyword is used rather than FROM because FROM is already a
	 * keyword in the DELETE syntax.
	 */
	transformFromClause(pstate, stmt->usingClause);

	/* remaining clauses can reference the result relation normally */
	nsitem->p_lateral_only = false;
	nsitem->p_lateral_ok = true;

	qual = transformWhereClause(pstate, stmt->whereClause,
								EXPR_KIND_WHERE, "WHERE");

	qry->returningList = transformReturningList(pstate, stmt->returningList);

	/* done building the range table and jointree */
	qry->rtable = pstate->p_rtable;
	qry->jointree = makeFromExpr(pstate->p_joinlist, qual);

	qry->hasSubLinks = pstate->p_hasSubLinks;
	qry->hasWindowFuncs = pstate->p_hasWindowFuncs;
	qry->hasAggs = pstate->p_hasAggs;
	qry->hasFuncsWithExecRestrictions = pstate->p_hasFuncsWithExecRestrictions;

	if (pstate->p_hasTblValueExpr)
		parseCheckTableFunctions(pstate, qry);

	assign_query_collations(pstate, qry);

	/* this must be done after collations, for reliable comparison of exprs */
	if (pstate->p_hasAggs)
		parseCheckAggregates(pstate, qry);

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
	List	   *sub_namespace;
	List	   *icolumns;
	List	   *attrnos;
	RangeTblEntry *rte;
	RangeTblRef *rtr;
	ListCell   *icols;
	ListCell   *attnos;
	ListCell   *lc;

	/* There can't be any outer WITH to worry about */
	Assert(pstate->p_ctenamespace == NIL);

	qry->commandType = CMD_INSERT;
	pstate->p_is_insert = true;

	/* process the WITH clause independently of all else */
	if (stmt->withClause)
	{
		qry->hasRecursive = stmt->withClause->recursive;
		qry->cteList = transformWithClause(pstate, stmt->withClause);
		qry->hasModifyingCTE = pstate->p_hasModifyingCTE;

		/*
		 * Since GPDB currently only support a single writer gang, only one
		 * writable clause is permitted per CTE. Once we get flexible gangs
		 * with more than one writer gang we can lift this restriction.
		 */
		if (pstate->p_hasModifyingCTE)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("writable CTE queries cannot be themselves writable"),
					 errdetail("Greenplum Database currently only support CTEs with one writable clause, called in a non-writable context."),
					 errhint("Rewrite the query to only include one writable clause.")));
	}

	/*
	 * We have three cases to deal with: DEFAULT VALUES (selectStmt == NULL),
	 * VALUES list, or general SELECT input.  We special-case VALUES, both for
	 * efficiency and so we can handle DEFAULT specifications.
	 *
	 * The grammar allows attaching ORDER BY, LIMIT, FOR UPDATE, or WITH to a
	 * VALUES clause.  If we have any of those, treat it as a general SELECT;
	 * so it will work, but you can't use DEFAULT items together with those.
	 */
	isGeneralSelect = (selectStmt && (selectStmt->valuesLists == NIL ||
									  selectStmt->sortClause != NIL ||
									  selectStmt->limitOffset != NULL ||
									  selectStmt->limitCount != NULL ||
									  selectStmt->lockingClause != NIL ||
									  selectStmt->withClause != NULL));

	/*
	 * If a non-nil rangetable/namespace was passed in, and we are doing
	 * INSERT/SELECT, arrange to pass the rangetable/namespace down to the
	 * SELECT.  This can only happen if we are inside a CREATE RULE, and in
	 * that case we want the rule's OLD and NEW rtable entries to appear as
	 * part of the SELECT's rtable, not as outer references for it.  (Kluge!)
	 * The SELECT's joinlist is not affected however.  We must do this before
	 * adding the target table to the INSERT's rtable.
	 */
	if (isGeneralSelect)
	{
		sub_rtable = pstate->p_rtable;
		pstate->p_rtable = NIL;
		sub_namespace = pstate->p_namespace;
		pstate->p_namespace = NIL;
	}
	else
	{
		sub_rtable = NIL;		/* not used, but keep compiler quiet */
		sub_namespace = NIL;
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
		sub_pstate->p_namespace = sub_namespace;

		selectQuery = transformStmt(sub_pstate, stmt->selectStmt);

		free_parsestate(sub_pstate);

		/* The grammar should have produced a SELECT */
		if (!IsA(selectQuery, Query) ||
			selectQuery->commandType != CMD_SELECT ||
			selectQuery->utilityStmt != NULL)
			elog(ERROR, "unexpected non-SELECT command in INSERT ... SELECT");

		/*
		 * Make the source be a subquery in the INSERT's rangetable, and add
		 * it to the INSERT's joinlist.
		 */
		rte = addRangeTableEntryForSubquery(pstate,
											selectQuery,
											makeAlias("*SELECT*", NIL),
											false,
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
		List	   *collations = NIL;
		int			sublist_length = -1;
		bool		lateral = false;
		int			i;

		Assert(selectStmt->intoClause == NULL);

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

			/*
			 * We must assign collations now because assign_query_collations
			 * doesn't process rangetable entries.  We just assign all the
			 * collations independently in each row, and don't worry about
			 * whether they are consistent vertically.  The outer INSERT query
			 * isn't going to care about the collations of the VALUES columns,
			 * so it's not worth the effort to identify a common collation for
			 * each one here.  (But note this does have one user-visible
			 * consequence: INSERT ... VALUES won't complain about conflicting
			 * explicit COLLATEs in a column, whereas the same VALUES
			 * construct in another context would complain.)
			 */
			assign_list_collations(pstate, sublist);

			exprsLists = lappend(exprsLists, sublist);
		}

		/*
		 * Although we don't really need collation info, let's just make sure
		 * we provide a correctly-sized list in the VALUES RTE.
		 */
		for (i = 0; i < sublist_length; i++)
			collations = lappend_oid(collations, InvalidOid);

		/*
		 * Ordinarily there can't be any current-level Vars in the expression
		 * lists, because the namespace was empty ... but if we're inside
		 * CREATE RULE, then NEW/OLD references might appear.  In that case we
		 * have to mark the VALUES RTE as LATERAL.
		 */
		if (list_length(pstate->p_rtable) != 1 &&
			contain_vars_of_level((Node *) exprsLists, 0))
			lateral = true;

		/*
		 * Generate the VALUES RTE
		 */
		rte = addRangeTableEntryForValues(pstate, exprsLists, collations,
										  NULL, lateral, true);
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
		Assert(selectStmt->intoClause == NULL);

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
	 * If we have a RETURNING clause, we need to add the target relation to
	 * the query namespace before processing it, so that Var references in
	 * RETURNING will work.  Also, remove any namespace entries added in a
	 * sub-SELECT or VALUES list.
	 */
	if (stmt->returningList)
	{
		pstate->p_namespace = NIL;
		addRTEtoQuery(pstate, pstate->p_target_rangetblentry,
					  false, true, true);
		qry->returningList = transformReturningList(pstate,
													stmt->returningList);
	}

	/* done building the range table and jointree */
	qry->rtable = pstate->p_rtable;
	qry->jointree = makeFromExpr(pstate->p_joinlist, NULL);

	qry->hasSubLinks = pstate->p_hasSubLinks;
	qry->hasFuncsWithExecRestrictions = pstate->p_hasFuncsWithExecRestrictions;

	assign_query_collations(pstate, qry);

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
	 * defaulted).  Note we must check this *after* transformation because
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
	{
		/*
		 * We can get here for cases like INSERT ... SELECT (a,b,c) FROM ...
		 * where the user accidentally created a RowExpr instead of separate
		 * columns.  Add a suitable hint if that seems to be the problem,
		 * because the main error message is quite misleading for this case.
		 * (If there's no stmtcols, you'll get something about data type
		 * mismatch, which is less misleading so we don't worry about giving a
		 * hint in that case.)
		 */
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("INSERT has more target columns than expressions"),
				 ((list_length(exprlist) == 1 &&
				   count_rowexpr_columns(pstate, linitial(exprlist)) ==
				   list_length(icolumns)) ?
				  errhint("The insertion source is a row expression containing the same number of columns expected by the INSERT. Did you accidentally use extra parentheses?") : 0),
				 parser_errposition(pstate,
									exprLocation(list_nth(icolumns,
												  list_length(exprlist))))));
	}

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
transformGroupedWindows(ParseState *pstate, Query *qry)
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

	assign_query_collations(pstate, subq);

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
		var->varcollid = exprCollation((Node*)tle->expr);
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
 * count_rowexpr_columns -
 *	  get number of columns contained in a ROW() expression;
 *	  return -1 if expression isn't a RowExpr or a Var referencing one.
 *
 * This is currently used only for hint purposes, so we aren't terribly
 * tense about recognizing all possible cases.  The Var case is interesting
 * because that's what we'll get in the INSERT ... SELECT (...) case.
 */
static int
count_rowexpr_columns(ParseState *pstate, Node *expr)
{
	if (expr == NULL)
		return -1;
	if (IsA(expr, RowExpr))
		return list_length(((RowExpr *) expr)->args);
	if (IsA(expr, Var))
	{
		Var		   *var = (Var *) expr;
		AttrNumber	attnum = var->varattno;

		if (attnum > 0 && var->vartype == RECORDOID)
		{
			RangeTblEntry *rte;

			rte = GetRTEByRangeTablePosn(pstate, var->varno, var->varlevelsup);
			if (rte->rtekind == RTE_SUBQUERY)
			{
				/* Subselect-in-FROM: examine sub-select's output expr */
				TargetEntry *ste = get_tle_by_resno(rte->subquery->targetList,
													attnum);

				if (ste == NULL || ste->resjunk)
					return -1;
				expr = (Node *) ste->expr;
				if (IsA(expr, RowExpr))
					return list_length(((RowExpr *) expr)->args);
			}
		}
	}
	return -1;
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

	/* process the WITH clause independently of all else */
	if (stmt->withClause)
	{
		qry->hasRecursive = stmt->withClause->recursive;
		qry->cteList = transformWithClause(pstate, stmt->withClause);
		qry->hasModifyingCTE = pstate->p_hasModifyingCTE;
	}

	/* Complain if we get called from someplace where INTO is not allowed */
	if (stmt->intoClause)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("SELECT ... INTO is not allowed here"),
				 parser_errposition(pstate,
								  exprLocation((Node *) stmt->intoClause))));

	/* make FOR UPDATE/FOR SHARE info available to addRangeTableEntry */
	pstate->p_locking_clause = stmt->lockingClause;

	/*
	 * Put WINDOW clause data into pstate so that window references know
	 * about them.
	 */
	pstate->p_windowdefs = stmt->windowClause;

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

	/* initial processing of HAVING clause is much like WHERE clause */
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
										  true /* fix unknowns */ ,
										  false /* allow SQL92 rules */ );

	qry->groupClause = transformGroupClause(pstate,
											stmt->groupClause,
											&qry->targetList,
											qry->sortClause,
											EXPR_KIND_GROUP_BY,
											false /* allow SQL92 rules */ );

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
		if (!pstate->p_hasAggs && !pstate->p_hasWindowFuncs && qry->groupClause == NIL &&
			qry->targetList != NIL)
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

	qry->rtable = pstate->p_rtable;
	qry->jointree = makeFromExpr(pstate->p_joinlist, qual);

	qry->hasSubLinks = pstate->p_hasSubLinks;
	qry->hasWindowFuncs = pstate->p_hasWindowFuncs;
	qry->hasFuncsWithExecRestrictions = pstate->p_hasFuncsWithExecRestrictions;
	qry->hasAggs = pstate->p_hasAggs;

	if (pstate->p_hasTblValueExpr)
		parseCheckTableFunctions(pstate, qry);

	foreach(l, stmt->lockingClause)
	{
		transformLockingClause(pstate, qry,
							   (LockingClause *) lfirst(l), false);
	}

	assign_query_collations(pstate, qry);

	/* this must be done after collations, for reliable comparison of exprs */
	if (pstate->p_hasAggs || qry->groupClause || qry->havingQual)
		parseCheckAggregates(pstate, qry);

	/*
	 * If the query mixes window functions and aggregates, we need to
	 * transform it such that the grouped query appears as a subquery
	 *
	 * This must be done after collations. Because it, specifically the
	 * grouped_window_mutator() it called, will replace some expressions with
	 * Var and set the varcollid with the replaced expressions' original
	 * collations, which are from assign_query_collations().
	 *
	 * Note: assign_query_collations() doesn't handle Var's collation.
	 */
	if (qry->hasWindowFuncs && (qry->groupClause || qry->hasAggs))
		transformGroupedWindows(pstate, qry);

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
	List	   *exprsLists;
	List	   *collations;
	List	  **colexprs = NULL;
	int			sublist_length = -1;
	bool		lateral = false;
	RangeTblEntry *rte;
	int			rtindex;
	ListCell   *lc;
	ListCell   *lc2;
	int			i;

	qry->commandType = CMD_SELECT;

	/* Most SELECT stuff doesn't apply in a VALUES clause */
	Assert(stmt->distinctClause == NIL);
	Assert(stmt->intoClause == NULL);
	Assert(stmt->targetList == NIL);
	Assert(stmt->fromClause == NIL);
	Assert(stmt->whereClause == NULL);
	Assert(stmt->groupClause == NIL);
	Assert(stmt->havingClause == NULL);
	Assert(stmt->scatterClause == NIL);
	Assert(stmt->op == SETOP_NONE);

	/* process the WITH clause independently of all else */
	if (stmt->withClause)
	{
		qry->hasRecursive = stmt->withClause->recursive;
		qry->cteList = transformWithClause(pstate, stmt->withClause);
		qry->hasModifyingCTE = pstate->p_hasModifyingCTE;
	}

	/*
	 * For each row of VALUES, transform the raw expressions.  This is also a
	 * handy place to reject DEFAULT nodes, which the grammar allows for
	 * simplicity.
	 *
	 * Note that the intermediate representation we build is column-organized
	 * not row-organized.  That simplifies the type and collation processing
	 * below.
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
			/* and allocate array for per-column lists */
			colexprs = (List **) palloc0(sublist_length * sizeof(List *));
		}
		else if (sublist_length != list_length(sublist))
		{
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("VALUES lists must all be the same length"),
					 parser_errposition(pstate,
										exprLocation((Node *) sublist))));
		}

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

		/* Release sub-list's cells to save memory */
		list_free(sublist);
	}

	/*
	 * Now resolve the common types of the columns, and coerce everything to
	 * those types.  Then identify the common collation, if any, of each
	 * column.
	 *
	 * We must do collation processing now because (1) assign_query_collations
	 * doesn't process rangetable entries, and (2) we need to label the VALUES
	 * RTE with column collations for use in the outer query.  We don't
	 * consider conflict of implicit collations to be an error here; instead
	 * the column will just show InvalidOid as its collation, and you'll get a
	 * failure later if that results in failure to resolve a collation.
	 *
	 * Note we modify the per-column expression lists in-place.
	 */
	collations = NIL;
	for (i = 0; i < sublist_length; i++)
	{
		Oid			coltype;
		Oid			colcoll;

		coltype = select_common_type(pstate, colexprs[i], "VALUES", NULL);

		foreach(lc, colexprs[i])
		{
			Node	   *col = (Node *) lfirst(lc);

			col = coerce_to_common_type(pstate, col, coltype, "VALUES");
			lfirst(lc) = (void *) col;
		}

		colcoll = select_common_collation(pstate, colexprs[i], true);

		collations = lappend_oid(collations, colcoll);
	}

	/*
	 * Finally, rearrange the coerced expressions into row-organized lists.
	 */
	exprsLists = NIL;
	foreach(lc, colexprs[0])
	{
		Node	   *col = (Node *) lfirst(lc);
		List	   *sublist;

		sublist = list_make1(col);
		exprsLists = lappend(exprsLists, sublist);
	}
	list_free(colexprs[0]);
	for (i = 1; i < sublist_length; i++)
	{
		forboth(lc, colexprs[i], lc2, exprsLists)
		{
			Node	   *col = (Node *) lfirst(lc);
			List	   *sublist = lfirst(lc2);

			/* sublist pointer in exprsLists won't need adjustment */
			(void) lappend(sublist, col);
		}
		list_free(colexprs[i]);
	}

	/*
	 * Ordinarily there can't be any current-level Vars in the expression
	 * lists, because the namespace was empty ... but if we're inside CREATE
	 * RULE, then NEW/OLD references might appear.  In that case we have to
	 * mark the VALUES RTE as LATERAL.
	 */
	if (pstate->p_rtable != NIL &&
		contain_vars_of_level((Node *) exprsLists, 0))
		lateral = true;

	/*
	 * Generate the VALUES RTE
	 */
	rte = addRangeTableEntryForValues(pstate, exprsLists, collations,
									  NULL, lateral, true);
	addRTEtoQuery(pstate, rte, true, true, true);

	/* assume new rte is at end */
	rtindex = list_length(pstate->p_rtable);
	Assert(rte == rt_fetch(rtindex, pstate->p_rtable));

	/*
	 * Generate a targetlist as though expanding "*"
	 */
	Assert(pstate->p_next_resno == 1);
	qry->targetList = expandRelAttrs(pstate, rte, rtindex, 0, -1);

	/*
	 * The grammar allows attaching ORDER BY, LIMIT, and FOR UPDATE to a
	 * VALUES, so cope.
	 */
	qry->sortClause = transformSortClause(pstate,
										  stmt->sortClause,
										  &qry->targetList,
										  EXPR_KIND_ORDER_BY,
										  true /* fix unknowns */ ,
										  false /* allow SQL92 rules */ );

	qry->limitOffset = transformLimitClause(pstate, stmt->limitOffset,
											EXPR_KIND_OFFSET, "OFFSET");
	qry->limitCount = transformLimitClause(pstate, stmt->limitCount,
										   EXPR_KIND_LIMIT, "LIMIT");

	if (stmt->lockingClause)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		/*------
		  translator: %s is a SQL row locking clause such as FOR UPDATE */
				 errmsg("%s cannot be applied to VALUES",
						LCS_asString(((LockingClause *)
								linitial(stmt->lockingClause))->strength))));

	qry->rtable = pstate->p_rtable;
	qry->jointree = makeFromExpr(pstate->p_joinlist, NULL);

	qry->hasSubLinks = pstate->p_hasSubLinks;
	qry->hasFuncsWithExecRestrictions = pstate->p_hasFuncsWithExecRestrictions;

	assign_query_collations(pstate, qry);

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
	SetOperationStmt *sostmt;
	List	   *sortClause;
	Node	   *limitOffset;
	Node	   *limitCount;
	List	   *lockingClause;
	WithClause *withClause;
	Node	   *node;
	ListCell   *left_tlist,
			   *lct,
			   *lcm,
			   *lcc,
			   *l;
	List	   *targetvars,
			   *targetnames,
			   *sv_namespace;
	int			sv_rtable_length;
	RangeTblEntry *jrte;
	int			tllen;

	qry->commandType = CMD_SELECT;

	/*
	 * Find leftmost leaf SelectStmt.  We currently only need to do this in
	 * order to deliver a suitable error message if there's an INTO clause
	 * there, implying the set-op tree is in a context that doesn't allow
	 * INTO.  (transformSetOperationTree would throw error anyway, but it
	 * seems worth the trouble to throw a different error for non-leftmost
	 * INTO, so we produce that error in transformSetOperationTree.)
	 */
	leftmostSelect = stmt->larg;
	while (leftmostSelect && leftmostSelect->op != SETOP_NONE)
		leftmostSelect = leftmostSelect->larg;
	Assert(leftmostSelect && IsA(leftmostSelect, SelectStmt) &&
		   leftmostSelect->larg == NULL);
	if (leftmostSelect->intoClause)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("SELECT ... INTO is not allowed here"),
				 parser_errposition(pstate,
						exprLocation((Node *) leftmostSelect->intoClause))));

	/*
	 * We need to extract ORDER BY and other top-level clauses here and not
	 * let transformSetOperationTree() see them --- else it'll just recurse
	 * right back here!
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
		/*------
		  translator: %s is a SQL row locking clause such as FOR UPDATE */
				 errmsg("%s is not allowed with UNION/INTERSECT/EXCEPT",
						LCS_asString(((LockingClause *)
									  linitial(lockingClause))->strength))));

	/* Process the WITH clause independently of all else */
	if (withClause)
	{
		qry->hasRecursive = withClause->recursive;
		qry->cteList = transformWithClause(pstate, withClause);
		qry->hasModifyingCTE = pstate->p_hasModifyingCTE;
	}

	/*
	 * Recursively transform the components of the tree.
	 */
	sostmt = (SetOperationStmt *) transformSetOperationTree(pstate, stmt,
															true,
															NULL);
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
	qry->intoPolicy = leftmostQuery->intoPolicy;

	/*
	 * Generate dummy targetlist for outer query using column names of
	 * leftmost select and common datatypes/collations of topmost set
	 * operation.  Also make lists of the dummy vars and their names for use
	 * in parsing ORDER BY.
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

	forthree(lct, sostmt->colTypes,
			 lcm, sostmt->colTypmods,
			 lcc, sostmt->colCollations)
	{
		Oid			colType = lfirst_oid(lct);
		int32		colTypmod = lfirst_int(lcm);
		Oid			colCollation = lfirst_oid(lcc);
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
					  colCollation,
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
	 * using the output columns, generate a namespace entry that makes the
	 * output columns visible.  A Join RTE node is handy for this, since we
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

	sv_namespace = pstate->p_namespace;
	pstate->p_namespace = NIL;

	/* add jrte to column namespace only */
	addRTEtoQuery(pstate, jrte, false, false, true);

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
										  false /* no unknowns expected */ ,
										  false /* allow SQL92 rules */ );

	/* restore namespace, remove jrte from rtable */
	pstate->p_namespace = sv_namespace;
	pstate->p_rtable = list_truncate(pstate->p_rtable, sv_rtable_length);

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

	qry->rtable = pstate->p_rtable;
	qry->jointree = makeFromExpr(pstate->p_joinlist, NULL);

	qry->hasSubLinks = pstate->p_hasSubLinks;
	qry->hasWindowFuncs = pstate->p_hasWindowFuncs;
	qry->hasFuncsWithExecRestrictions = pstate->p_hasFuncsWithExecRestrictions;
	qry->hasAggs = pstate->p_hasAggs;

	if (pstate->p_hasTblValueExpr)
		parseCheckTableFunctions(pstate, qry);

	foreach(l, lockingClause)
	{
		transformLockingClause(pstate, qry,
							   (LockingClause *) lfirst(l), false);
	}

	assign_query_collations(pstate, qry);

	/* this must be done after collations, for reliable comparison of exprs */
	if (pstate->p_hasAggs || qry->groupClause || qry->havingQual)
		parseCheckAggregates(pstate, qry);

	return qry;
}

/*
 * transformSetOperationTree
 *		Recursively transform leaves and internal nodes of a set-op tree
 *
 * In addition to returning the transformed node, if targetlist isn't NULL
 * then we return a list of its non-resjunk TargetEntry nodes.  For a leaf
 * set-op node these are the actual targetlist entries; otherwise they are
 * dummy entries created to carry the type, typmod, collation, and location
 * (for error messages) of each output column of the set-op node.  This info
 * is needed only during the internal recursion of this function, so outside
 * callers pass NULL for targetlist.  Note: the reason for passing the
 * actual targetlist entries of a leaf node is so that upper levels can
 * replace UNKNOWN Consts with properly-coerced constants.
 */
static Node *
transformSetOperationTree(ParseState *pstate, SelectStmt *stmt,
						  bool isTopLevel, List **targetlist)
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

	coerceSetOpTypes(pstate, top, selected_types, selected_typmods, targetlist);

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
			 * cast UNKNOWN to text if the other columns are STRING_TYPE
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
		/*------
		  translator: %s is a SQL row locking clause such as FOR UPDATE */
				 errmsg("%s is not allowed with UNION/INTERSECT/EXCEPT",
						LCS_asString(((LockingClause *)
								linitial(stmt->lockingClause))->strength))));

	/*
	 * If an internal node of a set-op tree has ORDER BY, LIMIT, FOR UPDATE,
	 * or WITH clauses attached, we need to treat it like a leaf node to
	 * generate an independent sub-Query tree.  Otherwise, it can be
	 * represented by a SetOperationStmt node underneath the parent Query.
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
		RangeTblEntry *rte PG_USED_FOR_ASSERTS_ONLY;
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
		if (pstate->p_namespace)
		{
			if (contain_vars_of_level((Node *) selectQuery, 1))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
						 errmsg("UNION/INTERSECT/EXCEPT member statement cannot refer to other relations of same query level"),
						 parser_errposition(pstate,
							 locate_var_of_level((Node *) selectQuery, 1))));
		}

		/*
		 * Extract a list of the non-junk TLEs for upper-level processing.
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

			/*
			 * It's possible that this leaf query has a different number
			 * of columns than the previous ones. That's an error, but
			 * we don't throw it here because we don't have the context
			 * needed for a good error message. We don't know which
			 * operation of the setop tree is the one where the number
			 * of columns between the left and right branches differ.
			 * Therefore, just return here as if nothing happened, and
			 * we'll catch that error in the parent instead.
			 */
			if (numCols == setop_types->ncols)
			{
				i = 0;
				foreach(tl, selectQuery->targetList)
				{
					TargetEntry *tle = (TargetEntry *) lfirst(tl);

					if (tle->resjunk)
						continue;

					setop_types->leafinfos[i] = lappend(setop_types->leafinfos[i],
														(Node *) tle->expr);
					i++;
				}
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
											false,
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
		 * If we are processing a recursive union query, now is the time to
		 * examine the non-recursive term's output columns and mark the
		 * containing CTE as having those result columns.  We should do this
		 * only at the topmost setop of the CTE, of course.
		 *
		 * In PostgreSQL, transformSetOperationTree() runs as a single pass,
		 * and we coerce the column types as we go. In GPDB, it's a two-pass
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
			List *ltargetlist;
			List *selected_types;
			List *selected_typmods;

			select_setop_types(pstate, setop_types, stmt->op, &selected_types, &selected_typmods);

			coerceSetOpTypes(pstate, op->larg, selected_types, selected_typmods, &ltargetlist);

			determineRecursiveColTypes(pstate, op->larg, ltargetlist);
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
				 List **targetlist)
{
	if (IsA(sop, RangeTblRef))
	{
		RangeTblEntry *rte = rt_fetch((((RangeTblRef *) sop)->rtindex), pstate->p_rtable);
		Query	   *selectQuery = rte->subquery;
		ListCell   *tl;

		/*
		 * Extract a list of the non-junk TLEs for upper-level processing.
		 * This is the same we did in the first pass, in
		 * transformSetOperationTree_internal().
		 */
		if (targetlist)
		{
			*targetlist = NIL;
			foreach(tl, selectQuery->targetList)
			{
				TargetEntry *tle = (TargetEntry *) lfirst(tl);

				if (!tle->resjunk)
					*targetlist = lappend(*targetlist, tle);
			}
		}
		return;
	}
	else
	{
		SetOperationStmt *op = (SetOperationStmt *) sop;
		List	   *ltargetlist;
		List	   *rtargetlist;
		ListCell   *ltl;
		ListCell   *rtl;
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
						 &ltargetlist);

		coerceSetOpTypes(pstate, op->rarg,
						 preselected_coltypes, preselected_coltypmods,
						 &rtargetlist);

		/*
		 * Verify that the two children have the same number of non-junk
		 * columns, and determine the types of the merged output columns.
		 */
		if (list_length(ltargetlist) != list_length(rtargetlist))
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("each %s query must have the same number of columns",
						context),
					 parser_errposition(pstate,
										exprLocation((Node *) rtargetlist))));

		Assert(list_length(preselected_coltypes) == list_length(preselected_coltypmods));

		if (targetlist)
			*targetlist = NIL;
		op->colTypes = NIL;
		op->colTypmods = NIL;
		op->colCollations = NIL;
		/* don't have a "foreach5", so chase three of the lists by hand */
		pct = list_head(preselected_coltypes);
		pcm = list_head(preselected_coltypmods);
		forboth(ltl, ltargetlist, rtl, rtargetlist)
		{
			TargetEntry *ltle = (TargetEntry *) lfirst(ltl);
			TargetEntry *rtle = (TargetEntry *) lfirst(rtl);
			Node	   *lcolnode = (Node *) ltle->expr;
			Node	   *rcolnode = (Node *) rtle->expr;
			Oid			lcoltype = exprType(lcolnode);
			Oid			rcoltype = exprType(rcolnode);
			int32		lcoltypmod = exprTypmod(lcolnode);
			int32		rcoltypmod = exprTypmod(rcolnode);
			Node       *bestexpr = NULL;
			int			bestlocation;
			Oid			rescoltype = pct ? lfirst_oid(pct) : InvalidOid;
			int32		rescoltypmod = pcm ? lfirst_int(pcm) : -1;
			Oid			rescolcoll;

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
				bestlocation = exprLocation(bestexpr);
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
				bestlocation = exprLocation(lcolnode);
			}

			/*
			 * Verify the coercions are actually possible.  If not, we'd fail
			 * later anyway, but we want to fail now while we have sufficient
			 * context to produce an error cursor position.
			 *
			 * For all non-UNKNOWN-type cases, we verify coercibility but we
			 * don't modify the child's expression, for fear of changing the
			 * child query's semantics.
			 *
			 * If a child expression is an UNKNOWN-type Const or Param, we
			 * want to replace it with the coerced expression.  This can only
			 * happen when the child is a leaf set-op node.  It's safe to
			 * replace the expression because if the child query's semantics
			 * depended on the type of this output column, it'd have already
			 * coerced the UNKNOWN to something else.  We want to do this
			 * because (a) we want to verify that a Const is valid for the
			 * target type, or resolve the actual type of an UNKNOWN Param,
			 * and (b) we want to avoid unnecessary discrepancies between the
			 * output type of the child query and the resolved target type.
			 * Such a discrepancy would disable optimization in the planner.
			 *
			 * If it's some other UNKNOWN-type node, eg a Var, we do nothing
			 * (knowing that coerce_to_common_type would fail).  The planner
			 * is sometimes able to fold an UNKNOWN Var to a constant before
			 * it has to coerce the type, so failing now would just break
			 * cases that might work.
			 */
			if (lcoltype != UNKNOWNOID)
				lcolnode = coerce_to_common_type(pstate, lcolnode,
												 rescoltype, context);
			else if (IsA(lcolnode, Const) ||
					 IsA(lcolnode, Param))
			{
				lcolnode = coerce_to_common_type(pstate, lcolnode,
												 rescoltype, context);
				ltle->expr = (Expr *) lcolnode;
			}

			if (rcoltype != UNKNOWNOID)
				rcolnode = coerce_to_common_type(pstate, rcolnode,
												 rescoltype, context);
			else if (IsA(rcolnode, Const) ||
					 IsA(rcolnode, Param))
			{
				rcolnode = coerce_to_common_type(pstate, rcolnode,
												 rescoltype, context);
				rtle->expr = (Expr *) rcolnode;
			}

			/*
			 * Select common collation.  A common collation is required for
			 * all set operators except UNION ALL; see SQL:2008 7.13 <query
			 * expression> Syntax Rule 15c.  (If we fail to identify a common
			 * collation for a UNION ALL column, the curCollations element
			 * will be set to InvalidOid, which may result in a runtime error
			 * if something at a higher query level wants to use the column's
			 * collation.)
			 */
			rescolcoll = select_common_collation(pstate,
											  list_make2(lcolnode, rcolnode),
										 (op->op == SETOP_UNION && op->all));

			/* emit results */
			op->colTypes = lappend_oid(op->colTypes, rescoltype);
			op->colTypmods = lappend_int(op->colTypmods, rescoltypmod);
			op->colCollations = lappend_oid(op->colCollations, rescolcoll);

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
				bool		hashable;
				ParseCallbackState pcbstate;

				setup_parser_errposition_callback(&pcbstate, pstate,
												  bestlocation);

				/* determine the eqop and optional sortop */
				get_sort_group_operators(rescoltype,
										 false, true, false,
										 &sortop, &eqop, NULL,
										 &hashable);

				cancel_parser_errposition_callback(&pcbstate);

				/* we don't have a tlist yet, so can't assign sortgrouprefs */
				grpcl->tleSortGroupRef = 0;
				grpcl->eqop = eqop;
				grpcl->sortop = sortop;
				grpcl->nulls_first = false;		/* OK with or without sortop */
				grpcl->hashable = hashable;

				op->groupClauses = lappend(op->groupClauses, grpcl);
			}

			/*
			 * Construct a dummy tlist entry to return.  We use a SetToDefault
			 * node for the expression, since it carries exactly the fields
			 * needed, but any other expression node type would do as well.
			 */
			if (targetlist)
			{
				SetToDefault *rescolnode = makeNode(SetToDefault);
				TargetEntry *restle;

				rescolnode->typeId = rescoltype;
				rescolnode->typeMod = rescoltypmod;
				rescolnode->collation = rescolcoll;
				rescolnode->location = bestlocation;
				restle = makeTargetEntry((Expr *) rescolnode,
										 0,		/* no need to set resno */
										 NULL,
										 false);
				*targetlist = lappend(*targetlist, restle);
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
determineRecursiveColTypes(ParseState *pstate, Node *larg, List *nrtargetlist)
{
	Node	   *node;
	int			leftmostRTI;
	Query	   *leftmostQuery;
	List	   *targetList;
	ListCell   *left_tlist;
	ListCell   *nrtl;
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
	 * Generate dummy targetlist using column names of leftmost select and
	 * dummy result expressions of the non-recursive term.
	 */
	targetList = NIL;
	left_tlist = list_head(leftmostQuery->targetList);
	next_resno = 1;

	foreach(nrtl, nrtargetlist)
	{
		TargetEntry *nrtle = (TargetEntry *) lfirst(nrtl);
		TargetEntry *lefttle = (TargetEntry *) lfirst(left_tlist);
		char	   *colName;
		TargetEntry *tle;

		Assert(!lefttle->resjunk);
		colName = pstrdup(lefttle->resname);
		tle = makeTargetEntry(nrtle->expr,
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
 * transformUpdateStmt -
 *	  transforms an update statement
 */
static Query *
transformUpdateStmt(ParseState *pstate, UpdateStmt *stmt)
{
	Query	   *qry = makeNode(Query);
	ParseNamespaceItem *nsitem;
	RangeTblEntry *target_rte;
	Node	   *qual;
	ListCell   *origTargetList;
	ListCell   *tl;

	qry->commandType = CMD_UPDATE;
	pstate->p_is_update = true;

	/* process the WITH clause independently of all else */
	if (stmt->withClause)
	{
		qry->hasRecursive = stmt->withClause->recursive;
		qry->cteList = transformWithClause(pstate, stmt->withClause);
		qry->hasModifyingCTE = pstate->p_hasModifyingCTE;

		/*
		 * Since GPDB currently only support a single writer gang, only one
		 * writable clause is permitted per CTE. Once we get flexible gangs
		 * with more than one writer gang we can lift this restriction.
		 */
		if (pstate->p_hasModifyingCTE)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("writable CTE queries cannot be themselves writable"),
					 errdetail("Greenplum Database currently only support CTEs with one writable clause, called in a non-writable context."),
					 errhint("Rewrite the query to only include one writable clause.")));
	}

	qry->resultRelation = setTargetTable(pstate, stmt->relation,
								  interpretInhOption(stmt->relation->inhOpt),
										 true,
										 ACL_UPDATE);

	/* grab the namespace item made by setTargetTable */
	nsitem = (ParseNamespaceItem *) llast(pstate->p_namespace);

	/* subqueries in FROM cannot access the result relation */
	nsitem->p_lateral_only = true;
	nsitem->p_lateral_ok = false;

	/*
	 * the FROM clause is non-standard SQL syntax. We used to be able to do
	 * this with REPLACE in POSTQUEL so we keep the feature.
	 */
	transformFromClause(pstate, stmt->fromClause);

	/* remaining clauses can reference the result relation normally */
	nsitem->p_lateral_only = false;
	nsitem->p_lateral_ok = true;

	qry->targetList = transformTargetList(pstate, stmt->targetList,
										  EXPR_KIND_UPDATE_SOURCE);

	qual = transformWhereClause(pstate, stmt->whereClause,
								EXPR_KIND_WHERE, "WHERE");

	qry->returningList = transformReturningList(pstate, stmt->returningList);

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

	assign_query_collations(pstate, qry);

	return qry;
}

/*
 * transformReturningList -
 *	handle a RETURNING clause in INSERT/UPDATE/DELETE
 */
static List *
transformReturningList(ParseState *pstate, List *returningList)
{
	List	   *rlist;
	int			save_next_resno;

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

	/*
	 * Complain if the nonempty tlist expanded to nothing (which is possible
	 * if it contains only a star-expansion of a zero-column table).  If we
	 * allow this, the parsed Query will look like it didn't have RETURNING,
	 * with results that would probably surprise the user.
	 */
	if (rlist == NIL)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("RETURNING must have at least one column"),
				 parser_errposition(pstate,
									exprLocation(linitial(returningList)))));

	/* mark column origins */
	markTargetListOrigins(pstate, rlist);

	/* restore state */
	pstate->p_next_resno = save_next_resno;

	return rlist;
}


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

	/*
	 * We also disallow data-modifying WITH in a cursor.  (This could be
	 * allowed, but the semantics of when the updates occur might be
	 * surprising.)
	 */
	if (result->hasModifyingCTE)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("DECLARE CURSOR must not contain data-modifying statements in WITH")));

	/* FOR UPDATE and WITH HOLD are not compatible */
	if (result->rowMarks != NIL && (stmt->options & CURSOR_OPT_HOLD))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		/*------
		  translator: %s is a SQL row locking clause such as FOR UPDATE */
				 errmsg("DECLARE CURSOR WITH HOLD ... %s is not supported",
						LCS_asString(((RowMarkClause *)
									  linitial(result->rowMarks))->strength)),
				 errdetail("Holdable cursors must be READ ONLY.")));

	/* FOR UPDATE and SCROLL are not compatible */
	if (result->rowMarks != NIL && (stmt->options & CURSOR_OPT_SCROLL))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		/*------
		  translator: %s is a SQL row locking clause such as FOR UPDATE */
				 errmsg("DECLARE SCROLL CURSOR ... %s is not supported",
						LCS_asString(((RowMarkClause *)
									  linitial(result->rowMarks))->strength)),
				 errdetail("Scrollable cursors must be READ ONLY.")));

	/* FOR UPDATE and INSENSITIVE are not compatible */
	if (result->rowMarks != NIL && (stmt->options & CURSOR_OPT_INSENSITIVE))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		/*------
		  translator: %s is a SQL row locking clause such as FOR UPDATE */
				 errmsg("DECLARE INSENSITIVE CURSOR ... %s is not supported",
						LCS_asString(((RowMarkClause *)
									  linitial(result->rowMarks))->strength)),
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
 * EXPLAIN is like other utility statements in that we emit it as a
 * CMD_UTILITY Query node; however, we must first transform the contained
 * query.  We used to postpone that until execution, but it's really necessary
 * to do it during the normal parse analysis phase to ensure that side effects
 * of parser hooks happen at the expected time.
 */
static Query *
transformExplainStmt(ParseState *pstate, ExplainStmt *stmt)
{
	Query	   *result;

	/* transform contained query, allowing SELECT INTO */
	stmt->query = (Node *) transformTopLevelStmt(pstate, stmt->query);

	/* represent the command as a utility Query */
	result = makeNode(Query);
	result->commandType = CMD_UTILITY;
	result->utilityStmt = (Node *) stmt;

	return result;
}


/*
 * transformCreateTableAsStmt -
 *	transform a CREATE TABLE AS, SELECT ... INTO, or CREATE MATERIALIZED VIEW
 *	Statement
 *
 * As with EXPLAIN, transform the contained statement now.
 */
static Query *
transformCreateTableAsStmt(ParseState *pstate, CreateTableAsStmt *stmt)
{
	Query	   *result;
	Query	   *query;

	/* transform contained query */
	query = transformStmt(pstate, stmt->query);
	stmt->query = (Node *) query;

	/* additional work needed for CREATE MATERIALIZED VIEW */
	if (stmt->relkind == OBJECT_MATVIEW)
	{
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("materialized view is not supported on greenplum")));

		/*
		 * Prohibit a data-modifying CTE in the query used to create a
		 * materialized view. It's not sufficiently clear what the user would
		 * want to happen if the MV is refreshed or incrementally maintained.
		 */
		if (query->hasModifyingCTE)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("materialized views must not use data-modifying statements in WITH")));

		/*
		 * Check whether any temporary database objects are used in the
		 * creation query. It would be hard to refresh data or incrementally
		 * maintain it if a source disappeared.
		 */
		if (isQueryUsingTempRelation(query))
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("materialized views must not use temporary tables or views")));

		/*
		 * A materialized view would either need to save parameters for use in
		 * maintaining/loading the data or prohibit them entirely.  The latter
		 * seems safer and more sane.
		 */
		if (query_contains_extern_params(query))
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("materialized views may not be defined using bound parameters")));

		/*
		 * For now, we disallow unlogged materialized views, because it seems
		 * like a bad idea for them to just go to empty after a crash. (If we
		 * could mark them as unpopulated, that would be better, but that
		 * requires catalog changes which crash recovery can't presently
		 * handle.)
		 */
		if (stmt->into->rel->relpersistence == RELPERSISTENCE_UNLOGGED)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("materialized views cannot be UNLOGGED")));

		/*
		 * At runtime, we'll need a copy of the parsed-but-not-rewritten Query
		 * for purposes of creating the view's ON SELECT rule.  We stash that
		 * in the IntoClause because that's where intorel_startup() can
		 * conveniently get it from.
		 */
		stmt->into->viewQuery = copyObject(query);
	}

	/* represent the command as a utility Query */
	result = makeNode(Query);
	result->commandType = CMD_UTILITY;
	result->utilityStmt = (Node *) stmt;

	/* GPDB: Set parentStmtType to PARENTSTMTTYPE_CTAS as we know this query is for CTAS */
	((Query*)stmt->query)->parentStmtType = PARENTSTMTTYPE_CTAS;

	if (stmt->into->distributedBy && Gp_role == GP_ROLE_DISPATCH)
		setQryDistributionPolicy(stmt->into, (Query *)stmt->query);

	return result;
}


char *
LCS_asString(LockClauseStrength strength)
{
	switch (strength)
	{
		case LCS_FORKEYSHARE:
			return "FOR KEY SHARE";
		case LCS_FORSHARE:
			return "FOR SHARE";
		case LCS_FORNOKEYUPDATE:
			return "FOR NO KEY UPDATE";
		case LCS_FORUPDATE:
			return "FOR UPDATE";
	}
	return "FOR some";			/* shouldn't happen */
}

/*
 * Check for features that are not supported with FOR [KEY] UPDATE/SHARE.
 *
 * exported so planner can check again after rewriting, query pullup, etc
 */
void
CheckSelectLocking(Query *qry, LockClauseStrength strength)
{
	if (qry->setOperations)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		/*------
		  translator: %s is a SQL row locking clause such as FOR UPDATE */
				 errmsg("%s is not allowed with UNION/INTERSECT/EXCEPT",
						LCS_asString(strength))));
	if (qry->distinctClause != NIL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		/*------
		  translator: %s is a SQL row locking clause such as FOR UPDATE */
				 errmsg("%s is not allowed with DISTINCT clause",
						LCS_asString(strength))));
	if (qry->groupClause != NIL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		/*------
		  translator: %s is a SQL row locking clause such as FOR UPDATE */
				 errmsg("%s is not allowed with GROUP BY clause",
						LCS_asString(strength))));
	if (qry->havingQual != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		/*------
		  translator: %s is a SQL row locking clause such as FOR UPDATE */
				 errmsg("%s is not allowed with HAVING clause",
						LCS_asString(strength))));
	if (qry->hasAggs)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		/*------
		  translator: %s is a SQL row locking clause such as FOR UPDATE */
				 errmsg("%s is not allowed with aggregate functions",
						LCS_asString(strength))));
	if (qry->hasWindowFuncs)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		/*------
		  translator: %s is a SQL row locking clause such as FOR UPDATE */
				 errmsg("%s is not allowed with window functions",
						LCS_asString(strength))));
	if (expression_returns_set((Node *) qry->targetList))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		/*------
		  translator: %s is a SQL row locking clause such as FOR UPDATE */
				 errmsg("%s is not allowed with set-returning functions in the target list",
						LCS_asString(strength))));
}

/*
 * Transform a FOR [KEY] UPDATE/SHARE clause
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

	CheckSelectLocking(qry, lc->strength);

	/* make a clause we can pass down to subqueries to select all rels */
	allrels = makeNode(LockingClause);
	allrels->lockedRels = NIL;	/* indicates all rels */
	allrels->strength = lc->strength;
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
					if (get_rel_relstorage(rte->relid) == RELSTORAGE_EXTERNAL)
						ereport(ERROR,
								(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
								 errmsg("SELECT FOR UPDATE/SHARE cannot be applied to external tables")));

					applyLockingClause(qry, i,
									   lc->strength, lc->noWait, pushedDown);
					rte->requiredPerms |= ACL_SELECT_FOR_UPDATE;
					break;
				case RTE_SUBQUERY:
					applyLockingClause(qry, i,
									   lc->strength, lc->noWait, pushedDown);

					/*
					 * FOR UPDATE/SHARE of subquery is propagated to all of
					 * subquery's rels, too.  We could do this later (based on
					 * the marking of the subquery RTE) but it is convenient
					 * to have local knowledge in each query level about which
					 * rels need to be opened with RowShareLock.
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
				/*------
				  translator: %s is a SQL row locking clause such as FOR UPDATE */
						 errmsg("%s must specify unqualified relation names",
								LCS_asString(lc->strength)),
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
							if (get_rel_relstorage(rte->relid) == RELSTORAGE_EXTERNAL)
								ereport(ERROR,
										(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
										 errmsg("SELECT FOR UPDATE/SHARE cannot be applied to external tables")));
							applyLockingClause(qry, i,
											   lc->strength, lc->noWait,
											   pushedDown);
							rte->requiredPerms |= ACL_SELECT_FOR_UPDATE;
							break;
						case RTE_SUBQUERY:
							applyLockingClause(qry, i,
											   lc->strength, lc->noWait,
											   pushedDown);
							/* see comment above */
							transformLockingClause(pstate, rte->subquery,
												   allrels, true);
							break;
						case RTE_JOIN:
							ereport(ERROR,
									(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							/*------
							  translator: %s is a SQL row locking clause such as FOR UPDATE */
									 errmsg("%s cannot be applied to a join",
											LCS_asString(lc->strength)),
							 parser_errposition(pstate, thisrel->location)));
							break;
						case RTE_FUNCTION:
							ereport(ERROR,
									(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							/*------
							  translator: %s is a SQL row locking clause such as FOR UPDATE */
								 errmsg("%s cannot be applied to a function",
										LCS_asString(lc->strength)),
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
							/*------
							  translator: %s is a SQL row locking clause such as FOR UPDATE */
									 errmsg("%s cannot be applied to VALUES",
											LCS_asString(lc->strength)),
							 parser_errposition(pstate, thisrel->location)));
							break;
						case RTE_CTE:
							ereport(ERROR,
									(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							/*------
							  translator: %s is a SQL row locking clause such as FOR UPDATE */
							   errmsg("%s cannot be applied to a WITH query",
									  LCS_asString(lc->strength)),
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
				/*------
				  translator: %s is a SQL row locking clause such as FOR UPDATE */
						 errmsg("relation \"%s\" in %s clause not found in FROM clause",
								thisrel->relname,
								LCS_asString(lc->strength)),
						 parser_errposition(pstate, thisrel->location)));
		}
	}
}

/*
 * Record locking info for a single rangetable item
 */
void
applyLockingClause(Query *qry, Index rtindex,
				   LockClauseStrength strength, bool noWait, bool pushedDown)
{
	RowMarkClause *rc;

	/* If it's an explicit clause, make sure hasForUpdate gets set */
	if (!pushedDown)
		qry->hasForUpdate = true;

	/* Check for pre-existing entry for same rtindex */
	if ((rc = get_parse_rowmark(qry, rtindex)) != NULL)
	{
		/*
		 * If the same RTE is specified for more than one locking strength,
		 * treat is as the strongest.  (Reasonable, since you can't take both
		 * a shared and exclusive lock at the same time; it'll end up being
		 * exclusive anyway.)
		 *
		 * We also consider that NOWAIT wins if it's specified both ways. This
		 * is a bit more debatable but raising an error doesn't seem helpful.
		 * (Consider for instance SELECT FOR UPDATE NOWAIT from a view that
		 * internally contains a plain FOR UPDATE spec.)
		 *
		 * And of course pushedDown becomes false if any clause is explicit.
		 */
		rc->strength = Max(rc->strength, strength);
		rc->noWait |= noWait;
		rc->pushedDown &= pushedDown;
		return;
	}

	/* Make a new RowMarkClause */
	rc = makeNode(RowMarkClause);
	rc->rti = rtindex;
	rc->strength = strength;
	rc->noWait = noWait;
	rc->pushedDown = pushedDown;
	qry->rowMarks = lappend(qry->rowMarks, rc);
}

/*
 * Get distribute key by name.
 *
 * Find the distribute key in into->colNames if it is not NULL, otherwise
 * search qry->targetList.
 */
static int
get_distkey_by_name(char *key, IntoClause *into, Query *qry, bool *found)
{
	ListCell   *lc;
	if (into->colNames)
	{
		int colindex = 1;
		foreach(lc, into->colNames)
		{
			if (strcmp(strVal(lfirst(lc)), key) == 0)
			{
				*found = true;
				return colindex;
			}

			colindex++;
		}
	}
	else
	{
		foreach(lc, qry->targetList)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(lc);

			if (tle->resname && strcmp(tle->resname, key) == 0)
			{
				*found = true;
				return tle->resno;
			}
		}
	}

	*found = false;
	return 0;
}

/*
 * Set Query->intoPolicy based on the DISTRIBUTED BY clause, in a
 * CREATE TABLE AS statement.
 *
 * This performs some of the same checks and processing that
 * transformDistributedBy() does for a regular CREATE TABLE. There are some
 * differences, however:
 *
 * 1. We form a GpPolicy to represent the DISTRIBUTED BY clause. In a regular
 * CREATE TABLE, we must delay doing that until DefineRelation, after we have
 * merged inherited columns into the table definition, but with CREATE TABLE
 * AS, it's OK, because there is no inheritance.
 *
 * 2. If no DISTRIBUTED BY was given explicitly, we don't try to deduce a
 * default here. We delay that into the planner because we'll have more
 * information available at that point (see apply_motion()).
 */
static void
setQryDistributionPolicy(IntoClause *into, Query *qry)
{
	ListCell   *lc;
	DistributedBy *dist;

	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(into != NULL);
	Assert(into->distributedBy != NULL);

	dist = (DistributedBy *)into->distributedBy;

	if (dist->numsegments < 0)
		dist->numsegments = GP_POLICY_DEFAULT_NUMSEGMENTS();

	/*
	 * We have a DISTRIBUTED BY column list specified by the user
	 * Process it now and set the distribution policy.
	 */
	if (list_length(dist->keyCols) > MaxPolicyAttributeNumber)
		ereport(ERROR,
				(errcode(ERRCODE_TOO_MANY_COLUMNS),
				 errmsg("number of distributed by columns exceeds limit (%d)",
						MaxPolicyAttributeNumber)));

	if (dist->ptype == POLICYTYPE_REPLICATED)
		qry->intoPolicy = createReplicatedGpPolicy(dist->numsegments);
	else
	{
		List	*policykeys = NIL;
		List	*policyopclasses = NIL;

		foreach(lc, dist->keyCols)
		{
			IndexElem  *ielem = (IndexElem *) lfirst(lc);
			bool		found = false;
			int			keyindex;
			Oid			keytype;
			Oid			keyopclass;
			TargetEntry *tle;

			keyindex = get_distkey_by_name(ielem->name, into, qry, &found);
			if (!found)
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_COLUMN),
						 errmsg("column \"%s\" named in DISTRIBUTED BY "
								"clause does not exist",
								ielem->name)));

			tle = list_nth(qry->targetList, keyindex - 1);

			keytype = exprType((Node *) tle->expr);
			keyopclass = GetIndexOpClass(ielem->opclass, keytype, "hash", HASH_AM_OID);

			policykeys = lappend_int(policykeys, keyindex);
			policyopclasses = lappend_oid(policyopclasses, keyopclass);
		}

		qry->intoPolicy = createHashPartitionedPolicy(policykeys,
													  policyopclasses,
													  dist->numsegments);
	}
}
