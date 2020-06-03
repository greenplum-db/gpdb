/*-------------------------------------------------------------------------
 *
 * cdbmutate.c
 *	  Parallelize a PostgreSQL sequential plan tree.
 *
 * Portions Copyright (c) 2004-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbmutate.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/xact.h"
#include "parser/parsetree.h"	/* for rt_fetch() */
#include "utils/relcache.h"		/* RelationGetPartitioningKey() */
#include "optimizer/planmain.h"
#include "optimizer/var.h"
#include "parser/parse_relation.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/datum.h"
#include "utils/syscache.h"
#include "optimizer/clauses.h"
#include "optimizer/pathnode.h"
#include "nodes/makefuncs.h"

#include "commands/tablecmds.h"
#include "commands/tablespace.h"
#include "commands/trigger.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"

#include "cdb/cdbhash.h"
#include "cdb/cdbllize.h"
#include "cdb/cdbmutate.h"
#include "cdb/cdbpartition.h"
#include "cdb/cdbplan.h"
#include "cdb/cdbpullup.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbtargeteddispatch.h"

#include "executor/executor.h"

typedef struct
{
	plan_tree_base_prefix base; /* Required prefix for
								 * plan_tree_walker/mutator */
	EState	   *estate;
	bool		single_row_insert;
	List	   *cursorPositions;
} pre_dispatch_function_evaluation_context;

/*
 * Forward Declarations
 */
static Node *pre_dispatch_function_evaluation_mutator(Node *node,
										 pre_dispatch_function_evaluation_context *context);
static bool replace_shareinput_targetlists_walker(Node *node, PlannerInfo *root, bool fPop);


Motion *
make_union_motion(Plan *lefttree)
{
	Motion	   *motion;

	motion = make_motion(NULL, lefttree,
						 0, NULL, NULL, NULL, NULL /* no ordering */);

	motion->motionType = MOTIONTYPE_GATHER;
	motion->hashExprs = NIL;
	motion->hashFuncs = NULL;

	return motion;
}

Motion *
make_sorted_union_motion(PlannerInfo *root, Plan *lefttree, int numSortCols,
						 AttrNumber *sortColIdx, Oid *sortOperators,
						 Oid *collations, bool *nullsFirst)
{
	Motion	   *motion;

	motion = make_motion(root, lefttree,
						 numSortCols, sortColIdx, sortOperators, collations, nullsFirst);
	motion->motionType = MOTIONTYPE_GATHER;
	motion->hashExprs = NIL;
	motion->hashFuncs = NULL;

	return motion;
}

Motion *
make_hashed_motion(Plan *lefttree,
				   List *hashExprs,
				   List *hashOpfamilies,
				   int numHashSegments)
{
	Motion	   *motion;
	Oid		   *hashFuncs;
	ListCell   *expr_cell;
	ListCell   *opf_cell;
	int			i;

	Assert(numHashSegments > 0);
	Assert(list_length(hashExprs) == list_length(hashOpfamilies));

	/* Look up the right hash functions for the hash expressions */
	hashFuncs = palloc(list_length(hashExprs) * sizeof(Oid));
	i = 0;
	forboth(expr_cell, hashExprs, opf_cell, hashOpfamilies)
	{
		Node	   *expr = lfirst(expr_cell);
		Oid			opfamily = lfirst_oid(opf_cell);
		Oid			typeoid = exprType(expr);

		hashFuncs[i++] = cdb_hashproc_in_opfamily(opfamily, typeoid);
	}

	motion = make_motion(NULL, lefttree,
						 0, NULL, NULL, NULL, NULL /* no ordering */);
	motion->motionType = MOTIONTYPE_HASH;
	motion->hashExprs = hashExprs;
	motion->hashFuncs = hashFuncs;
	motion->numHashSegments = numHashSegments;

	return motion;
}

Motion *
make_broadcast_motion(Plan *lefttree)
{
	Motion	   *motion;

	motion = make_motion(NULL, lefttree,
						 0, NULL, NULL, NULL, NULL /* no ordering */);
	motion->motionType = MOTIONTYPE_BROADCAST;
	motion->hashExprs = NIL;
	motion->hashFuncs = NULL;

	return motion;
}

Plan *
make_explicit_motion(PlannerInfo *root, Plan *lefttree, AttrNumber segidColIdx)
{
	Motion	   *motion;
	plan_tree_base_prefix base;

	base.node = (Node *) root;

	Assert(segidColIdx > 0 && segidColIdx <= list_length(lefttree->targetlist));

	motion = make_motion(NULL, lefttree,
						 0, NULL, NULL, NULL, NULL /* no ordering */);
	motion->motionType = MOTIONTYPE_EXPLICIT;
	motion->hashExprs = NIL;
	motion->hashFuncs = NULL;
	motion->segidColIdx = segidColIdx;

	return (Plan *) motion;
}

/* --------------------------------------------------------------------
 *
 *	Static Helper routines
 * --------------------------------------------------------------------
 */

/* ----------------------------------------------------------------
 * getExprListFromTargetList
 *
 * Creates a VAR that references the indicated entries from the target list,
 * sets the restype and restypmod fields from the target list info,
 * and puts them into a list.
 *
 * The AttrNumber indexes actually refer to the 1 based index into the
 * target list.
 *
 * The entries have the varno field replaced by references in OUTER_VAR.
 * ----------------------------------------------------------------
 */
List *
getExprListFromTargetList(List *tlist,
						  int numCols,
						  AttrNumber *colIdx)
{
	int			i;
	List	   *elist = NIL;

	for (i = 0; i < numCols; i++)
	{
		/* Find expr in TargetList */
		AttrNumber	n = colIdx[i];

		TargetEntry *target = get_tle_by_resno(tlist, n);

		if (target == NULL)
			elog(ERROR, "no tlist entry for key %d", n);

		elist = lappend(elist, copyObject(target->expr));
	}

	return elist;
}

/* ----------------------------------------------------------------------- *
 * cdbmutate_warn_ctid_without_segid() warns the user if the plan refers to a
 * partitioned table's ctid column without also referencing its
 * gp_segment_id column.
 * ----------------------------------------------------------------------- *
 */
typedef struct ctid_inventory_context
{
	plan_tree_base_prefix base; /* Required prefix for
								 * plan_tree_walker/mutator */
	bool		uses_ctid;
	bool		uses_segid;
	Index		relid;
} ctid_inventory_context;

static bool
ctid_inventory_walker(Node *node, ctid_inventory_context *inv)
{
	if (node == NULL)
		return false;

	if (IsA(node, Var))
	{
		Var		   *var = (Var *) node;

		if (var->varattno < 0 &&
			var->varno == inv->relid &&
			var->varlevelsup == 0)
		{
			if (var->varattno == SelfItemPointerAttributeNumber)
				inv->uses_ctid = true;
			else if (var->varattno == GpSegmentIdAttributeNumber)
				inv->uses_segid = true;
		}
		return false;
	}
	return plan_tree_walker(node, ctid_inventory_walker, inv, true);
}

void
cdbmutate_warn_ctid_without_segid(struct PlannerInfo *root, struct RelOptInfo *rel)
{
	ctid_inventory_context inv;
	Relids		relids_to_ignore;
	ListCell   *cell;
	AttrNumber	attno;
	int			ndx;

	planner_init_plan_tree_base(&inv.base, root);
	inv.uses_ctid = false;
	inv.uses_segid = false;
	inv.relid = rel->relid;

	/* Rel not distributed?  Then segment id doesn't matter. */
	if (!rel->cdbpolicy ||
		rel->cdbpolicy->ptype == POLICYTYPE_ENTRY)
		return;

	/* Ignore references occurring in the Query's final output targetlist. */
	relids_to_ignore = bms_make_singleton(0);

	/* Is 'ctid' referenced in join quals? */
	attno = SelfItemPointerAttributeNumber;
	Assert(attno >= rel->min_attr && attno <= rel->max_attr);
	ndx = attno - rel->min_attr;
	if (bms_nonempty_difference(rel->attr_needed[ndx], relids_to_ignore))
		inv.uses_ctid = true;

	/* Is 'gp_segment_id' referenced in join quals? */
	attno = GpSegmentIdAttributeNumber;
	Assert(attno >= rel->min_attr && attno <= rel->max_attr);
	ndx = attno - rel->min_attr;
	if (bms_nonempty_difference(rel->attr_needed[ndx], relids_to_ignore))
		inv.uses_segid = true;

	/* Examine the single-table quals on this rel. */
	foreach(cell, rel->baserestrictinfo)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(cell);

		Assert(IsA(rinfo, RestrictInfo));

		ctid_inventory_walker((Node *) rinfo->clause, &inv);
	}

	/* Notify client if found a reference to ctid only, without gp_segment_id */
	if (inv.uses_ctid &&
		!inv.uses_segid)
	{
		RangeTblEntry *rte = rt_fetch(rel->relid, root->parse->rtable);
		const char *cmd;
		int			elevel;

		/* Reject if UPDATE or DELETE.  Otherwise just give info msg. */
		switch (root->parse->commandType)
		{
			case CMD_UPDATE:
				cmd = "UPDATE";
				elevel = ERROR;
				break;

			case CMD_DELETE:
				cmd = "DELETE";
				elevel = ERROR;
				break;

			default:
				cmd = "SELECT";
				elevel = NOTICE;
		}

		ereport(elevel,
				(errmsg("%s uses system-defined column \"%s.ctid\" without the necessary companion column \"%s.gp_segment_id\"",
						cmd, rte->eref->aliasname, rte->eref->aliasname),
				 errhint("To uniquely identify a row within a distributed table, use the \"gp_segment_id\" column together with the \"ctid\" column.")));
	}
	bms_free(relids_to_ignore);
}								/* cdbmutate_warn_ctid_without_segid */


/*
 * Code that mutate the tree for share input
 *
 * After the planner, the plan is really a DAG.  SISCs will have valid
 * pointer to the underlying share.  However, other code (setrefs etc)
 * depends on the fact that the plan is a tree.  We first mutate the DAG
 * to a tree.
 *
 * Next, we will need to decide if the share is cross slices.  If the share
 * is not cross slice, we do not need the syncrhonization, and it is possible to
 * keep the Material/Sort in memory to save a sort.
 *
 * It is essential that we walk the tree in the same order as the ExecProcNode start
 * execution, otherwise, deadlock may rise.
 */

/* Walk the tree for shareinput.
 * Shareinput fix shared_as_id and underlying_share_id of nodes in place.  We do not want to use
 * the ordinary tree walker as it is unnecessary to make copies etc.
 */
typedef bool (*SHAREINPUT_MUTATOR) (Node *node, PlannerInfo *root, bool fPop);
static void
shareinput_walker(SHAREINPUT_MUTATOR f, Node *node, PlannerInfo *root)
{
	Plan	   *plan = NULL;
	bool		recursive_down;

	if (node == NULL)
		return;

	if (IsA(node, List))
	{
		List	   *l = (List *) node;
		ListCell   *lc;

		foreach(lc, l)
		{
			Node	   *n = lfirst(lc);

			shareinput_walker(f, n, root);
		}
		return;
	}

	if (!is_plan_node(node))
		return;

	plan = (Plan *) node;
	recursive_down = (*f) (node, root, false);

	if (recursive_down)
	{
		if (IsA(node, Append))
		{
			ListCell   *cell;
			Append	   *app = (Append *) node;

			foreach(cell, app->appendplans)
				shareinput_walker(f, (Node *) lfirst(cell), root);
		}
		else if (IsA(node, ModifyTable))
		{
			ListCell   *cell;
			ModifyTable *mt = (ModifyTable *) node;

			foreach(cell, mt->plans)
				shareinput_walker(f, (Node *) lfirst(cell), root);
		}
		else if (IsA(node, SubqueryScan))
		{
			SubqueryScan  *subqscan = (SubqueryScan *) node;
			PlannerGlobal *glob = root->glob;
			PlannerInfo   *subroot;
			List	      *save_rtable;
			RelOptInfo    *rel;

			/*
			 * If glob->finalrtable is not NULL, rtables have been flatten,
			 * thus we should use glob->finalrtable instead.
			 */
			save_rtable = glob->share.curr_rtable;
			if (root->glob->finalrtable == NULL)
			{
				rel = find_base_rel(root, subqscan->scan.scanrelid);
				/*
				 * The Assert() on RelOptInfo's subplan being
				 * same as the subqueryscan's subplan, is valid
				 * in Upstream but for not for GPDB, since we
				 * create a new copy of the subplan if two
				 * SubPlans refer to the same initplan.
				 */
				subroot = rel->subroot;
				glob->share.curr_rtable = subroot->parse->rtable;
			}
			else
			{
				subroot = root;
				glob->share.curr_rtable = glob->finalrtable;
			}
			shareinput_walker(f, (Node *) subqscan->subplan, subroot);
			glob->share.curr_rtable = save_rtable;
		}
		else if (IsA(node, TableFunctionScan))
		{
			TableFunctionScan  *tfscan = (TableFunctionScan *) node;
			PlannerGlobal *glob = root->glob;
			PlannerInfo   *subroot;
			List	      *save_rtable;
			RelOptInfo    *rel;

			/*
			 * If glob->finalrtable is not NULL, rtables have been flatten,
			 * thus we should use glob->finalrtable instead.
			 */
			save_rtable = glob->share.curr_rtable;
			if (root->glob->finalrtable == NULL)
			{
				rel = find_base_rel(root, tfscan->scan.scanrelid);
				subroot = rel->subroot;
				glob->share.curr_rtable = subroot->parse->rtable;
			}
			else
			{
				subroot = root;
				glob->share.curr_rtable = glob->finalrtable;
			}
			shareinput_walker(f, (Node *)  tfscan->scan.plan.lefttree, subroot);
			glob->share.curr_rtable = save_rtable;
		}
		else if (IsA(node, BitmapAnd))
		{
			ListCell   *cell;
			BitmapAnd  *ba = (BitmapAnd *) node;

			foreach(cell, ba->bitmapplans)
				shareinput_walker(f, (Node *) lfirst(cell), root);
		}
		else if (IsA(node, BitmapOr))
		{
			ListCell   *cell;
			BitmapOr   *bo = (BitmapOr *) node;

			foreach(cell, bo->bitmapplans)
				shareinput_walker(f, (Node *) lfirst(cell), root);
		}
		else if (IsA(node, NestLoop))
		{
			/*
			 * Nest loop join is strange.  Exec order depends on
			 * prefetch_inner
			 */
			NestLoop   *nl = (NestLoop *) node;

			if (nl->join.prefetch_inner)
			{
				shareinput_walker(f, (Node *) plan->righttree, root);
				shareinput_walker(f, (Node *) plan->lefttree, root);
			}
			else
			{
				shareinput_walker(f, (Node *) plan->lefttree, root);
				shareinput_walker(f, (Node *) plan->righttree, root);
			}
		}
		else if (IsA(node, HashJoin))
		{
			/* Hash join the hash table is at inner */
			shareinput_walker(f, (Node *) plan->righttree, root);
			shareinput_walker(f, (Node *) plan->lefttree, root);
		}
		else if (IsA(node, MergeJoin))
		{
			MergeJoin  *mj = (MergeJoin *) node;

			if (mj->unique_outer)
			{
				shareinput_walker(f, (Node *) plan->lefttree, root);
				shareinput_walker(f, (Node *) plan->righttree, root);
			}
			else
			{
				shareinput_walker(f, (Node *) plan->righttree, root);
				shareinput_walker(f, (Node *) plan->lefttree, root);
			}
		}
		else if (IsA(node, Sequence))
		{
			ListCell   *cell = NULL;
			Sequence   *sequence = (Sequence *) node;

			foreach(cell, sequence->subplans)
			{
				shareinput_walker(f, (Node *) lfirst(cell), root);
			}
		}
		else
		{
			shareinput_walker(f, (Node *) plan->lefttree, root);
			shareinput_walker(f, (Node *) plan->righttree, root);
			shareinput_walker(f, (Node *) plan->initPlan, root);
		}
	}

	(*f) (node, root, true);
}

/*
 * Create a fake CTE range table entry that reflects the target list of a
 * shared input.
 */
static RangeTblEntry *
create_shareinput_producer_rte(ApplyShareInputContext *ctxt, int share_id,
							   int refno)
{
	int			attno = 1;
	ListCell   *lc;
	Plan	   *subplan;
	char		buf[100];
	RangeTblEntry *rte;
	List	   *colnames = NIL;
	List	   *coltypes = NIL;
	List	   *coltypmods = NIL;
	List	   *colcollations = NIL;

	Assert(ctxt->shared_plans);
	Assert(ctxt->shared_input_count > share_id);
	subplan = ctxt->shared_plans[share_id];

	foreach(lc, subplan->targetlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);
		Oid			vartype;
		int32		vartypmod;
		Oid			varcollid;
		char	   *resname;

		vartype = exprType((Node *) tle->expr);
		vartypmod = exprTypmod((Node *) tle->expr);
		varcollid = exprCollation((Node *) tle->expr);

		/*
		 * We should've filled in tle->resname in shareinput_save_producer().
		 * Note that it's too late to call get_tle_name() here, because this
		 * runs after all the varnos in Vars have already been changed to
		 * INNER_VAR/OUTER_VAR.
		 */
		resname = tle->resname;
		if (!resname)
			resname = pstrdup("unnamed_attr");

		colnames = lappend(colnames, makeString(resname));
		coltypes = lappend_oid(coltypes, vartype);
		coltypmods = lappend_int(coltypmods, vartypmod);
		colcollations = lappend_oid(colcollations, varcollid);
		attno++;
	}

	/*
	 * Create a new RTE. Note that we use a different RTE for each reference,
	 * because we want to give each reference a different name.
	 */
	snprintf(buf, sizeof(buf), "share%d_ref%d", share_id, refno);

	rte = makeNode(RangeTblEntry);
	rte->rtekind = RTE_CTE;
	rte->ctename = pstrdup(buf);
	rte->ctelevelsup = 0;
	rte->self_reference = false;
	rte->alias = NULL;

	rte->eref = makeAlias(rte->ctename, colnames);
	rte->ctecoltypes = coltypes;
	rte->ctecoltypmods = coltypmods;
	rte->ctecolcollations = colcollations;

	rte->inh = false;
	rte->inFromCl = false;

	rte->requiredPerms = 0;
	rte->checkAsUser = InvalidOid;

	return rte;
}

/*
 * Memorize the shared plan of a shared input in an array, one per share_id.
 */
static void
shareinput_save_producer(ShareInputScan *plan, ApplyShareInputContext *ctxt)
{
	int			share_id = plan->share_id;
	int			new_shared_input_count = (share_id + 1);

	Assert(plan->share_id >= 0);

	if (ctxt->shared_plans == NULL)
	{
		ctxt->shared_plans = palloc0(sizeof(Plan *) * new_shared_input_count);
		ctxt->shared_input_count = new_shared_input_count;
	}
	else if (ctxt->shared_input_count < new_shared_input_count)
	{
		ctxt->shared_plans = repalloc(ctxt->shared_plans, new_shared_input_count * sizeof(Plan *));
		memset(&ctxt->shared_plans[ctxt->shared_input_count], 0, (new_shared_input_count - ctxt->shared_input_count) * sizeof(Plan *));
		ctxt->shared_input_count = new_shared_input_count;
	}

	Assert(ctxt->shared_plans[share_id] == NULL);
	ctxt->shared_plans[share_id] = plan->scan.plan.lefttree;
}

/*
 * When a plan comes out of the planner, all the ShareInputScan nodes belonging
 * to the same "share" have the same child node. apply_shareinput_dag_to_tree()
 * turns the DAG into a proper tree. The first occurrence of a ShareInput scan,
 * with a particular child tree, becomes the "producer" of the share, and the
 * others becomes consumers. The subtree is removed from all the consumer nodes.
 *
 * Also, a share_id is assigned to each ShareInputScan node, as well as the
 * Material/Sort nodes below the producers. The producers and its consumers
 * are linked together by the same share_id.
 */
static bool
shareinput_mutator_dag_to_tree(Node *node, PlannerInfo *root, bool fPop)
{
	PlannerGlobal *glob = root->glob;
	ApplyShareInputContext *ctxt = &glob->share;
	Plan	   *plan = (Plan *) node;

	if (fPop)
		return true;

	if (IsA(plan, ShareInputScan))
	{
		ShareInputScan *siscan = (ShareInputScan *) plan;
		Plan	   *subplan = plan->lefttree;
		int			share_id;
		int			attno;
		ListCell   *lc;

		/* on entry, all ShareInputScans should have a child */
		Assert(subplan);

		/* Is there a producer for this sub-tree already? */
		for (share_id = 0; share_id < ctxt->shared_input_count; share_id++)
		{
			if (ctxt->shared_plans[share_id] == subplan)
			{
				/*
				 * Yes. This is a consumer. Remove the subtree, and assign the
				 * same share_id as the producer.
				 */
				siscan->share_id = share_id;
				siscan->scan.plan.lefttree = NULL;
				return false;
			}
		}

		/*
		 * Couldn't find a match in existing list of producers, so this is a
		 * producer. Add this to the list of producers, and assign a new
		 * share_id.
		 */
		siscan->share_id = share_id;

		shareinput_save_producer(siscan, ctxt);

		/*
		 * Also make sure that all the entries in the subplan's target list
		 * have human-readable column names. They are used for EXPLAIN.
		 */
		attno = 1;
		foreach(lc, subplan->targetlist)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(lc);

			if (tle->resname == NULL)
			{
				char		default_name[100];
				char	   *resname;

				snprintf(default_name, sizeof(default_name), "col_%d", attno);

				resname = strVal(get_tle_name(tle, ctxt->curr_rtable, default_name));
				tle->resname = pstrdup(resname);
			}
			attno++;
		}
	}

	return true;
}

Plan *
apply_shareinput_dag_to_tree(PlannerInfo *root, Plan *plan)
{
	PlannerGlobal *glob = root->glob;

	glob->share.curr_rtable = root->parse->rtable;
	shareinput_walker(shareinput_mutator_dag_to_tree, (Node *) plan, root);
	return plan;
}

/*
 * Collect all the producer ShareInput nodes into an array, for later use by
 * replace_shareinput_targetlists().
 *
 * This is a stripped-down version of apply_shareinput_dag_to_tree(), for use
 * on ORCA-produced plans. ORCA assigns share_ids to all ShareInputScan nodes,
 * and only producer nodes have a subtree, so we don't need to do the DAG to
 * tree conversion or assign share_ids here.
 */
static bool
collect_shareinput_producers_walker(Node *node, PlannerInfo *root, bool fPop)
{
	PlannerGlobal *glob = root->glob;
	ApplyShareInputContext *ctxt = &glob->share;

	if (fPop)
		return true;

	if (IsA(node, ShareInputScan))
	{
		ShareInputScan *siscan = (ShareInputScan *) node;
		Plan	   *subplan = siscan->scan.plan.lefttree;

		Assert(siscan->share_id >= 0);

		if (subplan)
			shareinput_save_producer(siscan, ctxt);
	}
	return true;
}

void
collect_shareinput_producers(PlannerInfo *root, Plan *plan)
{
	PlannerGlobal *glob = root->glob;

	glob->share.curr_rtable = glob->finalrtable;
	shareinput_walker(collect_shareinput_producers_walker, (Node *) plan, root);
}

/* Some helper: implements a stack using List. */
static void
shareinput_pushmot(ApplyShareInputContext *ctxt, int motid)
{
	ctxt->motStack = lcons_int(motid, ctxt->motStack);
}
static void
shareinput_popmot(ApplyShareInputContext *ctxt)
{
	list_delete_first(ctxt->motStack);
}
static int
shareinput_peekmot(ApplyShareInputContext *ctxt)
{
	return linitial_int(ctxt->motStack);
}


/*
 * Replace the target list of ShareInputScan nodes, with references
 * to CTEs that we build on the fly.
 *
 * Only one of the ShareInputScan nodes in a plan tree contains the real
 * child plan, while others contain just a "share id" that binds all the
 * ShareInputScan nodes sharing the same input together. The missing
 * child plan is a problem for EXPLAIN, as any OUTER Vars in the
 * ShareInputScan's target list cannot be resolved without the child
 * plan.
 *
 * To work around that issue, create a CTE for each shared input node, with
 * columns that match the target list of the SharedInputScan's subplan,
 * and replace the target list entries of the SharedInputScan with
 * Vars that point to the CTE instead of the child plan.
 */
Plan *
replace_shareinput_targetlists(PlannerInfo *root, Plan *plan)
{
	shareinput_walker(replace_shareinput_targetlists_walker, (Node *) plan, root);
	return plan;
}

static bool
replace_shareinput_targetlists_walker(Node *node, PlannerInfo *root, bool fPop)
{
	PlannerGlobal *glob = root->glob;
	ApplyShareInputContext *ctxt = &glob->share;

	if (fPop)
		return true;

	if (IsA(node, ShareInputScan))
	{
		ShareInputScan *sisc = (ShareInputScan *) node;
		int			share_id = sisc->share_id;
		ListCell   *lc;
		int			attno;
		List	   *newtargetlist;
		RangeTblEntry *rte;

		/*
		 * Note that even though the planner assigns sequential share_ids for
		 * each shared node, so that share_id is always below
		 * list_length(ctxt->sharedNodes), ORCA has a different assignment
		 * scheme. So we have to be prepared for any share_id, at least when
		 * ORCA is in use.
		 */
		if (ctxt->share_refcounts == NULL)
		{
			int			new_sz = share_id + 1;

			ctxt->share_refcounts = palloc0(new_sz * sizeof(int));
			ctxt->share_refcounts_sz = new_sz;
		}
		else if (share_id >= ctxt->share_refcounts_sz)
		{
			int			old_sz = ctxt->share_refcounts_sz;
			int			new_sz = share_id + 1;

			ctxt->share_refcounts = repalloc(ctxt->share_refcounts, new_sz * sizeof(int));
			memset(&ctxt->share_refcounts[old_sz], 0, (new_sz - old_sz) * sizeof(int));
			ctxt->share_refcounts_sz = new_sz;
		}

		ctxt->share_refcounts[share_id]++;

		/*
		 * Create a new RTE. Note that we use a different RTE for each
		 * reference, because we want to give each reference a different name.
		 */
		rte = create_shareinput_producer_rte(ctxt, share_id,
											 ctxt->share_refcounts[share_id]);

		glob->finalrtable = lappend(glob->finalrtable, rte);
		sisc->scan.scanrelid = list_length(glob->finalrtable);

		/*
		 * Replace all the target list entries.
		 *
		 * SharedInputScan nodes are not projection-capable, so the target
		 * list of the SharedInputScan matches the subplan's target list.
		 */
		newtargetlist = NIL;
		attno = 1;
		foreach(lc, sisc->scan.plan.targetlist)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(lc);
			TargetEntry *newtle = flatCopyTargetEntry(tle);

			newtle->expr = (Expr *) makeVar(sisc->scan.scanrelid, attno,
											exprType((Node *) tle->expr),
											exprTypmod((Node *) tle->expr),
											exprCollation((Node *) tle->expr),
											0);
			newtargetlist = lappend(newtargetlist, newtle);
			attno++;
		}
		sisc->scan.plan.targetlist = newtargetlist;
	}

	return true;
}

/*
 * First walk on shareinput xslice. Collect information about the producer
 * and consumer slice IDs for each share. It also builds a list of shares
 * that should run in the QD.
 */
static bool
shareinput_mutator_xslice_1(Node *node, PlannerInfo *root, bool fPop)
{
	PlannerGlobal *glob = root->glob;
	ApplyShareInputContext *ctxt = &glob->share;
	Plan	   *plan = (Plan *) node;

	if (fPop)
	{
		if (IsA(plan, Motion))
			shareinput_popmot(ctxt);
		return false;
	}

	if (IsA(plan, Motion))
	{
		Motion	   *motion = (Motion *) plan;

		shareinput_pushmot(ctxt, motion->motionID);
		return true;
	}

	if (IsA(plan, ShareInputScan))
	{
		ShareInputScan *sisc = (ShareInputScan *) plan;
		int			motId = shareinput_peekmot(ctxt);
		Plan	   *shared = plan->lefttree;
		PlanSlice  *currentSlice;
		ApplyShareInputContextPerShare *share_info;

		share_info = &ctxt->shared_inputs[sisc->share_id];

		currentSlice = &ctxt->slices[motId];
		if (currentSlice->gangType == GANGTYPE_UNALLOCATED ||
			currentSlice->gangType == GANGTYPE_ENTRYDB_READER)
		{
			ctxt->qdShares = bms_add_member(ctxt->qdShares, sisc->share_id);
		}

		/* Remember information about the slice that this instance appears in. */
		if (shared)
			ctxt->shared_inputs[sisc->share_id].producer_slice_id = motId;
		share_info->participant_slices = bms_add_member(share_info->participant_slices, motId);

		sisc->this_slice_id = motId;
	}

	return true;
}

/*
 * Second pass:
 * 1. Mark shareinput scans with multiple consumer slices as cross-slice.
 * 2. Fill 'share_type' and 'share_id' fields in the shared Material/Sort nodes.
 */
static bool
shareinput_mutator_xslice_2(Node *node, PlannerInfo *root, bool fPop)
{
	PlannerGlobal *glob = root->glob;
	ApplyShareInputContext *ctxt = &glob->share;
	Plan	   *plan = (Plan *) node;

	if (fPop)
	{
		if (IsA(plan, Motion))
			shareinput_popmot(ctxt);
		return false;
	}

	if (IsA(plan, Motion))
	{
		Motion	   *motion = (Motion *) plan;

		shareinput_pushmot(ctxt, motion->motionID);
		return true;
	}

	if (IsA(plan, ShareInputScan))
	{
		ShareInputScan *sisc = (ShareInputScan *) plan;
		int			motId = shareinput_peekmot(ctxt);
		ApplyShareInputContextPerShare *pershare;

		pershare = &ctxt->shared_inputs[sisc->share_id];

		if (bms_num_members(pershare->participant_slices) > 1)
		{
			Assert(!sisc->cross_slice);
			sisc->cross_slice = true;
		}

		sisc->producer_slice_id = pershare->producer_slice_id;
		sisc->nconsumers = bms_num_members(pershare->participant_slices) - 1;

		/*
		 * If this share needs to run in the QD, mark the slice accordingly.
		 */
		if (bms_is_member(sisc->share_id, ctxt->qdShares))
		{
			PlanSlice  *currentSlice = &ctxt->slices[motId];

			switch (currentSlice->gangType)
			{
				case GANGTYPE_UNALLOCATED:
				case GANGTYPE_ENTRYDB_READER:
					break;
				case GANGTYPE_SINGLETON_READER:
					currentSlice->gangType = GANGTYPE_ENTRYDB_READER;
					break;
				case GANGTYPE_PRIMARY_READER:
				case GANGTYPE_PRIMARY_WRITER:
					elog(ERROR, "cannot share ShareInputScan between QD and primary reader/write gang");
					break;
			}
		}
	}
	return true;
}

/*
 * Scan through the plan tree and make note of which Share Input Scans
 * are cross-slice.
 */
Plan *
apply_shareinput_xslice(Plan *plan, PlannerInfo *root)
{
	PlannerGlobal *glob = root->glob;
	ApplyShareInputContext *ctxt = &glob->share;
	ListCell   *lp, *lr;
	int			subplan_id;

	/*
	 * If the plan tree has only one slice, there cannot be any cross-slice
	 * Share Input Scans. They were all marked as cross_slice=false when they
	 * were created. Note that we won't set slice_ids on them correctly;
	 * the executor knows not to expect that when numSlices == 1.
	 */
	if (root->glob->numSlices == 1)
		return plan;

	ctxt->motStack = NULL;
	ctxt->qdShares = NULL;
	ctxt->slices = root->glob->slices;

	ctxt->shared_inputs = palloc0(ctxt->shared_input_count * sizeof(ApplyShareInputContextPerShare));

	shareinput_pushmot(ctxt, 0);

	/*
	 * Walk the tree.  See comment for each pass for what each pass will do.
	 * The context is used to carry information from one pass to another, as
	 * well as within a pass.
	 */

	/*
	 * A subplan might have a SharedScan consumer while the SharedScan
	 * producer is in the main plan, or vice versa. So in the first pass, we
	 * walk through all plans and collect all producer subplans into the
	 * context, before processing the consumers.
	 */
	subplan_id = 0;
	forboth(lp, glob->subplans, lr, glob->subroots)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);
		PlannerInfo *subroot =  (PlannerInfo *) lfirst(lr);
		int			slice_id = glob->subplan_sliceIds[subplan_id];

		shareinput_pushmot(ctxt, slice_id);
		shareinput_walker(shareinput_mutator_xslice_1, (Node *) subplan, subroot);
		shareinput_popmot(ctxt);
		subplan_id++;
	}
	shareinput_walker(shareinput_mutator_xslice_1, (Node *) plan, root);

	/* Now walk the tree again, and process all the consumers. */
	subplan_id = 0;
	forboth(lp, glob->subplans, lr, glob->subroots)
	{
		Plan	   *subplan = (Plan *) lfirst(lp);
		PlannerInfo *subroot =  (PlannerInfo *) lfirst(lr);
		int			slice_id = glob->subplan_sliceIds[subplan_id];

		shareinput_pushmot(ctxt, slice_id);
		shareinput_walker(shareinput_mutator_xslice_2, (Node *) subplan, subroot);
		shareinput_popmot(ctxt);
		subplan_id++;
	}
	shareinput_walker(shareinput_mutator_xslice_2, (Node *) plan, root);

	return plan;
}

/*
 * Hash a list of const values with GPDB's hash function
 */
int32
cdbhash_const_list(List *plConsts, int iSegments, Oid *hashfuncs)
{
	ListCell   *lc;
	CdbHash    *pcdbhash;
	int			i;

	Assert(0 < list_length(plConsts));

	pcdbhash = makeCdbHash(iSegments, list_length(plConsts), hashfuncs);

	cdbhashinit(pcdbhash);

	Assert(0 < list_length(plConsts));

	i = 0;
	foreach(lc, plConsts)
	{
		Const	   *pconst = (Const *) lfirst(lc);

		cdbhash(pcdbhash, i + 1, pconst->constvalue, pconst->constisnull);
		i++;
	}

	return cdbhashreduce(pcdbhash);
}

/*
 * Construct an expression that checks whether the current segment is
 * 'segid'.
 */
Node *
makeSegmentFilterExpr(int segid)
{
	/* Build an expression: gp_execution_segment() = <segid> */
	return (Node *)
		make_opclause(Int4EqualOperator,
					  BOOLOID,
					  false,	/* opretset */
					  (Expr *) makeFuncExpr(F_MPP_EXECUTION_SEGMENT,
											INT4OID,
											NIL,	/* args */
											InvalidOid,
											InvalidOid,
											COERCE_EXPLICIT_CALL),
					  (Expr *) makeConst(INT4OID,
										 -1,		/* consttypmod */
										 InvalidOid, /* constcollid */
										 sizeof(int32),
										 Int32GetDatum(segid),
										 false,		/* constisnull */
										 true),		/* constbyval */
					  InvalidOid,	/* opcollid */
					  InvalidOid	/* inputcollid */
			);
}

typedef struct ParamWalkerContext
{
	plan_tree_base_prefix base; /* Required prefix for
								 * plan_tree_walker/mutator */
	Bitmapset  *paramids;		/* Bitmapset for Param */
	Bitmapset  *scanrelids;		/* Bitmapset for scanrelid */
} ParamWalkerContext;

static bool
param_walker(Node *node, ParamWalkerContext *context)
{
	PlannerInfo *root = (PlannerInfo *) context->base.node;
	Param	   *param;
	Aggref	   *aggref;
	Scan	   *scan;
	ListCell   *lc;

	if (node == NULL)
		return false;

	switch (nodeTag(node))
	{
		case T_Param:
			param = (Param *) node;
			if (param->paramkind == PARAM_EXEC)
			{
				if (!bms_is_member(param->paramid, context->paramids))
					context->paramids = bms_add_member(context->paramids,
													   param->paramid);
			}
			return false;

		case T_Aggref:
			/*
			 * See if it's an Aggref that will be replaced by a Param in
			 * set_plan_references()
			 */
			aggref = (Aggref *) node;
			if (root->minmax_aggs != NIL &&
				list_length(aggref->args) == 1)
			{
				TargetEntry *curTarget = (TargetEntry *) linitial(aggref->args);
				ListCell   *lc;

				foreach(lc, root->minmax_aggs)
				{
					MinMaxAggInfo *mminfo = (MinMaxAggInfo *) lfirst(lc);

					if (mminfo->aggfnoid == aggref->aggfnoid &&
						equal(mminfo->target, curTarget->expr))
						param_walker((Node *) mminfo->param, context);
				}
			}
			break;

		case T_ValuesScan:
		case T_FunctionScan:
		case T_TableFunctionScan:
			scan = (Scan *) node;
			if (!bms_is_member(scan->scanrelid, context->scanrelids))
				context->scanrelids = bms_add_member(context->scanrelids,
													 scan->scanrelid);
			break;

		case T_SubPlan:
			{
				PlannerInfo *root = (PlannerInfo *) context->base.node;
				SubPlan	   *spexpr = (SubPlan *) node;
				Plan	   *subplan_plan = planner_subplan_get_plan(root, spexpr);
				PlannerInfo *subplan_root = planner_subplan_get_root(root, spexpr);
				Node	   *save_root;

				if (spexpr->subLinkType == MULTIEXPR_SUBLINK &&
					spexpr->is_initplan)
				{
					foreach (lc, spexpr->setParam)
					{
						int			paramid = lfirst_int(lc);

						if (!bms_is_member(paramid, context->paramids))
							context->paramids = bms_add_member(context->paramids,
															   paramid);
					}
				}

				/* recurse into the subplan */
				save_root = context->base.node;
				context->base.node = (Node *) subplan_root;
				if (param_walker((Node *) subplan_plan, context))
					return true;

				context->base.node = save_root;

				/*
				 * fall through to let plan_tree_walker() handle any expressions in
				 * testexpr and args
				 */
			}
			break;

		default:
			break;
	}

	return plan_tree_walker(node, param_walker, context, false);
}

/*
 * Retrieve param ids that are referenced in RTEs.
 * We can't simply use range_table_walker() here, because we only
 * want to walk through RTEs that are referenced in the plan.
 */
static void
rte_param_walker(List *rtable, ParamWalkerContext *context)
{
	ListCell   *lc;
	int			rteid = 0;
	ListCell   *func_lc;

	foreach(lc, rtable)
	{
		rteid++;
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(lc);

		if (!bms_is_member(rteid, context->scanrelids))
		{
			/* rte not referenced in the plan */
			continue;
		}

		switch (rte->rtekind)
		{
			case RTE_RELATION:
			case RTE_VOID:
			case RTE_CTE:
				/* nothing to do */
				break;
			case RTE_SUBQUERY:
				param_walker((Node *) rte->subquery, context);
				break;
			case RTE_JOIN:
				param_walker((Node *) rte->joinaliasvars, context);
				break;
			case RTE_FUNCTION:
				foreach(func_lc, rte->functions)
				{
					RangeTblFunction *rtfunc = (RangeTblFunction *) lfirst(func_lc);
					param_walker(rtfunc->funcexpr, context);
				}
				break;
			case RTE_TABLEFUNCTION:
				param_walker((Node *) rte->subquery, context);
				foreach(func_lc, rte->functions)
				{
					RangeTblFunction *rtfunc = (RangeTblFunction *) lfirst(func_lc);
					param_walker(rtfunc->funcexpr, context);
				}
				break;
			case RTE_VALUES:
				param_walker((Node *) rte->values_lists, context);
				break;
		}
	}
}

static bool
initplan_walker(Node *node, ParamWalkerContext *context)
{
	PlannerInfo *root;
	List	   *new_initplans = NIL;
	ListCell   *lc,
			   *lp;
	bool		anyused;		/* Are any of this Init Plan's output
								 * parameters actually used? */

	if (node == NULL)
		return false;

	if (is_plan_node(node))
	{
		root = (PlannerInfo *) context->base.node;
		Plan	   *plan = (Plan *) node;

		foreach(lc, plan->initPlan)
		{
			SubPlan    *subplan = (SubPlan *) lfirst(lc);

			Assert(subplan->is_initplan);

			anyused = false;
			foreach(lp, subplan->setParam)
			{
				int			paramid = lfirst_int(lp);

				if (bms_is_member(paramid, context->paramids))
				{
					anyused = true;
					break;
				}
			}

			/* If none of its params are used, leave out from the new list */
			if (anyused)
				new_initplans = lappend(new_initplans, subplan);
			else
			{
				/*
				 * This init plan is unused. Leave it out of this plan node's
				 * initPlan list, and also replace it in the global list of
				 * subplans with a dummy. (We can't just remove it from the
				 * global list, because that would screw up the plan_id
				 * numbering of the subplans).
				 */
				Result	   *dummy = make_result(NIL,
												(Node *) list_make1(makeBoolConst(false, false)),
												NULL);

				planner_subplan_put_plan(root, subplan, (Plan *) dummy);
			}
		}

		/* remove unused params */
		plan->allParam = bms_intersect(plan->allParam, context->paramids);

		list_free(plan->initPlan);
		plan->initPlan = new_initplans;
	}

	return plan_tree_walker(node, initplan_walker, context, true);
}

/*
 * Remove unused initplans from the given plan object
 */
void
remove_unused_initplans(Plan *top_plan, PlannerInfo *root)
{
	ParamWalkerContext context;

	if (!root->glob->subplans)
		return;

	context.base.node = (Node *) root;
	context.paramids = NULL;
	context.scanrelids = NULL;

	/*
	 * Collect param ids of all the Params that are referenced in the plan,and
	 * the IDs of all the range table entries that are referenced in the Plan.
	 */
	param_walker((Node *) top_plan, &context);

	/*
	 * Now that we know which range table entries are referenced in the plan,
	 * also collect Params from those range table entries.
	 */
	rte_param_walker(root->glob->finalrtable, &context);

	/* workhorse to remove unused initplans */
	initplan_walker((Node *) top_plan, &context);

	bms_free(context.paramids);
	bms_free(context.scanrelids);
}

/*
 * Evaluate functions to constants.
 */
Node *
exec_make_plan_constant(struct PlannedStmt *stmt, EState *estate, bool is_SRI,
						List **cursorPositions)
{
	pre_dispatch_function_evaluation_context pcontext;
	Node	   *result;

	Assert(stmt);
	exec_init_plan_tree_base(&pcontext.base, stmt);
	pcontext.single_row_insert = is_SRI;
	pcontext.cursorPositions = NIL;
	pcontext.estate = estate;

	result = pre_dispatch_function_evaluation_mutator((Node *) stmt->planTree, &pcontext);

	*cursorPositions = pcontext.cursorPositions;
	return result;
}

/*
 * Remove subquery field in RTE's with subquery kind.
 */
void
remove_subquery_in_RTEs(Node *node)
{
	if (node == NULL)
	{
		return;
	}

	if (IsA(node, RangeTblEntry))
	{
		RangeTblEntry *rte = (RangeTblEntry *) node;

		if (RTE_SUBQUERY == rte->rtekind && NULL != rte->subquery)
		{
			/*
			 * Replace subquery with a dummy subquery.
			 *
			 * XXX: We could save a lot more memory by deep-freeing the many
			 * fields in the Query too. But I'm not sure which of them might
			 * be shared by other objects in the tree.
			 */
			pfree(rte->subquery);
			rte->subquery = NULL;
		}

		return;
	}

	if (IsA(node, List))
	{
		List	   *list = (List *) node;
		ListCell   *lc = NULL;

		foreach(lc, list)
		{
			remove_subquery_in_RTEs((Node *) lfirst(lc));
		}
	}
}

/*
 * Let's evaluate all STABLE functions that have constant args before
 * dispatch, so we get a consistent view across QEs
 *
 * Also, if this is a single_row insert, let's evaluate nextval() and
 * currval() before dispatching
 */
static Node *
pre_dispatch_function_evaluation_mutator(Node *node,
										 pre_dispatch_function_evaluation_context *context)
{
	Node	   *new_node = 0;

	if (node == NULL)
		return NULL;

	if (IsA(node, Param))
	{
		Param	   *param = (Param *) node;

		/* Not replaceable, so just copy the Param (no need to recurse) */
		return (Node *) copyObject(param);
	}
	else if (IsA(node, FuncExpr))
	{
		FuncExpr   *expr = (FuncExpr *) node;
		List	   *args;
		ListCell   *arg;
		Expr	   *simple;
		FuncExpr   *newexpr;
		bool		has_nonconst_input;

		Form_pg_proc funcform;
		EState	   *estate;
		ExprState  *exprstate;
		MemoryContext oldcontext;
		Datum		const_val;
		bool		const_is_null;
		int16		resultTypLen;
		bool		resultTypByVal;

		Oid			funcid;
		HeapTuple	func_tuple;

		/*
		 * Reduce constants in the FuncExpr's arguments. We know args is
		 * either NIL or a List node, so we can call expression_tree_mutator
		 * directly rather than recursing to self.
		 */
		args = (List *) expression_tree_mutator((Node *) expr->args,
												pre_dispatch_function_evaluation_mutator,
												(void *) context);

		funcid = expr->funcid;

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
		newexpr->is_tablefunc = expr->is_tablefunc;

		/*
		 * Check for constant inputs
		 */
		has_nonconst_input = false;

		foreach(arg, args)
		{
			if (!IsA(lfirst(arg), Const))
			{
				has_nonconst_input = true;
				break;
			}
		}

		if (!has_nonconst_input)
		{
			bool		is_seq_func = false;
			bool		tup_or_set;

			func_tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
			if (!HeapTupleIsValid(func_tuple))
				elog(ERROR, "cache lookup failed for function %u", funcid);

			funcform = (Form_pg_proc) GETSTRUCT(func_tuple);

			/* can't handle set returning or row returning functions */
			tup_or_set = (funcform->proretset || type_is_rowtype(funcform->prorettype));

			ReleaseSysCache(func_tuple);

			/* can't handle it */
			if (tup_or_set)
			{
				/*
				 * We haven't mutated this node, but we still return the
				 * mutated arguments.
				 *
				 * If we don't do this, we'll miss out on transforming
				 * function arguments which are themselves functions we need
				 * to mutated. For example, select foo(now()).
				 */
				return (Node *) newexpr;
			}

			/*
			 * Here we want to mark any statement that is going to use a
			 * sequence as dirty.  Doing this means that the QD will flush the
			 * xlog which will also flush any xlog writes that the sequence
			 * server might do.
			 */
			if (funcid == F_NEXTVAL_OID || funcid == F_CURRVAL_OID ||
				funcid == F_SETVAL_OID)
			{
				ExecutorMarkTransactionUsesSequences();
				is_seq_func = true;
			}

			if (funcform->provolatile == PROVOLATILE_IMMUTABLE)
				 /* okay */ ;
			else if (funcform->provolatile == PROVOLATILE_STABLE)
				 /* okay */ ;
			else if (context->single_row_insert && is_seq_func)
				;				/* Volatile, but special sequence function */
			else
				return (Node *) newexpr;

			/*
			 * Ok, we have a function that is STABLE (or IMMUTABLE), with
			 * constant args. Let's try to evaluate it.
			 */

			/*
			 * To use the executor, we need an EState.
			 */
			estate = CreateExecutorState();

			/* We can use the estate's working context to avoid memory leaks. */
			oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

			/*
			 * Prepare expr for execution.
			 */
			exprstate = ExecPrepareExpr((Expr *) newexpr, estate);

			/*
			 * And evaluate it.
			 *
			 * It is OK to use a default econtext because none of the
			 * ExecEvalExpr() code used in this situation will use econtext.
			 * That might seem fortuitous, but it's not so unreasonable --- a
			 * constant expression does not depend on context, by definition,
			 * n'est-ce pas?
			 */
			const_val = ExecEvalExprSwitchContext(exprstate,
												  GetPerTupleExprContext(estate),
												  &const_is_null, NULL);

			/* Get info needed about result datatype */
			get_typlenbyval(expr->funcresulttype, &resultTypLen, &resultTypByVal);

			/* Get back to outer memory context */
			MemoryContextSwitchTo(oldcontext);

			/* Must copy result out of sub-context used by expression eval */
			if (!const_is_null)
				const_val = datumCopy(const_val, resultTypByVal, resultTypLen);

			/* Release all the junk we just created */
			FreeExecutorState(estate);

			/*
			 * Make the constant result node.
			 */
			simple = (Expr *) makeConst(expr->funcresulttype,
										-1,
										expr->funccollid,
										resultTypLen,
										const_val, const_is_null,
										resultTypByVal);

			/* successfully simplified it */
			if (simple)
				return (Node *) simple;
		}

		/*
		 * The expression cannot be simplified any further, so build and
		 * return a replacement FuncExpr node using the possibly-simplified
		 * arguments.
		 */
		return (Node *) newexpr;
	}
	else if (IsA(node, OpExpr))
	{
		OpExpr	   *expr = (OpExpr *) node;
		List	   *args;

		OpExpr	   *newexpr;

		/*
		 * Reduce constants in the OpExpr's arguments.  We know args is either
		 * NIL or a List node, so we can call expression_tree_mutator directly
		 * rather than recursing to self.
		 */
		args = (List *) expression_tree_mutator((Node *) expr->args,
												pre_dispatch_function_evaluation_mutator,
												(void *) context);

		/*
		 * Need to get OID of underlying function.	Okay to scribble on input
		 * to this extent.
		 */
		set_opfuncid(expr);

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
	else if (IsA(node, CurrentOfExpr))
	{
		/*
		 * updatable cursors
		 *
		 * During constant folding, we collect the current position of each
		 * cursor mentioned in the plan into a list, and dispatch them to the
		 * QEs.
		 *
		 * We should not get here if called from planner_make_plan_constant().
		 * That is only used for simple Result plans, which should not contain
		 * CURRENT OF expressions.
		 */
		if (context->estate)
		{
			CurrentOfExpr *expr = (CurrentOfExpr *) node;
			CursorPosInfo *cpos;

			cpos = makeNode(CursorPosInfo);

			getCurrentOf(expr,
						 GetPerTupleExprContext(context->estate),
						 expr->target_relid,
						 &cpos->ctid,
						 &cpos->gp_segment_id,
						 &cpos->table_oid,
						 &cpos->cursor_name);

			context->cursorPositions = lappend(context->cursorPositions, cpos);
		}
	}

	/*
	 * For any node type not handled above, we recurse using
	 * plan_tree_mutator, which will copy the node unchanged but try to
	 * simplify its arguments (if any) using this routine.
	 */
	new_node = plan_tree_mutator(node,
								 pre_dispatch_function_evaluation_mutator,
								 (void *) context,
								 true);

	return new_node;
}

/*
 * cdbpathtoplan_create_sri_path
 *
 * Optimize for single-row-insertion for const result. If result is const, and
 * result relation is partitioned, we could decide partition relation during
 * plan time and replace targetPolicy by partition relation's targetPolicy.
 * In addition, we don't need tuple distribution, but do filter on each writer
 * segment.
 *
 * Inputs:
 *
 * root		PlannerInfo passed by caller
 * plan		should always be result node
 * rte		is the target relation entry
 *
 * Inputs/Outputs:
 *
 * targetPolicy(in/out) is the target relation policy, and would be replaced
 * by partition relation.
 * hashExpr is distribution expression of target relation, and would be
 * replaced by partition relation.
 */
Plan *
cdbpathtoplan_create_sri_plan(RangeTblEntry *rte, PlannerInfo *subroot, Path *subpath,
							  int createplan_flags)
{
	CdbMotionPath *motionpath;
	ResultPath *resultpath;
	Result	   *resultplan;
	Relation	rel;
	PartitionNode *pn;
	GpPolicy   *targetPolicy;
	List	   *hashExprs;
	List	   *hashOpfamilies;
	int			numHashAttrs;
	AttrNumber *hashAttrs;
	Oid		   *hashFuncs;
	int			i;
	ListCell   *cell;

	if (!gp_enable_fast_sri)
		return NULL;

	if (!IsA(subpath, CdbMotionPath))
		return NULL;
	motionpath = (CdbMotionPath *) subpath;

	if (!IsA(motionpath->subpath, ResultPath))
		return NULL;

	resultpath = (ResultPath *) motionpath->subpath;

	if (contain_mutable_functions((Node *) resultpath->path.pathtarget->exprs))
		return NULL;

	resultplan = (Result *) create_plan_recurse(subroot, (Path *) resultpath, createplan_flags);
	if (!IsA(resultplan, Result))
		return NULL;

	/* Suppose caller already hold proper locks for relation. */
	rel = relation_open(rte->relid, NoLock);
	targetPolicy = rel->rd_cdbpolicy;

	/* 1: See if it's partitioned */
	pn = RelationBuildPartitionDesc(rel, false);

	/* Look up the distribution policy for the target partition. */
	if (pn && !partition_policies_equal(targetPolicy, pn))
	{
		/*
		 * 2: See if partitioning columns are constant
		 */
		List	   *partatts = get_partition_attrs(pn);
		ListCell   *lc;
		TupleTableSlot *slot;
		bool	   *nulls;
		Datum	   *values;
		EState	   *estate = CreateExecutorState();
		ResultRelInfo *rri;

		/*
		 * 4: build tuple, look up partitioning key
		 */
		slot = MakeSingleTupleTableSlot(RelationGetDescr(rel));
		ExecClearTuple(slot);
		values = slot_get_values(slot);
		nulls = slot_get_isnull(slot);

		foreach(lc, partatts)
		{
			AttrNumber	attnum = lfirst_int(lc);
			List	   *tl = resultplan->plan.targetlist;
			ListCell   *cell;

			foreach(cell, tl)
			{
				TargetEntry *tle = (TargetEntry *) lfirst(cell);

				Assert(tle->expr);

				if (tle->resno == attnum)
				{
					/*
					 * GPDB_96_MERGE_FIXME: it's bogus to assume that the
					 * entries are all Consts. We used to verify that explicitly,
					 * but now we just call contain_mutable_functions(), which
					 * will return true also for more complicated expressions
					 * that only contain immutable functions. The right way would
					 * be to evaluate the expression here, with ExecPrepareExpr() +
					 * ExecEvalExpr(). In practice this works, because the planner
					 * has already "preprocessed" the target list, which does
					 * replace more complicated expressions with Consts.
					 */
					Assert(IsA(tle->expr, Const));

					nulls[attnum - 1] = ((Const *) tle->expr)->constisnull;
					if (!nulls[attnum - 1])
						values[attnum - 1] = ((Const *) tle->expr)->constvalue;
				}
			}
		}
		ExecStoreVirtualTuple(slot);

		estate->es_result_partitions = pn;
		estate->es_partition_state =
			createPartitionState(estate->es_result_partitions, 1 /* resultPartSize */ );

		rri = makeNode(ResultRelInfo);
		rri->ri_RangeTableIndex = 1;	/* dummy */
		rri->ri_RelationDesc = rel;

		estate->es_result_relations = rri;
		estate->es_num_result_relations = 1;
		estate->es_result_relation_info = rri;
		rri = slot_get_partition(slot, estate, false);

		/*
		 * 5: get target policy for destination table
		 */
		targetPolicy = RelationGetPartitioningKey(rri->ri_RelationDesc);

		if (targetPolicy->ptype != POLICYTYPE_PARTITIONED)
			elog(ERROR, "policy must be partitioned");

		heap_close(rri->ri_RelationDesc, NoLock);
		FreeExecutorState(estate);
	}

	hashExprs = getExprListFromTargetList(resultplan->plan.targetlist,
										  targetPolicy->nattrs,
										  targetPolicy->attrs);
	hashOpfamilies = NIL;
	for (int i = 0; i < targetPolicy->nattrs; i++)
	{
		Oid			opfamily = get_opclass_family(targetPolicy->opclasses[i]);

		hashOpfamilies = lappend_oid(hashOpfamilies, opfamily);
	}

	/*
	 * If there is no distribution key, don't do direct dispatch.
	 *
	 * GPDB_90_MERGE_FIXME: Is that the right thing to do? Couldn't we
	 * direct dispatch to any arbitrarily chosen segment, in that case?
	 */
	numHashAttrs = targetPolicy->nattrs;

	if (numHashAttrs > 0)
	{
		/* Get hash functions for the columns. */
		hashFuncs = palloc(numHashAttrs * sizeof(Oid));
		i = 0;
		foreach(cell, hashExprs)
		{
			Expr	   *elem = (Expr *) lfirst(cell);
			Oid			att_type = exprType((Node *) elem);
			Oid			opclass = targetPolicy->opclasses[i];

			hashFuncs[i++] =
				cdb_hashproc_in_opfamily(get_opclass_family(opclass),
										 att_type);
		}

		/*
		 * all constants in values clause -- no need to repartition.
		 */

		/* copy the attributes array */
		hashAttrs = palloc(numHashAttrs * sizeof(AttrNumber));
		for (i = 0; i < numHashAttrs; i++)
			hashAttrs[i] = targetPolicy->attrs[i];

		if (subroot->config->gp_enable_direct_dispatch)
		{
			DirectDispatchUpdateContentIdsForInsert(subroot,
													&resultplan->plan,
													targetPolicy,
													hashFuncs);

			/*
			 * we now either have a hash-code, or we've marked the plan
			 * non-directed.
			 */
		}

		resultplan->numHashFilterCols = numHashAttrs;
		resultplan->hashFilterColIdx = hashAttrs;
		resultplan->hashFilterFuncs = hashFuncs;
	}
	else
		resultplan = NULL;

	relation_close(rel, NoLock);

	return (Plan *) resultplan;
}

/*
 * Does the given expression contain Params that are passed down from
 * outer query?
 */
bool
contains_outer_params(Node *node, void *context)
{
	PlannerInfo *root = (PlannerInfo *) context;

	if (node == NULL)
		return false;
	if (IsA(node, Param))
	{
		Param	   *param = (Param *) node;

		if (param->paramkind == PARAM_EXEC)
		{
			/* Does this Param refer to a value that an outer query provides? */
			PlannerInfo *parent = root->parent_root;

			while (parent)
			{
				ListCell   *lc;

				foreach (lc, parent->plan_params)
				{
					PlannerParamItem *ppi = (PlannerParamItem *) lfirst(lc);

					if (ppi->paramId == param->paramid)
						return true;		/* abort the tree traversal and return true */
				}

				parent = parent->parent_root;
			}
		}
	}
	return expression_tree_walker(node, contains_outer_params, context);
}
