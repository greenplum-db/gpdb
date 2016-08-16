/*-------------------------------------------------------------------------
 *
 * caqlparse.h
 *	  Declarations for routines exported from lexer and parser files.
 *
 *-------------------------------------------------------------------------
 */

#ifndef CAQLPARSE_H
#define CAQLPARSE_H

#include "access/skey.h"
#include "nodes/nodes.h"
#include "nodes/pg_list.h"

/*
 * We track token locations in terms of byte offsets from the start of the
 * source string, not the column number/line number representation that
 * bison uses by default.  Also, to minimize overhead we track only one
 * location (usually the first token location) for each construct, not
 * the beginning and ending locations as bison does by default.  It's
 * therefore sufficient to make YYLTYPE an int.
 */
#define YYLTYPE  int

/*
 * SELECT and SELECT ... FOR UPDATE statements.
 */
typedef struct CaQLSelect
{
	NodeTag		type;		/* node tag */
	List	   *targetlist;	/* column name list of SELECT clause */
	char	   *from;		/* table name */
	List	   *where;		/* expression list in WHERE */
	List	   *orderby;	/* identifier list in ORDER BY */
	bool		forupdate;	/* true if this is FOR UPDATE */
} CaQLSelect;

/*
 * Expression node.
 *
 * Currently we only use expression in WHERE clause, and the right hand side
 * is always parameter, which is an integer between 1 and 5.
 */
typedef struct CaQLExpr
{
	NodeTag		type;		/* node tag */
	char	   *left;		/* left hand side identifier */
	char	   *op;			/* operator such as '=' */
	int			right;		/* parameter number from 1 to 5 */

	/* postprocess fields */
	AttrNumber		attnum;	/* attribute number */
	StrategyNumber	strategy; /* btree strategy number */
	Oid				fnoid;	/* operator function id */
	Oid				typid;	/* type id of attribute */
} CaQLExpr;

/*
 * The scanner state.  This should be an opaque type to comply reentrant
 * flex scanner.
 */
typedef void *caql_scanner_t;

/*
 * The parser state.
 */
typedef struct caql_parser
{
	caql_scanner_t	scanner;		/* scanner state to pass to yylex() */
	Node		   *parsetree;		/* the top node of result */
} caql_parser;

typedef struct caql_parser *caql_parser_t;

/* gram.y */
extern caql_parser_t caql_parser_init(const char *query, caql_parser_t yyparser,
				 const char *file, int line);
extern void caql_parser_finish(caql_parser_t yyparser);
extern Node *caql_raw_parser(const char *query, const char *file, int lineno);

#endif
