/*-------------------------------------------------------------------------
 *
 * setrefs.c
 *	  Post-processing of a completed plan tree: fix references to subplan
 *	  vars, compute regproc values for operators, etc
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/plan/setrefs.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/transam.h"
#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/pathnode.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/tlist.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "optimizer/pathnode.h"
#include "optimizer/planmain.h"
#include "optimizer/tlist.h"
#include "tcop/utility.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "cdb/cdbhash.h"


typedef struct
{
	Index		varno;			/* RT index of Var */
	AttrNumber	varattno;		/* attr number of Var */
	AttrNumber	resno;			/* TLE position of Var */
} tlist_vinfo;

typedef struct
{
	List	   *tlist;			/* underlying target list */
	int			num_vars;		/* number of plain Var tlist entries */
	bool		has_ph_vars;	/* are there PlaceHolderVar entries? */
	bool		has_non_vars;	/* are there other entries? */
	/* array of num_vars entries: */
	tlist_vinfo vars[1];		/* VARIABLE LENGTH ARRAY */
} indexed_tlist;				/* VARIABLE LENGTH STRUCT */

typedef struct
{
	PlannerInfo *root;
	int			rtoffset;
} fix_scan_expr_context;

typedef struct
{
	PlannerInfo *root;
	indexed_tlist *outer_itlist;
	indexed_tlist *inner_itlist;
	Index		acceptable_rel;
	int			rtoffset;
	bool        use_outer_tlist_for_matching_nonvars;
	bool        use_inner_tlist_for_matching_nonvars;
} fix_join_expr_context;

typedef struct
{
	PlannerInfo *root;
	indexed_tlist *subplan_itlist;
	Index		newvarno;
	int			rtoffset;
	bool		flow_expr;
} fix_upper_expr_context;

typedef struct
{
	PlannerInfo *root;
	plan_tree_base_prefix base;
} cdb_extract_plan_dependencies_context;

/*
 * Check if a Const node is a regclass value.  We accept plain OID too,
 * since a regclass Const will get folded to that type if it's an argument
 * to oideq or similar operators.  (This might result in some extraneous
 * values in a plan's list of relation dependencies, but the worst result
 * would be occasional useless replans.)
 */
#define ISREGCLASSCONST(con) \
	(((con)->consttype == REGCLASSOID || (con)->consttype == OIDOID) && \
	 !(con)->constisnull)

#define fix_scan_list(root, lst, rtoffset) \
	((List *) fix_scan_expr(root, (Node *) (lst), rtoffset))

static void add_rtes_to_flat_rtable(PlannerInfo *root, bool recursing);
static void flatten_unplanned_rtes(PlannerGlobal *glob, RangeTblEntry *rte);
static bool flatten_rtes_walker(Node *node, PlannerGlobal *glob);
static void add_rte_to_flat_rtable(PlannerGlobal *glob, RangeTblEntry *rte);
static Plan *set_plan_refs(PlannerInfo *root, Plan *plan, int rtoffset);
static Plan *set_indexonlyscan_references(PlannerInfo *root,
							 IndexOnlyScan *plan,
							 int rtoffset);
static Plan *set_subqueryscan_references(PlannerInfo *root,
							SubqueryScan *plan,
							int rtoffset);
static bool trivial_subqueryscan(SubqueryScan *plan);
static Node *fix_scan_expr(PlannerInfo *root, Node *node, int rtoffset);
static Node *fix_scan_expr_mutator(Node *node, fix_scan_expr_context *context);
static bool fix_scan_expr_walker(Node *node, fix_scan_expr_context *context);
static void set_join_references(PlannerInfo *root, Join *join, int rtoffset);
static void set_upper_references(PlannerInfo *root, Plan *plan, int rtoffset);
static void set_dummy_tlist_references(Plan *plan, int rtoffset);
static void set_splitupdate_tlist_references(Plan *plan, int rtoffset);
static indexed_tlist *build_tlist_index(List *tlist);
static Var *search_indexed_tlist_for_var(Var *var,
							 indexed_tlist *itlist,
							 Index newvarno,
							 int rtoffset);
static Var *search_indexed_tlist_for_non_var(Node *node,
								 indexed_tlist *itlist,
								 Index newvarno);
static Var *search_indexed_tlist_for_non_var_in_flow_expr(Node *node,
														 indexed_tlist *itlist,
														 Index newvarno);
static Var *search_indexed_tlist_for_sortgroupref(Node *node,
									  Index sortgroupref,
									  indexed_tlist *itlist,
									  Index newvarno);
static List *fix_join_expr(PlannerInfo *root,
			  List *clauses,
			  indexed_tlist *outer_itlist,
			  indexed_tlist *inner_itlist,
			  Index acceptable_rel, int rtoffset);
static Node *fix_join_expr_mutator(Node *node,
					  fix_join_expr_context *context);
static List *fix_hashclauses(PlannerInfo *root,
				List *clauses,
				indexed_tlist *outer_itlist,
				indexed_tlist *inner_itlist,
				Index acceptable_rel, int rtoffset);
static List *fix_child_hashclauses(PlannerInfo *root,
					  List *clauses,
					  indexed_tlist *outer_itlist,
					  indexed_tlist *inner_itlist,
					  Index acceptable_rel, int rtoffset,
					  Index child);
static Node *fix_upper_expr(PlannerInfo *root,
			   Node *node,
			   indexed_tlist *subplan_itlist,
			   Index newvarno,
			   int rtoffset);
static Node *fix_upper_flow_expr(PlannerInfo *root,
								  Node *node,
								  indexed_tlist *subplan_itlist,
								  Index newvarno,
								  int rtoffset);
static Node *fix_upper_expr_mutator(Node *node,
					   fix_upper_expr_context *context);
static List *set_returning_clause_references(PlannerInfo *root,
								List *rlist,
								Plan *topplan,
								Index resultRelation,
								int rtoffset);
static bool fix_opfuncids_walker(Node *node, void *context);
static  bool cdb_expr_requires_full_eval(Node *node);
static Plan *cdb_insert_result_node(PlannerInfo *root,
									Plan *plan, 
									int rtoffset);

static bool extract_query_dependencies_walker(Node *node,
								  PlannerInfo *context);
static bool cdb_extract_plan_dependencies_walker(Node *node,
									 cdb_extract_plan_dependencies_context *context);

#ifdef USE_ASSERT_CHECKING
#include "cdb/cdbplan.h"

/**
 * This method establishes asserts on the inputs to set_plan_references.
 */
static void set_plan_references_input_asserts(PlannerGlobal *glob, Plan *plan, List *rtable)
{
	/* Note that rtable MAY be NULL */

	/* Ensure that plan refers to vars that have varlevelsup = 0 AND varno is in the rtable */
	List *allVars = extract_nodes(glob, (Node *) plan, T_Var);
	ListCell *lc = NULL;

	foreach (lc, allVars)
	{
		Var *var = (Var *) lfirst(lc);
		Assert(var->varlevelsup == 0 && "Plan contains vars that refer to outer plan.");
		/**
		 * Append plans set varno = OUTER very early on.
		 */
		/**
		 * If shared input node exists, a subquery scan may refer to varnos outside
		 * its current rtable.
		 */

		/*
         * GPDB_92_MERGE_FIXME: In PG 9.2, there is a new varno 'INDEX_VAR'.
         * GPDB codes should revise to work with the new varno.
         */
		Assert((var->varno == OUTER_VAR || var->varno == INDEX_VAR
				|| (var->varno > 0 && var->varno <= list_length(rtable) + list_length(glob->finalrtable)))
				&& "Plan contains var that refer outside the rtable.");

#if 0
		/* ModifyTable plans have a funny target list, set up just for EXPLAIN. */
		if (!IsA(plan, ModifyTable) && var->varno != var->varnoold)
			Assert(false && "Varno and varnoold do not agree!");
#endif

		/** If a pseudo column, there should be a corresponding entry in the relation */
		if (var->varattno <= FirstLowInvalidHeapAttributeNumber)
		{
			RangeTblEntry *rte = rt_fetch(var->varno, rtable);
			Assert(rte);
			Assert(rte->pseudocols);
			Assert(list_length(rte->pseudocols) > var->varattno - FirstLowInvalidHeapAttributeNumber);
		}

	}
}

/**
 * This method establishes asserts on the output of set_plan_references.
 */
static void set_plan_references_output_asserts(PlannerGlobal *glob, Plan *plan)
{
	/**
	 * Ensure that all OpExprs have regproc OIDs.
	 */
	List *allOpExprs = extract_nodes(glob, (Node *) plan, T_OpExpr);

	ListCell *lc = NULL;

	foreach (lc, allOpExprs)
	{
		OpExpr *opExpr = (OpExpr *) lfirst(lc);
		Assert(opExpr->opfuncid != InvalidOid && "No function associated with OpExpr!");
	}

	/**
	 * All vars should be INNER or OUTER or point to a relation in the glob->finalrtable.
	 */

	List *allVars = extract_nodes(glob, (Node *) plan, T_Var);

	foreach (lc, allVars)
	{
		Var *var = (Var *) lfirst(lc);
		Assert((var->varno == INNER_VAR
				|| var->varno == OUTER_VAR
				|| var->varno == INDEX_VAR
				|| (var->varno > 0 && var->varno <= list_length(glob->finalrtable)))
				&& "Plan contains var that refer outside the rtable.");
		Assert(var->varattno > FirstLowInvalidHeapAttributeNumber && "Invalid attribute number in plan");
	}

	/** All subquery scan nodes should have their scanrelids point to a subquery entry in the finalrtable */
	List *allSubQueryScans = extract_nodes(glob, (Node *) plan, T_SubqueryScan);

	foreach (lc, allSubQueryScans)
	{
		SubqueryScan *subQueryScan = (SubqueryScan *) lfirst(lc);
		Assert(subQueryScan->scan.scanrelid <= list_length(glob->finalrtable) && "Subquery scan's scanrelid out of range");
		RangeTblEntry *rte = rt_fetch(subQueryScan->scan.scanrelid, glob->finalrtable);
		Assert((rte->rtekind == RTE_SUBQUERY || rte->rtekind == RTE_CTE) && "Subquery scan should correspond to a subquery RTE or cte RTE!");
	}
}

/* End of debug code */
#endif

/*****************************************************************************
 *
 *		SUBPLAN REFERENCES
 *
 *****************************************************************************/

/*
 * set_plan_references
 *
 * This is the final processing pass of the planner/optimizer.  The plan
 * tree is complete; we just have to adjust some representational details
 * for the convenience of the executor:
 *
 * 1. We flatten the various subquery rangetables into a single list, and
 * zero out RangeTblEntry fields that are not useful to the executor.
 *
 * 2. We adjust Vars in scan nodes to be consistent with the flat rangetable.
 *
 * 3. We adjust Vars in upper plan nodes to refer to the outputs of their
 * subplans.
 *
 * 4. We compute regproc OIDs for operators (ie, we look up the function
 * that implements each op).
 *
 * 5. We create lists of specific objects that the plan depends on.
 * This will be used by plancache.c to drive invalidation of cached plans.
 * Relation dependencies are represented by OIDs, and everything else by
 * PlanInvalItems (this distinction is motivated by the shared-inval APIs).
 * Currently, relations and user-defined functions are the only types of
 * objects that are explicitly tracked this way.
 *
 * We also perform one final optimization step, which is to delete
 * SubqueryScan plan nodes that aren't doing anything useful (ie, have
 * no qual and a no-op targetlist).  The reason for doing this last is that
 * it can't readily be done before set_plan_references, because it would
 * break set_upper_references: the Vars in the subquery's top tlist
 * wouldn't match up with the Vars in the outer plan tree.  The SubqueryScan
 * serves a necessary function as a buffer between outer query and subquery
 * variable numbering ... but after we've flattened the rangetable this is
 * no longer a problem, since then there's only one rtindex namespace.
 *
 * set_plan_references recursively traverses the whole plan tree.
 *
 * The return value is normally the same Plan node passed in, but can be
 * different when the passed-in Plan is a SubqueryScan we decide isn't needed.
 *
 * The flattened rangetable entries are appended to root->glob->finalrtable.
 * Also, rowmarks entries are appended to root->glob->finalrowmarks, and the
 * RT indexes of ModifyTable result relations to root->glob->resultRelations.
 * Plan dependencies are appended to root->glob->relationOids (for relations)
 * and root->glob->invalItems (for everything else).
 *
 * Notice that we modify Plan nodes in-place, but use expression_tree_mutator
 * to process targetlist and qual expressions.  We can assume that the Plan
 * nodes were just built by the planner and are not multiply referenced, but
 * it's not so safe to assume that for expression tree nodes.
 */
Plan *
set_plan_references(PlannerInfo *root, Plan *plan)
{
	PlannerGlobal *glob = root->glob;
	int			rtoffset = list_length(glob->finalrtable);
	ListCell   *lc;

#ifdef USE_ASSERT_CHECKING
	/* 
	 * This method formalizes our assumptions about the input to set_plan_references.
	 * This will hopefully, help us debug any problems.
	 */
	set_plan_references_input_asserts(glob, plan, root->parse->rtable);
#endif

	/*
	 * Add all the query's RTEs to the flattened rangetable.  The live ones
	 * will have their rangetable indexes increased by rtoffset.  (Additional
	 * RTEs, not referenced by the Plan tree, might get added after those.)
	 */
	add_rtes_to_flat_rtable(root, false);

	/*
	 * Adjust RT indexes of PlanRowMarks and add to final rowmarks list
	 */
	foreach(lc, root->rowMarks)
	{
		PlanRowMark *rc = (PlanRowMark *) lfirst(lc);
		PlanRowMark *newrc;

		Assert(IsA(rc, PlanRowMark));

		/* flat copy is enough since all fields are scalars */
		newrc = (PlanRowMark *) palloc(sizeof(PlanRowMark));
		memcpy(newrc, rc, sizeof(PlanRowMark));

		/* adjust indexes ... but *not* the rowmarkId */
		newrc->rti += rtoffset;
		newrc->prti += rtoffset;

		glob->finalrowmarks = lappend(glob->finalrowmarks, newrc);
	}

	/* Now fix the Plan tree */
	Plan *retPlan = set_plan_refs(root, plan, rtoffset);

#ifdef USE_ASSERT_CHECKING
	/**
	 * Ensuring that the output of setrefs behaves as expected.
	 */
	set_plan_references_output_asserts(glob, retPlan);
#endif

	return retPlan;
}

/*
 * Extract RangeTblEntries from the plan's rangetable, and add to flat rtable
 *
 * This can recurse into subquery plans; "recursing" is true if so.
 */
static void
add_rtes_to_flat_rtable(PlannerInfo *root, bool recursing)
{
	PlannerGlobal *glob = root->glob;
	Index		rti;
	ListCell   *lc;

	/*
	 * Add the query's own RTEs to the flattened rangetable.
	 *
	 * At top level, we must add all RTEs so that their indexes in the
	 * flattened rangetable match up with their original indexes.  When
	 * recursing, we only care about extracting relation RTEs.
	 */
	foreach(lc, root->parse->rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(lc);

		if (!recursing || rte->rtekind == RTE_RELATION)
			add_rte_to_flat_rtable(glob, rte);
	}

	/*
	 * If there are any dead subqueries, they are not referenced in the Plan
	 * tree, so we must add RTEs contained in them to the flattened rtable
	 * separately.  (If we failed to do this, the executor would not perform
	 * expected permission checks for tables mentioned in such subqueries.)
	 *
	 * Note: this pass over the rangetable can't be combined with the previous
	 * one, because that would mess up the numbering of the live RTEs in the
	 * flattened rangetable.
	 */
	rti = 1;
	foreach(lc, root->parse->rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(lc);

		/*
		 * We should ignore inheritance-parent RTEs: their contents have been
		 * pulled up into our rangetable already.  Also ignore any subquery
		 * RTEs without matching RelOptInfos, as they likewise have been
		 * pulled up.
		 */
		if (rte->rtekind == RTE_SUBQUERY && !rte->inh &&
			rti < root->simple_rel_array_size)
		{
			RelOptInfo *rel = root->simple_rel_array[rti];

			if (rel != NULL)
			{
				Assert(rel->relid == rti);		/* sanity check on array */

				/*
				 * The subquery might never have been planned at all, if it
				 * was excluded on the basis of self-contradictory constraints
				 * in our query level.  In this case apply
				 * flatten_unplanned_rtes.
				 *
				 * If it was planned but the plan is dummy, we assume that it
				 * has been omitted from our plan tree (see
				 * set_subquery_pathlist), and recurse to pull up its RTEs.
				 *
				 * Otherwise, it should be represented by a SubqueryScan node
				 * somewhere in our plan tree, and we'll pull up its RTEs when
				 * we process that plan node.
				 *
				 * However, if we're recursing, then we should pull up RTEs
				 * whether the subplan is dummy or not, because we've found
				 * that some upper query level is treating this one as dummy,
				 * and so we won't scan this level's plan tree at all.
				 */
				if (rel->subplan == NULL)
					flatten_unplanned_rtes(glob, rte);
				else if (recursing || is_dummy_plan(rel->subplan))
				{
					Assert(rel->subroot != NULL);
					add_rtes_to_flat_rtable(rel->subroot, true);
				}
			}
		}
		rti++;
	}
}

/*
 * Extract RangeTblEntries from a subquery that was never planned at all
 */
static void
flatten_unplanned_rtes(PlannerGlobal *glob, RangeTblEntry *rte)
{
	/* Use query_tree_walker to find all RTEs in the parse tree */
	(void) query_tree_walker(rte->subquery,
							 flatten_rtes_walker,
							 (void *) glob,
							 QTW_EXAMINE_RTES);
}

static bool
flatten_rtes_walker(Node *node, PlannerGlobal *glob)
{
	if (node == NULL)
		return false;
	if (IsA(node, RangeTblEntry))
	{
		RangeTblEntry *rte = (RangeTblEntry *) node;

		/* As above, we need only save relation RTEs */
		if (rte->rtekind == RTE_RELATION)
			add_rte_to_flat_rtable(glob, rte);
		return false;
	}
	if (IsA(node, Query))
	{
		/* Recurse into subselects */
		return query_tree_walker((Query *) node,
								 flatten_rtes_walker,
								 (void *) glob,
								 QTW_EXAMINE_RTES);
	}
	return expression_tree_walker(node, flatten_rtes_walker,
								  (void *) glob);
}

/*
 * Add (a copy of) the given RTE to the final rangetable
 *
 * In the flat rangetable, we zero out substructure pointers that are not
 * needed by the executor; this reduces the storage space and copying cost
 * for cached plans.  We keep only the ctename, alias and eref Alias fields,
 * which are needed by EXPLAIN, and the selectedCols and modifiedCols bitmaps,
 * which are needed for executor-startup permissions checking and for trigger
 * event checking.
 */
static void
add_rte_to_flat_rtable(PlannerGlobal *glob, RangeTblEntry *rte)
{
	RangeTblEntry *newrte;

	/* flat copy to duplicate all the scalar fields */
	newrte = (RangeTblEntry *) palloc(sizeof(RangeTblEntry));
	memcpy(newrte, rte, sizeof(RangeTblEntry));

	/* zap unneeded sub-structure */
	newrte->subquery = NULL;
	newrte->joinaliasvars = NIL;
	newrte->functions = NIL;
	newrte->values_lists = NIL;
	newrte->values_collations = NIL;
	newrte->ctecoltypes = NIL;
	newrte->ctecoltypmods = NIL;
	newrte->ctecolcollations = NIL;
	newrte->securityQuals = NIL;

	glob->finalrtable = lappend(glob->finalrtable, newrte);

	/*
	 * Check for RT index overflow; it's very unlikely, but if it did happen,
	 * the executor would get confused by varnos that match the special varno
	 * values.
	 */
	if (IS_SPECIAL_VARNO(list_length(glob->finalrtable)))
		ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("too many range table entries")));

	/*
	 * If it's a plain relation RTE, add the table to relationOids.
	 *
	 * We do this even though the RTE might be unreferenced in the plan tree;
	 * this would correspond to cases such as views that were expanded, child
	 * tables that were eliminated by constraint exclusion, etc. Schema
	 * invalidation on such a rel must still force rebuilding of the plan.
	 *
	 * Note we don't bother to avoid making duplicate list entries.  We could,
	 * but it would probably cost more cycles than it would save.
	 */
	if (newrte->rtekind == RTE_RELATION)
		glob->relationOids = lappend_oid(glob->relationOids, newrte->relid);
}

/*
 * set_plan_refs: recurse through the Plan nodes of a single subquery level
 */
static Plan *
set_plan_refs(PlannerInfo *root, Plan *plan, int rtoffset)
{
	ListCell   *l;

	if (plan == NULL)
		return NULL;

    /*
     * CDB: If plan has a Flow node, fix up its hashExpr to refer to the
     * plan's own targetlist.
     */
	if (plan->flow && plan->flow->hashExprs)
    {
        indexed_tlist  *plan_itlist = build_tlist_index(plan->targetlist);

		plan->flow->hashExprs =
			(List *) fix_upper_flow_expr(root,
									(Node *) plan->flow->hashExprs,
									plan_itlist,
									OUTER_VAR,
									rtoffset);
        pfree(plan_itlist);
    }

	/*
	 * Plan-type-specific fixes
	 */
	switch (nodeTag(plan))
	{
		case T_SeqScan: /* Rely on structure equivalence */
		case T_ExternalScan: /* Rely on structure equivalence */
			{
				Scan    *splan = (Scan *) plan;

				if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
					return cdb_insert_result_node(root, plan, rtoffset);

				splan->scanrelid += rtoffset;

				/* If the scan appears below a shareinput, we hit this assert. */
#ifdef USE_ASSERT_CHECKING
				Assert(splan->scanrelid <= list_length(root->glob->finalrtable) && "Scan node's relid is outside the finalrtable!");
				RangeTblEntry *rte = rt_fetch(splan->scanrelid, root->glob->finalrtable);
				Assert((rte->rtekind == RTE_RELATION || rte->rtekind == RTE_CTE) && "Scan plan should refer to a scan relation");
#endif

				splan->plan.targetlist =
					fix_scan_list(root, splan->plan.targetlist, rtoffset);
				splan->plan.qual =
					fix_scan_list(root, splan->plan.qual, rtoffset);
			}
			break;
		case T_IndexScan:
			{
				IndexScan  *splan = (IndexScan *) plan;

				if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
					return cdb_insert_result_node(root, plan, rtoffset);

				splan->scan.scanrelid += rtoffset;

#ifdef USE_ASSERT_CHECKING
				RangeTblEntry *rte = rt_fetch(splan->scan.scanrelid, root->glob->finalrtable);
				char relstorage = get_rel_relstorage(rte->relid);
				Assert(relstorage != RELSTORAGE_AOROWS &&
					   relstorage != RELSTORAGE_AOCOLS);
#endif

				splan->scan.plan.targetlist =
					fix_scan_list(root, splan->scan.plan.targetlist, rtoffset);
				splan->scan.plan.qual =
					fix_scan_list(root, splan->scan.plan.qual, rtoffset);
				splan->indexqual =
					fix_scan_list(root, splan->indexqual, rtoffset);
				splan->indexqualorig =
					fix_scan_list(root, splan->indexqualorig, rtoffset);
				splan->indexorderby =
					fix_scan_list(root, splan->indexorderby, rtoffset);
				splan->indexorderbyorig =
					fix_scan_list(root, splan->indexorderbyorig, rtoffset);
			}
			break;
		case T_IndexOnlyScan:
			{
				IndexOnlyScan *splan = (IndexOnlyScan *) plan;

				/* The node doesn't support for evaluating set-returning-functions in targetlists,
				 * they should be moved onto a new Result node which will be inserted above the given node.
				 * issue:https://github.com/greenplum-db/gpdb/issues/11307
				 */
				if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
					return cdb_insert_result_node(root, plan, rtoffset);

				return set_indexonlyscan_references(root, splan, rtoffset);
			}
			break;
		case T_BitmapIndexScan:
			{
				BitmapIndexScan *splan = (BitmapIndexScan *) plan;

				splan->scan.scanrelid += rtoffset;
				/* no need to fix targetlist and qual */
				Assert(splan->scan.plan.targetlist == NIL);
				Assert(splan->scan.plan.qual == NIL);
				splan->indexqual =
					fix_scan_list(root, splan->indexqual, rtoffset);
				splan->indexqualorig =
					fix_scan_list(root, splan->indexqualorig, rtoffset);
			}
			break;
		case T_BitmapHeapScan:
			{
				BitmapHeapScan *splan = (BitmapHeapScan *) plan;

				if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
					return cdb_insert_result_node(root, plan, rtoffset);

				splan->scan.scanrelid += rtoffset;

				splan->scan.plan.targetlist =
					fix_scan_list(root, splan->scan.plan.targetlist, rtoffset);
				splan->scan.plan.qual =
					fix_scan_list(root, splan->scan.plan.qual, rtoffset);
				splan->bitmapqualorig =
					fix_scan_list(root, splan->bitmapqualorig, rtoffset);
			}
			break;
		case T_TidScan:
			{
				TidScan    *splan = (TidScan *) plan;

				if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
					return cdb_insert_result_node(root, plan, rtoffset);

				splan->scan.scanrelid += rtoffset;

#ifdef USE_ASSERT_CHECKING
				/* We only support TidScans on heap tables currently */
				RangeTblEntry *rte = rt_fetch(splan->scan.scanrelid, root->glob->finalrtable);
				char relstorage = get_rel_relstorage(rte->relid);
				Assert(relstorage == RELSTORAGE_HEAP);
#endif

				splan->scan.plan.targetlist =
					fix_scan_list(root, splan->scan.plan.targetlist, rtoffset);
				splan->scan.plan.qual =
					fix_scan_list(root, splan->scan.plan.qual, rtoffset);
				splan->tidquals =
					fix_scan_list(root, splan->tidquals, rtoffset);
			}
			break;
		case T_SubqueryScan:

			if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
				return cdb_insert_result_node(root, plan, rtoffset);

			/* Needs special treatment, see comments below */
			return set_subqueryscan_references(root,
											   (SubqueryScan *) plan,
											   rtoffset);
		case T_TableFunctionScan:
			{
				TableFunctionScan *tplan	   = (TableFunctionScan *) plan;
				Plan	   *subplan   = tplan->scan.plan.lefttree;
				RelOptInfo *rel;


				if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
					return cdb_insert_result_node(root, plan, rtoffset);

				/* Need to look up the subquery's RelOptInfo, since we need its subroot */
				rel = find_base_rel(root, tplan->scan.scanrelid);
				Assert(rel->subplan == subplan);

				/* recursively process the subplan */
				plan->lefttree = set_plan_references(rel->subroot, subplan);

				/* adjust for the new range table offset */
				tplan->scan.scanrelid += rtoffset;
				tplan->scan.plan.targetlist =
					fix_scan_list(root, tplan->scan.plan.targetlist, rtoffset);
				tplan->scan.plan.qual =
					fix_scan_list(root, tplan->scan.plan.qual, rtoffset);
				tplan->function = (RangeTblFunction *)
					fix_scan_expr(root, (Node *) tplan->function, rtoffset);

				return plan;
			}
		case T_FunctionScan:
			{
				FunctionScan *splan = (FunctionScan *) plan;

				if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
					return cdb_insert_result_node(root, plan, rtoffset);

				splan->scan.scanrelid += rtoffset;
				splan->scan.plan.targetlist =
					fix_scan_list(root, splan->scan.plan.targetlist, rtoffset);
				splan->scan.plan.qual =
					fix_scan_list(root, splan->scan.plan.qual, rtoffset);
				splan->functions =
					fix_scan_list(root, splan->functions, rtoffset);
			}
			break;
		case T_ValuesScan:
			{
				ValuesScan *splan = (ValuesScan *) plan;

				if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
					return cdb_insert_result_node(root, plan, rtoffset);

				splan->scan.scanrelid += rtoffset;
				splan->scan.plan.targetlist =
					fix_scan_list(root, splan->scan.plan.targetlist, rtoffset);
				splan->scan.plan.qual =
					fix_scan_list(root, splan->scan.plan.qual, rtoffset);
				splan->values_lists =
					fix_scan_list(root, splan->values_lists, rtoffset);
			}
			break;
		case T_CteScan:
			{
				CteScan    *splan = (CteScan *) plan;

				splan->scan.scanrelid += rtoffset;
				splan->scan.plan.targetlist =
					fix_scan_list(root, splan->scan.plan.targetlist, rtoffset);
				splan->scan.plan.qual =
					fix_scan_list(root, splan->scan.plan.qual, rtoffset);
			}
			break;
		case T_WorkTableScan:
			{
				WorkTableScan *splan = (WorkTableScan *) plan;

				splan->scan.scanrelid += rtoffset;
				splan->scan.plan.targetlist =
					fix_scan_list(root, splan->scan.plan.targetlist, rtoffset);
				splan->scan.plan.qual =
					fix_scan_list(root, splan->scan.plan.qual, rtoffset);
			}
			break;
		case T_ForeignScan:
			{
				ForeignScan *splan = (ForeignScan *) plan;

				splan->scan.scanrelid += rtoffset;
				splan->scan.plan.targetlist =
					fix_scan_list(root, splan->scan.plan.targetlist, rtoffset);
				splan->scan.plan.qual =
					fix_scan_list(root, splan->scan.plan.qual, rtoffset);
				splan->fdw_exprs =
					fix_scan_list(root, splan->fdw_exprs, rtoffset);
			}
			break;

		case T_NestLoop:
		case T_MergeJoin:
		case T_HashJoin:
			if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
				return cdb_insert_result_node(root, plan, rtoffset);
			set_join_references(root, (Join *) plan, rtoffset);
			break;
		case T_Plan:
			/*
			 * Occurs only as a temporary fake outer subplan (created just
			 * above) for Adaptive NJ's HJ child.  This allows the HJ's outer
			 * subplan references to be fixed up normally while avoiding double
			 * fixup of the real outer subplan.  By the time we arrive here,
			 * this node has served its purpose and is no longer needed.
			 * Vanish, returning a null ptr to replace the temporary fake ptr.
			 *
			 * XXX is this still needed.  It it right??? bch 2010-02-07
			 */
			Assert(!plan->lefttree && !plan->righttree && !plan->initPlan);
			break;

		case T_Hash:
		case T_Material:
		case T_Sort:
		case T_Unique:
		case T_SetOp:

			/*
			 * These plan types don't actually bother to evaluate their
			 * targetlists, because they just return their unmodified input
			 * tuples.  Even though the targetlist won't be used by the
			 * executor, we fix it up for possible use by EXPLAIN (not to
			 * mention ease of debugging --- wrong varnos are very confusing).
			 */
			set_dummy_tlist_references(plan, rtoffset);

			/*
			 * Since these plan types don't check quals either, we should not
			 * find any qual expression attached to them.
			 */
			Assert(plan->qual == NIL);
			break;

		case T_ShareInputScan:
			{
				ShareInputScan *sisc = (ShareInputScan *) plan;
				Plan *childPlan = plan->lefttree;

				if (childPlan == NULL)
				{
					ShareInputScan *producer;

					Assert(sisc->share_type != SHARE_NOTSHARED);
					Assert(sisc->share_id >= 0 && sisc->share_id < root->glob->share.producer_count);
					producer = root->glob->share.producers[sisc->share_id];
					childPlan = producer->scan.plan.lefttree;
				}

#ifdef DEBUG
				Assert(childPlan && IsA(childPlan,Material) || IsA(childPlan, Sort));
				if (IsA(childPlan, Material))
				{
					Material *shared = (Material *) childPlan;
					Assert(shared->share_type != SHARE_NOTSHARED
						   && shared->share_id == sisc->share_id);
				}
				else
				{
					Sort *shared = (Sort *) childPlan;
					Assert(shared->share_type != SHARE_NOTSHARED
						   && shared->share_id == sisc->share_id);
				}
#endif
				set_dummy_tlist_references(plan, rtoffset);
			}
			break;

		case T_PartitionSelector:
			{
				PartitionSelector *ps = (PartitionSelector *) plan;
				indexed_tlist *childplan_itlist =
					build_tlist_index(plan->lefttree->targetlist);

				set_upper_references(root, plan, rtoffset);

				//set_dummy_tlist_references(plan, rtoffset);
				Assert(ps->plan.qual == NIL);

				ps->levelEqExpressions = (List *)
					fix_upper_expr(root, (Node *) ps->levelEqExpressions, childplan_itlist, OUTER_VAR,rtoffset);
				ps->levelExpressions = (List *)
					fix_upper_expr(root, (Node *) ps->levelExpressions, childplan_itlist, OUTER_VAR,rtoffset);
				ps->residualPredicate =
					fix_upper_expr(root, ps->residualPredicate, childplan_itlist, OUTER_VAR,rtoffset);
				ps->propagationExpression =
					fix_upper_expr(root, ps->propagationExpression, childplan_itlist, OUTER_VAR,rtoffset);
				ps->printablePredicate =
					fix_upper_expr(root, ps->printablePredicate, childplan_itlist, OUTER_VAR,rtoffset);
				ps->partTabTargetlist = (List *)
					fix_upper_expr(root, (Node *) ps->partTabTargetlist, childplan_itlist, OUTER_VAR,rtoffset);
			}
			break;
			
		case T_LockRows:
			{
				LockRows   *splan = (LockRows *) plan;

				/*
				 * Like the plan types above, LockRows doesn't evaluate its
				 * tlist or quals.  But we have to fix up the RT indexes in
				 * its rowmarks.
				 */
				set_dummy_tlist_references(plan, rtoffset);
				Assert(splan->plan.qual == NIL);

				foreach(l, splan->rowMarks)
				{
					PlanRowMark *rc = (PlanRowMark *) lfirst(l);

					rc->rti += rtoffset;
					rc->prti += rtoffset;
				}
			}
			break;
		case T_Limit:
			{
				Limit	   *splan = (Limit *) plan;

				/*
				 * Like the plan types above, Limit doesn't evaluate its tlist
				 * or quals.  It does have live expressions for limit/offset,
				 * however; and those cannot contain subplan variable refs, so
				 * fix_scan_expr works for them.
				 */
				set_dummy_tlist_references(plan, rtoffset);
				Assert(splan->plan.qual == NIL);

				splan->limitOffset =
					fix_scan_expr(root, splan->limitOffset, rtoffset);
				splan->limitCount =
					fix_scan_expr(root, splan->limitCount, rtoffset);
			}
			break;
		case T_Agg:
			set_upper_references(root, plan, rtoffset);
			break;
		case T_WindowAgg:
			{
				WindowAgg  *wplan = (WindowAgg *) plan;
				indexed_tlist  *subplan_itlist;

				set_upper_references(root, plan, rtoffset);

				if ( plan->targetlist == NIL )
					set_dummy_tlist_references(plan, rtoffset);

				/*
				 * Fix frame edges. PostgreSQL uses fix_scan_expr here, but
				 * in GPDB, we allow the ROWS/RANGE expressions to contain
				 * references to the subplan, so we have to use fix_upper_expr.
				 */
				if (wplan->startOffset || wplan->endOffset)
				{
					subplan_itlist =
						build_tlist_index(plan->lefttree->targetlist);

					wplan->startOffset =
						fix_upper_expr(root, wplan->startOffset,
									   subplan_itlist, OUTER_VAR, rtoffset);
					wplan->endOffset =
						fix_upper_expr(root, wplan->endOffset,
									   subplan_itlist, OUTER_VAR, rtoffset);
					pfree(subplan_itlist);
				}
			}
			break;
		case T_Result:
			{
				Result	   *splan = (Result *) plan;

				/*
				 * Result may or may not have a subplan; if not, it's more
				 * like a scan node than an upper node.
				 */
				if (splan->plan.lefttree != NULL)
					set_upper_references(root, plan, rtoffset);
				else
				{
					splan->plan.targetlist =
						fix_scan_list(root, splan->plan.targetlist, rtoffset);
					splan->plan.qual =
						fix_scan_list(root, splan->plan.qual, rtoffset);
				}
				/* resconstantqual can't contain any subplan variable refs */
				splan->resconstantqual =
					fix_scan_expr(root, splan->resconstantqual, rtoffset);
			}
			break;
		case T_Repeat:
			set_upper_references(root, plan, rtoffset);
			break;
		case T_ModifyTable:
			{
				ModifyTable *splan = (ModifyTable *) plan;

				Assert(splan->plan.targetlist == NIL);
				Assert(splan->plan.qual == NIL);

				splan->withCheckOptionLists =
					fix_scan_list(root, splan->withCheckOptionLists, rtoffset);

				if (splan->returningLists)
				{
					List	   *newRL = NIL;
					ListCell   *lcrl,
							   *lcrr,
							   *lcp;

					/*
					 * Pass each per-subplan returningList through
					 * set_returning_clause_references().
					 */
					Assert(list_length(splan->returningLists) == list_length(splan->resultRelations));
					Assert(list_length(splan->returningLists) == list_length(splan->plans));
					forthree(lcrl, splan->returningLists,
							 lcrr, splan->resultRelations,
							 lcp, splan->plans)
					{
						List	   *rlist = (List *) lfirst(lcrl);
						Index		resultrel = lfirst_int(lcrr);
						Plan	   *subplan = (Plan *) lfirst(lcp);

						rlist = set_returning_clause_references(root,
																rlist,
																subplan,
																resultrel,
																rtoffset);
						newRL = lappend(newRL, rlist);
					}
					splan->returningLists = newRL;

					/*
					 * Set up the visible plan targetlist as being the same as
					 * the first RETURNING list. This is for the use of
					 * EXPLAIN; the executor won't pay any attention to the
					 * targetlist.  We postpone this step until here so that
					 * we don't have to do set_returning_clause_references()
					 * twice on identical targetlists.
					 */
					splan->plan.targetlist = copyObject(linitial(newRL));
				}

				foreach(l, splan->resultRelations)
				{
					lfirst_int(l) += rtoffset;
				}
				foreach(l, splan->rowMarks)
				{
					PlanRowMark *rc = (PlanRowMark *) lfirst(l);

					rc->rti += rtoffset;
					rc->prti += rtoffset;
				}
				foreach(l, splan->plans)
				{
					lfirst(l) = set_plan_refs(root,
											  (Plan *) lfirst(l),
											  rtoffset);
				}

				/*
				 * Append this ModifyTable node's final result relation RT
				 * index(es) to the global list for the plan, and set its
				 * resultRelIndex to reflect their starting position in the
				 * global list.
				 */
				splan->resultRelIndex = list_length(root->glob->resultRelations);
				root->glob->resultRelations =
					list_concat(root->glob->resultRelations,
								list_copy(splan->resultRelations));
			}
			break;
		case T_Append:
			{
				Append	   *splan = (Append *) plan;

				/*
				 * Append, like Sort et al, doesn't actually evaluate its
				 * targetlist or check quals.
				 */
				set_dummy_tlist_references(plan, rtoffset);
				Assert(splan->plan.qual == NIL);
				foreach(l, splan->appendplans)
				{
					lfirst(l) = set_plan_refs(root,
											  (Plan *) lfirst(l),
											  rtoffset);
				}
			}
			break;
		case T_MergeAppend:
			{
				MergeAppend *splan = (MergeAppend *) plan;

				/*
				 * MergeAppend, like Sort et al, doesn't actually evaluate its
				 * targetlist or check quals.
				 */
				set_dummy_tlist_references(plan, rtoffset);
				Assert(splan->plan.qual == NIL);
				foreach(l, splan->mergeplans)
				{
					lfirst(l) = set_plan_refs(root,
											  (Plan *) lfirst(l),
											  rtoffset);
				}
			}
			break;
		case T_RecursiveUnion:
			/* This doesn't evaluate targetlist or check quals either */
			set_dummy_tlist_references(plan, rtoffset);
			Assert(plan->qual == NIL);
			break;
		case T_BitmapAnd:
			{
				BitmapAnd  *splan = (BitmapAnd *) plan;

				/* BitmapAnd works like Append, but has no tlist */
				Assert(splan->plan.targetlist == NIL);
				Assert(splan->plan.qual == NIL);
				foreach(l, splan->bitmapplans)
				{
					lfirst(l) = set_plan_refs(root,
											  (Plan *) lfirst(l),
											  rtoffset);
				}
			}
			break;
		case T_BitmapOr:
			{
				BitmapOr   *splan = (BitmapOr *) plan;

				/* BitmapOr works like Append, but has no tlist */
				Assert(splan->plan.targetlist == NIL);
				Assert(splan->plan.qual == NIL);
				foreach(l, splan->bitmapplans)
				{
					lfirst(l) = set_plan_refs(root,
											  (Plan *) lfirst(l),
											  rtoffset);
				}
			}
			break;
		case T_Motion:
			{
				Motion	   *motion = (Motion *) plan;
				/* test flag to prevent processing the node multi times */
				indexed_tlist *childplan_itlist =
					build_tlist_index(plan->lefttree->targetlist);

				motion->hashExprs = (List *)
					fix_upper_expr(root, (Node*) motion->hashExprs, childplan_itlist,  OUTER_VAR, rtoffset);

#ifdef USE_ASSERT_CHECKING
				/* 1. Assert that the Motion node has same number of hash data types as that of hash expressions*/
				/* 2. Motion node must have atleast one hash expression */
				/* 3. If the Motion node is of type hash_motion: ensure that the expression that it is hashed on is a hashable datatype in gpdb*/

				if (MOTIONTYPE_HASH == motion->motionType)
				{
					Assert(1 <= list_length(motion->hashExprs) && "Motion node must have atleast one hash expression!");
				}

#endif			/* USE_ASSERT_CHECKING */

				/* no need to fix targetlist and qual */
				Assert(plan->qual == NIL);
				set_dummy_tlist_references(plan, rtoffset);
				pfree(childplan_itlist);
			}
			break;
		case T_SplitUpdate:
			{
				Assert(plan->qual == NIL);
				set_splitupdate_tlist_references(plan, rtoffset);
			}
			break;
		default:
			elog(ERROR, "unrecognized node type: %d",
				 (int) nodeTag(plan));
			break;
	}

	/*
	 * Now recurse into child plans, if any
	 *
	 * NOTE: it is essential that we recurse into child plans AFTER we set
	 * subplan references in this plan's tlist and quals.  If we did the
	 * reference-adjustments bottom-up, then we would fail to match this
	 * plan's var nodes against the already-modified nodes of the children.
	 */
	plan->lefttree = set_plan_refs(root, plan->lefttree, rtoffset);
	plan->righttree = set_plan_refs(root, plan->righttree, rtoffset);

	return plan;
}

/*
 * set_indexonlyscan_references
 *		Do set_plan_references processing on an IndexOnlyScan
 *
 * This is unlike the handling of a plain IndexScan because we have to
 * convert Vars referencing the heap into Vars referencing the index.
 * We can use the fix_upper_expr machinery for that, by working from a
 * targetlist describing the index columns.
 */
static Plan *
set_indexonlyscan_references(PlannerInfo *root,
							 IndexOnlyScan *plan,
							 int rtoffset)
{
	indexed_tlist *index_itlist;

	index_itlist = build_tlist_index(plan->indextlist);

	plan->scan.scanrelid += rtoffset;
	plan->scan.plan.targetlist = (List *)
		fix_upper_expr(root,
					   (Node *) plan->scan.plan.targetlist,
					   index_itlist,
					   INDEX_VAR,
					   rtoffset);
	plan->scan.plan.qual = (List *)
		fix_upper_expr(root,
					   (Node *) plan->scan.plan.qual,
					   index_itlist,
					   INDEX_VAR,
					   rtoffset);
	/* indexqual is already transformed to reference index columns */
	plan->indexqual = fix_scan_list(root, plan->indexqual, rtoffset);
	/* indexqualorig is already transformed to reference index columns */
	plan->indexqualorig = fix_scan_list(root, plan->indexqualorig, rtoffset);
	/* indexorderby is already transformed to reference index columns */
	plan->indexorderby = fix_scan_list(root, plan->indexorderby, rtoffset);
	/* indextlist must NOT be transformed to reference index columns */
	plan->indextlist = fix_scan_list(root, plan->indextlist, rtoffset);

	pfree(index_itlist);

	return (Plan *) plan;
}

/*
 * set_subqueryscan_references
 *		Do set_plan_references processing on a SubqueryScan
 *
 * We try to strip out the SubqueryScan entirely; if we can't, we have
 * to do the normal processing on it.
 */
static Plan *
set_subqueryscan_references(PlannerInfo *root,
							SubqueryScan *plan,
							int rtoffset)
{
	RelOptInfo *rel;
	Plan	   *result;

	/* Need to look up the subquery's RelOptInfo, since we need its subroot */
	rel = find_base_rel(root, plan->scan.scanrelid);
	/*
	 * The Assert() on RelOptInfo's subplan being same as the
	 * subqueryscan's subplan, is valid in Upstream but not for GPDB,
	 * since we create a new copy of the subplan if two SubPlans refer to
	 * the same initplan inside adjust_appendrel_attrs_mutator().
	 */

	/* Recursively process the subplan */
	plan->subplan = set_plan_references(rel->subroot, plan->subplan);

	if (trivial_subqueryscan(plan))
	{
		/*
		 * We can omit the SubqueryScan node and just pull up the subplan.
		 */
		ListCell   *lp,
				   *lc;

		result = plan->subplan;

		/* We have to be sure we don't lose any initplans */
		result->initPlan = list_concat(plan->scan.plan.initPlan,
									   result->initPlan);

		/*
		 * We also have to transfer the SubqueryScan's result-column names
		 * into the subplan, else columns sent to client will be improperly
		 * labeled if this is the topmost plan level.  Copy the "source
		 * column" information too.
		 */
		forboth(lp, plan->scan.plan.targetlist, lc, result->targetlist)
		{
			TargetEntry *ptle = (TargetEntry *) lfirst(lp);
			TargetEntry *ctle = (TargetEntry *) lfirst(lc);

			ctle->resname = ptle->resname;
			ctle->resorigtbl = ptle->resorigtbl;
			ctle->resorigcol = ptle->resorigcol;
		}

		/* Honor the flow of the SubqueryScan, by copying it to the subplan. */
		result->flow = plan->scan.plan.flow;
	}
	else
	{
		/*
		 * Keep the SubqueryScan node.  We have to do the processing that
		 * set_plan_references would otherwise have done on it.  Notice we do
		 * not do set_upper_references() here, because a SubqueryScan will
		 * always have been created with correct references to its subplan's
		 * outputs to begin with.
		 */
		plan->scan.scanrelid += rtoffset;

		//Assert(plan->scan.scanrelid <= list_length(glob->finalrtable) && "Scan node's relid is outside the finalrtable!");

		plan->scan.plan.targetlist =
			fix_scan_list(root, plan->scan.plan.targetlist, rtoffset);
		plan->scan.plan.qual =
			fix_scan_list(root, plan->scan.plan.qual, rtoffset);

		result = (Plan *) plan;
	}

	return result;
}

/*
 * trivial_subqueryscan
 *		Detect whether a SubqueryScan can be deleted from the plan tree.
 *
 * We can delete it if it has no qual to check and the targetlist just
 * regurgitates the output of the child plan.
 */
static bool
trivial_subqueryscan(SubqueryScan *plan)
{
	int			attrno;
	ListCell   *lp,
			   *lc;

	if (plan->scan.plan.qual != NIL)
		return false;

	if (list_length(plan->scan.plan.targetlist) !=
		list_length(plan->subplan->targetlist))
		return false;			/* tlists not same length */

	attrno = 1;
	forboth(lp, plan->scan.plan.targetlist, lc, plan->subplan->targetlist)
	{
		TargetEntry *ptle = (TargetEntry *) lfirst(lp);
		TargetEntry *ctle = (TargetEntry *) lfirst(lc);

		if (ptle->resjunk != ctle->resjunk)
			return false;		/* tlist doesn't match junk status */

		/*
		 * We accept either a Var referencing the corresponding element of the
		 * subplan tlist, or a Const equaling the subplan element. See
		 * generate_setop_tlist() for motivation.
		 */
		if (ptle->expr && IsA(ptle->expr, Var))
		{
			Var		   *var = (Var *) ptle->expr;

			Assert(var->varlevelsup == 0);
			if (var->varattno != attrno)
				return false;	/* out of order */
		}
		else if (ptle->expr && IsA(ptle->expr, Const))
		{
			if (!equal(ptle->expr, ctle->expr))
				return false;
		}
		else
			return false;

		attrno++;
	}

	return true;
}

/*
 * copyVar
 *		Copy a Var node.
 *
 * fix_scan_expr and friends do this enough times that it's worth having
 * a bespoke routine instead of using the generic copyObject() function.
 */
static inline Var *
copyVar(Var *var)
{
	Var		   *newvar = (Var *) palloc(sizeof(Var));

	*newvar = *var;
	return newvar;
}

/*
 * fix_expr_common
 *		Do generic set_plan_references processing on an expression node
 *
 * This is code that is common to all variants of expression-fixing.
 * We must look up operator opcode info for OpExpr and related nodes,
 * add OIDs from regclass Const nodes into root->glob->relationOids, and
 * add PlanInvalItems for user-defined functions into root->glob->invalItems.
 *
 * We assume it's okay to update opcode info in-place.  So this could possibly
 * scribble on the planner's input data structures, but it's OK.
 */
static void
fix_expr_common(PlannerInfo *root, Node *node)
{
	/* We assume callers won't call us on a NULL pointer */
	if (IsA(node, Aggref))
	{
		record_plan_function_dependency(root,
										((Aggref *) node)->aggfnoid);
	}
	else if (IsA(node, WindowFunc))
	{
		record_plan_function_dependency(root,
										((WindowFunc *) node)->winfnoid);
	}
	else if (IsA(node, FuncExpr))
	{
		record_plan_function_dependency(root,
										((FuncExpr *) node)->funcid);
	}
	else if (IsA(node, OpExpr))
	{
		set_opfuncid((OpExpr *) node);
		record_plan_function_dependency(root,
										((OpExpr *) node)->opfuncid);
	}
	else if (IsA(node, DistinctExpr))
	{
		set_opfuncid((OpExpr *) node);	/* rely on struct equivalence */
		record_plan_function_dependency(root,
										((DistinctExpr *) node)->opfuncid);
	}
	else if (IsA(node, NullIfExpr))
	{
		set_opfuncid((OpExpr *) node);	/* rely on struct equivalence */
		record_plan_function_dependency(root,
										((NullIfExpr *) node)->opfuncid);
	}
	else if (IsA(node, ScalarArrayOpExpr))
	{
		set_sa_opfuncid((ScalarArrayOpExpr *) node);
		record_plan_function_dependency(root,
									 ((ScalarArrayOpExpr *) node)->opfuncid);
	}
	else if (IsA(node, ArrayCoerceExpr))
	{
		if (OidIsValid(((ArrayCoerceExpr *) node)->elemfuncid))
			record_plan_function_dependency(root,
									 ((ArrayCoerceExpr *) node)->elemfuncid);
	}
	else if (IsA(node, Const))
	{
		Const	   *con = (Const *) node;

		/* Check for regclass reference */
		if (ISREGCLASSCONST(con))
			root->glob->relationOids =
				lappend_oid(root->glob->relationOids,
							DatumGetObjectId(con->constvalue));
	}
    else if (IsA(node, Var))
    {
        Var    *var = (Var *)node;

        /*
         * CDB: If Var node refers to a pseudo column, note its varno.
         * By this point, no such Var nodes should be seen except for
         * local references in Scan or Append exprs.
         *
         * XXX callers must reinitialize this appropriately.  Ought
         *     to find a better way.
         */
        if (var->varattno <= FirstLowInvalidHeapAttributeNumber)
        {
            Assert(var->varlevelsup == 0 &&
                   var->varno > 0 &&
                   var->varno <= list_length(root->glob->finalrtable));
        }
    }
}

/*
 * fix_scan_expr
 *		Do set_plan_references processing on a scan-level expression
 *
 * This consists of incrementing all Vars' varnos by rtoffset,
 * looking up operator opcode info for OpExpr and related nodes,
 * and adding OIDs from regclass Const nodes into root->glob->relationOids.
 */
static Node *
fix_scan_expr(PlannerInfo *root, Node *node, int rtoffset)
{
	fix_scan_expr_context context;

	context.root = root;
	context.rtoffset = rtoffset;

	/*
	 * Postgres has an optimization to mutate the expression tree only if
	 * rtoffset is non-zero. However, this optimization does not work for
	 * GPDB planner. The planner in GPDB produces plans where rtoffset
	 * may be zero, but it uses gp_subplan_id as a pseudo column
	 * to deduplicate all the partition scans. This pseudo var needs
	 * to be unnested (i.e., the underlying expr needs to replace the Var)
	 * using mutation. Therefore, in GPDB we need to unconditionally mutate the tree.
	 */
	return fix_scan_expr_mutator(node, &context);
}

static Node *
fix_scan_expr_mutator(Node *node, fix_scan_expr_context *context)
{
	if (node == NULL)
		return NULL;
	if (IsA(node, Var))
	{
		Var		   *var = copyVar((Var *) node);

		Assert(var->varlevelsup == 0);

		/*
		 * We should not see any Vars marked INNER_VAR or OUTER_VAR.  But an
		 * indexqual expression could contain INDEX_VAR Vars.
		 */
		Assert(var->varno != INNER_VAR);
		Assert(var->varno != OUTER_VAR);
		if (!IS_SPECIAL_VARNO(var->varno))
			var->varno += context->rtoffset;
		if (var->varnoold > 0)
			var->varnoold += context->rtoffset;

        /* Pseudo column reference? */
        if (var->varattno <= FirstLowInvalidHeapAttributeNumber)
        {
            RangeTblEntry          *rte = NULL;
            CdbRelColumnInfo       *rci = NULL;
            Node 				   *exprCopy = NULL;
            
            /* Look up the pseudo column definition. */
            rte = rt_fetch(var->varno, context->root->glob->finalrtable);
            rci = cdb_rte_find_pseudo_column(rte, var->varattno);
            Assert(rci && rci->defexpr && "No expression for pseudo column");

            exprCopy = copyObject((Node *) rci->defexpr);
            /* Fill in OpExpr operator ids. */
            fix_scan_expr_walker(exprCopy, context);

            /* Replace the Var node with a copy of the defining expr. */
			return (Node *) exprCopy;
		}
		else
		{
			return (Node *) var;
		}
	}
	if (IsA(node, CurrentOfExpr))
	{
		CurrentOfExpr *cexpr = (CurrentOfExpr *) copyObject(node);

		Assert(cexpr->cvarno != INNER_VAR);
		Assert(cexpr->cvarno != OUTER_VAR);
		if (!IS_SPECIAL_VARNO(cexpr->cvarno))
			cexpr->cvarno += context->rtoffset;
		return (Node *) cexpr;
	}
	if (IsA(node, PlaceHolderVar))
	{
		/* At scan level, we should always just evaluate the contained expr */
		PlaceHolderVar *phv = (PlaceHolderVar *) node;

		return fix_scan_expr_mutator((Node *) phv->phexpr, context);
	}
	fix_expr_common(context->root, node);
	return expression_tree_mutator(node, fix_scan_expr_mutator,
								   (void *) context);
}

static bool
fix_scan_expr_walker(Node *node, fix_scan_expr_context *context)
{
	if (node == NULL)
		return false;
	Assert(!IsA(node, PlaceHolderVar));
	fix_expr_common(context->root, node);
	return expression_tree_walker(node, fix_scan_expr_walker,
								  (void *) context);
}

/*
 * set_join_references
 *	  Modify the target list and quals of a join node to reference its
 *	  subplans, by setting the varnos to OUTER_VAR or INNER_VAR and setting
 *	  attno values to the result domain number of either the corresponding
 *	  outer or inner join tuple item.  Also perform opcode lookup for these
 *	  expressions. and add regclass OIDs to root->glob->relationOids.
 */
static void
set_join_references(PlannerInfo *root, Join *join, int rtoffset)
{
	Plan	   *outer_plan = join->plan.lefttree;
	Plan	   *inner_plan = join->plan.righttree;
	indexed_tlist *outer_itlist;
	indexed_tlist *inner_itlist;

	outer_itlist = build_tlist_index(outer_plan->targetlist);
	inner_itlist = build_tlist_index(inner_plan->targetlist);

	/*
	 * First process the joinquals (including merge or hash clauses).  These
	 * are logically below the join so they can always use all values
	 * available from the input tlists.  It's okay to also handle
	 * NestLoopParams now, because those couldn't refer to nullable
	 * subexpressions.
	 */
	join->joinqual = fix_join_expr(root,
								   join->joinqual,
								   outer_itlist,
								   inner_itlist,
								   (Index) 0,
								   rtoffset);

	/* Now do join-type-specific stuff */
	if (IsA(join, NestLoop))
	{
		NestLoop   *nl = (NestLoop *) join;
		ListCell   *lc;

		foreach(lc, nl->nestParams)
		{
			NestLoopParam *nlp = (NestLoopParam *) lfirst(lc);

			nlp->paramval = (Var *) fix_upper_expr(root,
												   (Node *) nlp->paramval,
												   outer_itlist,
												   OUTER_VAR,
												   rtoffset);
			/* Check we replaced any PlaceHolderVar with simple Var */
			if (!(IsA(nlp->paramval, Var) &&
				  nlp->paramval->varno == OUTER_VAR))
				elog(ERROR, "NestLoopParam was not reduced to a simple Var");
		}
	}
	else if (IsA(join, MergeJoin))
	{
		MergeJoin  *mj = (MergeJoin *) join;

		mj->mergeclauses = fix_join_expr(root,
										 mj->mergeclauses,
										 outer_itlist,
										 inner_itlist,
										 (Index) 0,
										 rtoffset);
	}
	else if (IsA(join, HashJoin))
	{
		HashJoin   *hj = (HashJoin *) join;

		hj->hashclauses = fix_hashclauses(root,
										hj->hashclauses,
										outer_itlist,
										inner_itlist,
										(Index) 0,
										rtoffset);

		hj->hashqualclauses = fix_join_expr(root,
											hj->hashqualclauses,
											outer_itlist,
											inner_itlist,
											(Index) 0,
											rtoffset);
	}

	/*
	 * Now we need to fix up the targetlist and qpqual, which are logically
	 * above the join.  This means they should not re-use any input expression
	 * that was computed in the nullable side of an outer join.  Vars and
	 * PlaceHolderVars are fine, so we can implement this restriction just by
	 * clearing has_non_vars in the indexed_tlist structs.
	 *
	 * XXX This is a grotty workaround for the fact that we don't clearly
	 * distinguish between a Var appearing below an outer join and the "same"
	 * Var appearing above it.  If we did, we'd not need to hack the matching
	 * rules this way.
	 */
	switch (join->jointype)
	{
		case JOIN_LEFT:
		case JOIN_SEMI:
		case JOIN_ANTI:
		case JOIN_LASJ_NOTIN:
			inner_itlist->has_non_vars = false;
			break;
		case JOIN_RIGHT:
			outer_itlist->has_non_vars = false;
			break;
		case JOIN_FULL:
			outer_itlist->has_non_vars = false;
			inner_itlist->has_non_vars = false;
			break;
		default:
			break;
	}

	join->plan.targetlist = fix_join_expr(root,
										  join->plan.targetlist,
										  outer_itlist,
										  inner_itlist,
										  (Index) 0,
										  rtoffset);
	join->plan.qual = fix_join_expr(root,
									join->plan.qual,
									outer_itlist,
									inner_itlist,
									(Index) 0,
									rtoffset);

	pfree(outer_itlist);
	pfree(inner_itlist);
}

/*
 * set_upper_references
 *	  Update the targetlist and quals of an upper-level plan node
 *	  to refer to the tuples returned by its lefttree subplan.
 *	  Also perform opcode lookup for these expressions, and
 *	  add regclass OIDs to root->glob->relationOids.
 *
 * This is used for single-input plan types like Agg, Group, Result.
 *
 * In most cases, we have to match up individual Vars in the tlist and
 * qual expressions with elements of the subplan's tlist (which was
 * generated by flatten_tlist() from these selfsame expressions, so it
 * should have all the required variables).  There is an important exception,
 * however: GROUP BY and ORDER BY expressions will have been pushed into the
 * subplan tlist unflattened.  If these values are also needed in the output
 * then we want to reference the subplan tlist element rather than recomputing
 * the expression.
 */
static void
set_upper_references(PlannerInfo *root, Plan *plan, int rtoffset)
{
	Plan	   *subplan = plan->lefttree;
	indexed_tlist *subplan_itlist;
	List	   *output_targetlist;
	ListCell   *l;

	subplan_itlist = build_tlist_index(subplan->targetlist);

	output_targetlist = NIL;
	foreach(l, plan->targetlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(l);
		Node	   *newexpr;

		if (IsA(plan, Repeat) &&
			(IsA(tle->expr, Grouping) || IsA(tle->expr, GroupId)))
		{
			/*
			 * CDB: group_id() & grouping() rely on Repeat to generate non-zero
			 * values for repeated grouping columns. So, always compute them, rather
			 * than using OUTER refs from subplan.
			 */
			newexpr = copyObject(tle->expr);
		}
		else
		{
			/* If it's a non-Var sort/group item, first try to match by sortref */
			if (tle->ressortgroupref != 0 && !IsA(tle->expr, Var))
			{
				newexpr = (Node *)
					search_indexed_tlist_for_sortgroupref((Node *) tle->expr,
														  tle->ressortgroupref,
														  subplan_itlist,
														  OUTER_VAR);
				if (!newexpr)
					newexpr = fix_upper_expr(root,
											 (Node *) tle->expr,
											 subplan_itlist,
											 OUTER_VAR,
											 rtoffset);
			}
			else
				newexpr = fix_upper_expr(root,
										 (Node *) tle->expr,
										 subplan_itlist,
										 OUTER_VAR,
										 rtoffset);
		}
		tle = flatCopyTargetEntry(tle);
		tle->expr = (Expr *) newexpr;
		output_targetlist = lappend(output_targetlist, tle);
	}
	plan->targetlist = output_targetlist;

	plan->qual = (List *)
		fix_upper_expr(root,
					   (Node *) plan->qual,
					   subplan_itlist,
					   OUTER_VAR,
					   rtoffset);

	pfree(subplan_itlist);
}

/*
 * set_dummy_tlist_references
 *	  Replace the targetlist of an upper-level plan node with a simple
 *	  list of OUTER_VAR references to its child.
 *
 * This is used for plan types like Sort and Append that don't evaluate
 * their targetlists.  Although the executor doesn't care at all what's in
 * the tlist, EXPLAIN needs it to be realistic.
 *
 * Note: we could almost use set_upper_references() here, but it fails for
 * Append for lack of a lefttree subplan.  Single-purpose code is faster
 * anyway.
 */
static void
set_dummy_tlist_references(Plan *plan, int rtoffset)
{
	List	   *output_targetlist;
	ListCell   *l;

	output_targetlist = NIL;
	foreach(l, plan->targetlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(l);
		Var		   *oldvar = (Var *) tle->expr;
		Var		   *newvar;

		newvar = makeVar(OUTER_VAR,
						 tle->resno,
						 exprType((Node *) oldvar),
						 exprTypmod((Node *) oldvar),
						 exprCollation((Node *) oldvar),
						 0);
		if (IsA(oldvar, Var))
		{
			newvar->varnoold = oldvar->varno + rtoffset;
			newvar->varoattno = oldvar->varattno;
		}
		else
		{
			newvar->varnoold = 0;		/* wasn't ever a plain Var */
			newvar->varoattno = 0;
		}

		tle = flatCopyTargetEntry(tle);
		tle->expr = (Expr *) newvar;
		output_targetlist = lappend(output_targetlist, tle);
	}
	plan->targetlist = output_targetlist;

	/* We don't touch plan->qual here */
}

/*
 * Split update is a bit special. It doesn't evaluate targetlist expressions,
 * but it adds an extra DMLActionExpr attribute to the output. Also, because
 * there is an assertion in ModifyTable that its subplan must contain a NULL
 * Const for any dropped columns, we must represent NULL constants as Const
 * node, even though they are passed through from the node below, rather than
 * evaluated at the Split Update node. So this is mostly the same as
 * set_dummy_tlist_references(), except for the special handling of
 * DMLActionExpr and Consts.
 */
static void
set_splitupdate_tlist_references(Plan *plan, int rtoffset)
{
	List	   *output_targetlist;
	ListCell   *l;

	output_targetlist = NIL;
	foreach(l, plan->targetlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(l);
		Var		   *oldvar = (Var *) tle->expr;
		Var		   *newvar;

		if (IsA(tle->expr, DMLActionExpr))
		{
			output_targetlist = lappend(output_targetlist, tle);
			continue;
		}
		else if (IsA(tle->expr, Const))
		{
			output_targetlist = lappend(output_targetlist, tle);
			continue;
		}

		newvar = makeVar(OUTER_VAR,
						 tle->resno,
						 exprType((Node *) oldvar),
						 exprTypmod((Node *) oldvar),
						 exprCollation((Node *) oldvar),
						 0);
		if (IsA(oldvar, Var))
		{
			newvar->varnoold = oldvar->varno + rtoffset;
			newvar->varoattno = oldvar->varattno;
		}
		else
		{
			newvar->varnoold = 0;		/* wasn't ever a plain Var */
			newvar->varoattno = 0;
		}

		tle = flatCopyTargetEntry(tle);
		tle->expr = (Expr *) newvar;
		output_targetlist = lappend(output_targetlist, tle);
	}
	plan->targetlist = output_targetlist;

	/* We don't touch plan->qual here */
}

/*
 * build_tlist_index --- build an index data structure for a child tlist
 *
 * In most cases, subplan tlists will be "flat" tlists with only Vars,
 * so we try to optimize that case by extracting information about Vars
 * in advance.  Matching a parent tlist to a child is still an O(N^2)
 * operation, but at least with a much smaller constant factor than plain
 * tlist_member() searches.
 *
 * The result of this function is an indexed_tlist struct to pass to
 * search_indexed_tlist_for_var() or search_indexed_tlist_for_non_var().
 * When done, the indexed_tlist may be freed with a single pfree().
 */
static indexed_tlist *
build_tlist_index(List *tlist)
{
	indexed_tlist *itlist;
	tlist_vinfo *vinfo;
	ListCell   *l;

	/* Create data structure with enough slots for all tlist entries */
	itlist = (indexed_tlist *)
		palloc(offsetof(indexed_tlist, vars) +
			   list_length(tlist) * sizeof(tlist_vinfo));

	itlist->tlist = tlist;
	itlist->has_ph_vars = false;
	itlist->has_non_vars = false;

	/* Find the Vars and fill in the index array */
	vinfo = itlist->vars;
	foreach(l, tlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(l);
		Expr	   *expr = tle->expr;

		Assert(expr);

		/*
		 * Allow a Var in parent node's expr to find matching Var in tlist
		 * ignoring any RelabelType nodes atop the tlist Var.  Also set
		 * has_non_vars so tlist expr can be matched as a whole.
		 */
		while (IsA(expr, RelabelType))
		{
			expr = ((RelabelType *)expr)->arg;
			itlist->has_non_vars = true;
		}

		if (expr && IsA(expr, Var))
		{
			Var		   *var = (Var *) expr;

			vinfo->varno = var->varno;
			vinfo->varattno = var->varattno;
			vinfo->resno = tle->resno;
			vinfo++;
		}
		else if (tle->expr && IsA(tle->expr, PlaceHolderVar))
			itlist->has_ph_vars = true;
		else
			itlist->has_non_vars = true;
	}

	itlist->num_vars = (vinfo - itlist->vars);

	return itlist;
}

/*
 * build_tlist_index_other_vars --- build a restricted tlist index
 *
 * This is like build_tlist_index, but we only index tlist entries that
 * are Vars belonging to some rel other than the one specified.  We will set
 * has_ph_vars (allowing PlaceHolderVars to be matched), but not has_non_vars
 * (so nothing other than Vars and PlaceHolderVars can be matched).
 */
static indexed_tlist *
build_tlist_index_other_vars(List *tlist, Index ignore_rel)
{
	indexed_tlist *itlist;
	tlist_vinfo *vinfo;
	ListCell   *l;

	/* Create data structure with enough slots for all tlist entries */
	itlist = (indexed_tlist *)
		palloc(offsetof(indexed_tlist, vars) +
			   list_length(tlist) * sizeof(tlist_vinfo));

	itlist->tlist = tlist;
	itlist->has_ph_vars = false;
	itlist->has_non_vars = false;

	/* Find the desired Vars and fill in the index array */
	vinfo = itlist->vars;
	foreach(l, tlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(l);

		if (tle->expr && IsA(tle->expr, Var))
		{
			Var		   *var = (Var *) tle->expr;

			if (var->varno != ignore_rel)
			{
				vinfo->varno = var->varno;
				vinfo->varattno = var->varattno;
				vinfo->resno = tle->resno;
				vinfo++;
			}
		}
		else if (tle->expr && IsA(tle->expr, PlaceHolderVar))
			itlist->has_ph_vars = true;
	}

	itlist->num_vars = (vinfo - itlist->vars);

	return itlist;
}

/*
 * search_indexed_tlist_for_var --- find a Var in an indexed tlist
 *
 * If a match is found, return a copy of the given Var with suitably
 * modified varno/varattno (to wit, newvarno and the resno of the TLE entry).
 * Also ensure that varnoold is incremented by rtoffset.
 * If no match, return NULL.
 */
static Var *
search_indexed_tlist_for_var(Var *var, indexed_tlist *itlist,
							 Index newvarno, int rtoffset)
{
	Index		varno = var->varno;
	AttrNumber	varattno = var->varattno;
	tlist_vinfo *vinfo;
	int			i;

	vinfo = itlist->vars;
	i = itlist->num_vars;
	while (i-- > 0)
	{
		if (vinfo->varno == varno && vinfo->varattno == varattno)
		{
			/* Found a match */
			Var		   *newvar = copyVar(var);

			newvar->varno = newvarno;
			newvar->varattno = vinfo->resno;
			if (newvar->varnoold > 0)
				newvar->varnoold += rtoffset;
			return newvar;
		}
		vinfo++;
	}
	return NULL;				/* no match */
}

/*
 * search_indexed_tlist_for_non_var --- find a non-Var in an indexed tlist
 *
 * If a match is found, return a Var constructed to reference the tlist item.
 * If no match, return NULL.
 *
 * NOTE: it is a waste of time to call this unless itlist->has_ph_vars or
 * itlist->has_non_vars.  Furthermore, set_join_references() relies on being
 * able to prevent matching of non-Vars by clearing itlist->has_non_vars,
 * so there's a correctness reason not to call it unless that's set.
 */
static Var *
search_indexed_tlist_for_non_var(Node *node,
								 indexed_tlist *itlist, Index newvarno)
{
	TargetEntry *tle;

	tle = tlist_member(node, itlist->tlist);
	if (tle)
	{
		/* Found a matching subplan output expression */
		Var		   *newvar;

		newvar = makeVarFromTargetEntry(newvarno, tle);
		newvar->varnoold = 0;	/* wasn't ever a plain Var */
		newvar->varoattno = 0;
		return newvar;
	}
	return NULL;				/* no match */
}

/*
 * GPDB: same as search_indexed_tlist_for_non_var, except it ignores
 * relabel for matching non vars in flow->hashExpr nodes.
 */
static Var *
search_indexed_tlist_for_non_var_in_flow_expr(Node *node,
												indexed_tlist *itlist, Index newvarno)
{
	TargetEntry *tle;

	tle = tlist_member_ignore_relabel(node, itlist->tlist);
	if (tle)
	{
		/* Found a matching subplan output expression */
		Var		   *newvar;

		newvar = makeVarFromTargetEntry(newvarno, tle);
		newvar->varnoold = 0;	/* wasn't ever a plain Var */
		newvar->varoattno = 0;
		return newvar;
	}
	return NULL;				/* no match */
}

/*
 * search_indexed_tlist_for_sortgroupref --- find a sort/group expression
 *		(which is assumed not to be just a Var)
 *
 * If a match is found, return a Var constructed to reference the tlist item.
 * If no match, return NULL.
 *
 * This is needed to ensure that we select the right subplan TLE in cases
 * where there are multiple textually-equal()-but-volatile sort expressions.
 * And it's also faster than search_indexed_tlist_for_non_var.
 */
static Var *
search_indexed_tlist_for_sortgroupref(Node *node,
									  Index sortgroupref,
									  indexed_tlist *itlist,
									  Index newvarno)
{
	ListCell   *lc;

	foreach(lc, itlist->tlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);

		/* The equal() check should be redundant, but let's be paranoid */
		if (tle->ressortgroupref == sortgroupref &&
			equal(node, tle->expr))
		{
			/* Found a matching subplan output expression */
			Var		   *newvar;

			newvar = makeVarFromTargetEntry(newvarno, tle);
			newvar->varnoold = 0;		/* wasn't ever a plain Var */
			newvar->varoattno = 0;
			return newvar;
		}
	}
	return NULL;				/* no match */
}

/*
 * fix_join_expr
 *	   Create a new set of targetlist entries or join qual clauses by
 *	   changing the varno/varattno values of variables in the clauses
 *	   to reference target list values from the outer and inner join
 *	   relation target lists.  Also perform opcode lookup and add
 *	   regclass OIDs to root->glob->relationOids.
 *
 * This is used in two different scenarios: a normal join clause, where all
 * the Vars in the clause *must* be replaced by OUTER_VAR or INNER_VAR
 * references; and a RETURNING clause, which may contain both Vars of the
 * target relation and Vars of other relations.  In the latter case we want
 * to replace the other-relation Vars by OUTER_VAR references, while leaving
 * target Vars alone.
 *
 * For a normal join, acceptable_rel should be zero so that any failure to
 * match a Var will be reported as an error.  For the RETURNING case, pass
 * inner_itlist = NULL and acceptable_rel = the ID of the target relation.
 *
 * 'clauses' is the targetlist or list of join clauses
 * 'outer_itlist' is the indexed target list of the outer join relation
 * 'inner_itlist' is the indexed target list of the inner join relation,
 *		or NULL
 * 'acceptable_rel' is either zero or the rangetable index of a relation
 *		whose Vars may appear in the clause without provoking an error
 * 'rtoffset': how much to increment varnoold by
 *
 * Returns the new expression tree.  The original clause structure is
 * not modified.
 */
static List *
fix_join_expr(PlannerInfo *root,
			  List *clauses,
			  indexed_tlist *outer_itlist,
			  indexed_tlist *inner_itlist,
			  Index acceptable_rel,
			  int rtoffset)
{
	fix_join_expr_context context;

	context.root = root;
	context.outer_itlist = outer_itlist;
	context.inner_itlist = inner_itlist;
	context.acceptable_rel = acceptable_rel;
	context.rtoffset = rtoffset;
	context.use_outer_tlist_for_matching_nonvars = true;
	context.use_inner_tlist_for_matching_nonvars = true;

	return (List *) fix_join_expr_mutator((Node *) clauses, &context);
}

/*
 * fix_hashclauses
 *
 *  make sure that inner argument of each hashclause does not refer to
 *  target entries found in the target list of join's outer child
 *
 */
static List *fix_hashclauses(PlannerInfo *root,
                           List *clauses,
                           indexed_tlist *outer_itlist,
                           indexed_tlist *inner_itlist,
                           Index acceptable_rel, int rtoffset)
{
    Assert(clauses);
    ListCell *lc = NULL;
    foreach(lc, clauses)
    {
        Node *node = (Node *) lfirst(lc);
        Assert(IsA(node, OpExpr));
        OpExpr     *opexpr = (OpExpr *) node;
        Assert(list_length(opexpr->args) == 2);
        /* extract clause arguments */
        List *outer_arg = linitial(opexpr->args);
        List *inner_arg = lsecond(opexpr->args);
        List *new_args = NIL;
        /*
         * for outer argument, we cannot refer to target entries
         * in join's inner child target list
         * we change walker's context to guarantee this
         */
        List *new_outer_arg = fix_child_hashclauses(root,
                outer_arg,
                outer_itlist,
                inner_itlist,
                (Index) 0,
                rtoffset,
                OUTER_VAR);
        /*
         * for inner argument, we cannot refer to target entries
         * in join's outer child target list, otherwise hash table
         * creation could fail,
         * we change walker's context to guarantee this
         */
        List *new_inner_arg = fix_child_hashclauses(root,
                inner_arg,
                outer_itlist,
                inner_itlist,
                (Index) 0,
                rtoffset,
                INNER_VAR);
        new_args = lappend(new_args, new_outer_arg);
        new_args = lappend(new_args, new_inner_arg);
        /* replace old arguments with the fixed arguments */
        list_free(opexpr->args);
        opexpr->args = new_args;
        /* fix opexpr */
        fix_expr_common(root, node);
    }
    return clauses;
}
/*
 * fix_child_hashclauses
 *     A special case of fix_join_expr used to process hash join's child hashclauses.
 *     The main use case is MPP-18537 and MPP-21564, where we have a constant in the
 *     target list of hash join's child, and the constant is used when computing hash
 *     value of hash join's other child.
 *
 *     Example: select * from A, B where A.i = least(B.i,4) and A.j=4;
 *     Here, B's hash value is least(B.i,4), and constant 4 is defined by A's target list
 *
 *     Since during computing the hash value for a tuple on one side of hash join, we cannot access
 *     the target list of hash join's other child, this function skips using other target list
 *     when matching non-vars.
 *
 */
static List *
fix_child_hashclauses(PlannerInfo *root,
              List *clauses,
              indexed_tlist *outer_itlist,
              indexed_tlist *inner_itlist,
              Index acceptable_rel,
              int rtoffset,
              Index child)
{
    fix_join_expr_context context;
    context.root = root;
    context.outer_itlist = outer_itlist;
    context.inner_itlist = inner_itlist;
    context.acceptable_rel = acceptable_rel;
    context.rtoffset = rtoffset;
    if (INNER_VAR == child)
    {
    	/* skips using outer target list when matching non-vars */
    	context.use_outer_tlist_for_matching_nonvars = false;
    	context.use_inner_tlist_for_matching_nonvars = true;
	}
	else
	{
    	/* skips using inner target list when matching non-vars */
    	context.use_inner_tlist_for_matching_nonvars = false;
    	context.use_outer_tlist_for_matching_nonvars = true;
	}
    return (List *) fix_join_expr_mutator((Node *) clauses, &context);
}


static Node *
fix_join_expr_mutator(Node *node, fix_join_expr_context *context)
{
	Var		   *newvar;

	if (node == NULL)
		return NULL;
	if (IsA(node, Var))
	{
		Var		   *var = (Var *) node;

		/* First look for the var in the input tlists */
		newvar = search_indexed_tlist_for_var(var,
											  context->outer_itlist,
											  OUTER_VAR,
											  context->rtoffset);
		if (newvar)
			return (Node *) newvar;
		if (context->inner_itlist)
		{
			newvar = search_indexed_tlist_for_var(var,
												  context->inner_itlist,
												  INNER_VAR,
												  context->rtoffset);
			if (newvar)
				return (Node *) newvar;
		}

		/* If it's for an acceptable_rel (the inner relation in an index nested loop join), return it */
		if (var->varno == context->acceptable_rel)
		{
			var = copyVar(var);
			var->varno += context->rtoffset;
			if (var->varnoold > 0)
				var->varnoold += context->rtoffset;
			return (Node *) var;
		}

		/* No referent found for Var */
		elog(ERROR, "variable not found in subplan target lists");
	}
	if (IsA(node, PlaceHolderVar))
	{
		PlaceHolderVar *phv = (PlaceHolderVar *) node;

		/* See if the PlaceHolderVar has bubbled up from a lower plan node */
		if (context->outer_itlist->has_ph_vars)
		{
			newvar = search_indexed_tlist_for_non_var((Node *) phv,
													  context->outer_itlist,
													  OUTER_VAR);
			if (newvar)
				return (Node *) newvar;
		}
		if (context->inner_itlist && context->inner_itlist->has_ph_vars)
		{
			newvar = search_indexed_tlist_for_non_var((Node *) phv,
													  context->inner_itlist,
													  INNER_VAR);
			if (newvar)
				return (Node *) newvar;
		}

		/* If not supplied by input plans, evaluate the contained expr */
		return fix_join_expr_mutator((Node *) phv->phexpr, context);
	}

	/* Try matching more complex expressions too, if tlists have any */
	if (context->outer_itlist && context->outer_itlist->has_non_vars &&
	        context->use_outer_tlist_for_matching_nonvars)
	{
		newvar = search_indexed_tlist_for_non_var(node,
												  context->outer_itlist,
												  OUTER_VAR);
		if (newvar)
			return (Node *) newvar;
	}
	if (context->inner_itlist && context->inner_itlist->has_non_vars &&
	        context->use_inner_tlist_for_matching_nonvars)
	{
		newvar = search_indexed_tlist_for_non_var(node,
												  context->inner_itlist,
												  INNER_VAR);
		if (newvar)
			return (Node *) newvar;
	}
	fix_expr_common(context->root, node);
	return expression_tree_mutator(node,
								   fix_join_expr_mutator,
								   (void *) context);
}

/*
 * fix_upper_expr
 *		Modifies an expression tree so that all Var nodes reference outputs
 *		of a subplan.  Also performs opcode lookup, and adds regclass OIDs to
 *		root->glob->relationOids.
 *
 * This is used to fix up target and qual expressions of non-join upper-level
 * plan nodes, as well as index-only scan nodes.
 *
 * An error is raised if no matching var can be found in the subplan tlist
 * --- so this routine should only be applied to nodes whose subplans'
 * targetlists were generated via flatten_tlist() or some such method.
 *
 * If itlist->has_non_vars is true, then we try to match whole subexpressions
 * against elements of the subplan tlist, so that we can avoid recomputing
 * expressions that were already computed by the subplan.  (This is relatively
 * expensive, so we don't want to try it in the common case where the
 * subplan tlist is just a flattened list of Vars.)
 *
 * 'node': the tree to be fixed (a target item or qual)
 * 'subplan_itlist': indexed target list for subplan (or index)
 * 'newvarno': varno to use for Vars referencing tlist elements
 * 'rtoffset': how much to increment varnoold by
 *
 * The resulting tree is a copy of the original in which all Var nodes have
 * varno = newvarno, varattno = resno of corresponding targetlist element.
 * The original tree is not modified.
 */
static Node *
fix_upper_expr(PlannerInfo *root,
			   Node *node,
			   indexed_tlist *subplan_itlist,
			   Index newvarno,
			   int rtoffset)
{
	fix_upper_expr_context context;

	context.root = root;
	context.subplan_itlist = subplan_itlist;
	context.newvarno = newvarno;
	context.rtoffset = rtoffset;
	context.flow_expr = false;
	return fix_upper_expr_mutator(node, &context);
}

static Node *
fix_upper_flow_expr(PlannerInfo *root,
					Node *node,
					indexed_tlist *subplan_itlist,
					Index newvarno,
					int rtoffset)
{
	fix_upper_expr_context context;

	context.root = root;
	context.subplan_itlist = subplan_itlist;
	context.newvarno = newvarno;
	context.rtoffset = rtoffset;
	context.flow_expr = true;
	return fix_upper_expr_mutator(node, &context);
}
static Node *
fix_upper_expr_mutator(Node *node, fix_upper_expr_context *context)
{
	Var		   *newvar;

	if (node == NULL)
		return NULL;
	if (IsA(node, Var))
	{
		Var		   *var = (Var *) node;

		newvar = search_indexed_tlist_for_var(var,
											  context->subplan_itlist,
											  context->newvarno,
											  context->rtoffset);
		if (!newvar)
			elog(ERROR, "variable not found in subplan target list");
		return (Node *) newvar;
	}
	if (IsA(node, PlaceHolderVar))
	{
		PlaceHolderVar *phv = (PlaceHolderVar *) node;

		/* See if the PlaceHolderVar has bubbled up from a lower plan node */
		if (context->subplan_itlist->has_ph_vars)
		{
			newvar = search_indexed_tlist_for_non_var((Node *) phv,
													  context->subplan_itlist,
													  context->newvarno);
			if (newvar)
				return (Node *) newvar;
		}
		/* If not supplied by input plan, evaluate the contained expr */
		return fix_upper_expr_mutator((Node *) phv->phexpr, context);
	}
	/* Try matching more complex expressions too, if tlist has any */
	if (context->subplan_itlist->has_non_vars && !IsA(node, GroupId))
	{
		if (context->flow_expr)
		{
			newvar = search_indexed_tlist_for_non_var_in_flow_expr(node,
																  context->subplan_itlist,
																  context->newvarno);
		}
		else
		{
			newvar = search_indexed_tlist_for_non_var(node,
													  context->subplan_itlist,
													  context->newvarno);
		}
		if (newvar)
			return (Node *) newvar;
	}
	fix_expr_common(context->root, node);
	return expression_tree_mutator(node,
								   fix_upper_expr_mutator,
								   (void *) context);
}

/*
 * set_returning_clause_references
 *		Perform setrefs.c's work on a RETURNING targetlist
 *
 * If the query involves more than just the result table, we have to
 * adjust any Vars that refer to other tables to reference junk tlist
 * entries in the top subplan's targetlist.  Vars referencing the result
 * table should be left alone, however (the executor will evaluate them
 * using the actual heap tuple, after firing triggers if any).  In the
 * adjusted RETURNING list, result-table Vars will have their original
 * varno (plus rtoffset), but Vars for other rels will have varno OUTER_VAR.
 *
 * We also must perform opcode lookup and add regclass OIDs to
 * root->glob->relationOids.
 *
 * 'rlist': the RETURNING targetlist to be fixed
 * 'topplan': the top subplan node that will be just below the ModifyTable
 *		node (note it's not yet passed through set_plan_refs)
 * 'resultRelation': RT index of the associated result relation
 * 'rtoffset': how much to increment varnos by
 *
 * Note: the given 'root' is for the parent query level, not the 'topplan'.
 * This does not matter currently since we only access the dependency-item
 * lists in root->glob, but it would need some hacking if we wanted a root
 * that actually matches the subplan.
 *
 * Note: resultRelation is not yet adjusted by rtoffset.
 */
static List *
set_returning_clause_references(PlannerInfo *root,
								List *rlist,
								Plan *topplan,
								Index resultRelation,
								int rtoffset)
{
	indexed_tlist *itlist;

	/*
	 * We can perform the desired Var fixup by abusing the fix_join_expr
	 * machinery that formerly handled inner indexscan fixup.  We search the
	 * top plan's targetlist for Vars of non-result relations, and use
	 * fix_join_expr to convert RETURNING Vars into references to those tlist
	 * entries, while leaving result-rel Vars as-is.
	 *
	 * PlaceHolderVars will also be sought in the targetlist, but no
	 * more-complex expressions will be.  Note that it is not possible for a
	 * PlaceHolderVar to refer to the result relation, since the result is
	 * never below an outer join.  If that case could happen, we'd have to be
	 * prepared to pick apart the PlaceHolderVar and evaluate its contained
	 * expression instead.
	 */
	itlist = build_tlist_index_other_vars(topplan->targetlist, resultRelation);

	rlist = fix_join_expr(root,
						  rlist,
						  itlist,
						  NULL,
						  resultRelation,
						  rtoffset);

	pfree(itlist);

	return rlist;
}

/*****************************************************************************
 *					OPERATOR REGPROC LOOKUP
 *****************************************************************************/

/*
 * fix_opfuncids
 *	  Calculate opfuncid field from opno for each OpExpr node in given tree.
 *	  The given tree can be anything expression_tree_walker handles.
 *
 * The argument is modified in-place.  (This is OK since we'd want the
 * same change for any node, even if it gets visited more than once due to
 * shared structure.)
 */
void
fix_opfuncids(Node *node)
{
	/* This tree walk requires no special setup, so away we go... */
	fix_opfuncids_walker(node, NULL);
}

static bool
fix_opfuncids_walker(Node *node, void *context)
{
	if (node == NULL)
		return false;
	if (IsA(node, Grouping))
		return false;
	if (IsA(node, GroupId))
		return false;
	if (IsA(node, OpExpr))
		set_opfuncid((OpExpr *) node);
	else if (IsA(node, DistinctExpr))
		set_opfuncid((OpExpr *) node);	/* rely on struct equivalence */
	else if (IsA(node, NullIfExpr))
		set_opfuncid((OpExpr *) node);	/* rely on struct equivalence */
	else if (IsA(node, ScalarArrayOpExpr))
		set_sa_opfuncid((ScalarArrayOpExpr *) node);
	return expression_tree_walker(node, fix_opfuncids_walker, context);
}

/*
 * set_opfuncid
 *		Set the opfuncid (procedure OID) in an OpExpr node,
 *		if it hasn't been set already.
 *
 * Because of struct equivalence, this can also be used for
 * DistinctExpr and NullIfExpr nodes.
 */
void
set_opfuncid(OpExpr *opexpr)
{
	if (opexpr->opfuncid == InvalidOid)
		opexpr->opfuncid = get_opcode(opexpr->opno);
}

/*
 * set_sa_opfuncid
 *		As above, for ScalarArrayOpExpr nodes.
 */
void
set_sa_opfuncid(ScalarArrayOpExpr *opexpr)
{
	if (opexpr->opfuncid == InvalidOid)
		opexpr->opfuncid = get_opcode(opexpr->opno);
}

/*****************************************************************************
 *					QUERY DEPENDENCY MANAGEMENT
 *****************************************************************************/

/*
 * record_plan_function_dependency
 *		Mark the current plan as depending on a particular function.
 *
 * This is exported so that the function-inlining code can record a
 * dependency on a function that it's removed from the plan tree.
 */
void
record_plan_function_dependency(PlannerInfo *root, Oid funcid)
{
	/*
	 * For performance reasons, we don't bother to track built-in functions;
	 * we just assume they'll never change (or at least not in ways that'd
	 * invalidate plans using them).  For this purpose we can consider a
	 * built-in function to be one with OID less than FirstBootstrapObjectId.
	 * Note that the OID generator guarantees never to generate such an OID
	 * after startup, even at OID wraparound.
	 */
	if (funcid >= (Oid) FirstBootstrapObjectId)
	{
		PlanInvalItem *inval_item = makeNode(PlanInvalItem);

		/*
		 * It would work to use any syscache on pg_proc, but the easiest is
		 * PROCOID since we already have the function's OID at hand.  Note
		 * that plancache.c knows we use PROCOID.
		 */
		inval_item->cacheId = PROCOID;
		inval_item->hashValue = GetSysCacheHashValue1(PROCOID,
												   ObjectIdGetDatum(funcid));

		root->glob->invalItems = lappend(root->glob->invalItems, inval_item);
		add_proc_oids_for_dump(funcid);
	}
}

/*
 * extract_query_dependencies
 *		Given a not-yet-planned query or queries (i.e. a Query node or list
 *		of Query nodes), extract dependencies just as set_plan_references
 *		would do.
 *
 * This is needed by plancache.c to handle invalidation of cached unplanned
 * queries.
 */
void
extract_query_dependencies(Node *query,
						   List **relationOids,
						   List **invalItems)
{
	PlannerGlobal glob;
	PlannerInfo root;

	/* Make up dummy planner state so we can use this module's machinery */
	MemSet(&glob, 0, sizeof(glob));
	glob.type = T_PlannerGlobal;
	glob.relationOids = NIL;
	glob.invalItems = NIL;

	MemSet(&root, 0, sizeof(root));
	root.type = T_PlannerInfo;
	root.glob = &glob;

	(void) extract_query_dependencies_walker(query, &root);

	*relationOids = glob.relationOids;
	*invalItems = glob.invalItems;
}

static bool
extract_query_dependencies_walker(Node *node, PlannerInfo *context)
{
	if (node == NULL)
		return false;
	Assert(!IsA(node, PlaceHolderVar));
	/* Extract function dependencies and check for regclass Consts */
	fix_expr_common(context, node);
	if (IsA(node, Query))
	{
		Query	   *query = (Query *) node;
		ListCell   *lc;

		if (query->commandType == CMD_UTILITY)
		{
			/*
			 * Ignore utility statements, except those (such as EXPLAIN) that
			 * contain a parsed-but-not-planned query.
			 */
			query = UtilityContainsQuery(query->utilityStmt);
			if (query == NULL)
				return false;
		}

		/* Collect relation OIDs in this Query's rtable */
		foreach(lc, query->rtable)
		{
			RangeTblEntry *rte = (RangeTblEntry *) lfirst(lc);

			if (rte->rtekind == RTE_RELATION)
				context->glob->relationOids =
					lappend_oid(context->glob->relationOids, rte->relid);
		}

		/* And recurse into the query's subexpressions */
		return query_tree_walker(query, extract_query_dependencies_walker,
								 (void *) context, 0);
	}
	return expression_tree_walker(node, extract_query_dependencies_walker,
								  (void *) context);
}

/*
 * cdb_extract_plan_dependencies()
 *		Given a fully built Plan tree, extract their dependencies just as
 *		set_plan_references_ would have done.
 *
 * This is used to extract dependencies from a plan that has been created
 * by ORCA (set_plan_references() does this usually, but ORCA doesn't use
 * it). This adds the new entries directly to PlannerGlobal.relationOids
 * and invalItems.
 *
 * Note: This recurses into SubPlans. You better still call this for
 * every subplan in a overall plan, to make sure you capture dependencies
 * from subplans that are not referenced from the main plan, because
 * changes to the relations in eliminated subplans might require
 * re-planning, too. (XXX: it would be better to not recurse into SubPlans
 * here, as that's a waste of time.)
 */
void
cdb_extract_plan_dependencies(PlannerInfo *root, Plan *plan)
{
	cdb_extract_plan_dependencies_context context;

	context.base.node = (Node *) (root->glob);
	context.root = root;

	(void) cdb_extract_plan_dependencies_walker((Node *) plan, &context);
}

static bool
cdb_extract_plan_dependencies_walker(Node *node, cdb_extract_plan_dependencies_context *context)
{
	if (node == NULL)
		return false;
	/* Extract function dependencies and check for regclass Consts */
	fix_expr_common(context->root, node);

	return plan_tree_walker(node, cdb_extract_plan_dependencies_walker,
								  (void *) context);
}

/*
 * cdb_expr_requires_full_eval
 *
 * Returns true if expr could call a set-returning function.
 */
static bool
cdb_expr_requires_full_eval(Node *node)
{
    return expression_returns_set(node);
}                               /* cdb_expr_requires_full_eval */


/*
 * cdb_insert_result_node
 *
 * Adjusts the tree so that the target list of the given Plan node
 * will contain only Var nodes.  The old target list is moved onto
 * a new Result node which will be inserted above the given node.
 * Returns the new result node.
 *
 * This is needed, because we have gutted out the support for evaluating
 * set-returning-functions in targetlists in the executor, in all
 * nodes except the Result node. That gives a marginal performance
 * gain when there are no set-returning-functions in the target list,
 * which is the common case.
 */
static Plan *
cdb_insert_result_node(PlannerInfo *root, Plan *plan, int rtoffset)
{
    Plan   *resultplan;
    Flow   *flow;

    Assert(!IsA(plan, Result) &&
           cdb_expr_requires_full_eval((Node *)plan->targetlist));

    /* Unhook the Flow node temporarily.  Caller has already fixed it up. */
    flow = plan->flow;
	plan->flow = NULL;

	/*
	 * Build a Result node to take over the targetlist from the given Plan.
	 *
	 * XXX: We don't have a PlannerInfo struct at hand here, so we pass NULL
	 * and hope that make_result doesn't really need it. It's really too late
	 * to insert Result nodes at this late stage in the planner, we should
	 * eliminate the need for this.
	 */
    resultplan = (Plan *) make_result(NULL, plan->targetlist, NULL, plan);

    /* Build a new targetlist for the given Plan, with Var nodes only. */
    plan->targetlist = flatten_tlist(plan->targetlist,
									 PVC_RECURSE_AGGREGATES,
									 PVC_INCLUDE_PLACEHOLDERS);

    /* Fix up the Result node and the Plan tree below it. */
    resultplan = set_plan_refs(root, resultplan, rtoffset);

    /* Reattach the Flow node. */
    resultplan->flow = flow;
	plan->flow = flow;

    return resultplan;
}                               /* cdb_insert_result_node */
