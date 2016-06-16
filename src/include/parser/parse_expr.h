/*-------------------------------------------------------------------------
 *
 * parse_expr.h
 *	  handle expressions in parser
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/parser/parse_expr.h,v 1.38 2008/01/01 19:45:58 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef PARSE_EXPR_H
#define PARSE_EXPR_H

#include "parser/parse_node.h"


/* GUC parameters */
extern bool Transform_null_equals;


extern Node *transformExpr(ParseState *pstate, Node *expr);
extern Node *transformCaseExpr(ParseState *pstate, CaseExpr *c);

extern Oid	exprType(Node *expr);
extern int32 exprTypmod(Node *expr);
extern bool exprIsLengthCoercion(Node *expr, int32 *coercedTypmod);

extern int parse_expr_location(Expr *expr);

#endif   /* PARSE_EXPR_H */
