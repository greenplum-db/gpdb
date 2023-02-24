/*-------------------------------------------------------------------------
 *
 * planpartition.c
 *	  Transforms to produce plans that achieve dynamic partition elimination.
 *
 * Portions Copyright (c) 2011-2013, EMC Corporation
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/plan/planpartition.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "optimizer/planmain.h"
#include "optimizer/planpartition.h"
#include "optimizer/paths.h"
#include "optimizer/pathnode.h"
#include "optimizer/restrictinfo.h"
#include "cdb/cdbpartition.h"
#include "cdb/cdbplan.h"
#include "nodes/makefuncs.h"
#include "utils/lsyscache.h"
#include "cdb/cdbvars.h"
#include "parser/parse_oper.h"

static Expr *FindEqKey(PlannerInfo *root, Bitmapset *inner_relids, DynamicScanInfo *dyninfo, int partKeyAttno, Oid partKeyOpclass);

static void add_restrictinfos(PlannerInfo *root, DynamicScanInfo *dsinfo, Bitmapset *childrelids);

static bool IsPartKeyVar(Expr *expr, int partVarno, int partKeyAttno);

static Path *create_partition_selector_path(PlannerInfo *root,
							   Path *subpath,
							   DynamicScanInfo *dsinfo,
							   List *partKeyExprs,
							   List *partKeyAttnos);


/*
 * Try to perform "partition selection" on a join.
 *
 * If you have a join between a partitioned table and something else:
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
 *      -> Result
 *         One-Time Filter: PartSelected
 *            -> Seq Scan partition1
 *      -> Result
 *         One-Time Filter: PartSelected
 *            -> Seq Scan partition2
 *      -> Result
 *         One-Time Filter: PartSelected
 *            -> Seq Scan partition3
 *   -> Hash
 *      -> PartitionSelector for partitioned_table
 *         Filter: othertable.id
 *         -> SeqScan othertable
 *            Filter: <other conditions>
 *
 * While we build the Hash table, the Partition Selector computes for
 * each row, which partitions in partitioned_table can contain
 * tuples matching the join condition. After we have built the hash
 * table, and start to evaluate the outer side of the join, we can
 * skip those partitions that we didn't see any matching inner tuples
 * for. The One-Time Filters act as "gates" that eliminate those
 * partitions altogether.
 *
 * If such a plan is possible, this function returns 'true', and modifies
 * the given JoinPath to implement it. It wraps the inner child path with
 * suitable PartitionSelectorPaths, and adds quals to the rels in the outer
 * side, which will become One-Time Filters when the Plan is created for
 * the outer side.
 *
 * If any partition selectors were created, returns 'true'.
 */
bool
inject_partition_selectors_for_join(PlannerInfo *root, JoinPath *join_path,
									List **partSelectors_p)
{
	ListCell   *lc;
	Path	   *outerpath = join_path->outerjoinpath;
	Path	   *innerpath = join_path->innerjoinpath;
	Bitmapset  *inner_relids = innerpath->parent->relids;
	bool		any_selectors_created = false;
	bool		good_type = false;

	if (Gp_role != GP_ROLE_DISPATCH || !root->config->gp_dynamic_partition_pruning)
		return false;

	/*
	 * For the Partition Selector in the inner side of the join to have any
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
		return false;

	/*
	 * Cannot do it, if there inner and outer sides are not in the same slice.
	 * (This is just a quick check to see if there is any hope. We will check
	 * individual relations in the loop below.)
	 */
	if (bms_is_empty(outerpath->sameslice_relids))
		return false;

	/*
	 * Loop through all dynamic scans in the plan. For each one, check if
	 * the inner side of this join can provide values to the scan on the
	 * outer side.
	 */
	foreach(lc, root->dynamicScans)
	{
		DynamicScanInfo *dyninfo = (DynamicScanInfo *) lfirst(lc);
		ListCell   *lpk;
		ListCell   *lpkopclass;
		List	   *partKeyAttnos = NIL;
		List	   *partKeyExprs = NIL;
		Bitmapset  *childrelids;

		/*
		 * Does the outer side contain this dynamic scan? And it must be in
		 * the same slice as the join!
		 */
		childrelids = bms_intersect(dyninfo->children,
									outerpath->sameslice_relids);
		if (bms_is_empty(childrelids))
			continue;

		/*
		 * Find equivalence conditions between the partitioning keys and
		 * the relations on the inner side of the join.
		 */
		forboth(lpk, dyninfo->partKeyAttnos, lpkopclass, dyninfo->partKeyOpclass)
		{
			int			partKeyAttno = lfirst_int(lpk);
			Oid			partKeyOpclass = lfirst_oid(lpkopclass);
			Expr	   *expr;

			expr = FindEqKey(root, inner_relids, dyninfo, partKeyAttno, partKeyOpclass);

			if (expr)
			{
				partKeyAttnos = lappend_int(partKeyAttnos, partKeyAttno);
				partKeyExprs = lappend(partKeyExprs, expr);
			}
		}

		if (partKeyExprs)
		{
			/*
			 * Found some partitioning keys that can be computed from the inner
			 * side.
			 *
			 * Build a partition selector and put it on the inner side.
			 *
			 * If the inner side contains a Material, then we put the
			 * partition selector under the Material node. The Material
			 * will shield us from rescanning, and ensures that the inner
			 * side is scanned fully, before the outer side.
			 */
			if (IsA(innerpath, MaterialPath))
			{
				MaterialPath *matsubpath = (MaterialPath *) innerpath;

				matsubpath->subpath =
					create_partition_selector_path(root,
												   matsubpath->subpath,
												   dyninfo,
												   partKeyExprs,
												   partKeyAttnos);

				/*
				 * The inner side, with the Partition Selector, must be fully
				 * executed before the outer side.
				 */
				matsubpath->cdb_strict = true;
			}
			else
			{
				innerpath = create_partition_selector_path(root,
														   innerpath,
														   dyninfo,
														   partKeyExprs,
														   partKeyAttnos);
			}

			/*
			 * Add RestrictInfos on the partition scans on the outer side.
			 *
			 * NB: This modifies the global RelOptInfo structures! Therefore,
			 * if you tried to create the plan from some other Path, where
			 * the partition selector cannot be used, you'll nevertheless get
			 * the PartSelectExpr gates in the partition scans. Because of this,
			 * this function must only be used just before turning a path into
			 * plan.
			 */
			if (!dyninfo->hasSelector)
			{
				dyninfo->hasSelector = true;

				add_restrictinfos(root, dyninfo, childrelids);
			}

			any_selectors_created = true;
		}
	}

	if (any_selectors_created)
	{
		join_path->innerjoinpath = innerpath;
		return true;
	}
	else
		return false;
}

/*
 * Create a PartitionSelectorPath, for the inner side of a join.
 */
static Path *
create_partition_selector_path(PlannerInfo *root,
							   Path *subpath,
							   DynamicScanInfo *dsinfo,
							   List *partKeyExprs,
							   List *partKeyAttnos)
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
	pathnode->dsinfo = dsinfo;
	pathnode->partKeyExprs = partKeyExprs;
	pathnode->partKeyAttnos = partKeyAttnos;

	return (Path *) pathnode;
}

static Expr *
FindEqKey(PlannerInfo *root, Bitmapset *inner_relids,
		  DynamicScanInfo *dyninfo, int partKeyAttno, Oid partKeyOpclass)
{
	ListCell   *lem;
	ListCell   *lem2;
	ListCell   *lec;
	Oid			partkey_opfamily;

	partkey_opfamily = get_opclass_family(partKeyOpclass);
	Assert(OidIsValid(partkey_opfamily));

	/*
	 * NOTE: The equivalence class logic ensures that we don't get fooled
	 * by outer join conditions. The sides of an outer join equality are not
	 * put in the same equivalence class.
	 */
	foreach(lec, root->eq_classes)
	{
		EquivalenceClass *cur_ec = (EquivalenceClass *) lfirst(lec);

		/*
		 * If the partition key's btree operator's family is not in the
		 * equivalence opclass opfamilies of the join equality operator,
		 * we shouldn't performing partition pruning.
		 */
		if (!list_member_oid(cur_ec->ec_opfamilies, partkey_opfamily))
			continue;

		/*
		 * If the EquivalenceClass contains a const, the partitions should be
		 * chosen by constraint exclusion.
		 *
		 * A single-member isn't useful either (the latter test covers the
		 * volatile case too)
		 */
		if (cur_ec->ec_has_const || list_length(cur_ec->ec_members) <= 1)
			continue;

		/*
		 * No point in searching if rel not mentioned in eclass.
		 */
		if (!bms_is_member(dyninfo->rtindex, cur_ec->ec_relids))
			continue;
		/* or if there is nothing for the inner rel */
		if (!bms_overlap(cur_ec->ec_relids, inner_relids))
			continue;

		/* Search for a member that matches the partitioning key */
		foreach(lem, cur_ec->ec_members)
		{
			EquivalenceMember *part_em = (EquivalenceMember *) lfirst(lem);

			if (IsPartKeyVar(part_em->em_expr, dyninfo->rtindex, partKeyAttno))
			{
				/*
				 * Found partitioning key! Can we find something useful on the
				 * inner side in the same equivalence class?
				 */
				foreach (lem2, cur_ec->ec_members)
				{
					EquivalenceMember *inner_em = (EquivalenceMember *) lfirst(lem2);

					if (!bms_is_subset(inner_em->em_relids, inner_relids))
						continue; /* not computable on the inner side */
					/*
					 * The condition will be duplicated as the partition
					 * selection key in the PartitionSelector node. It is
					 * not OK to duplicate the expression, if it contains
					 * SubPlans, because the code that adds motion nodes to a
					 * subplan gets confused if there are multiple SubPlans
					 * referring the same subplan ID. It would probably
					 * perform badly too, since subplans are typically quite
					 * expensive.
					 */
					if (contain_subplans((Node *) inner_em->em_expr))
						continue;
					/*
					 * This can be computed from the inner side.
					 *
					 * Build a partition selector and put it on the inner side.
					 * Add RestrictInfos on the partition scans.
					 */
					return inner_em->em_expr;
				}
			}
		}
	}

	return NULL;
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
	ListCell   *lc_attno;
	ListCell   *lc_expr;
	List	   *partTabTargetlist;
	int			max_attr;
	int			attno;
	Expr	  **partKeyExprs;

	subplan = create_plan_recurse(root, best_path->subpath);

	max_attr = find_base_rel(root, best_path->dsinfo->rtindex)->max_attr;
	partKeyExprs = palloc0((max_attr + 1) * sizeof(Expr *));

	forboth(lc_attno, best_path->partKeyAttnos, lc_expr, best_path->partKeyExprs)
	{
		int			partKeyAttno = lfirst_int(lc_attno);
		Expr	   *partKeyExpr = (Expr *) lfirst(lc_expr);

		if (partKeyAttno > max_attr)
			elog(ERROR, "invalid partitioning key attribute number");

		partKeyExprs[partKeyAttno] = partKeyExpr;
	}

	partTabTargetlist = NIL;
	for (attno = 1; attno <= max_attr; attno++)
	{
		Expr	   *expr = partKeyExprs[attno];
		char		attname[20];

		if (!expr)
			expr = (Expr *) makeBoolConst(false, true);

		snprintf(attname, sizeof(attname), "partcol_%d", attno);

		partTabTargetlist = lappend(partTabTargetlist,
									makeTargetEntry(expr,
													attno,
													pstrdup(attname),
													false));
	}

	ps = makeNode(PartitionSelector);
	ps->plan.targetlist = subplan->targetlist;
	ps->plan.qual = NIL;
	ps->plan.lefttree = subplan;
	ps->plan.righttree = NULL;

	ps->plan.startup_cost = subplan->startup_cost;
	ps->plan.total_cost = subplan->total_cost;
	ps->plan.plan_rows = subplan->plan_rows;
	ps->plan.plan_width = subplan->plan_width;

	ps->relid = best_path->dsinfo->parentOid;
	ps->nLevels = 1;
	ps->scanId = best_path->dsinfo->dynamicScanId;
	ps->selectorId = -1;
	ps->partTabTargetlist = partTabTargetlist;
	ps->levelExpressions = NIL;
	ps->residualPredicate = NULL;
	ps->printablePredicate = (Node *) best_path->partKeyExprs;
	ps->staticSelection = false;
	ps->staticPartOids = NIL;
	ps->staticScanIds = NIL;

	ps->propagationExpression = (Node *)
		makeConst(INT4OID, -1, InvalidOid, 4, Int32GetDatum(ps->scanId), false, true);

	return (Plan *) ps;
}

/*
 * Add RestrictInfos representing PartSelectedExpr gating quals for
 * all the scans of each partition in a dynamic scan.
 */
static void
add_restrictinfos(PlannerInfo *root, DynamicScanInfo *dsinfo, Bitmapset *childrelids)
{
	ListCell   *lc;

	foreach(lc, root->append_rel_list)
	{
		AppendRelInfo *appinfo = (AppendRelInfo *) lfirst(lc);
		RelOptInfo *relinfo;
		RangeTblEntry *rte;
		RestrictInfo *rinfo;
		PartSelectedExpr *selectedExpr;
		Oid			childOid;

		if (!bms_is_member(appinfo->child_relid, childrelids))
			continue;

		relinfo = find_base_rel(root, appinfo->child_relid);

		rte = root->simple_rte_array[appinfo->child_relid];

		childOid = rte->relid;

		selectedExpr = makeNode(PartSelectedExpr);
		selectedExpr->dynamicScanId = dsinfo->dynamicScanId;
		selectedExpr->partOid = childOid;

		rinfo = make_restrictinfo((Expr *) selectedExpr,
								  true,
								  false,
								  true,
								  NULL,
								  NULL,
								  NULL);

		relinfo->baserestrictinfo = lappend(relinfo->baserestrictinfo, rinfo);

		root->hasPseudoConstantQuals = true;
	}
}

RestrictInfo *
make_mergeclause(Node *outer, Node *inner)
{
	OpExpr	   *opxpr;
	Expr	   *xpr;
	RestrictInfo *rinfo;

	opxpr = (OpExpr *) make_op(NULL, list_make1(makeString("=")),
							   outer,
							   inner, -1);
	opxpr->xpr.type = T_DistinctExpr;

	xpr = make_notclause((Expr *) opxpr);

	rinfo = make_restrictinfo(xpr, false, false, false, NULL, NULL, NULL);
	rinfo->mergeopfamilies = get_mergejoin_opfamilies(opxpr->opno);

	return rinfo;
}

/*
 * Does the given expression correspond to a var on partitioned relation.
 * This function ignores relabeling wrappers
 */
static bool
IsPartKeyVar(Expr *expr, int partVarno, int partKeyAttno)
{
	while (IsA(expr, RelabelType))
		expr = ((RelabelType *) expr)->arg;

	if (IsA(expr, Var))
	{
		Var *var = (Var *) expr;

		if (var->varno == partVarno && var->varattno == partKeyAttno)
			return true;
	}
	return false;
}
