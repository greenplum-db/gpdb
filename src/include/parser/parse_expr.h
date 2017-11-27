/*-------------------------------------------------------------------------
 *
 * parse_expr.h
 *	  handle expressions in parser
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/parser/parse_expr.h,v 1.40 2009/01/01 17:24:00 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef PARSE_EXPR_H
#define PARSE_EXPR_H

#include "parser/parse_node.h"

/* GUC parameters */
extern bool Transform_null_equals;

extern Node *transformExpr(ParseState *pstate, Node *expr, ParseExprKind exprKind);

extern const char *ParseExprKindName(ParseExprKind exprKind);

#endif   /* PARSE_EXPR_H */
