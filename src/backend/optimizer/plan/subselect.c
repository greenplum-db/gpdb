/*-------------------------------------------------------------------------
 *
 * subselect.c
 *	  Planning routines for subselects and parameters.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/optimizer/plan/subselect.c,v 1.129.2.3 2009/04/25 16:45:02 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/catalog.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "catalog/gp_policy.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/relation.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/subselect.h"
#include "optimizer/var.h"
#include "parser/parse_expr.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

#include "cdb/cdbmutate.h"
#include "cdb/cdbsubselect.h"
#include "cdb/cdbvars.h"


typedef struct convert_testexpr_context
{
	PlannerInfo *root;
	List	   *subst_nodes;	/* Nodes to substitute for Params */
} convert_testexpr_context;

typedef struct process_sublinks_context
{
	PlannerInfo *root;
	bool		isTopQual;
} process_sublinks_context;

typedef struct finalize_primnode_context
{
	PlannerInfo *root;
	Bitmapset  *paramids;		/* Non-local PARAM_EXEC paramids found */
} finalize_primnode_context;


static Node *build_subplan(PlannerInfo *root, Plan *plan, List *rtable,
			  SubLinkType subLinkType, Node *testexpr,
			  bool unknownEqFalse);

static List *generate_subquery_params(PlannerInfo *root, List *tlist,
						 List **paramIds);
static Node *convert_testexpr_mutator(Node *node,
						 convert_testexpr_context *context);
static bool subplan_is_hashable(PlannerInfo *root, Plan *plan);
static bool testexpr_is_hashable(Node *testexpr);
static bool hash_ok_operator(OpExpr *expr);
static Node *replace_correlation_vars_mutator(Node *node, PlannerInfo *root);
static Node *process_sublinks_mutator(Node *node,
						 process_sublinks_context *context);
static Bitmapset *finalize_plan(PlannerInfo *root,
			  Plan *plan,
			  Bitmapset *valid_params);
static bool finalize_primnode(Node *node, finalize_primnode_context *context);

extern	double global_work_mem(PlannerInfo *root);

/*
 * Generate a Param node to replace the given Var,
 * which is expected to have varlevelsup > 0 (ie, it is not local).
 */
static Param *
replace_outer_var(PlannerInfo *root, Var *var)
{
	Param	   *retval;
	ListCell   *ppl;
	PlannerParamItem *pitem;
	Index		abslevel;
	int			i;

	Assert(var->varlevelsup > 0 && var->varlevelsup < root->query_level);
	abslevel = root->query_level - var->varlevelsup;

	/*
	 * If there's already a paramlist entry for this same Var, just use it.
	 * NOTE: in sufficiently complex querytrees, it is possible for the same
	 * varno/abslevel to refer to different RTEs in different parts of the
	 * parsetree, so that different fields might end up sharing the same Param
	 * number.	As long as we check the vartype as well, I believe that this
	 * sort of aliasing will cause no trouble. The correct field should get
	 * stored into the Param slot at execution in each part of the tree.
	 *
	 * We also need to demand a match on vartypmod.  This does not matter for
	 * the Param itself, since those are not typmod-dependent, but it does
	 * matter when make_subplan() instantiates a modified copy of the Var for
	 * a subplan's args list.
	 */
	i = 0;
	foreach(ppl, root->glob->paramlist)
	{
		pitem = (PlannerParamItem *) lfirst(ppl);
		if (pitem->abslevel == abslevel && IsA(pitem->item, Var))
		{
			Var		   *pvar = (Var *) pitem->item;

			if (pvar->varno == var->varno &&
				pvar->varattno == var->varattno &&
				pvar->vartype == var->vartype &&
				pvar->vartypmod == var->vartypmod)
				break;
		}
		i++;
	}

	if (!ppl)
	{
		/* Nope, so make a new one */
		var = (Var *) copyObject(var);
		var->varlevelsup = 0;

		pitem = makeNode(PlannerParamItem);
		pitem->item = (Node *) var;
		pitem->abslevel = abslevel;

		root->glob->paramlist = lappend(root->glob->paramlist, pitem);
		/* i is already the correct index for the new item */
	}

	retval = makeNode(Param);
	retval->paramkind = PARAM_EXEC;
	retval->paramid = i;
	retval->paramtype = var->vartype;
	retval->paramtypmod = var->vartypmod;
	retval->location = -1;

	return retval;
}

/*
 * Generate a Param node to replace the given Aggref
 * which is expected to have agglevelsup > 0 (ie, it is not local).
 */
static Param *
replace_outer_agg(PlannerInfo *root, Aggref *agg)
{
	Param	   *retval;
	PlannerParamItem *pitem;
	Index		abslevel;
	int			i;

	Assert(agg->agglevelsup > 0 && agg->agglevelsup < root->query_level);
	abslevel = root->query_level - agg->agglevelsup;

	/*
	 * It does not seem worthwhile to try to match duplicate outer aggs. Just
	 * make a new slot every time.
	 */
	agg = (Aggref *) copyObject(agg);
	IncrementVarSublevelsUp((Node *) agg, -((int) agg->agglevelsup), 0);
	Assert(agg->agglevelsup == 0);

	pitem = makeNode(PlannerParamItem);
	pitem->item = (Node *) agg;
	pitem->abslevel = abslevel;

	root->glob->paramlist = lappend(root->glob->paramlist, pitem);
	i = list_length(root->glob->paramlist) - 1;

	retval = makeNode(Param);
	retval->paramkind = PARAM_EXEC;
	retval->paramid = i;
	retval->paramtype = agg->aggtype;
	retval->paramtypmod = -1;
	retval->location = -1;

	return retval;
}

/*
 * Generate a new Param node that will not conflict with any other.
 *
 * This is used to allocate PARAM_EXEC slots for subplan outputs.
 */
static Param *
generate_new_param(PlannerInfo *root, Oid paramtype, int32 paramtypmod)
{
	Param	   *retval;
	PlannerParamItem *pitem;

	retval = makeNode(Param);
	retval->paramkind = PARAM_EXEC;
	retval->paramid = list_length(root->glob->paramlist);
	retval->paramtype = paramtype;
	retval->paramtypmod = paramtypmod;
	retval->location = -1;

	pitem = makeNode(PlannerParamItem);
	pitem->item = (Node *) retval;
	pitem->abslevel = root->query_level;

	root->glob->paramlist = lappend(root->glob->paramlist, pitem);

	return retval;
}

/*
 * Get the datatype of the first column of the plan's output.
 *
 * This is stored for ARRAY_SUBLINK execution and for exprType()/exprTypmod(),
 * which have no way to get at the plan associated with a SubPlan node.
 * We really only need the info for EXPR_SUBLINK and ARRAY_SUBLINK subplans,
 * but for consistency we save it always.
 */
static void
get_first_col_type(Plan *plan, Oid *coltype, int32 *coltypmod)
{
	/* In cases such as EXISTS, tlist might be empty; arbitrarily use VOID */
	if (plan->targetlist)
	{
		TargetEntry *tent = (TargetEntry *) linitial(plan->targetlist);

		Assert(IsA(tent, TargetEntry));
		if (!tent->resjunk)
		{
			*coltype = exprType((Node *) tent->expr);
			*coltypmod = exprTypmod((Node *) tent->expr);
			return;
		}
	}
	*coltype = VOIDOID;
	*coltypmod = -1;
}

/**
 * Returns true if query refers to a distributed table.
 */
static bool QueryHasDistributedRelation(Query *q)
{
	ListCell   *rt = NULL;

	foreach(rt, q->rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(rt);

		if (rte->relid != InvalidOid
				&& rte->rtekind == RTE_RELATION)
		{
			GpPolicy *policy = GpPolicyFetch(CurrentMemoryContext, rte->relid);
			bool result = (policy->ptype == POLICYTYPE_PARTITIONED);
			pfree(policy);
			if (result)
			{
				return true;
			}
		}
	}
	return false;
}

typedef struct CorrelatedVarWalkerContext
{
	int maxLevelsUp;
} CorrelatedVarWalkerContext;

/**
 *  Walker finds the deepest correlation nesting i.e. maximum levelsup among all
 *  vars in subquery.
 */
static bool
CorrelatedVarWalker(Node *node, CorrelatedVarWalkerContext *ctx)
{
	Assert(ctx);

	if (node == NULL)
	{
		return false;
	}
	else if (IsA(node, Var))
	{
		Var * v = (Var *) node;
		if (v->varlevelsup > ctx->maxLevelsUp)
		{
			ctx->maxLevelsUp = v->varlevelsup;
		}
		return false;
	}
	else if (IsA(node, Query))
	{
		return query_tree_walker((Query *) node, CorrelatedVarWalker, ctx, 0 /* flags */);
	}

	return expression_tree_walker(node, CorrelatedVarWalker, ctx);
}

/**
 * Returns true if subquery is correlated
 */
bool
IsSubqueryCorrelated(Query *sq)
{
	Assert(sq);
	CorrelatedVarWalkerContext ctx;
	ctx.maxLevelsUp = 0;
	CorrelatedVarWalker((Node *) sq, &ctx);
	return (ctx.maxLevelsUp > 0);
}

/**
 * Returns true if subquery contains references to more than its immediate outer query.
 */
bool
IsSubqueryMultiLevelCorrelated(Query *sq)
{
	Assert(sq);
	CorrelatedVarWalkerContext ctx;
	ctx.maxLevelsUp = 0;
	CorrelatedVarWalker((Node *) sq, &ctx);
	return (ctx.maxLevelsUp > 1);
}

/*
 * Convert a SubLink (as created by the parser) into a SubPlan.
 *
 * We are given the SubLink's contained query, type, and testexpr.  We are
 * also told if this expression appears at top level of a WHERE/HAVING qual.
 *
 * Note: we assume that the testexpr has been AND/OR flattened (actually,
 * it's been through eval_const_expressions), but not converted to
 * implicit-AND form; and any SubLinks in it should already have been
 * converted to SubPlans.  The subquery is as yet untouched, however.
 *
 * The result is whatever we need to substitute in place of the SubLink
 * node in the executable expression.  This will be either the SubPlan
 * node (if we have to do the subplan as a subplan), or a Param node
 * representing the result of an InitPlan, or a row comparison expression
 * tree containing InitPlan Param nodes.
 */
static Node *
make_subplan(PlannerInfo *root, Query *orig_subquery, SubLinkType subLinkType,
			 Node *testexpr, bool isTopQual)
{
	Query	   *subquery;
	double		tuple_fraction = 1.0;

	/*
	 * Copy the source Query node.	This is a quick and dirty kluge to resolve
	 * the fact that the parser can generate trees with multiple links to the
	 * same sub-Query node, but the planner wants to scribble on the Query.
	 * Try to clean this up when we do querytree redesign...
	 */
	subquery = (Query *) copyObject(orig_subquery);

	/*
	 * For an EXISTS subplan, tell lower-level planner to expect that only the
	 * first tuple will be retrieved.  For ALL and ANY subplans, we will be
	 * able to stop evaluating if the test condition fails, so very often not
	 * all the tuples will be retrieved; for lack of a better idea, specify
	 * 50% retrieval.  For EXPR and ROWCOMPARE subplans, use default behavior
	 * (we're only expecting one row out, anyway).
	 *
	 * NOTE: if you change these numbers, also change cost_qual_eval_walker()
	 * and get_initplan_cost() in path/costsize.c.
	 *
	 * XXX If an ALL/ANY subplan is uncorrelated, we may decide to hash or
	 * materialize its result below.  In that case it would've been better to
	 * specify full retrieval.	At present, however, we can only detect
	 * correlation or lack of it after we've made the subplan :-(. Perhaps
	 * detection of correlation should be done as a separate step. Meanwhile,
	 * we don't want to be too optimistic about the percentage of tuples
	 * retrieved, for fear of selecting a plan that's bad for the
	 * materialization case.
	 */
	if (subLinkType == EXISTS_SUBLINK)
		tuple_fraction = 1.0;	/* just like a LIMIT 1 */
	else if (subLinkType == ALL_SUBLINK ||
			 subLinkType == ANY_SUBLINK)
		tuple_fraction = 0.5;	/* 50% */
	else
		tuple_fraction = 0.0;	/* default behavior */

	PlannerConfig *config = CopyPlannerConfig(root->config);

	if ((Gp_role == GP_ROLE_DISPATCH)
			&& IsSubqueryMultiLevelCorrelated(subquery)
			&& QueryHasDistributedRelation(subquery))
	{
		elog(ERROR, "correlated subquery with skip-level correlations is not supported");
	}

	if ((Gp_role == GP_ROLE_DISPATCH)
			&& IsSubqueryCorrelated(subquery)
			&& QueryHasDistributedRelation(subquery))
	{
		/*
		 * Generate the plan for the subquery with certain options disabled.
		 */
		config->gp_enable_direct_dispatch = false;
		config->gp_enable_multiphase_agg = false;

		/*
		 * Only create subplans with sequential scans
		 */
		config->enable_indexscan = false;
		config->enable_bitmapscan = false;
		config->enable_tidscan = false;
		config->enable_seqscan = true;
	}

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		config->gp_cte_sharing = IsSubqueryCorrelated(subquery) ||
				!(subLinkType == ROWCOMPARE_SUBLINK ||
				 subLinkType == ARRAY_SUBLINK ||
				 subLinkType == EXPR_SUBLINK ||
				 subLinkType == EXISTS_SUBLINK);
	}

	PlannerInfo *subroot = NULL;

	Plan *plan = subquery_planner(root->glob, subquery,
			root,
			tuple_fraction,
			&subroot,
			config);

	/* And convert to SubPlan or InitPlan format. */
	Node	   *result = build_subplan(root,
										plan,
										subroot->parse->rtable,
										subLinkType,
										testexpr,
										isTopQual);


	return result;
}

/*
 * Build a SubPlan node given the raw inputs --- subroutine for make_subplan
 *
 * Returns either the SubPlan, or an expression using initplan output Params,
 * as explained in the comments for make_subplan.
 */
static Node *
build_subplan(PlannerInfo *root, Plan *plan, List *rtable,
			  SubLinkType subLinkType, Node *testexpr,
			  bool unknownEqFalse)
{
	Node	   *result;
	SubPlan    *splan;
	Bitmapset  *tmpset;
	int			paramid;
	
	/*
	 * Initialize the SubPlan node.  Note plan_id, plan_name, and cost fields
	 * are set further down.
	 */
	splan = makeNode(SubPlan);
	splan->subLinkType = subLinkType;
    splan->qDispSliceId = 0;             /*CDB*/
	splan->testexpr = NULL;
	splan->paramIds = NIL;
	get_first_col_type(plan, &splan->firstColType, &splan->firstColTypmod);
	splan->useHashTable = false;
	splan->unknownEqFalse = unknownEqFalse;
	splan->is_initplan = false;
	splan->is_multirow = false;
	splan->is_parallelized = false;
	splan->setParam = NIL;
	splan->parParam = NIL;
	splan->args = NIL;
	//splan->extParam = NIL;

	/*
	 * Make parParam and args lists of param IDs and expressions that current
	 * query level will pass to this child plan.
	 */
	tmpset = bms_copy(plan->extParam);
	while ((paramid = bms_first_member(tmpset)) >= 0)
	{
		PlannerParamItem *pitem = list_nth(root->glob->paramlist, paramid);
		Node   *arg;

		if (pitem->abslevel == root->query_level)
		{
			splan->parParam = lappend_int(splan->parParam, paramid);
			/*
			 * The Var or Aggref has already been adjusted to have the correct
			 * varlevelsup or agglevelsup.	We probably don't even need to
			 * copy it again, but be safe.
			 */
			arg = copyObject(pitem->item);

			/*
			 * If it's an Aggref, its arguments might contain SubLinks,
			 * which have not yet been processed.  Do that now.
			 */
			if (IsA(arg, Aggref))
				arg = SS_process_sublinks(root, arg, false);

			splan->args = lappend(splan->args, arg);
		}
		else if (pitem->abslevel < root->query_level)
			splan->extParam = lappend_int(splan->extParam, paramid);
	}
	bms_free(tmpset);

	/*
	 * Un-correlated or undirect correlated plans of EXISTS, EXPR, ARRAY, or
	 * ROWCOMPARE types can be used as initPlans.  For EXISTS, EXPR, or ARRAY,
	 * we just produce a Param referring to the result of evaluating the
	 * initPlan.  For ROWCOMPARE, we must modify the testexpr tree to contain
	 * PARAM_EXEC Params instead of the PARAM_SUBLINK Params emitted by the
	 * parser.
	 */
	if (splan->parParam == NIL && subLinkType == EXISTS_SUBLINK && Gp_role == GP_ROLE_DISPATCH)
	{
		Param	   *prm;

		Assert(testexpr == NULL);
		prm = generate_new_param(root, BOOLOID, -1);
		splan->setParam = list_make1_int(prm->paramid);
		splan->is_initplan = true;
		result = (Node *) prm;
	}
	else if (splan->parParam == NIL && subLinkType == EXPR_SUBLINK && Gp_role == GP_ROLE_DISPATCH)
	{
		TargetEntry *te = linitial(plan->targetlist);
		Param	   *prm;

		Assert(!te->resjunk);
		Assert(testexpr == NULL);
		prm = generate_new_param(root,
								 exprType((Node *) te->expr),
								 exprTypmod((Node *) te->expr));
		splan->setParam = list_make1_int(prm->paramid);
		splan->is_initplan = true;
		result = (Node *) prm;
	}
	else if (splan->parParam == NIL && subLinkType == ARRAY_SUBLINK && Gp_role == GP_ROLE_DISPATCH)
	{
		TargetEntry *te = linitial(plan->targetlist);
		Oid			arraytype;
		Param	   *prm;

		Assert(!te->resjunk);
		Assert(testexpr == NULL);
		arraytype = get_array_type(exprType((Node *) te->expr));
		if (!OidIsValid(arraytype))
			elog(ERROR, "could not find array type for datatype %s",
				 format_type_be(exprType((Node *) te->expr)));
		prm = generate_new_param(root,
								 arraytype,
								 exprTypmod((Node *) te->expr));
		splan->setParam = list_make1_int(prm->paramid);
		splan->is_initplan = true;
		result = (Node *) prm;
	}
	else if (splan->parParam == NIL && subLinkType == ROWCOMPARE_SUBLINK && Gp_role == GP_ROLE_DISPATCH)
	{
		/* Adjust the Params */
		List	   *params;

		params = generate_subquery_params(root,
										  plan->targetlist,
										  &splan->paramIds);
		result = convert_testexpr(root,
								  testexpr,
								  params);
		splan->setParam = list_copy(splan->paramIds);
		splan->is_initplan = true;

		/*
		 * The executable expression is returned to become part of the outer
		 * plan's expression tree; it is not kept in the initplan node.
		 */
	}
	else
	{
		/* Adjust the Params in the testexpr */
		if (testexpr)
		{
			List	   *params;

			params = generate_subquery_params(root,
											  plan->targetlist,
											  &splan->paramIds);
			splan->testexpr = convert_testexpr(root,
											   testexpr,
											   params);
		}
		else
			splan->testexpr = testexpr;

		splan->is_multirow = true; /* CDB: take note. */

		/*
		 * We can't convert subplans of ALL_SUBLINK or ANY_SUBLINK types to
		 * initPlans, even when they are uncorrelated or undirect correlated,
		 * because we need to scan the output of the subplan for each outer
		 * tuple.  But if it's a not-direct-correlated IN (= ANY) test, we
		 * might be able to use a hashtable to avoid comparing all the tuples.
		 * TODO siva - I believe we should've pulled these up to be NL joins.
		 * We may want to assert that this is never exercised.
		 */
		if (subLinkType == ANY_SUBLINK &&
			splan->parParam == NIL &&
			subplan_is_hashable(root, plan) &&
			testexpr_is_hashable(splan->testexpr))
			splan->useHashTable = true;

		result = (Node *) splan;
	}

	AssertEquivalent(splan->is_initplan, !splan->is_multirow && splan->parParam == NIL);

	/*
	 * Add the subplan and its rtable to the global lists.
	 */
	root->glob->subplans = lappend(root->glob->subplans,
								   plan);
	root->glob->subrtables = lappend(root->glob->subrtables,
									 rtable);
	splan->plan_id = list_length(root->glob->subplans);

	if (splan->is_initplan)
		root->init_plans = lappend(root->init_plans, splan);

	/*
	 * A parameterless subplan (not initplan) should be prepared to handle
	 * REWIND efficiently.	If it has direct parameters then there's no point
	 * since it'll be reset on each scan anyway; and if it's an initplan then
	 * there's no point since it won't get re-run without parameter changes
	 * anyway.	The input of a hashed subplan doesn't need REWIND either.
	 */
	if (splan->parParam == NIL && !splan->is_initplan && !splan->useHashTable)
		root->glob->rewindPlanIDs = bms_add_member(root->glob->rewindPlanIDs,
												   splan->plan_id);

	/* Label the subplan for EXPLAIN purposes */
	if (splan->is_initplan)
	{
		ListCell   *lc;
		
		StringInfo buf = makeStringInfo();
		
		appendStringInfo(buf, "InitPlan %d (returns ", splan->plan_id);
		
		foreach(lc, splan->setParam)
		{
			appendStringInfo(buf, "$%d%s",
							lfirst_int(lc),
							lnext(lc) ? "," : "");
		}
		appendStringInfoString(buf, ")");
		splan->plan_name = pstrdup(buf->data);
		pfree(buf->data);
		pfree(buf);
		buf = NULL;
	}
	else
	{
		StringInfo buf = makeStringInfo();
		appendStringInfo(buf, "SubPlan %d", splan->plan_id);
		splan->plan_name = pstrdup(buf->data);
		pfree(buf->data);
		pfree(buf);
		buf = NULL;
	}

	return result;
}

/*
 * generate_subquery_params: build a list of Params representing the output
 * columns of a sublink's sub-select, given the sub-select's targetlist.
 *
 * We also return an integer list of the paramids of the Params.
 */
static List *
generate_subquery_params(PlannerInfo *root, List *tlist, List **paramIds)
{
	List	   *result;
	List	   *ids;
	ListCell   *lc;

	result = ids = NIL;
	foreach(lc, tlist)
	{
		TargetEntry *tent = (TargetEntry *) lfirst(lc);
		Param	   *param;

		if (tent->resjunk)
			continue;

		param = generate_new_param(root,
								   exprType((Node *) tent->expr),
								   exprTypmod((Node *) tent->expr));
		result = lappend(result, param);
		ids = lappend_int(ids, param->paramid);
	}

	*paramIds = ids;
	return result;
}

/*
 * generate_subquery_vars: build a list of Vars representing the output
 * columns of a sublink's sub-select, given the sub-select's targetlist.
 * The Vars have the specified varno (RTE index).
 */
List *
generate_subquery_vars(PlannerInfo *root, List *tlist, Index varno)
{
	List	   *result;
	ListCell   *lc;

	result = NIL;
	foreach(lc, tlist)
	{
		TargetEntry *tent = (TargetEntry *) lfirst(lc);
		Var		   *var;

		if (tent->resjunk)
			continue;

		var = makeVar(varno,
					  tent->resno,
					  exprType((Node *) tent->expr),
					  exprTypmod((Node *) tent->expr),
					  0);
		result = lappend(result, var);
	}

	return result;
}

/*
 * convert_testexpr: convert the testexpr given by the parser into
 * actually executable form.  This entails replacing PARAM_SUBLINK Params
 * with Params or Vars representing the results of the sub-select.  The
 * nodes to be substituted are passed in as the List result from
 * generate_subquery_params or generate_subquery_vars.
 *
 * The given testexpr has already been recursively processed by
 * process_sublinks_mutator.  Hence it can no longer contain any
 * PARAM_SUBLINK Params for lower SubLink nodes; we can safely assume that
 * any we find are for our own level of SubLink.
 */
Node *
convert_testexpr(PlannerInfo *root,
				 Node *testexpr,
				 List *subst_nodes)
{
	convert_testexpr_context context;

	context.root = root;
	context.subst_nodes = subst_nodes;
	return convert_testexpr_mutator(testexpr, &context);
}

static Node *
convert_testexpr_mutator(Node *node,
						 convert_testexpr_context *context)
{
	if (node == NULL)
		return NULL;
	if (IsA(node, Param))
	{
		Param	   *param = (Param *) node;

		if (param->paramkind == PARAM_SUBLINK)
		{
			if (param->paramid <= 0 ||
				param->paramid > list_length(context->subst_nodes))
				elog(ERROR, "unexpected PARAM_SUBLINK ID: %d", param->paramid);

			/*
			 * We copy the list item to avoid having doubly-linked
			 * substructure in the modified parse tree.  This is probably
			 * unnecessary when it's a Param, but be safe.
			 */
			return (Node *) copyObject(list_nth(context->subst_nodes,
												param->paramid - 1));
		}
	}
	return expression_tree_mutator(node,
								   convert_testexpr_mutator,
								   (void *) context);
}

/*
 * subplan_is_hashable: can we implement an ANY subplan by hashing?
 */
static bool
subplan_is_hashable(PlannerInfo *root, Plan *plan)
{
	double		subquery_size;

	/*
	 * The estimated size of the subquery result must fit in work_mem. (Note:
	 * we use sizeof(HeapTupleHeaderData) here even though the tuples will
	 * actually be stored as MinimalTuples; this provides some fudge factor
	 * for hashtable overhead.)
	 */
	subquery_size = plan->plan_rows *
		(MAXALIGN(plan->plan_width) + MAXALIGN(sizeof(HeapTupleHeaderData)));
	if (subquery_size > global_work_mem(root))
		return false;

	return true;
}

/*
 * testexpr_is_hashable: is an ANY SubLink's test expression hashable?
 */
static bool
testexpr_is_hashable(Node *testexpr)
{
	/*
	 * The testexpr must be a single OpExpr, or an AND-clause containing
	 * only OpExprs.
	 *
	 * The combining operators must be hashable and strict. The need for
	 * hashability is obvious, since we want to use hashing. Without
	 * strictness, behavior in the presence of nulls is too unpredictable.	We
	 * actually must assume even more than plain strictness: they can't yield
	 * NULL for non-null inputs, either (see nodeSubplan.c).  However, hash
	 * indexes and hash joins assume that too.
	 */
	if (testexpr && IsA(testexpr, OpExpr))
	{
		if (hash_ok_operator((OpExpr *) testexpr))
			return true;
	}
	else if (and_clause(testexpr))
	{
		ListCell   *l;

		foreach(l, ((BoolExpr *) testexpr)->args)
		{
			Node	   *andarg = (Node *) lfirst(l);

			if (!IsA(andarg, OpExpr))
				return false;
			if (!hash_ok_operator((OpExpr *) andarg))
				return false;
		}
		return true;
	}

	return false;
}

static bool
hash_ok_operator(OpExpr *expr)
{
	Oid			opid = expr->opno;
	HeapTuple	tup;
	Form_pg_operator optup;

	tup = SearchSysCache(OPEROID,
						 ObjectIdGetDatum(opid),
						 0, 0, 0);
	if (!HeapTupleIsValid(tup))
		elog(ERROR, "cache lookup failed for operator %u", opid);
	optup = (Form_pg_operator) GETSTRUCT(tup);
	if (!optup->oprcanhash || !func_strict(optup->oprcode))
	{
		ReleaseSysCache(tup);
		return false;
	}
	ReleaseSysCache(tup);
	return true;
}

/*
 * convert_IN_to_join: can we convert an IN SubLink to join style?
 *
 * The caller has found a SubLink at the top level of WHERE, but has not
 * checked the properties of the SubLink at all.  Decide whether it is
 * appropriate to process this SubLink in join style.  If not, return the
 * original sublink unmodified.
 * If so, build the qual clause(s) to replace the SubLink, and return them.
 *
 * Side effects of a successful conversion include adding the SubLink's
 * subselect to the query's rangetable and adding an InClauseInfo node to
 * its in_info_list.
 *
 * Upon creating an RTE for a flattened subquery, a corresponding RangeTblRef
 * node is appended to the caller's List referenced by *rtrlist_inout, so the
 * caller can add it to the jointree.
 */
Node *
convert_IN_to_join(PlannerInfo *root, List **rtrlist_inout, SubLink *sublink)
{
	Query	   *parse = root->parse;
	Query	   *subselect = (Query *) sublink->subselect;
	List	   *in_operators;
	List	   *left_exprs = NIL;
	List	   *right_exprs = NIL;
	Relids		left_varnos;
	int			rtindex;
	RangeTblEntry *rte;
	RangeTblRef *rtr;
	List	   *subquery_vars;
	InClauseInfo *ininfo;
	bool		correlated;
	Node	   *result;

	Assert(IsA(subselect, Query));

	cdbsubselect_drop_orderby(subselect);
	cdbsubselect_drop_distinct(subselect);

	/*
	 * The combining operators and left-hand expressions mustn't be volatile.
	 */
	if (contain_volatile_functions(sublink->testexpr))
		return (Node *) sublink;

	/*
	 * If subquery returns a set-returning function (SRF) in the targetlist, we
	 * do not attempt to convert the IN to a join.
	 */
	
	if (expression_returns_set((Node *) subselect->targetList))
		return (Node *) sublink;

	/*
	 * If deeply correlated, then don't pull it up
	 */
	if (IsSubqueryMultiLevelCorrelated(subselect))
		return (Node *) sublink;

	/*
	 * If there are CTEs, then the transformation does not work. Don't attempt
	 * to pullup.
	 */
	if (parse->cteList)
		return (Node *) sublink;

	/*
	 * If uncorrelated, and no Var nodes on lhs, the subquery will be executed
	 * only once.  It should become an InitPlan, but make_subplan() doesn't
	 * handle that case, so just flatten it for now.
	 * CDB TODO: Let it become an InitPlan, so its QEs can be recycled.
	 */
    correlated = contain_vars_of_level_or_above(sublink->subselect, 1);

    if (correlated)
    {
		/*
		 * Under certain conditions, we cannot pull up the subquery as a join.
		 */
    	if (subselect->hasAggs
    			|| (subselect->jointree->fromlist == NULL)
    			|| subselect->havingQual
    			|| subselect->groupClause
    			|| subselect->hasWindFuncs
    			|| subselect->distinctClause
    			|| subselect->setOperations)
    		return (Node *) sublink;
    	
		/*
		 * Do not pull subqueries with correlation in a func expr in the from
		 * clause of the subselect
		 */
    	if (has_correlation_in_funcexpr_rte(subselect->rtable))
    		return (Node *) sublink;

    	if (contain_subplans(subselect->jointree->quals))
    		return (Node *) sublink;
    }

	/*
	 * Okay, pull up the sub-select into top range table and jointree.
	 *
	 * We rely here on the assumption that the outer query has no references
	 * to the inner (necessarily true, other than the Vars that we build
	 * below). Therefore this is a lot easier than what pull_up_subqueries has
	 * to go through.
	 */
	rte = addRangeTableEntryForSubquery(NULL,
										subselect,
										makeAlias("IN_subquery", NIL),
										false);
	parse->rtable = lappend(parse->rtable, rte);
	rtindex = list_length(parse->rtable);
	rtr = makeNode(RangeTblRef);
	rtr->rtindex = rtindex;

	/* Tell caller to augment the jointree with a reference to the new RTE. */
	*rtrlist_inout = lappend(*rtrlist_inout, rtr);

	/*
	 * Build a list of Vars representing the subselect outputs.
	 */
	subquery_vars = generate_subquery_vars(root,
										   subselect->targetList,
										   rtindex);

	/*
	 * Build the result qual expression, replacing Params with these Vars.
	 */
	result = convert_testexpr(root,
							  sublink->testexpr,
							  subquery_vars);

	/*
	 * Now build the InClauseInfo node.
	 */
	ininfo = makeNode(InClauseInfo);
	ininfo->sub_targetlist = NULL;
	ininfo->righthand = bms_make_singleton(rtindex);

	/*
	 * Uncorrelated "=ANY" subqueries can use JOIN_UNIQUE dedup technique.  We
	 * expect that the test expression will be either a single OpExpr, or an
	 * AND-clause containing OpExprs.  (If it's anything else then the parser
	 * must have determined that the operators have non-equality-like
	 * semantics.  In the OpExpr case we can't be sure what the operator's
	 * semantics are like, and must check for ourselves.)
	 */
	ininfo->try_join_unique = false;
	in_operators = NIL;
	if (!correlated && sublink->testexpr)
	{
		ininfo->try_join_unique = true;
		if (IsA(sublink->testexpr, OpExpr))
	    {
			OpExpr 	   *op = (OpExpr *) sublink->testexpr;
			Oid			opno = op->opno;
		    List	   *opfamilies;
		    List	   *opstrats;

		    get_op_btree_interpretation(opno, &opfamilies, &opstrats);
		    if (!list_member_int(opstrats, ROWCOMPARE_EQ))
			    ininfo->try_join_unique = false;
			if (list_length(op->args) != 2)
				return (Node *) sublink;
			else
			{
				left_exprs = list_make1(linitial(op->args));
				right_exprs = list_make1(lsecond(op->args));
			}
			in_operators = list_make1_oid(opno);
	    }
		else if (and_clause(sublink->testexpr))
		{
			ListCell   *lc;

			/* OK, but we need to extract the per-column operator OIDs */
			in_operators = left_exprs = right_exprs = NIL;
			foreach(lc, ((BoolExpr *) sublink->testexpr)->args)
			{
				OpExpr *op = (OpExpr *) lfirst(lc);

				if (!IsA(op, OpExpr))           /* probably shouldn't happen */
					ininfo->try_join_unique = false;
				if (list_length(op->args) != 2)
					return (Node *) sublink;
				else
				{
					left_exprs = lappend(left_exprs, linitial(op->args));
					right_exprs = lappend(right_exprs, lsecond(op->args));
				}
				in_operators = lappend_oid(in_operators, op->opno);
			}
		}
		else
			ininfo->try_join_unique = false;
    }

	ininfo->in_operators = in_operators;

	/*
	 * The left-hand expressions must contain some Vars of the current query,
	 * else it's not gonna be a join.
	 */
	left_varnos = pull_varnos((Node *) left_exprs);
	ininfo->lefthand = left_varnos;

	/*
	 * ininfo->sub_targetlist must be filled with a list of expressions that
	 * would need to be unique-ified if we try to implement the IN using a
	 * regular join to unique-ified subquery output.  This is most easily done
	 * by applying convert_testexpr to just the RHS inputs of the testexpr
	 * operators.  That handles cases like type coercions of the subquery
	 * outputs, clauses dropped due to const-simplification, etc.
	 */
	ininfo->sub_targetlist = (List *) convert_testexpr(root,
													   (Node *) right_exprs,
													   subquery_vars);

	/* Add the completed node to the query's list */
	root->in_info_list = lappend(root->in_info_list, ininfo);

	return result;
}

/*
 * Replace correlation vars (uplevel vars) with Params.
 *
 * Uplevel aggregates are replaced, too.
 *
 * Note: it is critical that this runs immediately after SS_process_sublinks.
 * Since we do not recurse into the arguments of uplevel aggregates, they will
 * get copied to the appropriate subplan args list in the parent query with
 * uplevel vars not replaced by Params, but only adjusted in level (see
 * replace_outer_agg).	That's exactly what we want for the vars of the parent
 * level --- but if an aggregate's argument contains any further-up variables,
 * they have to be replaced with Params in their turn.	That will happen when
 * the parent level runs SS_replace_correlation_vars.  Therefore it must do
 * so after expanding its sublinks to subplans.  And we don't want any steps
 * in between, else those steps would never get applied to the aggregate
 * argument expressions, either in the parent or the child level.
 *
 * Another fairly tricky thing going on here is the handling of SubLinks in
 * the arguments of uplevel aggregates.  Those are not touched inside the
 * intermediate query level, either.  Instead, SS_process_sublinks recurses
 * on them after copying the Aggref expression into the parent plan level
 * (this is actually taken care of in make_subplan).
 */
Node *
SS_replace_correlation_vars(PlannerInfo *root, Node *expr)
{
	/* No setup needed for tree walk, so away we go */
	return replace_correlation_vars_mutator(expr, root);
}

static Node *
replace_correlation_vars_mutator(Node *node, PlannerInfo *root)
{
	if (node == NULL)
		return NULL;
	if (IsA(node, Var))
	{
		if (((Var *) node)->varlevelsup > 0)
			return (Node *) replace_outer_var(root, (Var *) node);
	}
	if (IsA(node, Aggref))
	{
		if (((Aggref *) node)->agglevelsup > 0)
			return (Node *) replace_outer_agg(root, (Aggref *) node);
	}
	return expression_tree_mutator(node,
								   replace_correlation_vars_mutator,
								   (void *) root);
}

/*
 * Expand SubLinks to SubPlans in the given expression.
 *
 * The isQual argument tells whether or not this expression is a WHERE/HAVING
 * qualifier expression.  If it is, any sublinks appearing at top level need
 * not distinguish FALSE from UNKNOWN return values.
 */
Node *
SS_process_sublinks(PlannerInfo *root, Node *expr, bool isQual)
{
	process_sublinks_context context;

	context.root = root;
	context.isTopQual = isQual;
	return process_sublinks_mutator(expr, &context);
}

static Node *
process_sublinks_mutator(Node *node, process_sublinks_context *context)
{
	process_sublinks_context locContext;

	locContext.root = context->root;

	if (node == NULL)
		return NULL;
	if (IsA(node, SubLink))
	{
		SubLink    *sublink = (SubLink *) node;
		Node	   *testexpr;

		/*
		 * First, recursively process the lefthand-side expressions, if any.
		 * They're not top-level anymore.
		 */
		locContext.isTopQual = false;
		testexpr = process_sublinks_mutator(sublink->testexpr, &locContext);

		/*
		 * Now build the SubPlan node and make the expr to return.
		 */
		return make_subplan(context->root,
							(Query *) sublink->subselect,
							sublink->subLinkType,
							testexpr,
							context->isTopQual);
	}

	/*
	 * Don't recurse into the arguments of an outer aggregate here.
	 * Any SubLinks in the arguments have to be dealt with at the outer
	 * query level; they'll be handled when make_subplan collects the
	 * Aggref into the arguments to be passed down to the current subplan.
	 */
	if (IsA(node, Aggref))
	{
		if (((Aggref *) node)->agglevelsup > 0)
			return node;
	}

	/*
	 * We should never see a SubPlan expression in the input (since this is
	 * the very routine that creates 'em to begin with).  We shouldn't find
	 * ourselves invoked directly on a Query, either.
	 */
	Assert(!is_subplan(node));
	Assert(!IsA(node, Query));

	/*
	 * Because make_subplan() could return an AND or OR clause, we have to
	 * take steps to preserve AND/OR flatness of a qual.  We assume the input
	 * has been AND/OR flattened and so we need no recursion here.
	 *
	 * If we recurse down through anything other than an AND node, we are
	 * definitely not at top qual level anymore.  (Due to the coding here, we
	 * will not get called on the List subnodes of an AND, so no check is
	 * needed for List.)
	 */
	if (and_clause(node))
	{
		List	   *newargs = NIL;
		ListCell   *l;

		/* Still at qual top-level */
		locContext.isTopQual = context->isTopQual;

		foreach(l, ((BoolExpr *) node)->args)
		{
			Node	   *newarg;

			newarg = process_sublinks_mutator(lfirst(l), &locContext);
			if (and_clause(newarg))
				newargs = list_concat(newargs, ((BoolExpr *) newarg)->args);
			else
				newargs = lappend(newargs, newarg);
		}
		return (Node *) make_andclause(newargs);
	}

	/* otherwise not at qual top-level */
	locContext.isTopQual = false;

	if (or_clause(node))
	{
		List	   *newargs = NIL;
		ListCell   *l;

		foreach(l, ((BoolExpr *) node)->args)
		{
			Node	   *newarg;

			newarg = process_sublinks_mutator(lfirst(l), &locContext);
			if (or_clause(newarg))
				newargs = list_concat(newargs, ((BoolExpr *) newarg)->args);
			else
				newargs = lappend(newargs, newarg);
		}
		return (Node *) make_orclause(newargs);
	}

	return expression_tree_mutator(node,
								   process_sublinks_mutator,
								   (void *) &locContext);
}

/*
 * SS_finalize_plan - do final sublink processing for a completed Plan.
 * Input:
 * 	root - PlannerInfo structure that is necessary for walking the tree
 * 	rtable - list of rangetable entries to look at for relids
 * 	attach_initplans - attach all initplans to the top plan node from root
 * Output:
 * 	plan->extParam and plan->allParam - attach params to top of the plan
 */
void
SS_finalize_plan(PlannerInfo *root, Plan *plan, bool attach_initplans)
{
	Bitmapset  *valid_params,
			   *initExtParam,
			   *initSetParam;
	Cost		initplan_cost;
	int			paramid;
	ListCell   *l;

	/*
	 * First, scan the param list to discover the sets of params that are
	 * available from outer query levels and my own query level. We do this
	 * once to save time in the per-plan recursion steps.  (This calculation
	 * is overly generous: it can include a lot of params that actually
	 * shouldn't be referenced here.  However, valid_params is just used as
	 * a debugging crosscheck, so it's not worth trying to be exact.)
	 */
	valid_params = NULL;
	paramid = 0;
	foreach(l, root->glob->paramlist)
	{
		PlannerParamItem *pitem = (PlannerParamItem *) lfirst(l);

		if (pitem->abslevel < root->query_level)
		{
			/* valid outer-level parameter */
			valid_params = bms_add_member(valid_params, paramid);
		}
		else if (pitem->abslevel == root->query_level &&
				 IsA(pitem->item, Param))
		{
			/* valid local parameter (i.e., a setParam of my child) */
			valid_params = bms_add_member(valid_params, paramid);
		}

		paramid++;
	}

	/*
	 * Now recurse through plan tree.
	 */
	(void) finalize_plan(root, plan, valid_params);

	bms_free(valid_params);

	/*
	 * Finally, attach any initPlans to the topmost plan node, and add their
	 * extParams to the topmost node's, too.  However, any setParams of the
	 * initPlans should not be present in the topmost node's extParams, only
	 * in its allParams.  (As of PG 8.1, it's possible that some initPlans
	 * have extParams that are setParams of other initPlans, so we have to
	 * take care of this situation explicitly.)
	 *
	 * We also add the eval cost of each initPlan to the startup cost of the
	 * top node.  This is a conservative overestimate, since in fact each
	 * initPlan might be executed later than plan startup, or even not at all.
	 */
	if (attach_initplans)
	{
		Insist(!plan->initPlan);
		plan->initPlan = root->init_plans;
		root->init_plans = NIL;		/* make sure they're not attached twice */

		initExtParam = initSetParam = NULL;
		initplan_cost = 0;

		foreach(l, plan->initPlan)
		{
			SubPlan    *initsubplan = (SubPlan *) lfirst(l);
			Plan	   *initplan = planner_subplan_get_plan(root, initsubplan);
			ListCell   *l2;

			initExtParam = bms_add_members(initExtParam, initplan->extParam);
			foreach(l2, initsubplan->setParam)
			{
				SubPlan    *initplan = (SubPlan *) lfirst(l);
				Plan	   *subplan_plan = planner_subplan_get_plan(root, initplan);
				ListCell   *l2;

				initExtParam = bms_add_members(initExtParam, subplan_plan->extParam);
				foreach(l2, initplan->setParam)
				{
					initSetParam = bms_add_member(initSetParam, lfirst_int(l2));
				}
				initplan_cost += subplan_plan->total_cost;
			}
			initplan_cost += get_initplan_cost(root, initsubplan);
		}
		/* allParam must include all these params */
		plan->allParam = bms_add_members(plan->allParam, initExtParam);
		plan->allParam = bms_add_members(plan->allParam, initSetParam);
		/* extParam must include any child extParam */
		plan->extParam = bms_add_members(plan->extParam, initExtParam);
		/* but extParam shouldn't include any setParams */
		plan->extParam = bms_del_members(plan->extParam, initSetParam);
		/* ensure extParam is exactly NULL if it's empty */
		if (bms_is_empty(plan->extParam))
			plan->extParam = NULL;

		plan->startup_cost += initplan_cost;
		plan->total_cost += initplan_cost;
	}
}

/*
 * Recursive processing of all nodes in the plan tree
 *
 * The return value is the computed allParam set for the given Plan node.
 * This is just an internal notational convenience.
 */
static Bitmapset *
finalize_plan(PlannerInfo *root, Plan *plan, Bitmapset *valid_params)
{
	finalize_primnode_context context;

	if (plan == NULL)
		return NULL;

	context.root = root;
	context.paramids = NULL;	/* initialize set to empty */

	/*
	 * When we call finalize_primnode, context.paramids sets are automatically
	 * merged together.  But when recursing to self, we have to do it the hard
	 * way.  We want the paramids set to include params in subplans as well as
	 * at this level.
	 */

	/* Find params in targetlist and qual */
	finalize_primnode((Node *) plan->targetlist, &context);
	finalize_primnode((Node *) plan->qual, &context);

	/* Check additional node-type-specific fields */
	switch (nodeTag(plan))
	{
		case T_Result:
			finalize_primnode(((Result *) plan)->resconstantqual,
							  &context);
			break;

		case T_IndexScan:
			finalize_primnode((Node *) ((IndexScan *) plan)->indexqual,
							  &context);

			/*
			 * we need not look at indexqualorig, since it will have the same
			 * param references as indexqual.
			 */
			break;

		case T_BitmapIndexScan:
			finalize_primnode((Node *) ((BitmapIndexScan *) plan)->indexqual,
							  &context);

			/*
			 * we need not look at indexqualorig, since it will have the same
			 * param references as indexqual.
			 */
			break;

		case T_BitmapHeapScan:
			finalize_primnode((Node *) ((BitmapHeapScan *) plan)->bitmapqualorig,
							  &context);
			break;

		case T_BitmapAppendOnlyScan:
			finalize_primnode((Node *) ((BitmapAppendOnlyScan *) plan)->bitmapqualorig,
							  &context);
			break;

		case T_BitmapTableScan:
			finalize_primnode((Node *) ((BitmapTableScan *) plan)->bitmapqualorig,
							  &context);
			break;

		case T_TidScan:
			finalize_primnode((Node *) ((TidScan *) plan)->tidquals,
							  &context);
			break;

		case T_SubqueryScan:
			/*
			 * In a SubqueryScan, SS_finalize_plan has already been run on the
			 * subplan by the inner invocation of subquery_planner, so there's
			 * no need to do it again.	Instead, just pull out the subplan's
			 * extParams list, which represents the params it needs from my
			 * level and higher levels.
			 */
			context.paramids = bms_add_members(context.paramids,
								 ((SubqueryScan *) plan)->subplan->extParam);
			break;

		case T_TableFunctionScan:
			{
				RangeTblEntry *rte;

				rte = rt_fetch(((TableFunctionScan *) plan)->scan.scanrelid,
							   root->parse->rtable);
				Assert(rte->rtekind == RTE_TABLEFUNCTION);
				finalize_primnode(rte->funcexpr, &context);
			}
			/* TableFunctionScan's lefttree is like SubqueryScan's subplan. */
			context.paramids = bms_add_members(context.paramids,
								 plan->lefttree->extParam);
			break;

		case T_FunctionScan:
			finalize_primnode(((FunctionScan *) plan)->funcexpr,
							  &context);
			break;

		case T_ValuesScan:
			finalize_primnode((Node *) ((ValuesScan *) plan)->values_lists,
							  &context);
			break;

		case T_Append:
			{
				ListCell   *l;

				foreach(l, ((Append *) plan)->appendplans)
				{
					context.paramids =
						bms_add_members(context.paramids,
										finalize_plan(root,
													  (Plan *) lfirst(l),
													  valid_params));
				}
			}
			break;

		case T_BitmapAnd:
			{
				ListCell   *l;

				foreach(l, ((BitmapAnd *) plan)->bitmapplans)
				{
					context.paramids =
						bms_add_members(context.paramids,
										finalize_plan(root,
													  (Plan *) lfirst(l),
													  valid_params));
				}
			}
			break;

		case T_BitmapOr:
			{
				ListCell   *l;

				foreach(l, ((BitmapOr *) plan)->bitmapplans)
				{
					context.paramids =
						bms_add_members(context.paramids,
										finalize_plan(root,
													  (Plan *) lfirst(l),
													  valid_params));
				}
			}
			break;

		case T_NestLoop:
			finalize_primnode((Node *) ((Join *) plan)->joinqual,
							  &context);
			break;

		case T_MergeJoin:
			finalize_primnode((Node *) ((Join *) plan)->joinqual,
							  &context);
			finalize_primnode((Node *) ((MergeJoin *) plan)->mergeclauses,
							  &context);
			break;

		case T_HashJoin:
			finalize_primnode((Node *) ((Join *) plan)->joinqual,
							  &context);
			finalize_primnode((Node *) ((HashJoin *) plan)->hashclauses,
							  &context);
			finalize_primnode((Node *) ((HashJoin *) plan)->hashqualclauses,
							  &context);
			break;

		case T_Motion:

			finalize_primnode((Node *) ((Motion *) plan)->hashExpr,
							  &context);
			break;

		case T_Limit:
			finalize_primnode(((Limit *) plan)->limitOffset,
							  &context);
			finalize_primnode(((Limit *) plan)->limitCount,
							  &context);
			break;

		case T_Hash:
		case T_Agg:
		case T_Window:
		case T_SeqScan:
		case T_ExternalScan:
		case T_Material:
		case T_Sort:
		case T_ShareInputScan:
		case T_Unique:
		case T_SetOp:
		case T_Repeat:
			break;

		default:
            Assert(false);
			elog(ERROR, "unrecognized node type: %d",
				 (int) nodeTag(plan));
	}

	/* Process left and right child plans, if any */
	/*
	 * In a TableFunctionScan, the 'lefttree' is more like a SubQueryScan's
	 * subplan, and contains a plan that's already been finalized by the
	 * inner invocation of subquery_planner(). So skip that.
	 */
	if (!IsA(plan, TableFunctionScan))
		context.paramids = bms_add_members(context.paramids,
										   finalize_plan(root,
														 plan->lefttree,
														 valid_params));

	context.paramids = bms_add_members(context.paramids,
									   finalize_plan(root,
													 plan->righttree,
													 valid_params));

	/* Now we have all the paramids */

	if (!bms_is_subset(context.paramids, valid_params))
		elog(ERROR, "plan should not reference subplan's variable");

	/*
	 * Note: by definition, extParam and allParam should have the same value
	 * in any plan node that doesn't have child initPlans.  We set them
	 * equal here, and later SS_finalize_plan will update them properly
	 * in node(s) that it attaches initPlans to.
	 *
	 * For speed at execution time, make sure extParam/allParam are actually
	 * NULL if they are empty sets.
	 */
	if (bms_is_empty(context.paramids))
	{
		plan->extParam = NULL;
		plan->allParam = NULL;
	}
	else
	{
		plan->extParam = context.paramids;
		plan->allParam = bms_copy(context.paramids);
	}

	return plan->allParam;
}

/*
 * finalize_primnode: add IDs of all PARAM_EXEC params appearing in the given
 * expression tree to the result set.
 */
static bool
finalize_primnode(Node *node, finalize_primnode_context *context)
{
	if (node == NULL)
		return false;
	if (IsA(node, Param))
	{
		if (((Param *) node)->paramkind == PARAM_EXEC)
		{
			int			paramid = ((Param *) node)->paramid;

			context->paramids = bms_add_member(context->paramids, paramid);
		}
		return false;			/* no more to do here */
	}
	if (is_subplan(node))
	{
		SubPlan    *subplan = (SubPlan *) node;
		Plan	   *plan = planner_subplan_get_plan(context->root, subplan);
		ListCell   *lc;
		Bitmapset  *subparamids;

		/* Recurse into the testexpr, but not into the Plan */
		finalize_primnode(subplan->testexpr, context);

		/*
		 * Remove any param IDs of output parameters of the subplan that were
		 * referenced in the testexpr.  These are not interesting for
		 * parameter change signaling since we always re-evaluate the subplan.
		 * Note that this wouldn't work too well if there might be uses of the
		 * same param IDs elsewhere in the plan, but that can't happen because
		 * generate_new_param never tries to merge params.
		 */
		foreach(lc, subplan->paramIds)
		{
			context->paramids = bms_del_member(context->paramids,
											   lfirst_int(lc));
		}

		/* Also examine args list */
		finalize_primnode((Node *) subplan->args, context);

		/*
		 * Add params needed by the subplan to paramids, but excluding those
		 * we will pass down to it.
		 */
		subparamids = bms_copy(plan->extParam);
		foreach(lc, subplan->parParam)
		{
			subparamids = bms_del_member(subparamids, lfirst_int(lc));
		}
		context->paramids = bms_join(context->paramids, subparamids);

		return false;			/* no more to do here */
	}
	return expression_tree_walker(node, finalize_primnode,
								  (void *) context);
}

/*
 * SS_make_initplan_from_plan - given a plan tree, make it an InitPlan
 *
 * The plan is expected to return a scalar value of the indicated type.
 * We build an EXPR_SUBLINK SubPlan node and put it into the initplan
 * list for the current query level.  A Param that represents the initplan's
 * output is returned.
 *
 * We assume the plan hasn't been put through SS_finalize_plan.
 *
 * We treat root->init_plans like the old PlannerInitPlan global here.
 */
Param *
SS_make_initplan_from_plan(PlannerInfo *root, Plan *plan,
						   Oid resulttype, int32 resulttypmod)
{
	SubPlan    *node;
	Param	   *prm;

	/*
	 * We must run SS_finalize_plan(), since that's normally done before a
	 * subplan gets put into the initplan list.  Tell it not to attach any
	 * pre-existing initplans to this one, since they are siblings not
	 * children of this initplan.  (This is something else that could perhaps
	 * be cleaner if we did extParam/allParam processing in setrefs.c instead
	 * of here?  See notes for materialize_finished_plan.)
	 */
	
	/*
	 * Build extParam/allParam sets for plan nodes.
	 */
	SS_finalize_plan(root, plan, false);
	
	/*
	 * Add the subplan and its rtable to the global lists.
	 */
	root->glob->subplans = lappend(root->glob->subplans,
								   plan);
	root->glob->subrtables = lappend(root->glob->subrtables,
									 root->parse->rtable);

	/*
	 * Create a SubPlan node and add it to the outer list of InitPlans.
	 */
	node = makeNode(SubPlan);
	node->subLinkType = EXPR_SUBLINK;
	get_first_col_type(plan, &node->firstColType, &node->firstColTypmod);
    node->qDispSliceId = 0;             /*CDB*/
	node->plan_id = list_length(root->glob->subplans);
	node->is_initplan = true;
	root->init_plans = lappend(root->init_plans, node);

	/*
	 * The node can't have any inputs (since it's an initplan), so the
	 * parParam and args lists remain empty.
	 */
	
	/* NB PostgreSQL calculates subplan cost here, but GPDB does it elsewhere. */

	/*
	 * Make a Param that will be the subplan's output.
	 */
	prm = generate_new_param(root, resulttype, resulttypmod);
	node->setParam = list_make1_int(prm->paramid);
	
	/* Label the subplan for EXPLAIN purposes */
	StringInfo buf = makeStringInfo();
	appendStringInfo(buf, "InitPlan %d (returns $%d)",
			node->plan_id, prm->paramid);
	node->plan_name = pstrdup(buf->data);
	pfree(buf->data);
	pfree(buf);
	buf = NULL;

	return prm;
}
