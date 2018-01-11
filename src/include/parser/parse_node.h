/*-------------------------------------------------------------------------
 *
 * parse_node.h
 *		Internal definitions for parser
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/parser/parse_node.h,v 1.62 2009/06/11 14:49:11 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef PARSE_NODE_H
#define PARSE_NODE_H

#include "nodes/parsenodes.h"
#include "utils/relcache.h"

struct HTAB;  /* utils/hsearch.h */

/*
 * Expression kinds distinguished by transformExpr().  Many of these are not
 * semantically distinct so far as expression transformation goes; rather,
 * we distinguish them so that context-specific error messages can be printed.
 *
 * Note: EXPR_KIND_OTHER is not used in the core code, but is left for use
 * by extension code that might need to call transformExpr().  The core code
 * will not enforce any context-driven restrictions on EXPR_KIND_OTHER
 * expressions, so the caller would have to check for sub-selects, aggregates,
 * and window functions if those need to be disallowed.
 */
typedef enum ParseExprKind
{
	EXPR_KIND_NONE = 0,				/* "not in an expression" */
	EXPR_KIND_OTHER,				/* reserved for extensions */
	EXPR_KIND_JOIN_ON,				/* JOIN ON */
	EXPR_KIND_JOIN_USING,			/* JOIN USING */
	EXPR_KIND_FROM_SUBSELECT,		/* sub-SELECT in FROM clause */
	EXPR_KIND_FROM_FUNCTION,		/* function in FROM clause */
	EXPR_KIND_WHERE,				/* WHERE */
	EXPR_KIND_HAVING,				/* HAVING */
	EXPR_KIND_FILTER,				/* FILTER */
	EXPR_KIND_WINDOW_PARTITION,		/* window definition PARTITION BY */
	EXPR_KIND_WINDOW_ORDER,			/* window definition ORDER BY */
	EXPR_KIND_WINDOW_FRAME_RANGE,	/* window frame clause with RANGE */
	EXPR_KIND_WINDOW_FRAME_ROWS,	/* window frame clause with ROWS */
	EXPR_KIND_SELECT_TARGET,		/* SELECT target list item */
	EXPR_KIND_INSERT_TARGET,		/* INSERT target list item */
	EXPR_KIND_UPDATE_SOURCE,		/* UPDATE assignment source item */
	EXPR_KIND_UPDATE_TARGET,		/* UPDATE assignment target item */
	EXPR_KIND_GROUP_BY,				/* GROUP BY */
	EXPR_KIND_ORDER_BY,				/* ORDER BY */
	EXPR_KIND_DISTINCT_ON,			/* DISTINCT ON */
	EXPR_KIND_LIMIT,				/* LIMIT */
	EXPR_KIND_OFFSET,				/* OFFSET */
	EXPR_KIND_RETURNING,			/* RETURNING */
	EXPR_KIND_VALUES,				/* VALUES */
	EXPR_KIND_CHECK_CONSTRAINT,		/* CHECK constraint for a table */
	EXPR_KIND_DOMAIN_CHECK,			/* CHECK constraint for a domain */
	EXPR_KIND_COLUMN_DEFAULT,		/* default value for a table column */
	EXPR_KIND_FUNCTION_DEFAULT,		/* default parameter value for function */
	EXPR_KIND_INDEX_EXPRESSION,		/* index expression */
	EXPR_KIND_INDEX_PREDICATE,		/* index predicate */
	EXPR_KIND_ALTER_COL_TRANSFORM,	/* transform expr in ALTER COLUMN TYPE */
	EXPR_KIND_EXECUTE_PARAMETER,	/* parameter value in EXECUTE */
	EXPR_KIND_TRIGGER_WHEN,			/* WHEN condition in CREATE TRIGGER */
	EXPR_KIND_PARTITION_EXPRESSION,	/* PARTITION BY expression */

	/* GPDB additions */
	EXPR_KIND_SCATTER_BY			/* SCATTER BY expression */
} ParseExprKind;


/*
 * State information used during parse analysis
 *
 * parentParseState: NULL in a top-level ParseState.  When parsing a subquery,
 * links to current parse state of outer query.
 *
 * p_sourcetext: source string that generated the raw parsetree being
 * analyzed, or NULL if not available.	(The string is used only to
 * generate cursor positions in error messages: we need it to convert
 * byte-wise locations in parse structures to character-wise cursor
 * positions.)
 *
 * p_rtable: list of RTEs that will become the rangetable of the query.
 * Note that neither relname nor refname of these entries are necessarily
 * unique; searching the rtable by name is a bad idea.
 *
 * p_joinexprs: list of JoinExpr nodes associated with p_rtable entries.
 * This is one-for-one with p_rtable, but contains NULLs for non-join
 * RTEs, and may be shorter than p_rtable if the last RTE(s) aren't joins.
 *
 * p_joinlist: list of join items (RangeTblRef and JoinExpr nodes) that
 * will become the fromlist of the query's top-level FromExpr node.
 *
 * p_relnamespace: list of RTEs that represents the current namespace for
 * table lookup, ie, those RTEs that are accessible by qualified names.
 * This may be just a subset of the rtable + joinlist, and/or may contain
 * entries that are not yet added to the main joinlist.
 *
 * p_varnamespace: list of RTEs that represents the current namespace for
 * column lookup, ie, those RTEs that are accessible by unqualified names.
 * This is different from p_relnamespace because a JOIN without an alias does
 * not hide the contained tables (so they must still be in p_relnamespace)
 * but it does hide their columns (unqualified references to the columns must
 * refer to the JOIN, not the member tables).  Also, we put POSTQUEL-style
 * implicit RTEs into p_relnamespace but not p_varnamespace, so that they
 * do not affect the set of columns available for unqualified references.
 *
 * p_ctenamespace: list of CommonTableExprs (WITH items) that are visible
 * at the moment.  This is different from p_relnamespace because you have
 * to make an RTE before you can access a CTE.
 *
 * p_future_ctes: list of CommonTableExprs (WITH items) that are not yet
 * visible due to scope rules.	This is used to help improve error messages.
 *
 * p_windowdefs: list of WindowDefs representing WINDOW and OVER clauses.
 * We collect these while transforming expressions and then transform them
 * afterwards (so that any resjunk tlist items needed for the sort/group
 * clauses end up at the end of the query tlist).  A WindowDef's location in
 * this list, counting from 1, is the winref number to use to reference it.
 *
 * p_paramtypes: an array of p_numparams type OIDs for $n parameter symbols
 * (zeroth entry in array corresponds to $1).  If p_variableparams is true, the
 * set of param types is not predetermined; in that case, a zero array entry
 * means that parameter number hasn't been seen, and UNKNOWNOID means the
 * parameter has been used but its type is not yet known.  NOTE: in a stack
 * of ParseStates, only the topmost ParseState contains paramtype info; but
 * we copy the p_variableparams flag down to the child nodes for speed in
 * coerce_type.
 */
typedef struct ParseState
{
	struct ParseState *parentParseState;		/* stack link */
	const char *p_sourcetext;	/* source text, or NULL if not available */
	List	   *p_rtable;		/* range table so far */
	List	   *p_joinexprs;	/* JoinExprs for RTE_JOIN p_rtable entries */
	List	   *p_joinlist;		/* join items so far (will become FromExpr
								 * node's fromlist) */
	List	   *p_relnamespace; /* current namespace for relations */
	List	   *p_varnamespace; /* current namespace for columns */
	List	   *p_ctenamespace; /* current namespace for common table exprs */
	List	   *p_future_ctes;	/* common table exprs not yet in namespace */
	List	   *p_windowdefs;	/* raw representations of window clauses */
	ParseExprKind p_expr_kind;	/* what kind of expression we're parsing */
	Oid		   *p_paramtypes;	/* OIDs of types for $n parameter symbols */
	int			p_numparams;	/* allocated size of p_paramtypes[] */
	int			p_next_resno;	/* next targetlist resno to assign */
	List	   *p_locking_clause;		/* raw FOR UPDATE/FOR SHARE info */
	Node	   *p_value_substitute;		/* what to replace VALUE with, if any */
	bool		p_variableparams;
	bool		p_hasAggs;
	bool		p_hasWindowFuncs;
	bool		p_hasSubLinks;
	bool		p_is_insert;
	bool		p_is_update;
	Relation	p_target_relation;
	RangeTblEntry *p_target_rangetblentry;
	struct HTAB *p_namecache;  /* parse state object name cache */
	bool        p_hasTblValueExpr;
	bool        p_hasDynamicFunction; /* function w/unstable return type */
	bool		p_hasFuncsWithExecRestrictions; /* funcion with EXECUTE ON MASTER / ALL SEGMENTS */

	List	   *p_grp_tles;
} ParseState;

/* Support for parser_errposition_callback function */
typedef struct ParseCallbackState
{
	ParseState *pstate;
	int			location;
	ErrorContextCallback errcontext;
} ParseCallbackState;


extern ParseState *make_parsestate(ParseState *parentParseState);
extern void free_parsestate(ParseState *pstate);
extern struct HTAB *parser_get_namecache(ParseState *pstate);
extern int	parser_errposition(ParseState *pstate, int location);

extern void setup_parser_errposition_callback(ParseCallbackState *pcbstate,
								  ParseState *pstate, int location);
extern void cancel_parser_errposition_callback(ParseCallbackState *pcbstate);

extern Var *make_var(ParseState *pstate, RangeTblEntry *rte, int attrno,
		 int location);
extern Oid	transformArrayType(Oid arrayType);
extern ArrayRef *transformArraySubscripts(ParseState *pstate,
						 Node *arrayBase,
						 Oid arrayType,
						 Oid elementType,
						 int32 elementTypMod,
						 List *indirection,
						 Node *assignFrom);
extern Const *make_const(ParseState *pstate, Value *value, int location);

#endif   /* PARSE_NODE_H */
