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
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "optimizer/clauses.h"
#include "optimizer/pathnode.h"
#include "nodes/primnodes.h"
#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"
#include "nodes/pg_list.h"
#include "nodes/print.h"

#include "optimizer/paths.h"
#include "optimizer/planmain.h" /* for is_projection_capable_plan() */
#include "parser/parsetree.h"	/* for rt_fetch() */
#include "nodes/makefuncs.h"	/* for makeTargetEntry() */
#include "utils/guc.h"			/* for Debug_pretty_print */

#include "cdb/cdbvars.h"
#include "cdb/cdbplan.h"
#include "cdb/cdbpullup.h"
#include "cdb/cdbllize.h"
#include "cdb/cdbmutate.h"
#include "optimizer/tlist.h"


/*
 * A PlanProfile holds state for cdbparallelize() and its prescan stage.
 *
 * PlanProfileSubPlanInfo holds extra information about every subplan in
 * the plan tree. There is one PlanProfileSubPlanInfo for each entry in
 * glob->subplans.
 */
typedef struct PlanProfileSubPlanInfo
{
	/*
	 * plan_id is used to identify this subplan in the overall plan tree.
	 * Same as SubPlan->plan_id.
	 */
	int			plan_id;

	bool		seen;			/* have we seen a SubPlan reference to this yet? */
	bool		initPlanParallel;	/* T = this is an Init Plan that needs to be
									 * dispatched, i.e. it contains Motions */

	/* Fields copied from SubPlan */
	bool		is_initplan;
	bool		is_multirow;
	bool		hasParParam;
	SubLinkType subLinkType;

	/* The context where we saw the SubPlan reference(s) for this. */
	Flow	   *parentFlow;
} PlanProfileSubPlanInfo;

typedef struct PlanProfile
{
	plan_tree_base_prefix base; /* Required prefix for
								 * plan_tree_walker/mutator */

	PlannerInfo *root;			/* cdbparallelize argument, root of top plan. */

	/* main plan is parallelled */
	bool		dispatchParallel;

	/* Any of the init plans is parallelled */
	bool		anyInitPlanParallel;

	/* array is indexed by plan_id (plan_id is 1-based, so 0 is unused) */
	PlanProfileSubPlanInfo *subplan_info;

	/*
	 * Working queue in prescan stage. Contains plan_ids of subplans that have been
	 * seen in SubPlan expressions, but haven't been parallelized yet.
	 */
	List	   *unvisited_subplans;

	/* working state for prescan_walker() */
	Flow	   *currentPlanFlow;	/* what is the flow of the current plan
									 * node */
} PlanProfile;

/*
 * Forward Declarations
 */
static void prescan_plantree(Plan *plan, PlanProfile *context);
static void prescan_subplan(int plan_id, PlanProfile *context);
static bool prescan_walker(Node *node, PlanProfile *context);
static void parallelize_subplans(Plan *plan, PlanProfile *context);

static void motion_sanity_check(PlannerInfo *root, Plan *plan);

/* ------------------------------------------------------------------------- *
 * Function cdbparallelize() is the main entry point.
 *
 * Parallelize a single raw PostgreSQL sequential plan. The input plan
 * is modified in that Flow nodes are attached to Plan nodes in the input
 * as appropriate and the top-level plan node's dispatch field is set
 * to specify how the plan should be handled by the executor.
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
 * - Walk through all init-plans. Make note which ones are used, and
 *   remove any unused ones (remove_unused_initplans())
 *
 * - Walk through the main plan tree, make note of SubPlan expressions
 *   and the context they appear in. (prescan_plantree())
 *
 * - Walk through any subplans that were referenced from the main plan
 *   tree (prescan_plantree_subplans()). If there are references to
 *   other subplans in a subplan, they are added to the working queue.
 *   Iterate until all reachable subplans have been visited.
 *
 * - If there were no Motions in the plan, the plan can be executed
 *   as is, and no further processing is required.
 *
 * - Otherwise, "parallelize" any subplan Plans into a form that can be
 *   executed as part of the main plan. (parallelize_subplans())
 *
 * - Implement the decisions recorded in the Flow nodes (apply_motion)
 *	 to produce a modified copy of the plan.  Mark the copy for parallel
 *	 dispatch and return it.
 * ------------------------------------------------------------------------- *
 */
Plan *
cdbparallelize(PlannerInfo *root, Plan *plan, bool *needToAssignDirectDispatchContentIds)
{
	PlanProfile profile;
	PlanProfile *context = &profile;

	/* Make sure we're called correctly (and need to be called).  */
	if (Gp_role == GP_ROLE_UTILITY)
		return plan;
	if (Gp_role != GP_ROLE_DISPATCH)
		elog(ERROR, "Plan parallelization invoked for incorrect role: %s",
			 role_to_string(Gp_role));

	Assert(is_plan_node((Node *) plan));
	/* this should only be called at the top-level, not on a subquery */
	Assert(root->parent_root == NULL);

	/*
	 * Walk plan and remove unused initplans and their params
	 */
	remove_unused_initplans(plan, root);

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
	context->dispatchParallel = false;
	context->anyInitPlanParallel = false;
	context->currentPlanFlow = NULL;
	context->subplan_info = (PlanProfileSubPlanInfo *)
		palloc((list_length(root->glob->subplans) + 1) * sizeof(PlanProfileSubPlanInfo));
	for (int i = 0; i <= list_length(root->glob->subplans); i++)
	{
		context->subplan_info[i].plan_id = i;
		context->subplan_info[i].seen = false;
		context->subplan_info[i].initPlanParallel = false;
	}
	context->unvisited_subplans = NIL;

	switch (root->parse->commandType)
	{
		case CMD_SELECT:
			/* SELECT INTO / CREATE TABLE AS always created partitioned tables. */
			if (root->parse->parentStmtType != PARENTSTMTTYPE_NONE)
				context->dispatchParallel = true;
			break;

		case CMD_INSERT:
		case CMD_UPDATE:
		case CMD_DELETE:
			break;

		default:
			elog(ERROR, "incorrect commandtype for Plan parallelization");
	}

	/*
	 * First scan the main plantree, making note of any Motion nodes, so that
	 * we know whether the plan needs to be dispatched in parallel. We also
	 * make note of any SubPlan expressions, and the parallelization context
	 * that they appear in.
	 */
	prescan_plantree(plan, context);

	/*
	 * Then scan all the subplans that were referenced from the main plan tree.
	 * Whenever we walk through a sub-plan, we might see more subplans.
	 * They are added to the 'unvisited_subplans' working queue, so keep
	 * going until the queue is empty.
	 */
	while (context->unvisited_subplans)
	{
		int			plan_id = linitial_int(context->unvisited_subplans);

		context->unvisited_subplans = list_delete_first(context->unvisited_subplans);

		prescan_subplan(plan_id, context);
	}

	root->glob->subplan_sliceIds = palloc0((list_length(root->glob->subplans) + 1) * sizeof(int));
	root->glob->subplan_initPlanParallel = palloc0((list_length(root->glob->subplans) + 1) * sizeof(bool));

	/*
	 * Parallelize any subplans.
	 *
	 * Do this even when not actually dispatching, to eliminate any unused
	 * subplans. That's not merely an optimization: the unused subplans might
	 * contain Motion nodes, and the executor gets confused if there are
	 * Motion nodes - even though they would never be executed - in a plan
	 * that's not dispatched.
	 */
	parallelize_subplans(plan, context);

	/*
	 * Implement the parallelizing directions in the Flow nodes attached
	 * to the root plan node of each root slice of the plan, and assign
	 * slice IDs to parts of the plan tree that will run in separate
	 * processes.
	 */
	if (context->dispatchParallel || context->anyInitPlanParallel)
		plan = apply_motion(root, plan, needToAssignDirectDispatchContentIds);

	/*
	 * Mark the root plan to DISPATCH_PARALLEL if prescan() says it is
	 * parallel. Each init plan has its own flag to indicate
	 * whether it's a parallel plan.
	 */
	if (context->dispatchParallel)
		plan->dispatch = DISPATCH_PARALLEL;

	if (context->anyInitPlanParallel)
	{
		for (int plan_id = 1; plan_id <= list_length(root->glob->subplans); plan_id++)
		{
			PlanProfileSubPlanInfo *spinfo = &context->subplan_info[plan_id];

			if (spinfo->seen && spinfo->is_initplan && spinfo->initPlanParallel)
				root->glob->subplan_initPlanParallel[plan_id] = true;
		}
	}

	if (gp_enable_motion_deadlock_sanity)
		motion_sanity_check(root, plan);

	return plan;
}

/*
 * parallelize_subplans()
 *
 * Transform subplans into something that can run in a parallel Greenplum
 * environment. Also remove unused subplans.
 */
static void
parallelize_subplans(Plan *plan, PlanProfile *context)
{
	PlannerInfo *root = context->root;

	/*
	 * Parallelize all subplans. And remove unused ones
	 */
	for (int plan_id = 1; plan_id <= list_length(root->glob->subplans); plan_id++)
	{
		PlanProfileSubPlanInfo *spinfo = &context->subplan_info[plan_id];
		ListCell   *planlist_cell = list_nth_cell(root->glob->subplans, plan_id - 1);
		Plan	   *subplan_plan = (Plan *) lfirst(planlist_cell);

		if (!spinfo->seen)
		{
			/*
			 * Remove unused subplans.
			 * Executor initializes state for subplans even they are unused.
			 * When the generated subplan is not used and has motion inside,
			 * causing motionID not being assigned, which will break sanity
			 * check when executor tries to initialize subplan state.
			 */
			/*
			 * This subplan is unused. Replace it in the global list of
			 * subplans with a dummy. (We can't just remove it from the global
			 * list, because that would screw up the plan_id numbering of the
			 * subplans).
			 */
			pfree(subplan_plan);
			subplan_plan = (Plan *) make_result(root, NIL,
												(Node *) list_make1(makeBoolConst(false, false)),
												NULL);
		}

		/* Replace subplan in global subplan list. */
		lfirst(planlist_cell) = subplan_plan;
	}
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
static void
prescan_plantree(Plan *plan, PlanProfile *context)
{
	(void) prescan_walker((Node *) plan, context);
}

static void
prescan_subplan(int plan_id, PlanProfile *context)
{
	PlanProfileSubPlanInfo *spinfo = &context->subplan_info[plan_id];
	Plan	   *subplan_plan = (Plan *) list_nth(context->root->glob->subplans, plan_id - 1);

	Assert(spinfo->seen);
	Assert(is_plan_node((Node *) subplan_plan));

	/*
	 * Init plan and main plan are dispatched separately,
	 * so we need to set DISPATCH_PARALLEL flag separately
	 * for them.
	 *
	 * save the parallel state of main plan. init plan
	 * should not effect main plan.
	 */
	if (spinfo->is_initplan)
	{
		bool		savedMainPlanParallel = context->dispatchParallel;

		context->dispatchParallel = false;

		(void) prescan_walker((Node *) subplan_plan, context);

		/* mark init plan parallel state and restore it for main plan */
		if (context->dispatchParallel)
		{
			spinfo->initPlanParallel = true;
			context->anyInitPlanParallel = true;
		}

		context->dispatchParallel = savedMainPlanParallel;
	}
	else
	{
		(void) prescan_walker((Node *) subplan_plan, context);
	}
}

static bool
prescan_walker(Node *node, PlanProfile *context)
{
	Flow	   *savedPlanFlow = context->currentPlanFlow;
	bool		result;

	if (node == NULL)
		return false;

	if (is_plan_node(node))
	{
		Plan	   *plan = (Plan *) node;

		if (plan->dispatch == DISPATCH_PARALLEL)
			context->dispatchParallel = true;

		if (plan->flow &&
			(plan->flow->flotype == FLOW_PARTITIONED
			 || plan->flow->flotype == FLOW_REPLICATED))
			context->dispatchParallel = true;

		/*
		 * Not all Plan types set the Flow information. If it's missing,
		 * assume it's the same as parent's
		 */
		if (plan->flow)
			context->currentPlanFlow = plan->flow;
	}

	if (IsA(node, SubPlan))
	{
		SubPlan	   *spexpr = (SubPlan *) node;
		PlanProfileSubPlanInfo *spinfo = &context->subplan_info[spexpr->plan_id];

		/*
		 * The Plans of the subqueries are handled separately, after the pass
		 * over the main plan tree. Remember the context where this SubPlan
		 * reference occurred, so that we can set the flow that the subplan
		 * needs to feed into correctly, in that separate pass.
		 *
		 * XXX: If there are multiple SubPlan references with the same plan_id,
		 * but in different slices, that would spell trouble, if the subplan's
		 * Plan tree also contained Motions, because same Motion plan would
		 * essentially have to send rows to two different receiver slices.
		 * We wrap potentially problematic SubPlans in PlaceHolderVars during
		 * planning to avoid that (see make_placeholders_for_subplans() in
		 * src/backend/optimizer/util/placeholder.c), but it would be nice to
		 * check for that here. However, we cannot detect that case here,
		 * because we haven't assigned slices yet.
		 */
		if (!context->currentPlanFlow)
			elog(ERROR, "SubPlan's parent Plan has no Flow information");

		if (!spinfo->seen)
		{
			spinfo->seen = true;
			spinfo->is_initplan = spexpr->is_initplan;
			spinfo->is_multirow = spexpr->is_multirow;
			spinfo->hasParParam = (spexpr->parParam) != NIL;
			spinfo->subLinkType = spexpr->subLinkType;
			spinfo->parentFlow = context->currentPlanFlow;

			context->unvisited_subplans = lappend_int(context->unvisited_subplans, spexpr->plan_id);
		}
		else
		{
			Assert(spinfo->is_initplan == spexpr->is_initplan);
			Assert(spinfo->is_multirow == spexpr->is_multirow);
			Assert(spinfo->hasParParam == ((spexpr->parParam) != NIL));
			Assert(spinfo->subLinkType == spexpr->subLinkType);
		}
	}

	if (IsA(node, Motion))
		context->dispatchParallel = true;

	/* don't recurse into plan trees of SubPlans */
	result = plan_tree_walker(node, prescan_walker, context, false);

	/* restore saved flow */
	context->currentPlanFlow = savedPlanFlow;

	return result;
}


/*
 * Construct a new Flow in the current memory context.
 */
Flow *
makeFlow(FlowType flotype, int numsegments)
{
	Flow	   *flow = makeNode(Flow);

	Assert(numsegments > 0);
	if (numsegments == GP_POLICY_INVALID_NUMSEGMENTS())
	{
		Assert(!"what's the proper value of numsegments?");
	}

	flow->flotype = flotype;
	flow->locustype = CdbLocusType_Null;
	flow->numsegments = numsegments;

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
	else if (IsA(plan, ModifyTable))
		Assert(list_member(((ModifyTable *) plan)->plans, subplan));
	else
		Assert(subplan == plan->lefttree);

	new_flow = makeFlow(model_flow->flotype, model_flow->numsegments);

	if (model_flow->flotype == FLOW_SINGLETON)
		new_flow->segindex = model_flow->segindex;

	/* Pull up hash key exprs, if they're all present in the plan's result. */
	else if (model_flow->flotype == FLOW_PARTITIONED &&
			 model_flow->hashExprs)
	{
		if (!is_projection_capable_plan(plan) ||
			cdbpullup_isExprCoveredByTargetlist((Expr *) model_flow->hashExprs,
												plan->targetlist))
		{
			new_flow->hashExprs = copyObject(model_flow->hashExprs);
			new_flow->hashOpfamilies = copyObject(model_flow->hashOpfamilies);
		}
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
		case T_MergeAppend:
		case T_SeqScan:
		case T_SampleScan:
		case T_ExternalScan:
		case T_IndexScan:
		case T_BitmapIndexScan:
		case T_BitmapHeapScan:
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
		case T_ForeignScan:
			if (plan_tree_walker(node, motion_sanity_walker, result, true))
				return true;
			break;

		case T_Motion:
			if (plan_tree_walker(node, motion_sanity_walker, result, true))
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



/*
 * Functions focusPlan, broadcastPlan, and repartitionPlan add a Motion node
 * on top of the given plan tree. If the input plan is already distributed
 * in the requested way, returns the input plan unmodified. If there is a
 * a different kind of Motion node at the top of the plan already, it is
 * replaced with the right kind of Motion. Also, if there is a Material
 * node at the top, it will be removed; a Motion node cannot be rescanned,
 * so materializing its input would be pointless.
 *
 * plan		   the plan to annotate
 *
 * stable	   if true, the result plan must present the result in the
 *			   order specified in the flow (which, presumably, matches
 *			   the input order.
 *
 * Remaining arguments, if any, determined by the requirements of the specific
 * function.
 */

/*
 * Function: focusPlan
 */
Plan *
focusPlan(Plan *plan, bool stable)
{
	Assert(plan->flow && plan->flow->flotype != FLOW_UNDEFINED);

	/*
	 * Already focused and flow is CdbLocusType_SingleQE, CdbLocusType_Entry,
	 * do nothing.
	 */
	if (plan->flow->flotype == FLOW_SINGLETON &&
		plan->flow->locustype != CdbLocusType_SegmentGeneral)
		return plan;

	/* Add motion operator. If there is a single sender, the order is preserved. */
	if (stable && plan->flow->flotype != FLOW_SINGLETON)
		elog(ERROR, "sorted focusPlan not supported");

	if (IsA(plan, Motion) || IsA(plan, Material))
	{
		return focusPlan(plan->lefttree, stable);
	}

	return (Plan *) make_union_motion(plan, plan->flow->numsegments);
}

/*
 * Function: broadcastPlan
 */
Plan *
broadcastPlan(Plan *plan, bool stable, int numsegments)
{
	Assert(plan->flow && plan->flow->flotype != FLOW_UNDEFINED);

	/*
	 * Already focused and flow is CdbLocusType_SegmentGeneral and data
	 * is replicated on every segment of target, do nothing.
	 */
	if (plan->flow->flotype == FLOW_SINGLETON &&
		plan->flow->locustype == CdbLocusType_SegmentGeneral &&
		plan->flow->numsegments >= numsegments)
		return plan;

	if (IsA(plan, Motion) || IsA(plan, Material))
	{
		return broadcastPlan(plan->lefttree, stable, numsegments);
	}

	return (Plan *) make_broadcast_motion(plan, numsegments);
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
Plan *
repartitionPlan(Plan *plan, bool stable,
				List *hashExprs, List *hashOpfamilies,
				int numsegments)
{
	Assert(plan->flow);
	Assert(plan->flow->flotype == FLOW_PARTITIONED ||
		   plan->flow->flotype == FLOW_SINGLETON);

	/* Already partitioned on the given hashExpr?  Do nothing. */
	if (hashExprs && plan->flow->numsegments == numsegments &&
		plan->flow->locustype == CdbLocusType_Hashed)
	{
		if (equal(hashOpfamilies, plan->flow->hashOpfamilies) &&
			loci_compatible(hashExprs, plan->flow->hashExprs))
		{
			return plan;
		}
	}

	if (IsA(plan, Motion) || IsA(plan, Material))
	{
		return repartitionPlan(plan->lefttree, stable,
							   hashExprs, hashOpfamilies,
							   numsegments);
	}

	return (Plan *) make_hashed_motion(plan, hashExprs, hashOpfamilies,
									   numsegments);
}
