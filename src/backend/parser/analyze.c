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
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *	$PostgreSQL: pgsql/src/backend/parser/analyze.c,v 1.360 2007/02/01 19:10:27 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/heapam.h"
#include "access/reloptions.h"
#include "catalog/catquery.h"
#include "catalog/gp_policy.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/pg_compression.h"
#include "catalog/pg_type.h"
#include "catalog/pg_type_encoding.h"
#include "cdb/cdbpartition.h"
#include "commands/defrem.h"
#include "commands/prepare.h"
#include "commands/tablecmds.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/plancat.h"
#include "optimizer/tlist.h"
#include "optimizer/var.h"
#include "parser/analyze.h"
#include "parser/gramparse.h"
#include "parser/parse_agg.h"
#include "parser/parse_clause.h"
#include "parser/parse_coerce.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "parser/parse_partition.h"
#include "parser/parse_relation.h"
#include "parser/parse_target.h"
#include "parser/parse_type.h"
#include "parser/parse_utilcmd.h"
#include "parser/parse_cte.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

#include "cdb/cdbvars.h"
#include "cdb/cdbhash.h"
#include "cdb/cdbsreh.h"


#define MaxPolicyAttributeNumber MaxHeapAttributeNumber

/* State shared by transformCreateSchemaStmt and its subroutines */
typedef struct
{
	const char *stmtType;		/* "CREATE SCHEMA" or "ALTER SCHEMA" */
	char	   *schemaname;		/* name of schema */
	char	   *authid;			/* owner of schema */
	List	   *sequences;		/* CREATE SEQUENCE items */
	List	   *tables;			/* CREATE TABLE items */
	List	   *views;			/* CREATE VIEW items */
	List	   *indexes;		/* CREATE INDEX items */
	List	   *triggers;		/* CREATE TRIGGER items */
	List	   *grants;			/* GRANT items */
	List	   *fwconstraints;	/* Forward referencing FOREIGN KEY constraints */
	List	   *alters;			/* Generated ALTER items (from the above) */
	List	   *ixconstraints;	/* index-creating constraints */
	List	   *blist;			/* "before list" of things to do before
								 * creating the schema */
	List	   *alist;			/* "after list" of things to do after creating
								 * the schema */
} CreateSchemaStmtContext;

typedef struct
{
	Oid		   *paramTypes;
	int			numParams;
} check_parameter_resolution_context;

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

static List *do_parse_analyze(Node *parseTree, ParseState *pstate);
static void parse_analyze_error_callback(void *parsestate);     /*CDB*/
static Query *transformStmt(ParseState *pstate, Node *stmt,
			  List **extras_before, List **extras_after);
static Query *transformViewStmt(ParseState *pstate, ViewStmt *stmt,
				  List **extras_before, List **extras_after);
static Query *transformDeleteStmt(ParseState *pstate, DeleteStmt *stmt);
static Query *transformInsertStmt(ParseState *pstate, InsertStmt *stmt,
					List **extras_before, List **extras_after);
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

static Query *transformIndexStmt(ParseState *pstate, IndexStmt *stmt,
								 List **extras_before, List **extras_after);
static Query *transformRuleStmt(ParseState *query, RuleStmt *stmt,
				  List **extras_before, List **extras_after);
static Query *transformSelectStmt(ParseState *pstate, SelectStmt *stmt);
static Query *transformValuesClause(ParseState *pstate, SelectStmt *stmt);
static Query *transformSetOperationStmt(ParseState *pstate, SelectStmt *stmt);
static Node *transformSetOperationTree(ParseState *pstate, SelectStmt *stmt);
static Query *transformUpdateStmt(ParseState *pstate, UpdateStmt *stmt);
static Query *transformDeclareCursorStmt(ParseState *pstate,
						   DeclareCursorStmt *stmt);
static bool isSimplyUpdatableQuery(Query *query);
static Query *transformPrepareStmt(ParseState *pstate, PrepareStmt *stmt);
static Query *transformExecuteStmt(ParseState *pstate, ExecuteStmt *stmt);
static Query *transformCreateExternalStmt(ParseState *pstate, CreateExternalStmt *stmt,
										  List **extras_before, List **extras_after);
static Query *transformAlterTableStmt(ParseState *pstate, AlterTableStmt *stmt,
						List **extras_before, List **extras_after);
static void transformColumnDefinition(ParseState *pstate,
						  CreateStmtContext *cxt,
						  ColumnDef *column);
static void transformTableConstraint(ParseState *pstate,
						 CreateStmtContext *cxt,
						 Constraint *constraint);
static void transformDistributedBy(ParseState *pstate,
								   CreateStmtContext *cxt,
								   List *distributedBy,
								   GpPolicy ** policy, List *likeDistributedBy,
								   bool bQuiet);

static void transformFKConstraints(ParseState *pstate,
					   CreateStmtContext *cxt,
					   bool skipValidation,
					   bool isAddConstraint);
static void applyColumnNames(List *dst, List *src);
static bool isSetopLeaf(SelectStmt *stmt);
static int collectSetopTypes(ParseState *pstate, SelectStmt *stmt,
							 List **types, List **typmods);
static void getSetColTypes(ParseState *pstate, Node *node,
			   List **colTypes, List **colTypmods);
static void transformLockingClause(Query *qry, LockingClause *lc);
static void transformConstraintAttrs(List *constraintList);
static void transformColumnType(ParseState *pstate, ColumnDef *column);
static void release_pstate_resources(ParseState *pstate);
static FromExpr *makeFromExpr(List *fromlist, Node *quals);
static bool check_parameter_resolution_walker(Node *node,
								check_parameter_resolution_context *context);

static void setQryDistributionPolicy(SelectStmt *stmt, Query *qry);
static List *getLikeDistributionPolicy(InhRelation *e);

static Query *transformGroupedWindows(Query *qry);
static void init_grouped_window_context(grouped_window_ctx *ctx, Query *qry);
static Var *var_for_gw_expr(grouped_window_ctx *ctx, Node *expr, bool force);
static void discard_grouped_window_context(grouped_window_ctx *ctx);
static Node *map_sgr_mutator(Node *node, void *context);
static Node *grouped_window_mutator(Node *node, void *context);
static Alias *make_replacement_alias(Query *qry, const char *aname);
static char *generate_positional_name(AttrNumber attrno);
static List*generate_alternate_vars(Var *var, grouped_window_ctx *ctx);

static List *fillin_encoding(List *list);

static List *transformAttributeEncoding(List *stenc, CreateStmt *stmt,
										CreateStmtContext *cxt);
/*
 * parse_analyze
 *		Analyze a raw parse tree and transform it to Query form.
 *
 * If available, pass the source text from which the raw parse tree was
 * generated; it's OK to pass NULL if this is not available.
 *
 * Optionally, information about $n parameter types can be supplied.
 * References to $n indexes not defined by paramTypes[] are disallowed.
 *
 * The result is a List of Query nodes (we need a list since some commands
 * produce multiple Queries).  Optimizable statements require considerable
 * transformation, while many utility-type statements are simply hung off
 * a dummy CMD_UTILITY Query node.
 */
List *
parse_analyze(Node *parseTree, const char *sourceText,
			  Oid *paramTypes, int numParams)
{
	ParseState *pstate = make_parsestate(NULL);
	List	   *result;

	pstate->p_sourcetext = sourceText;
	pstate->p_paramtypes = paramTypes;
	pstate->p_numparams = numParams;
	pstate->p_variableparams = false;

	result = do_parse_analyze(parseTree, pstate);

	free_parsestate(pstate);

	return result;
}

/*
 * parse_analyze_varparams
 *
 * This variant is used when it's okay to deduce information about $n
 * symbol datatypes from context.  The passed-in paramTypes[] array can
 * be modified or enlarged (via repalloc).
 */
List *
parse_analyze_varparams(Node *parseTree, const char *sourceText,
						Oid **paramTypes, int *numParams)
{
	ParseState *pstate = make_parsestate(NULL);
	List	   *result;

	pstate->p_sourcetext = sourceText;
	pstate->p_paramtypes = *paramTypes;
	pstate->p_numparams = *numParams;
	pstate->p_variableparams = true;

	result = do_parse_analyze(parseTree, pstate);

	*paramTypes = pstate->p_paramtypes;
	*numParams = pstate->p_numparams;

	free_parsestate(pstate);

	/* make sure all is well with parameter types */
	if (*numParams > 0)
	{
		check_parameter_resolution_context context;

		context.paramTypes = *paramTypes;
		context.numParams = *numParams;
		check_parameter_resolution_walker((Node *) result, &context);
	}

	return result;
}

/*
 * parse_sub_analyze
 *		Entry point for recursively analyzing a sub-statement.
 */
List *
parse_sub_analyze(Node *parseTree, ParseState *parentParseState)
{
	ParseState *pstate = make_parsestate(parentParseState);
	List	   *result;

	result = do_parse_analyze(parseTree, pstate);

	free_parsestate(pstate);

	return result;
}

static int
alter_cmp(const void *a, const void *b)
{
	Query *qa = *(Query **)a;
	Query *qb = *(Query **)b;
	AlterTableStmt *stmta = (AlterTableStmt *)qa->utilityStmt;
	AlterTableStmt *stmtb = (AlterTableStmt *)qb->utilityStmt;
	PartitionBy *pbya = NULL;
	PartitionBy *pbyb = NULL;
	int len1, len2;
	ListCell *lc;

	Assert(IsA(stmta, AlterTableStmt));
	Assert(IsA(stmtb, AlterTableStmt));

	foreach(lc, stmta->cmds)
	{
		AlterTableCmd *cmd = lfirst(lc);

		if (cmd->subtype == AT_PartAddInternal)
		{
			pbya = (PartitionBy *)cmd->def;
			break;
		}
	}

	foreach(lc, stmtb->cmds)
	{
		AlterTableCmd *cmd = lfirst(lc);

		if (cmd->subtype == AT_PartAddInternal)
		{
			pbyb = (PartitionBy *)cmd->def;
			break;
		}
	}

	if (pbya && pbyb)
	{
		if (pbya->partDepth < pbyb->partDepth)
			return -1;
		else if (pbya->partDepth > pbyb->partDepth)
			return 1;
		else
			return 0;
	}

	len1 = strlen(stmta->relation->relname);
	len2 = strlen(stmtb->relation->relname);

	if (len1 < len2)
		return -1;
	else if (len1 > len2)
		return 1;
	else
		/* same size */
		return strcmp(stmta->relation->relname, stmtb->relation->relname);
}

/*
 * do_parse_analyze
 *		Workhorse code shared by the above variants of parse_analyze.
 */
static List *
do_parse_analyze(Node *parseTree, ParseState *pstate)
{
	List	   *result = NIL;

	/* Lists to return extra commands from transformation */
	List	   *extras_before = NIL;
	List	   *extras_after = NIL;
	List	   *tmp = NIL;
	Query	   *query;
	ListCell   *l;

   	ErrorContextCallback errcontext;

	/* CDB: Request a callback in case ereport or elog is called. */
	errcontext.callback = parse_analyze_error_callback;
	errcontext.arg = pstate;
	errcontext.previous = error_context_stack;
	error_context_stack = &errcontext;

	query = transformStmt(pstate, parseTree, &extras_before, &extras_after);

	/* CDB: Pop error context callback stack. */
	error_context_stack = errcontext.previous;

	/* CDB: All breadcrumbs should have been popped. */
	Assert(!pstate->p_breadcrumb.pop);

	/* don't need to access result relation any more */
	release_pstate_resources(pstate);

	foreach(l, extras_before)
		result = list_concat(result, parse_sub_analyze(lfirst(l), pstate));

	result = lappend(result, query);

	foreach(l, extras_after)
 		tmp = list_concat(tmp, parse_sub_analyze(lfirst(l), pstate));

	/*
	 * If this is the top level query and it is a CreateStmt and it
	 * has a partition by clause, reorder the expanded extras_after so
	 * that AlterTable is able to build the partitioning hierarchy
	 * better. The problem with the existing list is that for
	 * subpartitioned tables, the subpartitions will be added to the
	 * hierarchy before the root, which means we cannot get the parent
	 * oid of rules.
	 *
	 * nefarious: special KeepMe case in cdbpartition.c:atpxPart_validate_spec
	 */
	if (pstate->parentParseState == NULL && query->utilityStmt &&
		IsA(query->utilityStmt, CreateStmt) &&
		((CreateStmt *)query->utilityStmt)->partitionBy)
	{
		/*
		 * We just break the statements into two lists: alter statements and
		 * other statements.
		 */
		List *alters = NIL;
		List *others = NIL;
		Query **stmts;
		int i = 0;
		int j;

		foreach(l, tmp)
		{
			Query *q = lfirst(l);

			Assert(IsA(q, Query));

			if (IsA(q->utilityStmt, AlterTableStmt))
				alters = lappend(alters, q);
			else
				others = lappend(others, q);
		}

		Assert(list_length(alters));

		/*
		 * Now, sort the ALTER statements so that the deeper partition members
		 * are processed last.
		 */
		stmts = palloc(list_length(alters) * sizeof(Query *));
		foreach(l, alters)
			stmts[i++] = (Query *)lfirst(l);

		qsort(stmts, i, sizeof(void *), alter_cmp);

		list_free(alters);
		alters = NIL;
		for (j = 0; j < i; j++)
		{
			AlterTableStmt *n;
			alters = lappend(alters, stmts[j]);

			n = (AlterTableStmt *)((Query *)stmts[j])->utilityStmt;
		}
		result = list_concat(result, others);
		result = list_concat(result, alters);

	}
	else
		result = list_concat(result, tmp);

	/*
	 * Make sure that only the original query is marked original. We have to
	 * do this explicitly since recursive calls of do_parse_analyze will have
	 * marked some of the added-on queries as "original".  Also mark only the
	 * original query as allowed to set the command-result tag.
	 */
	foreach(l, result)
	{
		Query	   *q = lfirst(l);

		if (q == query)
		{
			q->querySource = QSRC_ORIGINAL;
			q->canSetTag = true;
		}
		else
		{
			q->querySource = QSRC_PARSER;
			q->canSetTag = false;
		}
	}

	return result;
}

static void
release_pstate_resources(ParseState *pstate)
{
	if (pstate->p_target_relation != NULL)
		heap_close(pstate->p_target_relation, NoLock);
	pstate->p_target_relation = NULL;
	pstate->p_target_rangetblentry = NULL;
}


/*
 * parse_analyze_error_callback
 *
 * Called during elog/ereport to add context information to the error message.
 */
static void
parse_analyze_error_callback(void *parsestate)
{
    ParseState             *pstate = (ParseState *)parsestate;
    ParseStateBreadCrumb   *bc;
    int                     location = -1;

    /* No-op if errposition has already been set. */
    if (geterrposition() > 0)
        return;

    /* NOTICE messages don't need any extra baggage. */
    if (elog_getelevel() == NOTICE)
        return;

    /*
	 * Backtrack through trail of breadcrumbs to find a node with location
	 * info. A null node ptr tells us to keep quiet rather than give a
	 * misleading pointer to a token which may be far from the actual problem.
     */
    for (bc = &pstate->p_breadcrumb; bc && bc->node; bc = bc->pop)
    {
        location = parse_expr_location((Expr *)bc->node);
        if (location >= 0)
            break;
    }

    /* Shush the parent query's error callback if we found a location or null */
    if (bc &&
        pstate->parentParseState)
        pstate->parentParseState->p_breadcrumb.node = NULL;

    /* Report approximate offset of error from beginning of statement text. */
    if (location >= 0)
        parser_errposition(pstate, location);
}                               /* parse_analyze_error_callback */


/*
 * transformStmt -
 *	  transform a Parse tree into a Query tree.
 */
static Query *
transformStmt(ParseState *pstate, Node *parseTree,
			  List **extras_before, List **extras_after)
{
	Query	   *result = NULL;

	switch (nodeTag(parseTree))
	{
			/*
			 * Non-optimizable statements
			 */
		case T_CreateStmt:
			result = transformCreateStmt(pstate, (CreateStmt *) parseTree,
										 extras_before, extras_after);
			break;

		case T_CreateExternalStmt:
			result = transformCreateExternalStmt(pstate, (CreateExternalStmt *) parseTree,
												 extras_before, extras_after);
			break;

		case T_IndexStmt:
			result = transformIndexStmt(pstate, (IndexStmt *) parseTree,
										extras_before, extras_after);
			break;

		case T_RuleStmt:
			result = transformRuleStmt(pstate, (RuleStmt *) parseTree,
									   extras_before, extras_after);
			break;

		case T_ViewStmt:
			result = transformViewStmt(pstate, (ViewStmt *) parseTree,
									   extras_before, extras_after);
			break;

		case T_ExplainStmt:
			{
				ExplainStmt *n = (ExplainStmt *) parseTree;

				result = makeNode(Query);
				result->commandType = CMD_UTILITY;
				n->query = transformStmt(pstate, (Node *) n->query,
										 extras_before, extras_after);
				result->utilityStmt = (Node *) parseTree;
			}
			break;

		case T_CopyStmt:
			{
				CopyStmt   *n = (CopyStmt *) parseTree;

				result = makeNode(Query);
				result->commandType = CMD_UTILITY;
				if (n->query)
					n->query = transformStmt(pstate, (Node *) n->query,
											 extras_before, extras_after);
				result->utilityStmt = (Node *) parseTree;
			}
			break;

		case T_AlterTableStmt:
			result = transformAlterTableStmt(pstate,
											 (AlterTableStmt *) parseTree,
											 extras_before, extras_after);
			break;

		case T_PrepareStmt:
			result = transformPrepareStmt(pstate, (PrepareStmt *) parseTree);
			break;

		case T_ExecuteStmt:
			result = transformExecuteStmt(pstate, (ExecuteStmt *) parseTree);
			break;

			/*
			 * Optimizable statements
			 */
		case T_InsertStmt:
			result = transformInsertStmt(pstate, (InsertStmt *) parseTree,
										 extras_before, extras_after);
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

		case T_DeclareCursorStmt:
			result = transformDeclareCursorStmt(pstate,
											(DeclareCursorStmt *) parseTree);
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

	/*
	 * Check that we did not produce too many resnos; at the very least we
	 * cannot allow more than 2^16, since that would exceed the range of a
	 * AttrNumber. It seems safest to use MaxTupleAttributeNumber.
	 */
	if (pstate->p_next_resno - 1 > MaxTupleAttributeNumber)
		ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("target lists can have at most %d entries",
						MaxTupleAttributeNumber)));

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
			/* yes, because it's analyzed just like SELECT */
			result = true;
			break;

		default:
			/* other utility statements don't have any active parse analysis */
			result = false;
			break;
	}

	return result;
}

static Query *
transformViewStmt(ParseState *pstate, ViewStmt *stmt,
				  List **extras_before, List **extras_after)
{
	Query	   *result = makeNode(Query);

	result->commandType = CMD_UTILITY;
	result->utilityStmt = (Node *) stmt;

	stmt->query = transformStmt(pstate, (Node *) stmt->query,
								extras_before, extras_after);

	if (pstate->p_hasDynamicFunction)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INDETERMINATE_DATATYPE),
				 errmsg("CREATE VIEW statements cannot include calls to "
						"dynamically typed function")));
	}

	/*
	 * If a list of column names was given, run through and insert these into
	 * the actual query tree. - thomas 2000-03-08
	 *
	 * Outer loop is over targetlist to make it easier to skip junk targetlist
	 * entries.
	 */
	if (stmt->aliases != NIL)
	{
		ListCell   *alist_item = list_head(stmt->aliases);
		ListCell   *targetList;

		foreach(targetList, stmt->query->targetList)
		{
			TargetEntry *te = (TargetEntry *) lfirst(targetList);

			Assert(IsA(te, TargetEntry));
			/* junk columns don't get aliases */
			if (te->resjunk)
				continue;
			te->resname = pstrdup(strVal(lfirst(alist_item)));
			alist_item = lnext(alist_item);
			if (alist_item == NULL)
				break;			/* done assigning aliases */
		}

		if (alist_item != NULL)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("CREATE VIEW specifies more column "
							"names than columns")));
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

	qual = transformWhereClause(pstate, stmt->whereClause, "WHERE");

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

	/* CDB: Cursor position not available for errors below this point. */
	pstate->p_breadcrumb.node = NULL;

	/* done building the range table and jointree */
	qry->rtable = pstate->p_rtable;
	qry->jointree = makeFromExpr(pstate->p_joinlist, qual);

	qry->hasSubLinks = pstate->p_hasSubLinks;
	qry->hasAggs = pstate->p_hasAggs;
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
transformInsertStmt(ParseState *pstate, InsertStmt *stmt,
					List **extras_before, List **extras_after)
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
	 */
	isGeneralSelect = (selectStmt && selectStmt->valuesLists == NIL);

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
		sub_pstate->p_relnamespace = sub_relnamespace;
		sub_pstate->p_varnamespace = sub_varnamespace;

		/*
		 * Note: we are not expecting that extras_before and extras_after are
		 * going to be used by the transformation of the SELECT statement.
		 */
		selectQuery = transformStmt(sub_pstate, stmt->selectStmt,
									extras_before, extras_after);

		release_pstate_resources(sub_pstate);
		free_parsestate(sub_pstate);

		Assert(IsA(selectQuery, Query));
		Assert(selectQuery->commandType == CMD_SELECT);
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

		foreach(lc, selectStmt->valuesLists)
		{
			List	   *sublist = (List *) lfirst(lc);

			/* CDB: In case of error, note which sublist is involved. */
			pstate->p_breadcrumb.node = (Node *)sublist;

			/* Do basic expression transformation (same as a ROW() expr) */
			sublist = transformExpressionList(pstate, sublist);

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

		/* CDB: Clear error location. */
		pstate->p_breadcrumb.node = NULL;

		/*
		 * There mustn't have been any table references in the expressions,
		 * else strange things would happen, like Cartesian products of those
		 * tables with the VALUES list ...
		 */
		if (pstate->p_joinlist != NIL)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("VALUES must not contain table references")));

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
					 errhint("Use SELECT ... UNION ALL ... instead.")));

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
		/*----------
		 * Process INSERT ... VALUES with a single VALUES sublist.
		 * We treat this separately for efficiency and for historical
		 * compatibility --- specifically, allowing table references,
		 * such as
		 *			INSERT INTO foo VALUES(bar.*)
		 *
		 * The sublist is just computed directly as the Query's targetlist,
		 * with no VALUES RTE.	So it works just like SELECT without FROM.
		 *----------
		 */
		List	   *valuesLists = selectStmt->valuesLists;

		Assert(list_length(valuesLists) == 1);

		/* Do basic expression transformation (same as a ROW() expr) */
		exprList = transformExpressionList(pstate,
										   (List *) linitial(valuesLists));

		/* Prepare row for assignment to target table */
		exprList = transformInsertRow(pstate, exprList,
									  stmt->cols,
									  icolumns, attrnos);
	}

	/*
	 * Generate query's target list using the computed list of expressions.
	 */
	qry->targetList = NIL;
	icols = list_head(icolumns);
	attnos = list_head(attrnos);
	foreach(lc, exprList)
	{
		Expr	   *expr = (Expr *) lfirst(lc);
		ResTarget  *col;
		TargetEntry *tle;

		col = (ResTarget *) lfirst(icols);
		Assert(IsA(col, ResTarget));

		tle = makeTargetEntry(expr,
							  (AttrNumber) lfirst_int(attnos),
							  col->name,
							  false);
		qry->targetList = lappend(qry->targetList, tle);

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


	/* CDB: Cursor position not available for errors below this point. */
	pstate->p_breadcrumb.node = NULL;

	/* done building the range table and jointree */
	qry->rtable = pstate->p_rtable;
	qry->jointree = makeFromExpr(pstate->p_joinlist, NULL);

	qry->hasSubLinks = pstate->p_hasSubLinks;
	/* aggregates not allowed (but subselects are okay) */
	if (pstate->p_hasAggs)
		ereport(ERROR,
				(errcode(ERRCODE_GROUPING_ERROR),
				 errmsg("cannot use aggregate function in VALUES")));
	if (pstate->p_hasWindFuncs)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("cannot use window function in VALUES")));

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
				 errmsg("INSERT has more expressions than target columns")));
	if (stmtcols != NIL &&
		list_length(exprlist) < list_length(icolumns))
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("INSERT has more target columns than expressions")));

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
 * Tells the caller if CO is explicitly disabled, to handle cases where we
 * want to ignore encoding clauses in partition expansion.
 *
 * This is an ugly special case that backup expects to work and since we've got
 * tonnes of dumps out there and the possibility that users have learned this
 * grammar from them, we must continue to support it.
 */
static bool
co_explicitly_disabled(List *opts)
{
	ListCell *lc;

	foreach(lc, opts)
	{
		DefElem *el = lfirst(lc);
		char *arg = NULL;

		/* Arguement will be a Value */
		if (!el->arg)
		{
			continue;
		}

		arg = defGetString(el);
		bool result = false;
		if (pg_strcasecmp("appendonly", el->defname) == 0 &&
			pg_strcasecmp("false", arg) == 0)
		{
			result = true;
		}
		else if (pg_strcasecmp("orientation", el->defname) == 0 &&
				 pg_strcasecmp("column", arg) != 0)
		{
			result = true;
		}

		if (result)
		{
			return true;
		}
	}
	return false;
}

/*
 * Tell the caller whether appendonly=true and orientation=column
 * have been specified.
 */
bool
is_aocs(List *opts)
{
	bool found_ao = false;
	bool found_cs = false;
	bool aovalue = false;
	bool csvalue = false;

	ListCell *lc;

	foreach(lc, opts)
	{
		DefElem *el = lfirst(lc);
		char *arg = NULL;

		/* Arguement will be a Value */
		if (!el->arg)
		{
			continue;
		}

		arg = defGetString(el);

		if (pg_strcasecmp("appendonly", el->defname) == 0)
		{
			found_ao = true;
			if (!parse_bool(arg, &aovalue))
				elog(ERROR, "invalid value for option \"appendonly\"");
		}
		else if (pg_strcasecmp("orientation", el->defname) == 0)
		{
			found_cs = true;
			csvalue = pg_strcasecmp("column", arg) == 0;
		}
	}
	if (!found_ao)
		aovalue = isDefaultAO();
	if (!found_cs)
		csvalue = isDefaultAOCS();
	return (aovalue && csvalue);
}

/*
 * See if two encodings attempt to see the same parameters. If test_conflicts is
 * true, allow setting the same value, but the setting must be identical.
 */
static bool
encodings_overlap(List *a, List *b, bool test_conflicts)
{
	ListCell *lca;

	foreach(lca, a)
	{
		ListCell *lcb;
		DefElem *ela = lfirst(lca);

		foreach(lcb, b)
		{
			DefElem *elb = lfirst(lcb);

			if (pg_strcasecmp(ela->defname, elb->defname) == 0)
			{
				if (test_conflicts)
				{
					if (!ela->arg && !elb->arg)
						return true;
					else if (!ela->arg || !elb->arg)
					{
						/* skip */
					}
					else
					{
						char *ela_str = defGetString(ela);
						char *elb_str = defGetString(elb);

						if (pg_strcasecmp(ela_str, elb_str) != 0)
							return true;
					}
				}
				else
					return true;
			}
		}
	}
	return false;
}

/*
 * Transform and validate the actual encoding clauses.
 *
 * We need tell the underlying system that these are AO/CO tables too,
 * hence the concatenation of the extra elements.
 */
List *
transformStorageEncodingClause(List *options)
{
	Datum d;
	ListCell *lc;
	DefElem *dl;
	foreach(lc, options)
	{
		dl = (DefElem *) lfirst(lc);
		if (pg_strncasecmp(dl->defname, SOPT_CHECKSUM, strlen(SOPT_CHECKSUM))
			== 0)
		{
			elog(ERROR, "\"%s\" is not a column specific option.",
				 SOPT_CHECKSUM);
		}
	}
	List *extra = list_make2(makeDefElem("appendonly",
										 (Node *)makeString("true")),
							 makeDefElem("orientation",
										 (Node *)makeString("column")));

	/* add defaults for missing values */
	options = fillin_encoding(options);

	/*
	 * The following two statements validate that the encoding clause is well
	 * formed.
	 */
	d = transformRelOptions(PointerGetDatum(NULL),
									  list_concat(extra, options),
									  true, false);
	(void)heap_reloptions(RELKIND_RELATION, d, true);

	return options;
}

/*
 * Validate the sanity of column reference storage clauses.
 *
 * 1. Ensure that we only refer to columns that exist.
 * 2. Ensure that each column is referenced either zero times or once.
 */
static void
validateColumnStorageEncodingClauses(List *stenc, CreateStmt *stmt)
{
	ListCell *lc;
	struct HTAB *ht = NULL;
	struct colent {
		char colname[NAMEDATALEN];
		int count;
	} *ce = NULL;

	if (!stenc)
		return;

	/* Generate a hash table for all the columns */
	foreach(lc, stmt->tableElts)
	{
		Node *n = lfirst(lc);

		if (IsA(n, ColumnDef))
		{
			ColumnDef *c = (ColumnDef *)n;
			char *colname;
			bool found = false;
			size_t n = NAMEDATALEN - 1 < strlen(c->colname) ?
							NAMEDATALEN - 1 : strlen(c->colname);

			colname = palloc0(NAMEDATALEN);
			MemSet(colname, 0, NAMEDATALEN);
			memcpy(colname, c->colname, n);
			colname[n] = '\0';

			if (!ht)
			{
				HASHCTL  cacheInfo;
				int      cacheFlags;

				memset(&cacheInfo, 0, sizeof(cacheInfo));
				cacheInfo.keysize = NAMEDATALEN;
				cacheInfo.entrysize = sizeof(*ce);
				cacheFlags = HASH_ELEM;

				ht = hash_create("column info cache",
								 list_length(stmt->tableElts),
								 &cacheInfo, cacheFlags);
			}

			ce = hash_search(ht, colname, HASH_ENTER, &found);

			/*
			 * The user specified a duplicate column name. We check duplicate
			 * column names VERY late (under MergeAttributes(), which is called
			 * by DefineRelation(). For the specific case here, it is safe to
			 * call out that this is a duplicate. We don't need to delay until
			 * we look at inheritance.
			 */
			if (found)
			{
				ereport(ERROR,
						(errcode(ERRCODE_DUPLICATE_COLUMN),
						 errmsg("column \"%s\" duplicated",
								colname)));
				
			}
			ce->count = 0;
		}
	}

	/*
	 * If the table has no columns -- usually in the partitioning case -- then
	 * we can short circuit.
	 */
	if (!ht)
		return;

	/*
	 * All column reference storage directives without the DEFAULT
	 * clause should refer to real columns.
	 */
	foreach(lc, stenc)
	{
		ColumnReferenceStorageDirective *c = lfirst(lc);

		Insist(IsA(c, ColumnReferenceStorageDirective));

		if (c->deflt)
			continue;
		else
		{
			bool found = false;
			char colname[NAMEDATALEN];
			size_t collen = strlen(c->column);
			size_t n = NAMEDATALEN - 1 < collen ? NAMEDATALEN - 1 : collen;
			MemSet(colname, 0, NAMEDATALEN);
			memcpy(colname, c->column, n);
			colname[n] = '\0';

			ce = hash_search(ht, colname, HASH_FIND, &found);

			if (!found)
				elog(ERROR, "column \"%s\" does not exist", colname);

			ce->count++;

			if (ce->count > 1)
				elog(ERROR, "column \"%s\" referenced in more than one "
					 "COLUMN ENCODING clause", colname);

		}
	}

	hash_destroy(ht);
}

/*
 * Find the column reference storage encoding clause for `column'.
 *
 * This is called by transformAttributeEncoding() in a loop but stenc should be
 * quite small in practice.
 */
static ColumnReferenceStorageDirective *
find_crsd(char *column, List *stenc)
{
	ListCell *lc;

	foreach(lc, stenc)
	{
		ColumnReferenceStorageDirective *c = lfirst(lc);

		if (c->deflt == false && strcmp(column, c->column) == 0)
			return c;
	}
	return NULL;
}

List *
TypeNameGetStorageDirective(TypeName *typname)
{
	HeapTuple tuple;
	cqContext  *pcqCtx;
	Oid typid = typenameTypeId(NULL, typname);
	List *out = NIL;

	/* XXX XXX: SELECT typoptions */
	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_type_encoding "
				" WHERE typid = :1 ",
				ObjectIdGetDatum(typid)));

	tuple = caql_getnext(pcqCtx);

	if (HeapTupleIsValid(tuple))
	{
		Datum options;
		bool isnull;

		options = caql_getattr(pcqCtx, 
							   Anum_pg_type_encoding_typoptions,
							   &isnull);

		Insist(!isnull);

		out = untransformRelOptions(options);
	}

	caql_endscan(pcqCtx);
	return out;
}

/*
 * Make a default column storage directive from a WITH clause
 * Ignore options in the WITH clause that don't appear in 
 * storage_directives for column-level compression.
 */
List *
form_default_storage_directive(List *enc)
{
	List *out = NIL;
	ListCell *lc;

	foreach(lc, enc)
	{
		DefElem *el = lfirst(lc);

		if (!el->defname)
			out = lappend(out, copyObject(el));

		if (pg_strcasecmp("appendonly", el->defname) == 0)
			continue;
		if (pg_strcasecmp("orientation", el->defname) == 0)
			continue;
		if (pg_strcasecmp("oids", el->defname) == 0)
			continue;
		if (pg_strcasecmp("fillfactor", el->defname) == 0)
			continue;
		if (pg_strcasecmp("tablename", el->defname) == 0)
			continue;
		/* checksum is not a column specific attribute. */
		if (pg_strcasecmp("checksum", el->defname) == 0)
			continue;
		out = lappend(out, copyObject(el));
	}
	return out;
}

static List *
transformAttributeEncoding(List *stenc, CreateStmt *stmt, CreateStmtContext *cxt)
{
	ListCell *lc;
	bool found_enc = stenc != NIL;
	bool can_enc = is_aocs(stmt->options);
	ColumnReferenceStorageDirective *deflt = NULL;
	List *newenc = NIL;
	List *tmpenc;
	MemoryContext oldCtx;

#define UNSUPPORTED_ORIENTATION_ERROR() \
	ereport(ERROR, \
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED), \
			 errmsg("ENCODING clause only supported with column oriented tables")))

	/* We only support the attribute encoding clause on AOCS tables */
	if (stenc && !can_enc)
		UNSUPPORTED_ORIENTATION_ERROR();

	/* Use the temporary context to avoid leaving behind so much garbage. */
	oldCtx = MemoryContextSwitchTo(cxt->tempCtx);

	/* get the default clause, if there is one. */
	foreach(lc, stenc)
	{
		ColumnReferenceStorageDirective *c = lfirst(lc);
		Insist(IsA(c, ColumnReferenceStorageDirective));

		if (c->deflt)
		{
			/*
			 * Some quick validation: there should only be one default
			 * clause
			 */
			if (deflt)
				elog(ERROR, "only one default column encoding may be specified");

			deflt = copyObject(c);
			deflt->encoding = transformStorageEncodingClause(deflt->encoding);

			/*
			 * The default encoding and the with clause better not
			 * try and set the same options!
			 */
			if (encodings_overlap(stmt->options, deflt->encoding, false))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("DEFAULT COLUMN ENCODING clause cannot "
								"override values set in WITH clause")));
		}
	}

	/*
	 * If no default has been specified, we might create one out of the
	 * WITH clause.
	 */
	tmpenc = form_default_storage_directive(stmt->options);

	if (tmpenc)
	{
		deflt = makeNode(ColumnReferenceStorageDirective);
		deflt->deflt = true;
		deflt->encoding = transformStorageEncodingClause(tmpenc);
	}

	/*
	 * Loop over all columns. If a column has a column reference storage clause
	 * -- i.e., COLUMN name ENCODING () -- apply that. Otherwise, apply the
	 * default.
	 */
	foreach(lc, cxt->columns)
	{
		ColumnDef *d = (ColumnDef *) lfirst(lc);
		ColumnReferenceStorageDirective *c;

		Insist(IsA(d, ColumnDef));

		c = makeNode(ColumnReferenceStorageDirective);
		c->column = pstrdup(d->colname);

		/*
		 * Find a storage encoding for this column, in this order:
		 *
		 * 1. An explicit encoding clause in the ColumnDef
		 * 2. A column reference storage directive for this column
		 * 3. A default column encoding in the statement
		 * 4. A default for the type.
		 */
		if (d->encoding)
		{
			found_enc = true;
			c->encoding = transformStorageEncodingClause(d->encoding);
		}
		else
		{
			ColumnReferenceStorageDirective *s = find_crsd(c->column, stenc);

			if (s)
				c->encoding = transformStorageEncodingClause(s->encoding);
			else
			{
				if (deflt)
					c->encoding = copyObject(deflt->encoding);
				else
				{
					List *te = TypeNameGetStorageDirective(d->typname);

					if (te)
						c->encoding = copyObject(te);
					else
						c->encoding = default_column_encoding_clause();
				}
			}
		}
		newenc = lappend(newenc, c);
	}

	/* Check again incase we expanded a some column encoding clauses */
	if (!can_enc)
	{
		if (found_enc)
			UNSUPPORTED_ORIENTATION_ERROR();
		else
			newenc = NULL;
	}

	validateColumnStorageEncodingClauses(newenc, stmt);

	/* copy the result out of the temporary memory context */
	MemoryContextSwitchTo(oldCtx);
	newenc = copyObject(newenc);

	return newenc;
}

/*
 * transformCreateStmt -
 *	  transforms the "create table" statement
 *	  SQL92 allows constraints to be scattered all over, so thumb through
 *	   the columns and collect all constraints into one place.
 *	  If there are any implied indices (e.g. UNIQUE or PRIMARY KEY)
 *	   then expand those into multiple IndexStmt blocks.
 *	  - thomas 1997-12-02
 */
Query *
transformCreateStmt(ParseState *pstate, CreateStmt *stmt,
					List **extras_before, List **extras_after)
{
	CreateStmtContext cxt;
	Query	   *q;
	ListCell   *elements;
	List  	   *likeDistributedBy = NIL;
	bool	    bQuiet = false;	/* shut up transformDistributedBy messages */
	List	   *stenc = NIL; /* column reference storage encoding clauses */

	/*
	 * We don't normally care much about the memory consumption of parsing,
	 * because any memory leaked is leaked into MessageContext which is
	 * reset between each command. But if a table is heavily partitioned,
	 * the CREATE TABLE statement can be expanded into hundreds or even
	 * thousands of CreateStmts, so the leaks start to add up. To reduce
	 * the memory consumption, we use a temporary memory context that's
	 * destroyed after processing the CreateStmt for some parts of the
	 * processing.
	 */
	cxt.tempCtx =
		AllocSetContextCreate(CurrentMemoryContext,
							  "CreateStmt analyze context",
							  ALLOCSET_DEFAULT_MINSIZE,
							  ALLOCSET_DEFAULT_INITSIZE,
							  ALLOCSET_DEFAULT_MAXSIZE);

	cxt.stmtType = "CREATE TABLE";
	cxt.relation = stmt->relation;
	cxt.inhRelations = stmt->inhRelations;
	cxt.isalter = false;
	cxt.isaddpart = stmt->is_add_part;
	cxt.columns = NIL;
	cxt.ckconstraints = NIL;
	cxt.fkconstraints = NIL;
	cxt.ixconstraints = NIL;
	cxt.inh_indexes = NIL;
	cxt.blist = NIL;
	cxt.alist = NIL;
	cxt.dlist = NIL; /* for deferred analysis requiring the created table */
	cxt.pkey = NULL;
	cxt.hasoids = interpretOidsOption(stmt->options);

	stmt->policy = NULL;

	/* Disallow inheritance in combination with partitioning. */
	if (stmt->inhRelations && (stmt->partitionBy || stmt->is_part_child ))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("cannot mix inheritance with partitioning")));
	}

	/* Disallow inheritance for CO table */
	if (stmt->inhRelations && is_aocs(stmt->options))
	{
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("INHERITS clause cannot be used with column oriented tables")));
	}

	/* Only on top-most partitioned tables. */
	if ( stmt->partitionBy && !stmt->is_part_child )
	{
		fixCreateStmtForPartitionedTable(stmt);
	}

	/*
	 * Run through each primary element in the table creation clause. Separate
	 * column defs from constraints, and do preliminary analysis.
	 */
	foreach(elements, stmt->tableElts)
	{
		Node	   *element = lfirst(elements);

		switch (nodeTag(element))
		{
			case T_ColumnDef:
				transformColumnDefinition(pstate, &cxt,
										  (ColumnDef *) element);
				break;

			case T_Constraint:
				transformTableConstraint(pstate, &cxt,
										 (Constraint *) element);
				break;

			case T_FkConstraint:
				/* No pre-transformation needed */
				cxt.fkconstraints = lappend(cxt.fkconstraints, element);
				break;

			case T_InhRelation:
				{
					bool		isBeginning = (cxt.columns == NIL);

					transformInhRelation(pstate, &cxt,
										 (InhRelation *) element, false);

					if (Gp_role == GP_ROLE_DISPATCH && isBeginning &&
						stmt->distributedBy == NIL &&
						stmt->inhRelations == NIL &&
						stmt->policy == NULL)
					{
						likeDistributedBy = getLikeDistributionPolicy((InhRelation *) element);
					}
				}
				break;

			case T_ColumnReferenceStorageDirective:
				/* processed below in transformAttributeEncoding() */
				stenc = lappend(stenc, element);
				break;

			default:
				elog(ERROR, "unrecognized node type: %d",
					 (int) nodeTag(element));
				break;
		}
	}

	/*
	 * transformIndexConstraints wants cxt.alist to contain only index
	 * statements, so transfer anything we already have into extras_after
	 * immediately.
	 */
	*extras_after = list_concat(cxt.alist, *extras_after);
	cxt.alist = NIL;

	Assert(stmt->constraints == NIL);

	/*
	 * Postprocess constraints that give rise to index definitions.
	 */
	transformIndexConstraints(pstate, &cxt, stmt->is_add_part || stmt->is_split_part);

	/*
	 * Carry any deferred analysis statements forward.  Added for MPP-13750
	 * but should also apply to the similar case involving simple inheritance.
	 */
	if ( cxt.dlist )
	{
		stmt->deferredStmts = list_concat(stmt->deferredStmts, cxt.dlist);
		cxt.dlist = NIL;
	}

	/*
	 * Postprocess foreign-key constraints.
	 * But don't cascade FK constraints to parts, yet.
	 */
	if ( ! stmt->is_part_child )
		transformFKConstraints(pstate, &cxt, true, false);

	/*
	 * Analyze attribute encoding clauses.
	 *
	 * Partitioning configurations may have things like:
	 *
	 * CREATE TABLE ...
	 *  ( a int ENCODING (...))
	 * WITH (appendonly=true, orientation=column)
	 * PARTITION BY ...
	 * (PARTITION ... WITH (appendonly=false));
	 *
	 * We don't want to throw an error when we try to apply the ENCODING clause
	 * to the partition which the user wants to be non-AO. Just ignore it
	 * instead.
	 */
	if (!is_aocs(stmt->options) && stmt->is_part_child)
	{
		if (co_explicitly_disabled(stmt->options) || !stenc)
			stmt->attr_encodings = NIL;
		else
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("ENCODING clause only supported with "
							"column oriented partitioned tables")));

		}
	}
	else
		stmt->attr_encodings = transformAttributeEncoding(stenc, stmt, &cxt);

	/*
	 * Postprocess Greenplum Database distribution columns
	 */
	if (stmt->is_part_child ||
		(stmt->partitionBy &&
		 (
			 /* be very quiet if set subpartn template */
			 (((PartitionBy *)(stmt->partitionBy))->partQuiet ==
			  PART_VERBO_NOPARTNAME) ||
			 (
				 /* quiet for partitions of depth > 0 */
				 (((PartitionBy *)(stmt->partitionBy))->partDepth != 0) &&
				 (((PartitionBy *)(stmt->partitionBy))->partQuiet !=
				  PART_VERBO_NORMAL)
					 )
				 )
				))
			bQuiet = true; /* silence distro messages for partitions */

	transformDistributedBy(pstate, &cxt, stmt->distributedBy, &stmt->policy,
						   likeDistributedBy, bQuiet);

	/*
	 * Process table partitioning clause
	 */
	transformPartitionBy(pstate, &cxt, stmt, stmt->partitionBy, stmt->policy);

	/*
	 * Output results.
	 */
	q = makeNode(Query);
	q->commandType = CMD_UTILITY;
	q->utilityStmt = (Node *) stmt;
	stmt->tableElts = cxt.columns;
	stmt->constraints = cxt.ckconstraints;
	*extras_before = list_concat(*extras_before, cxt.blist);
	*extras_after = list_concat(cxt.alist, *extras_after);

	MemoryContextDelete(cxt.tempCtx);

	return q;
}

static Query *
transformCreateExternalStmt(ParseState *pstate, CreateExternalStmt *stmt,
							List **extras_before, List **extras_after)
{
	CreateStmtContext cxt;
	Query	   *q;
	ListCell   *elements;
	List  	   *likeDistributedBy = NIL;
	bool	    bQuiet = false;	/* shut up transformDistributedBy messages */
	bool		iswritable = stmt->iswritable;

	cxt.stmtType = "CREATE EXTERNAL TABLE";
	cxt.relation = stmt->relation;
	cxt.inhRelations = NIL;
	cxt.hasoids = false;
	cxt.isalter = false;
	cxt.columns = NIL;
	cxt.ckconstraints = NIL;
	cxt.fkconstraints = NIL;
	cxt.ixconstraints = NIL;
	cxt.pkey = NULL;

	cxt.blist = NIL;
	cxt.alist = NIL;

	/*
	 * Run through each primary element in the table creation clause. Separate
	 * column defs from constraints, and do preliminary analysis.
	 */
	foreach(elements, stmt->tableElts)
	{
		Node	   *element = lfirst(elements);

		switch (nodeTag(element))
		{
			case T_ColumnDef:
				transformColumnDefinition(pstate, &cxt,
										  (ColumnDef *) element);
				break;

			case T_Constraint:
			case T_FkConstraint:
				/* should never happen. If it does fix gram.y */
				elog(ERROR, "node type %d not supported for external tables",
					 (int) nodeTag(element));
				break;

			case T_InhRelation:
				{
					/* LIKE */
					bool	isBeginning = (cxt.columns == NIL);

					transformInhRelation(pstate, &cxt,
										 (InhRelation *) element, true);

					if (Gp_role == GP_ROLE_DISPATCH && isBeginning &&
						stmt->distributedBy == NIL &&
						stmt->policy == NULL &&
						iswritable /* dont bother if readable table */)
					{
						likeDistributedBy = getLikeDistributionPolicy((InhRelation *) element);
					}
				}
				break;

			default:
				elog(ERROR, "unrecognized node type: %d",
					 (int) nodeTag(element));
				break;
		}
	}

	/*
	 * Forbid LOG ERRORS and ON MASTER combination.
	 */
	if (stmt->exttypedesc->exttabletype == EXTTBL_TYPE_EXECUTE)
	{
		ListCell   *exec_location_opt;

		foreach(exec_location_opt, stmt->exttypedesc->on_clause)
		{
			DefElem    *defel = (DefElem *) lfirst(exec_location_opt);

			if (strcmp(defel->defname, "master") == 0)
			{
				SingleRowErrorDesc *srehDesc = (SingleRowErrorDesc *)stmt->sreh;

				if(srehDesc && srehDesc->into_file)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("External web table with ON MASTER clause "
									"cannot use LOG ERRORS feature.")));
			}
		}
	}

	/*
	 * Handle DISTRIBUTED BY clause, if any.
	 *
	 * For writeable external tables, by default we distribute RANDOMLY, or
	 * by the distribution key of the LIKE table if exists. However, if
	 * DISTRIBUTED BY was specified we use it by calling the regular
	 * transformDistributedBy and handle it like we would for non external
	 * tables.
	 *
	 * For readable external tables, don't create a policy row at all.
	 * Non-EXECUTE type external tables are implicitly randomly distributed.
	 * EXECUTE type external tables encapsulate similar information in the
	 * "ON <segment spec>" clause, which is stored in pg_exttable.location.
	 */
	if (iswritable)
	{
		if (stmt->distributedBy == NIL && likeDistributedBy == NIL)
		{
			/*
			 * defaults to DISTRIBUTED RANDOMLY irrespective of the
			 * gp_create_table_random_default_distribution guc.
			 */
			stmt->policy = createRandomDistribution(MaxPolicyAttributeNumber);
		}
		else
		{
			/* regular DISTRIBUTED BY transformation */
			transformDistributedBy(pstate, &cxt, stmt->distributedBy, &stmt->policy,
								   likeDistributedBy, bQuiet);
		}
	}
	else if (stmt->distributedBy != NIL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("Readable external tables can\'t specify a DISTRIBUTED BY clause.")));

	Assert(cxt.ckconstraints == NIL);
	Assert(cxt.fkconstraints == NIL);
	Assert(cxt.ixconstraints == NIL);

	/*
	 * Output results.
	 */
	q = makeNode(Query);
	q->commandType = CMD_UTILITY;
	q->utilityStmt = (Node *) stmt;
	stmt->tableElts = cxt.columns;
	*extras_before = list_concat(*extras_before, cxt.blist);
	*extras_after = list_concat(cxt.alist, *extras_after);

	return q;
}

static void
transformColumnDefinition(ParseState *pstate, CreateStmtContext *cxt,
						  ColumnDef *column)
{
	bool		is_serial;
	bool		saw_nullable;
	bool		saw_default;
	Constraint *constraint;
	ListCell   *clist;

	cxt->columns = lappend(cxt->columns, column);

	/* Check for SERIAL pseudo-types */
	is_serial = false;
	if (list_length(column->typname->names) == 1)
	{
		char	   *typname = strVal(linitial(column->typname->names));

		if (strcmp(typname, "serial") == 0 ||
			strcmp(typname, "serial4") == 0)
		{
			is_serial = true;
			column->typname->names = NIL;
			column->typname->typid = INT4OID;
		}
		else if (strcmp(typname, "bigserial") == 0 ||
				 strcmp(typname, "serial8") == 0)
		{
			is_serial = true;
			column->typname->names = NIL;
			column->typname->typid = INT8OID;
		}
	}

	/* Do necessary work on the column type declaration */
	transformColumnType(pstate, column);

	/* Special actions for SERIAL pseudo-types */
	if (is_serial)
	{
		Oid			snamespaceid;
		char	   *snamespace;
		char	   *sname;
		char	   *qstring;
		A_Const    *snamenode;
		FuncCall   *funccallnode;
		CreateSeqStmt *seqstmt;
		AlterSeqStmt *altseqstmt;
		List	   *attnamelist;

		/*
		 * Determine namespace and name to use for the sequence.
		 *
		 * Although we use ChooseRelationName, it's not guaranteed that the
		 * selected sequence name won't conflict; given sufficiently long
		 * field names, two different serial columns in the same table could
		 * be assigned the same sequence name, and we'd not notice since we
		 * aren't creating the sequence quite yet.  In practice this seems
		 * quite unlikely to be a problem, especially since few people would
		 * need two serial columns in one table.
		 */
		snamespaceid = RangeVarGetCreationNamespace(cxt->relation);
		snamespace = get_namespace_name(snamespaceid);
		sname = ChooseRelationName(cxt->relation->relname,
								   column->colname,
								   "seq",
								   snamespaceid);

		ereport(NOTICE,
				(errmsg("%s will create implicit sequence \"%s\" for serial column \"%s.%s\"",
						cxt->stmtType, sname,
						cxt->relation->relname, column->colname)));

		/*
		 * Build a CREATE SEQUENCE command to create the sequence object, and
		 * add it to the list of things to be done before this CREATE/ALTER
		 * TABLE.
		 */
		seqstmt = makeNode(CreateSeqStmt);
		seqstmt->sequence = makeRangeVar(snamespace, sname, -1);
		seqstmt->sequence->istemp = cxt->relation->istemp;
		seqstmt->options = NIL;


		cxt->blist = lappend(cxt->blist, seqstmt);

		/*
		 * Build an ALTER SEQUENCE ... OWNED BY command to mark the sequence
		 * as owned by this column, and add it to the list of things to be
		 * done after this CREATE/ALTER TABLE.
		 */
		altseqstmt = makeNode(AlterSeqStmt);
		altseqstmt->sequence = makeRangeVar(snamespace, sname, -1);
		attnamelist = list_make3(makeString(snamespace),
								 makeString(cxt->relation->relname),
								 makeString(column->colname));
		altseqstmt->options = list_make1(makeDefElem("owned_by",
													 (Node *) attnamelist));

		cxt->alist = lappend(cxt->alist, altseqstmt);

		/*
		 * Create appropriate constraints for SERIAL.  We do this in full,
		 * rather than shortcutting, so that we will detect any conflicting
		 * constraints the user wrote (like a different DEFAULT).
		 *
		 * Create an expression tree representing the function call
		 * nextval('sequencename').  We cannot reduce the raw tree to cooked
		 * form until after the sequence is created, but there's no need to do
		 * so.
		 */
		qstring = quote_qualified_identifier(snamespace, sname);
		snamenode = makeNode(A_Const);
		snamenode->val.type = T_String;
		snamenode->val.val.str = qstring;
		snamenode->typname = SystemTypeName("regclass");
        snamenode->location = -1;                                       /*CDB*/
		funccallnode = makeNode(FuncCall);
		funccallnode->funcname = SystemFuncName("nextval");
		funccallnode->args = list_make1(snamenode);
		funccallnode->agg_star = false;
		funccallnode->agg_distinct = false;
		funccallnode->func_variadic = false;
		funccallnode->location = -1;

		constraint = makeNode(Constraint);
		constraint->contype = CONSTR_DEFAULT;
		constraint->raw_expr = (Node *) funccallnode;
		constraint->cooked_expr = NULL;
		constraint->keys = NIL;
		column->constraints = lappend(column->constraints, constraint);

		constraint = makeNode(Constraint);
		constraint->contype = CONSTR_NOTNULL;
		column->constraints = lappend(column->constraints, constraint);
	}

	/* Process column constraints, if any... */
	transformConstraintAttrs(column->constraints);

	saw_nullable = false;
	saw_default = false;

	foreach(clist, column->constraints)
	{
		constraint = lfirst(clist);

		/*
		 * If this column constraint is a FOREIGN KEY constraint, then we fill
		 * in the current attribute's name and throw it into the list of FK
		 * constraints to be processed later.
		 */
		if (IsA(constraint, FkConstraint))
		{
			FkConstraint *fkconstraint = (FkConstraint *) constraint;

			fkconstraint->fk_attrs = list_make1(makeString(column->colname));
			cxt->fkconstraints = lappend(cxt->fkconstraints, fkconstraint);
			continue;
		}

		Assert(IsA(constraint, Constraint));

		switch (constraint->contype)
		{
			case CONSTR_NULL:
				if (saw_nullable && column->is_not_null)
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("conflicting NULL/NOT NULL declarations for column \"%s\" of table \"%s\"",
								  column->colname, cxt->relation->relname)));
				column->is_not_null = FALSE;
				saw_nullable = true;
				break;

			case CONSTR_NOTNULL:
				if (saw_nullable && !column->is_not_null)
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("conflicting NULL/NOT NULL declarations for column \"%s\" of table \"%s\"",
								  column->colname, cxt->relation->relname)));
				column->is_not_null = TRUE;
				saw_nullable = true;
				break;

			case CONSTR_DEFAULT:
				if (saw_default)
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("multiple default values specified for column \"%s\" of table \"%s\"",
								  column->colname, cxt->relation->relname)));
				/* 
				 * Note: DEFAULT NULL maps to constraint->raw_expr == NULL 
				 * 
				 * We lose the knowledge that the user specified DEFAULT NULL at
				 * this point, so we record it in default_is_null
				 */
				column->raw_default = constraint->raw_expr;
				column->default_is_null = !constraint->raw_expr;
				Assert(constraint->cooked_expr == NULL);
				saw_default = true;
				break;

			case CONSTR_PRIMARY:
			case CONSTR_UNIQUE:
				if (constraint->keys == NIL)
					constraint->keys = list_make1(makeString(column->colname));
				cxt->ixconstraints = lappend(cxt->ixconstraints, constraint);
				break;

			case CONSTR_CHECK:
				cxt->ckconstraints = lappend(cxt->ckconstraints, constraint);
				break;

			case CONSTR_ATTR_DEFERRABLE:
			case CONSTR_ATTR_NOT_DEFERRABLE:
			case CONSTR_ATTR_DEFERRED:
			case CONSTR_ATTR_IMMEDIATE:
				/* transformConstraintAttrs took care of these */
				break;

			default:
				elog(ERROR, "unrecognized constraint type: %d",
					 constraint->contype);
				break;
		}
	}
}

static void
transformTableConstraint(ParseState *pstate, CreateStmtContext *cxt,
						 Constraint *constraint)
{
	switch (constraint->contype)
	{
		case CONSTR_PRIMARY:
		case CONSTR_UNIQUE:
			cxt->ixconstraints = lappend(cxt->ixconstraints, constraint);
			break;

		case CONSTR_CHECK:
			cxt->ckconstraints = lappend(cxt->ckconstraints, constraint);
			break;

		case CONSTR_NULL:
		case CONSTR_NOTNULL:
		case CONSTR_DEFAULT:
		case CONSTR_ATTR_DEFERRABLE:
		case CONSTR_ATTR_NOT_DEFERRABLE:
		case CONSTR_ATTR_DEFERRED:
		case CONSTR_ATTR_IMMEDIATE:
			elog(ERROR, "invalid context for constraint type %d",
				 constraint->contype);
			break;

		default:
			elog(ERROR, "unrecognized constraint type: %d",
				 constraint->contype);
			break;
	}
}



/****************stmt->policy*********************/
static void
transformDistributedBy(ParseState *pstate, CreateStmtContext *cxt,
					   List *distributedBy, GpPolicy **policyp,
					   List *likeDistributedBy,
					   bool bQuiet)
{
	ListCell   *keys = NULL;
	GpPolicy  *policy = NULL;
	int			colindex = 0;
	int			maxattrs = MaxPolicyAttributeNumber;
	int			numUniqueIndexes = 0;
	Constraint *uniqueindex = NULL;

	/*
	 * utility mode creates can't have a policy.  Only the QD can have policies
	 *
	 */
	if (Gp_role != GP_ROLE_DISPATCH)
	{
		*policyp = NULL;
		return;
	}

	policy = (GpPolicy *) palloc(sizeof(GpPolicy) + maxattrs *
								 sizeof(policy->attrs[0]));
	policy->ptype = POLICYTYPE_PARTITIONED;
	policy->nattrs = 0;
	policy->attrs[0] = 1;

	/*
	 * If distributedBy is NIL, the user did not explicitly say what he
	 * wanted for a distribution policy.  So, we need to assign one.
	 */
	if (distributedBy == NIL && cxt && cxt->pkey != NULL)
	{
		/*
		 * We have a PRIMARY KEY, so let's assign the default distribution
		 * to be the key
		 */

		IndexStmt  *index = cxt->pkey;
		List	   *indexParams;
		ListCell   *ip = NULL;

		Assert(index->indexParams != NULL);
		indexParams = index->indexParams;

		foreach(ip, indexParams)
		{
			IndexElem  *iparam = lfirst(ip);

			if (iparam && iparam->name != 0)
			{

				if (distributedBy)
					distributedBy = lappend(distributedBy,
											(Node *) makeString(iparam->name));
				else
					distributedBy = list_make1((Node *) makeString(iparam->name));

			}
		}
	}

	if (cxt && cxt->ixconstraints != NULL)
	{
		ListCell   *lc = NULL;

		foreach(lc, cxt->ixconstraints)
		{
			Constraint *cons = lfirst(lc);

			if (cons->contype == CONSTR_UNIQUE)
			{
				if (uniqueindex == NULL)
					uniqueindex = cons;

				numUniqueIndexes++;

				if (cxt->pkey)
				{
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("Greenplum Database does not allow having both PRIMARY KEY and UNIQUE constraints")));
				}
			}
		}
		if (numUniqueIndexes > 1)
		{
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("Greenplum Database does not allow having multiple UNIQUE constraints")));
		}
	}

	if (distributedBy == NIL && cxt && cxt->ixconstraints != NULL &&
		numUniqueIndexes > 0)
	{
		/*
		 * No explicit distributed by clause, and no primary key.
		 * If there is a UNIQUE clause, let's use that
		 */
		ListCell   *lc = NULL;

		foreach(lc, cxt->ixconstraints)
		{

			Constraint *constraint = lfirst(lc);

			if (constraint->contype == CONSTR_UNIQUE)
			{

				ListCell   *ip = NULL;


				foreach(ip, constraint->keys)
				{
					Value	   *v = lfirst(ip);

					if (v && v->val.str != 0)
					{

						if (distributedBy)
							distributedBy = lappend(distributedBy, (Node *) makeString(v->val.str));
						else
							distributedBy = list_make1((Node *) makeString(v->val.str));

					}
				}
			}
		}

	}

	/*
	 * If new table INHERITS from one or more parent tables, check parents.
	 */
	if (cxt->inhRelations != NIL)
	{
		ListCell   *entry;

		foreach(entry, cxt->inhRelations)
		{
			RangeVar   *parent = (RangeVar *) lfirst(entry);
			Oid			relId = RangeVarGetRelid(parent, false);
			GpPolicy  *oldTablePolicy =
				GpPolicyFetch(CurrentMemoryContext, relId);

			/* Partitioned child must have partitioned parents. */
			if (oldTablePolicy == NULL ||
				 oldTablePolicy->ptype != POLICYTYPE_PARTITIONED)
			{
				ereport(ERROR, (errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
						errmsg("cannot inherit from catalog table \"%s\" "
							   "to create table \"%s\".",
							   parent->relname, cxt->relation->relname),
						errdetail("An inheritance hierarchy cannot contain a "
								  "mixture of distributed and "
								  "non-distributed tables.")));
			}
			/*
			 * If we still don't know what distribution to use, and this
			 * is an inherited table, set the distribution based on the
			 * parent (or one of the parents)
			 */
			if (distributedBy == NIL && oldTablePolicy->nattrs >= 0)
			{
				int ia;

				if (oldTablePolicy->nattrs > 0)
				{
					for (ia=0; ia<oldTablePolicy->nattrs; ia++)
					{
						char *attname =
							get_attname(relId, oldTablePolicy->attrs[ia]);

						distributedBy = lappend(distributedBy,
												(Node *) makeString(attname));
					}
				}
				else
				{
					/* strewn parent */
					distributedBy = lappend(distributedBy, (Node *)NULL);
				}
				if (!bQuiet)
				 elog(NOTICE, "Table has parent, setting distribution columns "
					 "to match parent table");
			}
			pfree(oldTablePolicy);
		}
	}

	if (distributedBy == NIL && likeDistributedBy != NIL)
	{
		distributedBy = likeDistributedBy;
		if (!bQuiet)
			elog(NOTICE, "Table doesn't have 'distributed by' clause, "
				 "defaulting to distribution columns from LIKE table");
	}

	if (gp_create_table_random_default_distribution && NIL == distributedBy)
	{
		Assert(NIL == likeDistributedBy);
		policy = createRandomDistribution(maxattrs);
		
		if (!bQuiet)
		{
			ereport(NOTICE,
				(errcode(ERRCODE_SUCCESSFUL_COMPLETION),
				 errmsg("Using default RANDOM distribution since no distribution was specified."),
				 errhint("Consider including the 'DISTRIBUTED BY' clause to determine the distribution of rows.")));
		}
	}
	else if (distributedBy == NIL)
	{
		/*
		 * if we get here, we haven't a clue what to use for the distribution columns.
		 */

		bool	assignedDefault = false;

		/*
		 * table has one or more attributes and there is still no distribution
		 * key. pick a default one. the winner is the first attribute that is
		 * an Greenplum Database-hashable data type.
		 */

		ListCell   *columns;
		ColumnDef  *column = NULL;

		/* we will distribute on at most one column */
		policy->nattrs = 1;

		colindex = 0;

		if (cxt->inhRelations)
		{
			bool		found = false;
			/* try inherited tables */
			ListCell   *inher;

			foreach(inher, cxt->inhRelations)
			{
				RangeVar   *inh = (RangeVar *) lfirst(inher);
				Relation	rel;
				int			count;

				Assert(IsA(inh, RangeVar));
				rel = heap_openrv(inh, AccessShareLock);
				if (rel->rd_rel->relkind != RELKIND_RELATION)
					ereport(ERROR,
							(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					   errmsg("inherited relation \"%s\" is not a table",
							  inh->relname)));
				for (count = 0; count < rel->rd_att->natts; count++)
				{
					Form_pg_attribute inhattr = rel->rd_att->attrs[count];
					Oid typeOid = inhattr->atttypid;

					if (inhattr->attisdropped)
						continue;
					colindex++;
					if(isGreenplumDbHashable(typeOid))
					{
						char	   *inhname = NameStr(inhattr->attname);
						policy->attrs[0] = colindex;
						assignedDefault = true;
						if (!bQuiet)
							ereport(NOTICE,
								(errcode(ERRCODE_SUCCESSFUL_COMPLETION),
								 errmsg("Table doesn't have 'DISTRIBUTED BY' clause -- Using column "
										"named '%s' from parent table as the Greenplum Database data distribution key for this "
										"table. ", inhname),
								 errhint("The 'DISTRIBUTED BY' clause determines the distribution of data."
								 		 " Make sure column(s) chosen are the optimal data distribution key to minimize skew.")));
						found = true;
						break;
					}
				}
				heap_close(rel, NoLock);

				if (assignedDefault)
					break;
			}

		}

		if (!assignedDefault)
		{
			foreach(columns, cxt->columns)
			{
				Oid typeOid;

				column = (ColumnDef *) lfirst(columns);
				colindex++;

				typeOid = typenameTypeId(NULL, column->typname);

				/*
				 * if we can hash on this type, or if it's an array type (which
				 * we can always hash) this column with be our default key.
				 */
				if(isGreenplumDbHashable(typeOid))
				{
					policy->attrs[0] = colindex;
					assignedDefault = true;
					if (!bQuiet)
						ereport(NOTICE,
							(errcode(ERRCODE_SUCCESSFUL_COMPLETION),
							 errmsg("Table doesn't have 'DISTRIBUTED BY' clause -- Using column "
									"named '%s' as the Greenplum Database data distribution key for this "
									"table. ", column->colname),
							 errhint("The 'DISTRIBUTED BY' clause determines the distribution of data."
							 		 " Make sure column(s) chosen are the optimal data distribution key to minimize skew.")));
					break;
				}

			}
		}

		if(!assignedDefault)
		{
			/*
			 * There was no eligible distribution column to default to. This table
			 * will be partitioned on an empty distribution key list. In other words
			 * tuples coming into the system will be randomly assigned a bucket.
			 */
			policy->nattrs = 0;
			if (!bQuiet)
				elog(NOTICE, "Table doesn't have 'distributed by' clause, and no column type is suitable for a distribution key. Creating a NULL policy entry.");
		}

	}
	else
	{
		/*
		 * We have a DISTRIBUTED BY column list, either specified by the user
		 * or defaulted to a primary key or unique column. Process it now and
		 * set the distribution policy.
		 */
		policy->nattrs = 0;
		if (!(distributedBy->length == 1 && linitial(distributedBy) == NULL))
		{
			foreach(keys, distributedBy)
			{
				char	   *key = strVal(lfirst(keys));
				bool		found = false;
				ColumnDef  *column = NULL;
				ListCell   *columns;

				colindex = 0;

				if (cxt->inhRelations)
				{
					/* try inherited tables */
					ListCell   *inher;

					foreach(inher, cxt->inhRelations)
					{
						RangeVar   *inh = (RangeVar *) lfirst(inher);
						Relation	rel;
						int			count;

						Assert(IsA(inh, RangeVar));
						rel = heap_openrv(inh, AccessShareLock);
						if (rel->rd_rel->relkind != RELKIND_RELATION)
							ereport(ERROR,
									(errcode(ERRCODE_WRONG_OBJECT_TYPE),
							   errmsg("inherited relation \"%s\" is not a table",
									  inh->relname)));
						for (count = 0; count < rel->rd_att->natts; count++)
						{
							Form_pg_attribute inhattr = rel->rd_att->attrs[count];
							char	   *inhname = NameStr(inhattr->attname);

							if (inhattr->attisdropped)
								continue;
							colindex++;
							if (strcmp(key, inhname) == 0)
							{
								found = true;

								break;
							}
						}
						heap_close(rel, NoLock);
						if (found)
							elog(DEBUG1, "DISTRIBUTED BY clause refers to columns of inherited table");

						if (found)
							break;
					}
				}

				if (!found)
				{
					foreach(columns, cxt->columns)
					{
						column = (ColumnDef *) lfirst(columns);
						Assert(IsA(column, ColumnDef));
						colindex++;

						if (strcmp(column->colname, key) == 0)
						{
							Oid typeOid = typenameTypeId(NULL, column->typname);

							/*
							 * To be a part of a distribution key, this type must
							 * be supported for hashing internally in Greenplum
							 * Database. We check if the base type is supported
							 * for hashing or if it is an array type (we support
							 * hashing on all array types).
							 */
							if (!isGreenplumDbHashable(typeOid))
							{
								ereport(ERROR,
										(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
										 errmsg("type \"%s\" can't be a part of a "
												"distribution key",
												format_type_be(typeOid))));
							}

							found = true;
							break;
						}
					}
				}

				/*
				* In the ALTER TABLE case, don't complain about index keys
				* not created in the command; they may well exist already.
				* DefineIndex will complain about them if not, and will also
				* take care of marking them NOT NULL.
				*/
				if (!found && !cxt->isalter)
					ereport(ERROR,
							(errcode(ERRCODE_UNDEFINED_COLUMN),
							 errmsg("column \"%s\" named in 'DISTRIBUTED BY' clause does not exist",
									key)));

				policy->attrs[policy->nattrs++] = colindex;

			}

			/* MPP-14770: we should check for duplicate column usage */
			foreach(keys, distributedBy)
			{
				char *key = strVal(lfirst(keys));

				ListCell *lkeys = NULL;
				for_each_cell (lkeys, keys->next)
				{
					char *lkey = strVal(lfirst(lkeys));
					if (strcmp(key,lkey) == 0)
						ereport(ERROR,
								(errcode(ERRCODE_DUPLICATE_COLUMN),
								 errmsg("duplicate column \"%s\" in DISTRIBUTED BY clause", key)));
				}
			}
		}

	}


	*policyp = policy;


	if (cxt && cxt->pkey)		/* Primary key	specified.	Make sure
								 * distribution columns match */
	{
		int			i = 0;
		IndexStmt  *index = cxt->pkey;
		List	   *indexParams = index->indexParams;
		ListCell   *ip;

		foreach(ip, indexParams)
		{
			IndexElem  *iparam;

			if (i >= policy->nattrs)
				break;

			iparam = lfirst(ip);
			if (iparam->name != 0)
			{
				bool	found = false;
				ColumnDef  *column = NULL;
				ListCell   *columns;

				colindex = 0;

				if (cxt->inhRelations)
				{
					/* try inherited tables */
					ListCell   *inher;

					foreach(inher, cxt->inhRelations)
					{
						RangeVar   *inh = (RangeVar *) lfirst(inher);
						Relation	rel;
						int			count;

						Assert(IsA(inh, RangeVar));
						rel = heap_openrv(inh, AccessShareLock);
						if (rel->rd_rel->relkind != RELKIND_RELATION)
							ereport(ERROR,
									(errcode(ERRCODE_WRONG_OBJECT_TYPE),
							   errmsg("inherited relation \"%s\" is not a table",
									  inh->relname)));
						for (count = 0; count < rel->rd_att->natts; count++)
						{
							Form_pg_attribute inhattr = rel->rd_att->attrs[count];
							char	   *inhname = NameStr(inhattr->attname);

							if (inhattr->attisdropped)
								continue;
							colindex++;

							if (strcmp(iparam->name, inhname) == 0)
							{
								found = true;
								break;
							}
						}
						heap_close(rel, NoLock);

						if (found)
							elog(DEBUG1, "'distributed by' clause refers to "
								 "columns of inherited table");

						if (found)
							break;
					}
				}

				if (!found)
				{
					foreach(columns, cxt->columns)
					{
						column = (ColumnDef *) lfirst(columns);
						Assert(IsA(column, ColumnDef));
						colindex++;
						if (strcmp(column->colname, iparam->name) == 0)
						{
							found = true;
							break;
						}
					}
				}
				if (colindex != policy->attrs[i])
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("PRIMARY KEY and DISTRIBUTED BY definitions incompatible"),
							 errhint("When there is both a PRIMARY KEY, and a "
									"DISTRIBUTED BY clause, the DISTRIBUTED BY "
									"clause must be equal to or a left-subset "
									"of the PRIMARY KEY")));
				}

				i++;
			}
		}
	}

	if (uniqueindex)			/* UNIQUE specified.  Make sure distribution
								 * columns match */
	{
		int			i = 0;

		List	   *keys = uniqueindex->keys;
		ListCell   *ip;

		foreach(ip, keys)
		{
			IndexElem  *iparam;

			if (i >= policy->nattrs)
				break;

			iparam = lfirst(ip);
			if (iparam->name != 0)
			{
				bool	found = false;
				ColumnDef  *column = NULL;
				ListCell   *columns;

				colindex = 0;

				if (cxt->inhRelations)
				{
					/* try inherited tables */
					ListCell   *inher;

					foreach(inher, cxt->inhRelations)
					{
						RangeVar   *inh = (RangeVar *) lfirst(inher);
						Relation	rel;
						int			count;

						Assert(IsA(inh, RangeVar));
						rel = heap_openrv(inh, AccessShareLock);
						if (rel->rd_rel->relkind != RELKIND_RELATION)
							ereport(ERROR,
									(errcode(ERRCODE_WRONG_OBJECT_TYPE),
							   errmsg("inherited relation \"%s\" is not a table",
									  inh->relname)));
						for (count = 0; count < rel->rd_att->natts; count++)
						{
							Form_pg_attribute inhattr = rel->rd_att->attrs[count];
							char	   *inhname = NameStr(inhattr->attname);

							if (inhattr->attisdropped)
								continue;
							colindex++;

							if (strcmp(iparam->name, inhname) == 0)
							{
								found = true;

								break;
							}
						}
						heap_close(rel, NoLock);
						if (found)
							elog(NOTICE, "'distributed by' clause refers to columns of inherited table");

						if (found)
							break;
					}
				}

				if (!found)
				foreach(columns, cxt->columns)
				{
					column = (ColumnDef *) lfirst(columns);
					Assert(IsA(column, ColumnDef));
					colindex++;
					if (strcmp(column->colname, iparam->name) == 0)
					{
						found = true;
						break;
					}
				}

				if (colindex != policy->attrs[i])
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("UNIQUE constraint and DISTRIBUTED BY "
									"definitions incompatible"),
							 errhint("When there is both a UNIQUE constraint, "
									 "and a DISTRIBUTED BY clause, the "
									 "DISTRIBUTED BY clause must be equal to "
									 "or a left-subset of the UNIQUE columns")));
				}
				i++;
			}
		}
	}
}

/*
 * Add any missing encoding attributes (compresstype = none,
 * blocksize=...).  The column specific encoding attributes supported
 * today are compresstype, compresslevel and blocksize.  Refer to
 * pg_compression.c for more info.
 */
static List *
fillin_encoding(List *list)
{
	bool foundCompressType = false;
	bool foundCompressTypeNone = false;
	char *cmplevel = NULL;
	bool foundBlockSize = false;
	char *arg;
	List *retList = list_copy(list);
	ListCell *lc;
	DefElem *el;
	const StdRdOptions *ao_opts = currentAOStorageOptions();

	foreach(lc, list)
	{
		el = lfirst(lc);

		if (pg_strcasecmp("compresstype", el->defname) == 0)
		{
			foundCompressType = true;
			arg = defGetString(el);
			if (pg_strcasecmp("none", arg) == 0)
				foundCompressTypeNone = true;
		}
		else if (pg_strcasecmp("compresslevel", el->defname) == 0)
		{
			cmplevel = defGetString(el);
		}
		else if (pg_strcasecmp("blocksize", el->defname) == 0)
			foundBlockSize = true;
	}

	if (foundCompressType == false && cmplevel == NULL)
	{
		/* No compression option specified, use current defaults. */
		arg = ao_opts->compresstype ?
				pstrdup(ao_opts->compresstype) : "none";
		el = makeDefElem("compresstype", (Node *) makeString(arg));
		retList = lappend(retList, el);
		el = makeDefElem("compresslevel",
						 (Node *) makeInteger(ao_opts->compresslevel));
		retList = lappend(retList, el);
	}
	else if (foundCompressType == false && cmplevel)
	{
		if (strcmp(cmplevel, "0") == 0)
		{
			/*
			 * User wants to disable compression by specifying
			 * compresslevel=0.
			 */
			el = makeDefElem("compresstype", (Node *) makeString("none"));
			retList = lappend(retList, el);
		}
		else
		{
			/*
			 * User wants to enable compression by specifying non-zero
			 * compresslevel.  Therefore, choose default compresstype
			 * if configured, otherwise use zlib.
			 */
			if (ao_opts->compresstype &&
				strcmp(ao_opts->compresstype, "none") != 0)
			{
				arg = pstrdup(ao_opts->compresstype);
			}
			else
			{
				arg = AO_DEFAULT_COMPRESSTYPE;
			}
			el = makeDefElem("compresstype", (Node *) makeString(arg));
			retList = lappend(retList, el);
		}
	}
	else if (foundCompressType && cmplevel == NULL)
	{
		if (foundCompressTypeNone)
		{
			/*
			 * User wants to disable compression by specifying
			 * compresstype=none.
			 */
			el = makeDefElem("compresslevel", (Node *) makeInteger(0));
			retList = lappend(retList, el);
		}
		else
		{
			/*
			 * Valid compresstype specified.  Use default
			 * compresslevel if it's non-zero, otherwise use 1.
			 */
			el = makeDefElem("compresslevel",
							 (Node *) makeInteger(ao_opts->compresslevel > 0 ?
												  ao_opts->compresslevel : 1));
			retList = lappend(retList, el);
		}
	}
	if (foundBlockSize == false)
	{
		el = makeDefElem("blocksize", (Node *) makeInteger(ao_opts->blocksize));
		retList = lappend(retList, el);
	}
	return retList;
}

static void
transformFKConstraints(ParseState *pstate, CreateStmtContext *cxt,
					   bool skipValidation, bool isAddConstraint)
{
	ListCell   *fkclist;

	if (cxt->fkconstraints == NIL)
		return;

	/*
	 * If CREATE TABLE or adding a column with NULL default, we can safely
	 * skip validation of the constraint.
	 */
	if (skipValidation)
	{
		foreach(fkclist, cxt->fkconstraints)
		{
			FkConstraint *fkconstraint = (FkConstraint *) lfirst(fkclist);

			fkconstraint->skip_validation = true;
		}
	}

	/*
	 * For CREATE TABLE or ALTER TABLE ADD COLUMN, gin up an ALTER TABLE ADD
	 * CONSTRAINT command to execute after the basic command is complete. (If
	 * called from ADD CONSTRAINT, that routine will add the FK constraints to
	 * its own subcommand list.)
	 *
	 * Note: the ADD CONSTRAINT command must also execute after any index
	 * creation commands.  Thus, this should run after
	 * transformIndexConstraints, so that the CREATE INDEX commands are
	 * already in cxt->alist.
	 */
	if (!isAddConstraint)
	{
		AlterTableStmt *alterstmt = makeNode(AlterTableStmt);

		alterstmt->relation = cxt->relation;
		alterstmt->cmds = NIL;
		alterstmt->relkind = OBJECT_TABLE;

		foreach(fkclist, cxt->fkconstraints)
		{
			FkConstraint *fkconstraint = (FkConstraint *) lfirst(fkclist);
			AlterTableCmd *altercmd = makeNode(AlterTableCmd);

			altercmd->subtype = AT_ProcessedConstraint;
			altercmd->name = NULL;
			altercmd->def = (Node *) fkconstraint;
			alterstmt->cmds = lappend(alterstmt->cmds, altercmd);
		}

		cxt->alist = lappend(cxt->alist, alterstmt);
	}
}

/*
 * transformIndexStmt -
 *	  transforms the qualification of the index statement
 *
 * If do_part is true, build create index statements for our children.
 */
static Query *
transformIndexStmt(ParseState *pstate, IndexStmt *stmt,
				   List **extras_before, List **extras_after)
{
	Query	   *qry;
	RangeTblEntry *rte = NULL;
	ListCell   *l;
	Oid			idxOid;
	Oid			nspOid;

	qry = makeNode(Query);
	qry->commandType = CMD_UTILITY;

	/*
	 * If the table already exists (i.e., this isn't a create table time
	 * expansion of primary key() or unique()) and we're the ultimate parent
	 * of a partitioned table, cascade to all children. We don't do this
	 * at create table time because transformPartitionBy() automatically
	 * creates the indexes on the child tables for us.
	 *
	 * If this is a CREATE INDEX statement, idxname should already exist.
	 */
	idxOid = RangeVarGetRelid(stmt->relation, true);
	nspOid = RangeVarGetCreationNamespace(stmt->relation);
	if (OidIsValid(idxOid) && stmt->idxname)
	{
		Relation rel;

		rel = heap_openrv(stmt->relation, AccessShareLock);

		if (RelationBuildPartitionDesc(rel, false))
			stmt->do_part = true;

		if (stmt->do_part && Gp_role != GP_ROLE_EXECUTE)
		{
			List		*children;
			struct HTAB *nameCache;

			/* Lookup the parser object name cache */
			nameCache = parser_get_namecache(pstate);

			/* Loop over all partition children */
			children = find_inheritance_children(RelationGetRelid(rel));

			foreach(l, children)
			{
				Oid relid = lfirst_oid(l);
				Relation crel = heap_open(relid, NoLock); /* lock on master
															 is enough */
				if (RelationIsExternal(crel))
				{
					elog(NOTICE, "skip building index for external partition \"%s\"",
						 RelationGetRelationName(crel));
					heap_close(crel, NoLock);
					continue;
				}
				IndexStmt *chidx;
				Relation partrel;
				HeapTuple tuple;
				cqContext	cqc;
				char *parname;
				int2 position;
				int4 depth;
				NameData name;
				Oid paroid;
				char depthstr[NAMEDATALEN];
				char prtstr[NAMEDATALEN];

				chidx = (IndexStmt *)copyObject((Node *)stmt);

				/* now just update the relation and index name fields */
				chidx->relation =
					makeRangeVar(get_namespace_name(RelationGetNamespace(crel)),
								 pstrdup(RelationGetRelationName(crel)), -1);

				elog(NOTICE, "building index for child partition \"%s\"",
					 RelationGetRelationName(crel));
				/*
				 * We want the index name to resemble our partition table name
				 * with the master index name on the front. This means, we
				 * append to the indexname the parname, position, and depth
				 * as we do in transformPartitionBy().
				 *
				 * So, firstly we must retrieve from pg_partition_rule the
				 * partition descriptor for the current relid. This gives us
				 * partition name and position. With paroid, we can get the
				 * partition level descriptor from pg_partition and therefore
				 * our depth.
				 */
				partrel = heap_open(PartitionRuleRelationId, AccessShareLock);

				tuple = caql_getfirst(
						caql_addrel(cqclr(&cqc), partrel), 
						cql("SELECT * FROM pg_partition_rule "
							" WHERE parchildrelid = :1 ",
							ObjectIdGetDatum(relid)));

				Assert(HeapTupleIsValid(tuple));

				name = ((Form_pg_partition_rule)GETSTRUCT(tuple))->parname;
				parname = pstrdup(NameStr(name));
				position = ((Form_pg_partition_rule)GETSTRUCT(tuple))->parruleord;
				paroid = ((Form_pg_partition_rule)GETSTRUCT(tuple))->paroid;

				heap_freetuple(tuple);
				heap_close(partrel, NoLock);

				partrel = heap_open(PartitionRelationId, AccessShareLock);

				tuple = caql_getfirst(
						caql_addrel(cqclr(&cqc), partrel), 
						cql("SELECT parlevel FROM pg_partition "
							" WHERE oid = :1 ",
							ObjectIdGetDatum(paroid)));

				Assert(HeapTupleIsValid(tuple));

				depth = ((Form_pg_partition)GETSTRUCT(tuple))->parlevel + 1;

				heap_freetuple(tuple);
				heap_close(partrel, NoLock);

				heap_close(crel, NoLock);

				/* now, build the piece to append */
				snprintf(depthstr, sizeof(depthstr), "%d", depth);
				if (strlen(parname) == 0)
					snprintf(prtstr, sizeof(prtstr), "prt_%d", position);
				else
					snprintf(prtstr, sizeof(prtstr), "prt_%s", parname);

				chidx->idxname = ChooseRelationNameWithCache(stmt->idxname,
													depthstr, /* depth */
													prtstr,   /* part spec */
												    nspOid,
													nameCache);

				*extras_after = lappend(*extras_after, chidx);
			}
		}

		heap_close(rel, AccessShareLock);
	}

	/* take care of the where clause */
	if (stmt->whereClause)
	{
		/*
		 * Put the parent table into the rtable so that the WHERE clause can
		 * refer to its fields without qualification.  Note that this only
		 * works if the parent table already exists --- so we can't easily
		 * support predicates on indexes created implicitly by CREATE TABLE.
		 * Fortunately, that's not necessary.
		 */
		rte = addRangeTableEntry(pstate, stmt->relation, NULL, false, true);

		/* no to join list, yes to namespaces */
		addRTEtoQuery(pstate, rte, false, true, true);

		stmt->whereClause = transformWhereClause(pstate, stmt->whereClause,
												 "WHERE");
	}

	/* take care of any index expressions */
	foreach(l, stmt->indexParams)
	{
		IndexElem  *ielem = (IndexElem *) lfirst(l);

		if (ielem->expr)
		{
			/* Set up rtable as for predicate, see notes above */
			if (rte == NULL)
			{
				rte = addRangeTableEntry(pstate, stmt->relation, NULL,
										 false, true);
				/* no to join list, yes to namespaces */
				addRTEtoQuery(pstate, rte, false, true, true);
			}
			ielem->expr = transformExpr(pstate, ielem->expr);

			/*
			 * We check only that the result type is legitimate; this is for
			 * consistency with what transformWhereClause() checks for the
			 * predicate.  DefineIndex() will make more checks.
			 */
			if (expression_returns_set(ielem->expr))
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("index expression cannot return a set")));
		}
	}

	qry->hasSubLinks = pstate->p_hasSubLinks;
	stmt->rangetable = pstate->p_rtable;

	qry->utilityStmt = (Node *) stmt;

	return qry;
}

/*
 * transformRuleStmt -
 *	  transform a Create Rule Statement. The actions is a list of parse
 *	  trees which is transformed into a list of query trees.
 */
static Query *
transformRuleStmt(ParseState *pstate, RuleStmt *stmt,
				  List **extras_before, List **extras_after)
{
	Query	   *qry;
	Relation	rel;
	RangeTblEntry *oldrte;
	RangeTblEntry *newrte;

	qry = makeNode(Query);
	qry->commandType = CMD_UTILITY;
	qry->utilityStmt = (Node *) stmt;

	/*
	 * To avoid deadlock, make sure the first thing we do is grab
	 * AccessExclusiveLock on the target relation.	This will be needed by
	 * DefineQueryRewrite(), and we don't want to grab a lesser lock
	 * beforehand.
	 */
	rel = heap_openrv(stmt->relation, AccessExclusiveLock);

	/*
	 * NOTE: 'OLD' must always have a varno equal to 1 and 'NEW' equal to 2.
	 * Set up their RTEs in the main pstate for use in parsing the rule
	 * qualification.
	 */
	Assert(pstate->p_rtable == NIL);
	oldrte = addRangeTableEntryForRelation(pstate, rel,
										   makeAlias("*OLD*", NIL),
										   false, false);
	newrte = addRangeTableEntryForRelation(pstate, rel,
										   makeAlias("*NEW*", NIL),
										   false, false);
	/* Must override addRangeTableEntry's default access-check flags */
	oldrte->requiredPerms = 0;
	newrte->requiredPerms = 0;

	/*
	 * They must be in the namespace too for lookup purposes, but only add the
	 * one(s) that are relevant for the current kind of rule.  In an UPDATE
	 * rule, quals must refer to OLD.field or NEW.field to be unambiguous, but
	 * there's no need to be so picky for INSERT & DELETE.  We do not add them
	 * to the joinlist.
	 */
	switch (stmt->event)
	{
		case CMD_SELECT:
			addRTEtoQuery(pstate, oldrte, false, true, true);
			break;
		case CMD_UPDATE:
			addRTEtoQuery(pstate, oldrte, false, true, true);
			addRTEtoQuery(pstate, newrte, false, true, true);
			break;
		case CMD_INSERT:
			addRTEtoQuery(pstate, newrte, false, true, true);
			break;
		case CMD_DELETE:
			addRTEtoQuery(pstate, oldrte, false, true, true);
			break;
		default:
			elog(ERROR, "unrecognized event type: %d",
				 (int) stmt->event);
			break;
	}

	/* take care of the where clause */
	stmt->whereClause = transformWhereClause(pstate, stmt->whereClause,
											 "WHERE");

    /* CDB: Cursor position not available for errors below this point. */
    pstate->p_breadcrumb.node = NULL;

	if (list_length(pstate->p_rtable) != 2)		/* naughty, naughty... */
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				 errmsg("rule WHERE condition cannot contain references to other relations")));

	/* aggregates not allowed (but subselects are okay) */
	if (pstate->p_hasAggs)
		ereport(ERROR,
				(errcode(ERRCODE_GROUPING_ERROR),
		   errmsg("cannot use aggregate function in rule WHERE condition")));

	if (pstate->p_hasWindFuncs)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("cannot use window function in rule WHERE condition")));


	/* save info about sublinks in where clause */
	qry->hasSubLinks = pstate->p_hasSubLinks;

	/*
	 * 'instead nothing' rules with a qualification need a query rangetable so
	 * the rewrite handler can add the negated rule qualification to the
	 * original query. We create a query with the new command type CMD_NOTHING
	 * here that is treated specially by the rewrite system.
	 */
	if (stmt->actions == NIL)
	{
		Query	   *nothing_qry = makeNode(Query);

		nothing_qry->commandType = CMD_NOTHING;
		nothing_qry->rtable = pstate->p_rtable;
		nothing_qry->jointree = makeFromExpr(NIL, NULL);		/* no join wanted */

		stmt->actions = list_make1(nothing_qry);
	}
	else
	{
		ListCell   *l;
		List	   *newactions = NIL;

		/*
		 * transform each statement, like parse_sub_analyze()
		 */
		foreach(l, stmt->actions)
		{
			Node	   *action = (Node *) lfirst(l);
			ParseState *sub_pstate = make_parsestate(pstate->parentParseState);
			Query	   *sub_qry,
					   *top_subqry;
			bool		has_old,
						has_new;

			/*
			 * Set up OLD/NEW in the rtable for this statement.  The entries
			 * are added only to relnamespace, not varnamespace, because we
			 * don't want them to be referred to by unqualified field names
			 * nor "*" in the rule actions.  We decide later whether to put
			 * them in the joinlist.
			 */
			oldrte = addRangeTableEntryForRelation(sub_pstate, rel,
												   makeAlias("*OLD*", NIL),
												   false, false);
			newrte = addRangeTableEntryForRelation(sub_pstate, rel,
												   makeAlias("*NEW*", NIL),
												   false, false);
			oldrte->requiredPerms = 0;
			newrte->requiredPerms = 0;
			addRTEtoQuery(sub_pstate, oldrte, false, true, false);
			addRTEtoQuery(sub_pstate, newrte, false, true, false);

			/* Transform the rule action statement */
			top_subqry = transformStmt(sub_pstate, action,
									   extras_before, extras_after);

			/*
			 * We cannot support utility-statement actions (eg NOTIFY) with
			 * nonempty rule WHERE conditions, because there's no way to make
			 * the utility action execute conditionally.
			 */
			if (top_subqry->commandType == CMD_UTILITY &&
				stmt->whereClause != NULL)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
						 errmsg("rules with WHERE conditions can only have SELECT, INSERT, UPDATE, or DELETE actions")));

			/*
			 * If the action is INSERT...SELECT, OLD/NEW have been pushed down
			 * into the SELECT, and that's what we need to look at. (Ugly
			 * kluge ... try to fix this when we redesign querytrees.)
			 */
			sub_qry = getInsertSelectQuery(top_subqry, NULL);

			/*
			 * If the sub_qry is a setop, we cannot attach any qualifications
			 * to it, because the planner won't notice them.  This could
			 * perhaps be relaxed someday, but for now, we may as well reject
			 * such a rule immediately.
			 */
			if (sub_qry->setOperations != NULL && stmt->whereClause != NULL)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("conditional UNION/INTERSECT/EXCEPT statements are not implemented")));

			/*
			 * Validate action's use of OLD/NEW, qual too
			 */
			has_old =
				rangeTableEntry_used((Node *) sub_qry, PRS2_OLD_VARNO, 0) ||
				rangeTableEntry_used(stmt->whereClause, PRS2_OLD_VARNO, 0);
			has_new =
				rangeTableEntry_used((Node *) sub_qry, PRS2_NEW_VARNO, 0) ||
				rangeTableEntry_used(stmt->whereClause, PRS2_NEW_VARNO, 0);

			switch (stmt->event)
			{
				case CMD_SELECT:
					if (has_old)
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
								 errmsg("ON SELECT rule cannot use OLD")));
					if (has_new)
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
								 errmsg("ON SELECT rule cannot use NEW")));
					break;
				case CMD_UPDATE:
					/* both are OK */
					break;
				case CMD_INSERT:
					if (has_old)
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
								 errmsg("ON INSERT rule cannot use OLD")));
					break;
				case CMD_DELETE:
					if (has_new)
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
								 errmsg("ON DELETE rule cannot use NEW")));
					break;
				default:
					elog(ERROR, "unrecognized event type: %d",
						 (int) stmt->event);
					break;
			}

			/*
			 * For efficiency's sake, add OLD to the rule action's jointree
			 * only if it was actually referenced in the statement or qual.
			 *
			 * For INSERT, NEW is not really a relation (only a reference to
			 * the to-be-inserted tuple) and should never be added to the
			 * jointree.
			 *
			 * For UPDATE, we treat NEW as being another kind of reference to
			 * OLD, because it represents references to *transformed* tuples
			 * of the existing relation.  It would be wrong to enter NEW
			 * separately in the jointree, since that would cause a double
			 * join of the updated relation.  It's also wrong to fail to make
			 * a jointree entry if only NEW and not OLD is mentioned.
			 */
			if (has_old || (has_new && stmt->event == CMD_UPDATE))
			{
				/*
				 * If sub_qry is a setop, manipulating its jointree will do no
				 * good at all, because the jointree is dummy. (This should be
				 * a can't-happen case because of prior tests.)
				 */
				if (sub_qry->setOperations != NULL)
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("conditional UNION/INTERSECT/EXCEPT statements are not implemented")));
				/* hack so we can use addRTEtoQuery() */
				sub_pstate->p_rtable = sub_qry->rtable;
				sub_pstate->p_joinlist = sub_qry->jointree->fromlist;
				addRTEtoQuery(sub_pstate, oldrte, true, false, false);
				sub_qry->jointree->fromlist = sub_pstate->p_joinlist;
			}

			newactions = lappend(newactions, top_subqry);

			release_pstate_resources(sub_pstate);
			free_parsestate(sub_pstate);
		}

		stmt->actions = newactions;
	}

	/* Close relation, but keep the exclusive lock */
	heap_close(rel, NoLock);

	return qry;
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
 * (Var, Aggref, and WindowRef nodes) in Q'' and below.  At the end,
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

	if ( !qry->hasWindFuncs || !(qry->groupClause || qry->hasAggs) )
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
	subq->hasWindFuncs = false; /* reevaluate later */
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
	 * we must mark hasWindFuncs in the sub query as well.
	 */
	if (checkExprHasWindFuncs((Node *)subq->jointree))
		subq->hasWindFuncs = true;

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
	ListCell *lc = NULL;
	Index maxsgr = 0;

	get_sortgroupclauses_tles(qry->groupClause, qry->targetList,
							  &grp_tles, &grp_sortops);
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

	else if (IsA(node, GroupClause))
	{
		GroupClause *g = (GroupClause*)node;
		GroupClause *new_g = makeNode(GroupClause);
		memcpy(new_g, g, sizeof(GroupClause));
		new_g->tleSortGroupRef = ctx->sgr_map[g->tleSortGroupRef];
		return (Node*)new_g;
	}

	/* Just like above, but don't assume identical */
	else if (IsA(node, SortClause))
	{
	SortClause *s = (SortClause*)node;
		 SortClause *new_s = makeNode(SortClause);
		 memcpy(new_s, s, sizeof(SortClause));
		 new_s->tleSortGroupRef = ctx->sgr_map[s->tleSortGroupRef];
		 return (Node*)new_s;
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
			 IsA(node, PercentileExpr) ||
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
					 errhint("You may need to use explicit aliases and/or to refer to grouping "
							 "keys in the same way throughout the query.")));
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
				 errmsg("can't generate internal attribute name"),
				 errOmitLocation(true)));
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
 * alias.  If not, we need to search the range table for occurances
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
 * Note: this is also used for DECLARE CURSOR statements.
 */
static Query *
transformSelectStmt(ParseState *pstate, SelectStmt *stmt)
{
	Query	   *qry = makeNode(Query);
	Node	   *qual;
	ListCell   *l;

	qry->commandType = CMD_SELECT;

	/* process the WITH clause */
	if (stmt->withClause != NULL)
	{
		qry->hasRecursive = stmt->withClause->recursive;
		qry->cteList = transformWithClause(pstate, stmt->withClause);
		qry->hasModifyingCTE = pstate->p_hasModifyingCTE;
	}

	/* make FOR UPDATE/FOR SHARE info available to addRangeTableEntry */
	pstate->p_locking_clause = stmt->lockingClause;

	/*
	 * Put WINDOW clause data into pstate so that window references know
	 * about them.
	 */
	pstate->p_win_clauses = stmt->windowClause;

	/* process the FROM clause */
	transformFromClause(pstate, stmt->fromClause);

	/* tidy up expressions in window clauses */
	transformWindowSpecExprs(pstate);

	/* transform targetlist */
	qry->targetList = transformTargetList(pstate, stmt->targetList);

	/* mark column origins */
	markTargetListOrigins(pstate, qry->targetList);

	/* transform WHERE */
	qual = transformWhereClause(pstate, stmt->whereClause, "WHERE");

	/*
	 * Initial processing of HAVING clause is just like WHERE clause.
	 */
	pstate->having_qual = transformWhereClause(pstate, stmt->havingClause,
										   "HAVING");

    /*
     * CDB: Untyped Const or Param nodes in a subquery in the FROM clause
     * might have been assigned proper types when we transformed the WHERE
     * clause, targetlist, etc.  Bring targetlist Var types up to date.
     */
    fixup_unknown_vars_in_targetlist(pstate, qry->targetList);

	/*
	 * Transform sorting/grouping stuff.  Do ORDER BY first because both
	 * transformGroupClause and transformDistinctClause need the results.
	 */
	qry->sortClause = transformSortClause(pstate,
										  stmt->sortClause,
										  &qry->targetList,
										  true, /* fix unknowns */
                                          false /* use SQL92 rules */);

	qry->groupClause = transformGroupClause(pstate,
											stmt->groupClause,
											&qry->targetList,
											qry->sortClause,
                                            false /* useSQL92 rules */);

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

    /* Having clause */
	qry->havingQual = pstate->having_qual;
	pstate->having_qual = NULL;

	/*
	 * Process WINDOW clause.
	 */
	transformWindowClause(pstate, qry);

	qry->distinctClause = transformDistinctClause(pstate,
												  stmt->distinctClause,
												  &qry->targetList,
												  &qry->sortClause,
												  &qry->groupClause);

	qry->limitOffset = transformLimitClause(pstate, stmt->limitOffset,
											"OFFSET");
	qry->limitCount = transformLimitClause(pstate, stmt->limitCount,
										   "LIMIT");

    /* CDB: Cursor position not available for errors below this point. */
    pstate->p_breadcrumb.node = NULL;

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
	qry->hasAggs = pstate->p_hasAggs;
	if (pstate->p_hasAggs || qry->groupClause || qry->havingQual)
		parseCheckAggregates(pstate, qry);

	if (pstate->p_hasTblValueExpr)
		parseCheckTableFunctions(pstate, qry);

	qry->hasWindFuncs = pstate->p_hasWindFuncs;
	if (pstate->p_hasWindFuncs)
		parseProcessWindFuncs(pstate, qry);


	foreach(l, stmt->lockingClause)
	{
		transformLockingClause(qry, (LockingClause *) lfirst(l));
	}

	/*
	 * If the query mixes window functions and aggregates, we need to
	 * transform it such that the grouped query appears as a subquery
	 */
	if (qry->hasWindFuncs && (qry->groupClause || qry->hasAggs))
		transformGroupedWindows(qry);

	return qry;
}

/*
 * transformValuesClause -
 *	  transforms a VALUES clause that's being used as a standalone SELECT
 *
 * We build a Query containing a VALUES RTE, rather as if one had written
 *			SELECT * FROM (VALUES ...)
 */
static Query *
transformValuesClause(ParseState *pstate, SelectStmt *stmt)
{
	Query	   *qry = makeNode(Query);
	List	   *exprsLists = NIL;
	List	  **coltype_lists = NULL;
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

	/* process the WITH clause independently of all else */
	if (stmt->withClause != NULL)
	{
		qry->hasRecursive = stmt->withClause->recursive;
		qry->cteList = transformWithClause(pstate, stmt->withClause);
		qry->hasModifyingCTE = pstate->p_hasModifyingCTE;
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
		sublist = transformExpressionList(pstate, sublist);

		/*
		 * All the sublists must be the same length, *after* transformation
		 * (which might expand '*' into multiple items).  The VALUES RTE can't
		 * handle anything different.
		 */
		if (sublist_length < 0)
		{
			/* Remember post-transformation length of first sublist */
			sublist_length = list_length(sublist);
			/* and allocate arrays for column-type info */
			coltype_lists = (List **) palloc0(sublist_length * sizeof(List *));
			coltypes = (Oid *) palloc0(sublist_length * sizeof(Oid));
		}
		else if (sublist_length != list_length(sublist))
		{
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("VALUES lists must all be the same length")));
		}

		exprsLists = lappend(exprsLists, sublist);

		i = 0;
		foreach(lc2, sublist)
		{
			Node	   *col = (Node *) lfirst(lc2);

			if (IsA(col, SetToDefault))
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("DEFAULT can only appear in a VALUES list within INSERT")));
			coltype_lists[i] = lappend_oid(coltype_lists[i], exprType(col));
			i++;
		}
	}

	/*
	 * Now resolve the common types of the columns, and coerce everything to
	 * those types.
	 */
	for (i = 0; i < sublist_length; i++)
	{
		coltypes[i] = select_common_type(coltype_lists[i], "VALUES");
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
										  true, /* fix unknowns */
                                          false /* use SQL92 rules */);

	qry->limitOffset = transformLimitClause(pstate, stmt->limitOffset,
											"OFFSET");
	qry->limitCount = transformLimitClause(pstate, stmt->limitCount,
										   "LIMIT");

    /* CDB: Cursor position not available for errors below this point. */
    pstate->p_breadcrumb.node = NULL;

	if (stmt->lockingClause)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("SELECT FOR UPDATE/SHARE cannot be applied to VALUES")));

	if (stmt->distributedBy && Gp_role == GP_ROLE_DISPATCH)
		setQryDistributionPolicy(stmt, qry);

	/* handle any CREATE TABLE AS spec */
	qry->intoClause = NULL;
	if (stmt->intoClause)
	{
		qry->intoClause = stmt->intoClause;
		if (stmt->intoClause->colNames)
			applyColumnNames(qry->targetList, stmt->intoClause->colNames);
	}

	/*
	 * There mustn't have been any table references in the expressions, else
	 * strange things would happen, like Cartesian products of those tables
	 * with the VALUES list.  We have to check this after parsing ORDER BY et
	 * al since those could insert more junk.
	 */
	if (list_length(pstate->p_joinlist) != 1)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("VALUES must not contain table references")));

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
				 errhint("Use SELECT ... UNION ALL ... instead.")));

	qry->rtable = pstate->p_rtable;
	qry->jointree = makeFromExpr(pstate->p_joinlist, NULL);

	qry->hasSubLinks = pstate->p_hasSubLinks;
	/* aggregates not allowed (but subselects are okay) */
	if (pstate->p_hasAggs)
		ereport(ERROR,
				(errcode(ERRCODE_GROUPING_ERROR),
				 errmsg("cannot use aggregate function in VALUES")));

	if (pstate->p_hasWindFuncs)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("cannot use window function in VALUES")));

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
			   *sv_varnamespace,
			   *sv_rtable;
	RangeTblEntry *jrte;
	int			tllen;
	List	   *colTypes, *colTypmods;

	qry->commandType = CMD_SELECT;

	/* process the WITH clause independently of all else */
	if (stmt->withClause != NULL)
	{
		qry->hasRecursive = stmt->withClause->recursive;
		qry->cteList = transformWithClause(pstate, stmt->withClause);
		qry->hasModifyingCTE = pstate->p_hasModifyingCTE;
	}

	/*
	 * Find leftmost leaf SelectStmt; extract the one-time-only items from it
	 * and from the top-level node.  (Most of the INTO options can be
	 * transferred to the Query immediately, but intoColNames has to be saved
	 * to apply below.)
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

	stmt->sortClause = NIL;
	stmt->limitOffset = NULL;
	stmt->limitCount = NULL;
	stmt->lockingClause = NIL;

	/* We don't support FOR UPDATE/SHARE with set ops at the moment. */
	if (lockingClause)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("SELECT FOR UPDATE/SHARE is not allowed with UNION/INTERSECT/EXCEPT")));

	/*
	 * Before transforming the subtrees, we collect all the data types
	 * and typmods by searching their targetList (ResTarget) or valuesClause.
	 * This is necessary because choosing column types in leaf query
	 * without knowing whole of tree may result in a wrong type. And this
	 * situation goes an error that is against user's instinct. Instead,
	 * we want to look at all the leavs at one time, and decide each column
	 * types. The resjunk columns are not interesting, because type coercion
	 * between queries is done only for each non-resjunk column in set operations.
	 */
	colTypes = NIL;
	colTypmods = NIL;
	pstate->p_setopTypes = NIL;
	pstate->p_setopTypmods = NIL;
	collectSetopTypes(pstate, stmt, &colTypes, &colTypmods);
	Insist(list_length(colTypes) == list_length(colTypmods));
	forboth (lct, colTypes, lcm, colTypmods)
	{
		List	   *types = (List *) lfirst(lct);
		List	   *typmods = (List *) lfirst(lcm);
		ListCell   *lct2, *lcm2;
		Oid			ptype;
		int32		ptypmod;
		Oid			restype;
		int32		restypmod;
		bool		allsame, hasnontext;
		char	   *context;

		Insist(list_length(types) == list_length(typmods));

		context = (stmt->op == SETOP_UNION ? "UNION" :
				   stmt->op == SETOP_INTERSECT ? "INTERSECT" :
				   "EXCEPT");
		allsame = true;
		hasnontext = false;
		ptype = linitial_oid(types);
		ptypmod = linitial_int(typmods);
		forboth (lct2, types, lcm2, typmods)
		{
			Oid		ntype = lfirst_oid(lct2);
			int32	ntypmod = lfirst_int(lcm2);

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
				STRING_TYPE != TypeCategory(getBaseType(ntype)))
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
			restype = select_common_type(types, context);
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

		pstate->p_setopTypes = lappend_oid(pstate->p_setopTypes, restype);
		pstate->p_setopTypmods = lappend_int(pstate->p_setopTypmods, restypmod);
		pstate->p_propagateSetopTypes = true; /* once p_setopTypes are set, we allow pstate to propagate setop types */
	}

	/*
	 * Recursively transform the components of the tree.
	 */
	sostmt = (SetOperationStmt *) transformSetOperationTree(pstate, stmt);
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
		Expr	   *expr;

		Assert(!lefttle->resjunk);
		colName = pstrdup(lefttle->resname);
		expr = (Expr *) makeVar(leftmostRTI,
								lefttle->resno,
								colType,
								colTypmod,
								0);
		tle = makeTargetEntry(expr,
							  (AttrNumber) pstate->p_next_resno++,
							  colName,
							  false);
		qry->targetList = lappend(qry->targetList, tle);
		targetvars = lappend(targetvars, expr);
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
	jrte = addRangeTableEntryForJoin(NULL,
									 targetnames,
									 JOIN_INNER,
									 targetvars,
									 NULL,
									 false);

	sv_rtable = pstate->p_rtable;
	pstate->p_rtable = list_make1(jrte);

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
										  false /* no unknowns expected */,
                                          false /* use SQL92 rules */ );

	pstate->p_rtable = sv_rtable;
	pstate->p_relnamespace = sv_relnamespace;
	pstate->p_varnamespace = sv_varnamespace;

	if (tllen != list_length(qry->targetList))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("invalid UNION/INTERSECT/EXCEPT ORDER BY clause"),
				 errdetail("Only result column names can be used, not expressions or functions."),
				 errhint("Add the expression/function to every SELECT, or move the UNION into a FROM clause.")));

	qry->limitOffset = transformLimitClause(pstate, limitOffset,
											"OFFSET");
	qry->limitCount = transformLimitClause(pstate, limitCount,
										   "LIMIT");

    /* CDB: Cursor position not available for errors below this point. */
    pstate->p_breadcrumb.node = NULL;

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
	qry->hasAggs = pstate->p_hasAggs;
	if (pstate->p_hasAggs || qry->groupClause || qry->havingQual)
		parseCheckAggregates(pstate, qry);

	if (pstate->p_hasTblValueExpr)
		parseCheckTableFunctions(pstate, qry);

	foreach(l, lockingClause)
	{
		transformLockingClause(qry, (LockingClause *) lfirst(l));
	}

	return qry;
}

/*
 * transformSetOperationTree
 *		Recursively transform leaves and internal nodes of a set-op tree
 */
static Node *
transformSetOperationTree(ParseState *pstate, SelectStmt *stmt)
{
	Assert(stmt && IsA(stmt, SelectStmt));

	/*
	 * Validity-check both leaf and internal SELECTs for disallowed ops.
	 */
	if (stmt->intoClause)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("INTO is only allowed on first SELECT of UNION/INTERSECT/EXCEPT")));
	/* We don't support FOR UPDATE/SHARE with set ops at the moment. */
	if (stmt->lockingClause)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("SELECT FOR UPDATE/SHARE is not allowed with UNION/INTERSECT/EXCEPT")));

	if (isSetopLeaf(stmt))
	{
		/* Process leaf SELECT */
		List	   *selectList;
		Query	   *selectQuery;
		char		selectName[32];
		RangeTblEntry *rte;
		RangeTblRef *rtr;

		/*
		 * Transform SelectStmt into a Query.
		 *
		 * Note: previously transformed sub-queries don't affect the parsing
		 * of this sub-query, because they are not in the toplevel pstate's
		 * namespace list.
		 */
		selectList = parse_sub_analyze((Node *) stmt, pstate);

		Assert(list_length(selectList) == 1);
		selectQuery = (Query *) linitial(selectList);
		Assert(IsA(selectQuery, Query));

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
						 errmsg("UNION/INTERSECT/EXCEPT member statement cannot refer to other relations of same query level")));
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
		List	   *lcoltypes;
		List	   *rcoltypes;
		List	   *lcoltypmods;
		List	   *rcoltypmods;
		ListCell   *lct;
		ListCell   *rct;
		ListCell   *mct;
		ListCell   *lcm;
		ListCell   *rcm;
		ListCell   *mcm;
		const char *context;

		context = (stmt->op == SETOP_UNION ? "UNION" :
				   (stmt->op == SETOP_INTERSECT ? "INTERSECT" :
					"EXCEPT"));

		op->op = stmt->op;
		op->all = stmt->all;

		/*
		 * Recursively transform the child nodes.
		 */
		op->larg = transformSetOperationTree(pstate, stmt->larg);
		op->rarg = transformSetOperationTree(pstate, stmt->rarg);

		/*
		 * Verify that the two children have the same number of non-junk
		 * columns, and determine the types of the merged output columns.
		 * At one time in past, this information was used to deduce
		 * common data type, but now we don't; predict it beforehand,
		 * since in some cases transformation of individual leaf query
		 * hides what type the column should be from the whole of tree view.
		 */
		getSetColTypes(pstate, op->larg, &lcoltypes, &lcoltypmods);
		getSetColTypes(pstate, op->rarg, &rcoltypes, &rcoltypmods);
		Assert(list_length(lcoltypes) == list_length(rcoltypes));
		Assert(list_length(lcoltypes) == list_length(lcoltypmods));
		Assert(list_length(rcoltypes) == list_length(rcoltypmods));

		op->colTypes = NIL;
		op->colTypmods = NIL;
		/* We should have predicted types and typmods up to now */
		Assert(pstate->p_setopTypes && pstate->p_setopTypmods);
		Assert(list_length(pstate->p_setopTypes) ==
			   list_length(pstate->p_setopTypmods));
		Assert(list_length(pstate->p_setopTypes) ==
			   list_length(lcoltypes));

		/* Iterate each column with tree candidates */
		lct = list_head(lcoltypes);
		rct = list_head(rcoltypes);
		lcm = list_head(lcoltypmods);
		rcm = list_head(rcoltypmods);
		forboth(mct, pstate->p_setopTypes, mcm, pstate->p_setopTypmods)
		{
			Oid			lcoltype = lfirst_oid(lct);
			Oid			rcoltype = lfirst_oid(rct);
			int32		lcoltypmod = lfirst_int(lcm);
			int32		rcoltypmod = lfirst_int(rcm);
			Oid			rescoltype = lfirst_oid(mct);
			int32		rescoltypmod = lfirst_int(mcm);

			/*
			 * If the preprocessed coltype is InvalidOid, we fall back
			 * to the old style type resolution for backward
			 * compatibility. See transformSetOperationStmt for the reason.
			 */
			if (!OidIsValid(rescoltype))
			{
				/* select common type, same as CASE et al */
				rescoltype = select_common_type(
						list_make2_oid(lcoltype, rcoltype), context);
				/* if same type and same typmod, use typmod; else default */
				if (lcoltype == rcoltype && lcoltypmod == rcoltypmod)
					rescoltypmod = lcoltypmod;
			}
			/* Set final decision */
			op->colTypes = lappend_oid(op->colTypes, rescoltype);
			op->colTypmods = lappend_int(op->colTypmods, rescoltypmod);
			lct = lnext(lct);
			lcm = lnext(lcm);
			rct = lnext(rct);
			rcm = lnext(rcm);
		}

		return (Node *) op;
	}
}

/*
 * isSetopLeaf
 *  returns true if the statement is set operation tree leaf.
 */
static bool
isSetopLeaf(SelectStmt *stmt)
{
	Assert(stmt && IsA(stmt, SelectStmt));
	/*
	 * If an internal node of a set-op tree has ORDER BY, UPDATE, or LIMIT
	 * clauses attached, we need to treat it like a leaf node to generate an
	 * independent sub-Query tree.	Otherwise, it can be represented by a
	 * SetOperationStmt node underneath the parent Query.
	 */
	if (stmt->op == SETOP_NONE)
	{
		Assert(stmt->larg == NULL && stmt->rarg == NULL);
		return true;
	}
	else
	{
		Assert(stmt->larg != NULL && stmt->rarg != NULL);
		if (stmt->sortClause || stmt->limitOffset || stmt->limitCount ||
			stmt->lockingClause)
			return true;
		else
			return false;
	}
}

/*
 * collectSetopTypes
 *  transforms the statement partially and collect data type oid
 * and typmod from targetlist recursively. In set operations, the
 * final data type should be determined from the total tree view,
 * so we traverse the tree and collect types naively (without coercing)
 * and use the information for later column type decision.
 * types and typmods are output parameter, and the returned values
 * are List of List which contain column number of elements as the
 * first dimension, and leaf number of elements as the second dimension.
 * Returns the number of non-junk columns.
 */
static int
collectSetopTypes(ParseState *pstate, SelectStmt *stmt,
				  List **types, List **typmods)
{
	if (isSetopLeaf(stmt))
	{
		ParseState	   *parentstate = pstate;
		SelectStmt	   *select_stmt = stmt;
		List		   *tlist, *temp_tlist;
		ListCell	   *lc, *lct, *lcm;
		int				tlist_length;

		/* Copy them just in case */
		pstate = make_parsestate(parentstate);
		stmt = copyObject(select_stmt);

		if (stmt->valuesLists)
		{
			/* in VALUES query, we can transform all */
			tlist = transformValuesClause(pstate, stmt)->targetList;
		}
		else
		{
			/* transform only tragetList */
			transformFromClause(pstate, stmt->fromClause);
			tlist = transformTargetList(pstate, stmt->targetList);
		}

		/* Filter out junk columns. */
		temp_tlist = NIL;
		foreach (lc, tlist)
		{
			TargetEntry	   *tle = (TargetEntry *) lfirst(lc);

			if (tle->resjunk)
				continue;
			temp_tlist = lappend(temp_tlist, tle);
		}
		tlist = temp_tlist;
		tlist_length = list_length(tlist);

		if (*types == NIL)
		{
			Assert(*typmods == NIL);
			/* Construct List of List for numbers of tlist */
			foreach(lc, tlist)
			{
				*types = lappend(*types, NIL);
				*typmods = lappend(*typmods, NIL);
			}
		}
		else if (list_length(*types) != tlist_length)
		{
			/*
			 * Must be an error in later process.
			 * Nothing to do in this preprocess (not an assert.)
			 */
			free_parsestate(pstate);
			pfree(stmt);
			return tlist_length;
		}
		lct = list_head(*types);
		lcm = list_head(*typmods);
		foreach (lc, tlist)
		{
			TargetEntry	   *tle = (TargetEntry *) lfirst(lc);
			List		   *typelist = (List *) lfirst(lct);
			List		   *typmodlist = (List *) lfirst(lcm);

			/* Keep back to the original List */
			lfirst(lct) = lappend_oid(typelist, exprType((Node *) tle->expr));
			lfirst(lcm) = lappend_int(typmodlist, exprTypmod((Node *) tle->expr));

			lct = lnext(lct);
			lcm = lnext(lcm);
		}
		/* They're not needed anymore */
		free_parsestate(pstate);
		pfree(stmt);

		return tlist_length;
	}
	else
	{
		int			lnum, rnum;
		const char *context;

		/* just recurse to the leaf */
		lnum = collectSetopTypes(pstate, stmt->larg, types, typmods);
		rnum = collectSetopTypes(pstate, stmt->rarg, types, typmods);

		context = (stmt->op == SETOP_UNION ? "UNION" :
				   (stmt->op == SETOP_INTERSECT ? "INTERSECT" :
					"EXCEPT"));
		/*
		 * We need to report error here before doing anything with the
		 * collected result.
		 */
		if (lnum != rnum)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("each %s query must have the same number of columns",
							context)));

		return lnum;
	}
}

/*
 * getSetColTypes
 *	  Get output column types/typmods of an (already transformed) set-op node
 */
static void
getSetColTypes(ParseState *pstate, Node *node,
			   List **colTypes, List **colTypmods)
{
	*colTypes = NIL;
	*colTypmods = NIL;
	if (IsA(node, RangeTblRef))
	{
		RangeTblRef *rtr = (RangeTblRef *) node;
		RangeTblEntry *rte = rt_fetch(rtr->rtindex, pstate->p_rtable);
		Query	   *selectQuery = rte->subquery;
		ListCell   *tl;

		Assert(selectQuery != NULL);
		/* Get types of non-junk columns */
		foreach(tl, selectQuery->targetList)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(tl);

			if (tle->resjunk)
				continue;
			*colTypes = lappend_oid(*colTypes,
									exprType((Node *) tle->expr));
			*colTypmods = lappend_int(*colTypmods,
									  exprTypmod((Node *) tle->expr));
		}
	}
	else if (IsA(node, SetOperationStmt))
	{
		SetOperationStmt *op = (SetOperationStmt *) node;

		/* Result already computed during transformation of node */
		Assert(op->colTypes != NIL);
		*colTypes = op->colTypes;
		*colTypmods = op->colTypmods;
	}
	else
		elog(ERROR, "unrecognized node type: %d", (int) nodeTag(node));
}

/* Attach column names from a ColumnDef list to a TargetEntry list */
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

	qry->targetList = transformTargetList(pstate, stmt->targetList);

	qual = transformWhereClause(pstate, stmt->whereClause, "WHERE");

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

    /* CDB: Cursor position not available for errors below this point. */
    pstate->p_breadcrumb.node = NULL;

	qry->rtable = pstate->p_rtable;
	qry->jointree = makeFromExpr(pstate->p_joinlist, qual);

	qry->hasSubLinks = pstate->p_hasSubLinks;

	/*
	 * Top-level aggregates are simply disallowed in UPDATE, per spec. (From
	 * an implementation point of view, this is forced because the implicit
	 * ctid reference would otherwise be an ungrouped variable.)
	 */
	if (pstate->p_hasAggs)
		ereport(ERROR,
				(errcode(ERRCODE_GROUPING_ERROR),
				 errmsg("cannot use aggregate function in UPDATE")));

	if (pstate->p_hasWindFuncs)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("cannot use window function in UPDATE")));

	/*
	 * Now we are done with SELECT-like processing, and can get on with
	 * transforming the target list to match the UPDATE target columns.
	 */

	/* Prepare to assign non-conflicting resnos to resjunk attributes */
	if (pstate->p_next_resno <= pstate->p_target_relation->rd_rel->relnatts)
		pstate->p_next_resno = pstate->p_target_relation->rd_rel->relnatts + 1;

	/* Prepare non-junk columns for assignment to target table */
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

        /* CDB: Drop a breadcrumb in case of error. */
        pstate->p_breadcrumb.node = (Node *)origTarget;

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
	bool		save_hasAggs;
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

	/* save other state so that we can detect disallowed stuff */
	save_hasAggs = pstate->p_hasAggs;
	pstate->p_hasAggs = false;
	length_rtable = list_length(pstate->p_rtable);

	/* transform RETURNING identically to a SELECT targetlist */
	rlist = transformTargetList(pstate, returningList);

    /* CDB: Cursor position not available for errors below this point. */
    pstate->p_breadcrumb.node = NULL;

	/* check for disallowed stuff */

	/* aggregates not allowed (but subselects are okay) */
	if (pstate->p_hasAggs)
		ereport(ERROR,
				(errcode(ERRCODE_GROUPING_ERROR),
				 errmsg("cannot use aggregate function in RETURNING")));

	if (pstate->p_hasWindFuncs)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("cannot use window function in RETURNING")));


	/* no new relation references please */
	if (list_length(pstate->p_rtable) != length_rtable)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		 errmsg("RETURNING cannot contain references to other relations")));

	/* mark column origins */
	markTargetListOrigins(pstate, rlist);

	/* restore state */
	pstate->p_next_resno = save_next_resno;
	pstate->p_hasAggs = save_hasAggs;

	return rlist;
}
#endif

/*
 * transformAlterTable_all_PartitionStmt -
 *	transform an Alter Table Statement for some Partition operation
 */
static AlterTableCmd *
transformAlterTable_all_PartitionStmt(
		ParseState *pstate,
		AlterTableStmt *stmt,
		CreateStmtContext *pCxt,
		AlterTableCmd *cmd,
		List **extras_before,
		List **extras_after)
{
	AlterPartitionCmd 	*pc   	   = (AlterPartitionCmd *) cmd->def;
	AlterPartitionCmd 	*pci  	   = pc;
	AlterPartitionId  	*pid  	   = (AlterPartitionId *)pci->partid;
	AlterTableCmd 		*atc1 	   = cmd;
	RangeVar 			*rv   	   = stmt->relation;
	PartitionNode 		*pNode 	   = NULL;
	PartitionNode 		*prevNode  = NULL;
	int 			 	 partDepth = 0;
	Oid 			 	 par_oid   = InvalidOid;
	StringInfoData   sid1, sid2;

	if (atc1->subtype == AT_PartAlter)
	{
		PgPartRule* 	 prule = NULL;
		char 			*lrelname;
		Relation 		 rel   = heap_openrv(rv, AccessShareLock);

		initStringInfo(&sid1);
		initStringInfo(&sid2);

		appendStringInfo(&sid1, "relation \"%s\"",
						 RelationGetRelationName(rel));

		lrelname = sid1.data;

		pNode = RelationBuildPartitionDesc(rel, false);

		if (!pNode)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("%s is not partitioned",
							lrelname)));

		/* Processes nested ALTER (if it exists) */
		while (1)
		{
			AlterPartitionId  	*pid2 = NULL;

			if (atc1->subtype != AT_PartAlter)
			{
				rv = makeRangeVar(
						get_namespace_name(
								RelationGetNamespace(rel)),
						pstrdup(RelationGetRelationName(rel)), -1);
				heap_close(rel, AccessShareLock);
				rel = NULL;
				break;
			}

			pid2 = (AlterPartitionId *)pci->partid;

			if (pid2 && (pid2->idtype == AT_AP_IDValue))
			{
				List *vallist = (List *)pid2->partiddef;
				pid2->partiddef =
						(Node *)transformExpressionList(
								pstate, vallist);
			}

			partDepth++;

			if (!pNode)
				prule = NULL;
			else
				prule = get_part_rule1(rel,
									   pid2,
									   false, true,
									   NULL,
									   pNode,
									   sid1.data, NULL);

			if (prule && prule->topRule &&
				prule->topRule->children)
			{
				prevNode = pNode;
				pNode = prule->topRule->children;
				par_oid = RelationGetRelid(rel);

				/*
				 * Don't hold a long lock -- lock on the master is
				 * sufficient
				 */
				heap_close(rel, AccessShareLock);
				rel = heap_open(prule->topRule->parchildrelid,
								AccessShareLock);

				appendStringInfo(&sid2, "partition%s of %s",
								 prule->partIdStr, sid1.data);
				truncateStringInfo(&sid1, 0);
				appendStringInfo(&sid1, "%s", sid2.data);
				truncateStringInfo(&sid2, 0);
			}
			else
			{
				prevNode = pNode;
				pNode = NULL;
			}

			atc1 = (AlterTableCmd *)pci->arg1;
			pci = (AlterPartitionCmd *)atc1->def;
		} /* end while */
		if (rel)
			/* No need to hold onto the lock -- see above */
			heap_close(rel, AccessShareLock);
	} /* end if alter */

	switch (atc1->subtype)
	{
		case AT_PartAdd:				/* Add */
		case AT_PartSetTemplate:		/* Set Subpartn Template */

			if (pci->arg2) /* could be null for settemplate... */
			{
				AlterPartitionCmd 	*pc2 = (AlterPartitionCmd *) pci->arg2;
				CreateStmt 			*ct;
				InhRelation			*inh = makeNode(InhRelation);
				List				*cl  = NIL;

				Assert(IsA(pc2->arg2, List));
				ct = (CreateStmt *)linitial((List *)pc2->arg2);

				inh->relation = copyObject(rv);
				inh->options = list_make3_int(
						CREATE_TABLE_LIKE_INCLUDING_DEFAULTS,
						CREATE_TABLE_LIKE_INCLUDING_CONSTRAINTS,
						CREATE_TABLE_LIKE_INCLUDING_INDEXES);
				/*
				 * fill in remaining fields from parse time (gram.y):
				 * the new partition is LIKE the parent and it
				 * inherits from it
				 */
				ct->tableElts = lappend(ct->tableElts, inh);

				/*
				 * If this is an AOCO ADD PARTITION, add in the
				 * DEFAULT ENCODING provided in the ADD PARTITION WITH clause.
				 */
				if (is_aocs(ct->options))
				{
					List *stenc = form_default_storage_directive(ct->options);
					ColumnReferenceStorageDirective *deflt =
						makeNode(ColumnReferenceStorageDirective);

					stenc = transformStorageEncodingClause(stenc);

					deflt->deflt = true;
					deflt->encoding = stenc;

					ct->attr_encodings = list_make1(deflt);
				}

				cl = list_make1(ct);

				pc2->arg2 = (Node *)cl;
			}
		case AT_PartCoalesce:			/* Coalesce */
		case AT_PartDrop:				/* Drop */
		case AT_PartExchange:			/* Exchange */
		case AT_PartMerge:				/* Merge */
		case AT_PartModify:				/* Modify */
		case AT_PartRename:				/* Rename */
		case AT_PartTruncate:			/* Truncate */
		case AT_PartSplit:				/* Split */
			/* MPP-4011: get right pid for FOR(value) */
			pid  	   = (AlterPartitionId *)pci->partid;
			if (pid && (pid->idtype == AT_AP_IDValue))
			{
				List *vallist = (List *)pid->partiddef;
				pid->partiddef =
						(Node *)transformExpressionList(
								pstate, vallist);
			}
	break;
		default:
			break;
	}
	/* transform boundary specifications at execute time */
	return cmd;
} /* end transformAlterTable_all_PartitionStmt */

/*
 * transformAlterTableStmt -
 *	transform an Alter Table Statement
 */
static Query *
transformAlterTableStmt(ParseState *pstate, AlterTableStmt *stmt,
						List **extras_before, List **extras_after)
{
	CreateStmtContext cxt;
	Query	   *qry;
	ListCell   *lcmd,
			   *l;
	List	   *newcmds = NIL;
	bool		skipValidation = true;
	AlterTableCmd *newcmd;

	cxt.stmtType = "ALTER TABLE";
	cxt.relation = stmt->relation;
	cxt.inhRelations = NIL;
	cxt.isalter = true;
	cxt.hasoids = false;		/* need not be right */
	cxt.columns = NIL;
	cxt.ckconstraints = NIL;
	cxt.fkconstraints = NIL;
	cxt.ixconstraints = NIL;
	cxt.inh_indexes = NIL;
	cxt.blist = NIL;
	cxt.alist = NIL;
	cxt.dlist = NIL; /* used by transformCreateStmt, not here */
	cxt.pkey = NULL;

	/*
	 * The only subtypes that currently require parse transformation handling
	 * are ADD COLUMN and ADD CONSTRAINT.  These largely re-use code from
	 * CREATE TABLE.
	 * And ALTER TABLE ... <operator> PARTITION ...
	 */
	foreach(lcmd, stmt->cmds)
	{
		AlterTableCmd *cmd = (AlterTableCmd *) lfirst(lcmd);

		switch (cmd->subtype)
		{
			case AT_AddColumn:
				{
					ColumnDef  *def = (ColumnDef *) cmd->def;

					Assert(IsA(cmd->def, ColumnDef));

					/*
					 * Disallow adding a column with primary key constraint
					 */
					if (Gp_role == GP_ROLE_DISPATCH)
					{
						ListCell *c;
						foreach(c, def->constraints)
						{
							Constraint	   *cons = (Constraint *) lfirst(c);
							if(cons->contype == CONSTR_PRIMARY)
								elog(ERROR, "Cannot add column with primary "
									 "key constraint");
							if(cons->contype == CONSTR_UNIQUE)
								elog(ERROR, "Cannot add column with unique "
									 "constraint");

						}
					}

					transformColumnDefinition(pstate, &cxt,
											  (ColumnDef *) cmd->def);

					/*
					 * If the column has a non-null default, we can't skip
					 * validation of foreign keys.
					 */
					if (((ColumnDef *) cmd->def)->raw_default != NULL)
						skipValidation = false;

					newcmds = lappend(newcmds, cmd);

					/*
					 * Convert an ADD COLUMN ... NOT NULL constraint to a
					 * separate command
					 */
					if (def->is_not_null)
					{
						/* Remove NOT NULL from AddColumn */
						def->is_not_null = false;

						/* Add as a separate AlterTableCmd */
						newcmd = makeNode(AlterTableCmd);
						newcmd->subtype = AT_SetNotNull;
						newcmd->name = pstrdup(def->colname);
						newcmds = lappend(newcmds, newcmd);
					}

					/*
					 * All constraints are processed in other ways. Remove the
					 * original list
					 */
					def->constraints = NIL;

					break;
				}
			case AT_AddConstraint:

				/*
				 * The original AddConstraint cmd node doesn't go to newcmds
				 */

				if (IsA(cmd->def, Constraint))
					transformTableConstraint(pstate, &cxt,
											 (Constraint *) cmd->def);
				else if (IsA(cmd->def, FkConstraint))
				{
					cxt.fkconstraints = lappend(cxt.fkconstraints, cmd->def);

					/* GPDB: always skip validation of foreign keys */
					skipValidation = true;
				}
				else
					elog(ERROR, "unrecognized node type: %d",
						 (int) nodeTag(cmd->def));
				break;

			case AT_ProcessedConstraint:

				/*
				 * Already-transformed ADD CONSTRAINT, so just make it look
				 * like the standard case.
				 */
				cmd->subtype = AT_AddConstraint;
				newcmds = lappend(newcmds, cmd);
				break;

			/* CDB: Partitioned Tables */
            case AT_PartAlter:				/* Alter */
            case AT_PartAdd:				/* Add */
            case AT_PartCoalesce:			/* Coalesce */
            case AT_PartDrop:				/* Drop */
            case AT_PartExchange:			/* Exchange */
            case AT_PartMerge:				/* Merge */
            case AT_PartModify:				/* Modify */
            case AT_PartRename:				/* Rename */
            case AT_PartSetTemplate:		/* Set Subpartition Template */
            case AT_PartSplit:				/* Split */
            case AT_PartTruncate:			/* Truncate */
			{
				cmd = transformAlterTable_all_PartitionStmt(
						pstate,
						stmt,
						&cxt,
						cmd,
						extras_before,
						extras_after);

				newcmds = lappend(newcmds, cmd);
				break;
			}
			default:
				newcmds = lappend(newcmds, cmd);
				break;
		}
	}

	/*
	 * transformIndexConstraints wants cxt.alist to contain only index
	 * statements, so transfer anything we already have into extras_after
	 * immediately.
	 */
	*extras_after = list_concat(cxt.alist, *extras_after);
	cxt.alist = NIL;

	/* Postprocess index and FK constraints */
	transformIndexConstraints(pstate, &cxt, false);

	transformFKConstraints(pstate, &cxt, skipValidation, true);

	/*
	 * Push any index-creation commands into the ALTER, so that they can be
	 * scheduled nicely by tablecmds.c.
	 */
	foreach(l, cxt.alist)
	{
		Node	   *idxstmt = (Node *) lfirst(l);

		Assert(IsA(idxstmt, IndexStmt));
		newcmd = makeNode(AlterTableCmd);
		newcmd->subtype = AT_AddIndex;
		newcmd->def = idxstmt;
		newcmds = lappend(newcmds, newcmd);
	}
	cxt.alist = NIL;

	/* Append any CHECK or FK constraints to the commands list */
	foreach(l, cxt.ckconstraints)
	{
		newcmd = makeNode(AlterTableCmd);
		newcmd->subtype = AT_AddConstraint;
		newcmd->def = (Node *) lfirst(l);
		newcmds = lappend(newcmds, newcmd);
	}
	foreach(l, cxt.fkconstraints)
	{
		newcmd = makeNode(AlterTableCmd);
		newcmd->subtype = AT_AddConstraint;
		newcmd->def = (Node *) lfirst(l);
		newcmds = lappend(newcmds, newcmd);
	}

	/* Update statement's commands list */
	stmt->cmds = newcmds;

	qry = makeNode(Query);
	qry->commandType = CMD_UTILITY;
	qry->utilityStmt = (Node *) stmt;

	*extras_before = list_concat(*extras_before, cxt.blist);
	*extras_after = list_concat(cxt.alist, *extras_after);

	return qry;
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
	Query	   *result = makeNode(Query);
	List	   *extras_before = NIL,
			   *extras_after = NIL;

	result->commandType = CMD_UTILITY;
	result->utilityStmt = (Node *) stmt;

	/*
	 * Don't allow both SCROLL and NO SCROLL to be specified
	 */
	if ((stmt->options & CURSOR_OPT_SCROLL) &&
		(stmt->options & CURSOR_OPT_NO_SCROLL))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_CURSOR_DEFINITION),
				 errmsg("cannot specify both SCROLL and NO SCROLL")));

	result = transformStmt(pstate, stmt->query,
									&extras_before, &extras_after);

	/* Shouldn't get any extras, since grammar only allows SelectStmt */
	if (extras_before || extras_after)
		elog(ERROR, "unexpected extra stuff in cursor statement");

	/* Grammar should not have allowed anything but SELECT */
	if (!IsA(result, Query) ||
		result->commandType != CMD_SELECT ||
		result->utilityStmt != NULL)
		elog(ERROR, "unexpected non-SELECT command in DECLARE CURSOR");

	/* But we must explicitly disallow DECLARE CURSOR ... SELECT INTO */
	if (result->intoClause)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_CURSOR_DEFINITION),
				 errmsg("DECLARE CURSOR cannot specify INTO")));

	/* We won't need the raw querytree any more */
	stmt->query = NULL;

	stmt->is_simply_updatable = isSimplyUpdatableQuery(result);

	result->utilityStmt = (Node *) stmt;

	return result;
}

/*
 * isSimplyUpdatableQuery -
 *  determine whether a query is a simply updatable scan of a relation
 *
 * A query is simply updatable if, and only if, it...
 * - has no window clauses
 * - has no sort clauses
 * - has no grouping, having, distinct clauses, or simple aggregates
 * - has no subqueries
 * - has no LIMIT/OFFSET
 * - references only one range table (i.e. no joins, self-joins)
 *   - this range table must itself be updatable
 *	
 */
static bool
isSimplyUpdatableQuery(Query *query)
{
	Assert(query->commandType == CMD_SELECT);
	if (query->windowClause == NIL &&
		query->sortClause == NIL &&
		query->groupClause == NIL &&
		query->havingQual == NULL &&
		query->distinctClause == NIL &&
		!query->hasAggs &&
		!query->hasSubLinks &&
		query->limitCount == NULL &&
		query->limitOffset == NULL &&
		list_length(query->rtable) == 1)
	{
		RangeTblEntry *rte = (RangeTblEntry *)linitial(query->rtable);
		if (isSimplyUpdatableRelation(rte->relid))
			return true;
	}
	return false;
}

static Query *
transformPrepareStmt(ParseState *pstate, PrepareStmt *stmt)
{
	Query	   *result = makeNode(Query);
	List	   *argtype_oids;	/* argtype OIDs in a list */
	Oid		   *argtoids = NULL;	/* and as an array */
	int			nargs;
	List	   *queries;
	int			i;

	result->commandType = CMD_UTILITY;
	result->utilityStmt = (Node *) stmt;

	/* Transform list of TypeNames to list (and array) of type OIDs */
	nargs = list_length(stmt->argtypes);

	if (nargs)
	{
		ListCell   *l;

		argtoids = (Oid *) palloc(nargs * sizeof(Oid));
		i = 0;

		foreach(l, stmt->argtypes)
		{
			TypeName   *tn = lfirst(l);
			Oid			toid = typenameTypeId(pstate, tn);

			/* Pseudotypes are not valid parameters to PREPARE */
			if (get_typtype(toid) == TYPTYPE_PSEUDO)
			{
				ereport(ERROR,
						(errcode(ERRCODE_INDETERMINATE_DATATYPE),
						 errmsg("type \"%s\" is not a valid parameter for PREPARE",
								TypeNameToString(tn))));
			}

			argtoids[i++] = toid;
		}
	}

	/*
	 * Analyze the statement using these parameter types (any parameters
	 * passed in from above us will not be visible to it), allowing
	 * information about unknown parameters to be deduced from context.
	 */
	queries = parse_analyze_varparams((Node *) stmt->query,
									  pstate->p_sourcetext,
									  &argtoids, &nargs);

	/*
	 * Shouldn't get any extra statements, since grammar only allows
	 * OptimizableStmt
	 */
	if (list_length(queries) != 1)
		elog(ERROR, "unexpected extra stuff in prepared statement");

	/*
	 * Check that all parameter types were determined, and convert the array
	 * of OIDs into a list for storage.
	 */
	argtype_oids = NIL;
	for (i = 0; i < nargs; i++)
	{
		Oid			argtype = argtoids[i];

		if (argtype == InvalidOid || argtype == UNKNOWNOID)
			ereport(ERROR,
					(errcode(ERRCODE_INDETERMINATE_DATATYPE),
					 errmsg("could not determine data type of parameter $%d",
							i + 1)));

		argtype_oids = lappend_oid(argtype_oids, argtype);
	}

	stmt->argtype_oids = argtype_oids;
	stmt->query = linitial(queries);
	return result;
}

static Query *
transformExecuteStmt(ParseState *pstate, ExecuteStmt *stmt)
{
	Query	   *result = makeNode(Query);
	List	   *paramtypes;

	result->commandType = CMD_UTILITY;
	result->utilityStmt = (Node *) stmt;

	paramtypes = FetchPreparedStatementParams(stmt->name);

	if (stmt->params || paramtypes)
	{
		int			nparams = list_length(stmt->params);
		int			nexpected = list_length(paramtypes);
		ListCell   *l,
				   *l2;
		int			i = 1;

		if (nparams != nexpected)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
			errmsg("wrong number of parameters for prepared statement \"%s\"",
				   stmt->name),
					 errdetail("Expected %d parameters but got %d.",
							   nexpected, nparams)));

		forboth(l, stmt->params, l2, paramtypes)
		{
			Node	   *expr = lfirst(l);
			Oid			expected_type_id = lfirst_oid(l2);
			Oid			given_type_id;

			expr = transformExpr(pstate, expr);

			/* Cannot contain subselects or aggregates */
			if (pstate->p_hasSubLinks)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot use subquery in EXECUTE parameter")));
			if (pstate->p_hasAggs)
				ereport(ERROR,
						(errcode(ERRCODE_GROUPING_ERROR),
						 errmsg("cannot use aggregate function in EXECUTE parameter")));
			if (pstate->p_hasWindFuncs)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
				 		 errmsg("cannot use window function in EXECUTE parameter")));


			given_type_id = exprType(expr);

			expr = coerce_to_target_type(pstate, expr, given_type_id,
										 expected_type_id, -1,
										 COERCION_ASSIGNMENT,
										 COERCE_IMPLICIT_CAST,
										 -1);

			if (expr == NULL)
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("parameter $%d of type %s cannot be coerced to the expected type %s",
								i,
								format_type_be(given_type_id),
								format_type_be(expected_type_id)),
				errhint("You will need to rewrite or cast the expression.")));

			lfirst(l) = expr;
			i++;
		}
	}

	return result;
}

/* exported so planner can check again after rewriting, query pullup, etc */
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
	if (qry->hasWindFuncs)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("SELECT FOR UPDATE/SHARE is not allowed with window functions")));
}

/*
 * Transform a FOR UPDATE/SHARE clause
 *
 * This basically involves replacing names by integer relids.
 *
 * NB: if you need to change this, see also markQueryForLocking()
 * in rewriteHandler.c.
 */
static void
transformLockingClause(Query *qry, LockingClause *lc)
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

					applyLockingClause(qry, i, lc->forUpdate, lc->noWait);
					rte->requiredPerms |= ACL_SELECT_FOR_UPDATE;
					break;
				case RTE_SUBQUERY:

					/*
					 * FOR UPDATE/SHARE of subquery is propagated to all of
					 * subquery's rels
					 */
					transformLockingClause(rte->subquery, allrels);
					break;
				default:
					/* ignore JOIN, SPECIAL, FUNCTION RTEs */
					break;
			}
		}
	}
	else
	{
		/* just the named tables */
		foreach(l, lockedRels)
		{
			char	   *relname = strVal(lfirst(l));

			i = 0;
			foreach(rt, qry->rtable)
			{
				RangeTblEntry *rte = (RangeTblEntry *) lfirst(rt);

				++i;
				if (strcmp(rte->eref->aliasname, relname) == 0)
				{
					switch (rte->rtekind)
					{
						case RTE_RELATION:
							if(get_rel_relstorage(rte->relid) == RELSTORAGE_EXTERNAL)
								ereport(ERROR,
										(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
										 errmsg("SELECT FOR UPDATE/SHARE cannot be applied to external tables")));

							applyLockingClause(qry, i,
											   lc->forUpdate, lc->noWait);
							rte->requiredPerms |= ACL_SELECT_FOR_UPDATE;
							break;
						case RTE_SUBQUERY:

							/*
							 * FOR UPDATE/SHARE of subquery is propagated to
							 * all of subquery's rels
							 */
							transformLockingClause(rte->subquery, allrels);
							break;
						case RTE_JOIN:
							ereport(ERROR,
									(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
									 errmsg("SELECT FOR UPDATE/SHARE cannot be applied to a join")));
							break;
						case RTE_SPECIAL:
							ereport(ERROR,
									(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
									 errmsg("SELECT FOR UPDATE/SHARE cannot be applied to NEW or OLD")));
							break;
						case RTE_FUNCTION:
							ereport(ERROR,
									(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
									 errmsg("SELECT FOR UPDATE/SHARE cannot be applied to a function")));
							break;
						case RTE_TABLEFUNCTION:
							ereport(ERROR,
									(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
									 errmsg("SELECT FOR UPDATE/SHARE cannot be applied to a table function")));
							break;
						case RTE_VALUES:
							ereport(ERROR,
									(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
									 errmsg("SELECT FOR UPDATE/SHARE cannot be applied to VALUES")));
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
								relname)));
		}
	}
}

/*
 * Record locking info for a single rangetable item
 */
void
applyLockingClause(Query *qry, Index rtindex, bool forUpdate, bool noWait)
{
	RowMarkClause *rc;

	/* Check for pre-existing entry for same rtindex */
	if ((rc = get_rowmark(qry, rtindex)) != NULL)
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
		 */
		rc->forUpdate |= forUpdate;
		rc->noWait |= noWait;
		return;
	}

	/* Make a new RowMarkClause */
	rc = makeNode(RowMarkClause);
	rc->rti = rtindex;
	rc->forUpdate = forUpdate;
	rc->noWait = noWait;
	qry->rowMarks = lappend(qry->rowMarks, rc);
}


/*
 * Preprocess a list of column constraint clauses
 * to attach constraint attributes to their primary constraint nodes
 * and detect inconsistent/misplaced constraint attributes.
 *
 * NOTE: currently, attributes are only supported for FOREIGN KEY primary
 * constraints, but someday they ought to be supported for other constraints.
 */
static void
transformConstraintAttrs(List *constraintList)
{
	Node	   *lastprimarynode = NULL;
	bool		saw_deferrability = false;
	bool		saw_initially = false;
	ListCell   *clist;

	foreach(clist, constraintList)
	{
		Node	   *node = lfirst(clist);

		if (!IsA(node, Constraint))
		{
			lastprimarynode = node;
			/* reset flags for new primary node */
			saw_deferrability = false;
			saw_initially = false;
		}
		else
		{
			Constraint *con = (Constraint *) node;

			switch (con->contype)
			{
				case CONSTR_ATTR_DEFERRABLE:
					if (lastprimarynode == NULL ||
						!IsA(lastprimarynode, FkConstraint))
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("misplaced DEFERRABLE clause")));
					if (saw_deferrability)
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("multiple DEFERRABLE/NOT DEFERRABLE clauses not allowed")));
					saw_deferrability = true;
					((FkConstraint *) lastprimarynode)->deferrable = true;
					break;
				case CONSTR_ATTR_NOT_DEFERRABLE:
					if (lastprimarynode == NULL ||
						!IsA(lastprimarynode, FkConstraint))
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("misplaced NOT DEFERRABLE clause")));
					if (saw_deferrability)
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("multiple DEFERRABLE/NOT DEFERRABLE clauses not allowed")));
					saw_deferrability = true;
					((FkConstraint *) lastprimarynode)->deferrable = false;
					if (saw_initially &&
						((FkConstraint *) lastprimarynode)->initdeferred)
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("constraint declared INITIALLY DEFERRED must be DEFERRABLE")));
					break;
				case CONSTR_ATTR_DEFERRED:
					if (lastprimarynode == NULL ||
						!IsA(lastprimarynode, FkConstraint))
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("misplaced INITIALLY DEFERRED clause")));
					if (saw_initially)
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("multiple INITIALLY IMMEDIATE/DEFERRED clauses not allowed")));
					saw_initially = true;
					((FkConstraint *) lastprimarynode)->initdeferred = true;

					/*
					 * If only INITIALLY DEFERRED appears, assume DEFERRABLE
					 */
					if (!saw_deferrability)
						((FkConstraint *) lastprimarynode)->deferrable = true;
					else if (!((FkConstraint *) lastprimarynode)->deferrable)
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("constraint declared INITIALLY DEFERRED must be DEFERRABLE")));
					break;
				case CONSTR_ATTR_IMMEDIATE:
					if (lastprimarynode == NULL ||
						!IsA(lastprimarynode, FkConstraint))
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
							errmsg("misplaced INITIALLY IMMEDIATE clause")));
					if (saw_initially)
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("multiple INITIALLY IMMEDIATE/DEFERRED clauses not allowed")));
					saw_initially = true;
					((FkConstraint *) lastprimarynode)->initdeferred = false;
					break;
				default:
					/* Otherwise it's not an attribute */
					lastprimarynode = node;
					/* reset flags for new primary node */
					saw_deferrability = false;
					saw_initially = false;
					break;
			}
		}
	}
}

/* Build a FromExpr node */
static FromExpr *
makeFromExpr(List *fromlist, Node *quals)
{
	FromExpr   *f = makeNode(FromExpr);

	f->fromlist = fromlist;
	f->quals = quals;
	return f;
}

/*
 * Special handling of type definition for a column
 */
static void
transformColumnType(ParseState *pstate, ColumnDef *column)
{
	/*
	 * All we really need to do here is verify that the type is valid.
	 */
	Type		ctype = typenameType(pstate, column->typname);

	ReleaseType(ctype);
}

static void
setSchemaName(char *context_schema, char **stmt_schema_name)
{
	if (*stmt_schema_name == NULL)
		*stmt_schema_name = context_schema;
	else if (strcmp(context_schema, *stmt_schema_name) != 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_SCHEMA_DEFINITION),
				 errmsg("CREATE specifies a schema (%s) "
						"different from the one being created (%s)",
						*stmt_schema_name, context_schema)));
}

/*
 * analyzeCreateSchemaStmt -
 *	  analyzes the "create schema" statement
 *
 * Split the schema element list into individual commands and place
 * them in the result list in an order such that there are no forward
 * references (e.g. GRANT to a table created later in the list). Note
 * that the logic we use for determining forward references is
 * presently quite incomplete.
 *
 * SQL92 also allows constraints to make forward references, so thumb through
 * the table columns and move forward references to a posterior alter-table
 * command.
 *
 * The result is a list of parse nodes that still need to be analyzed ---
 * but we can't analyze the later commands until we've executed the earlier
 * ones, because of possible inter-object references.
 *
 * Note: Called from commands/schemacmds.c
 */
List *
analyzeCreateSchemaStmt(CreateSchemaStmt *stmt)
{
	CreateSchemaStmtContext cxt;
	List	   *result;
	ListCell   *elements;

	cxt.stmtType = "CREATE SCHEMA";
	cxt.schemaname = stmt->schemaname;
	cxt.authid = stmt->authid;
	cxt.sequences = NIL;
	cxt.tables = NIL;
	cxt.views = NIL;
	cxt.indexes = NIL;
	cxt.grants = NIL;
	cxt.triggers = NIL;
	cxt.fwconstraints = NIL;
	cxt.alters = NIL;
	cxt.blist = NIL;
	cxt.alist = NIL;

	/*
	 * Run through each schema element in the schema element list. Separate
	 * statements by type, and do preliminary analysis.
	 */
	foreach(elements, stmt->schemaElts)
	{
		Node	   *element = lfirst(elements);

		switch (nodeTag(element))
		{
			case T_CreateSeqStmt:
				{
					CreateSeqStmt *elp = (CreateSeqStmt *) element;

					setSchemaName(cxt.schemaname, &elp->sequence->schemaname);
					cxt.sequences = lappend(cxt.sequences, element);
				}
				break;

			case T_CreateStmt:
				{
					CreateStmt *elp = (CreateStmt *) element;

					setSchemaName(cxt.schemaname, &elp->relation->schemaname);

					/*
					 * XXX todo: deal with constraints
					 */
					cxt.tables = lappend(cxt.tables, element);
				}
				break;

			case T_CreateExternalStmt:
				{
					CreateExternalStmt *elp = (CreateExternalStmt *) element;

					setSchemaName(cxt.schemaname, &elp->relation->schemaname);

					cxt.tables = lappend(cxt.tables, element);
				}
				break;

			case T_ViewStmt:
				{
					ViewStmt   *elp = (ViewStmt *) element;

					setSchemaName(cxt.schemaname, &elp->view->schemaname);

					/*
					 * XXX todo: deal with references between views
					 */
					cxt.views = lappend(cxt.views, element);
				}
				break;

			case T_IndexStmt:
				{
					IndexStmt  *elp = (IndexStmt *) element;

					setSchemaName(cxt.schemaname, &elp->relation->schemaname);
					cxt.indexes = lappend(cxt.indexes, element);
				}
				break;

			case T_CreateTrigStmt:
				{
					CreateTrigStmt *elp = (CreateTrigStmt *) element;

					setSchemaName(cxt.schemaname, &elp->relation->schemaname);
					cxt.triggers = lappend(cxt.triggers, element);
				}
				break;

			case T_GrantStmt:
				cxt.grants = lappend(cxt.grants, element);
				break;

			default:
				elog(ERROR, "unrecognized node type: %d",
					 (int) nodeTag(element));
		}
	}

	result = NIL;
	result = list_concat(result, cxt.sequences);
	result = list_concat(result, cxt.tables);
	result = list_concat(result, cxt.views);
	result = list_concat(result, cxt.indexes);
	result = list_concat(result, cxt.triggers);
	result = list_concat(result, cxt.grants);

	return result;
}

/*
 * Traverse a fully-analyzed tree to verify that parameter symbols
 * match their types.  We need this because some Params might still
 * be UNKNOWN, if there wasn't anything to force their coercion,
 * and yet other instances seen later might have gotten coerced.
 */
static bool
check_parameter_resolution_walker(Node *node,
								  check_parameter_resolution_context *context)
{
	if (node == NULL)
		return false;
	if (IsA(node, Param))
	{
		Param	   *param = (Param *) node;

		if (param->paramkind == PARAM_EXTERN)
		{
			int			paramno = param->paramid;

			if (paramno <= 0 || /* shouldn't happen, but... */
				paramno > context->numParams)
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_PARAMETER),
						 errmsg("there is no parameter $%d", paramno)));

			if (param->paramtype != context->paramTypes[paramno - 1])
				ereport(ERROR,
						(errcode(ERRCODE_AMBIGUOUS_PARAMETER),
					 errmsg("could not determine data type of parameter $%d",
							paramno)));
		}
		return false;
	}
	if (IsA(node, Query))
	{
		/* Recurse into RTE subquery or not-yet-planned sublink subquery */
		return query_tree_walker((Query *) node,
								 check_parameter_resolution_walker,
								 (void *) context, 0);
	}
	return expression_tree_walker(node, check_parameter_resolution_walker,
								  (void *) context);
}

static void
setQryDistributionPolicy(SelectStmt *stmt, Query *qry)
{
	ListCell   *keys = NULL;

	GpPolicy  *policy = NULL;
	int			colindex = 0;
	int			maxattrs = MaxPolicyAttributeNumber;

	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	Assert(stmt->distributedBy);

	if (stmt->distributedBy)
	{
		/*
		 * We have a DISTRIBUTED BY column list specified by the user
		 * Process it now and set the distribution policy.
		 */

		policy = (GpPolicy *) palloc(sizeof(GpPolicy) + maxattrs * sizeof(policy->attrs[0]));
		policy->ptype = POLICYTYPE_PARTITIONED;
		policy->nattrs = 0;
		policy->attrs[0] = 1;

		if (stmt->distributedBy->length == 1 && (list_head(stmt->distributedBy) == NULL || linitial(stmt->distributedBy) == NULL))
		{
			/* distributed randomly */
			qry->intoPolicy = policy;
		}
		else
		foreach(keys, stmt->distributedBy)
		{
			char	   *key = strVal(lfirst(keys));
			bool		found = false;

			AttrNumber	n;

			for(n=1;n<=list_length(qry->targetList);n++)
			{

				TargetEntry *target = get_tle_by_resno(qry->targetList, n);
				colindex = n;

				if (target->resname && strcmp(target->resname, key) == 0)
				{
					found = true;

				} /*if*/

				if (found)
					break;

			} /*for*/

			if (!found)
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_COLUMN),
						 errmsg("column \"%s\" named in DISTRIBUTED BY "
								"clause does not exist",
								key)));

			policy->attrs[policy->nattrs++] = colindex;

		} /*foreach */
		if (policy->nattrs > 0)
			qry->intoPolicy = policy;
	}
}

/*
 * getLikeDistributionPolicy
 *
 * For Greenplum Database distributed tables, default to
 * the same distribution as the first LIKE table, unless
 * we also have INHERITS
 */
static List*
getLikeDistributionPolicy(InhRelation* e)
{
	List*			likeDistributedBy = NIL;
	Oid				relId;
	GpPolicy*		oldTablePolicy;

	relId = RangeVarGetRelid(e->relation, false);
	oldTablePolicy = GpPolicyFetch(CurrentMemoryContext, relId);

	if (oldTablePolicy != NULL &&
		oldTablePolicy->ptype == POLICYTYPE_PARTITIONED)
	{
		int ia;

		if (oldTablePolicy->nattrs > 0)
		{
			for (ia = 0 ; ia < oldTablePolicy->nattrs ; ia++)
			{
				char *attname = get_attname(relId, oldTablePolicy->attrs[ia]);

				if (likeDistributedBy)
					likeDistributedBy = lappend(likeDistributedBy, (Node *) makeString(attname));
				else
					likeDistributedBy = list_make1((Node *) makeString(attname));
			}
		}
		else
		{	/* old table is distributed randomly. */
			likeDistributedBy = list_make1((Node *) NULL);
		}
	}

	return likeDistributedBy;
}
