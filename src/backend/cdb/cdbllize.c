/*-------------------------------------------------------------------------
 *
 * cdbllize.c
 *	  Parallelize a PostgreSQL sequential plan tree.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 * This file contains functions to process plan tree, at various stages in
 * planning, to produce a parallelize MPP plan. Outline of the stages,
 * as they happen in standard_planner():
 *
 * 1. Call subquery_planner(), to build the best Path.
 *
 * 2. Call cdbllize_adjust_top_path().
 *
 *    cdbllize_adjust_top_path() adds Motion paths on top of the best Path, so
 *    that the query's final result is made available at the correct nodes.
 *    That's usually the QD node, so that the QD can send the query result
 *    back to the client, but in a DML statement or CREATE TABLE AS, the final
 *    result might be needed in the segments instead.
 *
 *    (There's a similar function for Init Plans,
 *    cdbllize_adjust_init_plan_path(). The planner calls it when it builds
 *    Paths for SubPlans that are run as Init Plans before the main query.)
 *
 * 3. Call create_plan() to turn the best Path into a Plan tree.
 *
 * 4. Call cdbllize_decorate_subplans_with_motions()
 *
 *    cdbllize_decorate_subplans_with_motions() adds Motions to subplans as
 *    needed, to make subplan results available in the parent nodes.
 *
 * 5. Call set_plan_references()
 *
 * 6. Call cdbllize_build_slice_table().
 *
 *    cdbllize_build_slice_table() assigns slice IDs to Motion nodes, and
 *    builds the slice table needed by the executor to set up the gang of
 *    processes and the interconnects between them. It also eliminates unused
 *    subplans, and zaps some fields from the Query tree that are not needed
 *    after planning, to reduce the amount of data that needs to be dispatched.
 *
 *
 * In addition to the above-mentioned functions, there's a
 * motion_sanity_check() function, which is used to check that the query
 * plan doesn't contain deadlock hazards involving Motions across nodes.
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbllize.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/pg_am.h"
#include "commands/defrem.h"
#include "optimizer/clauses.h"
#include "optimizer/pathnode.h"
#include "nodes/primnodes.h"
#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"
#include "nodes/print.h"

#include "optimizer/paths.h"
#include "optimizer/planmain.h" /* for make_result() */
#include "optimizer/var.h"
#include "parser/parsetree.h"	/* for rt_fetch() */
#include "nodes/makefuncs.h"	/* for makeTargetEntry() */
#include "utils/guc.h"			/* for Debug_pretty_print */
#include "utils/lsyscache.h"

#include "cdb/cdbhash.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbpath.h"
#include "cdb/cdbplan.h"
#include "cdb/cdbpullup.h"
#include "cdb/cdbllize.h"
#include "cdb/cdbmutate.h"
#include "cdb/cdbutil.h"
#include "optimizer/tlist.h"

/*
 * decorate_subplans_with_motions_context holds state for the recursive
 * cdbllize_decorate_subplans_with_motions() function.
 */
typedef struct decorate_subplan_info
{
	int			sliceDepth;
	Flow	   *parentFlow;
	bool		is_initplan;
	bool		useHashTable;
	bool		processed;

} decorate_subplan_info;

typedef struct decorate_subplans_with_motions_context
{
	plan_tree_base_prefix base; /* Required prefix for
								 * plan_tree_walker/mutator */

	/* Queue of subplan IDs that we seen, but processed yet. */
	List	   *subplan_workingQueue;

	/*
	 * array, one element for each subplan. Use subplans[plan_id - 1] to
	 * access the struct for a particular subplan.
	 */
	decorate_subplan_info *subplans;

	/* Current position in the tree. */
	int			sliceDepth;
	Flow	   *currentPlanFlow;
} decorate_subplans_with_motions_context;

/* State for the recursive build_slice_table() function. */
typedef struct
{
	plan_tree_base_prefix base; /* Required prefix for
								 * plan_tree_walker/mutator */

	/*
	 * PlanSlice structs. Indexed by slice Id. This List is converted
	 * into an array, for PlannedStmt->slices, at the end of
	 * build_slice_table().
	 */
	List	   *slices;

	/* Current slice in the traversal. */
	int			currentSliceIndex;

	/*
	 * Subplan IDs that we have seen. Used to prevent scanning the
	 * same subplan more than once, even if there are multiple SubPlan
	 * nodes referring it, and to eliminate unused subplans.
	 */
	Bitmapset  *seen_subplans;
} build_slice_table_context;

/*
 * Forward Declarations
 */
static Node *fix_outer_query_motions_mutator(Node *node, decorate_subplans_with_motions_context *context);
static Plan *fix_subplan_motion(PlannerInfo *root, Plan *subplan, Flow *outer_query_flow);
static bool build_slice_table_walker(Node *node, build_slice_table_context *context);

/*
 * Create a GpPolicy that matches the natural distribution of the given plan.
 *
 * This is used with CREATE TABLE AS, to derive the distribution
 * key for the table from the query plan.
 */
static GpPolicy *
get_partitioned_policy_from_path(PlannerInfo *root, Path *path)
{
	/* Find out what the flow is partitioned on */
	List	   *policykeys;
	List	   *policyopclasses;
	ListCell   *dk_cell;
	ListCell   *ec_cell;
	ListCell   *em_cell;

	/*
	 * Is it a Hashed distribution?
	 *
	 * NOTE: HashedOJ is not OK, because we cannot let the NULLs be stored
	 * multiple segments.
	 */
	if (path->locus.locustype != CdbLocusType_Hashed)
	{
		return NULL;
	}

	policykeys = NIL;
	policyopclasses = NIL;

	foreach(dk_cell, path->locus.distkey)
	{
		DistributionKey *dk = lfirst(dk_cell);
		bool		found_expr = false;

		foreach(ec_cell, dk->dk_eclasses)
		{
			EquivalenceClass *ec = lfirst(ec_cell);

			while (ec->ec_merged)
				ec = ec->ec_merged;

			foreach(em_cell, ec->ec_members)
			{
				EquivalenceMember *em = lfirst(em_cell);
				Expr	   *var1 = (Expr *) em->em_expr;
				AttrNumber	attno;
				ListCell   *tle_cell;

				/*
				 * Right side variable may be encapsulated by a relabel node.
				 * Motion, however, does not care about relabel nodes.
				 */
				if (IsA(var1, RelabelType))
					var1 = ((RelabelType *) var1)->arg;

				/* See if this Expr is a column of the result table */
				attno = 0;
				foreach(tle_cell, root->processed_tlist)
				{
					TargetEntry *target = lfirst(tle_cell);

					attno++;

					if (target->resjunk)
						continue;

					if (equal(var1, target->expr))
					{
						/*
						 * If it is, use it to partition the result table, to avoid
						 * unnecessary redistribution of data
						 */
						Oid			opclass;
						Oid			eqop;
						Oid			typeid;
						ListCell   *opfam_cell;

						Assert(list_length(policykeys) < MaxPolicyAttributeNumber);

						if (list_member_int(policykeys, attno))
							ereport(ERROR,
									(errcode(ERRCODE_DUPLICATE_COLUMN),
									 errmsg("duplicate DISTRIBUTED BY column '%s'",
											target->resname ? target->resname : "???")));

						/*
						 * We know the btree operator family corresponding to
						 * the distribution, but we don't know the exact
						 * hash operator class that corresponds to that. In
						 * the common case, the datatype has exactly one
						 * default operator class, and you usually use only
						 * that. So look up the default operator class for the
						 * datatype, and if it's compatible with the btree
						 * operator family, use that.
						 *
						 * If that fails, we could do some further checks. We
						 * could check if there is some other operator class
						 * for the datatype, and if so, use that. But it
						 * doesn't seem worth adding much extra code to deal
						 * with more obscure cases. Deriving the distribution
						 * key from the query plan is a heuristic, anyway.
						 */
						typeid = exprType((Node *) target->expr);

						opclass = GetDefaultOpClass(typeid, HASH_AM_OID);
						eqop = cdb_eqop_in_hash_opfamily(get_opclass_family(opclass), typeid);
						foreach(opfam_cell, ec->ec_opfamilies)
						{
							Oid			btopfamily = lfirst_oid(opfam_cell);

							if (get_op_opfamily_strategy(eqop, btopfamily))
							{
								policykeys = lappend_int(policykeys, attno);
								policyopclasses = lappend_oid(policyopclasses, opclass);
								found_expr = true;
								break;
							}
						}
					}
					if (found_expr)
						break;
				}
				if (found_expr)
					break;
			}
			if (found_expr)
				break;
		}

		if (!found_expr)
		{
			/*
			 * This distribution key is not present in the target list. Give
			 * up.
			 */
			return NULL;
		}
	}

	/*
	 * We use the default number of segments, even if the flow was partially
	 * distributed. That defeats the performance benefit of using the same
	 * distribution key columns, because we'll need a Restribute Motion
	 * anyway. But presumably if the user had expanded the cluster, they want
	 * to use all the segments for new tables.
	 */
	return createHashPartitionedPolicy(policykeys,
									   policyopclasses,
									   GP_POLICY_DEFAULT_NUMSEGMENTS());
}

/*
 * cdbllize_adjust_top_path -- Adjust top Path for MPP
 *
 * Add a Motion to the top of the query path, so that the final result
 * is distributed correctly for the kind of query. For example, an INSERT
 * statement's result must be sent to the correct segments where the data
 * needs to be inserted, and a SELECT query's result must be brought to the
 * dispatcher, so that it can be sent to the client.
 *
 * For a CREATE TABLE AS command, this also determines the distribution
 * policy for the table, if it was not given explicitly with DISTRIBUTED BY.
 *
 * The input is a Path, produced by subquery_planner(). The result is a
 * Path, with a Motion added on top, if needed. Also, *topslice is adjusted
 * to reflect how/where the top slice needs to be executed.
 */
Path *
cdbllize_adjust_top_path(PlannerInfo *root, Path *best_path, PlanSlice *topslice)
{
	Query	   *query = root->parse;
	GpPolicy   *targetPolicy = NULL;

	/*
	 * NOTE: This code makes the assumption that if we are working on a
	 * hierarchy of tables, all the tables are distributed, or all are on the
	 * entry DB.  Any mixture will fail.
	 */
	if (query->resultRelation > 0)
	{
		RangeTblEntry *rte = rt_fetch(query->resultRelation, query->rtable);

		Assert(rte->rtekind == RTE_RELATION);

		targetPolicy = GpPolicyFetch(rte->relid);
	}

	if (query->commandType == CMD_SELECT && query->parentStmtType == PARENTSTMTTYPE_CTAS)
	{
		/* CREATE TABLE AS or SELECT INTO */
		if (query->intoPolicy != NULL)
		{
			targetPolicy = query->intoPolicy;

			Assert(query->intoPolicy->ptype != POLICYTYPE_ENTRY);
			Assert(query->intoPolicy->nattrs >= 0);
			Assert(query->intoPolicy->nattrs <= MaxPolicyAttributeNumber);
		}
		else if (gp_create_table_random_default_distribution)
		{
			targetPolicy = createRandomPartitionedPolicy(GP_POLICY_DEFAULT_NUMSEGMENTS());
			ereport(NOTICE,
					(errcode(ERRCODE_SUCCESSFUL_COMPLETION),
					 errmsg("using default RANDOM distribution since no distribution was specified"),
					 errhint("Consider including the 'DISTRIBUTED BY' clause to determine the distribution of rows.")));
		}
		else
		{
			/* First try to deduce the distribution from the query */
			targetPolicy = get_partitioned_policy_from_path(root, best_path);

			/*
			 * If that fails, hash on the first hashable column we can
			 * find.
			 */
			if (!targetPolicy)
			{
				int			i;
				List	   *policykeys = NIL;
				List	   *policyopclasses = NIL;
				ListCell   *lc;

				i = 0;
				foreach(lc, best_path->pathtarget->exprs)
				{
					Oid			typeOid = exprType((Node *) lfirst(lc));
					Oid			opclass = InvalidOid;

					/*
					 * Check for a legacy hash operator class if
					 * gp_use_legacy_hashops GUC is set. If
					 * InvalidOid is returned or the GUC is not
					 * set, we'll get the default operator class.
					 */
					if (gp_use_legacy_hashops)
						opclass = get_legacy_cdbhash_opclass_for_base_type(typeOid);

					if (!OidIsValid(opclass))
						opclass = cdb_default_distribution_opclass_for_type(typeOid);


					if (OidIsValid(opclass))
					{
						policykeys = lappend_int(policykeys, i + 1);
						policyopclasses = lappend_oid(policyopclasses, opclass);
						break;
					}
					i++;
				}
				targetPolicy = createHashPartitionedPolicy(policykeys,
														   policyopclasses,
														   GP_POLICY_DEFAULT_NUMSEGMENTS());
			}

			/* If we deduced the policy from the query, give a NOTICE */
			if (query->parentStmtType == PARENTSTMTTYPE_CTAS)
			{
				StringInfoData columnsbuf;
				int			i;

				initStringInfo(&columnsbuf);
				for (i = 0; i < targetPolicy->nattrs; i++)
				{
					TargetEntry *target = get_tle_by_resno(root->processed_tlist, targetPolicy->attrs[i]);

					if (i > 0)
						appendStringInfoString(&columnsbuf, ", ");
					if (target->resname)
						appendStringInfoString(&columnsbuf, target->resname);
					else
						appendStringInfoString(&columnsbuf, "???");

				}
				ereport(NOTICE,
						(errcode(ERRCODE_SUCCESSFUL_COMPLETION),
						 errmsg("Table doesn't have 'DISTRIBUTED BY' clause -- Using column(s) "
								"named '%s' as the Greenplum Database data distribution key for this "
								"table. ", columnsbuf.data),
						 errhint("The 'DISTRIBUTED BY' clause determines the distribution of data."
								 " Make sure column(s) chosen are the optimal data distribution key to minimize skew.")));
			}
		}
		Assert(targetPolicy->ptype != POLICYTYPE_ENTRY);

		query->intoPolicy = targetPolicy;

		if (GpPolicyIsReplicated(targetPolicy))
		{
			CdbPathLocus replicatedLocus;

			CdbPathLocus_MakeReplicated(&replicatedLocus,
										targetPolicy->numsegments);

			best_path = cdbpath_create_motion_path(root,
												   best_path,
												   NIL,
												   false,
												   replicatedLocus);
		}
		else
		{
			/*
			 * Make sure the top level flow is partitioned on the
			 * partitioning key of the target relation.	Since this is
			 * a SELECT INTO (basically same as an INSERT) command,
			 * the target list will correspond to the attributes of
			 * the target relation in order.
			 */
			best_path = create_motion_path_for_ctas(root, targetPolicy,
													best_path);
		}

		topslice->numsegments = targetPolicy->numsegments;
		topslice->segindex = 0;
		topslice->gangType = (targetPolicy->ptype == POLICYTYPE_ENTRY) ? GANGTYPE_UNALLOCATED : GANGTYPE_PRIMARY_WRITER;
	}
	else if (query->commandType == CMD_SELECT && query->parentStmtType == PARENTSTMTTYPE_REFRESH_MATVIEW)
	{
		/*
		 * REFRESH MATERIALIZED VIEW
		 *
		 * This is the same as the logic for CREATE TABLE AS with an explicit
		 * DISTRIBUTED BY above.
		 */
		targetPolicy = query->intoPolicy;
		if (!targetPolicy)
			elog(ERROR, "materialized view has no distribution policy");
		if (targetPolicy->ptype == POLICYTYPE_ENTRY)
			elog(ERROR, "materialized view with entry distribution policy not supported");

		if (GpPolicyIsReplicated(targetPolicy))
		{
			CdbPathLocus broadcastLocus;

			CdbPathLocus_MakeSegmentGeneral(&broadcastLocus,
											targetPolicy->numsegments);

			best_path = cdbpath_create_motion_path(root,
												   best_path,
												   NIL,
												   false,
												   broadcastLocus);
		}
		else
		{
			/*
			 * Make sure the top level flow is partitioned on the
			 * partitioning key of the target relation.	Since this is
			 * a SELECT INTO (basically same as an INSERT) command,
			 * the target list will correspond to the attributes of
			 * the target relation in order.
			 */
			best_path = create_motion_path_for_insert(root, targetPolicy,
													  best_path);
		}

		topslice->numsegments = targetPolicy->numsegments;
		topslice->segindex = 0;
		topslice->gangType = (targetPolicy->ptype == POLICYTYPE_ENTRY) ? GANGTYPE_UNALLOCATED : GANGTYPE_PRIMARY_WRITER;
	}
	else if (query->commandType == CMD_SELECT && query->parentStmtType == PARENTSTMTTYPE_COPY)
	{
		/* COPY (SELECT ...) TO */
		topslice->directDispatch.isDirectDispatch = false;
		topslice->directDispatch.haveProcessedAnyCalculations = true;
		topslice->numsegments = getgpsegmentCount();
		topslice->segindex = 0;
		topslice->gangType = GANGTYPE_PRIMARY_READER;
	}
	else if (query->commandType == CMD_SELECT ||
			 query->commandType == CMD_INSERT ||
			 query->commandType == CMD_UPDATE ||
			 query->commandType == CMD_DELETE)
	{
		Assert(query->parentStmtType == PARENTSTMTTYPE_NONE);

		if (query->commandType == CMD_SELECT || query->returningList)
		{
			/*
			 * Query result needs to be brought back to the QD. Ask for
			 * a motion to bring it in. If the result already has the
			 * right locus, cdbpath_create_motion_path() will return it
			 * unmodified.
			 *
			 * If the query has an ORDER BY clause, use Merge Receive to
			 * preserve the ordering. The plan has already been set up to
			 * ensure each qExec's result is properly ordered according to
			 * the ORDER BY specification.
			 */
			CdbPathLocus entryLocus;

			CdbPathLocus_MakeEntry(&entryLocus);

			/*
			 * In a Motion to Entry locus, the numsegments indicates the
			 * number of segments in the *sender*.
			 */
			entryLocus.numsegments = best_path->locus.numsegments;

			best_path = cdbpath_create_motion_path(root,
												   best_path,
												   root->sort_pathkeys,
												   false,
												   entryLocus);
			topslice->numsegments = 1;
			topslice->segindex = 0;
			topslice->gangType = GANGTYPE_UNALLOCATED;
		}
		else
		{
			topslice->numsegments = targetPolicy->numsegments;
			topslice->segindex = 0;
			topslice->gangType = (targetPolicy->ptype == POLICYTYPE_ENTRY) ? GANGTYPE_UNALLOCATED : GANGTYPE_PRIMARY_WRITER;
		}
	}
	else if (query->commandType == CMD_UTILITY)
	{
		/* nothing to do */
	}
	else
		elog(ERROR, "unknown command type %d", query->commandType);

	return best_path;
}

/*
 * cdbllize_adjust_init_plan_path -- Adjust Init Plan's Path for MPP
 *
 * Init Plans are dispatched and executed before the main query. Adjust the
 * Path to bring the result back to the QD.
 */
Path *
cdbllize_adjust_init_plan_path(PlannerInfo *root, Path *best_path)
{
	CdbPathLocus entryLocus;

	CdbPathLocus_MakeEntry(&entryLocus);

	/*
	 * In a Motion to Entry locus, the numsegments indicates the
	 * number of segments in the *sender*.
	 */
	entryLocus.numsegments = best_path->locus.numsegments;

	return cdbpath_create_motion_path(root,
									  best_path,
									  root->sort_pathkeys,
									  false,
									  entryLocus);
}

/*
 * cdbllize_decorate_subplans_with_motions - Adjust subplans for MPP.
 *
 * This function adds Motion nodes on top of plan trees for SubPlans, so that
 * the subplan results are available at the correct segment for the outer
 * query. It also fixes up any MOTIONTYPE_OUTER_QUERY Motions, changing them
 * into Gather or Broadcast Motions, depending on the parent slice. The plan
 * tree can contain outer-query Motions, even if there are no SubPlans, if it
 * contains subquery RTEs.
 *
 * The input is a Plan tree, from create_plan().
 *
 * Outline of processing:
 *
 * - Walk through the main Plan tree. Make note of all SubPlan expressions,
 *   and the context they appear in.
 *
 * - Recurse to process each subplan that was encountered. At the top of the
 *   subplan, add a Motion if needed, so that the result of the subplan is
 *   available in the outer slice. For example, if the SubPlan appears in
 *   a slice that's executed only on a single QE node, then the subplan's
 *   result must also be present at that node. A Gather Motion is put on
 *   top of the subplan, if needed. Or if the parent slice is executes on all
 *   nodes (Replicated or Partitioned), then a Broadcast Motion is put on
 *   top of the subplan, unless the subplan's result is already available on
 *   all QEs.
 *
 * - If any of the subplans contained more SubPlan expressions, process those
 *   too.
 *
 * The result is a deep copy of the argument Plan tree with added/modified
 * Motion nodes, or the original Plan tree unmodified if no changes are
 * needed.
 *
 * Note: Subqueries in from-list can contain MOTIONTYPE_OUTER_QUERY Motions,
 * too, so we need to scan the tree even if there are no SubPlans.
 */
Plan *
cdbllize_decorate_subplans_with_motions(PlannerInfo *root, Plan *plan)
{
	Plan	   *result;
	decorate_subplans_with_motions_context context;
	int			nsubplans;

	/* Initialize mutator context. */
	planner_init_plan_tree_base(&context.base, root);
	context.sliceDepth = 0;
	context.subplan_workingQueue = NIL;

	nsubplans = list_length(root->glob->subplans);
	context.subplans = (decorate_subplan_info *)
		palloc((nsubplans + 1) * sizeof(decorate_subplan_info));
	for (int i = 1; i <= nsubplans; i++)
	{
		decorate_subplan_info *sstate = &context.subplans[i];

		sstate->sliceDepth = -1;
		sstate->parentFlow = NULL;
		sstate->is_initplan = false;
		sstate->processed = false;
	}

	/*
	 * Process the main plan tree.
	 */
	context.currentPlanFlow = plan->flow;
	result = (Plan *) fix_outer_query_motions_mutator((Node *) plan, &context);

	/*
	 * Process any SubPlans that were reachable from the main plan tree.
	 * While we process the SubPlans, we might encounter more SubPlans.
	 * They will be added to the working queue, so keep going until the
	 * working queue is empty.
	 */
	while (context.subplan_workingQueue)
	{
		int			plan_id = linitial_int(context.subplan_workingQueue);
		decorate_subplan_info *sstate = &context.subplans[plan_id];
		ListCell   *planlist_cell = list_nth_cell(root->glob->subplans, plan_id - 1);
		Plan	   *subplan = (Plan *) lfirst(planlist_cell);

		context.subplan_workingQueue = list_delete_first(context.subplan_workingQueue);

		if (sstate->processed)
		{
			if (!sstate->is_initplan)
				elog(ERROR, "SubPlan %d already processed", plan_id);
			else
				continue;
		}
		sstate->processed = true;

		if (sstate->is_initplan)
		{
			/*
			 * InitPlans are dispatched separately, before the main plan,
			 * and the result is brought to the QD.
			 */
			Flow	   *topFlow = makeFlow(FLOW_SINGLETON, getgpsegmentCount());

			topFlow->segindex = -1;
			context.sliceDepth = 0;
			context.currentPlanFlow = topFlow;
		}
		else
		{
			context.sliceDepth = sstate->sliceDepth;
			context.currentPlanFlow = sstate->parentFlow;
		}

		if (!subplan->flow)
			elog(ERROR, "subplan is missing Flow information");

		/*
		 * If the subquery result is not available where the outer query needs it,
		 * we have to add a Motion node to redistribute it.
		 */
		if (subplan->flow->locustype != CdbLocusType_OuterQuery &&
			subplan->flow->locustype != CdbLocusType_SegmentGeneral &&
			subplan->flow->locustype != CdbLocusType_General)
		{
			subplan = fix_subplan_motion(root, subplan, context.currentPlanFlow);

			/*
			 * If we created a Motion, protect it from rescanning. Init Plans
			 * and hashed SubPlans are never rescanned.
			 */
			if (IsA(subplan, Motion) && !sstate->is_initplan &&
				!sstate->useHashTable)
				subplan = (Plan *) make_material(subplan);
		}

		subplan = (Plan *) fix_outer_query_motions_mutator((Node *) subplan, &context);

		lfirst(planlist_cell) = subplan;
	}

	return result;
}


/*
 * Workhorse for cdbllize_fix_outer_query_motions().
 */
static Node *
fix_outer_query_motions_mutator(Node *node, decorate_subplans_with_motions_context *context)
{
	Node	   *newnode;
	Plan	   *plan;
	Flow	   *saveCurrentPlanFlow;

#ifdef USE_ASSERT_CHECKING
	PlannerInfo *root = (PlannerInfo *) context->base.node;
	Assert(root && IsA(root, PlannerInfo));
#endif

	if (node == NULL)
		return NULL;

	/* An expression node might have subtrees containing plans to be mutated. */
	if (!is_plan_node(node))
	{
		node = plan_tree_mutator(node, fix_outer_query_motions_mutator, context, false);

		/*
		 * If we see a SubPlan, remember the context where we saw it. We memorize
		 * the parent's node's Flow, so that we know where the SubPlan needs to
		 * bring the result. There might be multiple references to the same SubPlan,
		 * if e.g. the result of the SubPlan is passed up the plan for use in later
		 * joins or for the final target list. Remember the *deepest* slice level
		 * where we saw it. We cannot pass SubPlan's result down the tree, but we
		 * can propagate it upwards if we put it to the target list. So it needs
		 * to be calculated at the bottom level, and propagated up from there.
		 */
		if (IsA(node, SubPlan))
		{
			SubPlan	   *spexpr = (SubPlan *) node;
			decorate_subplan_info *sstate = &context->subplans[spexpr->plan_id];

			sstate->is_initplan = spexpr->is_initplan;
			sstate->useHashTable = spexpr->useHashTable;

			if (sstate->processed)
				elog(ERROR, "found reference to already-processed SubPlan");
			if (sstate->sliceDepth == -1)
				context->subplan_workingQueue = lappend_int(context->subplan_workingQueue,
															spexpr->plan_id);

			if (!spexpr->is_initplan && context->sliceDepth > sstate->sliceDepth)
			{
				sstate->sliceDepth = context->sliceDepth;
				sstate->parentFlow = context->currentPlanFlow;
			}
		}
		return node;
	}

	plan = (Plan *) node;

	if (IsA(plan, Motion))
	{
		Motion	   *motion = (Motion *) plan;

		/* sanity check: Sub plan must have flow */
		Assert(motion->plan.lefttree->flow);

		/* sanity check: we haven't assigned slice/motion IDs yet */
		Assert(motion->motionID == 0);

		/*
		 * Recurse into the Motion's initPlans and other fields, and the
		 * subtree.
		 *
		 * All the fields on the Motion node are considered part of the sending
		 * slice.
		 */
		saveCurrentPlanFlow = context->currentPlanFlow;
		if (plan->lefttree->flow->locustype != CdbLocusType_OuterQuery)
			context->currentPlanFlow = plan->lefttree->flow;
		context->sliceDepth++;

		plan = (Plan *) plan_tree_mutator((Node *) plan,
										  fix_outer_query_motions_mutator,
										  context,
										  false);
		motion = (Motion *) plan;
		context->sliceDepth--;
		context->currentPlanFlow = saveCurrentPlanFlow;

		/*
		 * If this is a Motion node in a correlated SubPlan, where we bring
		 * the result to the parent, the Motion was marked as type
		 * MOTIONTYPE_OUTER_QUERY. Because we didn't know whether the parent
		 * is distributed, replicated, or a single QD/QE process, when the
		 * subquery was planned, we fix that up here. Modify the Motion node
		 * so that it brings the result to the parent.
		 */
		if (motion->motionType == MOTIONTYPE_OUTER_QUERY)
		{
			Assert(!motion->sendSorted);

			if (context->currentPlanFlow->flotype == FLOW_SINGLETON)
			{
				motion->motionType = MOTIONTYPE_GATHER;
			}
			else if (context->currentPlanFlow->flotype == FLOW_REPLICATED ||
					 context->currentPlanFlow->flotype == FLOW_PARTITIONED)
			{
				motion->motionType = MOTIONTYPE_BROADCAST;
			}
			else
				elog(ERROR, "unexpected Flow type in parent of a SubPlan");
		}

		/*
		 * For non-top slice, if this motion is QE singleton and subplan's locus
		 * is CdbLocusType_SegmentGeneral, omit this motion.
		 */
		if (context->sliceDepth > 0 &&
			context->currentPlanFlow->flotype == FLOW_SINGLETON &&
			context->currentPlanFlow->segindex == 0 &&
			motion->plan.lefttree->flow->locustype == CdbLocusType_SegmentGeneral)
		{
			/*
			 * Omit this motion. If there were any InitPlans attached to it,
			 * make sure we keep them.
			 */
			Plan	   *child;

			child = motion->plan.lefttree;
			child->initPlan = list_concat(child->initPlan, motion->plan.initPlan);

			newnode = (Node *) child;
		}
		else
		{
			newnode = (Node *) motion;
		}
	}
	else
	{
		/*
		 * Copy this node and mutate its children. Afterwards, this node should be
		 * an exact image of the input node, except that contained nodes requiring
		 * parallelization will have had it applied.
		 */
		saveCurrentPlanFlow = context->currentPlanFlow;
		if (plan->flow != NULL && plan->flow->locustype != CdbLocusType_OuterQuery)
			context->currentPlanFlow = plan->flow;
		newnode = plan_tree_mutator(node, fix_outer_query_motions_mutator, context, false);
		context->currentPlanFlow = saveCurrentPlanFlow;
	}

	return newnode;
}


/*
 * Add a Motion node on top of a Plan if needed, to make the result available
 * in 'outer_query_flow'. Subroutine of cdbllize_fix_outer_query_motions().
 */
static Plan *
fix_subplan_motion(PlannerInfo *root, Plan *subplan, Flow *outer_query_flow)
{
	bool		need_motion;

	if (outer_query_flow->flotype == FLOW_SINGLETON)
	{
		if (subplan->flow->flotype != FLOW_SINGLETON)
			need_motion = true;
		else
			need_motion = false;
	}
	else if (outer_query_flow->flotype == FLOW_REPLICATED ||
			 outer_query_flow->flotype == FLOW_PARTITIONED)
	{
		// FIXME: Isn't it pointless to broadcast if it's already replicated?
		need_motion = true;
	}
	else
		elog(ERROR, "unexpected flow type %d in parent of SubPlan expression",
			 outer_query_flow->flotype);

	if (need_motion)
	{
		/*
		 * We need to add a Motion to the top.
		 */
		PlanSlice  *sendSlice;
		Motion	   *motion;
		Flow	   *subFlow = subplan->flow;
		List	   *initPlans = NIL;

		Assert(subFlow);

		/*
		 * Strip off any Material nodes at the top. There's no point in
		 * materializing just below a Motion, because a Motion is never
		 * rescanned.
		 *
		 * If we strip off any nodes, make sure we preserve any initPlans
		 * that were attached to them.
		 */
		while (IsA(subplan, Material))
		{
			initPlans = list_concat(initPlans, subplan->initPlan);
			subplan = subplan->lefttree;
		}

		/*
		 * If there's an existing Motion on top, we can remove it, and
		 * replace it with a new Motion. But make sure we reuse the sender
		 * slice information. We could build a new one here, but then we'd
		 * lose any direct dispatch information.
		 *
		 * Otherwise, build a new sender slice.
		 */
		if (IsA(subplan, Motion))
		{
			Motion *strippedMotion = (Motion *) subplan;

			sendSlice = strippedMotion->senderSliceInfo;
			subplan = subplan->lefttree;
			initPlans = list_concat(initPlans, strippedMotion->plan.initPlan);
		}
		else
		{
			sendSlice = palloc0(sizeof(PlanSlice));
			sendSlice->gangType = GANGTYPE_PRIMARY_READER;
			sendSlice->sliceIndex = -1;

			if (subFlow->flotype != FLOW_SINGLETON)
			{
				sendSlice->numsegments = subFlow->numsegments;
				sendSlice->segindex = 0;
			}
			else
			{
				if (subFlow->segindex == -1)
					sendSlice->gangType = GANGTYPE_ENTRYDB_READER;
				else
					sendSlice->gangType = GANGTYPE_SINGLETON_READER;
				sendSlice->numsegments = 1;
				sendSlice->segindex = subFlow->segindex;
			}
		}

		motion = make_motion(NULL, subplan,
							 0, NULL, NULL, NULL, NULL /* no ordering */);
		motion->motionType = (outer_query_flow->flotype == FLOW_SINGLETON) ?
			MOTIONTYPE_GATHER : MOTIONTYPE_BROADCAST;
		motion->hashExprs = NIL;
		motion->hashFuncs = NULL;
		motion->plan.lefttree->flow = subFlow;
		motion->plan.initPlan = initPlans;
		motion->senderSliceInfo = sendSlice;

		subplan = (Plan *) motion;
	}
	return subplan;
}

/*
 * cdbllize_build_slice_table -- Build the (planner) slice table for a query.
 */
void
cdbllize_build_slice_table(PlannerInfo *root, Plan *top_plan,
						   PlanSlice *top_slice)
{
	PlannerGlobal *glob = root->glob;
	Query	   *query = root->parse;
	build_slice_table_context cxt;
	ListCell   *lc;
	int			sliceIndex;
	bool		all_root_slices;

	/*
	 * This can modify nodes, so the nodes we memorized earlier are no longer
	 * valid. Clear this array just to be sure we don't accidentally use the
	 * obsolete copies of the nodes later on.
	 */
	if (glob->share.shared_plans)
	{
		pfree(glob->share.shared_plans);
		glob->share.shared_plans = NULL;
	}

	/* subplan_sliceIds array needs to exist, even in non-dispatcher mode */
	root->glob->subplan_sliceIds = palloc(list_length(root->glob->subplans) * sizeof(int));
	for(int i = 0; i < list_length(root->glob->subplans); i++)
		root->glob->subplan_sliceIds[i] = -1;

	/*
	 * In non-dispatcher mode, there are no motions and no dispatching.
	 * In utility mode, we still need a slice, at least some of the
	 * EXPLAIN code expects it.
	 */
	if (Gp_role == GP_ROLE_UTILITY)
	{
		PlanSlice *dummySlice;

		dummySlice = palloc0(sizeof(PlanSlice));
		dummySlice->sliceIndex = 0;
		dummySlice->parentIndex = 0;
		dummySlice->gangType = GANGTYPE_UNALLOCATED;
		dummySlice->numsegments = 0;
		dummySlice->segindex = -1;

		root->glob->numSlices = 1;
		root->glob->slices = dummySlice;

		return;
	}
	if (Gp_role == GP_ROLE_EXECUTE)
		return;
	Assert(Gp_role == GP_ROLE_DISPATCH);

	planner_init_plan_tree_base(&cxt.base, root);
	cxt.seen_subplans = NULL;

	Assert(root->glob->slices == NULL);

	top_slice->sliceIndex = 0;
	top_slice->parentIndex = -1;
	cxt.slices = list_make1(top_slice);

	cxt.currentSliceIndex = 0;

	/*
	 * Walk through the main plan tree, and recursively all SubPlans.
	 * Create a new PlanSlice, and assign a slice ID at every Motion
	 * and Init Plan. Also makes note of all the Subplans that are
	 * reachable from the top plan, so that unused SubPlans can be
	 * eliminated later.
	 */
	(void) build_slice_table_walker((Node *) top_plan, &cxt);

	/*
	 * Remove unused subplans, and Init Plans.
	 *
	 */
	remove_unused_initplans(top_plan, root);

	/*
	 * Remove unused subplans.
	 *
	 * Executor initializes state for subplans even they are unused.
	 * When the generated subplan is not used and has motion inside,
	 * causing motionID not being assigned, which will break sanity
	 * check when executor tries to initialize subplan state.
	 */
	int			plan_id = 1;
	foreach(lc, root->glob->subplans)
	{
		if (!bms_is_member(plan_id, cxt.seen_subplans))
		{
			Plan	   *dummy_plan;

			/*
			 * This subplan is unused. Replace it in the global list of
			 * subplans with a dummy. (We can't just remove it from the global
			 * list, because that would screw up the plan_id numbering of the
			 * subplans).
			 */
			pfree(lfirst(lc));
			dummy_plan = (Plan *) make_result(NIL,
											  (Node *) list_make1(makeBoolConst(false, false)),
											  NULL);
			lfirst(lc) = dummy_plan;
		}
		plan_id++;
	}

	/*
	 * If none of the slices require dispatching, we can run everything in one slice.
	 */
	all_root_slices = true;
	foreach (lc, cxt.slices)
	{
		PlanSlice *slice = (PlanSlice *) lfirst(lc);

		if (slice->gangType != GANGTYPE_UNALLOCATED)
		{
			all_root_slices = false;
			break;
		}
	}
	if (all_root_slices)
	{
		for(int i = 0; i < list_length(root->glob->subplans); i++)
			root->glob->subplan_sliceIds[i] = -1;
		cxt.slices = list_truncate(cxt.slices, 1);
	}

	/*
	 * Turn the list of PlanSlices into an array, for dispatching.
	 */
	root->glob->numSlices = list_length(cxt.slices);
	root->glob->slices = palloc(root->glob->numSlices * sizeof(PlanSlice));
	sliceIndex = 0;
	foreach (lc, cxt.slices)
	{
		PlanSlice *src = (PlanSlice *) lfirst(lc);

		memcpy(&root->glob->slices[sliceIndex], src, sizeof(PlanSlice));
		sliceIndex++;
	}
	list_free_deep(cxt.slices);

	/*
	 * Discard subtrees of Query node that aren't needed for execution. Note
	 * the targetlist (query->targetList) is used in execution of prepared
	 * statements, so we leave that alone.
	 */
	query->jointree = NULL;
	query->groupClause = NIL;
	query->havingQual = NULL;
	query->distinctClause = NIL;
	query->sortClause = NIL;
	query->limitCount = NULL;
	query->limitOffset = NULL;
	query->setOperations = NULL;
}

static bool
build_slice_table_walker(Node *node, build_slice_table_context *context)
{
	PlannerInfo *root = (PlannerInfo *) context->base.node;

	if (node == NULL)
		return false;

	if (IsA(node, SubPlan))
	{
		SubPlan	   *spexpr = (SubPlan *) node;
		int			plan_id = spexpr->plan_id;

		/*
		 * Recurse into each subplan only once, even if there are multiple
		 * SubPlans referring to it.
		 */
		if (!bms_is_member(plan_id, context->seen_subplans))
		{
			bool		result;
			int			save_currentSliceIndex;

			context->seen_subplans = bms_add_member(context->seen_subplans, plan_id);

			save_currentSliceIndex = context->currentSliceIndex;
			if (spexpr->is_initplan)
			{
				PlanSlice *initTopSlice;

				initTopSlice = palloc0(sizeof(PlanSlice));
				initTopSlice->gangType = GANGTYPE_UNALLOCATED;
				initTopSlice->numsegments = 0;
				initTopSlice->segindex = -1;
				initTopSlice->parentIndex = -1;
				initTopSlice->sliceIndex = list_length(context->slices);
				context->slices = lappend(context->slices, initTopSlice);

				context->currentSliceIndex = initTopSlice->sliceIndex;
			}
			root->glob->subplan_sliceIds[plan_id - 1] = context->currentSliceIndex;

			result = plan_tree_walker(node,
									  build_slice_table_walker,
									  context,
									  true);

			context->currentSliceIndex = save_currentSliceIndex;

			return result;
		}
	}

	if (IsA(node, Motion))
	{
		Motion	   *motion = (Motion *) node;
		PlanSlice  *sendSlice;
		int			save_currentSliceIndex;
		bool		result;

		Assert(motion->motionID == 0);

		/* set up a new slice */
		sendSlice = motion->senderSliceInfo;
		Assert(sendSlice->sliceIndex == -1);
		sendSlice->sliceIndex = list_length(context->slices);
		sendSlice->parentIndex = context->currentSliceIndex;
		motion->motionID = sendSlice->sliceIndex;
		motion->senderSliceInfo = NULL;
		context->slices = lappend(context->slices, sendSlice);

		save_currentSliceIndex = context->currentSliceIndex;
		context->currentSliceIndex = sendSlice->sliceIndex;

		if (sendSlice->directDispatch.isDirectDispatch &&
			sendSlice->directDispatch.contentIds == NIL)
		{
			/*
			 * Direct dispatch, but we've already determined that there will
			 * be no match on any segment. Doesn't matter which segmnent produces
			 * the non-result.
			 *
			 * XXX: Would've been better to replace this with a dummy Result
			 * node or something in the planner.
			 */
			sendSlice->directDispatch.contentIds = list_make1_int(0);
		}

		result = plan_tree_walker((Node *) motion,
								  build_slice_table_walker,
								  context,
								  false);

		context->currentSliceIndex = save_currentSliceIndex;

		return result;
	}

	return plan_tree_walker(node,
							build_slice_table_walker,
							context,
							false);
}


/*
 * Construct a new Flow in the current memory context.
 */
Flow *
makeFlow(FlowType flotype, int numsegments)
{
	Flow	   *flow = makeNode(Flow);

	Assert(numsegments > 0);

	flow->flotype = flotype;
	flow->locustype = CdbLocusType_Null;
	flow->numsegments = numsegments;

	return flow;
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
		case T_IndexScan:
		case T_BitmapIndexScan:
		case T_BitmapHeapScan:
		case T_TidScan:
		case T_SubqueryScan:
		case T_FunctionScan:
		case T_ValuesScan:
		case T_Agg:
		case T_TupleSplit:
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

void
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
