/*-------------------------------------------------------------------------
 *
 * parse_agg.h
 *	  handle aggregates in parser
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/parser/parse_agg.h,v 1.36 2008/01/01 19:45:58 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef PARSE_AGG_H
#define PARSE_AGG_H

#include "parser/parse_node.h"

extern void transformAggregateCall(ParseState *pstate, Aggref *agg, 
                                   List *agg_order);
extern void transformWindowFuncCall(ParseState *pstate, WindowRef *wind);

extern void parseCheckAggregates(ParseState *pstate, Query *qry);
extern void parseProcessWindFuncs(ParseState *pstate, Query *qry);
extern void transformWindowSpec(ParseState *pstate, WindowSpec *spec);
extern void transformWindowSpecExprs(ParseState *pstate);

extern void build_aggregate_fnexprs(Oid *agg_input_types,
						int agg_num_inputs,
						Oid agg_state_type,
						Oid agg_result_type,
						Oid transfn_oid,
						Oid finalfn_oid,
						Oid prelimfn_oid,
						Oid invtransfn_oid,
						Oid invprelimfn_oid,
						Expr **transfnexpr,
						Expr **finalfnexpr,
						Expr **prelimfnexpr,
						Expr **invtransfnexpr,
						Expr **invprelimfnexpr);

extern bool checkExprHasWindFuncs(Node *node);
extern bool checkExprHasGroupExtFuncs(Node *node);

#endif   /* PARSE_AGG_H */
