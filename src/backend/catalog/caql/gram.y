%{
/*-------------------------------------------------------------------------
 *
 * gram.y
 *	  grammar rules for CaQL
 *
 * CaQL is a small set of SQL that operates on catalog tables.
 *
 * We employ reentrant lexer and parser.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "catalog/caqlparse.h"
#include "nodes/nodes.h"
#include "nodes/pg_list.h"
#include "nodes/value.h"

/* include this AFTER caqlparse.h, where YYLTYPE is defined. */
#include "gram.h"

/*
 * This tells bison how to pass the scanner state to lexer function (yylex)
 */
#define YYLEX_PARAM yyparser->scanner

#define caql_yyerror(yylloc, yyparser, msg) caql_scanner_yyerror(msg, yyparser->scanner)

/* Location tracking support --- simpler than bison's default */
#define YYLLOC_DEFAULT(Current, Rhs, N) \
	do { \
		if (N) \
			(Current) = (Rhs)[1]; \
		else \
			(Current) = (Rhs)[0]; \
	} while (0)

/*
 * Function prototypes from scan.l.  We declare them here since they are used
 * in this file and we include scan.c at the end.  Note YYSTYPE is defined
 * in gram.h.
 */
static void caql_scanner_yyerror(const char *message, caql_scanner_t scanner);
static caql_scanner_t caql_scanner_init(const char *str, const char *file, int line);
static void caql_scanner_finish(caql_scanner_t yyscanner);

int caql_yylex(YYSTYPE *lvalp, YYLTYPE *llocp, caql_scanner_t yyscanner);
%}


%expect 0
%pure-parser
%name-prefix="caql_yy"
%parse-param { caql_parser_t yyparser }
%lex-param   { caql_scanner_t YYLEX_PARAM }
%locations

%union
{
	int					ival;
	char				chr;
	char			   *str;
	const char		   *keyword;
	Node			   *node;
	List			   *list;
}

%type <node> stmtblock stmt SelectStmt SelectForUpdateStmt
%type <list> col_list opt_where opt_orderby expr_list
%type <str>  col_elem table_name op_any
%type <node> expr_elem

/*
 * All of CaQL keywords are reserved ones.
 */
%token <keyword> AND
				 BY
				 FOR
				 FROM
				 INTO
				 IS
				 ORDER
				 SELECT
				 UPDATE
				 WHERE

%token <str>	IDENT FCONST SCONST
%token <ival>	ICONST PARAM
%token <str>	OP_EQUAL OP_LT OP_LE OP_GE OP_GT

%left		AND
%left		OP_EQUAL OP_LT OP_LE OP_GE OP_GT

%%

stmtblock: stmt
			{
				yyparser->parsetree = $1;
			}
		;

stmt:
		SelectStmt
		| SelectForUpdateStmt
		| /*EMPTY*/
			{ $$ = NULL; }
		;

SelectStmt:
		SELECT col_list FROM table_name opt_where opt_orderby
			{
				CaQLSelect *n = makeNode(CaQLSelect);

				n->targetlist = $2;
				n->from = $4;
				n->where = $5;
				n->orderby = $6;
				n->forupdate = false;

				$$ = (Node *) n;
			}
		;

SelectForUpdateStmt:
		SELECT col_list FROM table_name opt_where opt_orderby FOR UPDATE
			{
				CaQLSelect *n = makeNode(CaQLSelect);

				n->targetlist = $2;
				n->from = $4;
				n->where = $5;
				n->orderby = $6;
				n->forupdate = true;

				$$ = (Node *) n;
			}
		;

col_list:
		col_elem
			{ $$ = list_make1(makeString($1)); }
		| col_list ',' col_elem
			{ $$ = lappend($1, makeString($3)); }
		| '*'
			{ $$ = list_make1(makeString("*")); }
		;

col_elem:
		IDENT								{ $$ = $1; }
		;

table_name:
		IDENT								{ $$ = $1; }
		;

opt_where:
		WHERE expr_list
			{ $$ = $2; }
		| /*EMPTY*/
			{ $$ = NIL; }
		;

opt_orderby:
		ORDER BY col_list
			{ $$ = $3; }
		| /*EMPTY*/
			{ $$ = NIL; }
		;

expr_list:
		expr_elem							{ $$ = list_make1($1); }
		| expr_list AND expr_elem			{ $$ = lappend($1, $3); }
		;

expr_elem:
		IDENT op_any PARAM
			{
				CaQLExpr *n = makeNode(CaQLExpr);

				n->left = $1;
				n->op = $2;
				n->right = $3;

				$$ = (Node *) n;
			}
/*		| IDENT IS PARAM */
		;

op_any:
		OP_EQUAL							{ $$ = $1; }
		| OP_LT								{ $$ = $1; }
		| OP_LE								{ $$ = $1; }
		| OP_GE								{ $$ = $1; }
		| OP_GT								{ $$ = $1; }
		;
%%

/*
 * Setup CaQL parser.  Returned parser is the same as input.
 */
caql_parser_t
caql_parser_init(const char *query, caql_parser_t yyparser,
				 const char *file, int line)
{
	caql_scanner_t			scanner;

	scanner = caql_scanner_init(query, file, line);
	yyparser->scanner = scanner;
	yyparser->parsetree = NULL;

	return yyparser;
}

/*
 * Clean up the parser.
 */
void
caql_parser_finish(caql_parser_t yyparser)
{
	caql_scanner_finish(yyparser->scanner);
}

/*
 * This is the main interface to the outside.  It returns NULL in unexpected
 * failures, but usually errors out with ereport().
 */
Node *
caql_raw_parser(const char *query, const char *file, int line)
{
	caql_parser		pstate, *parser = &pstate;
	int				yyresult;

	caql_parser_init(query, parser, file, line);
	yyresult = caql_yyparse(parser);
	caql_parser_finish(parser);

	if (yyresult) /* failed? */
		return NULL;

	return pstate.parsetree;
}

/*
 * Must undefine this stuff before including scan.c, since it has different
 * definitions for these macros.
 */
#undef yyerror
#undef yylval
#undef yylloc

#include "scan.c"
