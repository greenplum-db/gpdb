/*-------------------------------------------------------------------------
 *
 * cdbllize.c
 *	  Parallelize a PostgreSQL sequential plan tree.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbllize.c
 *
 * NOTES
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"
#include "optimizer/clauses.h"
#include "optimizer/pathnode.h"
#include "nodes/primnodes.h"
#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"
#include "nodes/pg_list.h"
#include "nodes/print.h"

#include "optimizer/paths.h"
#include "optimizer/planmain.h" /* for is_projection_capable_plan() */
#include "optimizer/var.h"		/* for contain_vars_of_level_or_above() */
#include "parser/parsetree.h"	/* for rt_fetch() */
#include "utils/lsyscache.h"	/* for getatttypetypmod() */
#include "parser/parse_oper.h"	/* for compatible_oper_opid() */
#include "nodes/makefuncs.h"	/* for makeVar() */
#include "nodes/value.h"		/* for makeString() */
#include "utils/relcache.h"		/* RelationGetPartitioningKey() */

#include "cdb/cdbvars.h"
#include "cdb/cdbplan.h"
#include "cdb/cdbpullup.h"
#include "cdb/cdbllize.h"
#include "cdb/cdbmutate.h"
#include "optimizer/tlist.h"

#define ARRAYCOPY(to, from, sz) \
	do { \
		Size	_size = (sz) * sizeof((to)[0]); \
		(to) = palloc(_size); \
		memcpy((to), (from), _size); \
	} while (0)

/*
 * A PlanProfile holds state for recursive prescan_walker().
 */
typedef struct PlanProfile
{
	plan_tree_base_prefix base; /* Required prefix for
								 * plan_tree_walker/mutator */

	PlannerInfo *root;			/* cdbparallelize argument, root of top plan. */

	/*
	 * The result flags refer to relation in the result of an insert, update,
	 * or delete
	 */
	bool		resultSegments;

	bool		dispatchParallel;

	Flow	   *currentPlanFlow;	/* what is the flow of the current plan
									 * node */
} PlanProfile;


/*
 * Forward Declarations
 */
static void prescan(Plan *plan, PlanProfile *context);
static bool prescan_walker(Node *node, PlanProfile *context);

static bool adjustPlanFlow(Plan *plan,
			   bool stable,
			   bool rescannable,
			   Movement req_move,
			   List *hashExpr);

static void motion_sanity_check(PlannerInfo *root, Plan *plan);
static bool loci_compatible(List *hashExpr1, List *hashExpr2);
static Plan *materialize_subplan(PlannerInfo *root, Plan *subplan);

/* ------------------------------------------------------------------------- *
 * Function cdbparallelize() is the main entry point.
 *
 * Parallelize a single raw PostgreSQL sequential plan. The input plan
 * is modified in that Flow nodes are attached to Plan nodes in the input
 * as appropriate and the top-level plan node's dispatch field is set
 * to specify how the plan should be handled by the executor.
 *
 * If the dispatch field of the top-level plan is set to a non-default
 * value on entry, the plan is returned unmodified.  This guarantees that
 * we won't parallelize the same input plan more than once, provides a way
 * for the planner to create a plan that this function will recognize as
 * already final.
 *
 * The result is [a copy of] the input plan with alterations to make the
 * plan suitable for execution by Greenplum Database.
 *
 *
 * TODO: Much investigation remains in the areas of
 *
 *		- initialization plans,
 *		- plan parameters,
 *		- subqueries,
 *		- ordering and hash attributes,
 *		- etc.
 *
 *		The current implementation is little more than a rough sketch.
 *
 *
 * Outline of processing:
 *
 * - Return the input plan unmodified, if it's dispatch field is set.
 *
 * - Scan (scanForManagedTables) the input plan looking for references to
 *	 managed tables.  If none are found, mark the plan for sequential
 *	 dispatch and return it.
 *
 * - Augment the input plan (prescan) by attaching Flow nodes to each of
 *	 its Plan nodes.  This is where all the decisions about parallelizing
 *	 the query occur.
 *
 * - Implement the decisions recorded in the Flow nodes (apply_motion)
 *	 to produce a modified copy of the plan.  Mark the copy for parallel
 *	 dispatch and return it.
 * ------------------------------------------------------------------------- *
 */
Plan *
cdbparallelize(PlannerInfo *root,
			   Plan *plan,
			   Query *query,
			   int cursorOptions __attribute__((unused)),
			   ParamListInfo boundParams __attribute__((unused))
)
{
	PlanProfile profile;
	PlanProfile *context = &profile;

	/* Make sure we're called correctly (and need to be called).  */
	switch (Gp_role)
	{
		case GP_ROLE_DISPATCH:
			break;
		case GP_ROLE_UTILITY:
			return plan;
		case GP_ROLE_EXECUTE:
		case GP_ROLE_UNDEFINED:
			Insist(0);
	}
	Assert(is_plan_node((Node *) plan));
	Assert(query !=NULL && IsA(query, Query));

	/* Print plan if debugging. */
	if (Debug_print_prelim_plan)
		elog_node_display(DEBUG1, "preliminary plan", plan, Debug_pretty_print);

	/*
	 * Don't parallelize plans rendered impotent be constraint exclusion. See
	 * MPP-2168.  We could fold this into prescan() but this quick check is
	 * effective in the only case we know of.
	 */
	if (IsA(plan, Result) &&plan->lefttree == NULL
		&& plan->targetlist == NULL
		&& plan->qual == NULL
		&& plan->initPlan == NULL)
	{
		Assert(plan->dispatch != DISPATCH_PARALLEL);
		return plan;
	}

	/* Initialize the PlanProfile result ... */
	planner_init_plan_tree_base(&context->base, root);
	context->root = root;
	context->resultSegments = false;

	switch (query->commandType)
	{
		case CMD_SELECT:
			if (query->intoClause)
			{
				/* SELECT INTO always created partitioned tables. */
				context->resultSegments = true;
			}
			break;

		case CMD_INSERT:
		case CMD_UPDATE:
		case CMD_DELETE:
			{
				/*
				 * Since this is a data modification command, we need to
				 * include information about the result relation in the
				 * summary of relations touched.
				 */
				Oid			reloid;
				Relation	relation;
				GpPolicy   *policy = NULL;

				/* Get Oid of target relation. */
				Insist(query->resultRelation > 0 &&
					   query->resultRelation <= list_length(root->glob->finalrtable));
				reloid = getrelid(query->resultRelation, root->glob->finalrtable);

				/* Get a copy of the rel's GpPolicy from the relcache. */
				relation = relation_open(reloid, NoLock);
				policy = RelationGetPartitioningKey(relation);
				relation_close(relation, NoLock);

				/* Tell caller if target rel is distributed. */
				if (policy &&
					policy->ptype == POLICYTYPE_PARTITIONED)
					context->resultSegments = true;

				if (policy)
					pfree(policy);

				/* RETURNING is not yet implemented for partitioned tables. */
				if (query->returningList &&
					context->resultSegments)
				{
					const char *cmd = (query->commandType == CMD_INSERT) ? "INSERT"
					: (query->commandType == CMD_UPDATE) ? "UPDATE"
					: "DELETE";

					ereport(ERROR, (errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
									errmsg("The RETURNING clause of the %s "
										   "statement is not yet supported in "
										   "this version of " PACKAGE_NAME ".",
										   cmd)
									));
				}
			}
			break;

		default:
			Insist(0);
	}


	/*
	 * We need to keep track of whether any part of the plan needs to be
	 * dispatched in parallel.
	 */
	context->dispatchParallel = context->resultSegments;
	context->currentPlanFlow = NULL;

	/*
	 * Prescan the plan and attach Flow nodes to its Plan nodes. These nodes
	 * describe how to parallelize the plan.  We also accumulate summary
	 * information in the profile stucture.
	 */
	prescan(plan, context);

	if (context->dispatchParallel)
	{
		/*
		 * Implement the parallelizing directions in the Flow nodes attached
		 * to the root plan node of each root slice of the plan.
		 */
		Assert(root->parse == query);
		plan = apply_motion(root, plan, query);

		/*
		 * From this point on, since part of plan is parallel, we have to
		 * regard the whole plan as parallel.
		 */
		plan->dispatch = DISPATCH_PARALLEL;
	}

	if (gp_enable_motion_deadlock_sanity)
		motion_sanity_check(root, plan);

	return plan;
}


/* ----------------------------------------------------------------------- *
 * Functions prescan() and prescan_walker() use the plan_tree_walker()
 * framework to visit the nodes of the plan tree in order to check for
 * unsupported usages and collect information for use in finalizing the
 * plan.  The walker exits with an error only due an unexpected problems.
 *
 * The plan and query arguments should not be null.
 * ----------------------------------------------------------------------- *
 */
static void
prescan(Plan *plan, PlanProfile *context)
{
	if (prescan_walker((Node *) plan, context))
	{
		Insist(0);
	}
}

/**
 * Context information to be able to parallelize a subplan
 */
typedef struct ParallelizeCorrelatedPlanWalkerContext
{
	plan_tree_base_prefix base; /* Required prefix for
								 * plan_tree_walker/mutator */
	SubPlan    *sp;				/* subplan node */
	Movement	movement;		/* What is the final movement necessary? Is it
								 * gather or broadcast */
	List	   *rtable;			/* rtable from the global context */
	bool		subPlanDistributed; /* is original subplan distributed */
} ParallelizeCorrelatedPlanWalkerContext;

/**
 * Context information to map vars occuring in the Result node's targetlist
 * and quals to what is produced by the underlying SeqScan node.
 */
typedef struct MapVarsMutatorContext
{
	plan_tree_base_prefix base; /* Required prefix for
								 * plan_tree_walker/mutator */
	List	   *outerTL;		/* Target list from underlying scan node */
} MapVarsMutatorContext;

/**
 * Forward declarations of static functions
 */
static bool ContainsParamWalker(Node *expr, void *ctx);
static void ParallelizeSubplan(SubPlan *spExpr, PlanProfile *context);
static Plan *ParallelizeCorrelatedSubPlan(PlannerInfo *root, SubPlan *spExpr, Plan *plan, Movement m, bool subPlanDistributed);
static Node *MapVarsMutator(Node *expr, MapVarsMutatorContext *ctx);
static Node *ParallelizeCorrelatedSubPlanMutator(Node *node, ParallelizeCorrelatedPlanWalkerContext *ctx);
static Node *ParallelizeCorrelatedSubPlanUpdateFlowMutator(Node *node);

/**
 * Does an expression contain a parameter?
 */
static bool
ContainsParamWalker(Node *expr, void *ctx)
{
	Assert(ctx == NULL);

	if (expr == NULL)
	{
		return false;
	}
	else if (IsA(expr, Param) ||IsA(expr, SubPlan))
	{
		return true;
	}

	return expression_tree_walker(expr, ContainsParamWalker, ctx);
}

/**
 * Replaces all vars in an expression with OUTER vars referring to the
 * targetlist contained in the context (ctx->outerTL).
 */
static Node *
MapVarsMutator(Node *expr, MapVarsMutatorContext *ctx)
{
	Assert(ctx);

	if (expr == NULL)
		return NULL;

	if (IsA(expr, Var))
	{
		Assert(ctx->outerTL);
		TargetEntry *tle = tlist_member(expr, ctx->outerTL);

		Assert(tle && "unable to find var in outer TL");

		Var		   *vOrig = (Var *) expr;
		Var		   *vOuter = (Var *) copyObject(vOrig);

		vOuter->varno = OUTER;
		vOuter->varattno = tle->resno;
		vOuter->varnoold = vOrig->varno;
		return (Node *) vOuter;
	}

	return expression_tree_mutator(expr, MapVarsMutator, ctx);
}

/**
 * Add a materialize node to prevent rescan of subplan.
 */
static Plan *
materialize_subplan(PlannerInfo *root, Plan *subplan)
{
	Plan	   *mat = materialize_finished_plan(root, subplan);

	((Material *) mat)->cdb_strict = true;

	if (mat->flow)
	{
		mat->flow->req_move = MOVEMENT_NONE;
		mat->flow->flow_before_req_move = NULL;
	}

	return mat;
}

/* *
 * Not listing every node type here, if further bug found due to not updating flow
 * after cdbparallelize, just simply add that node type here.
 * */
static Node *
ParallelizeCorrelatedSubPlanUpdateFlowMutator(Node *node)
{
	Assert(is_plan_node(node));
	switch (nodeTag(node))
	{
		case T_Agg:
		case T_WindowAgg:
		case T_Sort:
		case T_Material:
		case T_Limit:
		case T_Result:
			{
				if (((Plan *) node)->lefttree && ((Plan *) node)->lefttree->flow)
					((Plan *) node)->flow = pull_up_Flow((Plan *) node, ((Plan *) node)->lefttree);
				break;
			}
		case T_Append:
			{
				List	   *append_list = (List *) ((Append *) node)->appendplans;

				if (append_list == NULL)
					break;
				Plan	   *first_append = (Plan *) (lfirst(list_head(append_list)));

				Assert(first_append && first_append->flow);
				((Plan *) node)->flow = pull_up_Flow((Plan *) node, first_append);
				break;
			}
		default:
			break;
	}
	return node;
}

/**
 * This is the workhorse method that transforms a plan containing correlated references
 * to one that is executable as part of a parallel plan.
 * 1. This method recurses down the subplan till it finds a leaf scan node
 *    (i.e. a seqscan, ao or co scan).
 * 2. This scan node is replaced by a tree that looks like:
 * 			Result (with quals)
 * 				\
 * 				 \_Material
 * 				 	\
 * 				 	 \_Broadcast (or Gather)
 * 				 	 	\
 * 				 	 	 \_SeqScan (no quals)
 * 	This transformed plan can be executed in a parallel setting since the correlation
 * 	is now part of the result node which executes in the same slice as the outer plan node.
 *
 * XXX: This relies on the planner to not generate other kinds of scans, like
 * IndexScans. We don't have the machinery in place to rescan those with different
 * parameters. We could support e.g. IndexScans as long as the index qual doesn't
 * refer to the outer parameter, but the planner isn't currently smart enough to
 * distinguish that, so we just disable index scans altogether in a subplan.
 */
static Node *
ParallelizeCorrelatedSubPlanMutator(Node *node, ParallelizeCorrelatedPlanWalkerContext *ctx)
{
	if (node == NULL)
		return NULL;

	if (IsA(node, FunctionScan))
	{
		RangeTblEntry *rte = rt_fetch(((Scan *) node)->scanrelid,
									  ctx->rtable);

		Assert(rte->rtekind == RTE_FUNCTION);

		if (rte->funcexpr &&
			ContainsParamWalker(rte->funcexpr, NULL /* ctx */ ) && ctx->subPlanDistributed)
		{
			ereport(ERROR, (errcode(ERRCODE_GP_FEATURE_NOT_YET),
							errmsg("Cannot parallelize that query yet."),
							errdetail("In a subquery FROM clause, a "
									  "function invocation cannot contain "
									  "a correlated reference.")
							));
		}
	}

	if (IsA(node, SeqScan)
		||IsA(node, AppendOnlyScan)
		||IsA(node, AOCSScan)
		||IsA(node, ShareInputScan)
		||IsA(node, ExternalScan))
	{
		Plan	   *scanPlan = (Plan *) node;

		/**
		 * If original subplan has no motion, we double check whether the scan
		 * node is on catalog table or not. If catalog, no need to apply
		 * parallelization.
		 * This is for case like:
		 * SELECT array(select case when p.oid in (select
		 * unnest(array[typoutput, typsend]) from pg_type) then 'upg_catalog.'
		 * else 'pg_catalog.' end) FROM pg_proc p;
		 **/
		if (scanPlan->flow && (scanPlan->flow->locustype == CdbLocusType_Entry))
			return (Node *) node;

		/**
		 * Steps:
		 * 1 - get targetlist from seqscan
		 * 2 - pull up 1 and set it as TL of result node
		 * 3 - get quals from seqscan
		 * 4 - extract out all vars from (3) and add it to TL
		 * 5 - transform (3) using TL and set it as qual of result node (use OUTER for varno)
		 */

		/**
		 * Step 1: Save targetlist, quals and paramset from the scan node
		 */
		List	   *saveTL = copyObject(scanPlan->targetlist);
		Bitmapset  *saveAllParam = bms_copy(scanPlan->allParam);
		Bitmapset  *saveExtParam = bms_copy(scanPlan->extParam);

		/**
		 * Step 2: Next, iterate over quals and find out if they need to be part of the scan or the result node.
		 */
		ListCell   *lc = NULL;
		List	   *resQual = NIL;
		List	   *scanQual = NIL;

		foreach(lc, scanPlan->qual)
		{
			Node	   *qual = (Node *) lfirst(lc);

			if (ContainsParamWalker(qual, NULL /* ctx */ ))
			{
				resQual = lappend(resQual, qual);
			}
			else
			{
				scanQual = lappend(scanQual, qual);
			}
		}
		scanPlan->qual = scanQual;
		scanPlan->allParam = NULL;
		scanPlan->extParam = NULL;

		/**
		 * Step 3: Find all the vars that are needed from the scan node
		 * for upstream processing.
		 */
		/* GPDB_84_MERGE_FIXME: What are the correct PVC_* modes here? */
		List	   *scanVars;

		scanVars = list_concat(
			pull_var_clause((Node *) scanPlan->targetlist,
							PVC_RECURSE_AGGREGATES,
							PVC_REJECT_PLACEHOLDERS),
			pull_var_clause((Node *) resQual,
							PVC_RECURSE_AGGREGATES,
							PVC_REJECT_PLACEHOLDERS));

		/*
		 * Step 4: Construct the new targetlist for the scan node
		 */
		{
			ListCell   *lc = NULL;
			int			resno = 1;
			List	   *scanTL = NIL;

			foreach(lc, scanVars)
			{
				Var		   *var = (Var *) copyObject(lfirst(lc));

				scanTL = lappend(scanTL, makeTargetEntry((Expr *) var,
														 resno,
														 NULL,
														 false));
				resno++;
			}
			scanPlan->targetlist = scanTL;
		}

		/**
		 * There should be no subplans in the scan node anymore.
		 */
		Assert(!contain_subplans((Node *) scanPlan->targetlist));
		Assert(!contain_subplans((Node *) scanPlan->qual));

		/**
		 * Step 5: Construct result node's targetlist and quals
		 */
		MapVarsMutatorContext ctx1;

		ctx1.base.node = ctx->base.node;
		ctx1.outerTL = scanPlan->targetlist;

		resQual = (List *) MapVarsMutator((Node *) resQual, &ctx1);
		List	   *resTL = (List *) MapVarsMutator((Node *) saveTL, &ctx1);

		/**
		 * Step 6: Ensure that apply_motion adds a broadcast or a gather motion
		 * depending on the desired movement.
		 */
		if (ctx->movement == MOVEMENT_BROADCAST)
		{
			broadcastPlan(scanPlan, false /* stable */ , false /* rescannable */ );
		}
		else
		{
			focusPlan(scanPlan, false /* stable */ , false /* rescannable */ );
		}

		/**
		 * Step 7: Add a material node
		 */
		Plan	   *mat = materialize_subplan((PlannerInfo *) ctx->base.node, scanPlan);

		/**
		 * Step 8: Fix up the result node on top of the material node
		 */
		Result	   *res = make_result((PlannerInfo *) ctx->base.node, resTL, NULL, mat);

		res->plan.qual = resQual;
		((Plan *) res)->allParam = saveAllParam;
		((Plan *) res)->extParam = saveExtParam;

		Assert(((Plan *) res)->lefttree && ((Plan *) res)->lefttree->flow);
		((Plan *) res)->flow = pull_up_Flow((Plan *) res, ((Plan *) res)->lefttree);

		/**
		 * It is possible that there is an additional level of correlation in the result node.
		 * we will need to recurse in these structures again.
		 */
		if (contain_subplans((Node *) res->plan.targetlist))
		{
			res->plan.targetlist = (List *) ParallelizeCorrelatedSubPlanMutator((Node *) res->plan.targetlist, ctx);
		}
		if (contain_subplans((Node *) res->plan.qual))
		{
			res->plan.qual = (List *) ParallelizeCorrelatedSubPlanMutator((Node *) res->plan.qual, ctx);
		}

		return (Node *) res;
	}

	if (IsA(node, SubPlan))
	{
		SubPlan    *sp = (SubPlan *) node;

		if (sp->is_initplan)
		{
			return node;
		}

		/**
		 * Pull up the parallelization of subplan to here, because we want to
		 * set the is_parallelized flag. We don't want to apply double
		 * parallelization for nested subplan.
		 * This is for case like:
		 * select A.i, A.j, (select sum(C.j) from C where C.j = A.j and C.i =
		 * (select A.i from A where A.i = C.i)) from A;
		 **/
		SubPlan    *new_sp = (SubPlan *) plan_tree_mutator(node, ParallelizeCorrelatedSubPlanMutator, ctx);

		Assert(new_sp);
		new_sp->is_parallelized = true;
		return (Node *) new_sp;
	}

	/**
	 * Remove any existing motion nodes in the plan. For an informal proof as to why
	 * this is correct, please refer to the correlated subquery design doc.
	 */
	if (IsA(node, Motion))
	{
		Plan	   *plan = (Plan *) node;

		node = (Node *) plan->lefttree;
		Assert(node);
		return ParallelizeCorrelatedSubPlanMutator(node, ctx);
	}

	Node	   *result = plan_tree_mutator(node, ParallelizeCorrelatedSubPlanMutator, ctx);

	/*
	 * * Update the flow of the current plan node.
	 */
	if (is_plan_node(node))
		return ParallelizeCorrelatedSubPlanUpdateFlowMutator(result);
	return result;
}

/**
 * Parallelizes a correlated subplan. See ParallelizeCorrelatedSubPlanMutator for details.
 */
Plan *
ParallelizeCorrelatedSubPlan(PlannerInfo *root, SubPlan *spExpr, Plan *plan, Movement m, bool subPlanDistributed)
{
	ParallelizeCorrelatedPlanWalkerContext ctx;

	ctx.base.node = (Node *) root;
	ctx.movement = m;
	ctx.subPlanDistributed = subPlanDistributed;
	ctx.sp = spExpr;
	ctx.rtable = root->glob->finalrtable;
	return (Plan *) ParallelizeCorrelatedSubPlanMutator((Node *) plan, &ctx);

}

/**
 * This function takes a specific SubPlan and transforms its underlying plan
 * it so that it may be executed as part of a distributed plan.
 * - If the subplan corresponds to an initplan, then requests the results
 * to be focused.
 * - If the subplan is uncorrelated, multi-row subquery, then it either focuses or broadcasts
 * the subplan based on the flow of the containing plan node.
 * - If the subplan is correlated, then it transforms the correlated into a form that is
 * executable
 */
void
ParallelizeSubplan(SubPlan *spExpr, PlanProfile *context)
{
	Assert(!spExpr->is_parallelized);

	/**
	 * If a subplan is multi-row, it cannot be an initplan right now.
	 */
	AssertImply(spExpr->is_multirow, !spExpr->is_initplan);

	/**
	 * If it is an initplan, there can be no parameters from outside
	 */
	AssertImply(spExpr->is_initplan, spExpr->parParam == NIL);

	Plan	   *origPlan = planner_subplan_get_plan(context->root, spExpr);

	bool		containingPlanDistributed = (context->currentPlanFlow && context->currentPlanFlow->flotype == FLOW_PARTITIONED);
	bool		subPlanDistributed = (origPlan->flow && origPlan->flow->flotype == FLOW_PARTITIONED);
	bool		hasParParam = (list_length(spExpr->parParam) > 0);

	/**
	 * If containing plan is distributed then we must know the flow of the subplan.
	 */
	AssertImply(containingPlanDistributed, (origPlan->flow && origPlan->flow->flotype != FLOW_UNDEFINED));

	Plan	   *newPlan = copyObject(origPlan);

	if (spExpr->is_initplan)
	{
		/*
		 * Corresponds to an initplan whose results need to end up at the QD.
		 * Request focus flow. Note ARRAY() subquery needs its order
		 * preserved, per spec. Otherwise, initplan doesn't care the query
		 * result order even if it is specified.
		 */
		focusPlan(newPlan, spExpr->subLinkType == ARRAY_SUBLINK, false);
	}
	else if (spExpr->is_multirow
			 && spExpr->parParam == NIL)
	{
		/**
		 * Corresponds to a multi-row uncorrelated subplan. The results need
		 * to be either broadcasted or focused depending on the flow of the containing
		 * plan node.
		 */

		if (containingPlanDistributed)
		{
			broadcastPlan(newPlan, false /* stable */ , false /* rescannable */ );
		}
		else
		{
			focusPlan(newPlan, false /* stable */ , false /* rescannable */ );
		}

		newPlan = materialize_subplan(context->root, newPlan);
	}

	/*
	 * * [JIRA: MPP-24563] Adding hasParParam check here, for the kind of
	 * cases in JIRA, which has both focused parent plan and subplan.
	 */
	else if (containingPlanDistributed || subPlanDistributed || hasParParam)
	{
		Movement	reqMove = containingPlanDistributed ? MOVEMENT_BROADCAST : MOVEMENT_FOCUS;

		/**
		 * Subplan corresponds to a correlated subquery and its parallelization
		 * requires certain transformations.
		 */
		newPlan = ParallelizeCorrelatedSubPlan(context->root, spExpr, newPlan, reqMove, subPlanDistributed);
	}

	Assert(newPlan);

	/**
	 * Replace subplan in global subplan list.
	 */
	list_nth_replace(context->root->glob->subplans, spExpr->plan_id - 1, newPlan);
	spExpr->is_parallelized = true;

}

/*
 * Function prescan_walker is the workhorse of prescan.
 *
 * The driving function, prescan(), should be called only once on a
 * plan produced by the Greenplum Database optimizer for dispatch on the QD.  There
 * are two main task performed:
 *
 * 1. Since the optimizer isn't in a position to view plan globally,
 *    it may generate plans that we can't execute.  This function is
 *    responsible for detecting potential problem usages:
 *
 *    Example: Non-initplan subplans are legal in sequential contexts
 *    but not in parallel contexs.
 *
 * 2. The function specifies (by marking Flow nodes) what (if any)
 *    motions need to be applied to initplans prior to dispatch.
 *    As in the case of the main plan, the actual Motion node is
 *    attached later.
 */
static bool
prescan_walker(Node *node, PlanProfile *context)
{
	if (node == NULL)
		return false;

	Flow	   *savedPlanFlow = context->currentPlanFlow;

	if (is_plan_node(node))
	{
		Plan	   *plan = (Plan *) node;

		if (plan->dispatch == DISPATCH_PARALLEL)
			context->dispatchParallel = true;

		if (plan->flow &&
			(plan->flow->flotype == FLOW_PARTITIONED
			 || plan->flow->flotype == FLOW_REPLICATED))
			context->dispatchParallel = true;

		context->currentPlanFlow = plan->flow;
		if (context->currentPlanFlow
			&& context->currentPlanFlow->flow_before_req_move)
		{
			context->currentPlanFlow = context->currentPlanFlow->flow_before_req_move;
		}

	}

	switch (nodeTag(node))
	{
		case T_SubPlan:

			/*
			 * A SubPlan node is not a Plan node, but an Expr node that
			 * contains a Plan and its range table.  It corresponds to a
			 * SubLink in the query tree (a SQL subquery expression) and may
			 * occur in an expression or an initplan list.
			 */

			{
				/*
				 * SubPlans establish a local state for the contained plan,
				 * notably the range table.
				 */
				SubPlan    *subplan = (SubPlan *) node;

				if (!subplan->is_parallelized)
				{
					ParallelizeSubplan(subplan, context);
				}
				break;
			}

		case T_Motion:
			{
				context->dispatchParallel = true;
				break;
			}

		default:
			break;

	}

	bool		result = plan_tree_walker(node, prescan_walker, context);

	/**
	 * Replace saved flow
	 */
	context->currentPlanFlow = savedPlanFlow;

	return result;
}


/*
 * Construct a new Flow in the current memory context.
 */
Flow *
makeFlow(FlowType flotype)
{
	Flow	   *flow = makeNode(Flow);

	flow->flotype = flotype;
	flow->req_move = MOVEMENT_NONE;
	flow->locustype = CdbLocusType_Null;

	return flow;
}


/*
 * Create a flow for the given Plan node based on the flow in its
 * lefttree (outer) plan.  Partitioning information is preserved.
 * Sort specifications are preserved only if withSort is true.
 *
 * NB: At one time this function was called during cdbparallelize(), after
 * transformation of the plan by set_plan_references().  Later, calls were
 * added upstream of set_plan_references(); only these remain at present.
 *
 * Don't call on a SubqueryScan plan.  If you are tempted, you probably
 * want to use mark_passthru_locus (after make_subqueryscan) instead.
 */
Flow *
pull_up_Flow(Plan *plan, Plan *subplan)
{
	Flow	   *model_flow = NULL;
	Flow	   *new_flow = NULL;

	Insist(subplan);

	model_flow = subplan->flow;

	Insist(model_flow);

	/* SubqueryScan always has manifest Flow, so we shouldn't see one here. */
	Assert(!IsA(plan, SubqueryScan));

	if (IsA(plan, MergeJoin) ||IsA(plan, NestLoop) ||IsA(plan, HashJoin))
		Assert(subplan == plan->lefttree || subplan == plan->righttree);
	else if (IsA(plan, Append))
		Assert(list_member(((Append *) plan)->appendplans, subplan));
	else
		Assert(subplan == plan->lefttree);

	new_flow = makeFlow(model_flow->flotype);

	if (model_flow->flotype == FLOW_SINGLETON)
		new_flow->segindex = model_flow->segindex;

	/* Pull up hash key exprs, if they're all present in the plan's result. */
	else if (model_flow->flotype == FLOW_PARTITIONED &&
			 model_flow->hashExpr)
	{
		if (!is_projection_capable_plan(plan) ||
			cdbpullup_isExprCoveredByTargetlist((Expr *) model_flow->hashExpr,
												plan->targetlist))
			new_flow->hashExpr = copyObject(model_flow->hashExpr);
	}

	new_flow->locustype = model_flow->locustype;

	return new_flow;
}


/*
 * Is the node a "subclass" of Plan?
 */
bool
is_plan_node(Node *node)
{
	if (node == NULL)
		return false;

	if (nodeTag(node) >= T_Plan_Start && nodeTag(node) < T_Plan_End)
		return true;
	return false;
}

/* Functions focusPlan, broadcastPlan, and repartitionPlan annotate the top
 * of the given plan tree with a motion request for later implementation by
 * function apply_motion in cdbmutate.c.
 *
 * plan		   the plan to annotate
 *
 * stable	   if true, the result plan must present the result in the
 *			   order specified in the flow (which, presumably, matches
 *			   the input order.
 *
 * rescannable	if true, the resulting plan must be rescannable, i.e.,
 *			   it must be possible to use it like a scan. (Note that
 *			   currently, no Motion nodes are rescannable.
 *
 * Remaining arguments, if any, determined by the requirements of the specific
 * function.
 *
 * The result is true if the requested annotation could be made. Else, false.
 */

/*
 * Function: focusPlan
 */
bool
focusPlan(Plan *plan, bool stable, bool rescannable)
{
	Assert(plan->flow && plan->flow->flotype != FLOW_UNDEFINED);

	/* Already focused?  Do nothing. */
	if (plan->flow->flotype == FLOW_SINGLETON)
		return true;

	/* TODO How specify deep-six? */
	if (plan->flow->flotype == FLOW_REPLICATED)
		return false;

	return adjustPlanFlow(plan, stable, rescannable, MOVEMENT_FOCUS, NIL);
}

/*
 * Function: broadcastPlan
 */
bool
broadcastPlan(Plan *plan, bool stable, bool rescannable)
{
	Assert(plan->flow && plan->flow->flotype != FLOW_UNDEFINED);

	return adjustPlanFlow(plan, stable, rescannable, MOVEMENT_BROADCAST, NIL);
}


/**
 * This method is used to determine if motion nodes may be avoided for certain insert-select
 * statements. To do this it determines if the loci are compatible (ignoring relabeling).
 */
static bool
loci_compatible(List *hashExpr1, List *hashExpr2)
{
	ListCell   *cell1;
	ListCell   *cell2;

	if (list_length(hashExpr1) != list_length(hashExpr2))
		return false;

	forboth(cell1, hashExpr1, cell2, hashExpr2)
	{
		Expr	   *var1 = (Expr *) lfirst(cell1);
		Expr	   *var2 = (Expr *) lfirst(cell2);

		/*
		 * right side variable may be encapsulated by a relabel node. motion,
		 * however, does not care about relabel nodes.
		 */
		if (IsA(var2, RelabelType))
			var2 = ((RelabelType *) var2)->arg;

		if (!equal(var1, var2))
			return false;
	}
	return true;
}

/*
 * Function: repartitionPlan
 */
bool
repartitionPlan(Plan *plan, bool stable, bool rescannable, List *hashExpr)
{
	Assert(plan->flow);
	Assert(plan->flow->flotype == FLOW_PARTITIONED ||
		   plan->flow->flotype == FLOW_SINGLETON);

	/* Already partitioned on the given hashExpr?  Do nothing. */
	if (hashExpr)
	{
		if (equal(hashExpr, plan->flow->hashExpr))
			return true;

		/*
		 * MPP-4922: The earlier check is very conservative and may result in
		 * unnecessary motion nodes. Adding another check to avoid motion
		 * nodes, if possible.
		 */
		if (loci_compatible(hashExpr, plan->flow->hashExpr))
			return true;
	}

	return adjustPlanFlow(plan, stable, rescannable, MOVEMENT_REPARTITION, hashExpr);
}

/*
 * repartitionPlanForGroupClauses
 */
bool
repartitionPlanForGroupClauses(PlannerInfo *root, Plan *plan,
							   bool stable, bool rescannable,
							   List *groupclauses, List *targetlist)
{
	List	   *hashExpr;
	ListCell   *lc;

	/*
	 * Build a hashExpr from the SortGroupClauses.
	 */
	hashExpr = NIL;
	foreach (lc, groupclauses)
	{
		SortGroupClause *sortcl = (SortGroupClause *) lfirst(lc);
		Node	   *n;

		n = get_sortgroupclause_expr(sortcl, targetlist);

		hashExpr = lappend(hashExpr, n);
	}

	return repartitionPlan(plan, stable, rescannable, hashExpr);
}

/*
 * Helper Function: adjustPlanFlow
 *
 * Recursively pushes a motion request down the plan tree which allows us
 * to remove constraints in some cases.	For example, pushing a motion below
 * a Sort removes the stability constraint.
 *
 * Returns true if successful.  Returns false and doesn't change anything
 * if the request cannot be honored.
 */
static bool
adjustPlanFlow(Plan *plan,
			   bool stable,
			   bool rescannable,
			   Movement req_move,
			   List *hashExpr)
{
	Flow	   *flow = plan->flow;
	bool		disorder = false;
	bool		recur = false;
	bool		reorder = false;

	Assert(flow && flow->flotype != FLOW_UNDEFINED);
	Assert(flow->req_move == MOVEMENT_NONE);
	Assert(flow->flow_before_req_move == NULL);

	switch (req_move)
	{
		case MOVEMENT_FOCUS:
			/* Don't request Merge Receive unless ordering is significant. */
			if (!stable)
				disorder = true;
			break;

		case MOVEMENT_BROADCAST:
		case MOVEMENT_REPARTITION:
			if (flow->flotype == FLOW_PARTITIONED)
				disorder = true;
			break;

		default:
			break;
	}

	/*
	 * Decide whether to push the request further down the plan tree and
	 * possibly remove restrictions.
	 */
	switch (plan->type)
	{
		case T_Sort:
			if (!stable)
			{
				/*---------------------------------------------------------
                 * Q: If order doesn't matter, why is there a Sort here?
                 * A: Could be INSERT...SELECT...ORDER BY; each QE in the
                 * inserting gang will sort its own partition locally.  Weird,
                 * but could be used for clustering.  But note it in the debug
                 * log; some caller might specify stable = false by mistake.
				 *---------------------------------------------------------
                 */
				ereport(DEBUG4, (errmsg("adjustPlanFlow stable=false applied "
										"to Sort operator; req_move=%d",
										req_move)));
			}

			if (req_move == MOVEMENT_FOCUS)
			{
				/*
				 * We'd rather do a merge of a parallel sort, than do a
				 * sequential sort. A fixed motion can do that as long as we
				 * don't need to rescan.
				 */
				break;
			}
			else
			{
				/* No need to preserve order or restartability below a sort. */
				recur = true;
				reorder = true;
				rescannable = false;
			}
			break;

		case T_Material:
			/* Always push motion below Material. */
			/* No need to preserve restartability below a materialization. */
			recur = true;
			rescannable = false;
			break;

		case T_Append:			/* Maybe handle specially some day. */
		default:
			break;
	}

	/*
	 * Push motion down to a descendant of this node (= upstream).
	 */
	if (recur)
	{

		Assert(plan->lefttree &&
			   plan->lefttree->flow &&
			   plan->lefttree->flow->flotype != FLOW_UNDEFINED);

		/* Descend to apply the requested Motion somewhere below this node. */
		if (!adjustPlanFlow(plan->lefttree,
							stable && !reorder,
							rescannable,
							req_move,
							hashExpr))
			return false;

		/* After updating subplan, bubble new distribution back up the tree. */
		Flow	   *kidflow = plan->lefttree->flow;

		flow->flotype = kidflow->flotype;
		flow->segindex = kidflow->segindex;
		flow->hashExpr = copyObject(kidflow->hashExpr);
		plan->dispatch = plan->lefttree->dispatch;

		return true;			/* success */
	}

	/*
	 * The buck stops here.  We have arrived at the node whose output is to be
	 * moved.  Return false if can't do the motion as requested.
	 */

	/* Would motion mess up a required ordering? */
	if (stable && disorder)
		return false;

	/* Adding a Motion node would make the result non-rescannable. */
	if (rescannable)
		return false;

	/*
	 * Make our changes to a shallow copy of the original Flow node. Later,
	 * after applying the requested motion, apply_motion_mutator() will
	 * restore the original Flow ptr and discard the modified copy.
	 *
	 * Note that the original Flow node describes the present Plan node's own
	 * result distribution and ordering: i.e. the input to the future Motion.
	 * The temporary modified Flow describes the result of the requested
	 * Motion: the input to the next operator downstream.
	 */
	{
		Assert(flow->flow_before_req_move == NULL);

		plan->flow = (Flow *) palloc(sizeof(*plan->flow));
		*plan->flow = *flow;
		plan->flow->flow_before_req_move = flow;
		flow = plan->flow;
	}

	/*
	 * Set flag to tell apply_motion() to insert a Motion node immediately
	 * above this node (= downstream).  Overwrite the present Plan node's Flow
	 * node to describe the distribution and ordering that the future Motion
	 * node will produce.
	 */
	flow->req_move = req_move;

	switch (req_move)
	{
		case MOVEMENT_FOCUS:
			/* Converge to a single QE (or QD; that choice is made later). */
			flow->flotype = FLOW_SINGLETON;
			flow->hashExpr = NIL;
			flow->segindex = 0;
			break;

		case MOVEMENT_BROADCAST:
			flow->flotype = FLOW_REPLICATED;
			flow->hashExpr = NIL;
			flow->segindex = 0;
			break;

		case MOVEMENT_REPARTITION:
			flow->flotype = FLOW_PARTITIONED;
			flow->hashExpr = copyObject(hashExpr);
			flow->segindex = 0;
			break;

		default:
			Assert(0);
	}

	/* Since we added Motion, dispatch must be parallel. */
	plan->dispatch = DISPATCH_PARALLEL;

	return true;				/* success */
}								/* adjustPlanFlow */

#define SANITY_MOTION 0x1
#define SANITY_DEADLOCK 0x2

typedef struct sanity_result_t
{
	plan_tree_base_prefix base; /* Required prefix for
								 * plan_tree_walker/mutator */
	int			flags;
} sanity_result_t;

static bool
motion_sanity_walker(Node *node, sanity_result_t *result)
{
	sanity_result_t left_result;
	sanity_result_t right_result;
	bool		deadlock_safe = false;
	char	   *branch_label;

	left_result.base = result->base;
	left_result.flags = 0;
	right_result.base = result->base;
	right_result.flags = 0;

	if (node == NULL)
		return false;

	if (!is_plan_node(node))
		return false;

	/*
	 * Special handling for branch points because there is a possibility of a
	 * deadlock if there are Motions in both branches and one side is not
	 * first pre-fetched.
	 *
	 * The deadlock occurs because, when the buffers on the Send side of a
	 * Motion are completely filled with tuples, it blocks waiting for an ACK.
	 * Without prefetch_inner, the Join node reads one tuple from the outer
	 * side first and then starts retrieving tuples from the inner side -
	 * either to build a hash table (in case of HashJoin) or for joining (in
	 * case of MergeJoin and NestedLoopJoin).
	 *
	 * Thus we can end up with 4 processes infinitely waiting for each other :
	 *
	 * A : Join slice that already retrieved an outer tuple, and is waiting
	 * for inner tuples from D. B : Join slice that is still waiting for the
	 * first outer tuple from C. C : Outer slice whose buffer is full sending
	 * tuples to A and is blocked waiting for an ACK from A. D : Inner slice
	 * that is full sending tuples to B and is blocked waiting for an ACK from
	 * B.
	 *
	 * A cannot ACK C because it is waiting to finish retrieving inner tuples
	 * from D. B cannot ACK D because it is waiting for it's first outer tuple
	 * from C before accepting any inner tuples. This forms a circular
	 * dependency resulting in a deadlock : C -> A -> D -> B -> C.
	 *
	 * We avoid this by pre-fetching all the inner tuples in such cases and
	 * materializing them in some fashion, before moving on to outer_tuples.
	 * This effectively breaks the cycle and prevents deadlock.
	 *
	 * Details:
	 * https://groups.google.com/a/greenplum.org/forum/#!msg/gpdb-dev/gMa1tW0x_fk/wuzvGXBaBAAJ
	 */
	switch (nodeTag(node))
	{
		case T_HashJoin:		/* Hash join can't deadlock -- it fully
								 * materializes its inner before switching to
								 * its outer. */
			branch_label = "HJ";
			if (((HashJoin *) node)->join.prefetch_inner)
				deadlock_safe = true;
			break;
		case T_NestLoop:		/* Nested loop joins are safe only if the
								 * prefetch flag is set */
			branch_label = "NL";
			if (((NestLoop *) node)->join.prefetch_inner)
				deadlock_safe = true;
			break;
		case T_MergeJoin:
			branch_label = "MJ";
			if (((MergeJoin *) node)->join.prefetch_inner)
				deadlock_safe = true;
			break;
		default:
			branch_label = NULL;
			break;
	}

	/* now scan the subplans */
	switch (nodeTag(node))
	{
		case T_Result:
		case T_WindowAgg:
		case T_TableFunctionScan:
		case T_ShareInputScan:
		case T_Append:
		case T_SeqScan:
		case T_ExternalScan:
		case T_AppendOnlyScan:
		case T_AOCSScan:
		case T_IndexScan:
		case T_BitmapIndexScan:
		case T_BitmapHeapScan:
		case T_BitmapAppendOnlyScan:
		case T_BitmapTableScan:
		case T_TidScan:
		case T_SubqueryScan:
		case T_FunctionScan:
		case T_ValuesScan:
		case T_Agg:
		case T_Unique:
		case T_Hash:
		case T_SetOp:
		case T_Limit:
		case T_Sort:
		case T_Material:
			if (plan_tree_walker(node, motion_sanity_walker, result))
				return true;
			break;

		case T_Motion:
			if (plan_tree_walker(node, motion_sanity_walker, result))
				return true;
			result->flags |= SANITY_MOTION;
			elog(DEBUG5, "   found motion");
			break;

		case T_HashJoin:
		case T_NestLoop:
		case T_MergeJoin:
			{
				Plan	   *plan = (Plan *) node;

				elog(DEBUG5, "    %s going left", branch_label);
				if (motion_sanity_walker((Node *) plan->lefttree, &left_result))
					return true;
				elog(DEBUG5, "    %s going right", branch_label);
				if (motion_sanity_walker((Node *) plan->righttree, &right_result))
					return true;

				elog(DEBUG5, "    %s branch point left 0x%x right 0x%x",
					 branch_label, left_result.flags, right_result.flags);

				/* deadlocks get sent up immediately */
				if ((left_result.flags & SANITY_DEADLOCK) ||
					(right_result.flags & SANITY_DEADLOCK))
				{
					result->flags |= SANITY_DEADLOCK;
					break;
				}

				/*
				 * if this node is "deadlock safe" then even if we have motion
				 * on both sides we will not deadlock (because the access
				 * pattern is deadlock safe: all rows are retrieved from one
				 * side before the first row from the other).
				 */
				if (!deadlock_safe && ((left_result.flags & SANITY_MOTION) &&
									   (right_result.flags & SANITY_MOTION)))
				{
					elog(LOG, "FOUND MOTION DEADLOCK in %s", branch_label);
					result->flags |= SANITY_DEADLOCK;
					break;
				}

				result->flags |= left_result.flags | right_result.flags;

				elog(DEBUG5, "    %s branch point left 0x%x right 0x%x res 0x%x%s",
					 branch_label, left_result.flags, right_result.flags, result->flags, deadlock_safe ? " deadlock safe: prefetching" : "");
			}
			break;
		default:
			break;
	}
	return false;
}

static void
motion_sanity_check(PlannerInfo *root, Plan *plan)
{
	sanity_result_t sanity_result;

	planner_init_plan_tree_base(&sanity_result.base, root);
	sanity_result.flags = 0;

	elog(DEBUG5, "Motion Deadlock Sanity Check");

	if (motion_sanity_walker((Node *) plan, &sanity_result))
	{
		Insist(0);
	}

	if (sanity_result.flags & SANITY_DEADLOCK)
		elog(ERROR, "Post-planning sanity check detected motion deadlock.");
}
