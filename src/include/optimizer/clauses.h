/*-------------------------------------------------------------------------
 *
 * clauses.h
 *	  prototypes for clauses.c.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/optimizer/clauses.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CLAUSES_H
#define CLAUSES_H

#include "access/htup.h"
#include "nodes/pathnodes.h"

#include "optimizer/walkers.h"


// max size of a folded constant when optimizing queries in Orca
// Note: this is to prevent OOM issues when trying to serialize very large constants
// Current limit: 100KB
#define GPOPT_MAX_FOLDED_CONSTANT_SIZE (100*1024)

typedef struct
{
	bool		hasOrderedAggs;	/* any ordered aggs? */
	int			numWindowFuncs; /* total number of WindowFuncs found */
	Index		maxWinRef;		/* windowFuncs[] is indexed 0 .. maxWinRef */
	List	  **windowFuncs;	/* lists of WindowFuncs for each winref */
} WindowFuncLists;

extern bool contain_agg_clause(Node *clause);
extern void get_agg_clause_costs(PlannerInfo *root, Node *clause,
								 AggSplit aggsplit, AggClauseCosts *costs);

extern bool contain_window_function(Node *clause);
extern WindowFuncLists *find_window_functions(Node *clause, Index maxWinRef);

extern double expression_returns_set_rows(PlannerInfo *root, Node *clause);

extern bool contain_subplans(Node *clause);

extern char max_parallel_hazard(Query *parse);
extern bool is_parallel_safe(PlannerInfo *root, Node *node);
extern bool contain_nonstrict_functions(Node *clause);
extern bool contain_exec_param(Node *clause, List *param_ids);
extern bool contain_leaked_vars(Node *clause);

extern Relids find_nonnullable_rels(Node *clause);
extern List *find_nonnullable_vars(Node *clause);
extern List *find_forced_null_vars(Node *clause);
extern Var *find_forced_null_var(Node *clause);

extern char check_execute_on_functions(Node *clause);

extern bool is_pseudo_constant_clause(Node *clause);
extern bool is_pseudo_constant_clause_relids(Node *clause, Relids relids);

extern int	NumRelids(Node *clause);
extern int	NumRelids_new(PlannerInfo *root, Node *clause);

extern void CommuteOpExpr(OpExpr *clause);

extern Query *fold_constants(PlannerInfo *root, Query *q, ParamListInfo boundParams, Size max_size);

extern Expr *transform_array_Const_to_ArrayExpr(Const *c);

extern Query *inline_set_returning_function(PlannerInfo *root,
											RangeTblEntry *rte);

extern Expr *evaluate_expr(Expr *expr, Oid result_type, int32 result_typmod,
			  Oid result_collation);

extern bool subexpression_match(Expr *expr1, Expr *expr2);

// resolve the join alias varno/varattno information to its base varno/varattno information
extern Query *flatten_join_alias_var_optimizer(Query *query, int queryLevel);

#endif							/* CLAUSES_H */
