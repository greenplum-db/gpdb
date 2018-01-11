/*-------------------------------------------------------------------------
 *
 * setrefs.c
 *	  Post-processing of a completed plan tree: fix references to subplan
 *	  vars, compute regproc values for operators, etc
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/optimizer/plan/setrefs.c,v 1.150 2009/06/11 14:48:59 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/transam.h"
#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/pathnode.h"
#include "optimizer/planmain.h"
#include "optimizer/tlist.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
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
	PlannerGlobal *glob;
	int			rtoffset;
} fix_scan_expr_context;

typedef struct
{
	PlannerGlobal *glob;
	indexed_tlist *outer_itlist;
	indexed_tlist *inner_itlist;
	Index		acceptable_rel;
	int			rtoffset;
	bool        use_outer_tlist_for_matching_nonvars;
	bool        use_inner_tlist_for_matching_nonvars;
} fix_join_expr_context;

typedef struct
{
	PlannerGlobal *glob;
	indexed_tlist *subplan_itlist;
	int			rtoffset;
} fix_upper_expr_context;

typedef struct
{
	plan_tree_base_prefix base;

	PlannerGlobal *glob;

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

#define fix_scan_list(glob, lst, rtoffset) \
	((List *) fix_scan_expr(glob, (Node *) (lst), rtoffset))

static Plan *set_plan_refs(PlannerGlobal *glob, Plan *plan, int rtoffset);
static Plan *set_subqueryscan_references(PlannerGlobal *glob,
							SubqueryScan *plan,
							int rtoffset);
static bool trivial_subqueryscan(SubqueryScan *plan);
static Node *fix_scan_expr(PlannerGlobal *glob, Node *node, int rtoffset);
static Node *fix_scan_expr_mutator(Node *node, fix_scan_expr_context *context);
static bool fix_scan_expr_walker(Node *node, fix_scan_expr_context *context);
static void set_join_references(PlannerGlobal *glob, Join *join, int rtoffset);
static void set_inner_join_references(PlannerGlobal *glob, Plan *inner_plan,
						  indexed_tlist *outer_itlist);
static void set_upper_references(PlannerGlobal *glob, Plan *plan, int rtoffset);
static void set_dummy_tlist_references(Plan *plan, int rtoffset);
static indexed_tlist *build_tlist_index(List *tlist);
static Var *search_indexed_tlist_for_var(Var *var,
							 indexed_tlist *itlist,
							 Index newvarno,
							 int rtoffset);
static Var *search_indexed_tlist_for_non_var(Node *node,
								 indexed_tlist *itlist,
								 Index newvarno);
static List *fix_join_expr(PlannerGlobal *glob,
			  List *clauses,
			  indexed_tlist *outer_itlist,
			  indexed_tlist *inner_itlist,
			  Index acceptable_rel, int rtoffset);
static Node *fix_join_expr_mutator(Node *node,
					  fix_join_expr_context *context);
static List *fix_hashclauses(PlannerGlobal *glob,
				List *clauses,
				indexed_tlist *outer_itlist,
				indexed_tlist *inner_itlist,
				Index acceptable_rel, int rtoffset);
static List *fix_child_hashclauses(PlannerGlobal *glob,
					  List *clauses,
					  indexed_tlist *outer_itlist,
					  indexed_tlist *inner_itlist,
					  Index acceptable_rel, int rtoffset,
					  Index child);
static Node *fix_upper_expr(PlannerGlobal *glob,
			   Node *node,
			   indexed_tlist *subplan_itlist,
			   int rtoffset);
static Node *fix_upper_expr_mutator(Node *node,
					   fix_upper_expr_context *context);
static bool fix_opfuncids_walker(Node *node, void *context);
static  bool cdb_expr_requires_full_eval(Node *node);
static Plan *cdb_insert_result_node(PlannerGlobal *glob, 
									Plan *plan, 
									int rtoffset);

static bool extract_query_dependencies_walker(Node *node,
								  PlannerGlobal *context);
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
	List *allParams;

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
		Assert((var->varno == OUTER
				|| (var->varno > 0 && var->varno <= list_length(rtable) + list_length(glob->finalrtable)))
				&& "Plan contains var that refer outside the rtable.");
		Assert(var->varno == var->varnoold && "Varno and varnoold do not agree!");

		/** If a pseudo column, there should be a corresponding entry in the relation */
		if (var->varattno <= FirstLowInvalidHeapAttributeNumber)
		{
			RangeTblEntry *rte = rt_fetch(var->varno, rtable);
			Assert(rte);
			Assert(rte->pseudocols);
			Assert(list_length(rte->pseudocols) > var->varattno - FirstLowInvalidHeapAttributeNumber);
		}

	}

	/* Ensure that all params that the plan refers to has a corresponding subplan */
	allParams = extract_nodes(glob, (Node *) plan, T_Param);

	foreach (lc, allParams)
	{
		Param *param = lfirst(lc);
		if (param->paramkind == PARAM_EXEC)
		{
			Assert(param->paramid < list_length(glob->paramlist) && "Parameter ID outside range of parameters known at the global level.");
			PlannerParamItem *paramItem = list_nth(glob->paramlist, param->paramid);
			Assert(paramItem);

			if (IsA(paramItem->item, Var))
			{
				Var *var = (Var *) paramItem->item;
				Assert(param->paramtype == var->vartype && "Parameter type and var type do not match!");
			}
			else if (IsA(paramItem->item, Aggref))
			{
				Aggref *aggRef = (Aggref *) paramItem->item;
				Assert(param->paramtype == aggRef->aggtype && "Param type and aggref type do not match!");
			}
			else
			{
				Assert("Global PlannerParamItem is not a var or an aggref node");
			}

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
		Assert((var->varno == INNER
				|| var->varno == OUTER
				|| (var->varno > 0 && var->varno <= list_length(glob->finalrtable)))
				&& "Plan contains var that refer outside the rtable.");
		Assert(var->varattno > FirstLowInvalidHeapAttributeNumber && "Invalid attribute number in plan");

		if (var->varno > 0 && var->varno <= list_length(glob->finalrtable))
		{
			List *colNames = NULL;
			RangeTblEntry *rte = rt_fetch(var->varno, glob->finalrtable);
			Assert(rte && "Invalid RTE");
			Assert(rte->rtekind != RTE_VOID && "Var points to a void RTE!");

			/* Make sure attnum refers to a column in the relation */
			expandRTE(rte, var->varno, 0, -1, true, &colNames, NULL);

			AssertImply(var->varattno >= 0, var->varattno <= list_length(colNames) + list_length(rte->pseudocols)); /* Only asserting on non-system attributes */
		}

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
 * This is the final processing pass of the planner/optimizer.	The plan
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
 * Inputs:
 *	glob: global data for planner run
 *	plan: the topmost node of the plan
 *	rtable: the rangetable for the current subquery
 *
 * The return value is normally the same Plan node passed in, but can be
 * different when the passed-in Plan is a SubqueryScan we decide isn't needed.
 *
 * The flattened rangetable entries are appended to glob->finalrtable, and
 * plan dependencies are appended to glob->relationOids (for relations)
 * and glob->invalItems (for everything else).
 *
 * Notice that we modify Plan nodes in-place, but use expression_tree_mutator
 * to process targetlist and qual expressions.	We can assume that the Plan
 * nodes were just built by the planner and are not multiply referenced, but
 * it's not so safe to assume that for expression tree nodes.
 */
Plan *
set_plan_references(PlannerGlobal *glob, Plan *plan, List *rtable)
{
	int			rtoffset = list_length(glob->finalrtable);
	ListCell   *lc;

#ifdef USE_ASSERT_CHECKING
	/* 
	 * This method formalizes our assumptions about the input to set_plan_references.
	 * This will hopefully, help us debug any problems.
	 */
	set_plan_references_input_asserts(glob, plan, rtable);
#endif

	/*
	 * In the flat rangetable, we zero out substructure pointers that are not
	 * needed by the executor; this reduces the storage space and copying cost
	 * for cached plans.  We keep only the alias and eref Alias fields, which
	 * are needed by EXPLAIN, and the selectedCols and modifiedCols bitmaps,
	 * which are needed for executor-startup permissions checking.
	 */
	foreach(lc, rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(lc);
		RangeTblEntry *newrte;

		/* flat copy to duplicate all the scalar fields */
		newrte = copyObject(rte);

		/** Need to fix up some of the references in the newly created newrte */
		fix_scan_expr(glob, (Node *) newrte->funcexpr, rtoffset);
		fix_scan_expr(glob, (Node *) newrte->joinaliasvars, rtoffset);
		fix_scan_expr(glob, (Node *) newrte->values_lists, rtoffset);

		glob->finalrtable = lappend(glob->finalrtable, newrte);

		/*
		 * If it's a plain relation RTE, add the table to relationOids.
		 *
		 * We do this even though the RTE might be unreferenced in the plan
		 * tree; this would correspond to cases such as views that were
		 * expanded, child tables that were eliminated by constraint
		 * exclusion, etc.	Schema invalidation on such a rel must still force
		 * rebuilding of the plan.
		 *
		 * Note we don't bother to avoid duplicate list entries.  We could,
		 * but it would probably cost more cycles than it would save.
		 */
		if (newrte->rtekind == RTE_RELATION)
			glob->relationOids = lappend_oid(glob->relationOids,
											 newrte->relid);
	}

	/* Now fix the Plan tree */
	Plan *retPlan = set_plan_refs(glob, plan, rtoffset);

#ifdef USE_ASSERT_CHECKING
	/**
	 * Ensuring that the output of setrefs behaves as expected.
	 */
	set_plan_references_output_asserts(glob, retPlan);
#endif

	return retPlan;
}

/*
 * set_plan_refs: recurse through the Plan nodes of a single subquery level
 */
static Plan *
set_plan_refs(PlannerGlobal *glob, Plan *plan, int rtoffset)
{
	ListCell   *l;

	if (plan == NULL)
		return NULL;

    /*
     * CDB: If plan has a Flow node, fix up its hashExpr to refer to the
     * plan's own targetlist.
     */
	if (plan->flow && plan->flow->hashExpr)
    {
        indexed_tlist  *plan_itlist = build_tlist_index(plan->targetlist);

        plan->flow->hashExpr =
		(List *)fix_upper_expr(glob,
							   (Node *)plan->flow->hashExpr,
							   plan_itlist,
							   rtoffset);
        pfree(plan_itlist);
    }

	/*
	 * Plan-type-specific fixes
	 */
	switch (nodeTag(plan))
	{
		case T_SeqScan: /* Rely on structure equivalence */
		case T_AppendOnlyScan: /* Rely on structure equivalence */
		case T_AOCSScan: /* Rely on structure equivalence */
		case T_ExternalScan: /* Rely on structure equivalence */
			{
				Scan    *splan = (Scan *) plan;

				if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
					return cdb_insert_result_node(glob, plan, rtoffset);

				splan->scanrelid += rtoffset;

				/* If the scan appears below a shareinput, we hit this assert. */
#ifdef USE_ASSERT_CHECKING
				Assert(splan->scanrelid <= list_length(glob->finalrtable) && "Scan node's relid is outside the finalrtable!");
				RangeTblEntry *rte = rt_fetch(splan->scanrelid, glob->finalrtable);
				Assert((rte->rtekind == RTE_RELATION || rte->rtekind == RTE_CTE) && "Scan plan should refer to a scan relation");
#endif

				splan->plan.targetlist =
					fix_scan_list(glob, splan->plan.targetlist, rtoffset);
				splan->plan.qual =
					fix_scan_list(glob, splan->plan.qual, rtoffset);
			}
			break;
		case T_IndexScan:
			{
				IndexScan  *splan = (IndexScan *) plan;

				if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
					return cdb_insert_result_node(glob, plan, rtoffset);

				splan->scan.scanrelid += rtoffset;

#ifdef USE_ASSERT_CHECKING
				RangeTblEntry *rte = rt_fetch(splan->scan.scanrelid, glob->finalrtable);
				char relstorage = get_rel_relstorage(rte->relid);
				Assert(relstorage != RELSTORAGE_AOROWS &&
					   relstorage != RELSTORAGE_AOCOLS);
#endif

				splan->scan.plan.targetlist =
					fix_scan_list(glob, splan->scan.plan.targetlist, rtoffset);
				splan->scan.plan.qual =
					fix_scan_list(glob, splan->scan.plan.qual, rtoffset);
				splan->indexqual =
					fix_scan_list(glob, splan->indexqual, rtoffset);
				splan->indexqualorig =
					fix_scan_list(glob, splan->indexqualorig, rtoffset);
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
					fix_scan_list(glob, splan->indexqual, rtoffset);
				splan->indexqualorig =
					fix_scan_list(glob, splan->indexqualorig, rtoffset);
			}
			break;
		case T_BitmapHeapScan:
			{
				BitmapHeapScan *splan = (BitmapHeapScan *) plan;

				if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
					return cdb_insert_result_node(glob, plan, rtoffset);

				splan->scan.scanrelid += rtoffset;

#ifdef USE_ASSERT_CHECKING
				RangeTblEntry *rte = rt_fetch(splan->scan.scanrelid, glob->finalrtable);
				char relstorage = get_rel_relstorage(rte->relid);
				Assert(relstorage != RELSTORAGE_AOROWS &&
					   relstorage != RELSTORAGE_AOCOLS);
#endif
				splan->scan.plan.targetlist =
					fix_scan_list(glob, splan->scan.plan.targetlist, rtoffset);
				splan->scan.plan.qual =
					fix_scan_list(glob, splan->scan.plan.qual, rtoffset);
				splan->bitmapqualorig =
					fix_scan_list(glob, splan->bitmapqualorig, rtoffset);
			}
			break;
		case T_BitmapAppendOnlyScan:
			{
				BitmapAppendOnlyScan *splan = (BitmapAppendOnlyScan *) plan;

				if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
					return cdb_insert_result_node(glob, plan, rtoffset);

				splan->scan.scanrelid += rtoffset;

#ifdef USE_ASSERT_CHECKING
				RangeTblEntry *rte = rt_fetch(splan->scan.scanrelid, glob->finalrtable);
				char relstorage = get_rel_relstorage(rte->relid);
				Assert(relstorage == RELSTORAGE_AOROWS ||
					   relstorage == RELSTORAGE_AOCOLS);
#endif

				splan->scan.plan.targetlist =
					fix_scan_list(glob, splan->scan.plan.targetlist, rtoffset);
				splan->scan.plan.qual =
					fix_scan_list(glob, splan->scan.plan.qual, rtoffset);
				splan->bitmapqualorig =
					fix_scan_list(glob, splan->bitmapqualorig, rtoffset);
			}
			break;
		case T_BitmapTableScan:
			{
				BitmapTableScan *splan = (BitmapTableScan *) plan;

				if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
					return cdb_insert_result_node(glob, plan, rtoffset);

				splan->scan.scanrelid += rtoffset;

				splan->scan.plan.targetlist =
					fix_scan_list(glob, splan->scan.plan.targetlist, rtoffset);
				splan->scan.plan.qual =
					fix_scan_list(glob, splan->scan.plan.qual, rtoffset);
				splan->bitmapqualorig =
					fix_scan_list(glob, splan->bitmapqualorig, rtoffset);
			}
			break;
		case T_TidScan:
			{
				TidScan    *splan = (TidScan *) plan;

				if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
					return cdb_insert_result_node(glob, plan, rtoffset);

				splan->scan.scanrelid += rtoffset;

#ifdef USE_ASSERT_CHECKING
				/* We only support TidScans on heap tables currently */
				RangeTblEntry *rte = rt_fetch(splan->scan.scanrelid, glob->finalrtable);
				char relstorage = get_rel_relstorage(rte->relid);
				Assert(relstorage == RELSTORAGE_HEAP);
#endif

				splan->scan.plan.targetlist =
					fix_scan_list(glob, splan->scan.plan.targetlist, rtoffset);
				splan->scan.plan.qual =
					fix_scan_list(glob, splan->scan.plan.qual, rtoffset);
				splan->tidquals =
					fix_scan_list(glob, splan->tidquals, rtoffset);
			}
			break;
		case T_SubqueryScan:

			if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
				return cdb_insert_result_node(glob, plan, rtoffset);

			/* Needs special treatment, see comments below */
			return set_subqueryscan_references(glob,
											   (SubqueryScan *) plan,
											   rtoffset);
		case T_TableFunctionScan:
			{
				TableFunctionScan	*tplan	   = (TableFunctionScan *) plan;
				Plan				*subplan   = tplan->scan.plan.lefttree;
				List				*subrtable = tplan->subrtable;

				if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
					return cdb_insert_result_node(glob, plan, rtoffset);

				/* recursively process the subplan */
				plan->lefttree = set_plan_references(glob, subplan, subrtable);

				/* subrtable is no longer needed in the plan tree */
				tplan->subrtable = NIL;

				/* adjust for the new range table offset */
				tplan->scan.scanrelid += rtoffset;
				tplan->scan.plan.targetlist =
					fix_scan_list(glob, tplan->scan.plan.targetlist, rtoffset);
				tplan->scan.plan.qual =
					fix_scan_list(glob, tplan->scan.plan.qual, rtoffset);

				return plan;
			}
		case T_FunctionScan:
			{
				FunctionScan *splan = (FunctionScan *) plan;

				if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
					return cdb_insert_result_node(glob, plan, rtoffset);

				splan->scan.scanrelid += rtoffset;
				splan->scan.plan.targetlist =
					fix_scan_list(glob, splan->scan.plan.targetlist, rtoffset);
				splan->scan.plan.qual =
					fix_scan_list(glob, splan->scan.plan.qual, rtoffset);
				splan->funcexpr =
					fix_scan_expr(glob, splan->funcexpr, rtoffset);
			}
			break;
		case T_ValuesScan:
			{
				ValuesScan *splan = (ValuesScan *) plan;

				if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
					return cdb_insert_result_node(glob, plan, rtoffset);

				splan->scan.scanrelid += rtoffset;
				splan->scan.plan.targetlist =
					fix_scan_list(glob, splan->scan.plan.targetlist, rtoffset);
				splan->scan.plan.qual =
					fix_scan_list(glob, splan->scan.plan.qual, rtoffset);
				splan->values_lists =
					fix_scan_list(glob, splan->values_lists, rtoffset);
			}
			break;
		case T_CteScan:
			{
				CteScan    *splan = (CteScan *) plan;

				splan->scan.scanrelid += rtoffset;
				splan->scan.plan.targetlist =
					fix_scan_list(glob, splan->scan.plan.targetlist, rtoffset);
				splan->scan.plan.qual =
					fix_scan_list(glob, splan->scan.plan.qual, rtoffset);
			}
			break;
		case T_WorkTableScan:
			{
				WorkTableScan *splan = (WorkTableScan *) plan;

				splan->scan.scanrelid += rtoffset;
				splan->scan.plan.targetlist =
					fix_scan_list(glob, splan->scan.plan.targetlist, rtoffset);
				splan->scan.plan.qual =
					fix_scan_list(glob, splan->scan.plan.qual, rtoffset);
			}
			break;
		case T_NestLoop:
		case T_MergeJoin:
		case T_HashJoin:
			if (cdb_expr_requires_full_eval((Node *)plan->targetlist))
				return cdb_insert_result_node(glob, plan, rtoffset);
			set_join_references(glob, (Join *) plan, rtoffset);
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

		case T_Sort:
		case T_Hash:
		case T_Material:
		case T_Unique:
		case T_SetOp:

			/*
			 * These plan types don't actually bother to evaluate their
			 * targetlists, because they just return their unmodified input
			 * tuples.	Even though the targetlist won't be used by the
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
					Assert(sisc->share_id >= 0 && sisc->share_id < glob->share.producer_count);
					producer = glob->share.producers[sisc->share_id];
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

				set_upper_references(glob, plan, rtoffset);

				//set_dummy_tlist_references(plan, rtoffset);
				Assert(ps->plan.qual == NIL);

				ps->levelEqExpressions = (List *)
					fix_upper_expr(glob, (Node *) ps->levelEqExpressions, childplan_itlist, rtoffset);
				ps->levelExpressions = (List *)
					fix_upper_expr(glob, (Node *) ps->levelExpressions, childplan_itlist, rtoffset);
				ps->residualPredicate =
					fix_upper_expr(glob, ps->residualPredicate, childplan_itlist, rtoffset);
				ps->propagationExpression =
					fix_upper_expr(glob, ps->propagationExpression, childplan_itlist, rtoffset);
				ps->printablePredicate =
					fix_upper_expr(glob, ps->printablePredicate, childplan_itlist, rtoffset);
				ps->partTabTargetlist = (List *)
					fix_upper_expr(glob, (Node *) ps->partTabTargetlist, childplan_itlist, rtoffset);
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
					fix_scan_expr(glob, splan->limitOffset, rtoffset);
				splan->limitCount =
					fix_scan_expr(glob, splan->limitCount, rtoffset);
			}
			break;
		case T_Agg:
			set_upper_references(glob, plan, rtoffset);
			break;
		case T_WindowAgg:
			{
				WindowAgg  *wplan = (WindowAgg *) plan;
				indexed_tlist  *subplan_itlist;

				set_upper_references(glob, plan, rtoffset);

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
						fix_upper_expr(glob, wplan->startOffset,
									   subplan_itlist, rtoffset);
					wplan->endOffset =
						fix_upper_expr(glob, wplan->endOffset,
									   subplan_itlist, rtoffset);
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
				{
					set_upper_references(glob, plan, rtoffset);
				}
				splan->plan.targetlist =
					fix_scan_list(glob, splan->plan.targetlist, rtoffset);
				splan->plan.qual =
					fix_scan_list(glob, splan->plan.qual, rtoffset);

				/* resconstantqual can't contain any subplan variable refs */
				splan->resconstantqual =
					fix_scan_expr(glob, splan->resconstantqual, rtoffset);
			}
			break;
		case T_Repeat:
			set_upper_references(glob, plan, rtoffset);
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
					lfirst(l) = set_plan_refs(glob,
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
					lfirst(l) = set_plan_refs(glob,
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
					lfirst(l) = set_plan_refs(glob,
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

				motion->hashExpr = (List *)
					fix_upper_expr(glob, (Node*) motion->hashExpr, childplan_itlist, rtoffset);

#ifdef USE_ASSERT_CHECKING
				/* 1. Assert that the Motion node has same number of hash data types as that of hash expressions*/
				/* 2. Motion node must have atleast one hash expression */
				/* 3. If the Motion node is of type hash_motion: ensure that the expression that it is hashed on is a hashable datatype in gpdb*/

				Assert(list_length(motion->hashExpr) == list_length(motion->hashDataTypes)  && "Number of hash expression not equal to number of hash data types!");

				if (MOTIONTYPE_HASH == motion->motionType)
				{
					Assert(1 <= list_length(motion->hashExpr) && "Motion node must have atleast one hash expression!");

					ListCell *lcNode;
					foreach(lcNode, motion->hashExpr)
					{
						Assert(isGreenplumDbHashable(exprType((Node *) lfirst(lcNode)))  && "The expression is not GPDB hashable!");
					}
				}

#endif			/* USE_ASSERT_CHECKING */

				/* no need to fix targetlist and qual */
				Assert(plan->qual == NIL);
				set_dummy_tlist_references(plan, rtoffset);
				pfree(childplan_itlist);
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
	plan->lefttree = set_plan_refs(glob, plan->lefttree, rtoffset);
	plan->righttree = set_plan_refs(glob, plan->righttree, rtoffset);

	return plan;
}

/*
 * set_subqueryscan_references
 *		Do set_plan_references processing on a SubqueryScan
 *
 * We try to strip out the SubqueryScan entirely; if we can't, we have
 * to do the normal processing on it.
 */
static Plan *
set_subqueryscan_references(PlannerGlobal *glob,
							SubqueryScan *plan,
							int rtoffset)
{
	Plan	   *result;

	/* First, recursively process the subplan */
	plan->subplan = set_plan_references(glob, plan->subplan, plan->subrtable);

	/* subrtable is no longer needed in the plan tree */
	plan->subrtable = NIL;

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
	}
	else
	{
		/*
		 * Keep the SubqueryScan node.	We have to do the processing that
		 * set_plan_references would otherwise have done on it.  Notice we do
		 * not do set_upper_references() here, because a SubqueryScan will
		 * always have been created with correct references to its subplan's
		 * outputs to begin with.
		 */
		plan->scan.scanrelid += rtoffset;

		//Assert(plan->scan.scanrelid <= list_length(glob->finalrtable) && "Scan node's relid is outside the finalrtable!");

		plan->scan.plan.targetlist =
			fix_scan_list(glob, plan->scan.plan.targetlist, rtoffset);
		plan->scan.plan.qual =
			fix_scan_list(glob, plan->scan.plan.qual, rtoffset);

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
 * add OIDs from regclass Const nodes into glob->relationOids,
 * and add catalog TIDs for user-defined functions into glob->invalItems.
 *
 * We assume it's okay to update opcode info in-place.  So this could possibly
 * scribble on the planner's input data structures, but it's OK.
 */
static void
fix_expr_common(PlannerGlobal *glob, Node *node)
{
	/* We assume callers won't call us on a NULL pointer */
	if (IsA(node, Aggref))
	{
		record_plan_function_dependency(glob,
										((Aggref *) node)->aggfnoid);
	}
	else if (IsA(node, WindowFunc))
	{
		record_plan_function_dependency(glob,
										((WindowFunc *) node)->winfnoid);
	}
	else if (IsA(node, FuncExpr))
	{
		record_plan_function_dependency(glob,
										((FuncExpr *) node)->funcid);
	}
	else if (IsA(node, OpExpr))
	{
		set_opfuncid((OpExpr *) node);
		record_plan_function_dependency(glob,
										((OpExpr *) node)->opfuncid);
	}
	else if (IsA(node, DistinctExpr))
	{
		set_opfuncid((OpExpr *) node);	/* rely on struct equivalence */
		record_plan_function_dependency(glob,
										((DistinctExpr *) node)->opfuncid);
	}
	else if (IsA(node, NullIfExpr))
	{
		set_opfuncid((OpExpr *) node);	/* rely on struct equivalence */
		record_plan_function_dependency(glob,
										((NullIfExpr *) node)->opfuncid);
	}
	else if (IsA(node, ScalarArrayOpExpr))
	{
		set_sa_opfuncid((ScalarArrayOpExpr *) node);
		record_plan_function_dependency(glob,
									 ((ScalarArrayOpExpr *) node)->opfuncid);
	}
	else if (IsA(node, ArrayCoerceExpr))
	{
		if (OidIsValid(((ArrayCoerceExpr *) node)->elemfuncid))
			record_plan_function_dependency(glob,
									 ((ArrayCoerceExpr *) node)->elemfuncid);
	}
	else if (IsA(node, Const))
	{
		Const	   *con = (Const *) node;

		/* Check for regclass reference */
		if (ISREGCLASSCONST(con))
			glob->relationOids =
				lappend_oid(glob->relationOids,
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
                   var->varno <= list_length(glob->finalrtable));
        }
    }
}

/*
 * fix_scan_expr
 *		Do set_plan_references processing on a scan-level expression
 *
 * This consists of incrementing all Vars' varnos by rtoffset,
 * looking up operator opcode info for OpExpr and related nodes,
 * and adding OIDs from regclass Const nodes into glob->relationOids.
 *
 * TODO: rename to reflect functionality more accurately.
 */
static Node *
fix_scan_expr(PlannerGlobal *glob, Node *node, int rtoffset)
{
	fix_scan_expr_context context;

	context.glob = glob;
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
		 * We should not see any Vars marked INNER, but in a nestloop inner
		 * scan there could be OUTER Vars.	Leave them alone.
		 */
		Assert(var->varno != INNER);
		if (var->varno > 0 && var->varno != OUTER)
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
            rte = rt_fetch(var->varno, context->glob->finalrtable);
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
	if (IsA(node, PlaceHolderVar))
	{
		/* At scan level, we should always just evaluate the contained expr */
		PlaceHolderVar *phv = (PlaceHolderVar *) node;

		return fix_scan_expr_mutator((Node *) phv->phexpr, context);
	}
	fix_expr_common(context->glob, node);
	return expression_tree_mutator(node, fix_scan_expr_mutator,
								   (void *) context);
}

static bool
fix_scan_expr_walker(Node *node, fix_scan_expr_context *context)
{
	if (node == NULL)
		return false;
	Assert(!IsA(node, PlaceHolderVar));

	/*
	 * fix_expr_common will look up and set operator opcodes in the
	 * nodes. That's not needed, as ORCA has set those already, but
	 * shouldn't do any harm either.
	 */
	fix_expr_common(context->glob, node);
	return expression_tree_walker(node, fix_scan_expr_walker,
								  (void *) context);
}

/*
 * set_join_references
 *	  Modify the target list and quals of a join node to reference its
 *	  subplans, by setting the varnos to OUTER or INNER and setting attno
 *	  values to the result domain number of either the corresponding outer
 *	  or inner join tuple item.  Also perform opcode lookup for these
 *	  expressions. and add regclass OIDs to glob->relationOids.
 *
 * In the case of a nestloop with inner indexscan, we will also need to
 * apply the same transformation to any outer vars appearing in the
 * quals of the child indexscan.  set_inner_join_references does that.
 */
static void
set_join_references(PlannerGlobal *glob, Join *join, int rtoffset)
{
	Plan	   *outer_plan = join->plan.lefttree;
	Plan	   *inner_plan = join->plan.righttree;
	indexed_tlist *outer_itlist;
	indexed_tlist *inner_itlist;

	outer_itlist = build_tlist_index(outer_plan->targetlist);
	inner_itlist = build_tlist_index(inner_plan->targetlist);

	/* All join plans have tlist, qual, and joinqual */
	join->plan.targetlist = fix_join_expr(glob,
										  join->plan.targetlist,
										  outer_itlist,
										  inner_itlist,
										  (Index) 0,
										  rtoffset);
	join->plan.qual = fix_join_expr(glob,
									join->plan.qual,
									outer_itlist,
									inner_itlist,
									(Index) 0,
									rtoffset);
	join->joinqual = fix_join_expr(glob,
								   join->joinqual,
								   outer_itlist,
								   inner_itlist,
								   (Index) 0,
								   rtoffset);

	/* Now do join-type-specific stuff */
	if (IsA(join, NestLoop))
	{
		/* This processing is split out to handle possible recursion */
		set_inner_join_references(glob, inner_plan, outer_itlist);
	}
	else if (IsA(join, MergeJoin))
	{
		MergeJoin  *mj = (MergeJoin *) join;

		mj->mergeclauses = fix_join_expr(glob,
										 mj->mergeclauses,
										 outer_itlist,
										 inner_itlist,
										 (Index) 0,
										 rtoffset);
	}
	else if (IsA(join, HashJoin))
	{
		HashJoin   *hj = (HashJoin *) join;

		hj->hashclauses = fix_hashclauses(glob,
										hj->hashclauses,
										outer_itlist,
										inner_itlist,
										(Index) 0,
										rtoffset);

		hj->hashqualclauses = fix_join_expr(glob,
											hj->hashqualclauses,
											outer_itlist,
											inner_itlist,
											(Index) 0,
											rtoffset);
	}

	pfree(outer_itlist);
	pfree(inner_itlist);
}

/*
 * set_inner_join_references
 *		Handle join references appearing in an inner indexscan's quals
 *
 * To handle bitmap-scan plan trees, we have to be able to recurse down
 * to the bottom BitmapIndexScan nodes; likewise, appendrel indexscans
 * require recursing through Append nodes.	This is split out as a separate
 * function so that it can recurse.
 *
 * Note we do *not* apply any rtoffset for non-join Vars; this is because
 * the quals will be processed again by fix_scan_expr when the set_plan_refs
 * recursion reaches the inner indexscan, and so we'd have done it twice.
 */
static void
set_inner_join_references(PlannerGlobal *glob, Plan *inner_plan,
						  indexed_tlist *outer_itlist)
{
	if (IsA(inner_plan, IndexScan))
	{
		/*
		 * An index is being used to reduce the number of tuples scanned in
		 * the inner relation.	If there are join clauses being used with the
		 * index, we must update their outer-rel var nodes to refer to the
		 * outer side of the join.
		 */
		IndexScan  *innerscan = (IndexScan *) inner_plan;
		List	   *indexqualorig = innerscan->indexqualorig;

		/* No work needed if indexqual refers only to its own rel... */
		if (NumRelids((Node *) indexqualorig) > 1)
		{
			Index		innerrel = innerscan->scan.scanrelid;

			/* only refs to outer vars get changed in the inner qual */
			innerscan->indexqualorig = fix_join_expr(glob,
													 indexqualorig,
													 outer_itlist,
													 NULL,
													 innerrel,
													 0);
			innerscan->indexqual = fix_join_expr(glob,
												 innerscan->indexqual,
												 outer_itlist,
												 NULL,
												 innerrel,
												 0);

			/*
			 * We must fix the inner qpqual too, if it has join clauses (this
			 * could happen if special operators are involved: some indexquals
			 * may get rechecked as qpquals).
			 */
			if (NumRelids((Node *) inner_plan->qual) > 1)
				inner_plan->qual = fix_join_expr(glob,
												 inner_plan->qual,
												 outer_itlist,
												 NULL,
												 innerrel,
												 0);
		}
	}
	else if (IsA(inner_plan, BitmapIndexScan))
	{
		/*
		 * Same, but index is being used within a bitmap plan.
		 */
		BitmapIndexScan *innerscan = (BitmapIndexScan *) inner_plan;
		List	   *indexqualorig = innerscan->indexqualorig;

		/* No work needed if indexqual refers only to its own rel... */
		if (NumRelids((Node *) indexqualorig) > 1)
		{
			Index		innerrel = innerscan->scan.scanrelid;

			/* only refs to outer vars get changed in the inner qual */
			innerscan->indexqualorig = fix_join_expr(glob,
													 indexqualorig,
													 outer_itlist,
													 NULL,
													 innerrel,
													 0);
			innerscan->indexqual = fix_join_expr(glob,
												 innerscan->indexqual,
												 outer_itlist,
												 NULL,
												 innerrel,
												 0);
			/* no need to fix inner qpqual */
			Assert(inner_plan->qual == NIL);
		}
	}
	else if (IsA(inner_plan, BitmapHeapScan) ||
			 IsA(inner_plan, BitmapAppendOnlyScan))
	{
		/*
		 * The inner side is a bitmap scan plan.  Fix the top node, and
		 * recurse to get the lower nodes.
		 *
		 * Note: create_bitmap_scan_plan removes clauses from bitmapqualorig
		 * if they are duplicated in qpqual, so must test these independently.
		 */
		Index innerrel;
		List **bitmapqualorig_p;
		if (IsA(inner_plan, BitmapHeapScan))
		{
			BitmapHeapScan *innerscan = (BitmapHeapScan *) inner_plan;
			innerrel = innerscan->scan.scanrelid;
			bitmapqualorig_p = &(innerscan->bitmapqualorig);
		}
		else
		{
			Assert(IsA(inner_plan, BitmapAppendOnlyScan));

			BitmapAppendOnlyScan *innerscan = (BitmapAppendOnlyScan *) inner_plan;
			innerrel = innerscan->scan.scanrelid;
			bitmapqualorig_p = &(innerscan->bitmapqualorig);
		}

		/* only refs to outer vars get changed in the inner qual */
		if (NumRelids((Node *) (*bitmapqualorig_p)) > 1)
			*bitmapqualorig_p = fix_join_expr(glob,
											  *bitmapqualorig_p,
											  outer_itlist,
											  NULL,
											  innerrel,
											  0);

		/*
		 * We must fix the inner qpqual too, if it has join clauses (this
		 * could happen if special operators are involved: some indexquals may
		 * get rechecked as qpquals).
		 */
		if (NumRelids((Node *) inner_plan->qual) > 1)
			inner_plan->qual = fix_join_expr(glob,
											 inner_plan->qual,
											 outer_itlist,
											 NULL,
											 innerrel,
											 0);

		/* Now recurse */
		set_inner_join_references(glob, inner_plan->lefttree, outer_itlist);
	}
	else if (IsA(inner_plan, BitmapAnd))
	{
		/* All we need do here is recurse */
		BitmapAnd  *innerscan = (BitmapAnd *) inner_plan;
		ListCell   *l;

		foreach(l, innerscan->bitmapplans)
		{
			set_inner_join_references(glob, (Plan *) lfirst(l), outer_itlist);
		}
	}
	else if (IsA(inner_plan, BitmapOr))
	{
		/* All we need do here is recurse */
		BitmapOr   *innerscan = (BitmapOr *) inner_plan;
		ListCell   *l;

		foreach(l, innerscan->bitmapplans)
		{
			set_inner_join_references(glob, (Plan *) lfirst(l), outer_itlist);
		}
	}
	else if (IsA(inner_plan, TidScan))
	{
		TidScan    *innerscan = (TidScan *) inner_plan;
		Index		innerrel = innerscan->scan.scanrelid;

		innerscan->tidquals = fix_join_expr(glob,
											innerscan->tidquals,
											outer_itlist,
											NULL,
											innerrel,
											0);
	}
	else if (IsA(inner_plan, Append))
	{
		/*
		 * The inner side is an append plan.  Recurse to see if it contains
		 * indexscans that need to be fixed.
		 */
		Append	   *appendplan = (Append *) inner_plan;
		ListCell   *l;

		foreach(l, appendplan->appendplans)
		{
			set_inner_join_references(glob, (Plan *) lfirst(l), outer_itlist);
		}
	}
	else if (IsA(inner_plan, Result))
	{
		/* Recurse through a gating Result node (similar to Append case) */
		Result	   *result = (Result *) inner_plan;

		if (result->plan.lefttree)
			set_inner_join_references(glob, result->plan.lefttree, outer_itlist);
	}
}

/*
 * set_upper_references
 *	  Update the targetlist and quals of an upper-level plan node
 *	  to refer to the tuples returned by its lefttree subplan.
 *	  Also perform opcode lookup for these expressions, and
 *	  add regclass OIDs to glob->relationOids.
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
set_upper_references(PlannerGlobal *glob, Plan *plan, int rtoffset)
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

		if (IsA(tle->expr, Grouping) ||
				IsA(tle->expr, GroupId))
			newexpr = copyObject(tle->expr);
		else
			newexpr = fix_upper_expr(glob,
					(Node *) tle->expr,
					subplan_itlist,
					rtoffset);
		tle = flatCopyTargetEntry(tle);
		tle->expr = (Expr *) newexpr;
		output_targetlist = lappend(output_targetlist, tle);
	}
	plan->targetlist = output_targetlist;

	plan->qual = (List *)
		fix_upper_expr(glob,
					   (Node *) plan->qual,
					   subplan_itlist,
					   rtoffset);

	pfree(subplan_itlist);
}

/*
 * set_dummy_tlist_references
 *	  Replace the targetlist of an upper-level plan node with a simple
 *	  list of OUTER references to its child.
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

		newvar = makeVar(OUTER,
						 tle->resno,
						 exprType((Node *) oldvar),
						 exprTypmod((Node *) oldvar),
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
 * in advance.	Matching a parent tlist to a child is still an O(N^2)
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
 * itlist->has_non_vars
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

		newvar = makeVar(newvarno,
						 tle->resno,
						 exprType((Node *) tle->expr),
						 exprTypmod((Node *) tle->expr),
						 0);
		newvar->varnoold = 0;	/* wasn't ever a plain Var */
		newvar->varoattno = 0;
		return newvar;
	}
	return NULL;				/* no match */
}

/*
 * fix_join_expr
 *	   Create a new set of targetlist entries or join qual clauses by
 *	   changing the varno/varattno values of variables in the clauses
 *	   to reference target list values from the outer and inner join
 *	   relation target lists.  Also perform opcode lookup and add
 *	   regclass OIDs to glob->relationOids.
 *
 * This is used in two different scenarios: a normal join clause, where
 * all the Vars in the clause *must* be replaced by OUTER or INNER references;
 * and an indexscan being used on the inner side of a nestloop join.
 * In the latter case we want to replace the outer-relation Vars by OUTER
 * references, while Vars of the inner relation should be returned without change
 * (those will later be adjusted in fix_scan_list).
 * (We also implement RETURNING clause fixup using this second scenario.)
 *
 * For a normal join, acceptable_rel should be zero so that any failure to
 * match a Var will be reported as an error.  For the indexscan case,
 * pass inner_itlist = NULL and acceptable_rel = the (not-offseted-yet) ID
 * of the inner relation.
 *
 * 'clauses' is the targetlist or list of join clauses
 * 'outer_itlist' is the indexed target list of the outer join relation
 * 'inner_itlist' is the indexed target list of the inner join relation,
 *		or NULL
 * 'acceptable_rel' is either zero or the rangetable index of a relation
 *		whose Vars may appear in the clause without provoking an error.
 * 'rtoffset' is what to add to varno for Vars of relations other than acceptable_rel.
 *
 * Returns the new expression tree.  The original clause structure is
 * not modified.
 */
static List *
fix_join_expr(PlannerGlobal *glob,
			  List *clauses,
			  indexed_tlist *outer_itlist,
			  indexed_tlist *inner_itlist,
			  Index acceptable_rel,
			  int rtoffset)
{
	fix_join_expr_context context;

	context.glob = glob;
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
static List *fix_hashclauses(PlannerGlobal *glob,
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
        List *new_outer_arg = fix_child_hashclauses(glob,
                outer_arg,
                outer_itlist,
                inner_itlist,
                (Index) 0,
                rtoffset,
                OUTER);
        /*
         * for inner argument, we cannot refer to target entries
         * in join's outer child target list, otherwise hash table
         * creation could fail,
         * we change walker's context to guarantee this
         */
        List *new_inner_arg = fix_child_hashclauses(glob,
                inner_arg,
                outer_itlist,
                inner_itlist,
                (Index) 0,
                rtoffset,
                INNER);
        new_args = lappend(new_args, new_outer_arg);
        new_args = lappend(new_args, new_inner_arg);
        /* replace old arguments with the fixed arguments */
        list_free(opexpr->args);
        opexpr->args = new_args;
        /* fix opexpr */
        fix_expr_common(glob, node);
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
fix_child_hashclauses(PlannerGlobal *glob,
              List *clauses,
              indexed_tlist *outer_itlist,
              indexed_tlist *inner_itlist,
              Index acceptable_rel,
              int rtoffset,
              Index child)
{
    fix_join_expr_context context;
    context.glob = glob;
    context.outer_itlist = outer_itlist;
    context.inner_itlist = inner_itlist;
    context.acceptable_rel = acceptable_rel;
    context.rtoffset = rtoffset;
    if (INNER == child)
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
											  OUTER,
											  context->rtoffset);
		if (newvar)
			return (Node *) newvar;
		if (context->inner_itlist)
		{
			newvar = search_indexed_tlist_for_var(var,
												  context->inner_itlist,
												  INNER,
												  context->rtoffset);
			if (newvar)
				return (Node *) newvar;
		}

		/* If it's for an acceptable_rel (the inner relation in an index nested loop join), return it */
		if (var->varno == context->acceptable_rel)
		{
			var = copyVar(var);
			var->varno += context->rtoffset;
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
													  OUTER);
			if (newvar)
				return (Node *) newvar;
		}
		if (context->inner_itlist && context->inner_itlist->has_ph_vars)
		{
			newvar = search_indexed_tlist_for_non_var((Node *) phv,
													  context->inner_itlist,
													  INNER);
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
												  OUTER);
		if (newvar)
			return (Node *) newvar;
	}
	if (context->inner_itlist && context->inner_itlist->has_non_vars &&
	        context->use_inner_tlist_for_matching_nonvars)

	if (context->inner_itlist && context->inner_itlist->has_non_vars)
	{
		newvar = search_indexed_tlist_for_non_var(node,
												  context->inner_itlist,
												  INNER);
		if (newvar)
			return (Node *) newvar;
	}
	fix_expr_common(context->glob, node);
	return expression_tree_mutator(node,
								   fix_join_expr_mutator,
								   (void *) context);
}

/*
 * fix_upper_expr
 *		Modifies an expression tree so that all Var nodes reference outputs
 *		of a subplan.  Also performs opcode lookup, and adds regclass OIDs to
 *		glob->relationOids.
 *
 * This is used to fix up target and qual expressions of non-join upper-level
 * plan nodes.
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
 * 'subplan_itlist': indexed target list for subplan
 * 'rtoffset': how much to increment varnoold by
 *
 * The resulting tree is a copy of the original in which all Var nodes have
 * varno = OUTER, varattno = resno of corresponding subplan target.
 * The original tree is not modified.
 */
static Node *
fix_upper_expr(PlannerGlobal *glob,
			   Node *node,
			   indexed_tlist *subplan_itlist,
			   int rtoffset)
{
	fix_upper_expr_context context;

	context.glob = glob;
	context.subplan_itlist = subplan_itlist;
	context.rtoffset = rtoffset;
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
											  OUTER,
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
													  OUTER);
			if (newvar)
				return (Node *) newvar;
		}
		/* If not supplied by input plan, evaluate the contained expr */
		return fix_upper_expr_mutator((Node *) phv->phexpr, context);
	}
	/* Try matching more complex expressions too, if tlist has any */
	if (context->subplan_itlist->has_non_vars && !IsA(node, GroupId))
	{
		newvar = search_indexed_tlist_for_non_var(node,
												  context->subplan_itlist,
												  OUTER);
		if (newvar)
			return (Node *) newvar;
	}
	fix_expr_common(context->glob, node);
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
 * entries in the top plan's targetlist.  Vars referencing the result
 * table should be left alone, however (the executor will evaluate them
 * using the actual heap tuple, after firing triggers if any).	In the
 * adjusted RETURNING list, result-table Vars will still have their
 * original varno, but Vars for other rels will have varno OUTER.
 *
 * We also must perform opcode lookup and add regclass OIDs to
 * glob->relationOids.
 *
 * 'rlist': the RETURNING targetlist to be fixed
 * 'topplan': the top Plan node for the query (not yet passed through
 *		set_plan_references)
 * 'resultRelation': RT index of the associated result relation
 *
 * Note: we assume that result relations will have rtoffset zero, that is,
 * they are not coming from a subplan.
 */
List *
set_returning_clause_references(PlannerGlobal *glob,
								List *rlist,
								Plan *topplan,
								Index resultRelation)
{
	indexed_tlist *itlist;

	/*
	 * We can perform the desired Var fixup by abusing the fix_join_expr
	 * machinery that normally handles inner indexscan fixup.  We search the
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

	rlist = fix_join_expr(glob,
						  rlist,
						  itlist,
						  NULL,
						  resultRelation,
						  0);

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
record_plan_function_dependency(PlannerGlobal *glob, Oid funcid)
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
		HeapTuple	func_tuple;
		PlanInvalItem *inval_item;

		func_tuple = SearchSysCache(PROCOID,
									ObjectIdGetDatum(funcid),
									0, 0, 0);
		if (!HeapTupleIsValid(func_tuple))
			elog(ERROR, "cache lookup failed for function %u", funcid);

		inval_item = makeNode(PlanInvalItem);

		/*
		 * It would work to use any syscache on pg_proc, but plancache.c
		 * expects us to use PROCOID.
		 */
		inval_item->cacheId = PROCOID;
		inval_item->tupleId = func_tuple->t_self;

		glob->invalItems = lappend(glob->invalItems, inval_item);

		ReleaseSysCache(func_tuple);
	}
}

/*
 * extract_query_dependencies
 *		Given a list of not-yet-planned queries (i.e. Query nodes),
 *		extract their dependencies just as set_plan_references would do.
 *
 * This is needed by plancache.c to handle invalidation of cached unplanned
 * queries.
 */
void
extract_query_dependencies(List *queries,
						   List **relationOids,
						   List **invalItems)
{
	PlannerGlobal glob;

	/* Make up a dummy PlannerGlobal so we can use this module's machinery */
	MemSet(&glob, 0, sizeof(glob));
	glob.type = T_PlannerGlobal;
	glob.relationOids = NIL;
	glob.invalItems = NIL;

	(void) extract_query_dependencies_walker((Node *) queries, &glob);

	*relationOids = glob.relationOids;
	*invalItems = glob.invalItems;
}

static bool
extract_query_dependencies_walker(Node *node, PlannerGlobal *context)
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

		/* Collect relation OIDs in this Query's rtable */
		foreach(lc, query->rtable)
		{
			RangeTblEntry *rte = (RangeTblEntry *) lfirst(lc);

			if (rte->rtekind == RTE_RELATION)
				context->relationOids = lappend_oid(context->relationOids,
													rte->relid);
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
cdb_extract_plan_dependencies(PlannerGlobal *glob, Plan *plan)
{
	cdb_extract_plan_dependencies_context context;

	context.base.node = (Node *) glob;
	context.glob = glob;

	(void) cdb_extract_plan_dependencies_walker((Node *) plan, &context);
}

static bool
cdb_extract_plan_dependencies_walker(Node *node, cdb_extract_plan_dependencies_context *context)
{
	if (node == NULL)
		return false;
	/* Extract function dependencies and check for regclass Consts */
	fix_expr_common(context->glob, node);

	return plan_tree_walker(node, cdb_extract_plan_dependencies_walker,
								  (void *) context);
}

/*
 * cdb_expr_requires_full_eval
 *
 * Returns true if expr could call a set-returning function.
 */
bool
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
Plan *
cdb_insert_result_node(PlannerGlobal *glob, Plan *plan, int rtoffset)
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
    resultplan = set_plan_refs(glob, resultplan, rtoffset);

    /* Reattach the Flow node. */
    resultplan->flow = flow;
	plan->flow = flow;

    return resultplan;
}                               /* cdb_insert_result_node */
