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

static Expr *FindEqKey(PlannerInfo *root, Bitmapset *inner_relids, DynamicScanInfo *dyninfo, int partKeyAttno);

static PartitionSelector *create_partition_selector(PlannerInfo *root, DynamicScanInfo *dsinfo, Plan *subplan, List *partTabTargetlist, Expr *printablePredicate);

static void add_restrictinfos(PlannerInfo *root, DynamicScanInfo *dsinfo, Bitmapset *childrelids);

static bool IsPartKeyVar(Expr *expr, int partVarno, int partKeyAttno);

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
 *
 * The caller should have already built the Plan for the inner side,
 * but *not* for the outer side. This function adds quals to rels in
 * the outer side, they will become One-Time Filters when the Plan
 * is created for the inner side. For the inner side, this function
 * wraps the outer Plan with Partition Selectors.
 *
 * If any partition selectors were created, returns 'true'.
 */
bool
inject_partition_selectors_for_join(PlannerInfo *root, JoinPath *join_path,
									Plan **inner_plan_p)
{
	ListCell   *lc;
	Path	   *outerpath = join_path->outerjoinpath;
	Path	   *innerpath = join_path->innerjoinpath;
	Bitmapset  *inner_relids = innerpath->parent->relids;
	bool		any_selectors_created = false;
	bool		good_type;
	Plan	   *inner_parent;
	Plan	   *inner_child;

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
		inner_parent = NULL;
		inner_child = *inner_plan_p;
	}
	/*
	 * If we're doing a merge join, and will sort the inner side, then the sort
	 * will process all the inner rows before scanning the outer side. The Sort
	 * node will later be placed on top of the PartitionSelector we create here.
	 */
	else if (join_path->path.pathtype == T_MergeJoin && ((MergePath *) join_path)->innersortkeys)
	{
		good_type = true;
		inner_parent = NULL;
		inner_child = *inner_plan_p;
	}
	/*
	 * For a nested loop or a merge join, if there happens to be a Material
	 * or Sort node on the inner side, then we can place the Partition Selector
	 * below it. Even though a nested loop join will rescan the inner side,
	 * the Material or Sort will be executed all in one go.
	 */
	else if ((join_path->path.pathtype == T_NestLoop && !path_contains_inner_index(innerpath)) ||
			 join_path->path.pathtype == T_MergeJoin)
	{
		if (IsA(*inner_plan_p, Material) || IsA(*inner_plan_p, Sort))
		{
			good_type = true;
			inner_parent = *inner_plan_p;
			inner_child = inner_parent->lefttree;
		}
		else
			good_type = false;
	}
	else
		good_type = false;

	if (!good_type)
		return false;

	/*
	 * Cannot do it, if there inner and outer sides are not in the same slice.
	 */
	if (bms_is_empty(outerpath->sameslice_relids))
		return false;

	/*
	 * NOTE: The equivalence class logic ensures that we don't get fooled
	 * by outer join conditions. The sides of an outer join equality are not
	 * put in the same equivalence class.
	 */
	foreach(lc, root->dynamicScans)
	{
		DynamicScanInfo *dyninfo = (DynamicScanInfo *) lfirst(lc);
		ListCell   *lpk;
		Expr	  **partKeyExprs = NULL;
		int			max_attr = -1;
		List	   *printablePredicate = NIL;
		Bitmapset  *childrelids;

		/*
		 * Does the outer side contain this dynamic scan? And it must be in
		 * the same slice as the join!
		 */
		childrelids = bms_intersect(dyninfo->children,
									outerpath->sameslice_relids);
		if (bms_is_empty(childrelids))
			continue;

		foreach(lpk, dyninfo->partKeyAttnos)
		{
			int			partKeyAttno = lfirst_int(lpk);
			Expr	   *expr;

			expr = FindEqKey(root, inner_relids, dyninfo, partKeyAttno);

			if (!expr)
				continue;

			if (!partKeyExprs)
			{
				max_attr = find_base_rel(root, dyninfo->rtindex)->max_attr;
				partKeyExprs = palloc0((max_attr + 1) * sizeof(Expr *));
			}
			if (partKeyAttno > max_attr)
				elog(ERROR, "invalid partitioning key attribute number");

			partKeyExprs[partKeyAttno] = expr;

			printablePredicate = lappend(printablePredicate, expr);
		}

		if (partKeyExprs)
		{
			/*
			 * This can be computed from the inner side.
			 *
			 * Build a partition selector and put it on the inner side.
			 * Add RestrictInfos on the partition scans.
			 */
			List	   *partTabTargetlist = NIL;
			int			attno;
			PartitionSelector *partSelector;

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

			partSelector = create_partition_selector(root,
													 dyninfo,
													 inner_child,
													 partTabTargetlist,
													 (Expr *) printablePredicate);
			inner_child = (Plan *) partSelector;
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
		if (inner_parent)
		{
			inner_parent->lefttree = inner_child;
			if (IsA(inner_parent, Material))
				((Material *)inner_parent)->cdb_strict = true;
		}
		else
			*inner_plan_p = inner_child;
		return true;
	}
	else
		return false;
}

static Expr *
FindEqKey(PlannerInfo *root, Bitmapset *inner_relids,
		  DynamicScanInfo *dyninfo, int partKeyAttno)
{
	ListCell   *lem;
	ListCell   *lem2;
	ListCell   *lec;

	foreach(lec, root->eq_classes)
	{
		EquivalenceClass *cur_ec = (EquivalenceClass *) lfirst(lec);

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

static PartitionSelector *
create_partition_selector(PlannerInfo *root, DynamicScanInfo *dsinfo,
						  Plan *subplan, List *partTabTargetlist,
						  Expr *printablePredicate)
{
	PartitionSelector *ps;

	ps = makeNode(PartitionSelector);
	ps->plan.targetlist = subplan->targetlist;
	ps->plan.qual = NIL;
	ps->plan.lefttree = subplan;
	ps->plan.righttree = NULL;

	ps->plan.startup_cost = subplan->startup_cost;
	ps->plan.total_cost = subplan->total_cost;
	ps->plan.plan_rows = subplan->plan_rows;
	ps->plan.plan_width = subplan->plan_width;

	ps->relid = dsinfo->parentOid;
	ps->nLevels = 1;
	ps->scanId = dsinfo->dynamicScanId;
	ps->selectorId = -1;
	ps->partTabTargetlist = partTabTargetlist;
	ps->levelExpressions = NIL;
	ps->residualPredicate = NULL;
	ps->printablePredicate = (Node *) printablePredicate;
	ps->staticSelection = false;
	ps->staticPartOids = NIL;
	ps->staticScanIds = NIL;

	ps->propagationExpression = (Node *)
		makeConst(INT4OID, -1, 4, Int32GetDatum(ps->scanId), false, true);

	return ps;
}

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
