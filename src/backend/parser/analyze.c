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
 *	$PostgreSQL: pgsql/src/backend/parser/analyze.c,v 1.353.2.1 2007/06/20 18:21:08 tgl Exp $
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
#include "catalog/pg_partition.h"
#include "catalog/pg_partition_rule.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "catalog/pg_type_encoding.h"
#include "cdb/cdbpartition.h"
#include "commands/defrem.h"
#include "commands/prepare.h"
#include "commands/tablecmds.h"
#include "commands/tablespace.h"
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
#include "parser/parse_relation.h"
#include "parser/parse_target.h"
#include "parser/parse_type.h"
#include "parser/parse_utilcmd.h"
#include "parser/parse_cte.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbcat.h"
#include "cdb/cdbhash.h"
#include "cdb/cdbsreh.h"


#define MaxPolicyAttributeNumber MaxHeapAttributeNumber
/* temporary rule to control whether we generate RULEs or not -- for testing */
bool        enable_partition_rules = false;

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

typedef struct
{
	ParseState *pstate;
	List *cols;
} part_col_cxt;

/* state structures for validing parsed partition specifications */
typedef struct
{
	ParseState *pstate;
	CreateStmtContext *cxt;
	CreateStmt *stmt;
	PartitionBy *pBy;
	PartitionElem *pElem;
	PartitionElem *prevElem;
	Node *spec;
	int partNumber;
	char *namBuf;
	char *at_depth;
	bool prevHadName;
	int prevStartEnd;
	List *allRangeVals;
	List *allListVals;
} partValidationState;

typedef struct
{
	ParseState *pstate;
	int location;
} range_partition_ctx;

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
static void transformETDistributedBy(ParseState *pstate, CreateStmtContext *cxt,
									 List *distributedBy, GpPolicy **policyp,
									 List *likeDistributedBy,
									 bool bQuiet,
									 bool iswritable,
									 bool onmaster);
static void transformDistributedBy(ParseState *pstate,
								   CreateStmtContext *cxt,
								   List *distributedBy,
								   GpPolicy ** policy, List *likeDistributedBy,
								   bool bQuiet);
static void transformPartitionBy(ParseState *pstate,
		 CreateStmtContext *cxt, CreateStmt *stmt,
		 Node *partitionBy, GpPolicy *policy);

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

static void transformSingleRowErrorHandling(ParseState *pstate, CreateStmtContext *cxt,
											SingleRowErrorDesc *sreh);

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

static int deparse_partition_rule(Node *pNode, char *outbuf, size_t outsize);

static int partition_range_compare(ParseState *pstate,
								   CreateStmtContext *cxt, CreateStmt *stmt,
								   PartitionBy *pBy,
								   char	   *at_depth,
								   int		partNumber,
								   char	   *compare_op,
								   PartitionRangeItem *pRI1,
								   PartitionRangeItem *pRI2);

static int partition_range_every(ParseState *pstate,
								 PartitionBy *partitionBy,
								 List *coltypes,
								 char *at_depth,
								 partValidationState *vstate);
static Datum partition_arg_get_val(Node *node, bool *isnull);
static Datum eval_basic_opexpr(ParseState *pstate, List *oprname,
							   Node *leftarg, Node *rightarg,
							   bool *typbyval, int16 *typlen,
							   Oid *restypid,
							   int location);

static Node *make_prule_catalog(ParseState *pstate,
		 CreateStmtContext *cxt, CreateStmt *stmt,
		 Node *partitionBy, PartitionElem *pElem,
 		 char *at_depth, char *child_name_str,
		 char *exprBuf,
		 Node *pWhere
		);

static Node *make_prule_rulestmt(ParseState *pstate,
		 CreateStmtContext *cxt, CreateStmt *stmt,
		 Node *partitionBy, PartitionElem *pElem,
 		 char *at_depth, char *child_name_str,
		 char *exprBuf,
		 Node *pWhere
		);

static List *transformAttributeEncoding(List *stenc, CreateStmt *stmt,
										CreateStmtContext cxt);
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

				/*
				 * Check if we need to create an error table. If so, add it to the
				 * before list.
				 */
				if(n->sreh && ((SingleRowErrorDesc *)n->sreh)->errtable)
				{
					CreateStmtContext cxt;
					cxt.blist = NIL;
					cxt.alist = NIL;


					transformSingleRowErrorHandling(pstate, &cxt,
													(SingleRowErrorDesc *)n->sreh);
					*extras_before = list_concat(*extras_before, cxt.blist);
				}

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

		bool need_free_arg = false;
		arg = defGetString(el, &need_free_arg);
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
		if (need_free_arg)
		{
			pfree(arg);
			arg = NULL;
		}

		AssertImply(need_free_arg, NULL == arg);

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
static bool
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

		bool need_free_arg = false;
		arg = defGetString(el, &need_free_arg);

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
		if (need_free_arg)
		{
			pfree(arg);
			arg = NULL;
		}

		AssertImply(need_free_arg, NULL == arg);
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
						bool need_free_ela = false;
						bool need_free_elb = false;
						char *ela_str = defGetString(ela, &need_free_ela);
						char *elb_str = defGetString(elb, &need_free_elb);
						int result = pg_strcasecmp(ela_str,elb_str);
						// free ela_str, elb_str if it is initialized via TypeNameToString
						if (need_free_ela)
						{
							pfree(ela_str);
							ela_str = NULL;
						}
						if (need_free_elb)
						{
							pfree(elb_str);
							elb_str = NULL;
						}

						AssertImply(need_free_ela, NULL == ela_str);
						AssertImply(need_free_elb, NULL == elb_str);

						if (result != 0)
						{
							return true;
						}
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
			size_t collen = strlen(strVal(c->column));
			size_t n = NAMEDATALEN - 1 < collen ? NAMEDATALEN - 1 : collen;
			MemSet(colname, 0, NAMEDATALEN);
			memcpy(colname, strVal(c->column), n);
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
find_crsd(Value *column, List *stenc)
{
	ListCell *lc;

	foreach(lc, stenc)
	{
		ColumnReferenceStorageDirective *c = lfirst(lc);

		if (c->deflt == false && equal(column, c->column))
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
transformAttributeEncoding(List *stenc, CreateStmt *stmt, CreateStmtContext cxt)
{
	ListCell *lc;
	bool found_enc = stenc != NIL;
	bool can_enc = is_aocs(stmt->options);
	ColumnReferenceStorageDirective *deflt = NULL;
	List *newenc = NIL;
	List *tmpenc;

#define UNSUPPORTED_ORIENTATION_ERROR() \
	ereport(ERROR, \
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED), \
			 errmsg("ENCODING clause only supported with column oriented tables")))

	/* We only support the attribute encoding clause on AOCS tables */
	if (stenc && !can_enc)
		UNSUPPORTED_ORIENTATION_ERROR();


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
			else
			{
				deflt = c;
				deflt->encoding = transformStorageEncodingClause(deflt->encoding);

				/*
				 * The default encoding and the with clause better not
				 * try and set the same options!
				 */

				if (encodings_overlap(stmt->options, c->encoding, false))
					ereport(ERROR,
						    (errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 	 errmsg("DEFAULT COLUMN ENCODING clause cannot "
						 			"override values set in WITH clause")));
			}
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
	foreach(lc, cxt.columns)
	{
		Node *n = lfirst(lc);
		ColumnDef *d = (ColumnDef *)n;
		ColumnReferenceStorageDirective *c =
			makeNode(ColumnReferenceStorageDirective);

		Insist(IsA(d, ColumnDef));

		c->column = makeString(pstrdup(d->colname));

		if (d->encoding)
		{
			found_enc = true;
			c->encoding = d->encoding;
			c->encoding = transformStorageEncodingClause(c->encoding);
		}
		else
		{
			/*
			 * No explicit encoding clause but we may still have a
			 * clause if
			 * i. There's a column reference storage directive for this
			 * column
			 * ii. There's a default column encoding
			 * iii. There's a default for the type.
			 *
			 * If none of these is the case, we set an 'empty' encoding
			 * clause.
			 */

			/*
			 * We use stenc here -- the storage encoding directives
			 * gleaned from the table elements list because we know
			 * there's nothing to look at in new_enc, since we're
			 * generating that
			 */
			ColumnReferenceStorageDirective *s = find_crsd(c->column, stenc);

			if (s)
			{
				s->encoding = transformStorageEncodingClause(s->encoding);
				newenc = lappend(newenc, s);
				continue;
			}

			/* ... and so we beat on, boats against the current... */
			if (deflt)
			{
				c->encoding = copyObject(deflt->encoding);
			}
			else
			{
				List *te = TypeNameGetStorageDirective(d->typname);

				if (te)
				{
					c->encoding = copyObject(te);
				}
				else
					c->encoding = default_column_encoding_clause();
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
			return NULL;
	}

	validateColumnStorageEncodingClauses(newenc, stmt);

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
		stmt->attr_encodings = transformAttributeEncoding(stenc, stmt, cxt);

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

	return q;
}

static Query *
transformCreateExternalStmt(ParseState *pstate, CreateExternalStmt *stmt,
							List **extras_before, List **extras_after)
{
	CreateStmtContext cxt;
	Query	   *q;
	ListCell   *elements;
	ExtTableTypeDesc	*exttypeDesc = NULL;
	List  	   *likeDistributedBy = NIL;
	bool	    bQuiet = false;	/* shut up transformDistributedBy messages */
	bool		onmaster = false;
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
	 * Check if this is an EXECUTE ON MASTER table. We'll need this information
	 * in transformExternalDistributedBy. While at it, we also check if an error
	 * table or LOG ERRORS is attempted to be used on ON MASTER table and error if so.
	 */
	if(!iswritable)
	{
		exttypeDesc = (ExtTableTypeDesc *)stmt->exttypedesc;

		if(exttypeDesc->exttabletype == EXTTBL_TYPE_EXECUTE)
		{
			ListCell   *exec_location_opt;

			foreach(exec_location_opt, exttypeDesc->on_clause)
			{
				DefElem    *defel = (DefElem *) lfirst(exec_location_opt);

				if (strcmp(defel->defname, "master") == 0)
				{
					SingleRowErrorDesc *srehDesc = (SingleRowErrorDesc *)stmt->sreh;

					onmaster = true;

					if(srehDesc && (srehDesc->errtable || srehDesc->into_file))
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
								 errmsg("External web table with ON MASTER clause "
										"cannot use LOG ERRORS feature.")));
				}
			}
		}
	}

	/*
	 * Check if we need to create an error table. If so, add it to the
	 * before list.
	 */
	if(stmt->sreh && ((SingleRowErrorDesc *)stmt->sreh)->errtable)
		transformSingleRowErrorHandling(pstate, &cxt,
										(SingleRowErrorDesc *) stmt->sreh);

	transformETDistributedBy(pstate, &cxt, stmt->distributedBy, &stmt->policy,
							 likeDistributedBy, bQuiet, iswritable, onmaster);

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



/*
 * transformETDistributedBy - transform DISTRIBUTED BY clause for
 * external tables.
 *
 * WET: by default we distribute RANDOMLY, or by the distribution key
 * of the LIKE table if exists. However, if DISTRIBUTED BY was
 * specified we use it by calling the regular transformDistributedBy and
 * handle it like we would for non external tables.
 *
 * RET: We always create a random distribution policy entry *unless*
 * this is an EXECUTE table with ON MASTER specified, in which case
 * we create no policy so that the master will be accessed.
 */
static void
transformETDistributedBy(ParseState *pstate, CreateStmtContext *cxt,
						 List *distributedBy, GpPolicy **policyp,
						 List *likeDistributedBy,
						 bool bQuiet,
						 bool iswritable,
						 bool onmaster)
{
	int			maxattrs = MaxPolicyAttributeNumber;
	GpPolicy*	p = NULL;

	/*
	 * utility mode creates can't have a policy.  Only the QD can have policies
	 */
	if (Gp_role != GP_ROLE_DISPATCH)
	{
		*policyp = NULL;
		return;
	}

	if(!iswritable && list_length(distributedBy) > 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("Readable external tables can\'t specify a DISTRIBUTED BY clause.")));

	if(iswritable)
	{
		/* WET */

		if(distributedBy == NIL && likeDistributedBy == NIL)
		{
			/* defaults to DISTRIBUTED RANDOMLY irrespective of the gp_create_table_random_default_distribution guc */
			*policyp = createRandomDistribution(maxattrs);
		}
		else
		{
			/* regular DISTRIBUTED BY transformation */
			transformDistributedBy(pstate, cxt, distributedBy, policyp,
								   likeDistributedBy, bQuiet);
		}
	}
	else
	{
		/* RET */

		if(onmaster)
		{
			p = NULL;
		}
		else
		{
			/* defaults to DISTRIBUTED RANDOMLY */
			p = createRandomDistribution(maxattrs);
		}

		*policyp = p;
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

		/* If we have no attributes, make sure that we don't create a bogus policy! */
		if (cxt && list_length(cxt->columns) == 0 && !cxt->inhRelations)
		{
			elog(NOTICE, "Table has no attributes to distribute on.");
			pfree(policy);
			return;
		}

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
	bool need_free_cmplevel = false;
	bool foundBlockSize = false;
	char *arg;
	List *retList = list;
	ListCell *lc;
	DefElem *el;
	const StdRdOptions *ao_opts = currentAOStorageOptions();

	foreach(lc, list)
	{
		el = lfirst(lc);

		if (pg_strcasecmp("compresstype", el->defname) == 0)
		{
			foundCompressType = true;
			bool need_free_arg = false;
			arg = defGetString(el, &need_free_arg);
			if (pg_strcasecmp("none", arg) == 0)
				foundCompressTypeNone = true;
			if (need_free_arg)
			{
				pfree(arg);
				arg = NULL;
			}

			AssertImply(need_free_arg, NULL == arg);
		}
		else if (pg_strcasecmp("compresslevel", el->defname) == 0)
		{
			cmplevel = defGetString(el, &need_free_cmplevel);
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

static int
deparse_partition_rule(Node *pNode, char *outbuf, size_t outsize)
{
	if (!pNode)
		return 0;

	switch (nodeTag(pNode))
	{
		case T_NullTest:
			{
				NullTest *nt = (NullTest *)pNode;
				char leftbuf[1000];
				if (!deparse_partition_rule((Node *)nt->arg, leftbuf,
										   sizeof(leftbuf)))
					return 0;

				snprintf(outbuf, outsize, "%s %s",
						 leftbuf, nt->nulltesttype == IS_NULL ?
						 "ISNULL" : "IS NOT NULL");
			}

			break;
		case T_Value:

				/* XXX XXX XXX ??? */

			break;
		case T_ColumnRef:
		{
			ColumnRef		*pCRef = (ColumnRef *)pNode;
			List			*coldefs = pCRef->fields;
			ListCell		*lc      = NULL;
			StringInfoData   sid;
			int colcnt = 0;

			initStringInfo(&sid);

			lc = list_head(coldefs);

			for (; lc; lc = lnext(lc)) /* for all cols */
			{
				Node *pCol = lfirst(lc);
				char	 leftBuf[10000];

				if (!deparse_partition_rule(pCol, leftBuf, sizeof(leftBuf)))
						return 0;

				if (colcnt)
				{
					appendStringInfo(&sid, ".");
				}

				appendStringInfo(&sid, "%s", leftBuf);

				colcnt++;
			} /* end for all cols */

			outbuf[0] = '\0';
			snprintf(outbuf, outsize, "%s", sid.data);
			pfree(sid.data);

			break;
		}
		case T_String:
				snprintf(outbuf, outsize, "%s",
						 strVal(pNode));
			break;
		case T_Integer:
				snprintf(outbuf, outsize, "%ld",
						 intVal(pNode));
			break;
		case T_Float:
				snprintf(outbuf, outsize, "%f",
						 floatVal(pNode));
			break;
		case T_A_Const:
		{
			A_Const *acs = (A_Const *)pNode;

			if (acs->val.type == T_String)
			{
				if (acs->typname) /* deal with explicit types */
				{
					/* XXX XXX: simple types only -- need to
					 * handle Interval, etc */

					snprintf(outbuf, outsize, "\'%s\'::%s",
							 acs->val.val.str,
							 TypeNameToString(acs->typname));
				}
				else
				{
					snprintf(outbuf, outsize, "\'%s\'",
							 acs->val.val.str);
				}

				return 1;
			}
			return (deparse_partition_rule((Node *)&(acs->val),
										   outbuf, outsize));
		}
			break;
		case T_A_Expr:
		{
			A_Expr	*ax = (A_Expr *)pNode;
			char	 leftBuf[10000];
			char	 rightBuf[10000];
			char    *infix_op;

			switch (ax->kind)
			{
				case AEXPR_OP:			/* normal operator */
				case AEXPR_AND:			/* booleans - name field is unused */
				case AEXPR_OR:
					break;
				default:
					return 0;
			}
			if (!deparse_partition_rule(ax->lexpr, leftBuf, sizeof(leftBuf)))
				return 0;
			if (!deparse_partition_rule(ax->rexpr, rightBuf, sizeof(rightBuf)))
				return 0;
			switch (ax->kind)
			{
				case AEXPR_OP:			/* normal operator */
					infix_op = strVal(lfirst(list_head(ax->name)));
					break;
				case AEXPR_AND:			/* booleans - name field is unused */
					infix_op = "AND";
					break;
				case AEXPR_OR:
					infix_op = "OR";
					break;
				default:
					return 0;
			}
			snprintf(outbuf, outsize, "(%s %s %s)",
					 leftBuf, infix_op, rightBuf);

		}
			break;
		case T_Var:
			{
/*				Var		   *var = (Var *) node; */
			}
		default:
				break;
	}
	return 1;
}

#if 0
static A_Const *
make_a_const(A_Const *pValConst, char *val_str)
{
	A_Const *pNewValConst = makeNode(A_Const);
	int estat = 0;

	Assert(val_str);

	pNewValConst->val.type = pValConst->val.type;
	pNewValConst->typname = pValConst->typname;
    pNewValConst->location = pValConst->location;

	switch (pValConst->val.type)
	{
		case T_Integer:
			estat = sscanf(val_str, "%ld",
						   &(pNewValConst->val.val.ival));
			break;
		case T_Float:
/*			estat = scanf(val_str, "%f",
			(pNewValConst->val.val.str)); */

		case T_String:
			pNewValConst->val.val.str = val_str;

			estat = 1;
			break;

		default:

			break;
	}

	if (estat < 1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("unknown type or operator: %s",
						val_str)));
	}

	return (pNewValConst);
} /* end make_a_const */
#endif

static Node *
make_prule_catalog(ParseState *pstate,
		 CreateStmtContext *cxt, CreateStmt *stmt,
		 Node *partitionBy, PartitionElem *pElem,
 		 char *at_depth, char *child_name_str,
		 char *exprBuf,
		 Node *pWhere
		 )
{
	Node				*pResult = NULL;
	InsertStmt			*pIns    = NULL;
	RangeVar			*parent_tab_name;
	RangeVar			*child_tab_name;
	char				 ruleBuf[10000];
	char				 newVals[10000];

	{
		List			*coldefs = stmt->tableElts;
		ListCell		*lc      = NULL;
		StringInfoData   sid;
		int colcnt = 0;

		initStringInfo(&sid);

		lc = list_head(coldefs);

		for (; lc; lc = lnext(lc)) /* for all cols */
		{
			Node *pCol = lfirst(lc);
			ColumnDef *pColDef;

			if (nodeTag(pCol) != T_ColumnDef) /* avoid constraints, etc */
				continue;

			pColDef = (ColumnDef *)pCol;

			if (colcnt)
			{
				appendStringInfo(&sid, ", ");
			}

			appendStringInfo(&sid, "new.%s", pColDef->colname);

			colcnt++;
		} /* end for all cols */

		newVals[0] = '\0';
		snprintf(newVals, sizeof(newVals), "VALUES (%s)", sid.data);
		pfree(sid.data);
	}

	parent_tab_name = makeNode(RangeVar);
	parent_tab_name->catalogname = cxt->relation->catalogname;
	parent_tab_name->schemaname  = cxt->relation->schemaname;
	parent_tab_name->relname     = cxt->relation->relname;
	parent_tab_name->location    = -1;

	child_tab_name = makeNode(RangeVar);
	child_tab_name->catalogname = cxt->relation->catalogname;
	child_tab_name->schemaname  = cxt->relation->schemaname;
	child_tab_name->relname     = child_name_str;
	child_tab_name->location    = -1;

	snprintf(ruleBuf, sizeof(ruleBuf),
			 "CREATE RULE %s AS ON INSERT to %s WHERE %s DO INSTEAD INSERT INTO %s %s", child_name_str, parent_tab_name->relname, exprBuf, child_name_str, newVals);


	pIns = makeNode(InsertStmt);

	pResult = (Node *)pIns;
	pIns->relation = makeNode(RangeVar);
	pIns->relation->catalogname = NULL;
	pIns->relation->schemaname = NULL;
	pIns->relation->relname = "partition_rule";
	pIns->relation->location = -1;
	pIns->returningList = NULL;

	pIns->cols = NIL;

	if (1)
	{
		List *vl1 = NULL;

		A_Const *acs = makeNode(A_Const);
		acs->val.type = T_String;
		acs->val.val.str = pstrdup(parent_tab_name->relname);
		acs->typname = SystemTypeName("text");
        acs->location = -1;

		vl1 = list_make1(acs);

		acs = makeNode(A_Const);
		acs->val.type = T_String;
		acs->val.val.str = pstrdup(child_name_str);
		acs->typname = SystemTypeName("text");
        acs->location = -1;

		vl1 = lappend(vl1, acs);

		acs = makeNode(A_Const);
		acs->val.type = T_String;
		acs->val.val.str = pstrdup(exprBuf);
		acs->typname = SystemTypeName("text");
        acs->location = -1;

		vl1 = lappend(vl1, acs);

		acs = makeNode(A_Const);
		acs->val.type = T_String;
		acs->val.val.str = pstrdup(ruleBuf);
		acs->typname = SystemTypeName("text");
        acs->location = -1;

		vl1 = lappend(vl1, acs);

		pIns->selectStmt = (Node *)makeNode(SelectStmt);
		((SelectStmt *)pIns->selectStmt)->valuesLists =
				list_make1(vl1);

	}

	return (pResult);
} /* end make_prule_catalog */

static Node *
make_prule_rulestmt(ParseState *pstate,
		 CreateStmtContext *cxt, CreateStmt *stmt,
		 Node *partitionBy, PartitionElem *pElem,
 		 char *at_depth, char *child_name_str,
		 char *exprBuf,
		 Node *pWhere
		 )
{
	Node				*pResult = NULL;
	RuleStmt			*pRule   = NULL;
	InsertStmt			*pIns    = NULL;
	RangeVar			*parent_tab_name;
	RangeVar			*child_tab_name;

	parent_tab_name = makeNode(RangeVar);
	parent_tab_name->catalogname = cxt->relation->catalogname;
	parent_tab_name->schemaname  = cxt->relation->schemaname;
	parent_tab_name->relname     = cxt->relation->relname;
	parent_tab_name->location    = -1;

	child_tab_name = makeNode(RangeVar);
	child_tab_name->catalogname = cxt->relation->catalogname;
	child_tab_name->schemaname  = cxt->relation->schemaname;
	child_tab_name->relname     = child_name_str;
	child_tab_name->location    = -1;

	pIns = makeNode(InsertStmt);

	pRule = makeNode(RuleStmt);
	pRule->replace = false; /* do not replace */
	pRule->relation = parent_tab_name;
	pRule->rulename = pstrdup(child_name_str);
	pRule->whereClause = pWhere;
	pRule->event = CMD_INSERT;
	pRule->instead = true; /* do instead */
	pRule->actions = list_make1(pIns);

	pResult = (Node *)pRule;

	pIns->relation = makeNode(RangeVar);
	pIns->relation->catalogname = cxt->relation->catalogname;
	pIns->relation->schemaname  = cxt->relation->schemaname;
	pIns->relation->relname     = child_name_str;
	pIns->relation->location    = -1;

	pIns->returningList = NULL;

	pIns->cols = NIL;

	if (1)
	{
		List			*coldefs = stmt->tableElts;
		ListCell		*lc      = NULL;
		List *vl1 = NULL;

		lc = list_head(coldefs);

		for (; lc; lc = lnext(lc)) /* for all cols */
		{
			Node *pCol = lfirst(lc);
			ColumnDef *pColDef;
			ColumnRef *pCRef;

			if (nodeTag(pCol) != T_ColumnDef) /* avoid constraints, etc */
				continue;

			pCRef = makeNode(ColumnRef);

			pColDef = (ColumnDef *)pCol;

			pCRef->location = -1;
			/* NOTE: gram.y uses "*NEW*" for "new" */
			pCRef->fields = list_make2(makeString("*NEW*"),
									   makeString(pColDef->colname));

			vl1 = lappend(vl1, pCRef);
		}

		pIns->selectStmt = (Node *)makeNode(SelectStmt);
		((SelectStmt *)pIns->selectStmt)->valuesLists =
				list_make1(vl1);

	}


	return (pResult);
} /* end make_prule_rulestmt */

List *
make_partition_rules(ParseState *pstate,
					 CreateStmtContext *cxt, CreateStmt *stmt,
					 Node *partitionBy, PartitionElem *pElem,
					 char  *at_depth, char *child_name_str,
					 int partNumId, int maxPartNum,
					 int everyOffset, int maxEveryOffset,
					 ListCell	**pp_lc_anp,
					 bool doRuleStmt
		)
{
	PartitionBy			*pBy    = (PartitionBy *)partitionBy;
	Node				*pRule  = NULL;
	List				*allRules  = NULL;

	if (pBy->partType != PARTTYP_HASH)
	{
		Assert(pElem);
	}
	if (pBy->partType == PARTTYP_HASH)
	{
		List			*colElts = pBy->keys;
		ListCell		*lc      = NULL;
		char			 exprBuf[10000];
		StringInfoData   sid;
		int colcnt = 0;
		Node			*pHashArgs = NULL;
		Node			*pExpr = NULL;
		PartitionBoundSpec *pBSpec = NULL;

		if (pElem)
		{
			pBSpec = (PartitionBoundSpec *)pElem->boundSpec;
		}
		exprBuf[0] = '\0';

		initStringInfo(&sid);

		lc = list_head(colElts);

		for (; lc; lc = lnext(lc)) /* for all cols */
		{
			Node *pCol = lfirst(lc);
			Value *pColConst;
			ColumnRef *pCRef = NULL;

			pColConst = (Value *)pCol;

			Assert(IsA(pColConst, String));

			if (!deparse_partition_rule((Node *)pCol,
										exprBuf, sizeof(exprBuf)))
			{
				int loco = pBy->location;

				if (pBSpec)
					loco = pBSpec->location;

				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("unknown type or operator%s",
								at_depth),
						 parser_errposition(pstate, loco)));
			}

			pCRef = makeNode(ColumnRef);
			pCRef->location = -1;
			pCRef->fields = list_make1(pCol);

			if (colcnt)
			{
				Node *pAppOp = NULL;

				pAppOp =
						(Node *) makeSimpleA_Expr(AEXPR_OP, "||",
												  pHashArgs,
												  (Node *)pCRef,
												  -1);
				pHashArgs = pAppOp;

				appendStringInfo(&sid, "||");
			}
			else
			{
				pHashArgs = (Node *)pCRef;
			}

			appendStringInfo(&sid, "%s", exprBuf);

			colcnt++;
		} /* end for all cols */

		/* magic_hash: maximum number of partitions is maxPartNum
		   current partition number is parNumId
		*/

		/* modulus arithmetic is 0 to N-1, so go to (partNumId-1).
		 * Also, hashtext can return a negative, so fix it up */

		snprintf(exprBuf, sizeof(exprBuf),
/*				 "magic_hash(%d, %s) = %d", */
/* double % for % literal - ((hash(cols)%max + max) % max) */
				 "(hashtext(%s)%%%d + %d)%%%d = %d",
				 sid.data,
				 maxPartNum,
				 maxPartNum,
				 maxPartNum,
				 (partNumId-1)
				);

		pfree(sid.data);

		{
			Node *pPlusOp  = NULL;
			Node *pModOp  = NULL;
			A_Const *pModBy  = NULL;
			A_Const *pPartID = NULL;
			FuncCall *pFC = makeNode(FuncCall);

			pFC->funcname = list_make1(makeString("hashtext"));
			pFC->args = list_make1(pHashArgs);
			pFC->agg_star = FALSE;
			pFC->agg_distinct = FALSE;
			pFC->location = -1;
			pFC->over = NULL;

			pModBy  = makeNode(A_Const);
			pModBy->val.type = T_Integer;
			pModBy->val.val.ival = maxPartNum;
            pModBy->location = -1;

			pPartID  = makeNode(A_Const);
			pPartID->val.type = T_Integer;
			pPartID->val.val.ival = (partNumId - 1);
            pPartID->location = -1;

			pModOp = (Node *)makeSimpleA_Expr(AEXPR_OP, "%",
									  (Node *)pFC, (Node *)pModBy, -1);

			pPlusOp = (Node *)makeSimpleA_Expr(AEXPR_OP, "+",
									  (Node *)pModOp, (Node *)pModBy, -1);

			pModOp = (Node *)makeSimpleA_Expr(AEXPR_OP, "%",
									  (Node *)pPlusOp, (Node *)pModBy, -1);

			pExpr = (Node *)makeSimpleA_Expr(AEXPR_OP, "=",
									 pModOp, (Node *)pPartID, -1);

		}

		/* first the CHECK constraint, then the INSERT statement, then
		 * the RULE statement
		 */
		allRules = list_make1(pExpr);

		if (doRuleStmt)
		{
			pRule = make_prule_catalog(pstate,
									   cxt, stmt,
									   partitionBy, pElem,
									   at_depth, child_name_str,
									   exprBuf,
									   pExpr);

			allRules = lappend(allRules, pRule);

			pRule = make_prule_rulestmt(pstate,
										cxt, stmt,
										partitionBy, pElem,
										at_depth, child_name_str,
										exprBuf,
										pExpr);

			allRules = lappend(allRules, pRule);
		}


	} /* end if HASH */

	if (pBy->partType == PARTTYP_LIST)
	{
		Node			*pIndAND = NULL;
		Node			*pIndOR  = NULL;
		List			*colElts = pBy->keys;
		ListCell		*lc      = NULL;
		List			*valElts = NULL;
		ListCell		*lc_val  = NULL;
		ListCell		*lc_valent = NULL;
		char			 exprBuf[10000];
		char			 ANDBuf[10000];
		char			 ORBuf[10000];
		PartitionValuesSpec *spec = (PartitionValuesSpec *)pElem->boundSpec;

		exprBuf[0] = '\0';
		ANDBuf[0]  = '\0';
		ORBuf[0]   = '\0';

		valElts = spec->partValues;
		lc_valent = list_head(valElts);

		/* number of values is a multiple of the number of columns */
		if (lc_valent)
			lc_val = list_head((List *)lfirst(lc_valent));
		for ( ; lc_val ; ) /* for all vals */
		{
			lc = list_head(colElts);
			pIndAND = NULL;

			for (; lc; lc = lnext(lc)) /* for all cols */
			{
				Node *pCol = lfirst(lc);
				Node *pEq  = NULL;
				Value *pColConst;
				A_Const *pValConst;
				ColumnRef *pCRef;
				bool isnull = false;

				pCRef = makeNode(ColumnRef); /* need columnref for WHERE */

				if (NULL == lc_val)
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("mismatched columns for VALUES%s",
									at_depth),
							 parser_errposition(pstate, spec->location)));
				}

				pColConst = (Value *)pCol;
				pValConst = (A_Const *)lfirst(lc_val);

				Assert(IsA(pColConst, String));

				pCRef->location = -1;
				pCRef->fields = list_make1(pColConst);

				if (!(IsA(pValConst, A_Const)))
				{
					Const *c = (Const *)pValConst;
					Type typ;
					Form_pg_type pgtype;
					Datum dat;
					A_Const *aconst;
					Value *val;

					Assert(IsA(c, Const));

					if (c->constisnull)
					{
						isnull = true;
						aconst = NULL;
					}
					else
					{
						aconst = makeNode(A_Const);
						typ = typeidType(c->consttype);
						pgtype = (Form_pg_type)GETSTRUCT(typ);
						dat = OidFunctionCall1(pgtype->typoutput,c->constvalue);

						ReleaseType(typ);
						val = makeString(DatumGetCString(dat));
						aconst->val = *val;
                    	aconst->location = -1;
					}

					pValConst = aconst;
				}

				if (isnull)
				{
					NullTest *nt = makeNode(NullTest);
					nt->arg = (Expr *)pCRef;
					nt->nulltesttype = IS_NULL;
					pEq = (Node *)nt;
				}
				else
					/* equality expression: column = value */
					pEq = (Node *) makeSimpleA_Expr(AEXPR_OP,
													"=",
													(Node *)pCRef,
													(Node *)pValConst,
													-1 /* position */);


				if (!deparse_partition_rule((Node *)pEq,
											exprBuf, sizeof(exprBuf)))
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("unknown type or operator%s",
									at_depth),
							 parser_errposition(pstate, spec->location)));

				}

				/* for multiple cols - AND the matches eg:
				   (col = value) AND (col = value)
				*/
				if (pIndAND)
				{
					char *pfoo = pstrdup(ANDBuf);

					pIndAND =
						(Node *) makeA_Expr(AEXPR_AND, NIL,
											pIndAND,
											pEq,
											-1 /* position */);

					snprintf(ANDBuf, sizeof(ANDBuf), "((%s) and %s)",
							 exprBuf,
							 pfoo
						 );

					pfree(pfoo);
				}
				else
				{
					pIndAND = pEq;
					snprintf(ANDBuf, sizeof(ANDBuf), "(%s)", exprBuf);
				}

				lc_val = lnext(lc_val);
			} /* end for all cols */

			/* if more VALUES than columns, then multiple matching
			   conditions, so OR them eg:

			   ((col = value) AND (col = value)) OR
			   ((col = value) AND (col = value)) OR
			   ((col = value) AND (col = value))

			*/

			if (pIndOR)
			{
				char *pfoo = pstrdup(ORBuf);

				pIndOR =
					(Node *) makeA_Expr(AEXPR_OR, NIL,
										pIndOR,
										pIndAND,
										-1 /* position */);

				snprintf(ORBuf, sizeof(ORBuf), "((%s) OR %s)",
						 ANDBuf,
						 pfoo
						 );

					pfree(pfoo);

			}
			else
			{
				pIndOR = pIndAND;
				snprintf(ORBuf, sizeof(ORBuf), "(%s)", ANDBuf);
			}
			if (lc_val == NULL)
			{
				lc_valent = lnext(lc_valent);
				if (lc_valent)
					lc_val = list_head((List *)lfirst(lc_valent));
			}

		} /* end for all vals */

		/* first the CHECK constraint, then the INSERT statement, then
		 * the RULE statement
		 */
		allRules = list_make1(pIndOR);

		if (doRuleStmt)
		{
			pRule = make_prule_catalog(pstate,
									   cxt, stmt,
									   partitionBy, pElem,
									   at_depth, child_name_str,
									   ORBuf,
									   pIndOR);

			allRules = lappend(allRules, pRule);

			pRule = make_prule_rulestmt(pstate,
										cxt, stmt,
										partitionBy, pElem,
										at_depth, child_name_str,
										ORBuf,
										pIndOR);

			allRules = lappend(allRules, pRule);
		}
	} /* end if LIST */

	/*
	     For RANGE partitions with an EVERY clause, the following
		 fields are defined:
		 int everyOffset - 0 for no EVERY, else 1 to maxEveryOffset
		 int maxEveryOffset - 1 for no EVERY, else >1
		 ListCell	**pp_lc_anp - the pointer to the pointer
		                          to the ListCell of [A]ll[N]ew[P]artitions,
								  a list of lists of stringified values
								  (as opposed to A_Const's).
	*/

	if (pBy->partType == PARTTYP_RANGE)
	{
		Node			*pIndAND = NULL;
		Node			*pIndOR  = NULL;
		List			*colElts = pBy->keys;
		ListCell		*lc      = NULL;
		List			*valElts = NULL;
		ListCell		*lc_val  = NULL;
		PartitionRangeItem *pRI  = NULL;
		char			 exprBuf[10000];
		char			 ANDBuf[10000];
		char			 ORBuf[10000];
		int				 range_idx;
		PartitionBoundSpec	*pBSpec = NULL;

		ListCell		*lc_every_val = NULL;
		List *allNewCols = NIL;

		if (pElem)
		{
			pBSpec = (PartitionBoundSpec *)pElem->boundSpec;
		}
		exprBuf[0] = '\0';
		ANDBuf[0]  = '\0';
		ORBuf[0]   = '\0';

		pRI = (PartitionRangeItem *)(pBSpec->partStart);

		if (pRI)
			valElts = pRI->partRangeVal;
		else
			valElts = NULL;

		if (maxEveryOffset > 1) /* if have an EVERY clause */
		{
			ListCell	*lc_anp = NULL;

			/* check the list of "all new partitions" */
			if (pp_lc_anp)
			{
				lc_anp = *pp_lc_anp;
				if (lc_anp)
				{
					allNewCols = lfirst(lc_anp);

					/* find the list of columns for a new partition */
					if (allNewCols)
					{
						lc_every_val = list_head(allNewCols);
					}
				}
			}
		} /* end if every */

		/* number of values must equal of the number of columns */

		for (range_idx = 0; range_idx < 2; range_idx++)
		{
			char *expr_op = ">";

			if (everyOffset > 1) /* for generated START for EVERY */
				expr_op = ">=";  /* always be inclusive           */
			else
				/* only inclusive set that way */
				if (pRI && (PART_EDGE_INCLUSIVE == pRI->partedge))
					expr_op = ">=";

			if (range_idx) /* Only do it for the upper bound iteration */
			{
				pRI = (PartitionRangeItem *)(pBSpec->partEnd);

				if (pRI)
					valElts = pRI->partRangeVal;
				else
					valElts = NULL;

				expr_op = "<";

				/* for generated END for EVERY always be exclusive */
				if ((everyOffset + 1) <  maxEveryOffset)
					expr_op = "<";
				else
					/* only be inclusive if set that way */
					if (pRI && (PART_EDGE_INCLUSIVE == pRI->partedge))
						expr_op = "<=";

				/* If have EVERY, and not the very first START or last END */
				if ((0 != everyOffset) && (everyOffset+1 <= maxEveryOffset))
				{
					if (*pp_lc_anp)
					{
						if (everyOffset != 1)
							*pp_lc_anp = lnext(*pp_lc_anp);

						if (*pp_lc_anp)
						{
							allNewCols = lfirst(*pp_lc_anp);

							if (allNewCols)
								lc_every_val = list_head(allNewCols);
						}
					}
				}
			} /* end if range_idx != 0 */

			lc_val = list_head(valElts);

			for ( ; lc_val ; ) /* for all vals */
			{
				lc = list_head(colElts);
				pIndAND = NULL;

				for (; lc; lc = lnext(lc)) /* for all cols */
				{
					Node *pCol = lfirst(lc);
					Node *pEq  = NULL;
					Value *pColConst;
					A_Const *pValConst;
					ColumnRef *pCRef;

					pCRef = makeNode(ColumnRef); /* need columnref for WHERE */

					if (NULL == lc_val)
					{
						char *st_end = "START";

						if (range_idx)
						{
							st_end = "END";
						}

						ereport(ERROR,
								(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
								 errmsg("too few columns in %s specification%s",
										st_end,
										at_depth),
								 parser_errposition(pstate, pBSpec->location)));
					}

					pColConst = (Value *)pCol;
					pValConst = (A_Const *)lfirst(lc_val);

					Assert(IsA(pColConst, String));

					pCRef->location = -1;
					pCRef->fields = list_make1(pColConst);

					if (!(IsA(pValConst, A_Const)))
					{
						Const *c = (Const *)pValConst;
						Type typ;
						Form_pg_type pgtype;
						Datum dat;
						A_Const *aconst = makeNode(A_Const);
						Value *val;

						Assert(IsA(c, Const));

						typ = typeidType(c->consttype);
						pgtype = (Form_pg_type)GETSTRUCT(typ);
						dat = OidFunctionCall1(pgtype->typoutput,c->constvalue);

						val= makeString(DatumGetCString(dat));
						aconst->val = *val;
                        aconst->location = -1;
						ReleaseType(typ);

						pValConst = aconst;
					}

					pEq = (Node *) makeSimpleA_Expr(AEXPR_OP,
													expr_op,
													(Node *)pCRef,
													(Node *)pValConst,
													-1 /* position */);

					if (!deparse_partition_rule((Node *)pEq,
												exprBuf, sizeof(exprBuf)))
					{
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
								 errmsg("unknown type or operator%s",
										at_depth),
								 parser_errposition(pstate, pBSpec->location)));

					}


					/* for multiple cols - AND the matches eg:
					   (col = value) AND (col = value)
					*/
					if (pIndAND)
					{
						char *pfoo = pstrdup(ANDBuf);

						pIndAND =
							(Node *) makeA_Expr(AEXPR_AND, NIL,
												pIndAND,
												pEq,
												-1 /* position */);

						snprintf(ANDBuf, sizeof(ANDBuf), "((%s) and %s)",
								 exprBuf,
								 pfoo
							 );

						pfree(pfoo);
					}
					else
					{
						pIndAND = pEq;
						snprintf(ANDBuf, sizeof(ANDBuf), "(%s)", exprBuf);
					}

					lc_val = lnext(lc_val);

					if (((0 == range_idx)
						 && (everyOffset > 1))
						|| ((1 == range_idx)
							&& (everyOffset+1 < maxEveryOffset))
							)
					{

						if (lc_every_val)
						{
							lc_every_val = lnext(lc_every_val);
						}
					}
				} /* end for all cols */

				/* if more VALUES than columns, then complain
				*/
				if (lc_val)
				{
					char *st_end = "START";

					if (range_idx)
					{
						st_end = "END";
					}

					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("too many columns in %s specification%s",
									st_end,
									at_depth),
							 parser_errposition(pstate, pBSpec->location)));
				}

				if (pIndOR)
				{
					char *pfoo = pstrdup(ORBuf);
					/* XXX XXX build an AND for now.  But later we
					 * split this to distinguish START and END
					 * conditions */

					pIndOR =
						(Node *) makeA_Expr(AEXPR_AND, NIL,
											pIndOR,
											pIndAND,
											-1 /* position */);

					snprintf(ORBuf, sizeof(ORBuf), "((%s) AND %s)",
							 ANDBuf,
							 pfoo
							 );

						pfree(pfoo);

				}
				else
				{
					pIndOR = pIndAND;
					snprintf(ORBuf, sizeof(ORBuf), "(%s)", ANDBuf);
				}

			} /* end for all vals */

		} /* end for range_idx */

		/* first the CHECK constraint, then the INSERT statement, then
		 * the RULE statement
		 */
		allRules = list_make1(pIndOR);

		if (doRuleStmt)
		{
			pRule = make_prule_catalog(pstate,
									   cxt, stmt,
									   partitionBy, pElem,
									   at_depth, child_name_str,
									   ORBuf,
									   pIndOR);
			allRules = lappend(allRules, pRule);

			pRule = make_prule_rulestmt(pstate,
										cxt, stmt,
										partitionBy, pElem,
										at_depth, child_name_str,
										ORBuf,
										pIndOR);
			allRules = lappend(allRules, pRule);
		}
	} /* end if RANGE */

	return allRules;
} /* end make_partition_rules */

/* XXX: major cleanup required. Get rid of gotos at least */
static int
partition_range_compare(ParseState *pstate, CreateStmtContext *cxt,
						CreateStmt *stmt, PartitionBy *pBy,
						char *at_depth, int partNumber,
						char *compare_op, /* =, <, > only */
						PartitionRangeItem *pRI1,
						PartitionRangeItem *pRI2)
{
	int rc = -1;
	ListCell		*lc1 = NULL;
	ListCell		*lc2 = NULL;
	List			*cop = lappend(NIL, makeString(compare_op));

	if (!pRI1 || !pRI2) /* error */
		return rc;

	lc1 = list_head(pRI1->partRangeVal);
	lc2 = list_head(pRI2->partRangeVal);
L_redoLoop:
	for ( ; lc1 && lc2; )
	{
		Node *n1 = lfirst(lc1);
		Node *n2 = lfirst(lc2);

		if (!equal(n1, n2))
			break;

		lc1 = lnext(lc1);
		lc2 = lnext(lc2);
	}

	if (!lc1 && !lc2) /* both empty, so all values are equal */
	{
		if (strcmp("=", compare_op) == 0)
			return 1;
		else
			return 0;
	}
	else if (lc1 && lc2) /* both not empty, so last value was different */
	{
		Datum res;
		Node *n1 = lfirst(lc1);
		Node *n2 = lfirst(lc2);

		/* XXX XXX: can't trust equal() code on "typed" data like date
		 * types (because it compares things like "location") so do a
		 * real compare using eval .
		 */
		res = eval_basic_opexpr(pstate, cop, n1, n2, NULL, NULL, NULL, -1);

		if (DatumGetBool(res) && strcmp("=", compare_op) == 0)
		{
			/* surprise! they were equal after all.  So keep going... */
			lc1 = lnext(lc1);
			lc2 = lnext(lc2);

			goto L_redoLoop;
		}

		return DatumGetBool(res);
	}
	else if (lc1 || lc2)
	{
		/* lists of different lengths */
		if (strcmp("=", compare_op) == 0)
			return 0;

		/* longer list is bigger? */

		if (strcmp("<", compare_op) == 0)
		{
			if (lc1)
				return 0;
			else
				return 1;
		}
		if (strcmp(">", compare_op) == 0)
		{
			if (lc1)
				return 1;
			else
				return 0;
		}
	}

	return rc;
} /* end partition_range_compare */

static int
part_el_cmp(void *a, void *b, void *arg)
{
	RegProcedure **sortfuncs = (RegProcedure **)arg;
	PartitionElem *el1 = (PartitionElem *)a;
	PartitionElem *el2 = (PartitionElem *)b;
	PartitionBoundSpec *bs1;
	PartitionBoundSpec *bs2;
	int i;
	List *start1 = NIL, *start2 = NIL;
	ListCell *lc1, *lc2;

	/*
	 * We call this function from all over the place so don't assume that
	 * things are valid.
	 */

	if (!el1)
		return -1;
	else if (!el2)
		return 1;

	if (el1->isDefault)
		return -1;

	if (el2->isDefault)
		return 1;

	bs1 = (PartitionBoundSpec *)el1->boundSpec;
	bs2 = (PartitionBoundSpec *)el2->boundSpec;

	if (!bs1)
		return -1;
	if (!bs2)
		return 1;

	if (bs1->partStart)
		start1 = ((PartitionRangeItem *)bs1->partStart)->partRangeVal;
	if (bs2->partStart)
		start2 = ((PartitionRangeItem *)bs2->partStart)->partRangeVal;

	if (!start1 && !start2)
	{
		/* these cases are syntax errors but they'll be picked up later */
		if (!bs1->partEnd && !bs2->partEnd)
			return 0;
		else if (!bs1->partEnd)
			return 1;
		else if (!bs2->partEnd)
			return -1;
		else
		{
			List *end1 = ((PartitionRangeItem *)bs1->partEnd)->partRangeVal;
			List *end2 = ((PartitionRangeItem *)bs2->partEnd)->partRangeVal;

			i = 0;
			forboth(lc1, end1, lc2, end2)
			{
				Const *c1 = lfirst(lc1);
				Const *c2 = lfirst(lc2);

				/* use < */
				RegProcedure sortFunction = sortfuncs[0][i++];

				if (DatumGetBool(OidFunctionCall2(sortFunction, c1->constvalue,
												  c2->constvalue)))
					return -1; /* a < b */
				if (DatumGetBool(OidFunctionCall2(sortFunction, c2->constvalue,
												  c1->constvalue)))
					return 1;
			}
			/* equal */
			return 0;
		}
	}
	else if (!start1 && start2)
	{
		/*
		 * compare end1 to start2. Whether the end and start are inclusive
		 * or exclusive is very important here. For example, if
		 * end1 is exclusive and start2 is inclusive, their being equal means
		 * that end1 finishes *before* start2.
		 */

		PartitionRangeItem *pri1 = (PartitionRangeItem *)bs1->partEnd;
		PartitionRangeItem *pri2 = (PartitionRangeItem *)bs2->partStart;
		List *end1 = pri1->partRangeVal;
		PartitionEdgeBounding pe1 = pri1->partedge;
		PartitionEdgeBounding pe2 = pri2->partedge;

		Assert(pe1 != PART_EDGE_UNSPECIFIED);
		Assert(pe2 != PART_EDGE_UNSPECIFIED);

		i = 0;
		forboth(lc1, end1, lc2, start2)
		{
			Const *c1 = lfirst(lc1);
			Const *c2 = lfirst(lc2);
			RegProcedure sortFunction;

			sortFunction = sortfuncs[0][i];

			/* try < first */
			if (DatumGetBool(OidFunctionCall2(sortFunction, c1->constvalue,
											  c2->constvalue)))
				return -1;

			/* see if they're equal */
			sortFunction = sortfuncs[1][i];

			if (DatumGetBool(OidFunctionCall2(sortFunction, c1->constvalue,
											  c2->constvalue)))
			{
				/* equal, but that might actually mean < */
				if (pe1 == PART_EDGE_EXCLUSIVE)
					return -1; /* it's less than */
				else if (pe1 == PART_EDGE_INCLUSIVE &&
						 pe2 == PART_EDGE_EXCLUSIVE)
					return -1; /* it's less than */

				/* otherwise, they're equal */
			}
			else
				return 1;

			i++;
		}
		return 0;
	}
	else if (start1 && !start2)
	{
		/* opposite of above */
		return -part_el_cmp(b, a, arg);
	}
	else
	{
		/* we have both starts */
		PartitionRangeItem *pri1 = (PartitionRangeItem *)bs1->partStart;
		PartitionRangeItem *pri2 = (PartitionRangeItem *)bs2->partStart;
		PartitionEdgeBounding pe1 = pri1->partedge;
		PartitionEdgeBounding pe2 = pri2->partedge;
		i = 0;
		forboth(lc1, start1, lc2, start2)
		{
			Const *c1 = lfirst(lc1);
			Const *c2 = lfirst(lc2);

			/* use < */
			RegProcedure sortFunction = sortfuncs[0][i];

			if (DatumGetBool(OidFunctionCall2(sortFunction, c1->constvalue,
											  c2->constvalue)))
				return -1; /* a < b */

			sortFunction = sortfuncs[1][i];
			if (DatumGetBool(OidFunctionCall2(sortFunction, c1->constvalue,
											  c2->constvalue)))
			{
				if (pe1 == PART_EDGE_INCLUSIVE &&
					pe2 == PART_EDGE_EXCLUSIVE)
					return -1; /* actually, it was < */
				else if (pe1 == PART_EDGE_EXCLUSIVE &&
						 pe2 == PART_EDGE_INCLUSIVE)
					return 1;

			}
			else
				return 1;

			i++;
		}
	}

	/* all equal */
	return 0;
}

static List *
sort_range_elems(List *opclasses, List *elems)
{
	ListCell *lc;
	PartitionElem *clauses;
	RegProcedure *sortfuncs[2];
	int i;
	List *newelems = NIL;

	sortfuncs[0] = palloc(list_length(opclasses) *
						  sizeof(RegProcedure));
	sortfuncs[1] = palloc(list_length(opclasses) *
						  sizeof(RegProcedure));
	i = 0;

	foreach(lc, opclasses)
	{
		Oid			opclass = lfirst_oid(lc);
		Oid			intype = get_opclass_input_type(opclass);
		Oid			opfam = get_opclass_family(opclass);
		Oid			opoid;

		/* < first */
		opoid = get_opfamily_member(opfam, intype, intype, BTLessStrategyNumber);
		sortfuncs[0][i] = get_opcode(opoid);

		opoid = get_opfamily_member(opfam, intype, intype, BTEqualStrategyNumber);

		sortfuncs[1][i] = get_opcode(opoid);
		i++;
	}

	i = 0;
	clauses = palloc(sizeof(PartitionElem) * list_length(elems));

	foreach(lc, elems)
		clauses[i++] = *(PartitionElem *)lfirst(lc);

	qsort_arg(clauses, list_length(elems), sizeof(PartitionElem),
			  (qsort_arg_comparator) part_el_cmp, (void *)sortfuncs);


	for (i = 0; i < list_length(elems); i++)
		newelems = lappend(newelems, &clauses[i]);

	return newelems;
}

static void
preprocess_range_spec(partValidationState *vstate)
{
	PartitionSpec *spec = (PartitionSpec *)vstate->pBy->partSpec;
	ParseState *pstate = vstate->pstate;
	ListCell *lc;
	List *plusop = list_make2(makeString("pg_catalog"), makeString("+"));
	List *ltop = list_make2(makeString("pg_catalog"), makeString("<"));
	List *coltypes = NIL;
	List *stenc;
	List *newelts = NIL;

	foreach(lc, vstate->pBy->keys)
	{
		char *colname = strVal(lfirst(lc));
		bool found = false;
		ListCell *lc2;
		TypeName *typname = NULL;

		foreach(lc2, vstate->cxt->columns)
		{
			ColumnDef *column = lfirst(lc2);

			if (strcmp(column->colname, colname) == 0)
			{
				found = true;
				if (!OidIsValid(column->typname->typid))
				{
					Type type = typenameType(vstate->pstate, column->typname);
					column->typname->typid = typeTypeId(type);
					ReleaseType(type);
				}
				typname = column->typname;
				break;
			}
		}
		Assert(found);
		coltypes = lappend(coltypes, typname);
	}

	partition_range_every(pstate, vstate->pBy, coltypes, vstate->at_depth,
						  vstate);

	/*
	 * Must happen after partition_range_every(), since that gathers up
	 * encoding clauses for us.
	 */
	stenc = spec->enc_clauses;

	/*
	 * Iterate over the elements in a given partition specification and
	 * expand any EVERY clauses into raw elements.
	 */
	foreach(lc, spec->partElem)
	{
		PartitionElem	*el			= lfirst(lc);
		PartitionBoundSpec *pbs = (PartitionBoundSpec *)el->boundSpec;

		Node			*pStoreAttr	= NULL;
		ListCell		*lc2;
		bool			 bTablename = false;

		if (IsA(el, ColumnReferenceStorageDirective))
		{
			stenc = lappend(stenc, el);
			continue;
		}

		/* we might not have a boundary spec if user just specified DEFAULT */
		if (!pbs)
		{
			newelts = lappend(newelts, el);
			continue;
		}

		pbs = (PartitionBoundSpec *)transformExpr(pstate, (Node *)pbs);

		pStoreAttr = el->storeAttr;

		/* MPP-6297: check for WITH (tablename=name) clause
		 * [only for dump/restore, set deep in the guts of
		 * partition_range_every...]
		 */
		bTablename = (NULL != pbs->pWithTnameStr);

		if (!bTablename && pbs->partEvery)
		{
			/* the start expression might come from a previous end
			 * expression
			 */
			List *start = NIL;
			List *curstart = NIL;
			List *end = ((PartitionRangeItem *)pbs->partEnd)->partRangeVal;
			List *newend = NIL;
			List *every =
				(List *)((PartitionRangeItem *)pbs->partEvery)->partRangeVal;
			List *everyinc;
			bool lessthan = true;
			int everycount = 0;
			List *everyelts = NIL;
			bool first = true;
			List *everytypes = NIL;
			int i;

			Assert(pbs->partStart);
			Assert(pbs->partEnd);
			Assert(IsA(end, List));
			Assert(IsA(every, List));

			/* we modify every, so copy it */
			everyinc = copyObject(every);

			while (lessthan)
			{
				Datum res;
				Oid restypid;
				PartitionElem *newel;
				PartitionBoundSpec *newbs;
				PartitionRangeItem *newri;
				ListCell *lctypes, *lceveryinc;
				ListCell *lcstart, *lcend, *lcevery, *lccol, *lclastend;
				List *lastend = NIL;

				/*
				 * Other parts of the parser want to know how many clauses
				 * we expanded here.
				 */
				everycount++;

				if (start == NIL)
				{
					start = ((PartitionRangeItem *)pbs->partStart)->partRangeVal;

					lcend = list_head(end);
					/* coerce to target type */
					forboth(lcstart, start, lccol, coltypes)
					{
						TypeName *typ = lfirst(lccol);
						Node *newnode;
						Oid			typid = typenameTypeId(vstate->pstate, typ);
						int32		typmod = typenameTypeMod(vstate->pstate,
													 typ, typid);

						newnode = coerce_partition_value(lfirst(lcstart),
														 typid, typmod,
														 PARTTYP_RANGE);

						lfirst(lcstart) = newnode;

						/* be sure we coerce the end value */
						newnode = coerce_partition_value(lfirst(lcend),
														 typid,
														 typmod,
														 PARTTYP_RANGE);
						lfirst(lcend) = newnode;
						lcend = lnext(lcend);
					}

					curstart = copyObject(start);
					forboth(lccol, coltypes, lcevery, every)
					{
						TypeName *typ = lfirst(lccol);
						HeapTuple optup;
						List *opname = list_make2(makeString("pg_catalog"),
												  makeString("+"));
						Node *e = lfirst(lcevery);
						Oid rtypeId = exprType(e);
						Oid newrtypeId;

						/* first, make sure we can build up an operator */
						optup = oper(pstate, opname, typ->typid, rtypeId,
									 true, -1);
						if (!HeapTupleIsValid(optup))
							ereport(ERROR,
									(errcode(ERRCODE_SYNTAX_ERROR),
									 errmsg("could not identify operator for partitioning operation between type \"%s\" and type \"%s\"",
											format_type_be(typ->typid),
											format_type_be(rtypeId)),
  									 errhint("Add an explicit cast to the partitioning parameters")));


						newrtypeId = ((Form_pg_operator)GETSTRUCT(optup))->oprright;
						ReleaseOperator(optup);

						if (rtypeId != newrtypeId)
						{
							Type newetyp = typeidType(newrtypeId);
							int4 typmod =
								((Form_pg_type)GETSTRUCT(newetyp))->typtypmod;

							ReleaseType(newetyp);

							/* we need to coerce */
							e  = coerce_partition_value(e,
													   newrtypeId,
													   typmod,
													   PARTTYP_RANGE);

							lfirst(lcevery) = e;
							rtypeId = newrtypeId;
						}

						everytypes = lappend_oid(everytypes, rtypeId);
					}
				}
				else
				{
					curstart = newend;
					lastend = newend;
					newend = NIL;
				}

				lcend = list_head(end);
				lcevery = list_head(everyinc);
				lclastend = list_head(lastend);

				forboth(lcstart, start, lccol, coltypes)
				{
					Const *mystart = lfirst(lcstart);
					TypeName *type = lfirst(lccol);
					Const *myend;
					Const *clauseend;
					Const *clauseevery;
					Oid			typid = typenameTypeId(vstate->pstate, type);
					int32		typmod = typenameTypeMod(vstate->pstate,
													 type, typid);
					int16 len = get_typlen(typid);
					bool typbyval = get_typbyval(typid);

					Assert(lcevery);
					Assert(lcend);

					clauseevery = lfirst(lcevery);
					clauseend = lfirst(lcend);

					/* add the every value to the start */
					res = eval_basic_opexpr(pstate, plusop, (Node *)mystart,
											(Node *)clauseevery,
											&typbyval, &len, &typid,
										   	-1);

					myend = makeConst(typid, typmod, len,
									  datumCopy(res, typbyval, len),
									  false, typbyval);

					/* make sure res is bigger than the last value */
					if (lclastend)
					{
						Oid typ = InvalidOid;
						Const *prevval = (Const *)lfirst(lclastend);
						Datum is_lt = eval_basic_opexpr(pstate, ltop,
														(Node *)prevval,
														(Node *)myend,
														NULL, NULL, &typ,
														-1);

						if (!DatumGetBool(is_lt))
							elog(ERROR, "every interval too small");
					}
					restypid = InvalidOid;
					res = eval_basic_opexpr(pstate, ltop, (Node *)myend,
										(Node *)clauseend, NULL, NULL,
										&restypid, -1);
					if (!DatumGetBool(res))
					{
						newend = end;
						lessthan = false;
						break;
					}
					newend = lappend(newend, myend);

					lcend = lnext(lcend);
					lcevery = lnext(lcevery);
					if (lclastend)
						lclastend = lnext(lclastend);
				}

				lctypes = list_head(everytypes);
				forboth(lcevery, every, lceveryinc, everyinc)
				{
					Oid typid = lfirst_oid(lctypes);
					bool byval = get_typbyval(typid);
					int16 typlen = get_typlen(typid);
					Const *c;

					/* increment every */
					res = eval_basic_opexpr(pstate, plusop,
											(Node *)lfirst(lcevery),
											(Node *)lfirst(lceveryinc),
											NULL, NULL,
											&typid, -1);

					c = makeConst(typid, -1, typlen, res, false, byval);
					pfree(lfirst(lceveryinc));
					lfirst(lceveryinc) = c;
				}

				newel = makeNode(PartitionElem);
				newel->subSpec = copyObject(el->subSpec);
				newel->storeAttr = copyObject(el->storeAttr);
				newel->AddPartDesc = copyObject(el->AddPartDesc);
				newel->location = el->location;

				newbs = makeNode(PartitionBoundSpec);

				newel->boundSpec = (Node *)newbs;

				/* start */
				newri = makeNode(PartitionRangeItem);

				/* modifier only relevant on the first iteration */
				if (everycount == 1)
					newri->partedge =
						((PartitionRangeItem *)pbs->partStart)->partedge;
				else
					newri->partedge = PART_EDGE_INCLUSIVE;

				newri->location =
					((PartitionRangeItem *)pbs->partStart)->location;
				newri->partRangeVal = curstart;
				newbs->partStart = (Node *)newri;

				/* end */
				newri = makeNode(PartitionRangeItem);
				newri->partedge = PART_EDGE_EXCLUSIVE;
				newri->location =
					((PartitionRangeItem *)pbs->partEnd)->location;
				newri->partRangeVal = newend;
				newbs->partEnd = (Node *)newri;

				/* every */
				newbs->partEvery = (Node *)copyObject(pbs->partEvery);
				newbs->location = pbs->location;
				newbs->everyGenList = pbs->everyGenList;

				everyelts = lappend(everyelts, newel);

				if (first)
					first = false;
			}

			/*
			 * Update the final PartitionElem's partEnd modifier if it isn't
			 * the default
			 */
			if (((PartitionRangeItem *)pbs->partEnd)->partedge !=
				PART_EDGE_EXCLUSIVE)
			{
				PartitionElem *elem = lfirst(list_tail(everyelts));
				PartitionBoundSpec *s = (PartitionBoundSpec *)elem->boundSpec;
				PartitionRangeItem *ri = (PartitionRangeItem *)s->partEnd;

				ri->partedge = ((PartitionRangeItem *)pbs->partEnd)->partedge;
			}

			/* add everycount to each EVERY clause */
			i = 0;
			foreach(lc2, everyelts)
			{
				PartitionElem *el2 = lfirst(lc2);
				PartitionBoundSpec *pbs = (PartitionBoundSpec *)el2->boundSpec;
				PartitionRangeItem *ri = (PartitionRangeItem *)pbs->partEvery;

				ri->everycount = everycount;

				/*
				 * Generate the new name, which is parname + every count
				 * but only if every expands to more than one partition.
				*/
				if (el->partName)
				{
					char newname[sizeof(NameData) + 10];

					if (list_length(everyelts) > 1)
						snprintf(newname, sizeof(newname),
								 "%s_%u", strVal(el->partName),
								 ++i);
					else
						snprintf(newname, sizeof(newname),
								 "%s", strVal(el->partName));

					if (strlen(newname) > NAMEDATALEN)
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
								 errmsg("partition name \"%s\" too long",
										strVal(el->partName)),
						 parser_errposition(vstate->pstate, el->location)));

					el2->partName = (Node *)makeString(pstrdup(newname));
				}
			}

			newelts = list_concat(newelts, everyelts);
		}
		else
		{
			if (pbs->partStart)
			{
				ListCell *lccol;
				ListCell *lcstart;
				List *start =
					((PartitionRangeItem *)pbs->partStart)->partRangeVal;

				/* coerce to target type */
				forboth(lcstart, start, lccol, coltypes)
				{
					Node *mystart = lfirst(lcstart);
					TypeName *typ = lfirst(lccol);
					Node *newnode;
					Oid			typid = typenameTypeId(vstate->pstate, typ);
					int32		typmod = typenameTypeMod(vstate->pstate,
													 typ, typid);
					newnode = coerce_partition_value(mystart,
													 typid, typmod,
													 PARTTYP_RANGE);

					lfirst(lcstart) = newnode;
				}
			}

			if (pbs->partEnd)
			{
				List *end = ((PartitionRangeItem *)pbs->partEnd)->partRangeVal;
				ListCell *lccol;
				ListCell *lcend;

				/* coerce to target type */
				forboth(lcend, end, lccol, coltypes)
				{
					Node *myend = lfirst(lcend);
					TypeName *typ = lfirst(lccol);
					Node *newnode;
					Oid			typid = typenameTypeId(vstate->pstate, typ);
					int32		typmod = typenameTypeMod(vstate->pstate,
														 typ, typid);

					newnode = coerce_partition_value(myend,
													 typid, typmod,
													 PARTTYP_RANGE);

					lfirst(lcend) = newnode;
				}
			}
			newelts = lappend(newelts, el);
		}
	}

	/* do an initial sort */
	spec->partElem = sort_range_elems(vstate->pBy->keyopclass, newelts);
	spec->enc_clauses = stenc;
}

static bool
range_partition_walker(Node *node, void *context)
{
	range_partition_ctx *ctx = (range_partition_ctx *)context;
	if (node == NULL)
		return false;
	else if (IsA(node, Const))
	{
		Const *c = (Const *)node;

		if (c->constisnull)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("cannot use NULL with range partition specification"),
					 parser_errposition(ctx->pstate, ctx->location)));
		return false;
	}
	else if (IsA(node, A_Const))
	{
		A_Const *c = (A_Const *)node;
		if (IsA(&c->val, Null))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("cannot use NULL with range partition specification"),
					 parser_errposition(ctx->pstate, ctx->location)));
		return false;
	}
	return expression_tree_walker(node, range_partition_walker, ctx);
}

void
PartitionRangeItemIsValid(ParseState *pstate, PartitionRangeItem *pri)
{
	ListCell *lc;
	range_partition_ctx ctx;

	if (!pri)
		return;

	ctx.pstate = pstate;
	ctx.location = pri->location;

	foreach(lc, pri->partRangeVal)
	{
		range_partition_walker(lfirst(lc), &ctx);
	}
}

/*
 * Basic partition validation:
 * Check that PARTITIONS matches specification (for HASH). Perform basic error
 * checking on boundary specifications.
 */
static void
validate_range_partition(partValidationState *vstate)
{
	bool bAppendRange = false;
	PartitionBoundSpec *prevBSpec = NULL;
	PartitionBoundSpec *spec = (PartitionBoundSpec *)vstate->spec;

	vstate->spec = (Node *)spec;

	if (spec)
	{
		/* XXX: create a type2name function */
		char *specTName = "LIST";

		if (IsA(spec, PartitionValuesSpec))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("invalid use of %s boundary "
							"specification in partition clause",
							specTName),
						 /* MPP-4249: use value spec location if have one */
					errOmitLocation(true),
			 parser_errposition(vstate->pstate,
								((PartitionValuesSpec*)spec)->location)));
		}
	}
	else
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("missing boundary specification"),
				 parser_errposition(vstate->pstate, vstate->pElem->location)));

	}

	{
		/* if the previous partition was named, and
		 * current is not, and the prev and current
		 * use single, complementary START/END specs,
		 * then complain.
		 */

		/* must have a valid RANGE pBSpec now or else
		 * would have error'd out...
		 */
		int currStartEnd = 0;

		if (spec->partStart)
			currStartEnd += 1;
		if (spec->partEnd)
			currStartEnd += 2;

		/* Note: for first loop, prevStartEnd = 0, so
		 * this test is ok */

		if (((vstate->prevHadName && !(vstate->pElem->partName))
			 || (!vstate->prevHadName && vstate->pElem->partName))
			&&
			(((vstate->prevStartEnd == 1)
			  && (currStartEnd == 2))
			 ||
			 ((vstate->prevStartEnd == 2)
			  && (currStartEnd == 1)))
				)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("invalid use of mixed named and "
							"unnamed RANGE boundary "
							"specifications%s",
							vstate->at_depth),
					 parser_errposition(vstate->pstate,
										spec->location)));

		}

		vstate->prevStartEnd = currStartEnd;
	}

	vstate->prevHadName  = !!(vstate->pElem->partName); /* bool t/f */

	if (spec->partStart)
		PartitionRangeItemIsValid(vstate->pstate, (PartitionRangeItem *)spec->partStart);
	if (spec->partEnd)
		PartitionRangeItemIsValid(vstate->pstate, (PartitionRangeItem *)spec->partEnd);

	/*
	 * Fixup boundaries for previous partition ending if necessary
	 */
	if (!vstate->prevElem)
	{
		bAppendRange = true;
		goto L_setprevElem;
	}

	prevBSpec = (PartitionBoundSpec *)vstate->prevElem->boundSpec;
	/* XXX XXX: can check for overlap here too */

	if (!prevBSpec)
	{
		/* XXX XXX: if the previous partition declaration
		 * does not have a boundary spec then its end
		 * needs to be the start of the current */

		bAppendRange = true;
		goto L_setprevElem;
	}

	if (spec->partStart)
	{
		if (!prevBSpec->partEnd)
		{
			PartitionRangeItem *pRI = (PartitionRangeItem *)(spec->partStart);
			PartitionRangeItem *prevRI = makeNode(PartitionRangeItem);

			if (prevBSpec->partStart)
			{
				int				 compareRc = 0;
				PartitionRangeItem *pRI1 =
						(PartitionRangeItem *)spec->partStart;
				PartitionRangeItem *pRI2 =
						(PartitionRangeItem *)prevBSpec->partStart;


				compareRc = partition_range_compare(vstate->pstate,
													vstate->cxt, vstate->stmt,
													vstate->pBy,
													vstate->at_depth,
													vstate->partNumber,
													"<", pRI2,pRI1);

				if (1 != compareRc)
				{
					/* XXX: better message */
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("START of partition%s less "
									"than START of previous%s",
							vstate->namBuf,
							vstate->at_depth),
					 parser_errposition(vstate->pstate,
										spec->location)));

				}

			}

			prevRI->location  = pRI->location;

			prevRI->partRangeVal  = list_copy(pRI->partRangeVal);

			/* invert sense of inclusiveness */
			prevRI->partedge =
					(PART_EDGE_INCLUSIVE == pRI->partedge) ?
					 PART_EDGE_EXCLUSIVE : PART_EDGE_INCLUSIVE;

			prevBSpec->partEnd = (Node *)prevRI;

			/* don't need to check if range overlaps */
			bAppendRange = true;

		}
		else if (0) /* XXX XXX XXX XXX */
		{
			/* else if overlap complain */
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("start of partition%s overlaps previous%s",
								vstate->namBuf,
								vstate->at_depth),
						 parser_errposition(vstate->pstate,
											spec->location)));

		}

	} /* end if current partStart */
	else /* no start, so use END of previous */
	{
		if (!prevBSpec->partEnd)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("cannot derive starting value of "
							"partition%s based upon ending of "
							"previous%s",
							vstate->namBuf,
							vstate->at_depth),
					 parser_errposition(vstate->pstate,
										spec->location)));

		}
		else /* build ri for current */
		{
			PartitionRangeItem *pRI =
					makeNode(PartitionRangeItem);
			PartitionRangeItem *prevRI =
					(PartitionRangeItem *)
					(prevBSpec->partEnd);

			pRI->location  =
					prevRI->location;

			pRI->partRangeVal  =
					list_copy(prevRI->partRangeVal);

			/* invert sense of inclusiveness */
			pRI->partedge =
					(PART_EDGE_INCLUSIVE == prevRI->partedge) ?
					PART_EDGE_EXCLUSIVE : PART_EDGE_INCLUSIVE;

			spec->partStart = (Node *)pRI;

			/* don't need to check if range overlaps */
			bAppendRange = true;
		} /* end build ri for current */
	} /* end use END of previous */

L_setprevElem:

	/* check for overlap,
	 * then add new partitions to sorted list
	 */

	if (spec->partStart &&
		spec->partEnd)
	{
		int				 compareRc = 0;
		PartitionRangeItem *pRI1 =
				(PartitionRangeItem *)spec->partStart;
		PartitionRangeItem *pRI2 =
				(PartitionRangeItem *)spec->partEnd;

		PartitionRangeItemIsValid(vstate->pstate, (PartitionRangeItem *)spec->partEnd);
		compareRc =
				partition_range_compare(vstate->pstate,
										vstate->cxt, vstate->stmt,
										vstate->pBy,
										vstate->at_depth,
										vstate->partNumber,
										">",
										pRI1,
										pRI2);

		if (0 != compareRc)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("START greater than END for partition%s%s",
							vstate->namBuf,
							vstate->at_depth),
					 parser_errposition(vstate->pstate,
										spec->location)));

		}
		compareRc =
				partition_range_compare(vstate->pstate,
										vstate->cxt, vstate->stmt,
										vstate->pBy,
										vstate->at_depth,
										vstate->partNumber,
										"=",
										pRI1,
										pRI2);

		if (0 != compareRc)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("START equal to END for partition%s%s",
							vstate->namBuf,
							vstate->at_depth),
					 parser_errposition(vstate->pstate,
										spec->location)));

		}

	}

	if (NIL == vstate->allRangeVals)
		bAppendRange = true;

	/* check previous first */
	if (!bAppendRange &&
		(spec->partStart &&
		 prevBSpec &&
		 prevBSpec->partEnd))
	{
		/* compare to last */
		int				 rc = 0;
		PartitionRangeItem *pRI1 =
				(PartitionRangeItem *)spec->partStart;
		PartitionRangeItem *pRI2 =
				(PartitionRangeItem *)prevBSpec->partEnd;

		rc = partition_range_compare(vstate->pstate,
											vstate->cxt, vstate->stmt,
											vstate->pBy,
											vstate->at_depth,
											vstate->partNumber,
											"=",
											pRI1,
											pRI2);

		if ((pRI2->partedge == PART_EDGE_INCLUSIVE) &&
			(pRI1->partedge == PART_EDGE_INCLUSIVE) &&
			(1 == rc))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("starting value of partition%s "
							"overlaps previous range%s",
							vstate->namBuf,
							vstate->at_depth),
					 parser_errposition(vstate->pstate,
										spec->location)));

		}

		/*
		 * If values are equal, but not inclusive to both,
		 * then append is possible
		 */
		if (1 != rc)
			rc = partition_range_compare(vstate->pstate,
										 vstate->cxt, vstate->stmt,
										 vstate->pBy,
										 vstate->at_depth,
										 vstate->partNumber,
										 ">",
										 pRI1,
										 pRI2);

		if (1 == rc)
			bAppendRange = 1;

		if (rc != 1)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("starting value of partition%s "
							"overlaps previous range%s",
							vstate->namBuf,
							vstate->at_depth),
					 parser_errposition(vstate->pstate,
										spec->location)));
		}

	} /* end compare to last */

	if (bAppendRange)
	{
		vstate->allRangeVals = lappend(vstate->allRangeVals, spec);
	}
	else
	{ /* find position for current range */
		ListCell		*lc_all  = list_head(vstate->allRangeVals);
		ListCell		*lc_allPrev = NULL;
		int				 compareRc = 0;
		/* set up pieces of error messages */
		char			*currPartRI   = "Ending";
		char			*otherPartPos = "next";

		/* linear search sorted list of range specs:

		   While the current start key is >= loop val
		   start key advance.

		   If current start key < loop val start key,
		   then see if curr start key < *previous* loop
		   val end key, ie falls in previous range.

		   If so, error out, else splice current range in
		   after previous.

		*/

		for ( ; lc_all ; lc_all = lnext(lc_all)) /* for lc_all */
		{
			PartitionBoundSpec *lcBSpec =
					(PartitionBoundSpec *) lfirst(lc_all);
			PartitionRangeItem *pRI1 =
					(PartitionRangeItem *)spec->partStart;
			PartitionRangeItem *pRI2 =
					(PartitionRangeItem *)lcBSpec->partStart;

			Assert(spec->partStart);

			if (lcBSpec->partStart)
			{
				compareRc =
					partition_range_compare(vstate->pstate,
											vstate->cxt, vstate->stmt,
											vstate->pBy,
											vstate->at_depth,
											vstate->partNumber,
											"=",
											pRI1,
											pRI2);

				if (-1 == compareRc)
					break;

				if ((pRI2->partedge == PART_EDGE_INCLUSIVE) &&
					(pRI1->partedge == PART_EDGE_INCLUSIVE) &&
					(1 == compareRc))
				{
					currPartRI   = "Starting";
					otherPartPos = "previous";
					compareRc = -2;
					break;
				}

				compareRc =
					partition_range_compare(vstate->pstate,
											vstate->cxt, vstate->stmt,
											vstate->pBy,
											vstate->at_depth,
											vstate->partNumber,
											"<",
											pRI1,
											pRI2);

			}
			else
				/* if first range spec has no start
				 * (ie start=MINVALUE) then current start
				 * must be after it
				 */
				compareRc = 0; /* current > MINVALUE */

			if (-1 == compareRc)
				break;

			if (1 == compareRc) /* if curr less than loop val */
			{
				/* curr start is less than loop val, so
				 * check that curr end is less than loop
				 * val start (including equality case)
				 */

				pRI1 = (PartitionRangeItem *)spec->partEnd;

				if (!pRI1)
				{
					/* if current start is less than
					 * loop val start but current end is
					 * unterminated (ie ending=MAXVALUE)
					 * then it must overlap
					 */
					currPartRI   = "Ending";
					otherPartPos = "next";
					compareRc = -2;
					break;
				}

				compareRc =
					partition_range_compare(vstate->pstate,
											vstate->cxt, vstate->stmt,
											vstate->pBy,
											vstate->at_depth,
											vstate->partNumber,
											"=",
											pRI2,
											pRI1);

				if (-1 == compareRc)
					break;

				if (
					(pRI2->partedge == PART_EDGE_INCLUSIVE) &&
					(pRI1->partedge == PART_EDGE_INCLUSIVE) &&
					(1 == compareRc))
				{
					currPartRI   = "Ending";
					otherPartPos = "next";
					compareRc = -2;
					break;
				}

				compareRc = partition_range_compare(vstate->pstate,
													vstate->cxt, vstate->stmt,
													vstate->pBy,
													vstate->at_depth,
													vstate->partNumber,
													"<",
													pRI2,
													pRI1);

				if (-1 == compareRc)
					break;

				if (1 == compareRc)
				{
					currPartRI   = "Ending";
					otherPartPos = "next";
					compareRc = -2;
					break;
				}

				/* if current is less than loop val and no
				 * previous, then append vstate->allRangeVals to
				 * the current, instead of vice versa
				 */
				if (!lc_allPrev)
					break;

				lcBSpec =
					(PartitionBoundSpec *) lfirst(lc_allPrev);

				pRI2 =
					(PartitionRangeItem *)lcBSpec->partEnd;

				compareRc =
						partition_range_compare(vstate->pstate,
												vstate->cxt, vstate->stmt,
												vstate->pBy,
												vstate->at_depth,
												vstate->partNumber,
												"=",
												pRI1,
												pRI2);

				if (-1 == compareRc)
					break;

				if (
					(pRI2->partedge == PART_EDGE_INCLUSIVE) &&
					(pRI1->partedge == PART_EDGE_INCLUSIVE) &&
					(1 == compareRc))
				{
					currPartRI   = "Starting";
					otherPartPos = "previous";
					compareRc = -2;
					break;
				}

				compareRc =
						partition_range_compare(vstate->pstate,
												vstate->cxt, vstate->stmt,
												vstate->pBy,
												vstate->at_depth,
												vstate->partNumber,
												"<",
												pRI1,
												pRI2);

				if (-1 == compareRc)
					break;

				if (1 == compareRc)
				{
					currPartRI   = "Starting";
					otherPartPos = "previous";
					compareRc = -2;
					break;
				}
			} /* end if curr less than loop val */

			lc_allPrev = lc_all;
		} /* end for lc_all */

		if (-1 == compareRc)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("invalid range comparison for partition%s%s",
							vstate->namBuf,
							vstate->at_depth),
					 parser_errposition(vstate->pstate,
										spec->location)));

		}

		if (-2 == compareRc)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("%s value of partition%s overlaps %s range%s",
							currPartRI,
							vstate->namBuf,
							otherPartPos,
							vstate->at_depth),
					 parser_errposition(vstate->pstate,
										spec->location)));
		}

		if (lc_allPrev)
		{
			/* append cell returns new cell, not list */
			lappend_cell(vstate->allRangeVals,
						 lc_allPrev,
						 spec);
		}
		else /* no previous, so current is start */
			vstate->allRangeVals = list_concat(list_make1(spec),
									   vstate->allRangeVals);
	} /* end find position for current range */
	vstate->prevElem = vstate->pElem;
}

Node *
coerce_partition_value(Node *node, Oid typid, int32 typmod,
					   PartitionByType partype)
{
	Node	   *out;

	/* Create a coercion expression to the target type */
	out = coerce_to_target_type(NULL, node, exprType(node),
								typid, typmod,
								COERCION_EXPLICIT,
								COERCE_IMPLICIT_CAST,
								-1);
	/* MPP-3626: better error message */
	if (!out)
	{
		char	 exprBuf[10000];
		char	*specTName = "";
		char	*pparam	   = "";
		StringInfoData   sid;

/*		elog(ERROR, "cannot coerce partition parameter to column type"); */

		switch (partype)
		{
			case PARTTYP_HASH:
						specTName = "HASH ";
						break;
			case PARTTYP_LIST:
						specTName = "LIST ";
						break;
			case PARTTYP_RANGE:
						specTName = "RANGE ";
						break;
				default:
						break;
		}

		/* try to build a printable string of the node value */
		pparam = deparse_expression(node,
									deparse_context_for("partition",
														InvalidOid),
														false, false);
		if (pparam)
		{
			initStringInfo(&sid);
			appendStringInfo(&sid, "(");
			appendStringInfoString(&sid, pparam);
			appendStringInfo(&sid, ") ");

			pfree(pparam);

			exprBuf[0] = '\0';
			snprintf(exprBuf, sizeof(exprBuf), "%s", sid.data);
			pfree(sid.data);

			pparam = exprBuf;
		}
		else
			pparam = "";

		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("cannot coerce %spartition parameter %s"
						"to column type (%s)",
						specTName,
						pparam,
						format_type_be(typid))));
	}

	/*
	 * And then evaluate it. evaluate_expr calls possible cast function,
	 * and returns a Const. (the check for that below is just for paranoia)
	 */
	out = (Node *) evaluate_expr((Expr *) out, typid);
	if (!IsA(out, Const))
		elog(ERROR, "partition parameter is not constant");

	return out;
}

static void
validate_list_partition(partValidationState *vstate)
{
	PartitionValuesSpec *spec;
	Node *n = vstate->spec;
	ListCell *lc;
	List *coltypes = NIL;

	/* just recreate attnum references */
	foreach(lc, vstate->pBy->keys)
	{
		ListCell *llc2;
		char *colname = strVal(lfirst(lc));
		bool found = false;
		TypeName *typname = NULL;

		foreach(llc2, vstate->cxt->columns)
		{
			ColumnDef *column = lfirst(llc2);

			if (strcmp(column->colname, colname) == 0)
			{
				found = true;
				if (!OidIsValid(column->typname->typid))
				{
					Type type = typenameType(vstate->pstate, column->typname);
					column->typname->typid = typeTypeId(type);
					ReleaseType(type);
				}
				typname = column->typname;
				break;
			}
		}
		Assert(found);
		coltypes = lappend(coltypes, typname);
	}

	if (!PointerIsValid(n))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("missing boundary specification in "
						"partition%s of type LIST%s",
						vstate->namBuf,
						vstate->at_depth),
				 parser_errposition(vstate->pstate, vstate->pElem->location)));

	}

	if (!IsA(n, PartitionValuesSpec))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("invalid boundary specification for LIST partition"),
				 parser_errposition(vstate->pstate, vstate->pElem->location)));


	}

	spec = (PartitionValuesSpec *)n;
	if (spec->partValues)
	{
		ListCell *lc;
		List *newvals = NIL;

		foreach(lc, spec->partValues)
		{
			ListCell	*lc_val = NULL;
			List		*vals 	= lfirst(lc);
			List		*tvals 	= NIL;	/* transformed clause */
			int			 nvals;
			int			 nparts = list_length(vstate->pBy->keys);
			ListCell	*llc2;

			nvals = list_length(vals);

			/* Number of values should be same as specified partition keys */
			if (nvals != nparts)
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("partition key has %i column%s but %i column%s "
								"specified in VALUES clause",
								list_length(vstate->pBy->keys),
								list_length(vstate->pBy->keys) ? "s" : "",
							   	nvals,
								nvals ? "s" : ""),
						 parser_errposition(vstate->pstate, spec->location)));
			}

			/*
			 * Transform expressions
			 */
			llc2 = list_head(coltypes);
			foreach(lc_val, vals)
			{
				Node *node = transformExpr(vstate->pstate,
										   (Node *)lfirst(lc_val));
				TypeName *type = lfirst(llc2);
				Oid			typid = typenameTypeId(vstate->pstate, type);
				int32		typmod = typenameTypeMod(vstate->pstate,
													 type, typid);

				node = coerce_partition_value(node, typid, typmod,
											  PARTTYP_LIST);

				tvals = lappend(tvals, node);

				if (lnext(llc2))
					llc2 = lnext(llc2);
				else
					llc2 = list_head(coltypes); /* circular */
			}
			Assert(list_length(tvals) == nvals);

			/*
			 * Check for duplicate keys
			 */
			foreach(lc_val, vstate->allListVals) /* dups across all specs */
			{
				List *already = lfirst(lc_val);
				ListCell *lc2;

				foreach(lc2, already)
				{
					List *item = lfirst(lc2);
					
					Assert( IsA(item, List) && list_length(item) == nvals );

					/*
					 * Re MPP-17814
					 * The lists tvals and item each represent a tuple of nvals
					 * attribute values.  If they are equal, the new value (tvals)
					 * is in vstate->allListVals, i.e., tvals has been seen before.
					 */
					if ( equal(tvals, item) )
					{
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
								 errmsg("duplicate VALUES "
										"in partition%s%s",
										vstate->namBuf,
										vstate->at_depth),
							 parser_errposition(vstate->pstate, spec->location)));
					}
				}
			}
			newvals = list_append_unique(newvals, tvals);
		}
		vstate->allListVals = lappend(vstate->allListVals, newvals);
		spec->partValues = newvals;
	} /* end if spec partvalues */
} /* end validate_list_partition */

static List *
transformPartitionStorageEncodingClauses(List *enc)
{
	ListCell *lc;
	List *out = NIL;

	/*
	 * Test that directives at different levels of subpartitioning do not
	 * conflict. A conflict would be the case where a directive appears for the
	 * same column but different parameters have been supplied.
	 */
	foreach(lc, enc)
	{
		ListCell *in;
		ColumnReferenceStorageDirective *a = lfirst(lc);
		bool add = true;

		Insist(IsA(a, ColumnReferenceStorageDirective));

		foreach(in, out)
		{
			ColumnReferenceStorageDirective *b = lfirst(in);

			Insist(IsA(b, ColumnReferenceStorageDirective));

			if (lc == in)
				continue;

			if (equal(a->column, b->column))
			{
				if (!equal(a->encoding, b->encoding))
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("conflicting ENCODING clauses for column "
									"\"%s\"", strVal(a->column))));

				/*
				 * We found an identical directive on the same column. You'd
				 * think we should blow up but this is caused by recursive
				 * expansion of partitions. Anyway, only add it once.
				 */
				add = false;
			}
		}

		if (add)
			out = lappend(out, a);
	}

	/* validate and transform each encoding clauses */
	foreach(lc, out)
	{
		ColumnReferenceStorageDirective *c = lfirst(lc);

		c->encoding = transformStorageEncodingClause(c->encoding);
	}

	return out;

}

static void
split_encoding_clauses(List *encs, List **non_def,
					   ColumnReferenceStorageDirective **def)
{
	ListCell *lc;

	foreach(lc, encs)
	{
		ColumnReferenceStorageDirective *c = lfirst(lc);

		Insist(IsA(c, ColumnReferenceStorageDirective));

		if (c->deflt)
		{
			if (*def)
				elog(ERROR,
					 "DEFAULT COLUMN ENCODING clause specified more than "
					 "once for partition");
			*def = c;
		}
		else
			*non_def = lappend(*non_def, c);
	}
}

static void
merge_partition_encoding(ParseState *pstate, PartitionElem *elem, List *penc)
{
	List *elem_nondefs = NIL;
	List *part_nondefs = NIL;
	ColumnReferenceStorageDirective *elem_def = NULL;
	ColumnReferenceStorageDirective *part_def = NULL;
	ListCell *lc;
	AlterPartitionCmd *pc;

	/* 
	 * First of all, we shouldn't proceed if this partition isn't AOCO
	 */

	/* 
	 * Yes, I am as surprised as you are that this is how we represent the WITH
	 * clause here.
	 */
	pc = (AlterPartitionCmd *)elem->storeAttr;
	if (pc && !is_aocs((List *)pc->arg1))
	{
		if (elem->colencs)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("ENCODING clause only supported with "
							"column oriented partitions"),
				 	 parser_errposition(pstate, elem->location)));
		else
			return; /* nothing more to do */
	}

	/* 
	 * If the specific partition has no specific column encoding, just
	 * set it to the partition level default and we're done.
	 */
	if (!elem->colencs)
	{
		elem->colencs = penc;
		return;
	}
	
	/*
	 * Fixup the actual column encoding clauses for this specific partition
	 * element.
	 *
	 * Rules:
	 *
	 * 1. If an element level clause mentions a specific column, do not override
	 * it.
	 * 2. Clauses at the partition configuration level which mention a column
	 * not already mentioned at the element level, are applied to the element.
	 * 3. If an element level default clause exists, we're done.
	 * 4. If a partition configuration level default clause exists, apply it to
	 * the element level.
	 * 5. We're done.
	 */

	/* Split specific clauses and default clauses from both our lists */
	split_encoding_clauses(elem->colencs, &elem_nondefs, &elem_def);
	split_encoding_clauses(penc, &part_nondefs, &part_def);

	/* Add clauses from part_nondefs if the columns are not already mentioned */
	foreach(lc, part_nondefs)
	{
		ListCell *lc2;
		ColumnReferenceStorageDirective *pd = lfirst(lc);
		bool found = false;

		foreach(lc2, elem_nondefs)
		{
			ColumnReferenceStorageDirective *ed = lfirst(lc2);

			if (equal(pd->column, ed->column))
			{
				found = true;
				break;
			}
		}

		if (!found)
			elem->colencs = lappend(elem->colencs, pd);
	}

	if (elem_def)
		return;

	if (part_def)
		elem->colencs = lappend(elem->colencs, part_def);
}

int
validate_partition_spec(ParseState *pstate, CreateStmtContext *cxt,
						CreateStmt *stmt, PartitionBy *pBy,
						char *at_depth, int	partNumber)
{
	PartitionSpec	*pSpec;
	char			 namBuf[NAMEDATALEN];
	List			*partElts;
	ListCell		*lc = NULL;
	List			*allPartNames = NIL;
	partValidationState *vstate;
	PartitionElem *pDefaultElem = NULL;
	int partno = 0;
	List			*enc_cls = NIL;

	vstate = palloc0(sizeof(partValidationState));

	vstate->pstate = pstate;
	vstate->cxt = cxt;
	vstate->stmt = stmt;
	vstate->pBy = pBy;
	vstate->at_depth = at_depth;
	vstate->partNumber = partNumber;

	Assert(pBy);
	pSpec = (PartitionSpec *)pBy->partSpec;

	/*
	 * Find number of partitions in the specification, and match it up
	 * with the PARTITIONS clause if partitioned by HASH, and
	 * determine the subpartition specifications.  Perform basic error
	 * checking on boundary specifications.
	 */

	/* NOTE: a top-level PartitionSpec never has a subSpec, but a
	 * PartitionSpec derived from an inline SubPartition
	 * specification might have one
	 */


	/* track previous boundary spec to check for this subtle problem:

	(
	partition aa start (2007,1) end (2008,2),
	partition bb start (2008,2) end (2009,3)
	);
	This declaration would create four partitions:
	1. named aa, starting at 2007, 1
	2. unnamed, ending at 2008, 2
	3. named bb, starting at 2008, 2
	4. unnamed, ending at 2009, 3

	Warn user if they do this, since they probably wanted
	1. named aa, starting at 2007, 1 and ending at 2008, 2
	2. named bb, starting at 2008, 2 and ending at 2009, 3

	The extra comma between the start and end is the problem.

	*/

	if (pBy->partType == PARTTYP_RANGE)
		preprocess_range_spec(vstate);

	partElts = pSpec->partElem;

	/*
	 * If this is a RANGE partition, we might have gleaned some encoding clauses
	 * already, because preprocess_range_spec() looks at partition elements.
	 */
	enc_cls = pSpec->enc_clauses;

	foreach(lc, partElts)
	{
		PartitionElem *pElem = (PartitionElem *)lfirst(lc);
		PartitionBoundSpec *pBSpec = NULL;

		if (!IsA(pElem, PartitionElem))
		{
			Insist(IsA(pElem, ColumnReferenceStorageDirective));
			enc_cls = lappend(enc_cls, lfirst(lc));
			continue;
		}
		vstate->pElem = pElem;

		if (pSpec->istemplate && pElem->colencs)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("partition specific ENCODING clause not supported in SUBPARTITION TEMPLATE"),
					 parser_errposition(pstate, pElem->location)));


		/*
		 * We've done all possible expansions so now number the
		 * partition elements so that we can set the position when
		 * adding the configuration to the catalog.
		 */
		pElem->partno = ++partno;

		if (pElem)
		{
			pBSpec = (PartitionBoundSpec *)pElem->boundSpec;
			vstate->spec = (Node *)pBSpec;

			/* handle all default partition cases:

			   HASH partitioned tables cannot have DEFAULT partitions.
			   Can only have a single DEFAULT partition.
			   Default partitions cannot have a boundary specification.

			*/

			if (pElem->isDefault)
			{
				Assert(pElem->partName);  /* default partn must have a name */
				snprintf(namBuf, sizeof(namBuf), " \"%s\"",
						 strVal(pElem->partName));

				if (pDefaultElem)
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("multiple default partitions are not "
									"allowed"),
							 parser_errposition(pstate, pElem->location)));

				}

				pDefaultElem = pElem; /* save the default */

				if (PARTTYP_HASH == pBy->partType)
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("invalid use of DEFAULT partition "
									"for partition%s of type HASH%s",
									namBuf,
									at_depth),
							 parser_errposition(pstate, pElem->location)));

				}

				if (pBSpec)
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("invalid use of boundary specification "
									"for DEFAULT partition%s%s",
									namBuf,
									at_depth),
							 parser_errposition(pstate, pElem->location)));

				}

			} /* end if is default */

			if (pElem->partName)
			{
				bool doit = true;

				snprintf(namBuf, sizeof(namBuf), " \"%s\"",
						 strVal(pElem->partName));
				/*
				 * We might have expanded an EVERY clause here. If so, just
				 * add the first partition name.
				 */
				if (doit)
				{
					if (allPartNames &&
						list_member(allPartNames, pElem->partName))
					{
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
								 errmsg("duplicate partition name "
										"for partition%s%s",
										namBuf,
										at_depth),
								 parser_errposition(pstate, pElem->location)));
					}
					allPartNames = lappend(allPartNames, pElem->partName);
				}
			}
			else
			{
				if (pElem->AddPartDesc)
					snprintf(namBuf, sizeof(namBuf), "%s", pElem->AddPartDesc);
				else
					snprintf(namBuf, sizeof(namBuf), " number %d", partno);
			}
		}
		else
		{
			if (pElem->AddPartDesc)
				snprintf(namBuf, sizeof(namBuf), "%s", pElem->AddPartDesc);
			else
				snprintf(namBuf, sizeof(namBuf), " number %d", partno);
		}

		/* don't have to validate default partition boundary specs */
		if (pElem->isDefault)
			continue;

		vstate->namBuf = namBuf;

		switch (pBy->partType)
		{
			case PARTTYP_HASH:
				if (vstate->spec)
				{
					char *specTName = "RANGE";

					if (IsA(vstate->spec, PartitionValuesSpec))
						specTName = "LIST";

					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("invalid use of %s boundary specification "
									"in partition%s of type HASH%s",
									specTName,
									vstate->namBuf,
									vstate->at_depth),
						 /* MPP-4249: use value spec location if have one */
						 ((IsA(vstate->spec, PartitionValuesSpec)) ?
			  parser_errposition(pstate,
							 ((PartitionValuesSpec*)vstate->spec)->location) :
						  /* else use boundspec */
			  parser_errposition(pstate,
							 ((PartitionBoundSpec*)vstate->spec)->location)
								 )));
				}
				break;
			case PARTTYP_RANGE:
				validate_range_partition(vstate);
				break;

			case PARTTYP_LIST:
				validate_list_partition(vstate);
				break;

			case PARTTYP_REFERENCE: /* for future use... */
			default:
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("unknown partition type %d %s",
								pBy->partType, at_depth),
						 parser_errposition(pstate, pBy->location)));
				break;

		}
	} /* end foreach(lc, partElts) */

	pSpec->enc_clauses = transformPartitionStorageEncodingClauses(enc_cls);

	foreach(lc, partElts)
	{
		PartitionElem *elem = (PartitionElem *)lfirst(lc);

		if (!IsA(elem, PartitionElem))
			continue;

		merge_partition_encoding(pstate, elem, pSpec->enc_clauses);
	}

	/* validate_range_partition might have changed some boundaries */
	if (pBy->partType == PARTTYP_RANGE)
		pSpec->partElem = sort_range_elems(pBy->keyopclass, partElts);

	if (partNumber > -1 && partno != partNumber)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("PARTITIONS \"%d\" must match \"%d\" elements "
						"in specification%s",
						 partNumber, partno, vstate->at_depth),
				 parser_errposition(pstate, pBy->location)));

	if (vstate->allRangeVals)
		list_free(vstate->allRangeVals);

	if (allPartNames)
		list_free(allPartNames);

	return partno;
} /* end validate_partition_spec */

static Const *
flatten_partition_val(Node *node, Oid target_type)
{
	if (IsA(node, Const))
		return (Const *)node;
	else
	{
		Datum res;
		Oid curtyp;
		Const *c;
		Type typ = typeidType(target_type);
		int32 typmod = ((Form_pg_type)GETSTRUCT(typ))->typtypmod;
		int16 typlen = ((Form_pg_type)GETSTRUCT(typ))->typlen;
		bool typbyval = ((Form_pg_type)GETSTRUCT(typ))->typbyval;
		bool isnull;

		ReleaseType(typ);

		curtyp = exprType(node);
		Assert(OidIsValid(curtyp));

		if (curtyp != target_type && OidIsValid(target_type))
		{
			node = coerce_type(NULL, node, curtyp, target_type, typmod,
							   COERCION_EXPLICIT,
							   COERCE_IMPLICIT_CAST,
							   -1);

			if (!PointerIsValid(node))
				elog(ERROR, "could not coerce partitioning parameter");
		}

		res = partition_arg_get_val(node, &isnull);
		c = makeConst(target_type, typmod, typlen, res, isnull, typbyval);

		return c;
	}

}

/*
 * Get the actual value from the expression. There are only a limited range
 * of cases we must cover because the parser guarantees constant input.
 */
static Datum
partition_arg_get_val(Node *node, bool *isnull)
{
	Const	   *c;

	c = (Const *) evaluate_expr((Expr *) node, exprType(node));
	if (!IsA(c, Const))
		elog(ERROR, "partition parameter is not constant");

	*isnull = c->constisnull;
	return c->constvalue;
}

/*
 * Evaluate a basic operator expression from a partitioning specification.
 * The expression will only be an op expr but the sides might contain
 * a coercion function. The underlying value will be a simple constant,
 * however.
 *
 * If restypid is non-NULL and *restypid is set to InvalidOid, we tell the
 * caller what the return type of the operator is. If it is anything but
 * InvalidOid, coerce the operation's result to that type.
 */
static Datum
eval_basic_opexpr(ParseState *pstate, List *oprname, Node *leftarg,
				  Node *rightarg, bool *typbyval, int16 *typlen,
				  Oid *restypid, int location)
{
	Datum res = 0;
	Datum lhs = 0;
	Datum rhs = 0;
	OpExpr *opexpr;
	bool byval;
	int16 len;
	bool need_typ_info = (PointerIsValid(restypid) && *restypid == InvalidOid);
	bool isnull;
	Oid oprcode;
	int fetchCount;

	opexpr = (OpExpr *)make_op(pstate, oprname, leftarg, rightarg, location);

	oprcode = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT oprcode FROM pg_operator "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(opexpr->opno)));

	if (!fetchCount)	  /* should not fail */
		elog(ERROR, "cache lookup failed for operator %u", opexpr->opno);
	opexpr->opfuncid = oprcode;

	lhs = partition_arg_get_val((Node *)linitial(opexpr->args), &isnull);
	if (!isnull)
	{
		rhs = partition_arg_get_val((Node *)lsecond(opexpr->args), &isnull);
		if (!isnull)
			res = OidFunctionCall2(opexpr->opfuncid, lhs, rhs);
	}

	/* If the caller supplied a target result type, coerce if necesssary */
	if (PointerIsValid(restypid))
	{
		if (OidIsValid(*restypid))
		{
			if (*restypid != opexpr->opresulttype)
			{
				Expr *e;
				Type typ = typeidType(opexpr->opresulttype);
				int32 typmod;
				Const *c;

				c = makeConst(opexpr->opresulttype, -1, typeLen(typ), res,
							  isnull, typeByVal(typ));
				ReleaseType(typ);

				typ = typeidType(*restypid);
				typmod = ((Form_pg_type)GETSTRUCT(typ))->typtypmod;
				ReleaseType(typ);

				e = (Expr *)coerce_type(NULL, (Node *)c, opexpr->opresulttype,
										*restypid, typmod,
										COERCION_EXPLICIT,
										COERCE_IMPLICIT_CAST,
										-1);

				res = partition_arg_get_val((Node *)e, &isnull);
			}
		}
		else
		{
			*restypid = opexpr->opresulttype;
		}
	}
	else
	{
		return res;
	}

	if (need_typ_info || !PointerIsValid(typbyval))
	{
		Type typ = typeidType(*restypid);

		byval = typeByVal(typ);
		len = typeLen(typ);

		if (PointerIsValid(typbyval))
		{
			Assert(PointerIsValid(typlen));
			*typbyval = byval;
			*typlen = len;
		}

		ReleaseType(typ);
	}
	else
	{
		byval = *typbyval;
		len = *typlen;
	}

	res = datumCopy(res, byval, len);
	return res;
}

/*
 * XXX: this is awful, rewrite
 */
static int
partition_range_every(ParseState *pstate, PartitionBy *pBy, List *coltypes,
					  char *at_depth, partValidationState *vstate)
{
	PartitionSpec	*pSpec;
	char			 namBuf[NAMEDATALEN];
	int				 numElts = 0;
	List			*partElts;
	ListCell		*lc = NULL;
	ListCell		*lc_prev = NULL;
	List			*stenc = NIL;

	Assert(pBy);
	Assert(pBy->partType == PARTTYP_RANGE);

	pSpec    = (PartitionSpec *)pBy->partSpec;

	if (pSpec) /* no bound spec for default partition */
	{
		partElts = pSpec->partElem;
		stenc = pSpec->enc_clauses;

		lc = list_head(partElts);
	}

	for ( ; lc; ) /* foreach */
	{
		PartitionElem			*pElem = (PartitionElem *)lfirst(lc);

		PartitionBoundSpec		*pBSpec				 = NULL;
		int						 numCols			 = 0;
		int						 colCnt				 = 0;
		Const					*everyCnt;
		PartitionRangeItem		*pRI_Start			 = NULL;
		PartitionRangeItem		*pRI_End			 = NULL;
		PartitionRangeItem		*pRI_Every			 = NULL;
		ListCell				*lc_Start			 = NULL;
		ListCell				*lc_End				 = NULL;
		ListCell				*lc_Every			 = NULL;
		Node					*pStoreAttr			 = NULL;
		List					*allNewCols;
		List					*allNewPartns		 = NIL;
		List					*lastval			 = NIL;
		bool					 do_every_param_test = true;

		pBSpec = NULL;

		if (pElem && IsA(pElem, ColumnReferenceStorageDirective))
		{
			stenc = lappend(stenc, pElem);
			goto l_next_iteration;
		}

		numElts++;

		if (!pElem)
			goto l_EveryLoopEnd;
		else
		{
			pBSpec = (PartitionBoundSpec *)pElem->boundSpec;

			if (!pBSpec)
				goto l_EveryLoopEnd;

			pStoreAttr = pElem->storeAttr;

			if (pElem->partName)
			{
				snprintf(namBuf, sizeof(namBuf), " \"%s\"",
						 strVal(pElem->partName));
			}
			else
			{
				snprintf(namBuf, sizeof(namBuf), " number %d",
						 numElts);
			}

			if (pElem->isDefault)
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("invalid use of boundary specification "
								"for DEFAULT partition%s%s",
								namBuf,
								at_depth),
						 parser_errposition(pstate, pElem->location)));
			} /* end if is default */

			/* MPP-3541: invalid use of LIST spec */
			if (!IsA(pBSpec, PartitionBoundSpec))
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("invalid use of LIST boundary specification "
								"in partition%s of type RANGE%s",
								namBuf,
								at_depth),
						 /* MPP-4249: use value spec location if have one */
						 ((IsA(pBSpec, PartitionValuesSpec)) ?
			  parser_errposition(pstate,
								 ((PartitionValuesSpec*)pBSpec)->location) :
						  /* else use invalid parsestate/postition */
						  parser_errposition(NULL, 0)
								 )
								));
			}

			Assert(IsA(pBSpec, PartitionBoundSpec));

		}

		/* have a valid range bound spec */

		if (!pBSpec->partEvery)
			goto l_EveryLoopEnd;

		/* valid EVERY needs a START and END */
		if (!(pBSpec->partStart &&
			  pBSpec->partEnd))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("EVERY clause in partition%s "
							"requires START and END%s",
							namBuf,
							at_depth),
					 parser_errposition(pstate, pBSpec->location)));
		}

		/* MPP-6297: check for WITH (tablename=name) clause
		 * [magic to make dump/restore work by ignoring EVERY]
		 */
		if (pStoreAttr && ((AlterPartitionCmd *)pStoreAttr)->arg1)
		{
			ListCell *prev_lc = NULL;
			ListCell *def_lc  = NULL;
			List *pWithList = (List *)
					(((AlterPartitionCmd *)pStoreAttr)->arg1);
			bool bTablename = false;

			foreach(def_lc, pWithList)
			{
				DefElem *pDef = (DefElem *)lfirst(def_lc);

				/* get the tablename from the WITH, then remove this
				 * element from the list */
				if (0 == strcmp(pDef->defname, "tablename"))
				{
					/* if the string isn't quoted you get a typename ? */
					if (!IsA(pDef->arg, String))
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("invalid tablename specification")));

					bTablename = true;
					bool need_free_value = false;
					char * widthname_str = defGetString(pDef, &need_free_value);
					pBSpec->pWithTnameStr = pstrdup(widthname_str);
					if (need_free_value)
					{
						pfree(widthname_str);
						widthname_str = NULL;
					}

					AssertImply(need_free_value, NULL == widthname_str);

					pWithList = list_delete_cell(pWithList, def_lc, prev_lc);
					((AlterPartitionCmd *)pStoreAttr)->arg1 =
							(Node *)pWithList;
					break;
				}
				prev_lc = def_lc;
			} /* end foreach */

			if (bTablename)
				goto l_EveryLoopEnd;
		}

		everyCnt = make_const(pstate, makeInteger(1), -1);

		pRI_Start = (PartitionRangeItem *)pBSpec->partStart;
		pRI_End   = (PartitionRangeItem *)pBSpec->partEnd;
		pRI_Every = (PartitionRangeItem *)pBSpec->partEvery;

		numCols = list_length(pRI_Every->partRangeVal);

		if ((numCols != list_length(pRI_Start->partRangeVal))
			|| (numCols != list_length(pRI_End->partRangeVal)))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("mismatch between EVERY, START and END "
							"in partition%s%s",
							namBuf,
							at_depth),
					 parser_errposition(pstate, pBSpec->location)));

		}

		/* XXX XXX: need to save prev value of
		   every_num * (every_cnt * n3t) for first column only
		*/

		for ( ; ; ) /* loop until exceed end */
		{
			ListCell *coltype = list_head(coltypes);
			ListCell *lclastval = list_head(lastval);
			List *curval = NIL;

			int sqlRc = 0;
			lc_Start = list_head(pRI_Start->partRangeVal);
			lc_End   = list_head(pRI_End->partRangeVal);
			lc_Every = list_head(pRI_Every->partRangeVal);
			colCnt = numCols;
			allNewCols = NIL;

			for ( ; lc_Start && lc_End && lc_Every ; ) /* for all cols */
			{
				Node *n1 = lfirst(lc_Start);
				Node *n2 = lfirst(lc_End);
				Node *n3 = lfirst(lc_Every);
				TypeName *type = lfirst(coltype);
				char *compare_op = (1 == colCnt) ? "<" : "<=";
				Node *n1t, *n2t, *n3t;
				Datum res;
				Const *c;
				Const *newend;
				List *oprmul, *oprplus, *oprcompare, *ltop;
				Oid restypid;
				Type typ;
				char *outputstr;
				Oid			coltypid = typenameTypeId(vstate->pstate, type);
				int32		coltypmod = typenameTypeMod(vstate->pstate,
														type, coltypid);

				oprmul = lappend(NIL, makeString("*"));
				oprplus = lappend(NIL, makeString("+"));
				oprcompare = lappend(NIL, makeString(compare_op));
				ltop = lappend(NIL, makeString("<"));

				n1t = transformExpr(pstate, n1);
				n1t = coerce_type(NULL, n1t, exprType(n1t), coltypid,
								  coltypmod,
								  COERCION_EXPLICIT, COERCE_IMPLICIT_CAST,
								  -1);
				n1t = (Node *)flatten_partition_val(n1t, coltypid);

				n2t = transformExpr(pstate, n2);
				n2t = coerce_type(NULL, n2t, exprType(n2t), coltypid,
								  coltypmod,
								  COERCION_EXPLICIT, COERCE_IMPLICIT_CAST,
								  -1);
				n2t = (Node *)flatten_partition_val(n2t, coltypid);

				n3t = transformExpr(pstate, n3);
				n3t = (Node *)flatten_partition_val(n3t, exprType(n3t));

				Assert(IsA(n3t, Const));
				Assert(IsA(n2t, Const));
				Assert(IsA(n1t, Const));

				/* formula is n1t + (every_cnt * every_num * n3t) [<|<=] n2t */

				/* every_cnt * n3t */
				restypid = InvalidOid;
				res = eval_basic_opexpr(pstate, oprmul, (Node *)everyCnt, n3t,
										NULL, NULL, &restypid,
										((A_Const *)n3)->location);
				typ = typeidType(restypid);
				c = makeConst(restypid, -1, typeLen(typ), res, false,
							  typeByVal(typ));
				ReleaseType(typ);

				/*
				 * n1t + (...)
				 * NOTE: order is important because several useful operators,
				 * like date + interval, only have a built in function when
				 * the order is this way (the reverse is implemented using
				 * an SQL function which we cannot evaluate at the moment).
				 */
				restypid = exprType(n1t);
				res = eval_basic_opexpr(pstate, oprplus, n1t, (Node *)c,
										NULL, NULL,
										&restypid,
										((A_Const *)n1)->location);
				typ = typeidType(restypid);
				newend = makeConst(restypid, -1, typeLen(typ), res, false,
								   typeByVal(typ));
				ReleaseType(typ);

				/*
				 * Now we must detect a few conditions.
				 *
				 * We must reject every() parameters which do not significantly
				 * increment the starting value. For example:
				 *
				 * 1: start ('2008-01-01') end ('2010-01-01') every('0 days')
				 *
				 * We'll detect this case on the second iteration by observing
				 * that the current partition end is no greater than the
				 * previous.
				 *
				 * We must also consider:
				 *
				 * 2: start (1) end (10) every(0.5)
				 * 3: start (1) end (10) every(1.5)
				 *
				 * The problem here is that 1.5 and 2.5 will be rounded to 2 and
				 * 3 respectively. This means they look sane to the code but
				 * they aren't. So, we must test that:
				 *
				 *   cast(1 + 0.5 as int) != (1 + 0.5)::numeric
				 *
				 * If we make it past this point, we still might see a current
				 * value less than the previous value. That would be caused by
				 * an overflow of the type which is undetected by the type
				 * itself. Most types detect overflow but time and timetz do
				 * not. So, if we're beyond the second iteration and detect and
				 * overflow, error out.
				 */
				/* do it on the first iteration */
					if (!lclastval)
					{
						List *opreq = lappend(NIL, makeString("="));
						Datum uncast;
						Datum iseq;
						Const *tmpconst;
						Oid test_typid = InvalidOid;

						uncast = eval_basic_opexpr(pstate, oprplus, n1t,
												   (Node *)c, NULL, NULL,
												   &test_typid,
												   ((A_Const *)n1)->location);

						typ = typeidType(test_typid);
						tmpconst = makeConst(test_typid, -1, typeLen(typ), uncast,
											 false, typeByVal(typ));
						ReleaseType(typ);

						iseq = eval_basic_opexpr(NULL, opreq, (Node *)tmpconst,
												 (Node *)newend, NULL, NULL,
												 NULL, -1);

						if (!DatumGetBool(iseq))
							ereport(ERROR,
									(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
									 errmsg("EVERY parameter produces ambiguous partition rule"),
									 parser_errposition(pstate,
													((A_Const *)n3)->location)));

					}

				if (lclastval)
				{
					Datum res2;
					Oid tmptyp = InvalidOid;

					/*
					 * now test for case 2 and 3 above: ensure that
					 * the cast value is equal to the uncast value.
					 */

					res2 = eval_basic_opexpr(pstate, ltop,
											 (Node *)lfirst(lclastval),
											 (Node *)newend,
											 NULL, NULL,
											 &tmptyp,
											 ((A_Const *)n3)->location);

					if (!DatumGetBool(res2))
					{
						/*
						 * Second iteration: parameter hasn't increased the
						 * current end from the old end.
						 */
						if (do_every_param_test)
						{
							ereport(ERROR,
									(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
									 errmsg("EVERY parameter too small"),
									 parser_errposition(pstate,
													((A_Const *)n3)->location)));
						}
						else
						{
							/*
							 * We got a smaller value but later than we thought
							 * so it must be an overflow.
							 */
							ereport(ERROR,
									(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
									 errmsg("END parameter not reached before type overflows"),
									 parser_errposition(pstate,
													((A_Const *)n2)->location)));
						}
					}

		}

				curval = lappend(curval, newend);

				/* get the string for the calculated type */
				{
					Oid ooutput;
					bool isvarlena;
					FmgrInfo finfo;

					getTypeOutputInfo(restypid, &ooutput, &isvarlena);
					fmgr_info(ooutput, &finfo);

					if (isvarlena)
						res = PointerGetDatum(PG_DETOAST_DATUM(res));

					outputstr = OutputFunctionCall(&finfo, res);
				}



				/* the comparison */
				restypid = InvalidOid;
				res = eval_basic_opexpr(pstate, oprcompare, (Node *)newend, n2t,
										NULL, NULL,
										&restypid,
									   	((A_Const *)n2)->location);

				/* XXX XXX: also check stop flag.
				   and free up prev if stop is true or current > end.
				 */

				if (!DatumGetBool(res))
				{
					if (outputstr)
						pfree(outputstr);

					sqlRc = 0;
					break;
				}
				else
				{
					allNewCols = lappend(allNewCols, pstrdup(outputstr));

					if (outputstr)
						pfree(outputstr);

					sqlRc = 1;
				}
				lc_Start = lnext(lc_Start);
				lc_End   = lnext(lc_End);
				lc_Every = lnext(lc_Every);
				coltype = lnext(coltype);
				if (lclastval)
					lclastval = lnext(lclastval);

				colCnt--;
			} /* end for all cols */

			if (lc_Start || lc_End || lc_Every)
			{ /* allNewCols is incomplete - don't use */
				if (allNewCols)
					list_free_deep(allNewCols);
			}
			else
			{
				allNewPartns = lappend(allNewPartns, allNewCols);
			}

			if (!sqlRc)
				break;

			everyCnt->constvalue++;
			/* if we have lastval set, the every test will have been done */

			if (lastval)
				do_every_param_test = false;
			lastval = curval;
		} /* end loop until exceed end */

	l_EveryLoopEnd:

		if (allNewPartns)
			Assert(pBSpec); /* check for default partitions... */

		if (pBSpec)
			pBSpec->everyGenList = allNewPartns;

l_next_iteration:
		lc_prev = lc;
		lc = lnext(lc);
	} /* end foreach */

	pSpec->enc_clauses = stenc;

	return 1;
} /* end partition_range_every */

/*
 * Add or override any column reference storage directives inherited from the
 * parent table.
 */
static void
merge_part_column_encodings(CreateStmt *cs, List *stenc)
{
	ListCell *lc;
	List *parentencs = NIL;
	List *others = NIL;
	List *finalencs = NIL;

	/* Don't waste time unnecessarily */
	if (!stenc)
		return;

	/* 
	 * First, split the table elements into column reference storage directives
	 * and everything else.
	 */
	foreach(lc, cs->tableElts)
	{
		Node *n = lfirst(lc);

		if (IsA(n, ColumnReferenceStorageDirective))
			parentencs = lappend(parentencs, n);
		else
			others = lappend(others, n);
	}

	/*
	 * Now build a final set of storage directives for this partition. Start
	 * with stenc and then add everything from parentencs which doesn't appear
	 * in that. This means we prefer partition level directives over those
	 * directives specified for the parent.
	 *
	 * We must copy stenc, since list_concat may modify it destructively.
	 */
	finalencs = list_copy(stenc);

	foreach(lc, parentencs)
	{
		bool found = false;
		ListCell *lc2;
		ColumnReferenceStorageDirective *p = lfirst(lc);

		if (p->deflt)
			continue;

		foreach(lc2, finalencs)
		{
			ColumnReferenceStorageDirective *f = lfirst(lc2);
 
			if (f->deflt)
				continue;

			if (equal(f->column, p->column))
			{
				found = true;
				break;
			}
		}
		if (!found)
			finalencs = lappend(finalencs, p);
	}

	/*
	 * Finally, make sure we don't propagate any conflicting clauses in the
	 * ColumnDef.
	 */
	foreach(lc, others)
	{
		Node *n = lfirst(lc);

		if (IsA(n, ColumnDef))
		{
			ColumnDef *c = (ColumnDef *)n;
			ListCell *lc2;

			foreach(lc2, finalencs)
			{
				ColumnReferenceStorageDirective *r = lfirst(lc2);

				if (r->deflt)
					continue;

				if (strcmp(strVal(r->column), c->colname) == 0)
				{
					c->encoding = NIL;
					break;
				}
			}

		}
	}

	cs->tableElts = list_concat(others, finalencs);
}

static void
make_child_node(CreateStmt *stmt, CreateStmtContext *cxt, char *relname,
			   	PartitionBy *curPby, Node *newSub,
				Node *pRuleCatalog, Node *pPostCreate, Node *pConstraint,
				Node *pStoreAttr, char *prtstr, bool bQuiet,
				List *stenc)
{
	RangeVar *parent_tab_name = makeNode(RangeVar);
	RangeVar *child_tab_name = makeNode(RangeVar);
	CreateStmt *child_tab_stmt = makeNode(CreateStmt);

	parent_tab_name->catalogname = cxt->relation->catalogname;
	parent_tab_name->schemaname  = cxt->relation->schemaname;
	parent_tab_name->relname     = cxt->relation->relname;
    parent_tab_name->location    = -1;

	child_tab_name->catalogname = cxt->relation->catalogname;
	child_tab_name->schemaname  = cxt->relation->schemaname;
	child_tab_name->relname = relname;
    child_tab_name->location = -1;

	child_tab_stmt->relation = child_tab_name;
	child_tab_stmt->is_part_child = true;
	child_tab_stmt->is_add_part = stmt->is_add_part;

	if (!bQuiet)
		ereport(NOTICE,
				(errmsg("%s will create partition \"%s\" for "
						"table \"%s\"",
						cxt->stmtType, child_tab_name->relname,
						cxt->relation->relname)));

	/* set the "Post Create" rule if it exists */
	child_tab_stmt->postCreate = pPostCreate;

	/*
	 * Deep copy the parent's table elements.
	 *
	 * XXX The copy may be unnecessary, but it is safe.
	 *
	 * Previously, some following code updated constraint names
	 * in the tableElts to assure uniqueness and handle issues
	 * with FKs (?).  This required a copy.
	 *
	 * However, forcing a name change at this level overrides any
	 * user-specified constraint names, so we don't do one here
	 * any more.
	 */
	child_tab_stmt->tableElts = copyObject(stmt->tableElts);

	merge_part_column_encodings(child_tab_stmt, stenc);

	/* Hash partitioning special case. */
	if (pConstraint && ((enable_partition_rules &&
						 curPby->partType == PARTTYP_HASH) ||
						 curPby->partType != PARTTYP_HASH))
		child_tab_stmt->tableElts =
				lappend(child_tab_stmt->tableElts,
						pConstraint);

	/*
	 * XXX XXX: inheriting the parent causes a headache in
	 * transformDistributedBy, since it assumes the parent exists
	 * already.  Just add an "ALTER TABLE...INHERIT parent" after
	 * the create child table
	 */
	/*child_tab_stmt->inhRelations = list_make1(parent_tab_name); */
	child_tab_stmt->inhRelations = list_copy(stmt->inhRelations);

	child_tab_stmt->constraints = copyObject(stmt->constraints);
	child_tab_stmt->options = stmt->options;

	/* allow WITH clause for appendonly tables */
	if ( pStoreAttr )
	{
		AlterPartitionCmd *psa_apc = (AlterPartitionCmd *)pStoreAttr;

		/* Options */
		if ( psa_apc->arg1 )
			child_tab_stmt->options = (List *)psa_apc->arg1;
		/* Tablespace from parent (input CreateStmt)... */
		if ( psa_apc->arg2 && *strVal(psa_apc->arg2) )
			child_tab_stmt->tablespacename = strVal(psa_apc->arg2);
	}
	/* ...or tablespace from root. */
	if ( !child_tab_stmt->tablespacename && stmt->tablespacename )
		child_tab_stmt->tablespacename = stmt->tablespacename;

	child_tab_stmt->oncommit = stmt->oncommit;
	child_tab_stmt->distributedBy = stmt->distributedBy;

	/* use the newSub as the partitionBy if the current
	 * partition elem had an inline subpartition declaration
	 */
	child_tab_stmt->partitionBy = (Node *)newSub;

	child_tab_stmt->relKind = RELKIND_RELATION;

	/*
	 * Adjust tablespace name for the CREATE TABLE via ADD PARTITION. (MPP-8047)
	 *
	 * The way we traverse the hierarchy, parents are visited before children, so
	 * we can usually pick up tablespace from the parent relation.  If the child
	 * is a top-level branch, though, we take the tablespace from the root.
	 * Ultimately, we take the tablespace as specified in the command, or, if none
	 * was specified, the one from the root paritioned table.
	 */
	if ( ! child_tab_stmt->tablespacename )
	{
		Oid poid = RangeVarGetRelid(cxt->relation, true); /* parent branch */

		if ( ! poid )
		{
			poid = RangeVarGetRelid(stmt->relation, true); /* whole partitioned table */
		}
		if ( poid )
		{
			Relation prel = RelationIdGetRelation(poid);
			child_tab_stmt->tablespacename = get_tablespace_name(prel->rd_rel->reltablespace);
			RelationClose(prel);
		}
	}

	cxt->alist = lappend(cxt->alist, child_tab_stmt);

	/*
	 * ALTER TABLE for inheritance after CREATE TABLE ...
	 * XXX: Think of a better way.
	 */
	if (1)
	{
		AlterTableCmd  *atc = makeNode(AlterTableCmd);
		AlterTableStmt *ats = makeNode(AlterTableStmt);
		InheritPartitionCmd *ipc = makeNode(InheritPartitionCmd);

		/* alter table child inherits parent */
		atc->subtype = AT_AddInherit;
		ipc->parent = parent_tab_name;
		atc->def = (Node *)ipc;
		ats->relation = child_tab_name;
		ats->cmds 	  = list_make1((Node *)atc);
		ats->relkind  = OBJECT_TABLE;

		/* this is the deepest we're going, add the partition rules */
		if (1)
		{
			AlterTableCmd  *atc2 = makeNode(AlterTableCmd);

			/* alter table add child to partition set */
			atc2->subtype = AT_PartAddInternal;
			atc2->def = (Node *)curPby;
			ats->cmds = lappend(ats->cmds, atc2);
		}
		cxt->alist = lappend(cxt->alist, ats);
	}
}

static void
expand_hash_partition_spec(PartitionBy *pBy)
{
	PartitionSpec *spec;
	long i;
	long max;
	List *elem = NIL;
	A_Const *con;

	Assert(pBy->partType == PARTTYP_HASH);
	Assert(pBy->partSpec == NULL);

	spec = makeNode(PartitionSpec);

	con = (A_Const *)pBy->partNum;
	Assert(IsA(&con->val, Integer));
	max = intVal(&con->val);

	for (i = 0; i < max; i++)
	{
		PartitionElem *el = makeNode(PartitionElem);

		elem = lappend(elem, el);
	}
	spec->partElem = elem;
	pBy->partSpec = (Node *)spec;
}

static bool
partition_col_walker(Node *node, void *context)
{
	if (node == NULL)
		return false;

	if (IsA(node, PartitionSpec))
	{
		PartitionSpec *s = (PartitionSpec *)node;

		if (partition_col_walker(s->subSpec, context))
			return true;

		return partition_col_walker((Node *)s->partElem, context);
	}
	else if(IsA(node, PartitionElem))
	{
		PartitionElem *el = (PartitionElem *)node;

		return partition_col_walker(el->subSpec, context);

	}
	else if (IsA(node, PartitionBy))
	{
		PartitionBy *p = (PartitionBy *)node;
		ListCell *lc;
		part_col_cxt *cxt = (part_col_cxt *)context;

		foreach(lc, p->keys)
		{
			char *colname = strVal(lfirst(lc));
			ListCell *llc;

			foreach(llc, cxt->cols)
			{
				char *col = lfirst(llc);

				if (strcmp(col, colname) == 0)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("column \"%s\" specified in multiple "
									"partitioning keys",
									colname),
					 parser_errposition(cxt->pstate, p->location)));
			}
			cxt->cols = lappend(cxt->cols, colname);
		}

		if (partition_col_walker(p->subPart, context))
			return true;
		return partition_col_walker(p->partSpec, context);
	}

	return expression_tree_walker(node, partition_col_walker, context);
}

/*
 * transformPartitionBy() - transform a partitioning clause attached to a CREATE
 * TABLE statement.
 *
 * An example clause:
 *
 * PARTITION BY col1
 *   SUBPTN  BY col2 SUBTEMPLATE (Spec2),
 *   SUBPTN  BY col3 SUBTEMPLATE (Spec3)
 * (Spec1)
 *
 * becomes a chain of PartitionBy structs, each with a PartitionSpec:
 *
 *   pBy -> (col1, spec1, NULL subspec)
 *    |
 *    v
 *   pBy -> (col2, spec2, NULL subspec)
 *    |
 *    v
 *   pBy -> (col3, spec3, NULL subspec)
 *
 * This struct is easy to process recursively.  However, for the syntax:
 *
 * PARTITION BY col1
 *   SUBPTN  BY col2
 *   SUBPTN  BY col3
 * (
 * PTN AA (SUB CCC (SUB EE, SUB FF), SUB DDD (SUB EE, SUB FF))
 * PTN BB (SUB CCC (SUB EE, SUB FF), SUB DDD (SUB EE, SUB FF))
 * )
 *
 * the struct is more like:
 *
 *   pBy -> (col1, spec1, spec2->spec3)
 *    |
 *    v
 *   pBy -> (col2, NULL spec, NULL subspec)
 *    |
 *    v
 *   pBy -> (col3, NULL spec, NULL subspec)
 *
 *
 * We need to move the subpartition specifications to the correct spots.
 *
 */
static void
transformPartitionBy(ParseState *pstate, CreateStmtContext *cxt,
					 CreateStmt *stmt, Node *partitionBy, GpPolicy *policy)
{
	Oid			snamespaceid;
	char	   *snamespace;
	int		  	partDepth;	/* depth (starting at zero, but display at 1) */
	char	  	depthstr[NAMEDATALEN];
	char	   *at_depth = "";
	char	  	at_buf[NAMEDATALEN];
	int		  	partNumber = -1;
	int		  	partno = 1;
	int			everyno = 0;
	List	   *partElts = NIL;
	ListCell   *lc = NULL;
	PartitionBy		*pBy;				/* the current partitioning clause */
	PartitionBy		*psubBy  = NULL;	/* child of current */
	char	  	 prtstr[NAMEDATALEN];
	Oid 		accessMethodId = InvalidOid;
	bool		lookup_opclass;
	Node	   *prevEvery;
	ListCell   *lc_anp = NULL;
	List	   *key_attnums = NIL;
	List	   *key_attnames = NIL;
	part_col_cxt pcolcxt;
	List	   *stenc = NIL;

	if (NULL == partitionBy)
		return;

	pBy = (PartitionBy *)partitionBy;
	partDepth = pBy->partDepth;
	partDepth++;					/* increment depth for subpartition */

	if (0 < gp_max_partition_level && partDepth > gp_max_partition_level)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("Exceeded the maximum allowed level of partitioning of %d", gp_max_partition_level)));
	}

	snprintf(depthstr, sizeof(depthstr), "%d", partDepth);

	if (pBy->partDepth != 0)
	{
		/*
		 * Only mention the depth 2 and greater to aid in debugging
		 * subpartitions
		 */
		snprintf(at_buf, sizeof(at_buf),
				 " (at depth %d)", partDepth);
		at_depth = at_buf;
	}
	else
		pBy->parentRel = copyObject(stmt->relation);

	/* set the depth for the immediate subpartition */
	if (pBy->subPart)
	{
		psubBy = (PartitionBy *)pBy->subPart;
		psubBy->partDepth = partDepth;
		if (((PartitionBy *)pBy->subPart)->parentRel == NULL)
			((PartitionBy *)pBy->subPart)->parentRel =
					copyObject(pBy->parentRel);
	}

	/*
	 * Derive the number of partitions from the PARTITIONS clause.
	 */
	if (pBy->partNum)
	{
		A_Const *con = (A_Const *)pBy->partNum;

		Assert(IsA(&con->val, Integer));

		partNumber = intVal(&con->val);

		if (pBy->partType != PARTTYP_HASH)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("%sPARTITIONS clause requires a HASH partition%s",
							pBy->partDepth != 0 ? "SUB" : "", at_depth),
					 parser_errposition(pstate, pBy->location)));
		}

		if (partNumber < 1)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("%sPARTITIONS cannot be less than one%s",
							pBy->partDepth != 0 ? "SUB" : "", at_depth),
					 parser_errposition(pstate, pBy->location)));
		}
	}

	/*
	 * The recursive nature of this code means that if we're processing a
	 * sub partition rule, the opclass may have been looked up already.
	 */
	lookup_opclass = list_length(pBy->keyopclass) == 0;

	if ((list_length(pBy->keys) > 1) && (pBy->partType == PARTTYP_RANGE))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("too many columns for RANGE partition%s -- "
						"only one column is allowed.",
						at_depth),
				 parser_errposition(pstate, pBy->location)));
	}

	/* validate keys */
	foreach(lc, pBy->keys)
	{
		Value *colval = lfirst(lc);
		char *colname = strVal(colval);
		ListCell *columns;
		bool found = false;
		Oid typeid = InvalidOid;
		Oid opclass = InvalidOid;
		int i = 0;

		foreach(columns, cxt->columns)
		{
			ColumnDef *column = (ColumnDef *) lfirst(columns);
			Assert(IsA(column, ColumnDef));

			i++;
			if (strcmp(column->colname, colname) == 0)
			{
				found = true;

				if (list_member_int(key_attnums, i))
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("column \"%s\" specified more than once "
									"in partitioning key",
									colname),
					 parser_errposition(pstate, pBy->location)));


				key_attnums = lappend_int(key_attnums, i);
				Insist(IsA(colval, String));
				key_attnames = lappend(key_attnames, colval);

				if (lookup_opclass)
				{
					typeid = column->typname->typid;

					if (!OidIsValid(typeid))
					{
						Type type = typenameType(pstate, column->typname);
						column->typname->typid = typeid = typeTypeId(type);
						ReleaseType(type);
					}
				}
				break;
			}
		}

		if (!found)
		{
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_COLUMN),
					 errmsg("column \"%s\" does not exist in relation \"%s\"",
							colname, cxt->relation->relname),
					 parser_errposition(pstate, pBy->location)));
		}

		if (lookup_opclass)
		{
			/* get access method ID for this partition type */
			switch (pBy->partType)
			{
				case PARTTYP_HASH: accessMethodId = HASH_AM_OID; break;
				case PARTTYP_RANGE:
				case PARTTYP_LIST:
								   accessMethodId = BTREE_AM_OID; break;
				default:
								   elog(ERROR, "unknown partitioning type %i",
										pBy->partType);
								   break;
			}

			opclass = GetDefaultOpClass(typeid, accessMethodId);

			if (!OidIsValid(opclass))
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("data type %s has no default operator class",
								format_type_be(typeid))));
			pBy->keyopclass = lappend_oid(pBy->keyopclass, opclass);
		}
	}

	/* Have partitioning keys; check for violating unique constraints */
	foreach (lc, cxt->ixconstraints)
	{
		ListCell *ilc = NULL;

		Constraint *ucon = (Constraint*)lfirst(lc);
		Insist (ucon->keys != NIL);

		foreach (ilc, key_attnames)
		{
			Value *partkeyname = (Value *)lfirst(ilc);

			if (!list_member(ucon->keys,  partkeyname))
			{
				char *what = NULL;
				switch (ucon->contype)
				{
					case CONSTR_PRIMARY:
						what = "PRIMARY KEY";
						break;
					case CONSTR_UNIQUE:
						what = "UNIQUE";
						break;
					default:
						elog(ERROR, "unexpected constraint type in internal transformation");
						break;
				}
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("%s constraint must contain all columns in the "
								"partition key",
								what),
						 errhint("Include column \"%s\" in the %s constraint or create "
								 "a part-wise UNIQUE index after creating the table instead.",
								 strVal(partkeyname), what)));
			}
		}

	}

	/* No further use for key_attnames, so clean up. */
	if (key_attnames)
	{
		list_free(key_attnames);
	}
	key_attnames = NIL;

	/* see if there are any duplicate column references */
	if (0) /* MPP-3988: allow same column in multiple partitioning
			* keys at different levels */
		partition_col_walker((Node *)pBy, &pcolcxt);

	if (pBy->partType == PARTTYP_HASH)
	{

		if (pBy->partSpec == NULL)
		{
			if (pBy->partNum == NULL)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 		 errmsg("hash partition requires PARTITIONS clause "
								"or partition specification"),
				 		 parser_errposition(pstate, pBy->location)));

			/*
			 * Users don't have to specify a partition specification
			 * for HASH.  If they didn't, create one so that the rest
			 * of the code can generate some valid partition children.
	 		 */
			expand_hash_partition_spec(pBy);
		}
	}

	if (pBy->partSpec)
	{
		partNumber = validate_partition_spec(pstate, cxt, stmt, pBy, at_depth,
											 partNumber);
		stenc = ((PartitionSpec *)pBy->partSpec)->enc_clauses;
	}

	/*
	 * We'd better have some partitions to look at by now
	 */
	if (partNumber < 1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("no partitions specified at depth %d",
						partDepth),
				 parser_errposition(pstate, pBy->location)));
	}

    /* Clear error position. */
    pstate->p_breadcrumb.node = NULL;

	/*
	 * Determine namespace and name to use for the child table.
	 */
	snamespaceid = RangeVarGetCreationNamespace(cxt->relation);
	snamespace = get_namespace_name(snamespaceid);

	/* set up the partition specification element list if it exists */
	if (pBy->partSpec)
	{
		PartitionSpec	*pSpec   = (PartitionSpec *)pBy->partSpec;
		PartitionSpec	*subSpec = (PartitionSpec *)pSpec->subSpec;

		/*
		 * If the current specification has a subSpec, then that is
		 * the specification for the child, so we'd better have a
		 * SUBPARTITION BY clause for the child. The sub specification
		 * cannot contain a partition specification of its own.
		 */
		if (subSpec)
		{
			if (NULL == psubBy)
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("missing SUBPARTITION BY clause for "
								"subpartition specification%s",
								at_depth),
						 parser_errposition(pstate, pBy->location)));
			}
			if (psubBy && psubBy->partSpec)
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("subpartition specification conflict%s",
								at_depth),
						 parser_errposition(pstate, psubBy->location)));
			}
			psubBy->partSpec = (Node *)subSpec;
		}
		partElts = pSpec->partElem;
	}

	Assert(partNumber > 0);

	/*
	 * We must iterate through the elements in this way to support HASH
	 * partitioning, which likely has no partitioning elements.
	 */
	lc = list_head(partElts);

	prevEvery = NULL;
	/* Iterate through each partition element */
	for (partno = 0; partno < partNumber; partno++)
	{
		PartitionBy	*newSub = NULL;
		PartitionElem *pElem = lc ? (PartitionElem *)lfirst(lc) : NULL;

		Node					*pRuleCatalog = NULL;
		Node					*pPostCreate  = NULL;
		Node					*pConstraint  = NULL;
		Node					*pStoreAttr	  = NULL;
		char					*relname	  = NULL;
		PartitionBoundSpec		*pBSpec		  = NULL;
		Node					*every		  = NULL;
		PartitionBy				*curPby		  = NULL;
		bool					 bQuiet		  = false;
		char					*pWithTname	  = NULL;
		List					*colencs	  = NIL;

		/* silence partition name messages if really generating a
		 * subpartition template
		 */
		if (pBy->partQuiet == PART_VERBO_NOPARTNAME)
			bQuiet = true;

		if (pElem)
		{
			pStoreAttr = pElem->storeAttr;
			colencs = pElem->colencs;

			if (pElem->boundSpec && IsA(pElem->boundSpec, PartitionBoundSpec))
			{
				pBSpec	   = (PartitionBoundSpec *)pElem->boundSpec;
				every	   = pBSpec->partEvery;
				pWithTname = pBSpec->pWithTnameStr;

				if (prevEvery && every)
				{
					if (equal(prevEvery, every))
						everyno++;
					else
					{
						everyno = 1; /* reset */
						lc_anp = list_head(pBSpec->everyGenList);
					}
				}
				else if (every)
				{
					everyno++;
					if (!lc_anp)
						lc_anp = list_head(pBSpec->everyGenList);
				}
				else
				{
					everyno = 0;
					lc_anp = NULL;
				}
			}

			if (pElem->partName)
			{
				/*
				 * Use the partition name as part of the child
				 * table name
				 */
				char *pName = strVal(pElem->partName);

				snprintf(prtstr, sizeof(prtstr), "prt_%s", pName);
			}
			else
			{
				/*
				 * For the case where we don't have a partition name,
				 * don't worry about EVERY.
				 */
				if (pElem->rrand)
					/* make a random name for ALTER TABLE ... ADD PARTITION */
					snprintf(prtstr, sizeof(prtstr), "prt_r%lu",
							 pElem->rrand + partno + 1);
				else
					snprintf(prtstr, sizeof(prtstr), "prt_%d", partno + 1);
			}

			if (pElem->subSpec)
			{
				/*
				 * If the current partition element has a subspec,
				 * then that is the spec for the child.  So we'd
				 * better have a SUBPARTITION BY clause for the child,
				 * and that clause cannot contain a SUBPARTITION
				 * TEMPLATE (which is a spec).  Note that we might
				 * have just updated the subBy partition spec with the
				 * subspec of the current spec, which would be a
				 * conflict.  If the psubBy has a NULL spec, we make a
				 * copy of the node and update it -- the subSpec
				 * becomes the specification for the child.  And if
				 * the subspec has a subspec it gets handled at the
				 * top of this loop as the subspec of the current spec
				 * when transformPartitionBy is re-invoked for the
				 * child table.
				 */
				if (NULL == psubBy)
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("missing SUBPARTITION BY clause "
									"for subpartition specification%s",
									at_depth),
							 parser_errposition(pstate, pElem->location)));
				}

				if (psubBy && psubBy->partSpec)
				{
					Assert(((PartitionSpec *)psubBy->partSpec)->istemplate);

					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("subpartition configuration conflicts "
									"with subpartition template"),
							 parser_errposition(pstate, psubBy->location)));
				}

				if (!((PartitionSpec *) pElem->subSpec)->istemplate && !gp_allow_non_uniform_partitioning_ddl)
				{
					ereport(ERROR,
						   (errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						    errmsg("Multi-level partitioned tables without templates are not supported")));
				}

				newSub = makeNode(PartitionBy);

				newSub->partType  = psubBy->partType;
				newSub->keys 	  = psubBy->keys;
				newSub->partNum	  = psubBy->partNum;
				newSub->subPart	  = psubBy->subPart;
				newSub->partSpec  = pElem->subSpec; /* use the subspec */
				newSub->partDepth = psubBy->partDepth;
				newSub->partQuiet = pBy->partQuiet;

				if (pElem->subSpec) /* get a good location for error msg */
				{
					/* use subspec location */
					newSub->location  =
							((PartitionSpec *)pElem->subSpec)->location;
				}
				else
				{
					newSub->location = pElem->location;
				}
			}
		}
		else
			snprintf(prtstr, sizeof(prtstr), "prt_%d", partno + 1);

		/* MPP-6297: check for WITH (tablename=name) clause
		 * [only for dump/restore, set deep in the guts of
		 * partition_range_every...]
		 */
		if (pWithTname)
		{
			relname = pWithTname;
			prtstr[0] = '\0';
		}
		else if (pStoreAttr && ((AlterPartitionCmd *)pStoreAttr)->arg1)
		{
			/* MPP-6297, MPP-7661, MPP-7514: check for WITH
			 * (tablename=name) clause (AGAIN!) because the pWithTname
			 * is only set for an EVERY clause, and we might not have
			 * an EVERY clause.
			 */

			ListCell *prev_lc = NULL;
			ListCell *def_lc  = NULL;
			List *pWithList = (List *)
					(((AlterPartitionCmd *)pStoreAttr)->arg1);

			foreach(def_lc, pWithList)
			{
				DefElem *pDef = (DefElem *)lfirst(def_lc);

				/* get the tablename from the WITH, then remove this
				 * element from the list */
				if (0 == strcmp(pDef->defname, "tablename"))
				{
					/* if the string isn't quoted you get a typename ? */
					if (!IsA(pDef->arg, String))
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("invalid tablename specification")));

					bool need_free_value = false;
					char *relname_str = defGetString(pDef, &need_free_value);
					relname = pstrdup(relname_str);
					if (need_free_value)
					{
						pfree(relname_str);
						relname_str = NULL;
					}

					AssertImply(need_free_value, NULL == relname_str);

					prtstr[0] = '\0';
					pWithList = list_delete_cell(pWithList, def_lc, prev_lc);
					((AlterPartitionCmd *)pStoreAttr)->arg1 =
							(Node *)pWithList;
					break;
				}
				prev_lc = def_lc;
			} /* end foreach */
		}

		if (strlen(prtstr))
			relname = ChooseRelationName(cxt->relation->relname,
										 depthstr, /* depth */
										 prtstr, 	/* part spec */
										 snamespaceid);

		/* XXX: temporarily add rule creation code for debugging */

		/* now that we have the child table name, make the rule */
		if (!(pElem && pElem->isDefault))
		{
			ListCell *lc_rule = NULL;
			int everycount = every ?
					((PartitionRangeItem *)every)->everycount : 0;
			List *allRules =
					make_partition_rules(pstate,
										 cxt, stmt,
										 partitionBy, pElem, at_depth,
										 relname, partno + 1, partNumber,
										 everyno, everycount,
										 &lc_anp, true);

			if (allRules)
				lc_rule = list_head(allRules);

			if (lc_rule)
			{
				pConstraint = lfirst(lc_rule);

				if (pConstraint)
				{
					StringInfoData   sid;
					Constraint *pCon = makeNode(Constraint);

					initStringInfo(&sid);

					appendStringInfo(&sid, "%s_%s", relname, "check");

					pCon->contype = CONSTR_CHECK;
					pCon->name = sid.data;
					pCon->raw_expr = pConstraint;
					pCon->cooked_expr = NULL;
					pCon->indexspace = NULL;

					pConstraint = (Node *)pCon;
				}

				lc_rule = lnext(lc_rule);

				if (lc_rule)
				{
					pRuleCatalog = lfirst(lc_rule);

					/* look for a Rule statement to run after the
					 * relation is created
					 * (see DefinePartitionedRelation in tablecmds.c)
					 */
					lc_rule = lnext(lc_rule);
				}

				if (lc_rule)
				{
					List *pL1 = NULL;
					int  *pInt1;
					int  *pInt2;

					pInt1 = (int *)palloc(sizeof(int));
					pInt2 = (int *)palloc(sizeof(int));

					*pInt1 = partno + 1;
					*pInt2 = everyno;

					pL1 = list_make1(relname); /* rule name */
					pL1 = lappend(pL1, pInt1);     /* partition position */
					pL1 = lappend(pL1, pInt2);     /* every position */

					if (pElem && pElem->partName)
					{
						char *pName = strVal(pElem->partName);

						pL1 = lappend(pL1, pName);
					}
					else
						pL1 = lappend(pL1, NULL);

					pL1 = lappend(pL1, relname);  /* child name */
					pL1 = lappend(pL1, cxt->relation->relname); /* parent name */

					if (enable_partition_rules)
						pPostCreate = (Node *)list_make2(lfirst(lc_rule), pL1);
					else
						pPostCreate = NULL;
				}
			}
		}

		curPby = makeNode(PartitionBy);

		{
			PartitionSpec *tmppspec = makeNode(PartitionSpec);

			/* selectively copy pBy */
			curPby->partType = pBy->partType;

			curPby->keys = key_attnums;

			curPby->keyopclass = copyObject(pBy->keyopclass);
			curPby->partNum = copyObject(pBy->partNum);

			if (pElem)
			{
				PartitionSpec *_spec = (PartitionSpec *)pBy->partSpec;
				tmppspec->partElem = list_make1(copyObject(pElem));
				tmppspec->istemplate = _spec->istemplate;
				tmppspec->enc_clauses = copyObject(stenc);
			}

			curPby->partSpec = (Node *)tmppspec;
			curPby->partDepth = pBy->partDepth;
			curPby->partQuiet = pBy->partQuiet;
			curPby->parentRel = copyObject(pBy->parentRel);
		}

		if (!newSub)
			newSub = copyObject(psubBy);

		if (newSub)
			newSub->parentRel = copyObject(pBy->parentRel);

		make_child_node(stmt, cxt, relname, curPby, (Node *)newSub,
						pRuleCatalog, pPostCreate, pConstraint, pStoreAttr,
						prtstr, bQuiet, colencs);

		if (pBSpec)
			prevEvery = pBSpec->partEvery;

		if (lc)
			lc = lnext(lc);
	}

	/* nefarious: we need to keep the "top"
	 * partition by statement because
	 * analyze.c:do_parse_analyze needs to find
	 * it to re-order the ALTER statements.
	 * (see cdbpartition.c:atpxPart_validate_spec)
	 */
	if ((pBy->partDepth > 0) && (pBy->bKeepMe != true))
	{
		/* we don't need this any more */
		stmt->partitionBy = NULL;
		stmt->is_part_child = true;
	}

} /* end transformPartitionBy */


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
	Query				*qry;
	RangeTblEntry		*rte = NULL;
	ListCell			*l;
	Oid					 idxOid;
	Oid					 nspOid;

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
						 errmsg("index expression may not return a set")));
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
				 errmsg("rule WHERE condition may not contain references to other relations")));

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
						 errmsg("rules with WHERE conditions may only have SELECT, INSERT, UPDATE, or DELETE actions")));

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
								 errmsg("ON SELECT rule may not use OLD")));
					if (has_new)
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
								 errmsg("ON SELECT rule may not use NEW")));
					break;
				case CMD_UPDATE:
					/* both are OK */
					break;
				case CMD_INSERT:
					if (has_old)
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
								 errmsg("ON INSERT rule may not use OLD")));
					break;
				case CMD_DELETE:
					if (has_new)
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
								 errmsg("ON DELETE rule may not use NEW")));
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
	List *grp_tles = NIL;
	ListCell *lc = NULL;
	Index maxsgr = 0;

	grp_tles = get_sortgroupclauses_tles(qry->groupClause, qry->targetList);
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
				 errmsg("ORDER BY on a UNION/INTERSECT/EXCEPT result must be on one of the result columns")));

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
						 errmsg("UNION/INTERSECT/EXCEPT member statement may not refer to other relations of same query level")));
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
		 errmsg("RETURNING may not contain references to other relations")));

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
									   CurrentMemoryContext,
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
	Type		ctype = typenameType(NULL, column->typname);

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

/*
 * transformSingleRowErrorHandling
 *
 * If Single row error handling was specified with an error table, we first
 * check if a table with the same name already exists. If yes, we verify that
 * it has the correct pre-defined error table metadata format (and throw an
 * error is it doesn't). If such a table doesn't exist, we add a CREATE TABLE
 * statement to the before list of the CREATE EXTERNAL TABLE so that it will
 * be created before the external table.
 */
static void
transformSingleRowErrorHandling(ParseState *pstate, CreateStmtContext *cxt,
								SingleRowErrorDesc *sreh)
{

	/* check if error relation exists already */
	Oid errreloid;

	if (sreh->into_file)
	{
		/* LOG ERRORS without table name.  Log into file. */
		return;
	}

	errreloid = RangeVarGetRelid(sreh->errtable, true);

	/*
	 * auto-generate a new error table or verify an existing one
	 */
	if (OidIsValid(errreloid))
	{
		/*
		 * this table already exists in the database. Verify that specified
		 * table is a valid error table
		 */

		Relation rel;

		rel = heap_openrv(sreh->errtable, AccessShareLock);
		ValidateErrorTableMetaData(rel);
		relation_close(rel, AccessShareLock);

		sreh->reusing_existing_errtable = true;
	}
	else
	{
		/*
		 * no such table. auto create it by adding a CREATE TABLE <errortable>
		 * to the before list
		 */

		CreateStmt *createStmt;
		List	   *attrList;
		int			i = 0;

		ereport(NOTICE,
				(errmsg("Error table \"%s\" does not exist. Auto generating an "
						"error table with the same name", sreh->errtable->relname)));
		createStmt = makeNode(CreateStmt);

		/*
		 * create a list of ColumnDef nodes based on the names and types of the
		 * per-defined error table columns
		 */
		attrList = NIL;


		for (i = 1; i <= NUM_ERRORTABLE_ATTR; i++)
		{
			ColumnDef  *coldef = makeNode(ColumnDef);

			coldef->inhcount = 0;
			coldef->is_local = true;
			coldef->is_not_null = false;
			coldef->raw_default = NULL;
			coldef->cooked_default = NULL;
			coldef->constraints = NIL;

			switch (i)
			{
				case errtable_cmdtime:
					coldef->typname = makeTypeNameFromOid(TIMESTAMPTZOID, -1);
					coldef->colname = "cmdtime";
					break;
				case errtable_relname:
					coldef->typname = makeTypeNameFromOid(TEXTOID, -1);
					coldef->colname = "relname";
					break;
				case errtable_filename:
					coldef->typname = makeTypeNameFromOid(TEXTOID, -1);
					coldef->colname = "filename";
					break;
				case errtable_linenum:
					coldef->typname = makeTypeNameFromOid(INT4OID, -1);
					coldef->colname = "linenum";
					break;
				case errtable_bytenum:
					coldef->typname = makeTypeNameFromOid(INT4OID, -1);
					coldef->colname = "bytenum";
					break;
				case errtable_errmsg:
					coldef->typname = makeTypeNameFromOid(TEXTOID, -1);
					coldef->colname = "errmsg";
					break;
				case errtable_rawdata:
					coldef->typname = makeTypeNameFromOid(TEXTOID, -1);
					coldef->colname = "rawdata";
					break;
				case errtable_rawbytes:
					coldef->typname = makeTypeNameFromOid(BYTEAOID, -1);
					coldef->colname = "rawbytes";
					break;
			}
			attrList = lappend(attrList, coldef);
		}

		createStmt->relation = sreh->errtable;
		createStmt->tableElts = attrList;
		createStmt->inhRelations = NIL;
		createStmt->constraints = NIL;
		createStmt->options = NIL;
		createStmt->oncommit = ONCOMMIT_NOOP;
		createStmt->tablespacename = NULL;
		createStmt->relKind = RELKIND_RELATION;
		createStmt->relStorage = RELSTORAGE_HEAP;
		createStmt->distributedBy = list_make1(NULL); /* DISTRIBUTED RANDOMLY */
		createStmt->is_error_table = true;


		cxt->blist = lappend(cxt->blist, createStmt);

		sreh->reusing_existing_errtable = false;

	}

}

/*
 * create a policy with random distribution
 */
GpPolicy *createRandomDistribution(int maxattrs)
{
        GpPolicy*       p = NULL;
        p = (GpPolicy *) palloc(sizeof(GpPolicy) + maxattrs * sizeof(p->attrs[0]));
        p->ptype = POLICYTYPE_PARTITIONED;
        p->nattrs = 0;
        p->attrs[0] = 1;

        return p;
}
