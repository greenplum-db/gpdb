/*-------------------------------------------------------------------------
 *
 * joinpartprune.c
 *	  Transforms to produce plans that achieve dynamic partition elimination.
 *
 * This file contains GPDB specific code, to perform partition pruning
 * based on tuples seen on the "other" side of a join. Join pruning is
 * performed in co-operation between an Append node and a PartitionSelector
 * node. If you have a join between a partitioned table and something else:
 *
 * Hash Join
 *  Hash Join Cond: partitioned_table.id = othertable.id
 *   -> Append
 *      -> Seq Scan partition1
 *      -> Seq Scan partition2
 *      -> Seq Scan partition3
 *   -> Hash
 *      -> SeqScan othertable
 *         Filter: <other conditions>
 *
 * it would be good to skip those partitions that cannot contain any
 * tuples coming from the other side of the join. To do that, we
 * transform the plan into this:
 *
 * Join
 *  Join Cond: partitioned_table.id = othertable.id
 *   -> Append
 *       Partition Selectors: $0
 *            -> Seq Scan partition1
 *            -> Seq Scan partition2
 *            -> Seq Scan partition3
 *   -> Hash
 *      -> PartitionSelector (selector id: $0)
 *         Filter: othertable.id
 *         -> SeqScan othertable
 *            Filter: <other conditions>
 *
 * While the executor builds the Hash table, the Partition Selector
 * computes for each row which partitions in partitioned_table can contain
 * tuples matching the join condition. After we have built the hash
 * table, and start to evaluate the outer side of the join, we can
 * skip those partitions that we didn't see any matching inner tuples
 * for. The Partition Selector constructs a list of surviving partitions,
 * and the Append node skips ones that didn't survive.
 *
 * Planning these join pruning plans happens in the create_plan() stage:
 *
 * 1. Whenever a join is being processed, create_*join_plan() calls
 *    the push_partition_selector_candidate_for_join() function.
 *    push_partition_selector_candidate_for_join() decides if it's possible
 *    to put a partition selector to the inner side of the join, and if so,
 *    it records information about the join in a stack, in
 *    root->partition_selector_candidates. (In this phase, we don't yet
 *    determine if a Partition Selector would actually be useful, only
 *    if it would be valid to place one at this join.)
 *
 * 2. When create_append_plan() is called to create an Append plan for
 *    a partitioned table on the outer side of the join, create_append_plan()
 *    calls make_partition_join_pruneinfos() to check if any of the partitions
 *    could be pruned based on any of the joins recorded in the
 *    root->partition_selector_candidates stack. If so, it builds the pruning
 *    steps into a PartitionPruneInfo struct, and adds it to the stack entry.
 *    At this point, we have decided that a Partition Selector will be used at
 *    the join.
 *
 * 3. After finishing the outer side of the join, the create_*join_plan()
 *    calls pop_and_inject_partition_selectors(). If we decided to use
 *    any Partition Selectors at this join, the Partition Selectors are added
 *    to the inner side of the join.
 *
 *
 * Note: All this happens in the create_plan() stage,  after we have already
 * constructed the paths and decided the plan shape, so the possibility of
 * pruning doesn't affect cost estimates or the chosen plan shape. That's not
 * great, but will do for now...
 *
 * Portions Copyright (c) 2011-2013, EMC Corporation
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/plan/joinpartprune.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "optimizer/clauses.h"
#include "optimizer/joinpartprune.h"
#include "optimizer/paramassign.h"
#include "optimizer/planmain.h"
#include "optimizer/pathnode.h"
#include "partitioning/partprune.h"
#include "cdb/cdbvars.h"

static Path *create_partition_selector_path(PlannerInfo *root,
											Path *subpath,
											int paramid,
											PartitionPruneInfo *pruneinfo);

/*
 * Check if a Partition Selector could be placed at this join.
 *
 * This doesn't check if it would actually be useful here; that decision
 * is made in create_append_plan(). This merely checks if this is the
 * right kind of join.
 */
void
push_partition_selector_candidate_for_join(PlannerInfo *root, JoinPath *join_path)
{
	Path	   *innerpath = join_path->innerjoinpath;
	Relids		inner_relids = innerpath->parent->relids;
	bool		good_type = false;
	PartitionSelectorCandidateInfo *candidate;

	if (Gp_role != GP_ROLE_DISPATCH || !gp_dynamic_partition_pruning)
	{
		root->partition_selector_candidates = lcons(NULL, root->partition_selector_candidates);
		return;
	}

	if (!(join_path->jointype == JOIN_INNER ||
		  join_path->jointype == JOIN_RIGHT ||
		  join_path->jointype == JOIN_SEMI))
	{
		root->partition_selector_candidates = lcons(NULL, root->partition_selector_candidates);
		return;
	}

	/*
	 * For the partition selector in the inner side of the join to have any
	 * effect on the scans in the outer side, the inner side must be evaluated
	 * first. For a hash join, that is always true.
	 */
	if (join_path->path.pathtype == T_HashJoin)
	{
		good_type = true;
	}
	/*
	 * If we're doing a merge join, and will sort the inner side, then the sort
	 * will process all the inner rows before scanning the outer side. The Sort
	 * node will later be placed on top of the PartitionSelector we create here.
	 */
	else if (join_path->path.pathtype == T_MergeJoin && ((MergePath *) join_path)->innersortkeys)
	{
		good_type = true;
	}
	/*
	 * For a nested loop join, if there happens to be a Material node on the inner
	 * side, then we can place the Partition Selector below it. Even though a nested
	 * loop join will rescan the inner side, the Material or Sort will be executed
	 * all in one go.
	 *
	 * We could add a Material node, if there isn't one already, but a Material
	 * node is expensive. It would almost certainly make this plan worse than
	 * something else that the planner considered and rejected.
	 */
	else if (join_path->path.pathtype == T_NestLoop && !path_contains_inner_index(innerpath))
	{
		if (IsA(innerpath, MaterialPath))
			good_type = true;
		else
			good_type = false;
	}
	else if (join_path->path.pathtype == T_MergeJoin)
	{
		if (IsA(innerpath, MaterialPath))
			good_type = true;
		else if (((MergePath *) join_path)->innersortkeys)
			good_type = true;
		else
			good_type = false;
	}
	else
		good_type = false;

	if (!good_type)
	{
		root->partition_selector_candidates = lcons(NULL, root->partition_selector_candidates);
		return;
	}

	/* OK! */
	candidate = palloc(sizeof(PartitionSelectorCandidateInfo));

	candidate->joinrestrictinfo = join_path->joinrestrictinfo;
	candidate->inner_relids = inner_relids;
	candidate->slice = root->curSlice;
	candidate->selectors = NIL;

	root->partition_selector_candidates = lcons(candidate, root->partition_selector_candidates);
	return;
}

/*
 * Called after processing the outer side of a join. If the outer side
 * contained any Append nodes that want to do join pruning at this join,
 * adds Partition Selector nodes to the inner path, and returns 'true'.
 *
 * NB: This modifies the original Path. That's ugly, but this is only
 * used in the create_plan() stage where we've already decided the plan.
 * If we wanted to do this earlier while we could still affect the
 * plan shape, we shouldn't do that.
 */
bool
pop_and_inject_partition_selectors(PlannerInfo *root, JoinPath *jpath)
{
	PartitionSelectorCandidateInfo *candidate;
	MaterialPath *origmat = NULL;
	Path	   *subpath;
	ListCell   *lc;

	candidate = linitial(root->partition_selector_candidates);

	root->partition_selector_candidates =
		list_delete_first(root->partition_selector_candidates);

	if (!candidate || !candidate->selectors)
		return false;

	subpath = jpath->innerjoinpath;

	/*
	 * If the inner side contains a Material, then we put the Partition
	 * Selector under the Material node. The Material shields us from
	 * rescanning, and ensures that the inner side is scanned fully before
	 * the outer side.
	 */
	if (IsA(subpath, MaterialPath))
	{
		origmat = (MaterialPath *) subpath;
		subpath = origmat->subpath;
	}

	/* inject paths for the Partition Selectors to inner path */
	foreach (lc, candidate->selectors)
	{
		PartitionSelectorInfo *psinfo = (PartitionSelectorInfo *) lfirst(lc);

		subpath = create_partition_selector_path(root,
												 subpath,
												 psinfo->paramid,
												 psinfo->part_prune_info);
	}

	if (origmat)
	{
		origmat->subpath = subpath;
		subpath = &origmat->path;

		/*
		 * The inner side, with the Partition Selector, must be fully executed
		 * before the outer side.
		 */
		origmat->cdb_strict = true;
	}

	jpath->innerjoinpath = subpath;

	return true;
}

/* create a PartitionSelectorPath, for the inner side of a join. */
static Path *
create_partition_selector_path(PlannerInfo *root,
							   Path *subpath,
							   int paramid,
							   PartitionPruneInfo *pruneinfo)
{
	PartitionSelectorPath *pathnode = makeNode(PartitionSelectorPath);

	pathnode->path.pathtype = T_PartitionSelector;
	pathnode->path.parent = subpath->parent;

	pathnode->path.startup_cost = subpath->startup_cost;
	pathnode->path.total_cost = subpath->total_cost;
	pathnode->path.pathkeys = subpath->pathkeys;

	pathnode->path.locus = subpath->locus;
	pathnode->path.motionHazard = subpath->motionHazard;
	pathnode->path.rescannable = subpath->rescannable;
	pathnode->path.sameslice_relids = subpath->sameslice_relids;

	pathnode->subpath = subpath;
	pathnode->paramid = paramid;
	pathnode->part_prune_info = pruneinfo;

	return (Path *) pathnode;
}

/*
 * Create a PartitionSelector plan from a Path.
 *
 * This would logically belong in createplan.c, but let's keep everything
 * related to Partition Selectors in this file.
 */
Plan *
create_partition_selector_plan(PlannerInfo *root, PartitionSelectorPath *best_path)
{
	PartitionSelector *ps;
	Plan	   *subplan;
	PartitionPruneInfo *part_prune_info = best_path->part_prune_info;

	Assert(part_prune_info);

	subplan = create_plan_recurse(root, best_path->subpath, 0);

	ps = makeNode(PartitionSelector);
	ps->plan.targetlist = subplan->targetlist;
	ps->plan.qual = NIL;
	ps->plan.lefttree = subplan;
	ps->plan.righttree = NULL;

	ps->plan.startup_cost = subplan->startup_cost;
	ps->plan.total_cost = subplan->total_cost;
	ps->plan.plan_rows = subplan->plan_rows;
	ps->plan.plan_width = subplan->plan_width;

	ps->part_prune_info = part_prune_info;
	ps->paramid = best_path->paramid;

	return (Plan *) ps;
}

/*
 * Gather information needed to do join pruning.
 *
 * This is called from create_append_plan(), but the pruning steps are
 * actually put into the Partition Selector node on the other side of
 * the join.
 *
 * Returns list of special executor Param IDs that will contain
 * the results of the Partition Selectors at runtime.
 */
List *
make_partition_join_pruneinfos(PlannerInfo *root, RelOptInfo *parentrel,
							   List *subpaths, List *partitioned_rels)
{
	PartitionPruneInfo *part_prune_info;
	List	   *result = NIL;
	ListCell   *lc;

	foreach (lc, root->partition_selector_candidates)
	{
		PartitionSelectorCandidateInfo *candidate =
			(PartitionSelectorCandidateInfo *) lfirst(lc);

		if (!candidate)
			continue;

		/*
		 * Does the outer side contain this dynamic scan? And it must be in
		 * the same slice as the join! XXX
		 */
		if (candidate->slice != root->curSlice)
		{
			/*
			 * There's a Motion between this Partition Selector candidate and
			 * this Append. That won't work, the parameter containing the
			 * partition pruning results cannot be passed through a Motion.
			 * No need to look further, none of the nodes above this candidate
			 * can be in the same slice either.
			 */
			break;
		}

		part_prune_info =
			make_partition_pruneinfo_ext(root, parentrel,
										 subpaths, partitioned_rels,
										 candidate->joinrestrictinfo,
										 candidate->inner_relids);

		if (part_prune_info)
		{
			/*
			 * Stash the prune steps for the Partition Selector that
			 * will be put on the other side of the join.
			 */
			PartitionSelectorInfo *psinfo;

			psinfo = palloc(sizeof(PartitionSelectorInfo));
			psinfo->part_prune_info = part_prune_info;
			psinfo->paramid = assign_special_exec_param(root);

			candidate->selectors = lappend(candidate->selectors, psinfo);

			result = lappend_int(result, psinfo->paramid);
		}
	}

	return result;
}
