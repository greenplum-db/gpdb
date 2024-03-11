/*-------------------------------------------------------------------------
 *
 * clauses.c
 *	  routines to manipulate qualification clauses
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/util/clauses.c
 *
 * HISTORY
 *	  AUTHOR			DATE			MAJOR EVENT
 *	  Andrew Yu			Nov 3, 1994		clause.c and clauses.c combined
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/oid_dispatch.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_class.h"
#include "catalog/pg_language.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "executor/functions.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/supportnodes.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/plancat.h"
#include "optimizer/planmain.h"
#include "parser/analyze.h"
#include "parser/parse_agg.h"
#include "parser/parse_coerce.h"
#include "parser/parse_func.h"
#include "rewrite/rewriteManip.h"
#include "tcop/tcopprot.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
#include "utils/typcache.h"


/* source-code-compatibility hacks for pull_varnos() API change */
#define pull_varnos(a,b) pull_varnos_new(a,b)

typedef struct
{
	PlannerInfo *root;
	AggSplit	aggsplit;
	AggClauseCosts *costs;
} get_agg_clause_costs_context;

typedef struct
{
	ParamListInfo boundParams;
	PlannerInfo *root;
	List	   *active_fns;
	Node	   *case_val;
	bool		estimate;
	bool		eval_stable_functions;
	bool		recurse_queries; /* recurse into query structures */
	bool		recurse_sublink_testexpr; /* recurse into sublink test expressions */
	Size        max_size; /* max constant binary size in bytes, 0: no restrictions */
} eval_const_expressions_context;

typedef struct
{
	int			nargs;
	List	   *args;
	int		   *usecounts;
} substitute_actual_parameters_context;

typedef struct
{
	int			nargs;
	List	   *args;
	int			sublevels_up;
} substitute_actual_srf_parameters_context;

typedef struct
{
	char	   *proname;
	char	   *prosrc;
} inline_error_callback_arg;

typedef struct
{
	char		max_hazard;		/* worst proparallel hazard found so far */
	char		max_interesting;	/* worst proparallel hazard of interest */
	List	   *safe_param_ids; /* PARAM_EXEC Param IDs to treat as safe */
} max_parallel_hazard_context;

static bool contain_agg_clause_walker(Node *node, void *context);
static bool get_agg_clause_costs_walker(Node *node,
										get_agg_clause_costs_context *context);
static bool find_window_functions_walker(Node *node, WindowFuncLists *lists);
static bool contain_subplans_walker(Node *node, void *context);
static bool contain_mutable_functions_walker(Node *node, void *context);
static bool contain_volatile_functions_walker(Node *node, void *context);
static bool contain_volatile_functions_not_nextval_walker(Node *node, void *context);
static bool max_parallel_hazard_walker(Node *node,
									   max_parallel_hazard_context *context);
static bool contain_nonstrict_functions_walker(Node *node, void *context);
static bool contain_exec_param_walker(Node *node, List *param_ids);
static bool contain_context_dependent_node(Node *clause);
static bool contain_context_dependent_node_walker(Node *node, int *flags);
static bool contain_leaked_vars_walker(Node *node, void *context);
static Relids find_nonnullable_rels_walker(Node *node, bool top_level);
static List *find_nonnullable_vars_walker(Node *node, bool top_level);
static bool is_strict_saop(ScalarArrayOpExpr *expr, bool falseOK);
static Node *eval_const_expressions_mutator(Node *node,
											eval_const_expressions_context *context);
static bool contain_non_const_walker(Node *node, void *context);
static bool ece_function_is_safe(Oid funcid,
								 eval_const_expressions_context *context);
static List *simplify_or_arguments(List *args,
								   eval_const_expressions_context *context,
								   bool *haveNull, bool *forceTrue);
static List *simplify_and_arguments(List *args,
									eval_const_expressions_context *context,
									bool *haveNull, bool *forceFalse);
static Node *simplify_boolean_equality(Oid opno, List *args);
static Expr *simplify_function(Oid funcid,
							   Oid result_type, int32 result_typmod,
							   Oid result_collid, Oid input_collid, List **args_p,
							   bool funcvariadic, bool process_args, bool allow_non_const,
							   eval_const_expressions_context *context);
static bool large_const(Expr *expr, Size max_size);
static List *reorder_function_arguments(List *args, HeapTuple func_tuple);
static List *add_function_defaults(List *args, HeapTuple func_tuple);
static List *fetch_function_defaults(HeapTuple func_tuple);
static void recheck_cast_function_args(List *args, Oid result_type,
									   HeapTuple func_tuple);
static Expr *evaluate_function(Oid funcid, Oid result_type, int32 result_typmod,
							   Oid result_collid, Oid input_collid, List *args,
							   bool funcvariadic,
							   HeapTuple func_tuple,
							   eval_const_expressions_context *context);
static Expr *inline_function(Oid funcid, Oid result_type, Oid result_collid,
							 Oid input_collid, List *args,
							 bool funcvariadic,
							 HeapTuple func_tuple,
							 eval_const_expressions_context *context);
static Node *substitute_actual_parameters(Node *expr, int nargs, List *args,
										  int *usecounts);
static Node *substitute_actual_parameters_mutator(Node *node,
												  substitute_actual_parameters_context *context);
static void sql_inline_error_callback(void *arg);
static Query *substitute_actual_srf_parameters(Query *expr,
											   int nargs, List *args);
static Node *substitute_actual_srf_parameters_mutator(Node *node,
													  substitute_actual_srf_parameters_context *context);
static bool tlist_matches_coltypelist(List *tlist, List *coltypelist);

/*
 * Greenplum specific functions
 */
static bool should_eval_stable_functions(PlannerInfo *root);

/*****************************************************************************
 *		Aggregate-function clause manipulation
 *****************************************************************************/

/*
 * contain_agg_clause
 *	  Recursively search for Aggref/GroupingFunc nodes within a clause.
 *
 *	  Returns true if any aggregate found.
 *
 * This does not descend into subqueries, and so should be used only after
 * reduction of sublinks to subplans, or in contexts where it's known there
 * are no subqueries.  There mustn't be outer-aggregate references either.
 *
 * (If you want something like this but able to deal with subqueries,
 * see rewriteManip.c's contain_aggs_of_level().)
 */
bool
contain_agg_clause(Node *clause)
{
	return contain_agg_clause_walker(clause, NULL);
}

static bool
contain_agg_clause_walker(Node *node, void *context)
{
	if (node == NULL)
		return false;
	if (IsA(node, Aggref))
	{
		Assert(((Aggref *) node)->agglevelsup == 0);
		return true;			/* abort the tree traversal and return true */
	}
	if (IsA(node, GroupingFunc))
	{
		Assert(((GroupingFunc *) node)->agglevelsup == 0);
		return true;			/* abort the tree traversal and return true */
	}
	if (IsA(node, GroupId))
	{
		Assert(((GroupId *) node)->agglevelsup == 0);
		return true;			/* abort the tree traversal and return true */
	}
	if (IsA(node, GroupingSetId))
	{
		return true;			/* abort the tree traversal and return true */
	}

	Assert(!IsA(node, SubLink));
	return expression_tree_walker(node, contain_agg_clause_walker, context);
}

/*
 * get_agg_clause_costs
 *	  Recursively find the Aggref nodes in an expression tree, and
 *	  accumulate cost information about them.
 *
 * 'aggsplit' tells us the expected partial-aggregation mode, which affects
 * the cost estimates.
 *
 * NOTE that the counts/costs are ADDED to those already in *costs ... so
 * the caller is responsible for zeroing the struct initially.
 *
 * We count the nodes, estimate their execution costs, and estimate the total
 * space needed for their transition state values if all are evaluated in
 * parallel (as would be done in a HashAgg plan).  Also, we check whether
 * partial aggregation is feasible.  See AggClauseCosts for the exact set
 * of statistics collected.
 *
 * In addition, we mark Aggref nodes with the correct aggtranstype, so
 * that that doesn't need to be done repeatedly.  (That makes this function's
 * name a bit of a misnomer.)
 *
 * This does not descend into subqueries, and so should be used only after
 * reduction of sublinks to subplans, or in contexts where it's known there
 * are no subqueries.  There mustn't be outer-aggregate references either.
 */
void
get_agg_clause_costs(PlannerInfo *root, Node *clause, AggSplit aggsplit,
					 AggClauseCosts *costs)
{
	get_agg_clause_costs_context context;

	context.root = root;
	context.aggsplit = aggsplit;
	context.costs = costs;
	(void) get_agg_clause_costs_walker(clause, &context);
}

static bool
get_agg_clause_costs_walker(Node *node, get_agg_clause_costs_context *context)
{
	if (node == NULL)
		return false;
	if (IsA(node, Aggref))
	{
		Aggref	   *aggref = (Aggref *) node;
		AggClauseCosts *costs = context->costs;
		HeapTuple	aggTuple;
		Form_pg_aggregate aggform;
		Oid			aggtransfn;
		Oid			aggfinalfn;
		Oid			aggcombinefn;
		Oid			aggserialfn;
		Oid			aggdeserialfn;
		Oid			aggtranstype;
		int32		aggtransspace;
		QualCost	argcosts;

		Assert(aggref->agglevelsup == 0);

		/*
		 * Fetch info about aggregate from pg_aggregate.  Note it's correct to
		 * ignore the moving-aggregate variant, since what we're concerned
		 * with here is aggregates not window functions.
		 */
		aggTuple = SearchSysCache1(AGGFNOID,
								   ObjectIdGetDatum(aggref->aggfnoid));
		if (!HeapTupleIsValid(aggTuple))
			elog(ERROR, "cache lookup failed for aggregate %u",
				 aggref->aggfnoid);
		aggform = (Form_pg_aggregate) GETSTRUCT(aggTuple);
		aggtransfn = aggform->aggtransfn;
		aggfinalfn = aggform->aggfinalfn;
		aggcombinefn = aggform->aggcombinefn;
		aggserialfn = aggform->aggserialfn;
		aggdeserialfn = aggform->aggdeserialfn;
		aggtranstype = aggform->aggtranstype;
		aggtransspace = aggform->aggtransspace;
		ReleaseSysCache(aggTuple);

		/*
		 * Resolve the possibly-polymorphic aggregate transition type, unless
		 * already done in a previous pass over the expression.
		 */
		if (OidIsValid(aggref->aggtranstype))
			aggtranstype = aggref->aggtranstype;
		else
		{
			Oid			inputTypes[FUNC_MAX_ARGS];
			int			numArguments;

			/* extract argument types (ignoring any ORDER BY expressions) */
			numArguments = get_aggregate_argtypes(aggref, inputTypes);

			/* resolve actual type of transition state, if polymorphic */
			aggtranstype = resolve_aggregate_transtype(aggref->aggfnoid,
													   aggtranstype,
													   inputTypes,
													   numArguments);
			aggref->aggtranstype = aggtranstype;
		}

		/*
		 * Count it, and check for cases requiring ordered input.  Note that
		 * ordered-set aggs always have nonempty aggorder.  Any ordered-input
		 * case also defeats partial aggregation.
		 */
		costs->numAggs++;
		if (aggref->aggorder != NIL || aggref->aggdistinct != NIL)
		{
			costs->numOrderedAggs++;
			costs->hasNonPartial = true;
		}

		/*
		 * The PostgreSQL 'numOrderedAggs' field includes DISTINCT aggregates,
		 * too, but cdbgroup.c handles DISTINCT aggregates differently, and
		 * needs to know if there are any purely ordered aggs, not counting
		 * DISTINCT aggs.
		 */
		if (aggref->aggorder != NIL)
			costs->numPureOrderedAggs++;

		if (aggref->aggdistinct != NIL)
			costs->distinctAggrefs = lappend(costs->distinctAggrefs, aggref);

		/*
		 * Check whether partial aggregation is feasible, unless we already
		 * found out that we can't do it.
		 *
		 * In GPDB, we can do two-stage aggregation with DISTINCT-qualified
		 * aggregates, if the data distribution happens to match the DISTINCT
		 * expressions. So we keep track whether all aggregates have combine
		 * functions, even if there are DISTINCT aggregates. hasNonCombine is
		 * set if there are any aggregates without combine functions, even if
		 * there are DISTINCT aggregates.
		 */
		if (!costs->hasNonCombine)
		{
			/*
			 * If there is no combine function, then partial aggregation is
			 * not possible.
			 */
			if (!OidIsValid(aggcombinefn))
			{
				costs->hasNonCombine = true;
				costs->hasNonPartial = true;
			}

			/*
			 * If we have any aggs with transtype INTERNAL then we must check
			 * whether they have serialization/deserialization functions; if
			 * not, we can't serialize partial-aggregation results.
			 */
			else if (aggtranstype == INTERNALOID &&
					 (!OidIsValid(aggserialfn) || !OidIsValid(aggdeserialfn)))
				costs->hasNonSerial = true;
		}

		/*
		 * Add the appropriate component function execution costs to
		 * appropriate totals.
		 */
		if (DO_AGGSPLIT_COMBINE(context->aggsplit))
		{
			/* charge for combining previously aggregated states */
			add_function_cost(context->root, aggcombinefn, NULL,
							  &costs->transCost);
		}
		else
			add_function_cost(context->root, aggtransfn, NULL,
							  &costs->transCost);
		if (DO_AGGSPLIT_DESERIALIZE(context->aggsplit) &&
			OidIsValid(aggdeserialfn))
			add_function_cost(context->root, aggdeserialfn, NULL,
							  &costs->transCost);
		if (DO_AGGSPLIT_SERIALIZE(context->aggsplit) &&
			OidIsValid(aggserialfn))
			add_function_cost(context->root, aggserialfn, NULL,
							  &costs->finalCost);
		if (!DO_AGGSPLIT_SKIPFINAL(context->aggsplit) &&
			OidIsValid(aggfinalfn))
			add_function_cost(context->root, aggfinalfn, NULL,
							  &costs->finalCost);

		/*
		 * These costs are incurred only by the initial aggregate node, so we
		 * mustn't include them again at upper levels.
		 */
		if (!DO_AGGSPLIT_COMBINE(context->aggsplit))
		{
			/* add the input expressions' cost to per-input-row costs */
			cost_qual_eval_node(&argcosts, (Node *) aggref->args, context->root);
			costs->transCost.startup += argcosts.startup;
			costs->transCost.per_tuple += argcosts.per_tuple;

			/*
			 * Add any filter's cost to per-input-row costs.
			 *
			 * XXX Ideally we should reduce input expression costs according
			 * to filter selectivity, but it's not clear it's worth the
			 * trouble.
			 */
			if (aggref->aggfilter)
			{
				cost_qual_eval_node(&argcosts, (Node *) aggref->aggfilter,
									context->root);
				costs->transCost.startup += argcosts.startup;
				costs->transCost.per_tuple += argcosts.per_tuple;
			}
		}

		/*
		 * If there are direct arguments, treat their evaluation cost like the
		 * cost of the finalfn.
		 */
		if (aggref->aggdirectargs)
		{
			cost_qual_eval_node(&argcosts, (Node *) aggref->aggdirectargs,
								context->root);
			costs->finalCost.startup += argcosts.startup;
			costs->finalCost.per_tuple += argcosts.per_tuple;
		}

		/*
		 * If the transition type is pass-by-value then it doesn't add
		 * anything to the required size of the hashtable.  If it is
		 * pass-by-reference then we have to add the estimated size of the
		 * value itself, plus palloc overhead.
		 */
		if (!get_typbyval(aggtranstype))
		{
			int32		avgwidth;

			/* Use average width if aggregate definition gave one */
			if (aggtransspace > 0)
				avgwidth = aggtransspace;
			else if (aggtransfn == F_ARRAY_APPEND)
			{
				/*
				 * If the transition function is array_append(), it'll use an
				 * expanded array as transvalue, which will occupy at least
				 * ALLOCSET_SMALL_INITSIZE and possibly more.  Use that as the
				 * estimate for lack of a better idea.
				 */
				avgwidth = ALLOCSET_SMALL_INITSIZE;
			}
			else
			{
				/*
				 * If transition state is of same type as first aggregated
				 * input, assume it's the same typmod (same width) as well.
				 * This works for cases like MAX/MIN and is probably somewhat
				 * reasonable otherwise.
				 */
				int32		aggtranstypmod = -1;

				if (aggref->args)
				{
					TargetEntry *tle = (TargetEntry *) linitial(aggref->args);

					if (aggtranstype == exprType((Node *) tle->expr))
						aggtranstypmod = exprTypmod((Node *) tle->expr);
				}

				avgwidth = get_typavgwidth(aggtranstype, aggtranstypmod);
			}

			avgwidth = MAXALIGN(avgwidth);
			costs->transitionSpace += avgwidth + 2 * sizeof(void *);
		}
		else if (aggtranstype == INTERNALOID)
		{
			/*
			 * INTERNAL transition type is a special case: although INTERNAL
			 * is pass-by-value, it's almost certainly being used as a pointer
			 * to some large data structure.  The aggregate definition can
			 * provide an estimate of the size.  If it doesn't, then we assume
			 * ALLOCSET_DEFAULT_INITSIZE, which is a good guess if the data is
			 * being kept in a private memory context, as is done by
			 * array_agg() for instance.
			 */
			if (aggtransspace > 0)
				costs->transitionSpace += aggtransspace;
			else
				costs->transitionSpace += ALLOCSET_DEFAULT_INITSIZE;
		}

		/*
		 * Complain if the aggregate's arguments contain any aggregates;
		 * nested agg functions are semantically nonsensical.  Aggregates in
		 * the FILTER clause are detected in transformAggregateCall().
		 */
		if (contain_agg_clause((Node *) aggref->args) ||
			contain_agg_clause((Node *) aggref->aggorder))
			ereport(ERROR,
					(errcode(ERRCODE_GROUPING_ERROR),
					 errmsg("aggregate function calls cannot be nested")));

		/*
		 * We assume that the parser checked that there are no aggregates (of
		 * this level anyway) in the aggregated arguments, direct arguments,
		 * or filter clause.  Hence, we need not recurse into any of them.
		 */
		return false;
	}
	Assert(!IsA(node, SubLink));
	return expression_tree_walker(node, get_agg_clause_costs_walker,
								  (void *) context);
}


/*****************************************************************************
 *		Window-function clause manipulation
 *****************************************************************************/

/*
 * contain_window_function
 *	  Recursively search for WindowFunc nodes within a clause.
 *
 * Since window functions don't have level fields, but are hard-wired to
 * be associated with the current query level, this is just the same as
 * rewriteManip.c's function.
 */
bool
contain_window_function(Node *clause)
{
	return contain_windowfuncs(clause);
}

/*
 * find_window_functions
 *	  Locate all the WindowFunc nodes in an expression tree, and organize
 *	  them by winref ID number.
 *
 * Caller must provide an upper bound on the winref IDs expected in the tree.
 */
WindowFuncLists *
find_window_functions(Node *clause, Index maxWinRef)
{
	WindowFuncLists *lists = palloc(sizeof(WindowFuncLists));

	lists->numWindowFuncs = 0;
	lists->maxWinRef = maxWinRef;
	lists->windowFuncs = (List **) palloc0((maxWinRef + 1) * sizeof(List *));
	(void) find_window_functions_walker(clause, lists);
	return lists;
}

static bool
find_window_functions_walker(Node *node, WindowFuncLists *lists)
{
	if (node == NULL)
		return false;
	if (IsA(node, WindowFunc))
	{
		WindowFunc *wfunc = (WindowFunc *) node;

		/* winref is unsigned, so one-sided test is OK */
		if (wfunc->winref > lists->maxWinRef)
			elog(ERROR, "WindowFunc contains out-of-range winref %u",
				 wfunc->winref);
		/* eliminate duplicates, so that we avoid repeated computation */
		if (!list_member(lists->windowFuncs[wfunc->winref], wfunc))
		{
			lists->windowFuncs[wfunc->winref] =
				lappend(lists->windowFuncs[wfunc->winref], wfunc);
			lists->numWindowFuncs++;
		}

		/*
		 * We assume that the parser checked that there are no window
		 * functions in the arguments or filter clause.  Hence, we need not
		 * recurse into them.  (If either the parser or the planner screws up
		 * on this point, the executor will still catch it; see ExecInitExpr.)
		 */
		return false;
	}
	Assert(!IsA(node, SubLink));
	return expression_tree_walker(node, find_window_functions_walker,
								  (void *) lists);
}


/*****************************************************************************
 *		Support for expressions returning sets
 *****************************************************************************/

/*
 * expression_returns_set_rows
 *	  Estimate the number of rows returned by a set-returning expression.
 *	  The result is 1 if it's not a set-returning expression.
 *
 * We should only examine the top-level function or operator; it used to be
 * appropriate to recurse, but not anymore.  (Even if there are more SRFs in
 * the function's inputs, their multipliers are accounted for separately.)
 *
 * Note: keep this in sync with expression_returns_set() in nodes/nodeFuncs.c.
 */
double
expression_returns_set_rows(PlannerInfo *root, Node *clause)
{
	if (clause == NULL)
		return 1.0;
	if (IsA(clause, FuncExpr))
	{
		FuncExpr   *expr = (FuncExpr *) clause;

		if (expr->funcretset)
			return clamp_row_est(get_function_rows(root, expr->funcid, clause));
	}
	if (IsA(clause, OpExpr))
	{
		OpExpr	   *expr = (OpExpr *) clause;

		if (expr->opretset)
		{
			set_opfuncid(expr);
			return clamp_row_est(get_function_rows(root, expr->opfuncid, clause));
		}
	}
	return 1.0;
}


/*****************************************************************************
 *		Subplan clause manipulation
 *****************************************************************************/

/*
 * contain_subplans
 *	  Recursively search for subplan nodes within a clause.
 *
 * If we see a SubLink node, we will return true.  This is only possible if
 * the expression tree hasn't yet been transformed by subselect.c.  We do not
 * know whether the node will produce a true subplan or just an initplan,
 * but we make the conservative assumption that it will be a subplan.
 *
 * Returns true if any subplan found.
 */
bool
contain_subplans(Node *clause)
{
	return contain_subplans_walker(clause, NULL);
}

static bool
contain_subplans_walker(Node *node, void *context)
{
	if (node == NULL)
		return false;
	if (IsA(node, SubPlan) ||
		IsA(node, AlternativeSubPlan) ||
		IsA(node, SubLink))
		return true;			/* abort the tree traversal and return true */
	return expression_tree_walker(node, contain_subplans_walker, context);
}


/*****************************************************************************
 *		Check clauses for mutable functions
 *****************************************************************************/

/*
 * contain_mutable_functions
 *	  Recursively search for mutable functions within a clause.
 *
 * Returns true if any mutable function (or operator implemented by a
 * mutable function) is found.  This test is needed so that we don't
 * mistakenly think that something like "WHERE random() < 0.5" can be treated
 * as a constant qualification.
 *
 * We will recursively look into Query nodes (i.e., SubLink sub-selects)
 * but not into SubPlans.  See comments for contain_volatile_functions().
 */
bool
contain_mutable_functions(Node *clause)
{
	return contain_mutable_functions_walker(clause, NULL);
}

static bool
contain_mutable_functions_checker(Oid func_id, void *context)
{
	return (func_volatile(func_id) != PROVOLATILE_IMMUTABLE);
}

static bool
contain_mutable_functions_walker(Node *node, void *context)
{
	if (node == NULL)
		return false;

    /* the functions in predtest.c handle expressions and
     * RestrictInfo objects -- so make this function handle
     * them too for convenience */
    if (IsA(node, RestrictInfo))
    {
        RestrictInfo * info = (RestrictInfo *) node;
        return contain_mutable_functions_walker((Node*)info->clause, context);
    }

	/* Check for mutable functions in node itself */
	if (check_functions_in_node(node, contain_mutable_functions_checker,
								context))
		return true;

	if (IsA(node, SQLValueFunction))
	{
		/* all variants of SQLValueFunction are stable */
		return true;
	}

	if (IsA(node, NextValueExpr))
	{
		/* NextValueExpr is volatile */
		return true;
	}

	/*
	 * It should be safe to treat MinMaxExpr as immutable, because it will
	 * depend on a non-cross-type btree comparison function, and those should
	 * always be immutable.  Treating XmlExpr as immutable is more dubious,
	 * and treating CoerceToDomain as immutable is outright dangerous.  But we
	 * have done so historically, and changing this would probably cause more
	 * problems than it would fix.  In practice, if you have a non-immutable
	 * domain constraint you are in for pain anyhow.
	 */

	/* Recurse to check arguments */
	if (IsA(node, Query))
	{
		/* Recurse into subselects */
		return query_tree_walker((Query *) node,
								 contain_mutable_functions_walker,
								 context, 0);
	}
	return expression_tree_walker(node, contain_mutable_functions_walker,
								  context);
}


/*****************************************************************************
 *		Check clauses for volatile functions
 *****************************************************************************/

/*
 * contain_volatile_functions
 *	  Recursively search for volatile functions within a clause.
 *
 * Returns true if any volatile function (or operator implemented by a
 * volatile function) is found. This test prevents, for example,
 * invalid conversions of volatile expressions into indexscan quals.
 *
 * We will recursively look into Query nodes (i.e., SubLink sub-selects)
 * but not into SubPlans.  This is a bit odd, but intentional.  If we are
 * looking at a SubLink, we are probably deciding whether a query tree
 * transformation is safe, and a contained sub-select should affect that;
 * for example, duplicating a sub-select containing a volatile function
 * would be bad.  However, once we've got to the stage of having SubPlans,
 * subsequent planning need not consider volatility within those, since
 * the executor won't change its evaluation rules for a SubPlan based on
 * volatility.
 */
bool
contain_volatile_functions(Node *clause)
{
	return contain_volatile_functions_walker(clause, NULL);
}

static bool
contain_volatile_functions_checker(Oid func_id, void *context)
{
	return (func_volatile(func_id) == PROVOLATILE_VOLATILE);
}

static bool
contain_volatile_functions_walker(Node *node, void *context)
{
	if (node == NULL)
		return false;

	/*
	 * We need to handle RestrictInfo, a case that uses this
	 * is that replicated table with a volatile restriction.
	 * We have to find the pattern and turn it into singleQE.
	 */
	if (IsA(node, RestrictInfo))
	{
		RestrictInfo * info = (RestrictInfo *) node;
		return contain_volatile_functions_walker((Node*)info->clause, context);
	}

	/* Check for volatile functions in node itself */
	if (check_functions_in_node(node, contain_volatile_functions_checker,
								context))
		return true;

	if (IsA(node, NextValueExpr))
	{
		/* NextValueExpr is volatile */
		return true;
	}

	/*
	 * See notes in contain_mutable_functions_walker about why we treat
	 * MinMaxExpr, XmlExpr, and CoerceToDomain as immutable, while
	 * SQLValueFunction is stable.  Hence, none of them are of interest here.
	 */

	/* Recurse to check arguments */
	if (IsA(node, Query))
	{
		/* Recurse into subselects */
		return query_tree_walker((Query *) node,
								 contain_volatile_functions_walker,
								 context, 0);
	}

	return expression_tree_walker(node, contain_volatile_functions_walker,
								  context);
}

/*
 * Special purpose version of contain_volatile_functions() for use in COPY:
 * ignore nextval(), but treat all other functions normally.
 */
bool
contain_volatile_functions_not_nextval(Node *clause)
{
	return contain_volatile_functions_not_nextval_walker(clause, NULL);
}

static bool
contain_volatile_functions_not_nextval_checker(Oid func_id, void *context)
{
	return (func_id != F_NEXTVAL_OID &&
			func_volatile(func_id) == PROVOLATILE_VOLATILE);
}

static bool
contain_volatile_functions_not_nextval_walker(Node *node, void *context)
{
	if (node == NULL)
		return false;
	/* Check for volatile functions in node itself */
	if (check_functions_in_node(node,
								contain_volatile_functions_not_nextval_checker,
								context))
		return true;

	/*
	 * See notes in contain_mutable_functions_walker about why we treat
	 * MinMaxExpr, XmlExpr, and CoerceToDomain as immutable, while
	 * SQLValueFunction is stable.  Hence, none of them are of interest here.
	 * Also, since we're intentionally ignoring nextval(), presumably we
	 * should ignore NextValueExpr.
	 */

	/* Recurse to check arguments */
	if (IsA(node, Query))
	{
		/* Recurse into subselects */
		return query_tree_walker((Query *) node,
								 contain_volatile_functions_not_nextval_walker,
								 context, 0);
	}
	return expression_tree_walker(node,
								  contain_volatile_functions_not_nextval_walker,
								  context);
}


/*****************************************************************************
 *		Check queries for parallel unsafe and/or restricted constructs
 *****************************************************************************/

/*
 * max_parallel_hazard
 *		Find the worst parallel-hazard level in the given query
 *
 * Returns the worst function hazard property (the earliest in this list:
 * PROPARALLEL_UNSAFE, PROPARALLEL_RESTRICTED, PROPARALLEL_SAFE) that can
 * be found in the given parsetree.  We use this to find out whether the query
 * can be parallelized at all.  The caller will also save the result in
 * PlannerGlobal so as to short-circuit checks of portions of the querytree
 * later, in the common case where everything is SAFE.
 */
char
max_parallel_hazard(Query *parse)
{
	max_parallel_hazard_context context;

	context.max_hazard = PROPARALLEL_SAFE;
	context.max_interesting = PROPARALLEL_UNSAFE;
	context.safe_param_ids = NIL;
	(void) max_parallel_hazard_walker((Node *) parse, &context);
	return context.max_hazard;
}

/*
 * is_parallel_safe
 *		Detect whether the given expr contains only parallel-safe functions
 *
 * root->glob->maxParallelHazard must previously have been set to the
 * result of max_parallel_hazard() on the whole query.
 */
bool
is_parallel_safe(PlannerInfo *root, Node *node)
{
	max_parallel_hazard_context context;
	PlannerInfo *proot;
	ListCell   *l;

	/*
	 * Even if the original querytree contained nothing unsafe, we need to
	 * search the expression if we have generated any PARAM_EXEC Params while
	 * planning, because those are parallel-restricted and there might be one
	 * in this expression.  But otherwise we don't need to look.
	 */
	if (root->glob->maxParallelHazard == PROPARALLEL_SAFE &&
		root->glob->paramExecTypes == NIL)
		return true;
	/* Else use max_parallel_hazard's search logic, but stop on RESTRICTED */
	context.max_hazard = PROPARALLEL_SAFE;
	context.max_interesting = PROPARALLEL_RESTRICTED;
	context.safe_param_ids = NIL;

	/*
	 * The params that refer to the same or parent query level are considered
	 * parallel-safe.  The idea is that we compute such params at Gather or
	 * Gather Merge node and pass their value to workers.
	 */
	for (proot = root; proot != NULL; proot = proot->parent_root)
	{
		foreach(l, proot->init_plans)
		{
			SubPlan    *initsubplan = (SubPlan *) lfirst(l);
			ListCell   *l2;

			foreach(l2, initsubplan->setParam)
				context.safe_param_ids = lcons_int(lfirst_int(l2),
												   context.safe_param_ids);
		}
	}

	return !max_parallel_hazard_walker(node, &context);
}

/* core logic for all parallel-hazard checks */
static bool
max_parallel_hazard_test(char proparallel, max_parallel_hazard_context *context)
{
	switch (proparallel)
	{
		case PROPARALLEL_SAFE:
			/* nothing to see here, move along */
			break;
		case PROPARALLEL_RESTRICTED:
			/* increase max_hazard to RESTRICTED */
			Assert(context->max_hazard != PROPARALLEL_UNSAFE);
			context->max_hazard = proparallel;
			/* done if we are not expecting any unsafe functions */
			if (context->max_interesting == proparallel)
				return true;
			break;
		case PROPARALLEL_UNSAFE:
			context->max_hazard = proparallel;
			/* we're always done at the first unsafe construct */
			return true;
		default:
			elog(ERROR, "unrecognized proparallel value \"%c\"", proparallel);
			break;
	}
	return false;
}

/* check_functions_in_node callback */
static bool
max_parallel_hazard_checker(Oid func_id, void *context)
{
	return max_parallel_hazard_test(func_parallel(func_id),
									(max_parallel_hazard_context *) context);
}

static bool
max_parallel_hazard_walker(Node *node, max_parallel_hazard_context *context)
{
	if (node == NULL)
		return false;

	/* Check for hazardous functions in node itself */
	if (check_functions_in_node(node, max_parallel_hazard_checker,
								context))
		return true;

	/*
	 * It should be OK to treat MinMaxExpr as parallel-safe, since btree
	 * opclass support functions are generally parallel-safe.  XmlExpr is a
	 * bit more dubious but we can probably get away with it.  We err on the
	 * side of caution by treating CoerceToDomain as parallel-restricted.
	 * (Note: in principle that's wrong because a domain constraint could
	 * contain a parallel-unsafe function; but useful constraints probably
	 * never would have such, and assuming they do would cripple use of
	 * parallel query in the presence of domain types.)  SQLValueFunction
	 * should be safe in all cases.  NextValueExpr is parallel-unsafe.
	 */
	if (IsA(node, CoerceToDomain))
	{
		if (max_parallel_hazard_test(PROPARALLEL_RESTRICTED, context))
			return true;
	}

	else if (IsA(node, NextValueExpr))
	{
		if (max_parallel_hazard_test(PROPARALLEL_UNSAFE, context))
			return true;
	}

	/*
	 * Treat window functions as parallel-restricted because we aren't sure
	 * whether the input row ordering is fully deterministic, and the output
	 * of window functions might vary across workers if not.  (In some cases,
	 * like where the window frame orders by a primary key, we could relax
	 * this restriction.  But it doesn't currently seem worth expending extra
	 * effort to do so.)
	 */
	else if (IsA(node, WindowFunc))
	{
		if (max_parallel_hazard_test(PROPARALLEL_RESTRICTED, context))
			return true;
	}

	/*
	 * As a notational convenience for callers, look through RestrictInfo.
	 */
	else if (IsA(node, RestrictInfo))
	{
		RestrictInfo *rinfo = (RestrictInfo *) node;

		return max_parallel_hazard_walker((Node *) rinfo->clause, context);
	}

	/*
	 * Really we should not see SubLink during a max_interesting == restricted
	 * scan, but if we do, return true.
	 */
	else if (IsA(node, SubLink))
	{
		if (max_parallel_hazard_test(PROPARALLEL_RESTRICTED, context))
			return true;
	}

	/*
	 * Only parallel-safe SubPlans can be sent to workers.  Within the
	 * testexpr of the SubPlan, Params representing the output columns of the
	 * subplan can be treated as parallel-safe, so temporarily add their IDs
	 * to the safe_param_ids list while examining the testexpr.
	 */
	else if (IsA(node, SubPlan))
	{
		SubPlan    *subplan = (SubPlan *) node;
		List	   *save_safe_param_ids;

		if (!subplan->parallel_safe &&
			max_parallel_hazard_test(PROPARALLEL_RESTRICTED, context))
			return true;
		save_safe_param_ids = context->safe_param_ids;
		context->safe_param_ids = list_concat(list_copy(subplan->paramIds),
											  context->safe_param_ids);
		if (max_parallel_hazard_walker(subplan->testexpr, context))
			return true;		/* no need to restore safe_param_ids */
		context->safe_param_ids = save_safe_param_ids;
		/* we must also check args, but no special Param treatment there */
		if (max_parallel_hazard_walker((Node *) subplan->args, context))
			return true;
		/* don't want to recurse normally, so we're done */
		return false;
	}

	/*
	 * We can't pass Params to workers at the moment either, so they are also
	 * parallel-restricted, unless they are PARAM_EXTERN Params or are
	 * PARAM_EXEC Params listed in safe_param_ids, meaning they could be
	 * either generated within workers or can be computed by the leader and
	 * then their value can be passed to workers.
	 */
	else if (IsA(node, Param))
	{
		Param	   *param = (Param *) node;

		if (param->paramkind == PARAM_EXTERN)
			return false;

		if (param->paramkind != PARAM_EXEC ||
			!list_member_int(context->safe_param_ids, param->paramid))
		{
			if (max_parallel_hazard_test(PROPARALLEL_RESTRICTED, context))
				return true;
		}
		return false;			/* nothing to recurse to */
	}

	/*
	 * When we're first invoked on a completely unplanned tree, we must
	 * recurse into subqueries so to as to locate parallel-unsafe constructs
	 * anywhere in the tree.
	 */
	else if (IsA(node, Query))
	{
		Query	   *query = (Query *) node;

		/* SELECT FOR UPDATE/SHARE must be treated as unsafe */
		if (query->rowMarks != NULL)
		{
			context->max_hazard = PROPARALLEL_UNSAFE;
			return true;
		}

		/* Recurse into subselects */
		return query_tree_walker(query,
								 max_parallel_hazard_walker,
								 context, 0);
	}

	/* Recurse to check arguments */
	return expression_tree_walker(node,
								  max_parallel_hazard_walker,
								  context);
}


/*****************************************************************************
 *		Check clauses for nonstrict functions
 *****************************************************************************/

/*
 * contain_nonstrict_functions
 *	  Recursively search for nonstrict functions within a clause.
 *
 * Returns true if any nonstrict construct is found --- ie, anything that
 * could produce non-NULL output with a NULL input.
 *
 * The idea here is that the caller has verified that the expression contains
 * one or more Var or Param nodes (as appropriate for the caller's need), and
 * now wishes to prove that the expression result will be NULL if any of these
 * inputs is NULL.  If we return false, then the proof succeeded.
 */
bool
contain_nonstrict_functions(Node *clause)
{
	return contain_nonstrict_functions_walker(clause, NULL);
}

static bool
contain_nonstrict_functions_checker(Oid func_id, void *context)
{
	return !func_strict(func_id);
}

static bool
contain_nonstrict_functions_walker(Node *node, void *context)
{
	if (node == NULL)
		return false;
	if (IsA(node, Aggref))
	{
		/* an aggregate could return non-null with null input */
		return true;
	}
	if (IsA(node, GroupingFunc))
	{
		/*
		 * A GroupingFunc doesn't evaluate its arguments, and therefore must
		 * be treated as nonstrict.
		 */
		return true;
	}
	if (IsA(node, WindowFunc))
	{
		/* a window function could return non-null with null input */
		return true;
	}
	if (IsA(node, SubscriptingRef))
	{
		/*
		 * subscripting assignment is nonstrict, but subscripting itself is
		 * strict
		 */
		if (((SubscriptingRef *) node)->refassgnexpr != NULL)
			return true;

		/* else fall through to check args */
	}
	if (IsA(node, DistinctExpr))
	{
		/* IS DISTINCT FROM is inherently non-strict */
		return true;
	}
	if (IsA(node, NullIfExpr))
	{
		/* NULLIF is inherently non-strict */
		return true;
	}
	if (IsA(node, BoolExpr))
	{
		BoolExpr   *expr = (BoolExpr *) node;

		switch (expr->boolop)
		{
			case AND_EXPR:
			case OR_EXPR:
				/* AND, OR are inherently non-strict */
				return true;
			default:
				break;
		}
	}
	if (IsA(node, SubLink))
	{
		/* In some cases a sublink might be strict, but in general not */
		return true;
	}
	if (IsA(node, SubPlan))
		return true;
	if (IsA(node, AlternativeSubPlan))
		return true;
	if (IsA(node, FieldStore))
		return true;
	if (IsA(node, CoerceViaIO))
	{
		/*
		 * CoerceViaIO is strict regardless of whether the I/O functions are,
		 * so just go look at its argument; asking check_functions_in_node is
		 * useless expense and could deliver the wrong answer.
		 */
		return contain_nonstrict_functions_walker((Node *) ((CoerceViaIO *) node)->arg,
												  context);
	}
	if (IsA(node, ArrayCoerceExpr))
	{
		/*
		 * ArrayCoerceExpr is strict at the array level, regardless of what
		 * the per-element expression is; so we should ignore elemexpr and
		 * recurse only into the arg.
		 */
		return contain_nonstrict_functions_walker((Node *) ((ArrayCoerceExpr *) node)->arg,
												  context);
	}
	if (IsA(node, CaseExpr))
		return true;
	if (IsA(node, ArrayExpr))
		return true;
	if (IsA(node, RowExpr))
		return true;
	if (IsA(node, RowCompareExpr))
		return true;
	if (IsA(node, CoalesceExpr))
		return true;
	if (IsA(node, MinMaxExpr))
		return true;
	if (IsA(node, XmlExpr))
		return true;
	if (IsA(node, NullTest))
		return true;
	if (IsA(node, BooleanTest))
		return true;

	/* Check other function-containing nodes */
	if (check_functions_in_node(node, contain_nonstrict_functions_checker,
								context))
		return true;

	return expression_tree_walker(node, contain_nonstrict_functions_walker,
								  context);
}

/*****************************************************************************
 *		Check clauses for Params
 *****************************************************************************/

/*
 * contain_exec_param
 *	  Recursively search for PARAM_EXEC Params within a clause.
 *
 * Returns true if the clause contains any PARAM_EXEC Param with a paramid
 * appearing in the given list of Param IDs.  Does not descend into
 * subqueries!
 */
bool
contain_exec_param(Node *clause, List *param_ids)
{
	return contain_exec_param_walker(clause, param_ids);
}

static bool
contain_exec_param_walker(Node *node, List *param_ids)
{
	if (node == NULL)
		return false;
	if (IsA(node, Param))
	{
		Param	   *p = (Param *) node;

		if (p->paramkind == PARAM_EXEC &&
			list_member_int(param_ids, p->paramid))
			return true;
	}
	return expression_tree_walker(node, contain_exec_param_walker, param_ids);
}

/*****************************************************************************
 *		Check clauses for context-dependent nodes
 *****************************************************************************/

/*
 * contain_context_dependent_node
 *	  Recursively search for context-dependent nodes within a clause.
 *
 * CaseTestExpr nodes must appear directly within the corresponding CaseExpr,
 * not nested within another one, or they'll see the wrong test value.  If one
 * appears "bare" in the arguments of a SQL function, then we can't inline the
 * SQL function for fear of creating such a situation.  The same applies for
 * CaseTestExpr used within the elemexpr of an ArrayCoerceExpr.
 *
 * CoerceToDomainValue would have the same issue if domain CHECK expressions
 * could get inlined into larger expressions, but presently that's impossible.
 * Still, it might be allowed in future, or other node types with similar
 * issues might get invented.  So give this function a generic name, and set
 * up the recursion state to allow multiple flag bits.
 */
static bool
contain_context_dependent_node(Node *clause)
{
	int			flags = 0;

	return contain_context_dependent_node_walker(clause, &flags);
}

#define CCDN_CASETESTEXPR_OK	0x0001	/* CaseTestExpr okay here? */

static bool
contain_context_dependent_node_walker(Node *node, int *flags)
{
	if (node == NULL)
		return false;
	if (IsA(node, CaseTestExpr))
		return !(*flags & CCDN_CASETESTEXPR_OK);
	else if (IsA(node, CaseExpr))
	{
		CaseExpr   *caseexpr = (CaseExpr *) node;

		/*
		 * If this CASE doesn't have a test expression, then it doesn't create
		 * a context in which CaseTestExprs should appear, so just fall
		 * through and treat it as a generic expression node.
		 */
		if (caseexpr->arg)
		{
			int			save_flags = *flags;
			bool		res;

			/*
			 * Note: in principle, we could distinguish the various sub-parts
			 * of a CASE construct and set the flag bit only for some of them,
			 * since we are only expecting CaseTestExprs to appear in the
			 * "expr" subtree of the CaseWhen nodes.  But it doesn't really
			 * seem worth any extra code.  If there are any bare CaseTestExprs
			 * elsewhere in the CASE, something's wrong already.
			 */
			*flags |= CCDN_CASETESTEXPR_OK;
			res = expression_tree_walker(node,
										 contain_context_dependent_node_walker,
										 (void *) flags);
			*flags = save_flags;
			return res;
		}
	}
	else if (IsA(node, ArrayCoerceExpr))
	{
		ArrayCoerceExpr *ac = (ArrayCoerceExpr *) node;
		int			save_flags;
		bool		res;

		/* Check the array expression */
		if (contain_context_dependent_node_walker((Node *) ac->arg, flags))
			return true;

		/* Check the elemexpr, which is allowed to contain CaseTestExpr */
		save_flags = *flags;
		*flags |= CCDN_CASETESTEXPR_OK;
		res = contain_context_dependent_node_walker((Node *) ac->elemexpr,
													flags);
		*flags = save_flags;
		return res;
	}
	return expression_tree_walker(node, contain_context_dependent_node_walker,
								  (void *) flags);
}

/*****************************************************************************
 *		  Check clauses for Vars passed to non-leakproof functions
 *****************************************************************************/

/*
 * contain_leaked_vars
 *		Recursively scan a clause to discover whether it contains any Var
 *		nodes (of the current query level) that are passed as arguments to
 *		leaky functions.
 *
 * Returns true if the clause contains any non-leakproof functions that are
 * passed Var nodes of the current query level, and which might therefore leak
 * data.  Such clauses must be applied after any lower-level security barrier
 * clauses.
 */
bool
contain_leaked_vars(Node *clause)
{
	return contain_leaked_vars_walker(clause, NULL);
}

static bool
contain_leaked_vars_checker(Oid func_id, void *context)
{
	return !get_func_leakproof(func_id);
}

static bool
contain_leaked_vars_walker(Node *node, void *context)
{
	if (node == NULL)
		return false;

	switch (nodeTag(node))
	{
		case T_Var:
		case T_Const:
		case T_Param:
		case T_ArrayExpr:
		case T_FieldSelect:
		case T_FieldStore:
		case T_NamedArgExpr:
		case T_BoolExpr:
		case T_RelabelType:
		case T_CollateExpr:
		case T_CaseExpr:
		case T_CaseTestExpr:
		case T_RowExpr:
		case T_SQLValueFunction:
		case T_NullTest:
		case T_BooleanTest:
		case T_NextValueExpr:
		case T_List:

			/*
			 * We know these node types don't contain function calls; but
			 * something further down in the node tree might.
			 */
			break;

		case T_FuncExpr:
		case T_OpExpr:
		case T_DistinctExpr:
		case T_NullIfExpr:
		case T_ScalarArrayOpExpr:
		case T_CoerceViaIO:
		case T_ArrayCoerceExpr:

			/*
			 * If node contains a leaky function call, and there's any Var
			 * underneath it, reject.
			 */
			if (check_functions_in_node(node, contain_leaked_vars_checker,
										context) &&
				contain_var_clause(node))
				return true;
			break;

		case T_SubscriptingRef:
			{
				SubscriptingRef *sbsref = (SubscriptingRef *) node;

				/*
				 * subscripting assignment is leaky, but subscripted fetches
				 * are not
				 */
				if (sbsref->refassgnexpr != NULL)
				{
					/* Node is leaky, so reject if it contains Vars */
					if (contain_var_clause(node))
						return true;
				}
			}
			break;

		case T_RowCompareExpr:
			{
				/*
				 * It's worth special-casing this because a leaky comparison
				 * function only compromises one pair of row elements, which
				 * might not contain Vars while others do.
				 */
				RowCompareExpr *rcexpr = (RowCompareExpr *) node;
				ListCell   *opid;
				ListCell   *larg;
				ListCell   *rarg;

				forthree(opid, rcexpr->opnos,
						 larg, rcexpr->largs,
						 rarg, rcexpr->rargs)
				{
					Oid			funcid = get_opcode(lfirst_oid(opid));

					if (!get_func_leakproof(funcid) &&
						(contain_var_clause((Node *) lfirst(larg)) ||
						 contain_var_clause((Node *) lfirst(rarg))))
						return true;
				}
			}
			break;

		case T_MinMaxExpr:
			{
				/*
				 * MinMaxExpr is leakproof if the comparison function it calls
				 * is leakproof.
				 */
				MinMaxExpr *minmaxexpr = (MinMaxExpr *) node;
				TypeCacheEntry *typentry;
				bool		leakproof;

				/* Look up the btree comparison function for the datatype */
				typentry = lookup_type_cache(minmaxexpr->minmaxtype,
											 TYPECACHE_CMP_PROC);
				if (OidIsValid(typentry->cmp_proc))
					leakproof = get_func_leakproof(typentry->cmp_proc);
				else
				{
					/*
					 * The executor will throw an error, but here we just
					 * treat the missing function as leaky.
					 */
					leakproof = false;
				}

				if (!leakproof &&
					contain_var_clause((Node *) minmaxexpr->args))
					return true;
			}
			break;

		case T_CurrentOfExpr:

			/*
			 * WHERE CURRENT OF doesn't contain leaky function calls.
			 * Moreover, it is essential that this is considered non-leaky,
			 * since the planner must always generate a TID scan when CURRENT
			 * OF is present -- cf. cost_tidscan.
			 */
			return false;

		default:

			/*
			 * If we don't recognize the node tag, assume it might be leaky.
			 * This prevents an unexpected security hole if someone adds a new
			 * node type that can call a function.
			 */
			return true;
	}
	return expression_tree_walker(node, contain_leaked_vars_walker,
								  context);
}

/*
 * find_nonnullable_rels
 *		Determine which base rels are forced nonnullable by given clause.
 *
 * Returns the set of all Relids that are referenced in the clause in such
 * a way that the clause cannot possibly return TRUE if any of these Relids
 * is an all-NULL row.  (It is OK to err on the side of conservatism; hence
 * the analysis here is simplistic.)
 *
 * The semantics here are subtly different from contain_nonstrict_functions:
 * that function is concerned with NULL results from arbitrary expressions,
 * but here we assume that the input is a Boolean expression, and wish to
 * see if NULL inputs will provably cause a FALSE-or-NULL result.  We expect
 * the expression to have been AND/OR flattened and converted to implicit-AND
 * format.
 *
 * Note: this function is largely duplicative of find_nonnullable_vars().
 * The reason not to simplify this function into a thin wrapper around
 * find_nonnullable_vars() is that the tested conditions really are different:
 * a clause like "t1.v1 IS NOT NULL OR t1.v2 IS NOT NULL" does not prove
 * that either v1 or v2 can't be NULL, but it does prove that the t1 row
 * as a whole can't be all-NULL.  Also, the behavior for PHVs is different.
 *
 * top_level is true while scanning top-level AND/OR structure; here, showing
 * the result is either FALSE or NULL is good enough.  top_level is false when
 * we have descended below a NOT or a strict function: now we must be able to
 * prove that the subexpression goes to NULL.
 *
 * We don't use expression_tree_walker here because we don't want to descend
 * through very many kinds of nodes; only the ones we can be sure are strict.
 */
Relids
find_nonnullable_rels(Node *clause)
{
	return find_nonnullable_rels_walker(clause, true);
}

static Relids
find_nonnullable_rels_walker(Node *node, bool top_level)
{
	Relids		result = NULL;
	ListCell   *l;

	if (node == NULL)
		return NULL;
	if (IsA(node, Var))
	{
		Var		   *var = (Var *) node;

		if (var->varlevelsup == 0)
			result = bms_make_singleton(var->varno);
	}
	else if (IsA(node, List))
	{
		/*
		 * At top level, we are examining an implicit-AND list: if any of the
		 * arms produces FALSE-or-NULL then the result is FALSE-or-NULL. If
		 * not at top level, we are examining the arguments of a strict
		 * function: if any of them produce NULL then the result of the
		 * function must be NULL.  So in both cases, the set of nonnullable
		 * rels is the union of those found in the arms, and we pass down the
		 * top_level flag unmodified.
		 */
		foreach(l, (List *) node)
		{
			result = bms_join(result,
							  find_nonnullable_rels_walker(lfirst(l),
														   top_level));
		}
	}
	else if (IsA(node, FuncExpr))
	{
		FuncExpr   *expr = (FuncExpr *) node;

		if (func_strict(expr->funcid))
			result = find_nonnullable_rels_walker((Node *) expr->args, false);
	}
	else if (IsA(node, OpExpr))
	{
		OpExpr	   *expr = (OpExpr *) node;

		set_opfuncid(expr);
		if (func_strict(expr->opfuncid))
			result = find_nonnullable_rels_walker((Node *) expr->args, false);
	}
	else if (IsA(node, ScalarArrayOpExpr))
	{
		/* Strict if it's "foo op ANY array" and op is strict */
		ScalarArrayOpExpr *expr = (ScalarArrayOpExpr *) node;

		if (expr->useOr && op_strict(expr->opno))
			result = find_nonnullable_rels_walker((Node *) expr->args, false);
	}
	else if (IsA(node, BoolExpr))
	{
		BoolExpr   *expr = (BoolExpr *) node;

		switch (expr->boolop)
		{
			case AND_EXPR:
				/* At top level we can just recurse (to the List case) */
				if (top_level)
				{
					result = find_nonnullable_rels_walker((Node *) expr->args,
														  top_level);
					break;
				}

				/*
				 * Below top level, even if one arm produces NULL, the result
				 * could be FALSE (hence not NULL).  However, if *all* the
				 * arms produce NULL then the result is NULL, so we can take
				 * the intersection of the sets of nonnullable rels, just as
				 * for OR.  Fall through to share code.
				 */
				/* FALL THRU */
			case OR_EXPR:

				/*
				 * OR is strict if all of its arms are, so we can take the
				 * intersection of the sets of nonnullable rels for each arm.
				 * This works for both values of top_level.
				 */
				foreach(l, expr->args)
				{
					Relids		subresult;

					subresult = find_nonnullable_rels_walker(lfirst(l),
															 top_level);
					if (result == NULL) /* first subresult? */
						result = subresult;
					else
						result = bms_int_members(result, subresult);

					/*
					 * If the intersection is empty, we can stop looking. This
					 * also justifies the test for first-subresult above.
					 */
					if (bms_is_empty(result))
						break;
				}
				break;
			case NOT_EXPR:
				/* NOT will return null if its arg is null */
				result = find_nonnullable_rels_walker((Node *) expr->args,
													  false);
				break;
			default:
				elog(ERROR, "unrecognized boolop: %d", (int) expr->boolop);
				break;
		}
	}
	else if (IsA(node, RelabelType))
	{
		RelabelType *expr = (RelabelType *) node;

		result = find_nonnullable_rels_walker((Node *) expr->arg, top_level);
	}
	else if (IsA(node, CoerceViaIO))
	{
		/* not clear this is useful, but it can't hurt */
		CoerceViaIO *expr = (CoerceViaIO *) node;

		result = find_nonnullable_rels_walker((Node *) expr->arg, top_level);
	}
	else if (IsA(node, ArrayCoerceExpr))
	{
		/* ArrayCoerceExpr is strict at the array level; ignore elemexpr */
		ArrayCoerceExpr *expr = (ArrayCoerceExpr *) node;

		result = find_nonnullable_rels_walker((Node *) expr->arg, top_level);
	}
	else if (IsA(node, ConvertRowtypeExpr))
	{
		/* not clear this is useful, but it can't hurt */
		ConvertRowtypeExpr *expr = (ConvertRowtypeExpr *) node;

		result = find_nonnullable_rels_walker((Node *) expr->arg, top_level);
	}
	else if (IsA(node, CollateExpr))
	{
		CollateExpr *expr = (CollateExpr *) node;

		result = find_nonnullable_rels_walker((Node *) expr->arg, top_level);
	}
	else if (IsA(node, NullTest))
	{
		/* IS NOT NULL can be considered strict, but only at top level */
		NullTest   *expr = (NullTest *) node;

		if (top_level && expr->nulltesttype == IS_NOT_NULL && !expr->argisrow)
			result = find_nonnullable_rels_walker((Node *) expr->arg, false);
	}
	else if (IsA(node, BooleanTest))
	{
		/* Boolean tests that reject NULL are strict at top level */
		BooleanTest *expr = (BooleanTest *) node;

		if (top_level &&
			(expr->booltesttype == IS_TRUE ||
			 expr->booltesttype == IS_FALSE ||
			 expr->booltesttype == IS_NOT_UNKNOWN))
			result = find_nonnullable_rels_walker((Node *) expr->arg, false);
	}
	else if (IsA(node, PlaceHolderVar))
	{
		PlaceHolderVar *phv = (PlaceHolderVar *) node;

		/*
		 * If the contained expression forces any rels non-nullable, so does
		 * the PHV.
		 */
		result = find_nonnullable_rels_walker((Node *) phv->phexpr, top_level);

		/*
		 * If the PHV's syntactic scope is exactly one rel, it will be forced
		 * to be evaluated at that rel, and so it will behave like a Var of
		 * that rel: if the rel's entire output goes to null, so will the PHV.
		 * (If the syntactic scope is a join, we know that the PHV will go to
		 * null if the whole join does; but that is AND semantics while we
		 * need OR semantics for find_nonnullable_rels' result, so we can't do
		 * anything with the knowledge.)
		 */
		if (phv->phlevelsup == 0 &&
			bms_membership(phv->phrels) == BMS_SINGLETON)
			result = bms_add_members(result, phv->phrels);
	}
	return result;
}

/*
 * find_nonnullable_vars
 *		Determine which Vars are forced nonnullable by given clause.
 *
 * Returns a list of all level-zero Vars that are referenced in the clause in
 * such a way that the clause cannot possibly return TRUE if any of these Vars
 * is NULL.  (It is OK to err on the side of conservatism; hence the analysis
 * here is simplistic.)
 *
 * The semantics here are subtly different from contain_nonstrict_functions:
 * that function is concerned with NULL results from arbitrary expressions,
 * but here we assume that the input is a Boolean expression, and wish to
 * see if NULL inputs will provably cause a FALSE-or-NULL result.  We expect
 * the expression to have been AND/OR flattened and converted to implicit-AND
 * format.
 *
 * The result is a palloc'd List, but we have not copied the member Var nodes.
 * Also, we don't bother trying to eliminate duplicate entries.
 *
 * top_level is true while scanning top-level AND/OR structure; here, showing
 * the result is either FALSE or NULL is good enough.  top_level is false when
 * we have descended below a NOT or a strict function: now we must be able to
 * prove that the subexpression goes to NULL.
 *
 * We don't use expression_tree_walker here because we don't want to descend
 * through very many kinds of nodes; only the ones we can be sure are strict.
 */
List *
find_nonnullable_vars(Node *clause)
{
	return find_nonnullable_vars_walker(clause, true);
}

static List *
find_nonnullable_vars_walker(Node *node, bool top_level)
{
	List	   *result = NIL;
	ListCell   *l;

	if (node == NULL)
		return NIL;
	if (IsA(node, Var))
	{
		Var		   *var = (Var *) node;

		if (var->varlevelsup == 0)
			result = list_make1(var);
	}
	else if (IsA(node, List))
	{
		/*
		 * At top level, we are examining an implicit-AND list: if any of the
		 * arms produces FALSE-or-NULL then the result is FALSE-or-NULL. If
		 * not at top level, we are examining the arguments of a strict
		 * function: if any of them produce NULL then the result of the
		 * function must be NULL.  So in both cases, the set of nonnullable
		 * vars is the union of those found in the arms, and we pass down the
		 * top_level flag unmodified.
		 */
		foreach(l, (List *) node)
		{
			result = list_concat(result,
								 find_nonnullable_vars_walker(lfirst(l),
															  top_level));
		}
	}
	else if (IsA(node, FuncExpr))
	{
		FuncExpr   *expr = (FuncExpr *) node;

		if (func_strict(expr->funcid))
			result = find_nonnullable_vars_walker((Node *) expr->args, false);
	}
	else if (IsA(node, OpExpr))
	{
		OpExpr	   *expr = (OpExpr *) node;

		set_opfuncid(expr);
		if (func_strict(expr->opfuncid))
			result = find_nonnullable_vars_walker((Node *) expr->args, false);
	}
	else if (IsA(node, ScalarArrayOpExpr))
	{
		ScalarArrayOpExpr *expr = (ScalarArrayOpExpr *) node;

		if (is_strict_saop(expr, true))
			result = find_nonnullable_vars_walker((Node *) expr->args, false);
	}
	else if (IsA(node, BoolExpr))
	{
		BoolExpr   *expr = (BoolExpr *) node;

		switch (expr->boolop)
		{
			case AND_EXPR:
				/* At top level we can just recurse (to the List case) */
				if (top_level)
				{
					result = find_nonnullable_vars_walker((Node *) expr->args,
														  top_level);
					break;
				}

				/*
				 * Below top level, even if one arm produces NULL, the result
				 * could be FALSE (hence not NULL).  However, if *all* the
				 * arms produce NULL then the result is NULL, so we can take
				 * the intersection of the sets of nonnullable vars, just as
				 * for OR.  Fall through to share code.
				 */
				/* FALL THRU */
			case OR_EXPR:

				/*
				 * OR is strict if all of its arms are, so we can take the
				 * intersection of the sets of nonnullable vars for each arm.
				 * This works for both values of top_level.
				 */
				foreach(l, expr->args)
				{
					List	   *subresult;

					subresult = find_nonnullable_vars_walker(lfirst(l),
															 top_level);
					if (result == NIL)	/* first subresult? */
						result = subresult;
					else
						result = list_intersection(result, subresult);

					/*
					 * If the intersection is empty, we can stop looking. This
					 * also justifies the test for first-subresult above.
					 */
					if (result == NIL)
						break;
				}
				break;
			case NOT_EXPR:
				/* NOT will return null if its arg is null */
				result = find_nonnullable_vars_walker((Node *) expr->args,
													  false);
				break;
			default:
				elog(ERROR, "unrecognized boolop: %d", (int) expr->boolop);
				break;
		}
	}
	else if (IsA(node, RelabelType))
	{
		RelabelType *expr = (RelabelType *) node;

		result = find_nonnullable_vars_walker((Node *) expr->arg, top_level);
	}
	else if (IsA(node, CoerceViaIO))
	{
		/* not clear this is useful, but it can't hurt */
		CoerceViaIO *expr = (CoerceViaIO *) node;

		result = find_nonnullable_vars_walker((Node *) expr->arg, false);
	}
	else if (IsA(node, ArrayCoerceExpr))
	{
		/* ArrayCoerceExpr is strict at the array level; ignore elemexpr */
		ArrayCoerceExpr *expr = (ArrayCoerceExpr *) node;

		result = find_nonnullable_vars_walker((Node *) expr->arg, top_level);
	}
	else if (IsA(node, ConvertRowtypeExpr))
	{
		/* not clear this is useful, but it can't hurt */
		ConvertRowtypeExpr *expr = (ConvertRowtypeExpr *) node;

		result = find_nonnullable_vars_walker((Node *) expr->arg, top_level);
	}
	else if (IsA(node, CollateExpr))
	{
		CollateExpr *expr = (CollateExpr *) node;

		result = find_nonnullable_vars_walker((Node *) expr->arg, top_level);
	}
	else if (IsA(node, NullTest))
	{
		/* IS NOT NULL can be considered strict, but only at top level */
		NullTest   *expr = (NullTest *) node;

		if (top_level && expr->nulltesttype == IS_NOT_NULL && !expr->argisrow)
			result = find_nonnullable_vars_walker((Node *) expr->arg, false);
	}
	else if (IsA(node, BooleanTest))
	{
		/* Boolean tests that reject NULL are strict at top level */
		BooleanTest *expr = (BooleanTest *) node;

		if (top_level &&
			(expr->booltesttype == IS_TRUE ||
			 expr->booltesttype == IS_FALSE ||
			 expr->booltesttype == IS_NOT_UNKNOWN))
			result = find_nonnullable_vars_walker((Node *) expr->arg, false);
	}
	else if (IsA(node, PlaceHolderVar))
	{
		PlaceHolderVar *phv = (PlaceHolderVar *) node;

		result = find_nonnullable_vars_walker((Node *) phv->phexpr, top_level);
	}
	return result;
}

/*
 * find_forced_null_vars
 *		Determine which Vars must be NULL for the given clause to return TRUE.
 *
 * This is the complement of find_nonnullable_vars: find the level-zero Vars
 * that must be NULL for the clause to return TRUE.  (It is OK to err on the
 * side of conservatism; hence the analysis here is simplistic.  In fact,
 * we only detect simple "var IS NULL" tests at the top level.)
 *
 * The result is a palloc'd List, but we have not copied the member Var nodes.
 * Also, we don't bother trying to eliminate duplicate entries.
 */
List *
find_forced_null_vars(Node *node)
{
	List	   *result = NIL;
	Var		   *var;
	ListCell   *l;

	if (node == NULL)
		return NIL;
	/* Check single-clause cases using subroutine */
	var = find_forced_null_var(node);
	if (var)
	{
		result = list_make1(var);
	}
	/* Otherwise, handle AND-conditions */
	else if (IsA(node, List))
	{
		/*
		 * At top level, we are examining an implicit-AND list: if any of the
		 * arms produces FALSE-or-NULL then the result is FALSE-or-NULL.
		 */
		foreach(l, (List *) node)
		{
			result = list_concat(result,
								 find_forced_null_vars(lfirst(l)));
		}
	}
	else if (IsA(node, BoolExpr))
	{
		BoolExpr   *expr = (BoolExpr *) node;

		/*
		 * We don't bother considering the OR case, because it's fairly
		 * unlikely anyone would write "v1 IS NULL OR v1 IS NULL". Likewise,
		 * the NOT case isn't worth expending code on.
		 */
		if (expr->boolop == AND_EXPR)
		{
			/* At top level we can just recurse (to the List case) */
			result = find_forced_null_vars((Node *) expr->args);
		}
	}
	return result;
}

/*
 * find_forced_null_var
 *		Return the Var forced null by the given clause, or NULL if it's
 *		not an IS NULL-type clause.  For success, the clause must enforce
 *		*only* nullness of the particular Var, not any other conditions.
 *
 * This is just the single-clause case of find_forced_null_vars(), without
 * any allowance for AND conditions.  It's used by initsplan.c on individual
 * qual clauses.  The reason for not just applying find_forced_null_vars()
 * is that if an AND of an IS NULL clause with something else were to somehow
 * survive AND/OR flattening, initsplan.c might get fooled into discarding
 * the whole clause when only the IS NULL part of it had been proved redundant.
 */
Var *
find_forced_null_var(Node *node)
{
	if (node == NULL)
		return NULL;
	if (IsA(node, NullTest))
	{
		/* check for var IS NULL */
		NullTest   *expr = (NullTest *) node;

		if (expr->nulltesttype == IS_NULL && !expr->argisrow)
		{
			Var		   *var = (Var *) expr->arg;

			if (var && IsA(var, Var) &&
				var->varlevelsup == 0)
				return var;
		}
	}
	else if (IsA(node, BooleanTest))
	{
		/* var IS UNKNOWN is equivalent to var IS NULL */
		BooleanTest *expr = (BooleanTest *) node;

		if (expr->booltesttype == IS_UNKNOWN)
		{
			Var		   *var = (Var *) expr->arg;

			if (var && IsA(var, Var) &&
				var->varlevelsup == 0)
				return var;
		}
	}
	return NULL;
}

/*
 * Can we treat a ScalarArrayOpExpr as strict?
 *
 * If "falseOK" is true, then a "false" result can be considered strict,
 * else we need to guarantee an actual NULL result for NULL input.
 *
 * "foo op ALL array" is strict if the op is strict *and* we can prove
 * that the array input isn't an empty array.  We can check that
 * for the cases of an array constant and an ARRAY[] construct.
 *
 * "foo op ANY array" is strict in the falseOK sense if the op is strict.
 * If not falseOK, the test is the same as for "foo op ALL array".
 */
static bool
is_strict_saop(ScalarArrayOpExpr *expr, bool falseOK)
{
	Node	   *rightop;

	/* The contained operator must be strict. */
	set_sa_opfuncid(expr);
	if (!func_strict(expr->opfuncid))
		return false;
	/* If ANY and falseOK, that's all we need to check. */
	if (expr->useOr && falseOK)
		return true;
	/* Else, we have to see if the array is provably non-empty. */
	Assert(list_length(expr->args) == 2);
	rightop = (Node *) lsecond(expr->args);
	if (rightop && IsA(rightop, Const))
	{
		Datum		arraydatum = ((Const *) rightop)->constvalue;
		bool		arrayisnull = ((Const *) rightop)->constisnull;
		ArrayType  *arrayval;
		int			nitems;

		if (arrayisnull)
			return false;
		arrayval = DatumGetArrayTypeP(arraydatum);
		nitems = ArrayGetNItems(ARR_NDIM(arrayval), ARR_DIMS(arrayval));
		if (nitems > 0)
			return true;
	}
	else if (rightop && IsA(rightop, ArrayExpr))
	{
		ArrayExpr  *arrayexpr = (ArrayExpr *) rightop;

		if (arrayexpr->elements != NIL && !arrayexpr->multidims)
			return true;
	}
	return false;
}



typedef struct
{
	char		exec_location;
} check_execute_on_functions_context;

static bool
check_execute_on_functions_walker(Node *node,
								  check_execute_on_functions_context *context)
{
	if (node == NULL)
		return false;

	if (IsA(node, FuncExpr))
	{
		FuncExpr   *expr = (FuncExpr *) node;
		char		exec_location;

		exec_location = func_exec_location(expr->funcid);
		if (exec_location != PROEXECLOCATION_ANY && exec_location != context->exec_location)
		{
			if (context->exec_location != PROEXECLOCATION_ANY)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot mix EXECUTE ON COORDINATOR and EXECUTE ON ALL SEGMENTS functions in same query level")));
			context->exec_location = exec_location;
		}
		/* fall through to check args */
	}
	return expression_tree_walker(node, check_execute_on_functions_walker, context);
}


char
check_execute_on_functions(Node *clause)
{
	check_execute_on_functions_context context;

	context.exec_location = PROEXECLOCATION_ANY;

	check_execute_on_functions_walker(clause, &context);

	return context.exec_location;
}

/*****************************************************************************
 *		Check for "pseudo-constant" clauses
 *****************************************************************************/

/*
 * is_pseudo_constant_clause
 *	  Detect whether an expression is "pseudo constant", ie, it contains no
 *	  variables of the current query level and no uses of volatile functions.
 *	  Such an expr is not necessarily a true constant: it can still contain
 *	  Params and outer-level Vars, not to mention functions whose results
 *	  may vary from one statement to the next.  However, the expr's value
 *	  will be constant over any one scan of the current query, so it can be
 *	  used as, eg, an indexscan key.  (Actually, the condition for indexscan
 *	  keys is weaker than this; see is_pseudo_constant_for_index().)
 *
 * CAUTION: this function omits to test for one very important class of
 * not-constant expressions, namely aggregates (Aggrefs).  In current usage
 * this is only applied to WHERE clauses and so a check for Aggrefs would be
 * a waste of cycles; but be sure to also check contain_agg_clause() if you
 * want to know about pseudo-constness in other contexts.  The same goes
 * for window functions (WindowFuncs).
 */
bool
is_pseudo_constant_clause(Node *clause)
{
	/*
	 * We could implement this check in one recursive scan.  But since the
	 * check for volatile functions is both moderately expensive and unlikely
	 * to fail, it seems better to look for Vars first and only check for
	 * volatile functions if we find no Vars.
	 */
	if (!contain_var_clause(clause) &&
		!contain_volatile_functions(clause))
		return true;
	return false;
}

/*
 * is_pseudo_constant_clause_relids
 *	  Same as above, except caller already has available the var membership
 *	  of the expression; this lets us avoid the contain_var_clause() scan.
 */
bool
is_pseudo_constant_clause_relids(Node *clause, Relids relids)
{
	if (bms_is_empty(relids) &&
		!contain_volatile_functions(clause))
		return true;
	return false;
}


/*****************************************************************************
 *																			 *
 *		General clause-manipulating routines								 *
 *																			 *
 *****************************************************************************/

/*
 * NumRelids
 *		(formerly clause_relids)
 *
 * Returns the number of different relations referenced in 'clause'.
 */
int
NumRelids(Node *clause)
{
	return NumRelids_new(NULL, clause);
}

int
NumRelids_new(PlannerInfo *root, Node *clause)
{
	Relids		varnos = pull_varnos(root, clause);
	int			result = bms_num_members(varnos);

	bms_free(varnos);
	return result;
}

/*
 * CommuteOpExpr: commute a binary operator clause
 *
 * XXX the clause is destructively modified!
 */
void
CommuteOpExpr(OpExpr *clause)
{
	Oid			opoid;
	Node	   *temp;

	/* Sanity checks: caller is at fault if these fail */
	if (!is_opclause(clause) ||
		list_length(clause->args) != 2)
		elog(ERROR, "cannot commute non-binary-operator clause");

	opoid = get_commutator(clause->opno);

	if (!OidIsValid(opoid))
		elog(ERROR, "could not find commutator for operator %u",
			 clause->opno);

	/*
	 * modify the clause in-place!
	 */
	clause->opno = opoid;
	clause->opfuncid = InvalidOid;
	/* opresulttype, opretset, opcollid, inputcollid need not change */

	temp = linitial(clause->args);
	linitial(clause->args) = lsecond(clause->args);
	lsecond(clause->args) = temp;
}

/*
 * Helper for eval_const_expressions: check that datatype of an attribute
 * is still what it was when the expression was parsed.  This is needed to
 * guard against improper simplification after ALTER COLUMN TYPE.  (XXX we
 * may well need to make similar checks elsewhere?)
 *
 * rowtypeid may come from a whole-row Var, and therefore it can be a domain
 * over composite, but for this purpose we only care about checking the type
 * of a contained field.
 */
static bool
rowtype_field_matches(Oid rowtypeid, int fieldnum,
					  Oid expectedtype, int32 expectedtypmod,
					  Oid expectedcollation)
{
	TupleDesc	tupdesc;
	Form_pg_attribute attr;

	/* No issue for RECORD, since there is no way to ALTER such a type */
	if (rowtypeid == RECORDOID)
		return true;
	tupdesc = lookup_rowtype_tupdesc_domain(rowtypeid, -1, false);
	if (fieldnum <= 0 || fieldnum > tupdesc->natts)
	{
		ReleaseTupleDesc(tupdesc);
		return false;
	}
	attr = TupleDescAttr(tupdesc, fieldnum - 1);
	if (attr->attisdropped ||
		attr->atttypid != expectedtype ||
		attr->atttypmod != expectedtypmod ||
		attr->attcollation != expectedcollation)
	{
		ReleaseTupleDesc(tupdesc);
		return false;
	}
	ReleaseTupleDesc(tupdesc);
	return true;
}


/**
 * fold_constants
 *
 * Recurses into query tree and folds all constant expressions.
 */

Query *
fold_constants(PlannerInfo *root, Query *q, ParamListInfo boundParams, Size max_size)
{
	eval_const_expressions_context context;

	context.root = root;
	context.boundParams = boundParams;
	context.active_fns = NIL;	/* nothing being recursively simplified */
	context.case_val = NULL;	/* no CASE being examined */
	context.estimate = false;	/* safe transformations only */
	context.recurse_queries = true; /* recurse into query structures */
	context.recurse_sublink_testexpr = false; /* do not recurse into sublink test expressions */

	context.max_size = max_size;
	
	context.eval_stable_functions = should_eval_stable_functions(root);

	return (Query *) query_or_expression_tree_mutator
						(
						(Node *) q,
						eval_const_expressions_mutator,
						&context,
						0
						);
}

/*
 * Transform a small array constant to an ArrayExpr.
 *
 * This is used by ORCA, to transform the array argument of a ScalarArrayExpr
 * into an ArrayExpr. If a ScalarArrayExpr has an ArrayExpr argument, ORCA can
 * perform some optimizations - partition pruning at least - by first expanding
 * the ArrayExpr into its disjunctive normal form and then deriving constraints
 * based on the elements in the ArrayExpr. It doesn't currently know how to
 * extract elements from an Array const, however, so to enable those
 * optimizations in ORCA, we convert Array Consts into corresponding
 * ArrayExprs.
 *
 * If the argument is not an array constant return the original Const unmodified.
 * We convert an array const of any size to ArrayExpr. ORCA can use it to derive
 * statistics.
 */
Expr *
transform_array_Const_to_ArrayExpr(Const *c)
{
	Oid			elemtype;
	int16		elemlen;
	bool		elembyval;
	char		elemalign;
	int			nelems;
	Datum	   *elems;
	bool	   *nulls;
	ArrayType  *ac;
	ArrayExpr *aexpr;
	int			i;

	Assert(IsA(c, Const));

	/* Does it look like the right kind of an array Const? */
	if (c->constisnull)
		return (Expr *) c;	/* NULL const */

	elemtype = get_element_type(c->consttype);
	if (elemtype == InvalidOid)
		return (Expr *) c;	/* not an array */

	ac = DatumGetArrayTypeP(c->constvalue);
	nelems = ArrayGetNItems(ARR_NDIM(ac), ARR_DIMS(ac));

	/* All set, extract the elements, and an ArrayExpr to hold them. */
	get_typlenbyvalalign(elemtype, &elemlen, &elembyval, &elemalign);
	deconstruct_array(ac, elemtype, elemlen, elembyval, elemalign,
					  &elems, &nulls, &nelems);

	aexpr = makeNode(ArrayExpr);
	aexpr->array_typeid = c->consttype;
	aexpr->element_typeid = elemtype;
	aexpr->multidims = false;
	aexpr->location = c->location;

	for (i = 0; i < nelems; i++)
	{
		aexpr->elements = lappend(aexpr->elements,
								  makeConst(elemtype,
											-1,
											c->constcollid,
											elemlen,
											elems[i],
											nulls[i],
											elembyval));
	}

	return (Expr *) aexpr;
}

/*--------------------
 * eval_const_expressions
 *
 * Reduce any recognizably constant subexpressions of the given
 * expression tree, for example "2 + 2" => "4".  More interestingly,
 * we can reduce certain boolean expressions even when they contain
 * non-constant subexpressions: "x OR true" => "true" no matter what
 * the subexpression x is.  (XXX We assume that no such subexpression
 * will have important side-effects, which is not necessarily a good
 * assumption in the presence of user-defined functions; do we need a
 * pg_proc flag that prevents discarding the execution of a function?)
 *
 * We do understand that certain functions may deliver non-constant
 * results even with constant inputs, "nextval()" being the classic
 * example.  Functions that are not marked "immutable" in pg_proc
 * will not be pre-evaluated here, although we will reduce their
 * arguments as far as possible.
 *
 * Whenever a function is eliminated from the expression by means of
 * constant-expression evaluation or inlining, we add the function to
 * root->glob->invalItems.  This ensures the plan is known to depend on
 * such functions, even though they aren't referenced anymore.
 *
 * We assume that the tree has already been type-checked and contains
 * only operators and functions that are reasonable to try to execute.
 *
 * NOTE: "root" can be passed as NULL if the caller never wants to do any
 * Param substitutions nor receive info about inlined functions.
 *
 * NOTE: the planner assumes that this will always flatten nested AND and
 * OR clauses into N-argument form.  See comments in prepqual.c.
 *
 * NOTE: another critical effect is that any function calls that require
 * default arguments will be expanded, and named-argument calls will be
 * converted to positional notation.  The executor won't handle either.
 *--------------------
 */
Node *
eval_const_expressions(PlannerInfo *root, Node *node)
{
	eval_const_expressions_context context;
	Node                          *result;
	List                          *saved_oid_assignments;

	if (root)
		context.boundParams = root->glob->boundParams;	/* bound Params */
	else
		context.boundParams = NULL;
	context.root = root;		/* for inlined-function dependencies */
	context.active_fns = NIL;	/* nothing being recursively simplified */
	context.case_val = NULL;	/* no CASE being examined */
	context.estimate = false;	/* safe transformations only */
	context.recurse_queries = false; /* do not recurse into query structures */
	context.recurse_sublink_testexpr = true;
	context.max_size = 0;
	context.eval_stable_functions = should_eval_stable_functions(root);

	saved_oid_assignments = SaveOidAssignments();
	result = eval_const_expressions_mutator(node, &context);
	RestoreOidAssignments(saved_oid_assignments);

	return result;
}

/*--------------------
 * estimate_expression_value
 *
 * This function attempts to estimate the value of an expression for
 * planning purposes.  It is in essence a more aggressive version of
 * eval_const_expressions(): we will perform constant reductions that are
 * not necessarily 100% safe, but are reasonable for estimation purposes.
 *
 * Currently the extra steps that are taken in this mode are:
 * 1. Substitute values for Params, where a bound Param value has been made
 *	  available by the caller of planner(), even if the Param isn't marked
 *	  constant.  This effectively means that we plan using the first supplied
 *	  value of the Param.
 * 2. Fold stable, as well as immutable, functions to constants.
 * 3. Reduce PlaceHolderVar nodes to their contained expressions.
 *--------------------
 */
Node *
estimate_expression_value(PlannerInfo *root, Node *node)
{
	eval_const_expressions_context context;

	context.boundParams = root->glob->boundParams;	/* bound Params */
	/* we do not need to mark the plan as depending on inlined functions */
	context.root = NULL;
	context.active_fns = NIL;	/* nothing being recursively simplified */
	context.case_val = NULL;	/* no CASE being examined */
	context.estimate = true;	/* unsafe transformations OK */
	context.recurse_queries = false; /* do not recurse into query structures */
	context.recurse_sublink_testexpr = true;
	context.max_size = 0;
	context.eval_stable_functions = false;

	return eval_const_expressions_mutator(node, &context);
}

/*
 * The generic case in eval_const_expressions_mutator is to recurse using
 * expression_tree_mutator, which will copy the given node unchanged but
 * const-simplify its arguments (if any) as far as possible.  If the node
 * itself does immutable processing, and each of its arguments were reduced
 * to a Const, we can then reduce it to a Const using evaluate_expr.  (Some
 * node types need more complicated logic; for example, a CASE expression
 * might be reducible to a constant even if not all its subtrees are.)
 */
#define ece_generic_processing(node) \
	expression_tree_mutator((Node *) (node), eval_const_expressions_mutator, \
							(void *) context)

/*
 * Check whether all arguments of the given node were reduced to Consts.
 * By going directly to expression_tree_walker, contain_non_const_walker
 * is not applied to the node itself, only to its children.
 */
#define ece_all_arguments_const(node) \
	(!expression_tree_walker((Node *) (node), contain_non_const_walker, NULL))

/* Generic macro for applying evaluate_expr */
#define ece_evaluate_expr(node) \
	((Node *) evaluate_expr((Expr *) (node), \
							exprType((Node *) (node)), \
							exprTypmod((Node *) (node)), \
							exprCollation((Node *) (node))))

/*
 * Recursive guts of eval_const_expressions/estimate_expression_value
 */
static Node *
eval_const_expressions_mutator(Node *node,
							   eval_const_expressions_context *context)
{
	if (node == NULL)
		return NULL;
	switch (nodeTag(node))
	{
		case T_Param:
			{
				Param	   *param = (Param *) node;
				ParamListInfo paramLI = context->boundParams;

				/* Look to see if we've been given a value for this Param */
				if (param->paramkind == PARAM_EXTERN &&
					paramLI != NULL &&
					param->paramid > 0 &&
					param->paramid <= paramLI->numParams)
				{
					ParamExternData *prm;
					ParamExternData prmdata;

					/*
					 * Give hook a chance in case parameter is dynamic.  Tell
					 * it that this fetch is speculative, so it should avoid
					 * erroring out if parameter is unavailable.
					 */
					if (paramLI->paramFetch != NULL)
						prm = paramLI->paramFetch(paramLI, param->paramid,
												  true, &prmdata);
					else
						prm = &paramLI->params[param->paramid - 1];

					/*
					 * We don't just check OidIsValid, but insist that the
					 * fetched type match the Param, just in case the hook did
					 * something unexpected.  No need to throw an error here
					 * though; leave that for runtime.
					 */
					if (OidIsValid(prm->ptype) &&
						prm->ptype == param->paramtype)
					{
						/* OK to substitute parameter value? */
						if (context->estimate ||
							(prm->pflags & PARAM_FLAG_CONST))
						{
							/*
							 * Return a Const representing the param value.
							 * Must copy pass-by-ref datatypes, since the
							 * Param might be in a memory context
							 * shorter-lived than our output plan should be.
							 */
							int16		typLen;
							bool		typByVal;
							Datum		pval;

							get_typlenbyval(param->paramtype,
											&typLen, &typByVal);
							if (prm->isnull || typByVal)
								pval = prm->value;
							else
								pval = datumCopy(prm->value, typByVal, typLen);
							return (Node *) makeConst(param->paramtype,
													  param->paramtypmod,
													  param->paramcollid,
													  (int) typLen,
													  pval,
													  prm->isnull,
													  typByVal);
						}
					}
				}

				/*
				 * Not replaceable, so just copy the Param (no need to
				 * recurse)
				 */
				return (Node *) copyObject(param);
			}
		case T_WindowFunc:
			{
				WindowFunc *expr = (WindowFunc *) node;
				Oid			funcid = expr->winfnoid;
				List	   *args;
				Expr	   *aggfilter;
				HeapTuple	func_tuple;
				WindowFunc *newexpr;

				/*
				 * We can't really simplify a WindowFunc node, but we mustn't
				 * just fall through to the default processing, because we
				 * have to apply expand_function_arguments to its argument
				 * list.  That takes care of inserting default arguments and
				 * expanding named-argument notation.
				 */
				func_tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
				if (!HeapTupleIsValid(func_tuple))
					elog(ERROR, "cache lookup failed for function %u", funcid);

				args = expand_function_arguments(expr->args, expr->wintype,
												 func_tuple);

				ReleaseSysCache(func_tuple);

				/* Now, recursively simplify the args (which are a List) */
				args = (List *)
					expression_tree_mutator((Node *) args,
											eval_const_expressions_mutator,
											(void *) context);
				/* ... and the filter expression, which isn't */
				aggfilter = (Expr *)
					eval_const_expressions_mutator((Node *) expr->aggfilter,
												   context);

				/* And build the replacement WindowFunc node */
				newexpr = makeNode(WindowFunc);
				newexpr->winfnoid = expr->winfnoid;
				newexpr->wintype = expr->wintype;
				newexpr->wincollid = expr->wincollid;
				newexpr->inputcollid = expr->inputcollid;
				newexpr->args = args;
				newexpr->aggfilter = aggfilter;
				newexpr->winref = expr->winref;
				newexpr->winstar = expr->winstar;
				newexpr->winagg = expr->winagg;
				newexpr->windistinct = expr->windistinct;
				newexpr->location = expr->location;

				return (Node *) newexpr;
			}
		case T_FuncExpr:
			{
				FuncExpr   *expr = (FuncExpr *) node;
				List	   *args = expr->args;
				Expr	   *simple;
				FuncExpr   *newexpr;

				/*
				 * Code for op/func reduction is pretty bulky, so split it out
				 * as a separate function.  Note: exprTypmod normally returns
				 * -1 for a FuncExpr, but not when the node is recognizably a
				 * length coercion; we want to preserve the typmod in the
				 * eventual Const if so.
				 */
				simple = simplify_function(expr->funcid,
										   expr->funcresulttype,
										   exprTypmod(node),
										   expr->funccollid,
										   expr->inputcollid,
										   &args,
										   expr->funcvariadic,
										   true,
										   true,
										   context);
				if (simple)		/* successfully simplified it */
					return (Node *) simple;

				/*
				 * The expression cannot be simplified any further, so build
				 * and return a replacement FuncExpr node using the
				 * possibly-simplified arguments.  Note that we have also
				 * converted the argument list to positional notation.
				 */
				newexpr = makeNode(FuncExpr);
				newexpr->funcid = expr->funcid;
				newexpr->funcresulttype = expr->funcresulttype;
				newexpr->funcretset = expr->funcretset;
				newexpr->funcvariadic = expr->funcvariadic;
				newexpr->funcformat = expr->funcformat;
				newexpr->funccollid = expr->funccollid;
				newexpr->inputcollid = expr->inputcollid;
				newexpr->args = args;
				newexpr->location = expr->location;
				return (Node *) newexpr;
			}
		case T_OpExpr:
			{
				OpExpr	   *expr = (OpExpr *) node;
				List	   *args = expr->args;
				Expr	   *simple;
				OpExpr	   *newexpr;

				/*
				 * Need to get OID of underlying function.  Okay to scribble
				 * on input to this extent.
				 */
				set_opfuncid(expr);

				/*
				 * Code for op/func reduction is pretty bulky, so split it out
				 * as a separate function.
				 */
				simple = simplify_function(expr->opfuncid,
										   expr->opresulttype, -1,
										   expr->opcollid,
										   expr->inputcollid,
										   &args,
										   false,
										   true,
										   true,
										   context);
				if (simple)		/* successfully simplified it */
					return (Node *) simple;

				/*
				 * If the operator is boolean equality or inequality, we know
				 * how to simplify cases involving one constant and one
				 * non-constant argument.
				 */
				if (expr->opno == BooleanEqualOperator ||
					expr->opno == BooleanNotEqualOperator)
				{
					simple = (Expr *) simplify_boolean_equality(expr->opno,
																args);
					if (simple) /* successfully simplified it */
						return (Node *) simple;
				}

				/*
				 * The expression cannot be simplified any further, so build
				 * and return a replacement OpExpr node using the
				 * possibly-simplified arguments.
				 */
				newexpr = makeNode(OpExpr);
				newexpr->opno = expr->opno;
				newexpr->opfuncid = expr->opfuncid;
				newexpr->opresulttype = expr->opresulttype;
				newexpr->opretset = expr->opretset;
				newexpr->opcollid = expr->opcollid;
				newexpr->inputcollid = expr->inputcollid;
				newexpr->args = args;
				newexpr->location = expr->location;
				return (Node *) newexpr;
			}
		case T_DistinctExpr:
			{
				DistinctExpr *expr = (DistinctExpr *) node;
				List	   *args;
				ListCell   *arg;
				bool		has_null_input = false;
				bool		all_null_input = true;
				bool		has_nonconst_input = false;
				Expr	   *simple;
				DistinctExpr *newexpr;

				/*
				 * Reduce constants in the DistinctExpr's arguments.  We know
				 * args is either NIL or a List node, so we can call
				 * expression_tree_mutator directly rather than recursing to
				 * self.
				 */
				args = (List *) expression_tree_mutator((Node *) expr->args,
														eval_const_expressions_mutator,
														(void *) context);

				/*
				 * We must do our own check for NULLs because DistinctExpr has
				 * different results for NULL input than the underlying
				 * operator does.
				 */
				foreach(arg, args)
				{
					if (IsA(lfirst(arg), Const))
					{
						has_null_input |= ((Const *) lfirst(arg))->constisnull;
						all_null_input &= ((Const *) lfirst(arg))->constisnull;
					}
					else
						has_nonconst_input = true;
				}

				/* all constants? then can optimize this out */
				if (!has_nonconst_input)
				{
					/* all nulls? then not distinct */
					if (all_null_input)
						return makeBoolConst(false, false);

					/* one null? then distinct */
					if (has_null_input)
						return makeBoolConst(true, false);

					/* otherwise try to evaluate the '=' operator */
					/* (NOT okay to try to inline it, though!) */

					/*
					 * Need to get OID of underlying function.  Okay to
					 * scribble on input to this extent.
					 */
					set_opfuncid((OpExpr *) expr);	/* rely on struct
													 * equivalence */

					/*
					 * Code for op/func reduction is pretty bulky, so split it
					 * out as a separate function.
					 */
					simple = simplify_function(expr->opfuncid,
											   expr->opresulttype, -1,
											   expr->opcollid,
											   expr->inputcollid,
											   &args,
											   false,
											   false,
											   false,
											   context);
					if (simple) /* successfully simplified it */
					{
						/*
						 * Since the underlying operator is "=", must negate
						 * its result
						 */
						Const	   *csimple = castNode(Const, simple);

						csimple->constvalue =
							BoolGetDatum(!DatumGetBool(csimple->constvalue));
						return (Node *) csimple;
					}
				}

				/*
				 * The expression cannot be simplified any further, so build
				 * and return a replacement DistinctExpr node using the
				 * possibly-simplified arguments.
				 */
				newexpr = makeNode(DistinctExpr);
				newexpr->opno = expr->opno;
				newexpr->opfuncid = expr->opfuncid;
				newexpr->opresulttype = expr->opresulttype;
				newexpr->opretset = expr->opretset;
				newexpr->opcollid = expr->opcollid;
				newexpr->inputcollid = expr->inputcollid;
				newexpr->args = args;
				newexpr->location = expr->location;
				return (Node *) newexpr;
			}
		case T_ScalarArrayOpExpr:
			{
				ScalarArrayOpExpr *saop;

				/* Copy the node and const-simplify its arguments */
				saop = (ScalarArrayOpExpr *) ece_generic_processing(node);

				/* Make sure we know underlying function */
				set_sa_opfuncid(saop);

				/*
				 * If all arguments are Consts, and it's a safe function, we
				 * can fold to a constant
				 */
				if (ece_all_arguments_const(saop) &&
					ece_function_is_safe(saop->opfuncid, context))
					return ece_evaluate_expr(saop);
				return (Node *) saop;
			}
		case T_BoolExpr:
			{
				BoolExpr   *expr = (BoolExpr *) node;

				switch (expr->boolop)
				{
					case OR_EXPR:
						{
							List	   *newargs;
							bool		haveNull = false;
							bool		forceTrue = false;

							newargs = simplify_or_arguments(expr->args,
															context,
															&haveNull,
															&forceTrue);
							if (forceTrue)
								return makeBoolConst(true, false);
							if (haveNull)
								newargs = lappend(newargs,
												  makeBoolConst(false, true));
							/* If all the inputs are FALSE, result is FALSE */
							if (newargs == NIL)
								return makeBoolConst(false, false);

							/*
							 * If only one nonconst-or-NULL input, it's the
							 * result
							 */
							if (list_length(newargs) == 1)
								return (Node *) linitial(newargs);
							/* Else we still need an OR node */
							return (Node *) make_orclause(newargs);
						}
					case AND_EXPR:
						{
							List	   *newargs;
							bool		haveNull = false;
							bool		forceFalse = false;

							newargs = simplify_and_arguments(expr->args,
															 context,
															 &haveNull,
															 &forceFalse);
							if (forceFalse)
								return makeBoolConst(false, false);
							if (haveNull)
								newargs = lappend(newargs,
												  makeBoolConst(false, true));
							/* If all the inputs are TRUE, result is TRUE */
							if (newargs == NIL)
								return makeBoolConst(true, false);

							/*
							 * If only one nonconst-or-NULL input, it's the
							 * result
							 */
							if (list_length(newargs) == 1)
								return (Node *) linitial(newargs);
							/* Else we still need an AND node */
							return (Node *) make_andclause(newargs);
						}
					case NOT_EXPR:
						{
							Node	   *arg;

							Assert(list_length(expr->args) == 1);
							arg = eval_const_expressions_mutator(linitial(expr->args),
																 context);

							/*
							 * Use negate_clause() to see if we can simplify
							 * away the NOT.
							 */
							return negate_clause(arg);
						}
					default:
						elog(ERROR, "unrecognized boolop: %d",
							 (int) expr->boolop);
						break;
				}
				break;
			}
		case T_SubPlan:
		case T_AlternativeSubPlan:

			/*
			 * Return a SubPlan unchanged --- too late to do anything with it.
			 *
			 * XXX should we ereport() here instead?  Probably this routine
			 * should never be invoked after SubPlan creation.
			 */
			return node;
		case T_RelabelType:
			{
				RelabelType *relabel = (RelabelType *) node;
				Node	   *arg;

				/* Simplify the input ... */
				arg = eval_const_expressions_mutator((Node *) relabel->arg,
													 context);
				/* ... and attach a new RelabelType node, if needed */
				return applyRelabelType(arg,
										relabel->resulttype,
										relabel->resulttypmod,
										relabel->resultcollid,
										relabel->relabelformat,
										relabel->location,
										true);
			}
		case T_CoerceViaIO:
			{
				CoerceViaIO *expr = (CoerceViaIO *) node;
				List	   *args;
				Oid			outfunc;
				bool		outtypisvarlena;
				Oid			infunc;
				Oid			intypioparam;
				Expr	   *simple;
				CoerceViaIO *newexpr;

				/* Make a List so we can use simplify_function */
				args = list_make1(expr->arg);

				/*
				 * CoerceViaIO represents calling the source type's output
				 * function then the result type's input function.  So, try to
				 * simplify it as though it were a stack of two such function
				 * calls.  First we need to know what the functions are.
				 *
				 * Note that the coercion functions are assumed not to care
				 * about input collation, so we just pass InvalidOid for that.
				 */
				getTypeOutputInfo(exprType((Node *) expr->arg),
								  &outfunc, &outtypisvarlena);
				getTypeInputInfo(expr->resulttype,
								 &infunc, &intypioparam);

				simple = simplify_function(outfunc,
										   CSTRINGOID, -1,
										   InvalidOid,
										   InvalidOid,
										   &args,
										   false,
										   true,
										   true,
										   context);
				if (simple)		/* successfully simplified output fn */
				{
					/*
					 * Input functions may want 1 to 3 arguments.  We always
					 * supply all three, trusting that nothing downstream will
					 * complain.
					 */
					args = list_make3(simple,
									  makeConst(OIDOID,
												-1,
												InvalidOid,
												sizeof(Oid),
												ObjectIdGetDatum(intypioparam),
												false,
												true),
									  makeConst(INT4OID,
												-1,
												InvalidOid,
												sizeof(int32),
												Int32GetDatum(-1),
												false,
												true));

					simple = simplify_function(infunc,
											   expr->resulttype, -1,
											   expr->resultcollid,
											   InvalidOid,
											   &args,
											   false,
											   false,
											   true,
											   context);
					if (simple) /* successfully simplified input fn */
						return (Node *) simple;
				}

				/*
				 * The expression cannot be simplified any further, so build
				 * and return a replacement CoerceViaIO node using the
				 * possibly-simplified argument.
				 */
				newexpr = makeNode(CoerceViaIO);
				newexpr->arg = (Expr *) linitial(args);
				newexpr->resulttype = expr->resulttype;
				newexpr->resultcollid = expr->resultcollid;
				newexpr->coerceformat = expr->coerceformat;
				newexpr->location = expr->location;
				return (Node *) newexpr;
			}
		case T_ArrayCoerceExpr:
			{
				ArrayCoerceExpr *ac = makeNode(ArrayCoerceExpr);
				Node	   *save_case_val;

				/*
				 * Copy the node and const-simplify its arguments.  We can't
				 * use ece_generic_processing() here because we need to mess
				 * with case_val only while processing the elemexpr.
				 */
				memcpy(ac, node, sizeof(ArrayCoerceExpr));
				ac->arg = (Expr *)
					eval_const_expressions_mutator((Node *) ac->arg,
												   context);

				/*
				 * Set up for the CaseTestExpr node contained in the elemexpr.
				 * We must prevent it from absorbing any outer CASE value.
				 */
				save_case_val = context->case_val;
				context->case_val = NULL;

				ac->elemexpr = (Expr *)
					eval_const_expressions_mutator((Node *) ac->elemexpr,
												   context);

				context->case_val = save_case_val;

				/*
				 * If constant argument and the per-element expression is
				 * immutable, we can simplify the whole thing to a constant.
				 * Exception: although contain_mutable_functions considers
				 * CoerceToDomain immutable for historical reasons, let's not
				 * do so here; this ensures coercion to an array-over-domain
				 * does not apply the domain's constraints until runtime.
				 */
				if (ac->arg && IsA(ac->arg, Const) &&
					ac->elemexpr && !IsA(ac->elemexpr, CoerceToDomain) &&
					!contain_mutable_functions((Node *) ac->elemexpr))
					return ece_evaluate_expr(ac);

				return (Node *) ac;
			}
		case T_CollateExpr:
			{
				/*
				 * We replace CollateExpr with RelabelType, so as to improve
				 * uniformity of expression representation and thus simplify
				 * comparison of expressions.  Hence this looks very nearly
				 * the same as the RelabelType case, and we can apply the same
				 * optimizations to avoid unnecessary RelabelTypes.
				 */
				CollateExpr *collate = (CollateExpr *) node;
				Node	   *arg;

				/* Simplify the input ... */
				arg = eval_const_expressions_mutator((Node *) collate->arg,
													 context);
				/* ... and attach a new RelabelType node, if needed */
				return applyRelabelType(arg,
										exprType(arg),
										exprTypmod(arg),
										collate->collOid,
										COERCE_IMPLICIT_CAST,
										collate->location,
										true);
			}
		case T_CaseExpr:
			{
				/*----------
				 * CASE expressions can be simplified if there are constant
				 * condition clauses:
				 *		FALSE (or NULL): drop the alternative
				 *		TRUE: drop all remaining alternatives
				 * If the first non-FALSE alternative is a constant TRUE,
				 * we can simplify the entire CASE to that alternative's
				 * expression.  If there are no non-FALSE alternatives,
				 * we simplify the entire CASE to the default result (ELSE).
				 *
				 * If we have a simple-form CASE with constant test
				 * expression, we substitute the constant value for contained
				 * CaseTestExpr placeholder nodes, so that we have the
				 * opportunity to reduce constant test conditions.  For
				 * example this allows
				 *		CASE 0 WHEN 0 THEN 1 ELSE 1/0 END
				 * to reduce to 1 rather than drawing a divide-by-0 error.
				 * Note that when the test expression is constant, we don't
				 * have to include it in the resulting CASE; for example
				 *		CASE 0 WHEN x THEN y ELSE z END
				 * is transformed by the parser to
				 *		CASE 0 WHEN CaseTestExpr = x THEN y ELSE z END
				 * which we can simplify to
				 *		CASE WHEN 0 = x THEN y ELSE z END
				 * It is not necessary for the executor to evaluate the "arg"
				 * expression when executing the CASE, since any contained
				 * CaseTestExprs that might have referred to it will have been
				 * replaced by the constant.
				 *----------
				 */
				CaseExpr   *caseexpr = (CaseExpr *) node;
				CaseExpr   *newcase;
				Node	   *save_case_val;
				Node	   *newarg;
				List	   *newargs;
				bool		const_true_cond;
				Node	   *defresult = NULL;
				ListCell   *arg;

				/* Simplify the test expression, if any */
				newarg = eval_const_expressions_mutator((Node *) caseexpr->arg,
														context);

				/* Set up for contained CaseTestExpr nodes */
				save_case_val = context->case_val;
				if (newarg && IsA(newarg, Const))
				{
					context->case_val = newarg;
					newarg = NULL;	/* not needed anymore, see above */
				}
				else
					context->case_val = NULL;

				/* Simplify the WHEN clauses */
				newargs = NIL;
				const_true_cond = false;
				foreach(arg, caseexpr->args)
				{
					CaseWhen   *oldcasewhen = lfirst_node(CaseWhen, arg);
					Node	   *casecond;
					Node	   *caseresult;

					/* Simplify this alternative's test condition */
					casecond = eval_const_expressions_mutator((Node *) oldcasewhen->expr,
															  context);

					/*
					 * If the test condition is constant FALSE (or NULL), then
					 * drop this WHEN clause completely, without processing
					 * the result.
					 */
					if (casecond && IsA(casecond, Const))
					{
						Const	   *const_input = (Const *) casecond;

						if (const_input->constisnull ||
							!DatumGetBool(const_input->constvalue))
							continue;	/* drop alternative with FALSE cond */
						/* Else it's constant TRUE */
						const_true_cond = true;
					}

					/* Simplify this alternative's result value */
					caseresult = eval_const_expressions_mutator((Node *) oldcasewhen->result,
																context);

					/* If non-constant test condition, emit a new WHEN node */
					if (!const_true_cond)
					{
						CaseWhen   *newcasewhen = makeNode(CaseWhen);

						newcasewhen->expr = (Expr *) casecond;
						newcasewhen->result = (Expr *) caseresult;
						newcasewhen->location = oldcasewhen->location;
						newargs = lappend(newargs, newcasewhen);
						continue;
					}

					/*
					 * Found a TRUE condition, so none of the remaining
					 * alternatives can be reached.  We treat the result as
					 * the default result.
					 */
					defresult = caseresult;
					break;
				}

				/* Simplify the default result, unless we replaced it above */
				if (!const_true_cond)
					defresult = eval_const_expressions_mutator((Node *) caseexpr->defresult,
															   context);

				context->case_val = save_case_val;

				/*
				 * If no non-FALSE alternatives, CASE reduces to the default
				 * result
				 */
				if (newargs == NIL)
					return defresult;
				/* Otherwise we need a new CASE node */
				newcase = makeNode(CaseExpr);
				newcase->casetype = caseexpr->casetype;
				newcase->casecollid = caseexpr->casecollid;
				newcase->arg = (Expr *) newarg;
				newcase->args = newargs;
				newcase->defresult = (Expr *) defresult;
				newcase->location = caseexpr->location;
				return (Node *) newcase;
			}
		case T_CaseTestExpr:
			{
				/*
				 * If we know a constant test value for the current CASE
				 * construct, substitute it for the placeholder.  Else just
				 * return the placeholder as-is.
				 */
				if (context->case_val)
					return copyObject(context->case_val);
				else
					return copyObject(node);
			}
		case T_SubscriptingRef:
		case T_ArrayExpr:
		case T_RowExpr:
		case T_MinMaxExpr:
			{
				/*
				 * Generic handling for node types whose own processing is
				 * known to be immutable, and for which we need no smarts
				 * beyond "simplify if all inputs are constants".
				 *
				 * Treating MinMaxExpr this way amounts to assuming that the
				 * btree comparison function it calls is immutable; see the
				 * reasoning in contain_mutable_functions_walker.
				 */

				/* Copy the node and const-simplify its arguments */
				node = ece_generic_processing(node);

				/* If all arguments are Consts, we can fold to a constant */
				/* In gpdb, RowExpr's TupleDesc will lost in QE if we evaluate
				 * expr in planner. It is hard to dispatch these TupleDesc to QE
				 * since it affect typecache more complex.
				 */
				if (ece_all_arguments_const(node) &&
					(!IsA(node, RowExpr) || ((RowExpr *) node)->row_typeid != RECORDOID))
					return ece_evaluate_expr(node);
				return node;
			}
		case T_CoalesceExpr:
			{
				CoalesceExpr *coalesceexpr = (CoalesceExpr *) node;
				CoalesceExpr *newcoalesce;
				List	   *newargs;
				ListCell   *arg;

				newargs = NIL;
				foreach(arg, coalesceexpr->args)
				{
					Node	   *e;

					e = eval_const_expressions_mutator((Node *) lfirst(arg),
													   context);

					/*
					 * We can remove null constants from the list. For a
					 * non-null constant, if it has not been preceded by any
					 * other non-null-constant expressions then it is the
					 * result. Otherwise, it's the next argument, but we can
					 * drop following arguments since they will never be
					 * reached.
					 */
					if (IsA(e, Const))
					{
						if (((Const *) e)->constisnull)
							continue;	/* drop null constant */
						if (newargs == NIL)
							return e;	/* first expr */
						newargs = lappend(newargs, e);
						break;
					}
					newargs = lappend(newargs, e);
				}

				/*
				 * If all the arguments were constant null, the result is just
				 * null
				 */
				if (newargs == NIL)
					return (Node *) makeNullConst(coalesceexpr->coalescetype,
												  -1,
												  coalesceexpr->coalescecollid);

				newcoalesce = makeNode(CoalesceExpr);
				newcoalesce->coalescetype = coalesceexpr->coalescetype;
				newcoalesce->coalescecollid = coalesceexpr->coalescecollid;
				newcoalesce->args = newargs;
				newcoalesce->location = coalesceexpr->location;
				return (Node *) newcoalesce;
			}
		case T_SQLValueFunction:
			{
				/*
				 * All variants of SQLValueFunction are stable, so if we are
				 * estimating the expression's value, we should evaluate the
				 * current function value.
				 *
				 * In GPDB, we add eval_stable_functions field in the context
				 * to decide whether we should pre-evaluate this stable function.
				 * If it is true, we evaluate the function value here so that we
				 * can directly dispatch a single row insertion query that contains
				 * SQLValueFunction (Otherwise we need to add a redistribution
				 * motion). In a specific case where we use prepare/execute statement,
				 * we need to set oneoffPlan to true so that we can re-evaluate 
				 * the SQLValueFunction in the execute statement.
				 *
				 * If neither condition holds, we just copy.
				 */
				SQLValueFunction *svf = (SQLValueFunction *) node;

				if (context->eval_stable_functions)
				{
					context->root->glob->oneoffPlan = true;
					return (Node *) evaluate_expr((Expr *) svf,
												  svf->type,
												  svf->typmod,
												  InvalidOid);
				}

				if (context->estimate)
				{
					return (Node *) evaluate_expr((Expr *) svf,
												  svf->type,
												  svf->typmod,
												  InvalidOid);
				}

				return copyObject((Node *) svf);
			}
		case T_FieldSelect:
			{
				/*
				 * We can optimize field selection from a whole-row Var into a
				 * simple Var.  (This case won't be generated directly by the
				 * parser, because ParseComplexProjection short-circuits it.
				 * But it can arise while simplifying functions.)  Also, we
				 * can optimize field selection from a RowExpr construct, or
				 * of course from a constant.
				 *
				 * However, replacing a whole-row Var in this way has a
				 * pitfall: if we've already built the rel targetlist for the
				 * source relation, then the whole-row Var is scheduled to be
				 * produced by the relation scan, but the simple Var probably
				 * isn't, which will lead to a failure in setrefs.c.  This is
				 * not a problem when handling simple single-level queries, in
				 * which expression simplification always happens first.  It
				 * is a risk for lateral references from subqueries, though.
				 * To avoid such failures, don't optimize uplevel references.
				 *
				 * We must also check that the declared type of the field is
				 * still the same as when the FieldSelect was created --- this
				 * can change if someone did ALTER COLUMN TYPE on the rowtype.
				 * If it isn't, we skip the optimization; the case will
				 * probably fail at runtime, but that's not our problem here.
				 */
				FieldSelect *fselect = (FieldSelect *) node;
				FieldSelect *newfselect;
				Node	   *arg;

				arg = eval_const_expressions_mutator((Node *) fselect->arg,
													 context);
				if (arg && IsA(arg, Var) &&
					((Var *) arg)->varattno == InvalidAttrNumber &&
					((Var *) arg)->varlevelsup == 0)
				{
					if (rowtype_field_matches(((Var *) arg)->vartype,
											  fselect->fieldnum,
											  fselect->resulttype,
											  fselect->resulttypmod,
											  fselect->resultcollid))
						return (Node *) makeVar(((Var *) arg)->varno,
												fselect->fieldnum,
												fselect->resulttype,
												fselect->resulttypmod,
												fselect->resultcollid,
												((Var *) arg)->varlevelsup);
				}
				if (arg && IsA(arg, RowExpr))
				{
					RowExpr    *rowexpr = (RowExpr *) arg;

					if (fselect->fieldnum > 0 &&
						fselect->fieldnum <= list_length(rowexpr->args))
					{
						Node	   *fld = (Node *) list_nth(rowexpr->args,
															fselect->fieldnum - 1);

						if (rowtype_field_matches(rowexpr->row_typeid,
												  fselect->fieldnum,
												  fselect->resulttype,
												  fselect->resulttypmod,
												  fselect->resultcollid) &&
							fselect->resulttype == exprType(fld) &&
							fselect->resulttypmod == exprTypmod(fld) &&
							fselect->resultcollid == exprCollation(fld))
							return fld;
					}
				}
				newfselect = makeNode(FieldSelect);
				newfselect->arg = (Expr *) arg;
				newfselect->fieldnum = fselect->fieldnum;
				newfselect->resulttype = fselect->resulttype;
				newfselect->resulttypmod = fselect->resulttypmod;
				newfselect->resultcollid = fselect->resultcollid;
				if (arg && IsA(arg, Const))
				{
					Const	   *con = (Const *) arg;

					if (rowtype_field_matches(con->consttype,
											  newfselect->fieldnum,
											  newfselect->resulttype,
											  newfselect->resulttypmod,
											  newfselect->resultcollid))
						return ece_evaluate_expr(newfselect);
				}
				return (Node *) newfselect;
			}
		case T_NullTest:
			{
				NullTest   *ntest = (NullTest *) node;
				NullTest   *newntest;
				Node	   *arg;

				arg = eval_const_expressions_mutator((Node *) ntest->arg,
													 context);
				if (ntest->argisrow && arg && IsA(arg, RowExpr))
				{
					/*
					 * We break ROW(...) IS [NOT] NULL into separate tests on
					 * its component fields.  This form is usually more
					 * efficient to evaluate, as well as being more amenable
					 * to optimization.
					 */
					RowExpr    *rarg = (RowExpr *) arg;
					List	   *newargs = NIL;
					ListCell   *l;

					foreach(l, rarg->args)
					{
						Node	   *relem = (Node *) lfirst(l);

						/*
						 * A constant field refutes the whole NullTest if it's
						 * of the wrong nullness; else we can discard it.
						 */
						if (relem && IsA(relem, Const))
						{
							Const	   *carg = (Const *) relem;

							if (carg->constisnull ?
								(ntest->nulltesttype == IS_NOT_NULL) :
								(ntest->nulltesttype == IS_NULL))
								return makeBoolConst(false, false);
							continue;
						}

						/*
						 * Else, make a scalar (argisrow == false) NullTest
						 * for this field.  Scalar semantics are required
						 * because IS [NOT] NULL doesn't recurse; see comments
						 * in ExecEvalRowNullInt().
						 */
						newntest = makeNode(NullTest);
						newntest->arg = (Expr *) relem;
						newntest->nulltesttype = ntest->nulltesttype;
						newntest->argisrow = false;
						newntest->location = ntest->location;
						newargs = lappend(newargs, newntest);
					}
					/* If all the inputs were constants, result is TRUE */
					if (newargs == NIL)
						return makeBoolConst(true, false);
					/* If only one nonconst input, it's the result */
					if (list_length(newargs) == 1)
						return (Node *) linitial(newargs);
					/* Else we need an AND node */
					return (Node *) make_andclause(newargs);
				}
				if (!ntest->argisrow && arg && IsA(arg, Const))
				{
					Const	   *carg = (Const *) arg;
					bool		result;

					switch (ntest->nulltesttype)
					{
						case IS_NULL:
							result = carg->constisnull;
							break;
						case IS_NOT_NULL:
							result = !carg->constisnull;
							break;
						default:
							elog(ERROR, "unrecognized nulltesttype: %d",
								 (int) ntest->nulltesttype);
							result = false; /* keep compiler quiet */
							break;
					}

					return makeBoolConst(result, false);
				}

				newntest = makeNode(NullTest);
				newntest->arg = (Expr *) arg;
				newntest->nulltesttype = ntest->nulltesttype;
				newntest->argisrow = ntest->argisrow;
				newntest->location = ntest->location;
				return (Node *) newntest;
			}
		case T_BooleanTest:
			{
				/*
				 * This case could be folded into the generic handling used
				 * for SubscriptingRef etc.  But because the simplification
				 * logic is so trivial, applying evaluate_expr() to perform it
				 * would be a heavy overhead.  BooleanTest is probably common
				 * enough to justify keeping this bespoke implementation.
				 */
				BooleanTest *btest = (BooleanTest *) node;
				BooleanTest *newbtest;
				Node	   *arg;

				arg = eval_const_expressions_mutator((Node *) btest->arg,
													 context);
				if (arg && IsA(arg, Const))
				{
					Const	   *carg = (Const *) arg;
					bool		result;

					switch (btest->booltesttype)
					{
						case IS_TRUE:
							result = (!carg->constisnull &&
									  DatumGetBool(carg->constvalue));
							break;
						case IS_NOT_TRUE:
							result = (carg->constisnull ||
									  !DatumGetBool(carg->constvalue));
							break;
						case IS_FALSE:
							result = (!carg->constisnull &&
									  !DatumGetBool(carg->constvalue));
							break;
						case IS_NOT_FALSE:
							result = (carg->constisnull ||
									  DatumGetBool(carg->constvalue));
							break;
						case IS_UNKNOWN:
							result = carg->constisnull;
							break;
						case IS_NOT_UNKNOWN:
							result = !carg->constisnull;
							break;
						default:
							elog(ERROR, "unrecognized booltesttype: %d",
								 (int) btest->booltesttype);
							result = false; /* keep compiler quiet */
							break;
					}

					return makeBoolConst(result, false);
				}

				newbtest = makeNode(BooleanTest);
				newbtest->arg = (Expr *) arg;
				newbtest->booltesttype = btest->booltesttype;
				newbtest->location = btest->location;
				return (Node *) newbtest;
			}
		case T_CoerceToDomain:
			{
				/*
				 * If the domain currently has no constraints, we replace the
				 * CoerceToDomain node with a simple RelabelType, which is
				 * both far faster to execute and more amenable to later
				 * optimization.  We must then mark the plan as needing to be
				 * rebuilt if the domain's constraints change.
				 *
				 * Also, in estimation mode, always replace CoerceToDomain
				 * nodes, effectively assuming that the coercion will succeed.
				 */
				CoerceToDomain *cdomain = (CoerceToDomain *) node;
				CoerceToDomain *newcdomain;
				Node	   *arg;

				arg = eval_const_expressions_mutator((Node *) cdomain->arg,
													 context);
				if (context->estimate ||
					!DomainHasConstraints(cdomain->resulttype))
				{
					/* Record dependency, if this isn't estimation mode */
					if (context->root && !context->estimate)
						record_plan_type_dependency(context->root,
													cdomain->resulttype);

					/* Generate RelabelType to substitute for CoerceToDomain */
					return applyRelabelType(arg,
											cdomain->resulttype,
											cdomain->resulttypmod,
											cdomain->resultcollid,
											cdomain->coercionformat,
											cdomain->location,
											true);
				}

				newcdomain = makeNode(CoerceToDomain);
				newcdomain->arg = (Expr *) arg;
				newcdomain->resulttype = cdomain->resulttype;
				newcdomain->resulttypmod = cdomain->resulttypmod;
				newcdomain->resultcollid = cdomain->resultcollid;
				newcdomain->coercionformat = cdomain->coercionformat;
				newcdomain->location = cdomain->location;
				return (Node *) newcdomain;
			}
		case T_PlaceHolderVar:

			/*
			 * In estimation mode, just strip the PlaceHolderVar node
			 * altogether; this amounts to estimating that the contained value
			 * won't be forced to null by an outer join.  In regular mode we
			 * just use the default behavior (ie, simplify the expression but
			 * leave the PlaceHolderVar node intact).
			 */
			if (context->estimate)
			{
				PlaceHolderVar *phv = (PlaceHolderVar *) node;

				return eval_const_expressions_mutator((Node *) phv->phexpr,
													  context);
			}
			break;
		case T_ConvertRowtypeExpr:
			{
				ConvertRowtypeExpr *cre = castNode(ConvertRowtypeExpr, node);
				Node	   *arg;
				ConvertRowtypeExpr *newcre;

				arg = eval_const_expressions_mutator((Node *) cre->arg,
													 context);

				newcre = makeNode(ConvertRowtypeExpr);
				newcre->resulttype = cre->resulttype;
				newcre->convertformat = cre->convertformat;
				newcre->location = cre->location;

				/*
				 * In case of a nested ConvertRowtypeExpr, we can convert the
				 * leaf row directly to the topmost row format without any
				 * intermediate conversions. (This works because
				 * ConvertRowtypeExpr is used only for child->parent
				 * conversion in inheritance trees, which works by exact match
				 * of column name, and a column absent in an intermediate
				 * result can't be present in the final result.)
				 *
				 * No need to check more than one level deep, because the
				 * above recursion will have flattened anything else.
				 */
				if (arg != NULL && IsA(arg, ConvertRowtypeExpr))
				{
					ConvertRowtypeExpr *argcre = (ConvertRowtypeExpr *) arg;

					arg = (Node *) argcre->arg;

					/*
					 * Make sure an outer implicit conversion can't hide an
					 * inner explicit one.
					 */
					if (newcre->convertformat == COERCE_IMPLICIT_CAST)
						newcre->convertformat = argcre->convertformat;
				}

				newcre->arg = (Expr *) arg;

				if (arg != NULL && IsA(arg, Const))
					return ece_evaluate_expr((Node *) newcre);
				return (Node *) newcre;
			}
		default:
			break;
	}

	/* prevent recursion into sublinks */
	if (IsA(node, SubLink) && !context->recurse_sublink_testexpr)
	{
		SubLink    *sublink = (SubLink *) node;
		SubLink    *newnode = copyObject(sublink);

		/*
		 * Also invoke the mutator on the sublink's Query node, so it
		 * can recurse into the sub-query if it wants to.
		 */
		newnode->subselect = (Node *) query_tree_mutator((Query *) sublink->subselect, eval_const_expressions_mutator, (void*) context, 0);
		return (Node *) newnode;
	}

	/* recurse into query structure if requested */
	if (IsA(node, Query) && context->recurse_queries)
	{
		return (Node *)
					query_tree_mutator
					(
					(Query *) node,
					eval_const_expressions_mutator,
					(void *) context,
					0);
	}

	/*
	 * For any node type not handled above, copy the node unchanged but
	 * const-simplify its subexpressions.  This is the correct thing for node
	 * types whose behavior might change between planning and execution, such
	 * as CurrentOfExpr.  It's also a safe default for new node types not
	 * known to this routine.
	 */
	return ece_generic_processing(node);
}

/*
 * Subroutine for eval_const_expressions: check for non-Const nodes.
 *
 * We can abort recursion immediately on finding a non-Const node.  This is
 * critical for performance, else eval_const_expressions_mutator would take
 * O(N^2) time on non-simplifiable trees.  However, we do need to descend
 * into List nodes since expression_tree_walker sometimes invokes the walker
 * function directly on List subtrees.
 */
static bool
contain_non_const_walker(Node *node, void *context)
{
	if (node == NULL)
		return false;
	if (IsA(node, Const))
		return false;
	if (IsA(node, List))
		return expression_tree_walker(node, contain_non_const_walker, context);
	/* Otherwise, abort the tree traversal and return true */
	return true;
}

/*
 * Subroutine for eval_const_expressions: check if a function is OK to evaluate
 */
static bool
ece_function_is_safe(Oid funcid, eval_const_expressions_context *context)
{
	char		provolatile = func_volatile(funcid);

	/*
	 * Ordinarily we are only allowed to simplify immutable functions. But for
	 * purposes of estimation, we consider it okay to simplify functions that
	 * are merely stable; the risk that the result might change from planning
	 * time to execution time is worth taking in preference to not being able
	 * to estimate the value at all.
	 */
	if (provolatile == PROVOLATILE_IMMUTABLE)
		return true;
	if (context->estimate && provolatile == PROVOLATILE_STABLE)
		return true;
	return false;
}

/*
 * Subroutine for eval_const_expressions: process arguments of an OR clause
 *
 * This includes flattening of nested ORs as well as recursion to
 * eval_const_expressions to simplify the OR arguments.
 *
 * After simplification, OR arguments are handled as follows:
 *		non constant: keep
 *		FALSE: drop (does not affect result)
 *		TRUE: force result to TRUE
 *		NULL: keep only one
 * We must keep one NULL input because OR expressions evaluate to NULL when no
 * input is TRUE and at least one is NULL.  We don't actually include the NULL
 * here, that's supposed to be done by the caller.
 *
 * The output arguments *haveNull and *forceTrue must be initialized false
 * by the caller.  They will be set true if a NULL constant or TRUE constant,
 * respectively, is detected anywhere in the argument list.
 */
static List *
simplify_or_arguments(List *args,
					  eval_const_expressions_context *context,
					  bool *haveNull, bool *forceTrue)
{
	List	   *newargs = NIL;
	List	   *unprocessed_args;

	/*
	 * We want to ensure that any OR immediately beneath another OR gets
	 * flattened into a single OR-list, so as to simplify later reasoning.
	 *
	 * To avoid stack overflow from recursion of eval_const_expressions, we
	 * resort to some tenseness here: we keep a list of not-yet-processed
	 * inputs, and handle flattening of nested ORs by prepending to the to-do
	 * list instead of recursing.  Now that the parser generates N-argument
	 * ORs from simple lists, this complexity is probably less necessary than
	 * it once was, but we might as well keep the logic.
	 */
	unprocessed_args = list_copy(args);
	while (unprocessed_args)
	{
		Node	   *arg = (Node *) linitial(unprocessed_args);

		unprocessed_args = list_delete_first(unprocessed_args);

		/* flatten nested ORs as per above comment */
		if (is_orclause(arg))
		{
			List	   *subargs = list_copy(((BoolExpr *) arg)->args);

			/* overly tense code to avoid leaking unused list header */
			if (!unprocessed_args)
				unprocessed_args = subargs;
			else
				unprocessed_args = list_concat(subargs, unprocessed_args);
			continue;
		}

		/* If it's not an OR, simplify it */
		arg = eval_const_expressions_mutator(arg, context);

		/*
		 * It is unlikely but not impossible for simplification of a non-OR
		 * clause to produce an OR.  Recheck, but don't be too tense about it
		 * since it's not a mainstream case. In particular we don't worry
		 * about const-simplifying the input twice.
		 */
		if (is_orclause(arg))
		{
			List	   *subargs = list_copy(((BoolExpr *) arg)->args);

			unprocessed_args = list_concat(subargs, unprocessed_args);
			continue;
		}

		/*
		 * OK, we have a const-simplified non-OR argument.  Process it per
		 * comments above.
		 */
		if (IsA(arg, Const))
		{
			Const	   *const_input = (Const *) arg;

			if (const_input->constisnull)
				*haveNull = true;
			else if (DatumGetBool(const_input->constvalue))
			{
				*forceTrue = true;

				/*
				 * Once we detect a TRUE result we can just exit the loop
				 * immediately.  However, if we ever add a notion of
				 * non-removable functions, we'd need to keep scanning.
				 */
				return NIL;
			}
			/* otherwise, we can drop the constant-false input */
			continue;
		}

		/* else emit the simplified arg into the result list */
		newargs = lappend(newargs, arg);
	}

	return newargs;
}

/*
 * Subroutine for eval_const_expressions: process arguments of an AND clause
 *
 * This includes flattening of nested ANDs as well as recursion to
 * eval_const_expressions to simplify the AND arguments.
 *
 * After simplification, AND arguments are handled as follows:
 *		non constant: keep
 *		TRUE: drop (does not affect result)
 *		FALSE: force result to FALSE
 *		NULL: keep only one
 * We must keep one NULL input because AND expressions evaluate to NULL when
 * no input is FALSE and at least one is NULL.  We don't actually include the
 * NULL here, that's supposed to be done by the caller.
 *
 * The output arguments *haveNull and *forceFalse must be initialized false
 * by the caller.  They will be set true if a null constant or false constant,
 * respectively, is detected anywhere in the argument list.
 */
static List *
simplify_and_arguments(List *args,
					   eval_const_expressions_context *context,
					   bool *haveNull, bool *forceFalse)
{
	List	   *newargs = NIL;
	List	   *unprocessed_args;

	/* See comments in simplify_or_arguments */
	unprocessed_args = list_copy(args);
	while (unprocessed_args)
	{
		Node	   *arg = (Node *) linitial(unprocessed_args);

		unprocessed_args = list_delete_first(unprocessed_args);

		/* flatten nested ANDs as per above comment */
		if (is_andclause(arg))
		{
			List	   *subargs = list_copy(((BoolExpr *) arg)->args);

			/* overly tense code to avoid leaking unused list header */
			if (!unprocessed_args)
				unprocessed_args = subargs;
			else
				unprocessed_args = list_concat(subargs, unprocessed_args);
			continue;
		}

		/* If it's not an AND, simplify it */
		arg = eval_const_expressions_mutator(arg, context);

		/*
		 * It is unlikely but not impossible for simplification of a non-AND
		 * clause to produce an AND.  Recheck, but don't be too tense about it
		 * since it's not a mainstream case. In particular we don't worry
		 * about const-simplifying the input twice.
		 */
		if (is_andclause(arg))
		{
			List	   *subargs = list_copy(((BoolExpr *) arg)->args);

			unprocessed_args = list_concat(subargs, unprocessed_args);
			continue;
		}

		/*
		 * OK, we have a const-simplified non-AND argument.  Process it per
		 * comments above.
		 */
		if (IsA(arg, Const))
		{
			Const	   *const_input = (Const *) arg;

			if (const_input->constisnull)
				*haveNull = true;
			else if (!DatumGetBool(const_input->constvalue))
			{
				*forceFalse = true;

				/*
				 * Once we detect a FALSE result we can just exit the loop
				 * immediately.  However, if we ever add a notion of
				 * non-removable functions, we'd need to keep scanning.
				 */
				return NIL;
			}
			/* otherwise, we can drop the constant-true input */
			continue;
		}

		/* else emit the simplified arg into the result list */
		newargs = lappend(newargs, arg);
	}

	return newargs;
}

/*
 * Subroutine for eval_const_expressions: try to simplify boolean equality
 * or inequality condition
 *
 * Inputs are the operator OID and the simplified arguments to the operator.
 * Returns a simplified expression if successful, or NULL if cannot
 * simplify the expression.
 *
 * The idea here is to reduce "x = true" to "x" and "x = false" to "NOT x",
 * or similarly "x <> true" to "NOT x" and "x <> false" to "x".
 * This is only marginally useful in itself, but doing it in constant folding
 * ensures that we will recognize these forms as being equivalent in, for
 * example, partial index matching.
 *
 * We come here only if simplify_function has failed; therefore we cannot
 * see two constant inputs, nor a constant-NULL input.
 */
static Node *
simplify_boolean_equality(Oid opno, List *args)
{
	Node	   *leftop;
	Node	   *rightop;

	Assert(list_length(args) == 2);
	leftop = linitial(args);
	rightop = lsecond(args);
	if (leftop && IsA(leftop, Const))
	{
		Assert(!((Const *) leftop)->constisnull);
		if (opno == BooleanEqualOperator)
		{
			if (DatumGetBool(((Const *) leftop)->constvalue))
				return rightop; /* true = foo */
			else
				return negate_clause(rightop);	/* false = foo */
		}
		else
		{
			if (DatumGetBool(((Const *) leftop)->constvalue))
				return negate_clause(rightop);	/* true <> foo */
			else
				return rightop; /* false <> foo */
		}
	}
	if (rightop && IsA(rightop, Const))
	{
		Assert(!((Const *) rightop)->constisnull);
		if (opno == BooleanEqualOperator)
		{
			if (DatumGetBool(((Const *) rightop)->constvalue))
				return leftop;	/* foo = true */
			else
				return negate_clause(leftop);	/* foo = false */
		}
		else
		{
			if (DatumGetBool(((Const *) rightop)->constvalue))
				return negate_clause(leftop);	/* foo <> true */
			else
				return leftop;	/* foo <> false */
		}
	}
	return NULL;
}

/*
 * Subroutine for eval_const_expressions: try to simplify a function call
 * (which might originally have been an operator; we don't care)
 *
 * Inputs are the function OID, actual result type OID (which is needed for
 * polymorphic functions), result typmod, result collation, the input
 * collation to use for the function, the original argument list (not
 * const-simplified yet, unless process_args is false), and some flags;
 * also the context data for eval_const_expressions.
 *
 * Returns a simplified expression if successful, or NULL if cannot
 * simplify the function call.
 *
 * This function is also responsible for converting named-notation argument
 * lists into positional notation and/or adding any needed default argument
 * expressions; which is a bit grotty, but it avoids extra fetches of the
 * function's pg_proc tuple.  For this reason, the args list is
 * pass-by-reference.  Conversion and const-simplification of the args list
 * will be done even if simplification of the function call itself is not
 * possible.
 */
static Expr *
simplify_function(Oid funcid, Oid result_type, int32 result_typmod,
				  Oid result_collid, Oid input_collid, List **args_p,
				  bool funcvariadic, bool process_args, bool allow_non_const,
				  eval_const_expressions_context *context)
{
	List	   *args = *args_p;
	HeapTuple	func_tuple;
	Form_pg_proc func_form;
	Expr	   *newexpr;

	/*
	 * We have three strategies for simplification: execute the function to
	 * deliver a constant result, use a transform function to generate a
	 * substitute node tree, or expand in-line the body of the function
	 * definition (which only works for simple SQL-language functions, but
	 * that is a common case).  Each case needs access to the function's
	 * pg_proc tuple, so fetch it just once.
	 *
	 * Note: the allow_non_const flag suppresses both the second and third
	 * strategies; so if !allow_non_const, simplify_function can only return a
	 * Const or NULL.  Argument-list rewriting happens anyway, though.
	 */
	func_tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
	if (!HeapTupleIsValid(func_tuple))
		elog(ERROR, "cache lookup failed for function %u", funcid);
	func_form = (Form_pg_proc) GETSTRUCT(func_tuple);

	/*
	 * Process the function arguments, unless the caller did it already.
	 *
	 * Here we must deal with named or defaulted arguments, and then
	 * recursively apply eval_const_expressions to the whole argument list.
	 */
	if (process_args)
	{
		args = expand_function_arguments(args, result_type, func_tuple);
		args = (List *) expression_tree_mutator((Node *) args,
												eval_const_expressions_mutator,
												(void *) context);
		/* Argument processing done, give it back to the caller */
		*args_p = args;
	}

	/* Now attempt simplification of the function call proper. */

	newexpr = evaluate_function(funcid, result_type, result_typmod,
								result_collid, input_collid,
								args, funcvariadic,
								func_tuple, context);

	if (large_const(newexpr, context->max_size))
	{
		// folded expression prohibitively large
		newexpr = NULL;
	}

	if (!newexpr && allow_non_const && OidIsValid(func_form->prosupport))
	{
		/*
		 * Build a SupportRequestSimplify node to pass to the support
		 * function, pointing to a dummy FuncExpr node containing the
		 * simplified arg list.  We use this approach to present a uniform
		 * interface to the support function regardless of how the target
		 * function is actually being invoked.
		 */
		SupportRequestSimplify req;
		FuncExpr	fexpr;

		fexpr.xpr.type = T_FuncExpr;
		fexpr.funcid = funcid;
		fexpr.funcresulttype = result_type;
		fexpr.funcretset = func_form->proretset;
		fexpr.funcvariadic = funcvariadic;
		fexpr.funcformat = COERCE_EXPLICIT_CALL;
		fexpr.funccollid = result_collid;
		fexpr.inputcollid = input_collid;
		fexpr.args = args;
		fexpr.location = -1;

		req.type = T_SupportRequestSimplify;
		req.root = context->root;
		req.fcall = &fexpr;

		newexpr = (Expr *)
			DatumGetPointer(OidFunctionCall1(func_form->prosupport,
											 PointerGetDatum(&req)));

		/* catch a possible API misunderstanding */
		Assert(newexpr != (Expr *) &fexpr);
	}

	if (!newexpr && allow_non_const)
		newexpr = inline_function(funcid, result_type, result_collid,
								  input_collid, args, funcvariadic,
								  func_tuple, context);

	ReleaseSysCache(func_tuple);

	return newexpr;
}

/*
 * expand_function_arguments: convert named-notation args to positional args
 * and/or insert default args, as needed
 *
 * If we need to change anything, the input argument list is copied, not
 * modified.
 *
 * Note: this gets applied to operator argument lists too, even though the
 * cases it handles should never occur there.  This should be OK since it
 * will fall through very quickly if there's nothing to do.
 */
List *
expand_function_arguments(List *args, Oid result_type, HeapTuple func_tuple)
{
	Form_pg_proc funcform = (Form_pg_proc) GETSTRUCT(func_tuple);
	bool		has_named_args = false;
	ListCell   *lc;

	/* Do we have any named arguments? */
	foreach(lc, args)
	{
		Node	   *arg = (Node *) lfirst(lc);

		if (IsA(arg, NamedArgExpr))
		{
			has_named_args = true;
			break;
		}
	}

	/* If so, we must apply reorder_function_arguments */
	if (has_named_args)
	{
		args = reorder_function_arguments(args, func_tuple);
		/* Recheck argument types and add casts if needed */
		recheck_cast_function_args(args, result_type, func_tuple);
	}
	else if (list_length(args) < funcform->pronargs)
	{
		/* No named args, but we seem to be short some defaults */
		args = add_function_defaults(args, func_tuple);
		/* Recheck argument types and add casts if needed */
		recheck_cast_function_args(args, result_type, func_tuple);
	}

	return args;
}

/*
 * reorder_function_arguments: convert named-notation args to positional args
 *
 * This function also inserts default argument values as needed, since it's
 * impossible to form a truly valid positional call without that.
 */
static List *
reorder_function_arguments(List *args, HeapTuple func_tuple)
{
	Form_pg_proc funcform = (Form_pg_proc) GETSTRUCT(func_tuple);
	int			pronargs = funcform->pronargs;
	int			nargsprovided = list_length(args);
	Node	   *argarray[FUNC_MAX_ARGS];
	ListCell   *lc;
	int			i;

	Assert(nargsprovided <= pronargs);
	if (pronargs < 0 || pronargs > FUNC_MAX_ARGS)
		elog(ERROR, "too many function arguments");
	memset(argarray, 0, pronargs * sizeof(Node *));

	/* Deconstruct the argument list into an array indexed by argnumber */
	i = 0;
	foreach(lc, args)
	{
		Node	   *arg = (Node *) lfirst(lc);

		if (!IsA(arg, NamedArgExpr))
		{
			/* positional argument, assumed to precede all named args */
			Assert(argarray[i] == NULL);
			argarray[i++] = arg;
		}
		else
		{
			NamedArgExpr *na = (NamedArgExpr *) arg;

			Assert(argarray[na->argnumber] == NULL);
			argarray[na->argnumber] = (Node *) na->arg;
		}
	}

	/*
	 * Fetch default expressions, if needed, and insert into array at proper
	 * locations (they aren't necessarily consecutive or all used)
	 */
	if (nargsprovided < pronargs)
	{
		List	   *defaults = fetch_function_defaults(func_tuple);

		i = pronargs - funcform->pronargdefaults;
		foreach(lc, defaults)
		{
			if (argarray[i] == NULL)
				argarray[i] = (Node *) lfirst(lc);
			i++;
		}
	}

	/* Now reconstruct the args list in proper order */
	args = NIL;
	for (i = 0; i < pronargs; i++)
	{
		Assert(argarray[i] != NULL);
		args = lappend(args, argarray[i]);
	}

	return args;
}

/*
 * add_function_defaults: add missing function arguments from its defaults
 *
 * This is used only when the argument list was positional to begin with,
 * and so we know we just need to add defaults at the end.
 */
static List *
add_function_defaults(List *args, HeapTuple func_tuple)
{
	Form_pg_proc funcform = (Form_pg_proc) GETSTRUCT(func_tuple);
	int			nargsprovided = list_length(args);
	List	   *defaults;
	int			ndelete;

	/* Get all the default expressions from the pg_proc tuple */
	defaults = fetch_function_defaults(func_tuple);

	/* Delete any unused defaults from the list */
	ndelete = nargsprovided + list_length(defaults) - funcform->pronargs;
	if (ndelete < 0)
		elog(ERROR, "not enough default arguments");
	while (ndelete-- > 0)
		defaults = list_delete_first(defaults);

	/* And form the combined argument list, not modifying the input list */
	return list_concat(list_copy(args), defaults);
}

/*
 * fetch_function_defaults: get function's default arguments as expression list
 */
static List *
fetch_function_defaults(HeapTuple func_tuple)
{
	List	   *defaults;
	Datum		proargdefaults;
	bool		isnull;
	char	   *str;

	/* The error cases here shouldn't happen, but check anyway */
	proargdefaults = SysCacheGetAttr(PROCOID, func_tuple,
									 Anum_pg_proc_proargdefaults,
									 &isnull);
	if (isnull)
		elog(ERROR, "not enough default arguments");
	str = TextDatumGetCString(proargdefaults);
	defaults = castNode(List, stringToNode(str));
	pfree(str);
	return defaults;
}

/*
 * recheck_cast_function_args: recheck function args and typecast as needed
 * after adding defaults.
 *
 * It is possible for some of the defaulted arguments to be polymorphic;
 * therefore we can't assume that the default expressions have the correct
 * data types already.  We have to re-resolve polymorphics and do coercion
 * just like the parser did.
 *
 * This should be a no-op if there are no polymorphic arguments,
 * but we do it anyway to be sure.
 *
 * Note: if any casts are needed, the args list is modified in-place;
 * caller should have already copied the list structure.
 */
static void
recheck_cast_function_args(List *args, Oid result_type, HeapTuple func_tuple)
{
	Form_pg_proc funcform = (Form_pg_proc) GETSTRUCT(func_tuple);
	int			nargs;
	Oid			actual_arg_types[FUNC_MAX_ARGS];
	Oid			declared_arg_types[FUNC_MAX_ARGS];
	Oid			rettype;
	ListCell   *lc;

	if (list_length(args) > FUNC_MAX_ARGS)
		elog(ERROR, "too many function arguments");
	nargs = 0;
	foreach(lc, args)
	{
		actual_arg_types[nargs++] = exprType((Node *) lfirst(lc));
	}
	Assert(nargs == funcform->pronargs);
	memcpy(declared_arg_types, funcform->proargtypes.values,
		   funcform->pronargs * sizeof(Oid));
	rettype = enforce_generic_type_consistency(actual_arg_types,
											   declared_arg_types,
											   nargs,
											   funcform->prorettype,
											   false);
	/* let's just check we got the same answer as the parser did ... */
	if (rettype != result_type)
		elog(ERROR, "function's resolved result type changed during planning");

	/* perform any necessary typecasting of arguments */
	make_fn_arguments(NULL, args, actual_arg_types, declared_arg_types);
}

/*
 * large_const: check if given expression is a Const expression larger than
 * the given size
 *
 */
static bool
large_const(Expr *expr, Size max_size)
{
	if (NULL == expr || 0 == max_size)
	{
		return false;
	}
	
	if (!IsA(expr, Const))
	{
		return false;
	}
	
	Const *const_expr = (Const *) expr;
	
	if (const_expr->constisnull)
	{
		return false;
	}
	
	Size size = datumGetSize(const_expr->constvalue, const_expr->constbyval, const_expr->constlen);
	return size > max_size;
}

/*
 * the list of stable functions which prorettype==ANY
 */
static inline bool
in_stable_retany_funclist(Oid procid)
{
	switch (procid)
	{
	case 3209: /* jsonb_populate_record */
	case 3475: /* jsonb_populate_recordset */
	case 3960: /* json_populate_record */
	case 3961: /* json_populate_recordset */
		return true;
		break;
	default:
		/*
		 * only 4 functions now, let's use Assert() to help to notice
		 * more functions in future.
		 */
		/*
		 * GPDB_12_12_MERGE_FIXME: this Assert may impact UDFs,
		 * we need a better solution here.
		 */
		Assert(false);
		break;
	}
	return false;
}

/*
 * evaluate_function: try to pre-evaluate a function call
 *
 * We can do this if the function is strict and has any constant-null inputs
 * (just return a null constant), or if the function is immutable and has all
 * constant inputs (call it and return the result as a Const node).  In
 * estimation mode we are willing to pre-evaluate stable functions too.
 *
 * Returns a simplified expression if successful, or NULL if cannot
 * simplify the function.
 */
static Expr *
evaluate_function(Oid funcid, Oid result_type, int32 result_typmod,
				  Oid result_collid, Oid input_collid, List *args,
				  bool funcvariadic,
				  HeapTuple func_tuple,
				  eval_const_expressions_context *context)
{
	Form_pg_proc funcform = (Form_pg_proc) GETSTRUCT(func_tuple);
	bool		has_nonconst_input = false;
	bool		has_null_input = false;
	ListCell   *arg;
	FuncExpr   *newexpr;

	/*
	 * Can't simplify if it returns a set.
	 */
	if (funcform->proretset)
		return NULL;

	/*
	 * Can't simplify if it returns RECORD.  The immediate problem is that it
	 * will be needing an expected tupdesc which we can't supply here.
	 *
	 * In the case where it has OUT parameters, it could get by without an
	 * expected tupdesc, but we still have issues: get_expr_result_type()
	 * doesn't know how to extract type info from a RECORD constant, and in
	 * the case of a NULL function result there doesn't seem to be any clean
	 * way to fix that.  In view of the likelihood of there being still other
	 * gotchas, seems best to leave the function call unreduced.
	 */
	if (funcform->prorettype == RECORDOID)
		return NULL;

	/*
	 * https://github.com/greenplum-db/gpdb/issues/14499
	 * Don't pre-evaluate when it's a stable function which prorettype==ANY
	 * If it's in the FROM clause, pre-evaluting it will cause an ERROR, example:
	 * ```
	 * 	demo=# SELECT * FROM  json_populate_record(null::record, '{"x": 776}') AS (x int, y int);
	 *	psql: ERROR:  could not determine row type for result of json_populate_record
	 *	HINT:  Provide a non-null record argument, or call the function in the FROM clause using a column definition list.
	 * ```
	 */
	if (funcform->prorettype == ANYELEMENTOID && funcform->provolatile == PROVOLATILE_STABLE)
		if (in_stable_retany_funclist(funcform->oid))
			return NULL;

	/*
	 * Check for constant inputs and especially constant-NULL inputs.
	 */
	foreach(arg, args)
	{
		if (IsA(lfirst(arg), Const))
			has_null_input |= ((Const *) lfirst(arg))->constisnull;
		else
			has_nonconst_input = true;
	}

	/*
	 * If the function is strict and has a constant-NULL input, it will never
	 * be called at all, so we can replace the call by a NULL constant, even
	 * if there are other inputs that aren't constant, and even if the
	 * function is not otherwise immutable.
	 */
	if (funcform->proisstrict && has_null_input)
		return (Expr *) makeNullConst(result_type, result_typmod,
									  result_collid);

	/*
	 * Otherwise, can simplify only if all inputs are constants. (For a
	 * non-strict function, constant NULL inputs are treated the same as
	 * constant non-NULL inputs.)
	 */
	if (has_nonconst_input)
		return NULL;

	/*
	 * Ordinarily we are only allowed to simplify immutable functions. But for
	 * purposes of estimation, we consider it okay to simplify functions that
	 * are merely stable; the risk that the result might change from planning
	 * time to execution time is worth taking in preference to not being able
	 * to estimate the value at all.
	 */
	if (funcform->provolatile == PROVOLATILE_IMMUTABLE)
		 /* okay */ ;
	else if (context->estimate && funcform->provolatile == PROVOLATILE_STABLE)
		 /* okay */ ;
	else if (context->eval_stable_functions && funcform->provolatile == PROVOLATILE_STABLE)
	{
		/* okay, but we cannot reuse this plan */
		context->root->glob->oneoffPlan = true;
	}
	else
		return NULL;

	/*
	 * OK, looks like we can simplify this operator/function.
	 *
	 * Build a new FuncExpr node containing the already-simplified arguments.
	 */
	newexpr = makeNode(FuncExpr);
	newexpr->funcid = funcid;
	newexpr->funcresulttype = result_type;
	newexpr->funcretset = false;
	newexpr->funcvariadic = funcvariadic;
	newexpr->funcformat = COERCE_EXPLICIT_CALL; /* doesn't matter */
	newexpr->funccollid = result_collid;	/* doesn't matter */
	newexpr->inputcollid = input_collid;
	newexpr->args = args;
	newexpr->location = -1;

	return evaluate_expr((Expr *) newexpr, result_type, result_typmod,
						 result_collid);
}

/*
 * inline_function: try to expand a function call inline
 *
 * If the function is a sufficiently simple SQL-language function
 * (just "SELECT expression"), then we can inline it and avoid the rather
 * high per-call overhead of SQL functions.  Furthermore, this can expose
 * opportunities for constant-folding within the function expression.
 *
 * We have to beware of some special cases however.  A directly or
 * indirectly recursive function would cause us to recurse forever,
 * so we keep track of which functions we are already expanding and
 * do not re-expand them.  Also, if a parameter is used more than once
 * in the SQL-function body, we require it not to contain any volatile
 * functions (volatiles might deliver inconsistent answers) nor to be
 * unreasonably expensive to evaluate.  The expensiveness check not only
 * prevents us from doing multiple evaluations of an expensive parameter
 * at runtime, but is a safety value to limit growth of an expression due
 * to repeated inlining.
 *
 * We must also beware of changing the volatility or strictness status of
 * functions by inlining them.
 *
 * Also, at the moment we can't inline functions returning RECORD.  This
 * doesn't work in the general case because it discards information such
 * as OUT-parameter declarations.
 *
 * Also, context-dependent expression nodes in the argument list are trouble.
 *
 * Returns a simplified expression if successful, or NULL if cannot
 * simplify the function.
 */
static Expr *
inline_function(Oid funcid, Oid result_type, Oid result_collid,
				Oid input_collid, List *args,
				bool funcvariadic,
				HeapTuple func_tuple,
				eval_const_expressions_context *context)
{
	Form_pg_proc funcform = (Form_pg_proc) GETSTRUCT(func_tuple);
	char	   *src;
	Datum		tmp;
	bool		isNull;
	bool		modifyTargetList;
	MemoryContext oldcxt;
	MemoryContext mycxt;
	inline_error_callback_arg callback_arg;
	ErrorContextCallback sqlerrcontext;
	FuncExpr   *fexpr;
	SQLFunctionParseInfoPtr pinfo;
	ParseState *pstate;
	List	   *raw_parsetree_list;
	Query	   *querytree;
	Node	   *newexpr;
	int		   *usecounts;
	ListCell   *arg;
	int			i;

	/*
	 * Forget it if the function is not SQL-language or has other showstopper
	 * properties.  (The prokind and nargs checks are just paranoia.)
	 */
	if (funcform->prolang != SQLlanguageId ||
		funcform->prokind != PROKIND_FUNCTION ||
		funcform->prosecdef ||
		funcform->proretset ||
		funcform->prorettype == RECORDOID ||
		!heap_attisnull(func_tuple, Anum_pg_proc_proconfig, NULL) ||
		funcform->pronargs != list_length(args))
		return NULL;

	/* Check for recursive function, and give up trying to expand if so */
	if (list_member_oid(context->active_fns, funcid))
		return NULL;

	/* Check permission to call function (fail later, if not) */
	if (pg_proc_aclcheck(funcid, GetUserId(), ACL_EXECUTE) != ACLCHECK_OK)
		return NULL;

	/* Check whether a plugin wants to hook function entry/exit */
	if (FmgrHookIsNeeded(funcid))
		return NULL;

	/*
	 * Make a temporary memory context, so that we don't leak all the stuff
	 * that parsing might create.
	 */
	mycxt = AllocSetContextCreate(CurrentMemoryContext,
								  "inline_function",
								  ALLOCSET_DEFAULT_SIZES);
	oldcxt = MemoryContextSwitchTo(mycxt);

	/* Fetch the function body */
	tmp = SysCacheGetAttr(PROCOID,
						  func_tuple,
						  Anum_pg_proc_prosrc,
						  &isNull);
	if (isNull)
		elog(ERROR, "null prosrc for function %u", funcid);
	src = TextDatumGetCString(tmp);

	/*
	 * Setup error traceback support for ereport().  This is so that we can
	 * finger the function that bad information came from.
	 */
	callback_arg.proname = NameStr(funcform->proname);
	callback_arg.prosrc = src;

	sqlerrcontext.callback = sql_inline_error_callback;
	sqlerrcontext.arg = (void *) &callback_arg;
	sqlerrcontext.previous = error_context_stack;
	error_context_stack = &sqlerrcontext;

	/*
	 * Set up to handle parameters while parsing the function body.  We need a
	 * dummy FuncExpr node containing the already-simplified arguments to pass
	 * to prepare_sql_fn_parse_info.  (It is really only needed if there are
	 * some polymorphic arguments, but for simplicity we always build it.)
	 */
	fexpr = makeNode(FuncExpr);
	fexpr->funcid = funcid;
	fexpr->funcresulttype = result_type;
	fexpr->funcretset = false;
	fexpr->funcvariadic = funcvariadic;
	fexpr->funcformat = COERCE_EXPLICIT_CALL;	/* doesn't matter */
	fexpr->funccollid = result_collid;	/* doesn't matter */
	fexpr->inputcollid = input_collid;
	fexpr->args = args;
	fexpr->location = -1;

	pinfo = prepare_sql_fn_parse_info(func_tuple,
									  (Node *) fexpr,
									  input_collid);

	/*
	 * We just do parsing and parse analysis, not rewriting, because rewriting
	 * will not affect table-free-SELECT-only queries, which is all that we
	 * care about.  Also, we can punt as soon as we detect more than one
	 * command in the function body.
	 */
	raw_parsetree_list = pg_parse_query(src);
	if (list_length(raw_parsetree_list) != 1)
		goto fail;

	pstate = make_parsestate(NULL);
	pstate->p_sourcetext = src;
	sql_fn_parser_setup(pstate, pinfo);

	querytree = transformTopLevelStmt(pstate, linitial(raw_parsetree_list));

	free_parsestate(pstate);

	/*
	 * The single command must be a simple "SELECT expression".
	 *
	 * Note: if you change the tests involved in this, see also plpgsql's
	 * exec_simple_check_plan().  That generally needs to have the same idea
	 * of what's a "simple expression", so that inlining a function that
	 * previously wasn't inlined won't change plpgsql's conclusion.
	 */
	if (!IsA(querytree, Query) ||
		querytree->commandType != CMD_SELECT ||
		querytree->hasAggs ||
		querytree->hasWindowFuncs ||
		querytree->hasTargetSRFs ||
		querytree->hasSubLinks ||
		querytree->cteList ||
		querytree->rtable ||
		querytree->jointree->fromlist ||
		querytree->jointree->quals ||
		querytree->groupClause ||
		querytree->groupingSets ||
		querytree->havingQual ||
		querytree->windowClause ||
		querytree->distinctClause ||
		querytree->sortClause ||
		querytree->limitOffset ||
		querytree->limitCount ||
		querytree->setOperations ||
		list_length(querytree->targetList) != 1)
		goto fail;

	/*
	 * Make sure the function (still) returns what it's declared to.  This
	 * will raise an error if wrong, but that's okay since the function would
	 * fail at runtime anyway.  Note that check_sql_fn_retval will also insert
	 * a RelabelType if needed to make the tlist expression match the declared
	 * type of the function.
	 *
	 * Note: we do not try this until we have verified that no rewriting was
	 * needed; that's probably not important, but let's be careful.
	 */
	if (check_sql_fn_retval(funcid, result_type, list_make1(querytree),
							&modifyTargetList, NULL))
		goto fail;				/* reject whole-tuple-result cases */

	/* Now we can grab the tlist expression */
	newexpr = (Node *) ((TargetEntry *) linitial(querytree->targetList))->expr;

	/*
	 * If the SQL function returns VOID, we can only inline it if it is a
	 * SELECT of an expression returning VOID (ie, it's just a redirection to
	 * another VOID-returning function).  In all non-VOID-returning cases,
	 * check_sql_fn_retval should ensure that newexpr returns the function's
	 * declared result type, so this test shouldn't fail otherwise; but we may
	 * as well cope gracefully if it does.
	 */
	if (exprType(newexpr) != result_type)
		goto fail;

	/* check_sql_fn_retval couldn't have made any dangerous tlist changes */
	Assert(!modifyTargetList);

	/*
	 * Additional validity checks on the expression.  It mustn't be more
	 * volatile than the surrounding function (this is to avoid breaking hacks
	 * that involve pretending a function is immutable when it really ain't).
	 * If the surrounding function is declared strict, then the expression
	 * must contain only strict constructs and must use all of the function
	 * parameters (this is overkill, but an exact analysis is hard).
	 */
	if (funcform->provolatile == PROVOLATILE_IMMUTABLE &&
		contain_mutable_functions(newexpr))
		goto fail;
	else if (funcform->provolatile == PROVOLATILE_STABLE &&
			 contain_volatile_functions(newexpr))
		goto fail;

	if (funcform->proisstrict &&
		contain_nonstrict_functions(newexpr))
		goto fail;

	/*
	 * If any parameter expression contains a context-dependent node, we can't
	 * inline, for fear of putting such a node into the wrong context.
	 */
	if (contain_context_dependent_node((Node *) args))
		goto fail;

	/*
	 * We may be able to do it; there are still checks on parameter usage to
	 * make, but those are most easily done in combination with the actual
	 * substitution of the inputs.  So start building expression with inputs
	 * substituted.
	 */
	usecounts = (int *) palloc0(funcform->pronargs * sizeof(int));
	newexpr = substitute_actual_parameters(newexpr, funcform->pronargs,
										   args, usecounts);

	/* Now check for parameter usage */
	i = 0;
	foreach(arg, args)
	{
		Node	   *param = lfirst(arg);

		if (usecounts[i] == 0)
		{
			/* Param not used at all: uncool if func is strict */
			if (funcform->proisstrict)
				goto fail;
		}
		else if (usecounts[i] != 1)
		{
			/* Param used multiple times: uncool if expensive or volatile */
			QualCost	eval_cost;

			/*
			 * We define "expensive" as "contains any subplan or more than 10
			 * operators".  Note that the subplan search has to be done
			 * explicitly, since cost_qual_eval() will barf on unplanned
			 * subselects.
			 */
			if (contain_subplans(param))
				goto fail;
			cost_qual_eval(&eval_cost, list_make1(param), NULL);
			if (eval_cost.startup + eval_cost.per_tuple >
				10 * cpu_operator_cost)
				goto fail;

			/*
			 * Check volatility last since this is more expensive than the
			 * above tests
			 */
			if (contain_volatile_functions(param))
				goto fail;
		}
		i++;
	}

	/*
	 * Whew --- we can make the substitution.  Copy the modified expression
	 * out of the temporary memory context, and clean up.
	 */
	MemoryContextSwitchTo(oldcxt);

	newexpr = copyObject(newexpr);

	MemoryContextDelete(mycxt);

	/*
	 * If the result is of a collatable type, force the result to expose the
	 * correct collation.  In most cases this does not matter, but it's
	 * possible that the function result is used directly as a sort key or in
	 * other places where we expect exprCollation() to tell the truth.
	 */
	if (OidIsValid(result_collid))
	{
		Oid			exprcoll = exprCollation(newexpr);

		if (OidIsValid(exprcoll) && exprcoll != result_collid)
		{
			CollateExpr *newnode = makeNode(CollateExpr);

			newnode->arg = (Expr *) newexpr;
			newnode->collOid = result_collid;
			newnode->location = -1;

			newexpr = (Node *) newnode;
		}
	}

	/*
	 * Since there is now no trace of the function in the plan tree, we must
	 * explicitly record the plan's dependency on the function.
	 */
	if (context->root)
		record_plan_function_dependency(context->root, funcid);

	/*
	 * Recursively try to simplify the modified expression.  Here we must add
	 * the current function to the context list of active functions.
	 */
	context->active_fns = lcons_oid(funcid, context->active_fns);
	newexpr = eval_const_expressions_mutator(newexpr, context);
	context->active_fns = list_delete_first(context->active_fns);

	error_context_stack = sqlerrcontext.previous;

	return (Expr *) newexpr;

	/* Here if func is not inlinable: release temp memory and return NULL */
fail:
	MemoryContextSwitchTo(oldcxt);
	MemoryContextDelete(mycxt);
	error_context_stack = sqlerrcontext.previous;

	return NULL;
}

/*
 * Replace Param nodes by appropriate actual parameters
 */
static Node *
substitute_actual_parameters(Node *expr, int nargs, List *args,
							 int *usecounts)
{
	substitute_actual_parameters_context context;

	context.nargs = nargs;
	context.args = args;
	context.usecounts = usecounts;

	return substitute_actual_parameters_mutator(expr, &context);
}

static Node *
substitute_actual_parameters_mutator(Node *node,
									 substitute_actual_parameters_context *context)
{
	if (node == NULL)
		return NULL;
	if (IsA(node, Param))
	{
		Param	   *param = (Param *) node;

		if (param->paramkind != PARAM_EXTERN)
			elog(ERROR, "unexpected paramkind: %d", (int) param->paramkind);
		if (param->paramid <= 0 || param->paramid > context->nargs)
			elog(ERROR, "invalid paramid: %d", param->paramid);

		/* Count usage of parameter */
		context->usecounts[param->paramid - 1]++;

		/* Select the appropriate actual arg and replace the Param with it */
		/* We don't need to copy at this time (it'll get done later) */
		return list_nth(context->args, param->paramid - 1);
	}
	return expression_tree_mutator(node, substitute_actual_parameters_mutator,
								   (void *) context);
}

/*
 * error context callback to let us supply a call-stack traceback
 */
static void
sql_inline_error_callback(void *arg)
{
	inline_error_callback_arg *callback_arg = (inline_error_callback_arg *) arg;
	int			syntaxerrposition;

	/* If it's a syntax error, convert to internal syntax error report */
	syntaxerrposition = geterrposition();
	if (syntaxerrposition > 0)
	{
		errposition(0);
		internalerrposition(syntaxerrposition);
		internalerrquery(callback_arg->prosrc);
	}

	errcontext("SQL function \"%s\" during inlining", callback_arg->proname);
}

/*
 * evaluate_expr: pre-evaluate a constant expression
 *
 * We use the executor's routine ExecEvalExpr() to avoid duplication of
 * code and ensure we get the same result as the executor would get.
 */
Expr *
evaluate_expr(Expr *expr, Oid result_type, int32 result_typmod,
			  Oid result_collation)
{
	EState	   *estate;
	ExprState  *exprstate;
	MemoryContext oldcontext;
	Datum		const_val;
	bool		const_is_null;
	int16		resultTypLen;
	bool		resultTypByVal;

	/*
	 * To use the executor, we need an EState.
	 */
	estate = CreateExecutorState();

	/* We can use the estate's working context to avoid memory leaks. */
	oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

	/* Make sure any opfuncids are filled in. */
	fix_opfuncids((Node *) expr);

	/*
	 * Prepare expr for execution.  (Note: we can't use ExecPrepareExpr
	 * because it'd result in recursively invoking eval_const_expressions.)
	 */
	exprstate = ExecInitExpr(expr, NULL);

	/*
	 * And evaluate it.
	 *
	 * It is OK to use a default econtext because none of the ExecEvalExpr()
	 * code used in this situation will use econtext.  That might seem
	 * fortuitous, but it's not so unreasonable --- a constant expression does
	 * not depend on context, by definition, n'est ce pas?
	 */
	const_val = ExecEvalExprSwitchContext(exprstate,
										  GetPerTupleExprContext(estate),
										  &const_is_null);

	/* Get info needed about result datatype */
	get_typlenbyval(result_type, &resultTypLen, &resultTypByVal);

	/* Get back to outer memory context */
	MemoryContextSwitchTo(oldcontext);

	/*
	 * Must copy result out of sub-context used by expression eval.
	 *
	 * Also, if it's varlena, forcibly detoast it.  This protects us against
	 * storing TOAST pointers into plans that might outlive the referenced
	 * data.  (makeConst would handle detoasting anyway, but it's worth a few
	 * extra lines here so that we can do the copy and detoast in one step.)
	 */
	if (!const_is_null)
	{
		if (resultTypLen == -1)
			const_val = PointerGetDatum(PG_DETOAST_DATUM_COPY(const_val));
		else
			const_val = datumCopy(const_val, resultTypByVal, resultTypLen);
	}

	/* Release all the junk we just created */
	FreeExecutorState(estate);

	/*
	 * Make the constant result node.
	 */
	return (Expr *) makeConst(result_type, result_typmod, result_collation,
							  resultTypLen,
							  const_val, const_is_null,
							  resultTypByVal);
}


/*
 * inline_set_returning_function
 *		Attempt to "inline" a set-returning function in the FROM clause.
 *
 * "rte" is an RTE_FUNCTION rangetable entry.  If it represents a call of a
 * set-returning SQL function that can safely be inlined, expand the function
 * and return the substitute Query structure.  Otherwise, return NULL.
 *
 * This has a good deal of similarity to inline_function(), but that's
 * for the non-set-returning case, and there are enough differences to
 * justify separate functions.
 */
Query *
inline_set_returning_function(PlannerInfo *root, RangeTblEntry *rte)
{
	RangeTblFunction *rtfunc;
	FuncExpr   *fexpr;
	Oid			func_oid;
	HeapTuple	func_tuple;
	Form_pg_proc funcform;
	char	   *src;
	Datum		tmp;
	bool		isNull;
	bool		modifyTargetList;
	MemoryContext oldcxt;
	MemoryContext mycxt;
	List	   *saveInvalItems;
	inline_error_callback_arg callback_arg;
	ErrorContextCallback sqlerrcontext;
	SQLFunctionParseInfoPtr pinfo;
	List	   *raw_parsetree_list;
	List	   *querytree_list;
	Query	   *querytree;

	Assert(rte->rtekind == RTE_FUNCTION);

	/*
	 * It doesn't make a lot of sense for a SQL SRF to refer to itself in its
	 * own FROM clause, since that must cause infinite recursion at runtime.
	 * It will cause this code to recurse too, so check for stack overflow.
	 * (There's no need to do more.)
	 */
	check_stack_depth();

	/* Fail if the RTE has ORDINALITY - we don't implement that here. */
	if (rte->funcordinality)
		return NULL;

	/* Fail if RTE isn't a single, simple FuncExpr */
	if (list_length(rte->functions) != 1)
		return NULL;
	rtfunc = (RangeTblFunction *) linitial(rte->functions);

	if (!IsA(rtfunc->funcexpr, FuncExpr))
		return NULL;
	fexpr = (FuncExpr *) rtfunc->funcexpr;

	func_oid = fexpr->funcid;

	/*
	 * The function must be declared to return a set, else inlining would
	 * change the results if the contained SELECT didn't return exactly one
	 * row.
	 */
	if (!fexpr->funcretset)
		return NULL;

	/*
	 * Refuse to inline if the arguments contain any volatile functions or
	 * sub-selects.  Volatile functions are rejected because inlining may
	 * result in the arguments being evaluated multiple times, risking a
	 * change in behavior.  Sub-selects are rejected partly for implementation
	 * reasons (pushing them down another level might change their behavior)
	 * and partly because they're likely to be expensive and so multiple
	 * evaluation would be bad.
	 */
	if (contain_volatile_functions((Node *) fexpr->args) ||
		contain_subplans((Node *) fexpr->args))
		return NULL;

	/* Check permission to call function (fail later, if not) */
	if (pg_proc_aclcheck(func_oid, GetUserId(), ACL_EXECUTE) != ACLCHECK_OK)
		return NULL;

	/* Check whether a plugin wants to hook function entry/exit */
	if (FmgrHookIsNeeded(func_oid))
		return NULL;

	/*
	 * OK, let's take a look at the function's pg_proc entry.
	 */
	func_tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(func_oid));
	if (!HeapTupleIsValid(func_tuple))
		elog(ERROR, "cache lookup failed for function %u", func_oid);
	funcform = (Form_pg_proc) GETSTRUCT(func_tuple);

	/*
	 * Forget it if the function is not SQL-language or has other showstopper
	 * properties.  In particular it mustn't be declared STRICT, since we
	 * couldn't enforce that.  It also mustn't be VOLATILE, because that is
	 * supposed to cause it to be executed with its own snapshot, rather than
	 * sharing the snapshot of the calling query.  We also disallow returning
	 * SETOF VOID, because inlining would result in exposing the actual result
	 * of the function's last SELECT, which should not happen in that case.
	 * (Rechecking prokind and proretset is just paranoia.)
	 */
	if (funcform->prolang != SQLlanguageId ||
		funcform->prokind != PROKIND_FUNCTION ||
		funcform->proisstrict ||
		funcform->provolatile == PROVOLATILE_VOLATILE ||
		funcform->prorettype == VOIDOID ||
		funcform->prosecdef ||
		!funcform->proretset ||
		!heap_attisnull(func_tuple, Anum_pg_proc_proconfig, NULL))
	{
		ReleaseSysCache(func_tuple);
		return NULL;
	}

	/*
	 * Make a temporary memory context, so that we don't leak all the stuff
	 * that parsing might create.
	 */
	mycxt = AllocSetContextCreate(CurrentMemoryContext,
								  "inline_set_returning_function",
								  ALLOCSET_DEFAULT_SIZES);
	oldcxt = MemoryContextSwitchTo(mycxt);

	/*
	 * When we call eval_const_expressions below, it might try to add items to
	 * root->glob->invalItems.  Since it is running in the temp context, those
	 * items will be in that context, and will need to be copied out if we're
	 * successful.  Temporarily reset the list so that we can keep those items
	 * separate from the pre-existing list contents.
	 */
	saveInvalItems = root->glob->invalItems;
	root->glob->invalItems = NIL;

	/* Fetch the function body */
	tmp = SysCacheGetAttr(PROCOID,
						  func_tuple,
						  Anum_pg_proc_prosrc,
						  &isNull);
	if (isNull)
		elog(ERROR, "null prosrc for function %u", func_oid);
	src = TextDatumGetCString(tmp);

	/*
	 * Setup error traceback support for ereport().  This is so that we can
	 * finger the function that bad information came from.
	 */
	callback_arg.proname = NameStr(funcform->proname);
	callback_arg.prosrc = src;

	sqlerrcontext.callback = sql_inline_error_callback;
	sqlerrcontext.arg = (void *) &callback_arg;
	sqlerrcontext.previous = error_context_stack;
	error_context_stack = &sqlerrcontext;

	/*
	 * Run eval_const_expressions on the function call.  This is necessary to
	 * ensure that named-argument notation is converted to positional notation
	 * and any default arguments are inserted.  It's a bit of overkill for the
	 * arguments, since they'll get processed again later, but no harm will be
	 * done.
	 */
	fexpr = (FuncExpr *) eval_const_expressions(root, (Node *) fexpr);

	/* It should still be a call of the same function, but let's check */
	if (!IsA(fexpr, FuncExpr) ||
		fexpr->funcid != func_oid)
		goto fail;

	/* Arg list length should now match the function */
	if (list_length(fexpr->args) != funcform->pronargs)
		goto fail;

	/*
	 * Set up to handle parameters while parsing the function body.  We can
	 * use the FuncExpr just created as the input for
	 * prepare_sql_fn_parse_info.
	 */
	pinfo = prepare_sql_fn_parse_info(func_tuple,
									  (Node *) fexpr,
									  fexpr->inputcollid);

	/*
	 * Parse, analyze, and rewrite (unlike inline_function(), we can't skip
	 * rewriting here).  We can fail as soon as we find more than one query,
	 * though.
	 */
	raw_parsetree_list = pg_parse_query(src);
	if (list_length(raw_parsetree_list) != 1)
		goto fail;

	querytree_list = pg_analyze_and_rewrite_params(linitial(raw_parsetree_list),
												   src,
												   (ParserSetupHook) sql_fn_parser_setup,
												   pinfo, NULL);
	if (list_length(querytree_list) != 1)
		goto fail;
	querytree = linitial(querytree_list);

	/*
	 * The single command must be a plain SELECT.
	 */
	if (!IsA(querytree, Query) ||
		querytree->commandType != CMD_SELECT)
		goto fail;

	/*
	 * Make sure the function (still) returns what it's declared to.  This
	 * will raise an error if wrong, but that's okay since the function would
	 * fail at runtime anyway.  Note that check_sql_fn_retval will also insert
	 * RelabelType(s) and/or NULL columns if needed to make the tlist
	 * expression(s) match the declared type of the function.
	 *
	 * If the function returns a composite type, don't inline unless the check
	 * shows it's returning a whole tuple result; otherwise what it's
	 * returning is a single composite column which is not what we need. (Like
	 * check_sql_fn_retval, we deliberately exclude domains over composite
	 * here.)
	 */
	if (!check_sql_fn_retval(func_oid, fexpr->funcresulttype,
							 querytree_list,
							 &modifyTargetList, NULL) &&
		(get_typtype(fexpr->funcresulttype) == TYPTYPE_COMPOSITE ||
		 fexpr->funcresulttype == RECORDOID))
		goto fail;				/* reject not-whole-tuple-result cases */

	/*
	 * If we had to modify the tlist to make it match, and the statement is
	 * one in which changing the tlist contents could change semantics, we
	 * have to punt and not inline.
	 */
	if (modifyTargetList)
		goto fail;

	/*
	 * If it returns RECORD, we have to check against the column type list
	 * provided in the RTE; check_sql_fn_retval can't do that.  (If no match,
	 * we just fail to inline, rather than complaining; see notes for
	 * tlist_matches_coltypelist.)	We don't have to do this for functions
	 * with declared OUT parameters, even though their funcresulttype is
	 * RECORDOID, so check get_func_result_type too.
	 */
	if (fexpr->funcresulttype == RECORDOID &&
		get_func_result_type(func_oid, NULL, NULL) == TYPEFUNC_RECORD &&
		!tlist_matches_coltypelist(querytree->targetList,
								   rtfunc->funccoltypes))
		goto fail;

	/*
	 * Looks good --- substitute parameters into the query.
	 */
	querytree = substitute_actual_srf_parameters(querytree,
												 funcform->pronargs,
												 fexpr->args);

	/*
	 * Copy the modified query out of the temporary memory context, and clean
	 * up.
	 */
	MemoryContextSwitchTo(oldcxt);

	querytree = copyObject(querytree);

	/* copy up any new invalItems, too */
	root->glob->invalItems = list_concat(saveInvalItems,
										 copyObject(root->glob->invalItems));

	MemoryContextDelete(mycxt);
	error_context_stack = sqlerrcontext.previous;
	ReleaseSysCache(func_tuple);

	/*
	 * We don't have to fix collations here because the upper query is already
	 * parsed, ie, the collations in the RTE are what count.
	 */

	/*
	 * Since there is now no trace of the function in the plan tree, we must
	 * explicitly record the plan's dependency on the function.
	 */
	record_plan_function_dependency(root, func_oid);

	/*
	 * We must also notice if the inserted query adds a dependency on the
	 * calling role due to RLS quals.
	 */
	if (querytree->hasRowSecurity)
		root->glob->dependsOnRole = true;

	return querytree;

	/* Here if func is not inlinable: release temp memory and return NULL */
fail:
	MemoryContextSwitchTo(oldcxt);
	root->glob->invalItems = saveInvalItems;
	MemoryContextDelete(mycxt);
	error_context_stack = sqlerrcontext.previous;
	ReleaseSysCache(func_tuple);

	return NULL;
}

/*
 * Replace Param nodes by appropriate actual parameters
 *
 * This is just enough different from substitute_actual_parameters()
 * that it needs its own code.
 */
static Query *
substitute_actual_srf_parameters(Query *expr, int nargs, List *args)
{
	substitute_actual_srf_parameters_context context;

	context.nargs = nargs;
	context.args = args;
	context.sublevels_up = 1;

	return query_tree_mutator(expr,
							  substitute_actual_srf_parameters_mutator,
							  &context,
							  0);
}

static Node *
substitute_actual_srf_parameters_mutator(Node *node,
										 substitute_actual_srf_parameters_context *context)
{
	Node	   *result;

	if (node == NULL)
		return NULL;
	if (IsA(node, Query))
	{
		context->sublevels_up++;
		result = (Node *) query_tree_mutator((Query *) node,
											 substitute_actual_srf_parameters_mutator,
											 (void *) context,
											 0);
		context->sublevels_up--;
		return result;
	}
	if (IsA(node, Param))
	{
		Param	   *param = (Param *) node;

		if (param->paramkind == PARAM_EXTERN)
		{
			if (param->paramid <= 0 || param->paramid > context->nargs)
				elog(ERROR, "invalid paramid: %d", param->paramid);

			/*
			 * Since the parameter is being inserted into a subquery, we must
			 * adjust levels.
			 */
			result = copyObject(list_nth(context->args, param->paramid - 1));
			IncrementVarSublevelsUp(result, context->sublevels_up, 0);
			return result;
		}
	}
	return expression_tree_mutator(node,
								   substitute_actual_srf_parameters_mutator,
								   (void *) context);
}

/*
 * flatten_join_alias_var_optimizer
 *	  Replace Vars that reference JOIN outputs with references to the original
 *	  relation variables instead.
 */
Query *
flatten_join_alias_var_optimizer(Query *query, int queryLevel)
{
	Query *queryNew = (Query *) copyObject(query);

	/*
	 * Flatten join alias for expression in
	 * 1. targetlist
	 * 2. returningList
	 * 3. having qual
	 * 4. scatterClause
	 * 5. limit offset
	 * 6. limit count
	 * 
	 * We flatten the above expressions since these entries may be moved during the query 
	 * normalization step before algebrization. In contrast, the planner flattens alias 
	 * inside quals to allow predicates involving such vars to be pushed down. 
	 * 
	 * Here we ignore the flattening of quals due to the following reasons:
	 * 1. we assume that the function will be called before Query->DXL translation:
	 * 2. the quals never gets moved from old query to the new top-level query in the 
	 * query normalization phase before algebrization. In other words, the quals hang of 
	 * the same query structure that is now the new derived table.
	 * 3. the algebrizer can resolve the abiquity of join aliases in quals since we maintain 
	 * all combinations of <query level, varno, varattno> to DXL-ColId during Query->DXL translation.
	 * 
	 */

	List *targetList = queryNew->targetList;
	if (NIL != targetList)
	{
		queryNew->targetList = (List *) flatten_join_alias_vars(queryNew, (Node *) targetList);
		list_free(targetList);
	}

	List * returningList = queryNew->returningList;
	if (NIL != returningList)
	{
		queryNew->returningList = (List *) flatten_join_alias_vars(queryNew, (Node *) returningList);
		list_free(returningList);
	}

	Node *havingQual = queryNew->havingQual;
	if (NULL != havingQual)
	{
		queryNew->havingQual = flatten_join_alias_vars(queryNew, havingQual);
		pfree(havingQual);
	}

	List *scatterClause = queryNew->scatterClause;
	if (NIL != scatterClause)
	{
		queryNew->scatterClause = (List *) flatten_join_alias_vars(queryNew, (Node *) scatterClause);
		list_free(scatterClause);
	}

	Node *limitOffset = queryNew->limitOffset;
	if (NULL != limitOffset)
	{
		queryNew->limitOffset = flatten_join_alias_vars(queryNew, limitOffset);
		pfree(limitOffset);
	}

	List *windowClause = queryNew->windowClause;
	if (NIL != queryNew->windowClause)
	{
		ListCell *l;

		foreach (l, windowClause)
		{
			WindowClause *wc = (WindowClause *) lfirst(l);

			if (wc == NULL)
				continue;

			if (wc->startOffset)
				wc->startOffset = flatten_join_alias_vars(queryNew, wc->startOffset);

			if (wc->endOffset)
				wc->endOffset = flatten_join_alias_vars(queryNew, wc->endOffset);
		}
	}

	Node *limitCount = queryNew->limitCount;
	if (NULL != limitCount)
	{
		queryNew->limitCount = flatten_join_alias_vars(queryNew, limitCount);
		pfree(limitCount);
	}

    return queryNew;
}

/**
 * Structs and Methods to support searching of matching subexpressions.
 */

typedef struct subexpression_matching_context
{
	Expr *needle;	/* This is the expression being searched */
} subexpression_matching_context;

/**
 * expression_matching_walker checks if the expression 'needle' in context is a sub-expression of hayStack.
 */
static bool subexpression_matching_walker(Node *hayStack, void *context)
{
	Assert(context);
	subexpression_matching_context *ctx = (subexpression_matching_context *) context;
	Assert(ctx->needle);

	if (!hayStack)
	{
		return false;
	}

	if (equal(ctx->needle, hayStack))
	{
		return true;
	}

	return expression_tree_walker(hayStack, subexpression_matching_walker, (void *) context);
}

/**
 * Method checks if expr1 is a subexpression of expr2.
 * For example, expr1 = (x + 2) and expr = (x + 2 ) * 100 + 20 would return true.
 */
bool subexpression_match(Expr *expr1, Expr *expr2)
{
	subexpression_matching_context ctx;
	ctx.needle = expr1;
	return subexpression_matching_walker((Node *) expr2, (void *) &ctx);
}

/*
 * Check whether a SELECT targetlist emits the specified column types,
 * to see if it's safe to inline a function returning record.
 *
 * We insist on exact match here.  The executor allows binary-coercible
 * cases too, but we don't have a way to preserve the correct column types
 * in the correct places if we inline the function in such a case.
 *
 * Note that we only check type OIDs not typmods; this agrees with what the
 * executor would do at runtime, and attributing a specific typmod to a
 * function result is largely wishful thinking anyway.
 */
static bool
tlist_matches_coltypelist(List *tlist, List *coltypelist)
{
	ListCell   *tlistitem;
	ListCell   *clistitem;

	clistitem = list_head(coltypelist);
	foreach(tlistitem, tlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(tlistitem);
		Oid			coltype;

		if (tle->resjunk)
			continue;			/* ignore junk columns */

		if (clistitem == NULL)
			return false;		/* too many tlist items */

		coltype = lfirst_oid(clistitem);
		clistitem = lnext(clistitem);

		if (exprType((Node *) tle->expr) != coltype)
			return false;		/* column type mismatch */
	}

	if (clistitem != NULL)
		return false;			/* too few tlist items */

	return true;
}

/*
 * If this expression is part of a query, and the query isn't a simple
 * "SELECT foo()" style query with no actual tables involved, then we
 * also aggressively evaluate stable functions, in addition to immutable
 * ones. Such plans cannot be reused, and therefore need to be re-planned
 * on every execution, but it can be a big win if it allows partition
 * elimination to happen. That's considered a good tradeoff in GPDB, as
 * typical queries are long-running.
 */
static bool
should_eval_stable_functions(PlannerInfo *root)
{
	/*
	 * Without PlannerGlobal, we cannot mark the plan as a `oneoffPlan`
	 */
	if (root == NULL) return false;
	if (root->glob == NULL) return false;
	if (root->parse == NULL) return false;

	/*
	 * If the query has no range table, then there is no reason to need to
	 * pre-evaluate stable functions, as the output cannot be used as part
	 * of static partition elimination, unless the query is part of a
	 * subquery.
	 */
	if (root->query_level > 1) return true;

	int rtable_num_total = list_length(root->parse->rtable);
	ListCell *lc;
	foreach(lc, root->parse->rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *)lfirst(lc);

		/* RTE_RESULT is a dummy RTE generated by empty from clause */
		if (rte->rtekind == RTE_RESULT)
			rtable_num_total --;
	}

	return rtable_num_total > 0;
}


/*
 * get_leftscalararrayop
 *
 * Returns the left operand of a clause of the form (scalar op ANY/ALL (array))
 */
Node *
get_leftscalararrayop(const Expr *clause)
{
	const ScalarArrayOpExpr *expr = (const ScalarArrayOpExpr *) clause;

	if (expr->args != NIL)
		return linitial(expr->args);
	else
		return NULL;
}

/*
 * get_rightscalararrayop
 *
 * Returns the right operand in a clause of the form (scalar op ANY/ALL (array)).
 */
Node *
get_rightscalararrayop(const Expr *clause)
{
	const ScalarArrayOpExpr *expr = (const ScalarArrayOpExpr *) clause;

	if (list_length(expr->args) >= 2)
		return lsecond(expr->args);
	else
		return NULL;
}

/*
 * init_planglob_plannerinfo
 *
 * Initializes, PlannerGlobal and PlannerInfo for given query and query level.
 */
void init_planglob_plannerinfo(PlannerGlobal *glob, PlannerInfo *root, Query *query, Index query_level)
{
	glob->subplans = NIL;
	glob->subroots = NIL;
	glob->rewindPlanIDs = NULL;
	glob->transientPlan = false;
	glob->oneoffPlan = false;
	glob->share.shared_inputs = NULL;
	glob->share.shared_input_count = 0;
	glob->share.motStack = NIL;
	glob->share.qdShares = NULL;
	/* If required, these will be filled in the pre- and post-processing steps */
	glob->finalrtable = NIL;
	glob->relationOids = NIL;
	glob->invalItems = NIL;

	root->parse = query;
	root->glob = glob;
	root->query_level = query_level;
	root->planner_cxt = CurrentMemoryContext;
	root->wt_param_id = -1;
}
