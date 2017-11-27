/*-------------------------------------------------------------------------
 *
 * createplan.c
 *	  Routines to create the desired plan for processing a query.
 *	  Planning is complete, we just need to convert the selected
 *	  Path into a Plan.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/optimizer/plan/createplan.c,v 1.255 2009/01/01 17:23:44 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>
#include <math.h>

#include "catalog/pg_exttable.h"
#include "catalog/pg_type.h"	/* INT8OID */
#include "access/skey.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "executor/execHHashagg.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/plancat.h"
#include "optimizer/planmain.h"
#include "optimizer/planpartition.h"
#include "optimizer/predtest.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/tlist.h"
#include "optimizer/var.h"
#include "parser/parse_clause.h"
#include "parser/parsetree.h"
#include "parser/parse_oper.h"	/* ordering_oper_opid */
#include "utils/lsyscache.h"
#include "utils/uri.h"

#include "cdb/cdbllize.h"		/* pull_up_Flow() */
#include "cdb/cdbpath.h"		/* cdbpath_rows() */
#include "cdb/cdbpathtoplan.h"	/* cdbpathtoplan_create_flow() etc. */
#include "cdb/cdbpullup.h"		/* cdbpullup_targetlist() */
#include "cdb/cdbsreh.h"
#include "cdb/cdbvars.h"


static Plan *create_subplan(PlannerInfo *root, Path *best_path);		/* CDB */
static Plan *create_scan_plan(PlannerInfo *root, Path *best_path);
static bool use_physical_tlist(PlannerInfo *root, RelOptInfo *rel);
static void disuse_physical_tlist(Plan *plan, Path *path);
static Plan *create_gating_plan(PlannerInfo *root, Plan *plan, List *quals);
static Plan *create_join_plan(PlannerInfo *root, JoinPath *best_path);
static Plan *create_append_plan(PlannerInfo *root, AppendPath *best_path);
static Result *create_result_plan(PlannerInfo *root, ResultPath *best_path);
static Material *create_material_plan(PlannerInfo *root, MaterialPath *best_path);
static Plan *create_unique_plan(PlannerInfo *root, UniquePath *best_path);
static Plan *create_motion_plan(PlannerInfo *root, CdbMotionPath *path);
static SeqScan *create_seqscan_plan(PlannerInfo *root, Path *best_path,
					List *tlist, List *scan_clauses);
static ExternalScan *create_externalscan_plan(PlannerInfo *root, Path *best_path,
						 List *tlist, List *scan_clauses);
static AppendOnlyScan *create_appendonlyscan_plan(PlannerInfo *root, Path *best_path,
						   List *tlist, List *scan_clauses);
static AOCSScan *create_aocsscan_plan(PlannerInfo *root, Path *best_path,
					 List *tlist, List *scan_clauses);
static IndexScan *create_indexscan_plan(PlannerInfo *root, IndexPath *best_path,
					  List *tlist, List *scan_clauses);
static BitmapHeapScan *create_bitmap_scan_plan(PlannerInfo *root,
						BitmapHeapPath *best_path,
						List *tlist, List *scan_clauses);
static Plan *create_bitmap_subplan(PlannerInfo *root, Path *bitmapqual,
					  List **qual, List **indexqual);
static TidScan *create_tidscan_plan(PlannerInfo *root, TidPath *best_path,
					List *tlist, List *scan_clauses);
static SubqueryScan *create_subqueryscan_plan(PlannerInfo *root, Path *best_path,
						 List *tlist, List *scan_clauses);
static FunctionScan *create_functionscan_plan(PlannerInfo *root, Path *best_path,
						 List *tlist, List *scan_clauses);
static TableFunctionScan *create_tablefunction_plan(PlannerInfo *root,
						  Path *best_path,
						  List *tlist,
						  List *scan_clauses);
static ValuesScan *create_valuesscan_plan(PlannerInfo *root, Path *best_path,
					   List *tlist, List *scan_clauses);
static SubqueryScan * create_ctescan_plan(PlannerInfo *root, Path *best_path,
										  List *tlist, List *scan_clauses);
static WorkTableScan *create_worktablescan_plan(PlannerInfo *root, Path *best_path,
												List *tlist, List *scan_clauses);
static BitmapAppendOnlyScan *create_bitmap_appendonly_scan_plan(PlannerInfo *root,
								   BitmapAppendOnlyPath *best_path,
								   List *tlist, List *scan_clauses);
static NestLoop *create_nestloop_plan(PlannerInfo *root, NestPath *best_path,
					 Plan *outer_plan, Plan *inner_plan);
static MergeJoin *create_mergejoin_plan(PlannerInfo *root, MergePath *best_path,
					  Plan *outer_plan, Plan *inner_plan);
static HashJoin *create_hashjoin_plan(PlannerInfo *root, HashPath *best_path,
					 Plan *outer_plan, Plan *inner_plan);
static List *fix_indexqual_references(List *indexquals, IndexPath *index_path);
static Node *fix_indexqual_operand(Node *node, IndexOptInfo *index);
static List *get_switched_clauses(List *clauses, Relids outerrelids);
static List *order_qual_clauses(PlannerInfo *root, List *clauses);
static void copy_path_costsize(PlannerInfo *root, Plan *dest, Path *src);
static void copy_plan_costsize(Plan *dest, Plan *src);
static SeqScan *make_seqscan(List *qptlist, List *qpqual, Index scanrelid);
static AppendOnlyScan *make_appendonlyscan(List *qptlist, List *qpqual, Index scanrelid);
static AOCSScan *make_aocsscan(List *qptlist, List *qpqual, Index scanrelid);
static ExternalScan *make_externalscan(List *qptlist,
				  List *qpqual,
				  Index scanrelid,
				  List *filenames,
				  List *fmtopts,
				  bool istext,
				  bool ismasteronly,
				  int rejectlimit,
				  bool rejectlimitinrows,
				  Oid fmterrtableOid,
				  int encoding);
static IndexScan *make_indexscan(List *qptlist, List *qpqual, Index scanrelid,
			   Oid indexid, List *indexqual, List *indexqualorig,
			   ScanDirection indexscandir);
static BitmapIndexScan *make_bitmap_indexscan(Index scanrelid, Oid indexid,
					  List *indexqual,
					  List *indexqualorig);
static BitmapHeapScan *make_bitmap_heapscan(List *qptlist,
					 List *qpqual,
					 Plan *lefttree,
					 List *bitmapqualorig,
					 Index scanrelid);
static BitmapAppendOnlyScan *make_bitmap_appendonlyscan(List *qptlist,
						   List *qpqual,
						   Plan *lefttree,
						   List *bitmapqualorig,
						   Index scanrelid,
						   bool isAORow);
static TableFunctionScan *make_tablefunction(List *tlist,
				   List *scan_quals,
				   Plan *subplan,
				   List *subrtable,
				   Index scanrelid);
static TidScan *make_tidscan(List *qptlist, List *qpqual, Index scanrelid,
			 List *tidquals);
static FunctionScan *make_functionscan(List *qptlist, List *qpqual,
				  Index scanrelid, Node *funcexpr, List *funccolnames,
				  List *funccoltypes, List *funccoltypmods);
static ValuesScan *make_valuesscan(List *qptlist, List *qpqual,
				Index scanrelid, List *values_lists);
static CteScan *make_ctescan(List *qptlist, List *qpqual,
							 Index scanrelid, int ctePlanId, int cteParam);
static WorkTableScan *make_worktablescan(List *qptlist, List *qpqual,
										 Index scanrelid, int wtParam);
static BitmapAnd *make_bitmap_and(List *bitmapplans);
static BitmapOr *make_bitmap_or(List *bitmapplans);
static List *flatten_grouping_list(List *groupcls);


/*
 * create_plan
 *	  Creates the access plan for a query by tracing backwards through the
 *	  desired chain of pathnodes, starting at the node 'best_path'.  For
 *	  every pathnode found, we create a corresponding plan node containing
 *	  appropriate id, target list, and qualification information.
 *
 *	  The tlists and quals in the plan tree are still in planner format,
 *	  ie, Vars still correspond to the parser's numbering.  This will be
 *	  fixed later by setrefs.c.
 *
 *	  best_path is the best access path
 *
 *	  Returns a Plan tree.
 */
Plan *
create_plan(PlannerInfo *root, Path *path)
{
	Plan	   *plan;

	/* Modify path to support unique rowid operation for subquery preds. */
	if (root->join_info_list)
		cdbpath_dedup_fixup(root, path);

	/* Generate the Plan tree. */
	plan = create_subplan(root, path);

	/* Decorate the top node of the plan with a Flow node. */
	plan->flow = cdbpathtoplan_create_flow(root,
										   path->locus,
										   path->parent ? path->parent->relids
										   : NULL,
										   path->pathkeys,
										   plan);
	return plan;
}	/* create_plan */


/*
 * create_subplan
 */
Plan *
create_subplan(PlannerInfo *root, Path *best_path)
{
	Plan	   *plan;

	switch (best_path->pathtype)
	{
		case T_SeqScan:
		case T_IndexScan:
		case T_ExternalScan:
		case T_AppendOnlyScan:
		case T_AOCSScan:
		case T_BitmapHeapScan:
		case T_BitmapAppendOnlyScan:
		case T_BitmapTableScan:
		case T_TidScan:
		case T_SubqueryScan:
		case T_FunctionScan:
		case T_TableFunctionScan:
		case T_ValuesScan:
		case T_CteScan:
		case T_WorkTableScan:
			plan = create_scan_plan(root, best_path);
			break;
		case T_HashJoin:
		case T_MergeJoin:
		case T_NestLoop:
			plan = create_join_plan(root,
									(JoinPath *) best_path);
			break;
		case T_Append:
			plan = create_append_plan(root,
									  (AppendPath *) best_path);
			break;
		case T_Result:
			plan = (Plan *) create_result_plan(root,
											   (ResultPath *) best_path);
			break;
		case T_Material:
			plan = (Plan *) create_material_plan(root,
												 (MaterialPath *) best_path);
			break;
		case T_Unique:
			plan = create_unique_plan(root,
									  (UniquePath *) best_path);
			break;
		case T_Motion:
			plan = create_motion_plan(root, (CdbMotionPath *) best_path);
			break;
		default:
			elog(ERROR, "unrecognized node type: %d",
				 (int) best_path->pathtype);
			plan = NULL;		/* keep compiler quiet */
			break;
	}

	if (CdbPathLocus_IsPartitioned(best_path->locus) ||
		CdbPathLocus_IsReplicated(best_path->locus))
		plan->dispatch = DISPATCH_PARALLEL;

	return plan;
}

/*
 * create_scan_plan
 *	 Create a scan plan for the parent relation of 'best_path'.
 */
static Plan *
create_scan_plan(PlannerInfo *root, Path *best_path)
{
	RelOptInfo *rel = best_path->parent;
	List	   *tlist;
	List	   *scan_clauses;
	Plan	   *plan;

	/*
	 * For table scans, rather than using the relation targetlist (which is
	 * only those Vars actually needed by the query), we prefer to generate a
	 * tlist containing all Vars in order.	This will allow the executor to
	 * optimize away projection of the table tuples, if possible.  (Note that
	 * planner.c may replace the tlist we generate here, forcing projection to
	 * occur.)
	 */
	if (use_physical_tlist(root, rel))
	{
		tlist = build_physical_tlist(root, rel);
		/* if fail because of dropped cols, use regular method */
		if (tlist == NIL)
			tlist = build_relation_tlist(rel);
	}
	else
		tlist = build_relation_tlist(rel);

	/*
	 * Extract the relevant restriction clauses from the parent relation. The
	 * executor must apply all these restrictions during the scan, except for
	 * pseudoconstants which we'll take care of below.
	 */
	scan_clauses = rel->baserestrictinfo;

	switch (best_path->pathtype)
	{
		case T_SeqScan:
			plan = (Plan *) create_seqscan_plan(root,
												best_path,
												tlist,
												scan_clauses);
			break;

		case T_AppendOnlyScan:
			plan = (Plan *) create_appendonlyscan_plan(root,
													   best_path,
													   tlist,
													   scan_clauses);
			break;
		case T_AOCSScan:
			plan = (Plan *) create_aocsscan_plan(root,
												 best_path,
												 tlist,
												 scan_clauses);
			break;

		case T_ExternalScan:
			plan = (Plan *) create_externalscan_plan(root,
													 best_path,
													 tlist,
													 scan_clauses);
			break;

		case T_IndexScan:
			plan = (Plan *) create_indexscan_plan(root,
												  (IndexPath *) best_path,
												  tlist,
												  scan_clauses);
			break;

		case T_BitmapHeapScan:
			plan = (Plan *) create_bitmap_scan_plan(root,
												(BitmapHeapPath *) best_path,
													tlist,
													scan_clauses);
			break;

		case T_BitmapAppendOnlyScan:
			plan = (Plan *) create_bitmap_appendonly_scan_plan(root,
										  (BitmapAppendOnlyPath *) best_path,
															   tlist,
															   scan_clauses);
			break;

		case T_TidScan:
			plan = (Plan *) create_tidscan_plan(root,
												(TidPath *) best_path,
												tlist,
												scan_clauses);
			break;

		case T_SubqueryScan:
			plan = (Plan *) create_subqueryscan_plan(root,
													 best_path,
													 tlist,
													 scan_clauses);
			break;

		case T_FunctionScan:
			plan = (Plan *) create_functionscan_plan(root,
													 best_path,
													 tlist,
													 scan_clauses);
			break;

		case T_TableFunctionScan:
			plan = (Plan *) create_tablefunction_plan(root,
													  best_path,
													  tlist,
													  scan_clauses);
			break;

		case T_ValuesScan:
			plan = (Plan *) create_valuesscan_plan(root,
												   best_path,
												   tlist,
												   scan_clauses);
			break;

		case T_CteScan:
			plan = (Plan *) create_ctescan_plan(root,
												best_path,
												tlist,
												scan_clauses);
			break;

		case T_WorkTableScan:
			plan = (Plan *) create_worktablescan_plan(root,
													  best_path,
													  tlist,
													  scan_clauses);
			break;

		default:
			elog(ERROR, "unrecognized node type: %d",
				 (int) best_path->pathtype);
			plan = NULL;		/* keep compiler quiet */
			break;
	}

	/* Decorate the top node of the plan with a Flow node. */
	plan->flow = cdbpathtoplan_create_flow(root,
										   best_path->locus,
								best_path->parent ? best_path->parent->relids
										   : NULL,
										   best_path->pathkeys,
										   plan);

	/**
	 * If plan has a flow node, ensure all entries of hashExpr
	 * are in the targetlist.
	 */
	if (plan->flow && plan->flow->hashExpr)
	{
		plan->targetlist = add_to_flat_tlist_junk(plan->targetlist, plan->flow->hashExpr, true /* resjunk */ );
	}

	/*
	 * If there are any pseudoconstant clauses attached to this node, insert a
	 * gating Result node that evaluates the pseudoconstants as one-time
	 * quals.
	 */
	if (root->hasPseudoConstantQuals)
		plan = create_gating_plan(root, plan, scan_clauses);

	return plan;
}

/*
 * Build a target list (ie, a list of TargetEntry) for a relation.
 */
List *
build_relation_tlist(RelOptInfo *rel)
{
	List	   *tlist = NIL;
	int			resno = 1;
	ListCell   *v;

	foreach(v, rel->reltargetlist)
	{
		/* Do we really need to copy here?	Not sure */
		Node   *node = (Node *) copyObject(lfirst(v));

		tlist = lappend(tlist, makeTargetEntry((Expr *) node,
											   resno,
											   NULL,
											   false));
		resno++;
	}
	return tlist;
}

/*
 * use_physical_tlist
 *		Decide whether to use a tlist matching relation structure,
 *		rather than only those Vars actually referenced.
 */
static bool
use_physical_tlist(PlannerInfo *root, RelOptInfo *rel)
{
	RangeTblEntry *rte;
	int			i;
	ListCell   *lc;

	/*
	 * We can do this for real relation scans, subquery scans, function scans,
	 * values scans, and CTE scans (but not for, eg, joins).
	 */
	if (rel->rtekind != RTE_RELATION &&
		rel->rtekind != RTE_SUBQUERY &&
		rel->rtekind != RTE_FUNCTION &&
		rel->rtekind != RTE_VALUES &&
		rel->rtekind != RTE_TABLEFUNCTION &&
		rel->rtekind != RTE_CTE)
		return false;

	/*
	 * Can't do it with inheritance cases either (mainly because Append
	 * doesn't project).
	 */
	if (rel->reloptkind != RELOPT_BASEREL)
		return false;

	/*
	 * Can't do it if any system columns or whole-row Vars are requested.
	 * (This could possibly be fixed but would take some fragile assumptions
	 * in setrefs.c, I think.)
	 */
	for (i = rel->min_attr; i <= 0; i++)
	{
		if (!bms_is_empty(rel->attr_needed[i - rel->min_attr]))
			return false;
	}

	/*
	 * Can't do it if the rel is required to emit any placeholder expressions,
	 * either.
	 */
	foreach(lc, root->placeholder_list)
	{
		PlaceHolderInfo *phinfo = (PlaceHolderInfo *) lfirst(lc);

		if (bms_nonempty_difference(phinfo->ph_needed, rel->relids) &&
			bms_is_subset(phinfo->ph_eval_at, rel->relids))
			return false;
	}

	/* CDB: Don't use physical tlist if rel has pseudo columns. */
	rte = rt_fetch(rel->relid, root->parse->rtable);
	if (rte->pseudocols)
		return false;

	return true;
}

/*
 * disuse_physical_tlist
 *		Switch a plan node back to emitting only Vars actually referenced.
 *
 * If the plan node immediately above a scan would prefer to get only
 * needed Vars and not a physical tlist, it must call this routine to
 * undo the decision made by use_physical_tlist().	Currently, Hash, Sort,
 * and Material nodes want this, so they don't have to store useless columns.
 * We need to ensure that all vars referenced in Flow node, if any, are added
 * to the targetlist as resjunk.
 */
static void
disuse_physical_tlist(Plan *plan, Path *path)
{
	/* Only need to undo it for path types handled by create_scan_plan() */
	switch (path->pathtype)
	{
		case T_SeqScan:
		case T_AppendOnlyScan:
		case T_AOCSScan:
		case T_ExternalScan:
		case T_IndexScan:
		case T_BitmapHeapScan:
		case T_BitmapAppendOnlyScan:
		case T_BitmapTableScan:
		case T_TidScan:
		case T_SubqueryScan:
		case T_FunctionScan:
		case T_ValuesScan:
		case T_CteScan:
		case T_WorkTableScan:
			plan->targetlist = build_relation_tlist(path->parent);
			/**
			 * If plan has a flow node, ensure all entries of hashExpr
			 * are in the targetlist.
			 */
			if (plan->flow && plan->flow->hashExpr)
			{
				plan->targetlist = add_to_flat_tlist_junk(plan->targetlist, plan->flow->hashExpr, true /* resjunk */);
			}
			break;
		default:
			break;
	}
}

/*
 * create_gating_plan
 *	  Deal with pseudoconstant qual clauses
 *
 * If the node's quals list includes any pseudoconstant quals, put them
 * into a gating Result node atop the already-built plan.  Otherwise,
 * return the plan as-is.
 *
 * Note that we don't change cost or size estimates when doing gating.
 * The costs of qual eval were already folded into the plan's startup cost.
 * Leaving the size alone amounts to assuming that the gating qual will
 * succeed, which is the conservative estimate for planning upper queries.
 * We certainly don't want to assume the output size is zero (unless the
 * gating qual is actually constant FALSE, and that case is dealt with in
 * clausesel.c).  Interpolating between the two cases is silly, because
 * it doesn't reflect what will really happen at runtime, and besides which
 * in most cases we have only a very bad idea of the probability of the gating
 * qual being true.
 */
static Plan *
create_gating_plan(PlannerInfo *root, Plan *plan, List *quals)
{
	List	   *pseudoconstants;

	/* Sort into desirable execution order while still in RestrictInfo form */
	quals = order_qual_clauses(root, quals);

	/* Pull out any pseudoconstant quals from the RestrictInfo list */
	pseudoconstants = extract_actual_clauses(quals, true);

	if (!pseudoconstants)
		return plan;

	return (Plan *) make_result(root,
								plan->targetlist,
								(Node *) pseudoconstants,
								plan);
}

/*
 * create_join_plan
 *	  Create a join plan for 'best_path' and (recursively) plans for its
 *	  inner and outer paths.
 */
static Plan *
create_join_plan(PlannerInfo *root, JoinPath *best_path)
{
	Plan	   *outer_plan;
	Plan	   *inner_plan;
	Plan	   *plan;
	bool		partition_selector_created;

	Assert(best_path->outerjoinpath);
	Assert(best_path->innerjoinpath);

	inner_plan = create_subplan(root, best_path->innerjoinpath);

	/*
	 * Try to inject Partition Selectors.
	 */
	partition_selector_created =
		inject_partition_selectors_for_join(root,
												best_path,
												&inner_plan);

	outer_plan = create_subplan(root, best_path->outerjoinpath);

	switch (best_path->path.pathtype)
	{
		case T_MergeJoin:
			plan = (Plan *) create_mergejoin_plan(root,
												  (MergePath *) best_path,
												  outer_plan,
												  inner_plan);
			break;
		case T_HashJoin:
			plan = (Plan *) create_hashjoin_plan(root,
												 (HashPath *) best_path,
												 outer_plan,
												 inner_plan);
			break;
		case T_NestLoop:
			plan = (Plan *) create_nestloop_plan(root,
												 (NestPath *) best_path,
												 outer_plan,
												 inner_plan);
			break;
		default:
			elog(ERROR, "unrecognized node type: %d",
				 (int) best_path->path.pathtype);
			plan = NULL;		/* keep compiler quiet */
			break;
	}

	/*
	 * If we injected a partition selector to the inner side, we must evaluate
	 * the inner side before the outer side, so that the partition selector
	 * can influence the execution of the outer side.
	 */
	Assert(plan->type == best_path->path.pathtype);
	if (partition_selector_created)
		((Join *) plan)->prefetch_inner = true;

	plan->flow = cdbpathtoplan_create_flow(root,
			best_path->path.locus,
			best_path->path.parent ? best_path->path.parent->relids
					: NULL,
					  best_path->path.pathkeys,
					  plan);

	/**
	 * If plan has a flow node, ensure all entries of hashExpr
	 * are in the targetlist.
	 */
	if (plan->flow && plan->flow->hashExpr)
	{
		plan->targetlist = add_to_flat_tlist_junk(plan->targetlist, plan->flow->hashExpr, true /* resjunk */ );
	}

	/*
	 * If there are any pseudoconstant clauses attached to this node, insert a
	 * gating Result node that evaluates the pseudoconstants as one-time
	 * quals.
	 */
	if (root->hasPseudoConstantQuals)
		plan = create_gating_plan(root, plan, best_path->joinrestrictinfo);

#ifdef NOT_USED

	/*
	 * * Expensive function pullups may have pulled local predicates * into
	 * this path node.	Put them in the qpqual of the plan node. * JMH,
	 * 6/15/92
	 */
	if (get_loc_restrictinfo(best_path) != NIL)
		set_qpqual((Plan) plan,
				   list_concat(get_qpqual((Plan) plan),
					   get_actual_clauses(get_loc_restrictinfo(best_path))));
#endif

	return plan;
}

/*
 * create_append_plan
 *	  Create an Append plan for 'best_path' and (recursively) plans
 *	  for its subpaths.
 *
 *	  Returns a Plan node.
 */
static Plan *
create_append_plan(PlannerInfo *root, AppendPath *best_path)
{
	Append	   *plan;
	List	   *tlist = build_relation_tlist(best_path->path.parent);
	List	   *subplans = NIL;
	ListCell   *subpaths;

	/*
	 * It is possible for the subplans list to contain only one entry, or even
	 * no entries.	Handle these cases specially.
	 *
	 * XXX ideally, if there's just one entry, we'd not bother to generate an
	 * Append node but just return the single child.  At the moment this does
	 * not work because the varno of the child scan plan won't match the
	 * parent-rel Vars it'll be asked to emit.
	 */
	if (best_path->subpaths == NIL)
	{
		/* Generate a Result plan with constant-FALSE gating qual */
		return (Plan *) make_result(root,
									tlist,
									(Node *) list_make1(makeBoolConst(false,
																	  false)),
									NULL);
	}

	/* Normal case with multiple subpaths */
	foreach(subpaths, best_path->subpaths)
	{
		Path	   *subpath = (Path *) lfirst(subpaths);

		subplans = lappend(subplans, create_subplan(root, subpath));
	}

	plan = make_append(subplans, false, tlist);

	return (Plan *) plan;
}

/*
 * create_result_plan
 *	  Create a Result plan for 'best_path' and (recursively) plans
 *	  for its subpaths.
 *
 *	  Returns a Plan node.
 */
static Result *
create_result_plan(PlannerInfo *root, ResultPath *best_path)
{
	List	   *tlist;
	List	   *quals;

	if (best_path->path.parent)
		tlist = build_relation_tlist(best_path->path.parent);
	else
		tlist = NIL;			/* will be filled in later */

	/* best_path->quals is just bare clauses */

	quals = order_qual_clauses(root, best_path->quals);

	return make_result(root, tlist, (Node *) quals, NULL);
}

/*
 * create_material_plan
 *	  Create a Material plan for 'best_path' and (recursively) plans
 *	  for its subpaths.
 *
 *	  Returns a Plan node.
 */
static Material *
create_material_plan(PlannerInfo *root, MaterialPath *best_path)
{
	Material   *plan;
	Plan	   *subplan;

	subplan = create_subplan(root, best_path->subpath);

	/* We don't want any excess columns in the materialized tuples */
	disuse_physical_tlist(subplan, best_path->subpath);

	plan = make_material(subplan);

	plan->cdb_strict = best_path->cdb_strict;

	copy_path_costsize(root, &plan->plan, (Path *) best_path);

	return plan;
}

/*
 * create_unique_plan
 *	  Create a Unique plan for 'best_path' and (recursively) plans
 *	  for its subpaths.
 *
 *	  Returns a Plan node.
 */
static Plan *
create_unique_plan(PlannerInfo *root, UniquePath *best_path)
{
	Plan	   *plan;
	Plan	   *subplan;
	List	   *in_operators;
	List	   *uniq_exprs;
	List	   *newtlist;
	int			nextresno;
	bool		newitems;
	int			numGroupCols;
	AttrNumber *groupColIdx;
	int			groupColPos;
	ListCell   *l;

	subplan = create_subplan(root, best_path->subpath);

	/* Return naked subplan if we don't need to do any actual unique-ifying */
	if (best_path->umethod == UNIQUE_PATH_NOOP)
		return subplan;

	/* CDB: Result will surely be unique if we produce only its first row. */
	if (best_path->umethod == UNIQUE_PATH_LIMIT1)
	{
		Limit	   *limitplan;

		/* Top off the subplan with a LIMIT 1 node. */
		limitplan = makeNode(Limit);
		copy_path_costsize(root, &limitplan->plan, &best_path->path);
		limitplan->plan.targetlist = subplan->targetlist;
		limitplan->plan.qual = NIL;
		limitplan->plan.lefttree = subplan;
		limitplan->plan.righttree = NULL;
		limitplan->limitOffset = NULL;
		limitplan->limitCount = (Node *) makeConst(INT8OID, -1, sizeof(int64),
												   Int64GetDatum(1),
												   false, true);
		return (Plan *) limitplan;
	}

	/*
	 * As constructed, the subplan has a "flat" tlist containing just the
	 * Vars needed here and at upper levels.  The values we are supposed
	 * to unique-ify may be expressions in these variables.  We have to
	 * add any such expressions to the subplan's tlist.
	 *
	 * The subplan may have a "physical" tlist if it is a simple scan plan.
	 * If we're going to sort, this should be reduced to the regular tlist,
	 * so that we don't sort more data than we need to.  For hashing, the
	 * tlist should be left as-is if we don't need to add any expressions;
	 * but if we do have to add expressions, then a projection step will be
	 * needed at runtime anyway, so we may as well remove unneeded items.
	 * Therefore newtlist starts from build_relation_tlist() not just a
	 * copy of the subplan's tlist; and we don't install it into the subplan
	 * unless we are sorting or stuff has to be added.
	 */
	in_operators = best_path->distinct_on_eq_operators; /* CDB */
	uniq_exprs = best_path->distinct_on_exprs;	/* CDB */

	/* initialize modified subplan tlist as just the "required" vars */
	newtlist = build_relation_tlist(best_path->path.parent);
	nextresno = list_length(newtlist) + 1;
	newitems = false;

	foreach(l, uniq_exprs)
	{
		Node	   *uniqexpr = lfirst(l);
		TargetEntry *tle;

		tle = tlist_member(uniqexpr, newtlist);
		if (!tle)
		{
			tle = makeTargetEntry((Expr *) uniqexpr,
								  nextresno,
								  NULL,
								  false);
			newtlist = lappend(newtlist, tle);
			nextresno++;
			newitems = true;
		}
	}

	if (newitems || best_path->umethod == UNIQUE_PATH_SORT)
	{
		/*
		 * If the top plan node can't do projections, we need to add a Result
		 * node to help it along.
		 */
		subplan = plan_pushdown_tlist(root, subplan, newtlist);
	}

	/*
	 * Build control information showing which subplan output columns are to
	 * be examined by the grouping step.  Unfortunately we can't merge this
	 * with the previous loop, since we didn't then know which version of the
	 * subplan tlist we'd end up using.
	 */
	newtlist = subplan->targetlist;
	numGroupCols = list_length(uniq_exprs);
	groupColIdx = (AttrNumber *) palloc(numGroupCols * sizeof(AttrNumber));

	groupColPos = 0;
	foreach(l, uniq_exprs)
	{
		Node	   *uniqexpr = lfirst(l);
		TargetEntry *tle;

		tle = tlist_member(uniqexpr, newtlist);
		if (!tle)				/* shouldn't happen */
			elog(ERROR, "failed to find unique expression in subplan tlist");
		groupColIdx[groupColPos++] = tle->resno;
	}

	if (best_path->umethod == UNIQUE_PATH_HASH)
	{
		long		numGroups;
		Oid		   *groupOperators;

		numGroups = (long) Min(best_path->rows, (double) LONG_MAX);

		/*
		 * Get the hashable equality operators for the Agg node to use.
		 * Normally these are the same as the IN clause operators, but if
		 * those are cross-type operators then the equality operators are the
		 * ones for the IN clause operators' RHS datatype.
		 */
		groupOperators = (Oid *) palloc(numGroupCols * sizeof(Oid));
		groupColPos = 0;
		foreach(l, in_operators)
		{
			Oid			in_oper = lfirst_oid(l);
			Oid			eq_oper;

			if (!get_compatible_hash_operators(in_oper, NULL, &eq_oper))
				elog(ERROR, "could not find compatible hash operator for operator %u",
					 in_oper);
			groupOperators[groupColPos++] = eq_oper;
		}

		/*
		 * Since the Agg node is going to project anyway, we can give it the
		 * minimum output tlist, without any stuff we might have added to the
		 * subplan tlist.
		 */
		plan = (Plan *) make_agg(root,
								 build_relation_tlist(best_path->path.parent),
								 NIL,
								 AGG_HASHED, false,
								 numGroupCols,
								 groupColIdx,
								 groupOperators,
								 numGroups,
								 0, /* num_nullcols */
								 0, /* input_grouping */
								 0, /* grouping */
								 0, /* rollup_gs_times */
								 0,
								 0,
								 subplan);
	}
	else
	{
		List	   *sortList = NIL;

		/* Create an ORDER BY list to sort the input compatibly */
		groupColPos = 0;
		foreach(l, in_operators)
		{
			Oid			in_oper = lfirst_oid(l);
			Oid			sortop;
			TargetEntry *tle;
			SortGroupClause *sortcl;

			sortop = get_ordering_op_for_equality_op(in_oper, false);
			if (!OidIsValid(sortop))	/* shouldn't happen */
				elog(ERROR, "could not find ordering operator for equality operator %u",
					 in_oper);
			tle = get_tle_by_resno(subplan->targetlist,
								   groupColIdx[groupColPos]);
			Assert(tle != NULL);
			sortcl = makeNode(SortGroupClause);
			sortcl->tleSortGroupRef = assignSortGroupRef(tle,
														 subplan->targetlist);
			sortcl->eqop = in_oper;
			sortcl->sortop = sortop;
			sortcl->nulls_first = false;
			sortList = lappend(sortList, sortcl);
			groupColPos++;
		}
		plan = (Plan *) make_sort_from_sortclauses(root, sortList, subplan);
		plan = (Plan *) make_unique(plan, sortList);
	}

	/* Adjust output size estimate (other fields should be OK already) */
	plan->plan_rows = cdbpath_rows(root, (Path *) best_path);

	return plan;
}


/*
 * create_motion_plan
 */
Plan *
create_motion_plan(PlannerInfo *root, CdbMotionPath *path)
{
	Motion	   *motion;
	Path	   *subpath = path->subpath;
	Plan	   *subplan;

	/*
	 * singleQE-->entry:  Elide the motion.  The subplan will run in the same
	 * process with its parent: either the qDisp (if it is a top slice) or a
	 * singleton gang on the entry db (otherwise).
	 */
	if (CdbPathLocus_IsEntry(path->path.locus) &&
		CdbPathLocus_IsSingleQE(subpath->locus))
	{
		/* Push the MotionPath's locus and pathkeys down onto subpath. */
		subpath->locus = path->path.locus;
		subpath->pathkeys = path->path.pathkeys;

		subplan = create_subplan(root, subpath);
		return subplan;
	}

	subplan = create_subplan(root, subpath);

	/* Only the needed columns should be projected from base rel. */
	disuse_physical_tlist(subplan, subpath);

	/* Add motion operator. */
	motion = cdbpathtoplan_create_motion_plan(root, path, subplan);

	copy_path_costsize(root, &motion->plan, (Path *) path);
	return (Plan *) motion;
}	/* create_motion_plan */


/*****************************************************************************
 *
 *	BASE-RELATION SCAN METHODS
 *
 *****************************************************************************/


/*
 * create_seqscan_plan
 *	 Returns a seqscan plan for the base relation scanned by 'best_path'
 *	 with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static SeqScan *
create_seqscan_plan(PlannerInfo *root, Path *best_path,
					List *tlist, List *scan_clauses)
{
	SeqScan    *scan_plan;
	Index		scan_relid = best_path->parent->relid;

	/* it should be a base rel... */
	Assert(scan_relid > 0);
	Assert(best_path->parent->rtekind == RTE_RELATION);

	/* Sort clauses into best execution order */
	scan_clauses = order_qual_clauses(root, scan_clauses);

	/* Reduce RestrictInfo list to bare expressions; ignore pseudoconstants */
	scan_clauses = extract_actual_clauses(scan_clauses, false);

	scan_plan = make_seqscan(tlist,
							 scan_clauses,
							 scan_relid);

	copy_path_costsize(root, &scan_plan->plan, best_path);

	return scan_plan;
}

/*
 * create_appendonlyscan_plan
 *	 Returns a appendonlyscan plan for the base relation scanned by 'best_path'
 *	 with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static AppendOnlyScan *
create_appendonlyscan_plan(PlannerInfo *root, Path *best_path,
						   List *tlist, List *scan_clauses)
{
	AppendOnlyScan *scan_plan;
	Index		scan_relid = best_path->parent->relid;

	/* it should be a base rel... */
	Assert(scan_relid > 0);
	Assert(best_path->parent->rtekind == RTE_RELATION);

	/* Sort clauses into best execution order */
	scan_clauses = order_qual_clauses(root, scan_clauses);

	/* Reduce RestrictInfo list to bare expressions; ignore pseudoconstants */
	scan_clauses = extract_actual_clauses(scan_clauses, false);

	scan_plan = make_appendonlyscan(tlist,
									scan_clauses,
									scan_relid);

	copy_path_costsize(root, &scan_plan->scan.plan, best_path);

	return scan_plan;
}

/*
 * create_aocsscan_plan
 *	 Returns a appendonlyscan plan for the base relation scanned by 'best_path'
 *	 with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static AOCSScan *
create_aocsscan_plan(PlannerInfo *root, Path *best_path,
					 List *tlist, List *scan_clauses)
{
	AOCSScan   *scan_plan;
	Index		scan_relid = best_path->parent->relid;

	/* it should be a base rel... */
	Assert(scan_relid > 0);
	Assert(best_path->parent->rtekind == RTE_RELATION);

	/* Sort clauses into best execution order */
	scan_clauses = order_qual_clauses(root, scan_clauses);

	/* Reduce RestrictInfo list to bare expressions; ignore pseudoconstants */
	scan_clauses = extract_actual_clauses(scan_clauses, false);

	scan_plan = make_aocsscan(tlist,
							  scan_clauses,
							  scan_relid);

	copy_path_costsize(root, &scan_plan->scan.plan, best_path);

	return scan_plan;
}


/*
 * create_externalscan_plan
 *	 Returns an externalscan plan for the base relation scanned by 'best_path'
 *	 with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 *
 *	 The external plan also includes the data format specification and file
 *	 location specification. Here is where we do the mapping of external file
 *	 to segment database and add it to the plan (or bail out of the mapping
 *	 rules are broken)
 *
 *	 Mapping rules
 *	 -------------
 *	 - 'file' protocol: each location (URI of local file) gets mapped to one
 *						and one only primary segdb.
 *	 - 'http' protocol: each location (URI of http server) gets mapped to one
 *						and one only primary segdb.
 *	 - 'gpfdist' and 'gpfdists' protocols: all locations (URI of gpfdist(s) client) are mapped
 *						to all primary segdbs. If there are less URIs than
 *						segdbs (usually the case) the URIs are duplicated
 *						so that there will be one for each segdb. However, if
 *						the GUC variable gp_external_max_segs is set to a num
 *						less than (total segdbs/total URIs) then we make sure
 *						that no URI gets mapped to more than this GUC number by
 *						skipping some segdbs randomly.
 *	 - 'gphdfs' protocol: file is divided in to chucks and each chuck is assigned
 *						to a segdb.
 *	 - 'exec' protocol: all segdbs get mapped to execute the command (this is
 *						soon to be changed though).
 */
static ExternalScan *
create_externalscan_plan(PlannerInfo *root, Path *best_path,
						 List *tlist, List *scan_clauses)
{
	ExternalScan *scan_plan;
	Index		scan_relid = best_path->parent->relid;
	RelOptInfo *rel = best_path->parent;
	List	   *filenames;
	bool		ismasteronly = false;
	bool		islimitinrows = false;
	int			rejectlimit = -1;
	Oid			fmtErrTblOid = InvalidOid;
	ExtTableEntry *ext = rel->extEntry;
	List	   *fmtopts;

	/* it should be an external rel... */
	Assert(scan_relid > 0);
	Assert(rel->rtekind == RTE_RELATION);

	/* Sort clauses into best execution order */
	scan_clauses = order_qual_clauses(root, scan_clauses);

	/* Reduce RestrictInfo list to bare expressions; ignore pseudoconstants */
	scan_clauses = extract_actual_clauses(scan_clauses, false);

	Assert(ext->execlocations != NIL);

	if (ext->rejectlimit != -1)
	{
		/*
		 * single row error handling is requested, make sure reject limit and
		 * error table (if requested) are valid.
		 *
		 * NOTE: this should never happen unless somebody modified the catalog
		 * manually. We are just being pedantic here.
		 */
		VerifyRejectLimit(ext->rejectlimittype, ext->rejectlimit);
	}

	/* assign Uris to segments. */
	filenames = create_external_scan_uri_list(ext, &ismasteronly);

	/* data format description */
	Assert(ext->fmtopts);
	fmtopts = list_make1(makeString(pstrdup(ext->fmtopts)));

	/* single row error handling */
	if (ext->rejectlimit != -1)
	{
		islimitinrows = (ext->rejectlimittype == 'r' ? true : false);
		rejectlimit = ext->rejectlimit;
		fmtErrTblOid = ext->fmterrtbl;
	}

	scan_plan = make_externalscan(tlist,
								  scan_clauses,
								  scan_relid,
								  filenames,
								  fmtopts,
								  ext->fmtcode,
								  ismasteronly,
								  rejectlimit,
								  islimitinrows,
								  fmtErrTblOid,
								  ext->encoding);

	copy_path_costsize(root, &scan_plan->scan.plan, best_path);

	return scan_plan;
}

List *
create_external_scan_uri_list(ExtTableEntry *ext, bool *ismasteronly)
{
	ListCell   *c;
	List	   *modifiedloclist = NIL;
	int			i;
	CdbComponentDatabases *db_info;
	int			total_primaries;
	char	  **segdb_file_map;

	/* various processing flags */
	bool		using_execute = false;	/* true if EXECUTE is used */
	bool		using_location; /* true if LOCATION is used */
	bool		found_candidate = false;
	bool		found_match = false;
	bool		done = false;
	List	   *filenames;

	/* gpfdist(s) or EXECUTE specific variables */
	int			total_to_skip = 0;
	int			max_participants_allowed = 0;
	int			num_segs_participating = 0;
	bool	   *skip_map = NULL;
	bool		should_skip_randomly = false;

	Uri		   *uri;
	char	   *on_clause;

	*ismasteronly = false;

	/* is this an EXECUTE table or a LOCATION (URI) table */
	if (ext->command)
	{
		using_execute = true;
		using_location = false;
	}
	else
	{
		using_execute = false;
		using_location = true;
	}

	/* is this an EXECUTE table or a LOCATION (URI) table */
	if (ext->command && !gp_external_enable_exec)
	{
		ereport(ERROR,
				(errcode(ERRCODE_GP_FEATURE_NOT_CONFIGURED),	/* any better errcode? */
				 errmsg("Using external tables with OS level commands "
						"(EXECUTE clause) is disabled"),
				 errhint("To enable set gp_external_enable_exec=on")));
	}

	/* various validations */
	if (ext->iswritable)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("cannot read from a WRITABLE external table"),
				 errhint("Create the table as READABLE instead.")));

	/*
	 * take a peek at the first URI so we know which protocol we'll deal with
	 */
	if (!using_execute)
	{
		char	   *first_uri_str;

		first_uri_str = strVal(linitial(ext->urilocations));
		uri = ParseExternalTableUri(first_uri_str);
	}
	else
		uri = NULL;

	/* get the ON clause information, and restrict 'ON MASTER' to custom
	 * protocols only */
	on_clause = (char *) strVal(linitial(ext->execlocations));
	if ((strcmp(on_clause, "MASTER_ONLY") == 0)
		&& using_location && (uri->protocol != URI_CUSTOM)) {
		ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				errmsg("\'ON MASTER\' is not supported by this protocol yet.")));
	}

	/* get the total valid primary segdb count */
	db_info = getCdbComponentDatabases();
	total_primaries = 0;
	for (i = 0; i < db_info->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];

		if (SEGMENT_IS_ACTIVE_PRIMARY(p))
			total_primaries++;
	}

	/*
	 * initialize a file-to-segdb mapping. segdb_file_map string array indexes
	 * segindex and the entries are the external file path is assigned to this
	 * segment datbase. For example if segdb_file_map[2] has "/tmp/emp.1" then
	 * this file is assigned to primary segdb 2. if an entry has NULL then
	 * that segdb isn't assigned any file.
	 */
	segdb_file_map = (char **) palloc0(total_primaries * sizeof(char *));

	/*
	 * Now we do the actual assignment of work to the segment databases (where
	 * work is either a URI to open or a command to execute). Due to the big
	 * differences between the different protocols we handle each one
	 * separately. Unfortunately this means some code duplication, but keeping
	 * this separation makes the code much more understandable and (even) more
	 * maintainable.
	 *
	 * Outline of the following code blocks (from simplest to most complex):
	 * (only one of these will get executed for a statement)
	 *
	 * 1) segment mapping for tables with LOCATION http:// or file:// .
	 *
	 * These two protocols are very similar in that they enforce a
	 * 1-URI:1-segdb relationship. The only difference between them is that
	 * file:// URI must be assigned to a segdb on a host that is local to that
	 * URI.
	 *
	 * 2) segment mapping for tables with LOCATION gpfdist(s):// or custom
	 * protocol
	 *
	 * This protocol is more complicated - in here we usually duplicate the
	 * user supplied gpfdist(s):// URIs until there is one available to every
	 * segdb. However, in some cases (as determined by gp_external_max_segs
	 * GUC) we don't want to use *all* segdbs but instead figure out how many
	 * and pick them randomly (this is mainly for better performance and
	 * resource mgmt).
	 *
	 * 3) segment mapping for tables with EXECUTE 'cmd' ON.
	 *
	 * In here we don't have URI's. We have a single command string and a
	 * specification of the segdb granularity it should get executed on (the
	 * ON clause). Depending on the ON clause specification we could go many
	 * different ways, for example: assign the command to all segdb, or one
	 * command per host, or assign to 5 random segments, etc...
	 *
	 * 4) segment mapping for tables with LOCATION gphdfs://
	 *
	 * The file chuck division and assignment will be done in the external
	 * Java program. We simply assign the location to all the segdbs.
	 */

	/* (1) */
	if (using_location && (uri->protocol == URI_FILE || uri->protocol == URI_HTTP))
	{
		/*
		 * extract file path and name from URI strings and assign them a
		 * primary segdb
		 */
		foreach(c, ext->urilocations)
		{
			const char *uri_str = (char *) strVal(lfirst(c));

			uri = ParseExternalTableUri(uri_str);

			found_candidate = false;
			found_match = false;

			/*
			 * look through our segment database list and try to find a
			 * database that can handle this uri.
			 */
			for (i = 0; i < db_info->total_segment_dbs && !found_match; i++)
			{
				CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
				int segind = p->segindex;

				/* 
				 * Assign mapping of external file to this segdb only if:
				 * 1) This segdb is a valid primary.
				 * 2) An external file wasn't already assigned to it. 
				 * 3) If 'file' protocol, host of segdb and file must be 
				 *    the same.
				 *
				 * This logic also guarantees that file that appears first in
				 * the external location list for the same host gets assigned
				 * the segdb with the lowest index for this host.
				 */
				if (SEGMENT_IS_ACTIVE_PRIMARY(p))
				{
					if (uri->protocol == URI_FILE)
					{
						if (pg_strcasecmp(uri->hostname, p->hostname) != 0 && pg_strcasecmp(uri->hostname, p->address) != 0)
							continue;
					}

					/* a valid primary segdb exist on this host */
					found_candidate = true;

					if (segdb_file_map[segind] == NULL)
					{
						/* segdb not taken yet. assign this URI to this segdb */
						segdb_file_map[segind] = pstrdup(uri_str);
						found_match = true;
					}

					/*
					 * too bad. this segdb already has an external source
					 * assigned
					 */
				}
			}

			/*
			 * We failed to find a segdb for this URI.
			 */
			if (!found_match)
			{
				if (uri->protocol == URI_FILE)
				{
					if (found_candidate)
					{
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
								 errmsg("Could not assign a segment database for \"%s\". "
						"There are more external files than primary segment "
						"databases on host \"%s\"", uri_str, uri->hostname)));
					}
					else
					{
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
								 errmsg("Could not assign a segment database for \"%s\". "
							  "There isn't a valid primary segment database "
								 "on host \"%s\"", uri_str, uri->hostname)));
					}
				}
				else	/* HTTP */
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					errmsg("Could not assign a segment database for \"%s\". "
						   "There are more URIs than total primary segment "
						   "databases", uri_str)));
				}
			}
		}


	}
	/* (2) */
	else if (using_location && (uri->protocol == URI_GPFDIST ||
							   uri->protocol == URI_GPFDISTS ||
							   uri->protocol == URI_CUSTOM))
	{
		if ((strcmp(on_clause, "MASTER_ONLY") == 0) && (uri->protocol == URI_CUSTOM))
		{
			const char *uri_str = strVal(linitial(ext->urilocations));
			segdb_file_map[0] = pstrdup(uri_str);
			*ismasteronly = true;
		}
		else
		{
			/*
			 * Re-write the location list for GPFDIST or GPFDISTS before mapping to segments.
			 *
			 * If we happen to be dealing with URI's with the 'gpfdist' (or 'gpfdists') protocol
			 * we do an extra step here.
			 *
			 * (*) We modify the urilocationlist so that every
			 * primary segdb will get a URI (therefore we duplicate the existing
			 * URI's until the list is of size = total_primaries).
			 * Example: 2 URIs, 7 total segdbs.
			 * Original LocationList: URI1->URI2
			 * Modified LocationList: URI1->URI2->URI1->URI2->URI1->URI2->URI1
			 *
			 * (**) We also make sure that we don't allocate more segdbs than
			 * (# of URIs x gp_external_max_segs).
			 * Example: 2 URIs, 7 total segdbs, gp_external_max_segs = 3
			 * Original LocationList: URI1->URI2
			 * Modified LocationList: URI1->URI2->URI1->URI2->URI1->URI2 (6 total).
			 *
			 * (***) In that case that we need to allocate only a subset of primary
			 * segdbs and not all we then also create a random map of segments to skip.
			 * Using the previous example a we create a map of 7 entries and need to
			 * randomly select 1 segdb to skip (7 - 6 = 1). so it may look like this:
			 * [F F T F F F F] - in which case we know to skip the 3rd segment only.
			 */

			/* total num of segs that will participate in the external operation */
			num_segs_participating = total_primaries;

			/* max num of segs that are allowed to participate in the operation */
			if ((uri->protocol == URI_GPFDIST) || (uri->protocol == URI_GPFDISTS))
			{
				max_participants_allowed = list_length(ext->urilocations) *
					gp_external_max_segs;
			}
			else
			{
				/*
				 * for custom protocol, set max_participants_allowed to
				 * num_segs_participating so that assignment to segments will use
				 * all available segments
				 */
				max_participants_allowed = num_segs_participating;
			}

			elog(DEBUG5,
				 "num_segs_participating = %d. max_participants_allowed = %d. number of URIs = %d",
				 num_segs_participating, max_participants_allowed, list_length(ext->urilocations));

			/* see (**) above */
			if (num_segs_participating > max_participants_allowed)
			{
				total_to_skip = num_segs_participating - max_participants_allowed;
				num_segs_participating = max_participants_allowed;
				should_skip_randomly = true;

				elog(NOTICE, "External scan %s will utilize %d out "
					 "of %d segment databases",
					 (uri->protocol == URI_GPFDIST ? "from gpfdist(s) server" : "using custom protocol"),
					 num_segs_participating,
					 total_primaries);
			}

			if (list_length(ext->urilocations) > num_segs_participating)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("There are more external files (URLs) than primary "
								"segments that can read them. Found %d URLs and "
								"%d primary segments.",
								list_length(ext->urilocations),
								num_segs_participating)));

			/*
			 * restart location list and fill in new list until number of
			 * locations equals the number of segments participating in this
			 * action (see (*) above for more details).
			 */
			while (!done)
			{
				foreach(c, ext->urilocations)
				{
					char	   *uri_str = (char *) strVal(lfirst(c));

					/* append to a list of Value nodes, size nelems */
					modifiedloclist = lappend(modifiedloclist, makeString(pstrdup(uri_str)));

					if (list_length(modifiedloclist) == num_segs_participating)
					{
						done = true;
						break;
					}

					if (list_length(modifiedloclist) > num_segs_participating)
					{
						elog(ERROR, "External scan location list failed building distribution.");
					}
				}
			}

			/* See (***) above for details */
			if (should_skip_randomly)
				skip_map = makeRandomSegMap(total_primaries, total_to_skip);

			/*
			 * assign each URI from the new location list a primary segdb
			 */
			foreach(c, modifiedloclist)
			{
				const char *uri_str = strVal(lfirst(c));

				uri = ParseExternalTableUri(uri_str);

				found_candidate = false;
				found_match = false;

				/*
				 * look through our segment database list and try to find a
				 * database that can handle this uri.
				 */
				for (i = 0; i < db_info->total_segment_dbs && !found_match; i++)
				{
					CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
					int			segind = p->segindex;

					/*
					 * Assign mapping of external file to this segdb only if:
					 * 1) This segdb is a valid primary.
					 * 2) An external file wasn't already assigned to it.
					 */
					if (SEGMENT_IS_ACTIVE_PRIMARY(p))
					{
						/*
						 * skip this segdb if skip_map for this seg index tells us
						 * to skip it (set to 'true').
						 */
						if (should_skip_randomly)
						{
							Assert(segind < total_primaries);

							if (skip_map[segind])
								continue;	/* skip it */
						}

						/* a valid primary segdb exist on this host */
						found_candidate = true;

						if (segdb_file_map[segind] == NULL)
						{
							/* segdb not taken yet. assign this URI to this segdb */
							segdb_file_map[segind] = pstrdup(uri_str);
							found_match = true;
						}

						/*
						 * too bad. this segdb already has an external source
						 * assigned
						 */
					}
				}

				/* We failed to find a segdb for this gpfdist(s) URI */
				if (!found_match)
				{
					/* should never happen */
					elog(LOG, "external tables gpfdist(s) allocation error. "
						 "total_primaries: %d, num_segs_participating %d "
						 "max_participants_allowed %d, total_to_skip %d",
						 total_primaries, num_segs_participating,
						 max_participants_allowed, total_to_skip);

					ereport(ERROR,
							(errcode(ERRCODE_GP_INTERNAL_ERROR),
							 errmsg("Internal error in createplan for external tables"
									" when trying to assign segments for gpfdist(s)")));
				}
			}
		}
	}
	/* (3) */
	else if (using_execute)
	{
		const char *command = ext->command;
		const char *prefix = "execute:";
		char	   *prefixed_command;

		/* build the command string for the executor - 'execute:command' */
		StringInfo	buf = makeStringInfo();

		appendStringInfo(buf, "%s%s", prefix, command);
		prefixed_command = pstrdup(buf->data);

		pfree(buf->data);
		pfree(buf);
		buf = NULL;

		/*
		 * Now we handle each one of the ON locations separately:
		 *
		 * 1) all segs
		 * 2) one per host
		 * 3) all segs on host <foo>
		 * 4) seg <n> only
		 * 5) <n> random segs
		 * 6) master only
		 */
		if (strcmp(on_clause, "ALL_SEGMENTS") == 0)
		{
			/* all segments get a copy of the command to execute */

			for (i = 0; i < db_info->total_segment_dbs; i++)
			{
				CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
				int			segind = p->segindex;

				if (SEGMENT_IS_ACTIVE_PRIMARY(p))
					segdb_file_map[segind] = pstrdup(prefixed_command);
			}

		}
		else if (strcmp(on_clause, "PER_HOST") == 0)
		{
			/* 1 seg per host */

			List	   *visited_hosts = NIL;
			ListCell   *lc;

			for (i = 0; i < db_info->total_segment_dbs; i++)
			{
				CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
				int			segind = p->segindex;

				if (SEGMENT_IS_ACTIVE_PRIMARY(p))
				{
					bool		host_taken = false;

					foreach(lc, visited_hosts)
					{
						const char *hostname = strVal(lfirst(lc));

						if (pg_strcasecmp(hostname, p->hostname) == 0)
						{
							host_taken = true;
							break;
						}
					}

					/*
					 * if not assigned to a seg on this host before - do it
					 * now and add this hostname to the list so that we don't
					 * use segs on this host again.
					 */
					if (!host_taken)
					{
						segdb_file_map[segind] = pstrdup(prefixed_command);
						visited_hosts = lappend(visited_hosts,
										   makeString(pstrdup(p->hostname)));
					}
				}
			}
		}
		else if (strncmp(on_clause, "HOST:", strlen("HOST:")) == 0)
		{
			/* all segs on the specified host get copy of the command */
			char	   *hostname = on_clause + strlen("HOST:");
			bool		match_found = false;

			for (i = 0; i < db_info->total_segment_dbs; i++)
			{
				CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
				int			segind = p->segindex;

				if (SEGMENT_IS_ACTIVE_PRIMARY(p) &&
					pg_strcasecmp(hostname, p->hostname) == 0)
				{
					segdb_file_map[segind] = pstrdup(prefixed_command);
					match_found = true;
				}
			}

			if (!match_found)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("Could not assign a segment database for "
							  "command \"%s\". No valid primary segment was "
								"found in the requested host name \"%s\" ",
								command, hostname)));
		}
		else if (strncmp(on_clause, "SEGMENT_ID:", strlen("SEGMENT_ID:")) == 0)
		{
			/* 1 seg with specified id gets a copy of the command */
			int			target_segid = atoi(on_clause + strlen("SEGMENT_ID:"));
			bool		match_found = false;

			for (i = 0; i < db_info->total_segment_dbs; i++)
			{
				CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
				int			segind = p->segindex;

				if (SEGMENT_IS_ACTIVE_PRIMARY(p) && segind == target_segid)
				{
					segdb_file_map[segind] = pstrdup(prefixed_command);
					match_found = true;
				}
			}

			if (!match_found)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("Could not assign a segment database for "
								"command \"%s\". The requested segment id "
								"%d is not a valid primary segment or doesn't "
								"exist in the database", command, target_segid)));
		}
		else if (strncmp(on_clause, "TOTAL_SEGS:", strlen("TOTAL_SEGS:")) == 0)
		{
			/* total n segments selected randomly */

			int			num_segs_to_use = atoi(on_clause + strlen("TOTAL_SEGS:"));

			if (num_segs_to_use > total_primaries)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("Table defined with EXECUTE ON %d but there "
								"are only %d valid primary segments in the "
							"database.", num_segs_to_use, total_primaries)));

			total_to_skip = total_primaries - num_segs_to_use;
			skip_map = makeRandomSegMap(total_primaries, total_to_skip);

			for (i = 0; i < db_info->total_segment_dbs; i++)
			{
				CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
				int			segind = p->segindex;

				if (SEGMENT_IS_ACTIVE_PRIMARY(p))
				{
					Assert(segind < total_primaries);
					if (skip_map[segind])
						continue;		/* skip it */

					segdb_file_map[segind] = pstrdup(prefixed_command);
				}
			}
		}
		else if (strcmp(on_clause, "MASTER_ONLY") == 0)
		{
			/*
			 * store the command in first array entry and indicate that it is
			 * meant for the master segment (not seg o).
			 */
			segdb_file_map[0] = pstrdup(prefixed_command);
			*ismasteronly = true;
		}
		else
		{
			ereport(ERROR,
					(errcode(ERRCODE_GP_INTERNAL_ERROR),
					 errmsg("Internal error in createplan for external tables: "
							"got invalid ON clause code %s", on_clause)));
		}
	}
	/* (4) */
	else if (using_location && uri->protocol == URI_GPHDFS)
	{
		const char *uri_str = strVal(linitial(ext->urilocations));

		for (i = 0; i < db_info->total_segment_dbs; i++)
		{
			CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
			int			segind = p->segindex;

			segdb_file_map[segind] = pstrdup(uri_str);
		}
	}
	else
	{
		/* should never get here */
		ereport(ERROR,
				(errcode(ERRCODE_GP_INTERNAL_ERROR),
				 errmsg("Internal error in createplan for external tables")));
	}

	/*
	 * convert array map to a list so it can be serialized as part of the plan
	 */
	filenames = NIL;
	for (i = 0; i < total_primaries; i++)
	{
		if (segdb_file_map[i] != NULL)
			filenames = lappend(filenames, makeString(segdb_file_map[i]));
		else
		{
			/* no file for this segdb. add a null entry */
			Value	   *n = makeNode(Value);

			n->type = T_Null;
			filenames = lappend(filenames, n);
		}
	}

	freeCdbComponentDatabases(db_info);

	return filenames;
}


/*
 * create_indexscan_plan
 *	  Returns an indexscan plan for the base relation scanned by 'best_path'
 *	  with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 *
 * The indexquals list of the path contains implicitly-ANDed qual conditions.
 * The list can be empty --- then no index restrictions will be applied during
 * the scan.
 */
static IndexScan *
create_indexscan_plan(PlannerInfo *root,
					  IndexPath *best_path,
					  List *tlist,
					  List *scan_clauses)
{
	List	   *indexquals = best_path->indexquals;
	Index		baserelid = best_path->path.parent->relid;
	Oid			indexoid = best_path->indexinfo->indexoid;
	List	   *qpqual;
	List	   *stripped_indexquals;
	List	   *fixed_indexquals;
	ListCell   *l;
	IndexScan  *scan_plan;

	/* it should be a base rel... */
	Assert(baserelid > 0);
	Assert(best_path->path.parent->rtekind == RTE_RELATION);

	/*
	 * Build "stripped" indexquals structure (no RestrictInfos) to pass to
	 * executor as indexqualorig
	 */
	stripped_indexquals = get_actual_clauses(indexquals);

	/*
	 * The executor needs a copy with the indexkey on the left of each clause
	 * and with index attr numbers substituted for table ones.
	 */
	fixed_indexquals = fix_indexqual_references(indexquals, best_path);

	/*
	 * If this is an innerjoin scan, the indexclauses will contain join
	 * clauses that are not present in scan_clauses (since the passed-in value
	 * is just the rel's baserestrictinfo list).  We must add these clauses to
	 * scan_clauses to ensure they get checked.  In most cases we will remove
	 * the join clauses again below, but if a join clause contains a special
	 * operator, we need to make sure it gets into the scan_clauses.
	 *
	 * Note: pointer comparison should be enough to determine RestrictInfo
	 * matches.
	 */
	if (best_path->isjoininner)
		scan_clauses = list_union_ptr(scan_clauses, best_path->indexclauses);

	/*
	 * The qpqual list must contain all restrictions not automatically handled
	 * by the index.  All the predicates in the indexquals will be checked
	 * (either by the index itself, or by nodeIndexscan.c), but if there are
	 * any "special" operators involved then they must be included in qpqual.
	 * The upshot is that qpqual must contain scan_clauses minus whatever
	 * appears in indexquals.
	 *
	 * In normal cases simple pointer equality checks will be enough to spot
	 * duplicate RestrictInfos, so we try that first.  In some situations
	 * (particularly with OR'd index conditions) we may have scan_clauses that
	 * are not equal to, but are logically implied by, the index quals; so we
	 * also try a predicate_implied_by() check to see if we can discard quals
	 * that way.  (predicate_implied_by assumes its first input contains only
	 * immutable functions, so we have to check that.)
	 *
	 * We can also discard quals that are implied by a partial index's
	 * predicate, but only in a plain SELECT; when scanning a target relation
	 * of UPDATE/DELETE/SELECT FOR UPDATE, we must leave such quals in the
	 * plan so that they'll be properly rechecked by EvalPlanQual testing.
	 */
	qpqual = NIL;
	foreach(l, scan_clauses)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(l);

		Assert(IsA(rinfo, RestrictInfo));
		if (rinfo->pseudoconstant)
			continue;			/* we may drop pseudoconstants here */
		if (list_member_ptr(indexquals, rinfo))
			continue;
		if (!contain_mutable_functions((Node *) rinfo->clause))
		{
			List	   *clausel = list_make1(rinfo->clause);

			if (predicate_implied_by(clausel, indexquals))
				continue;
			if (best_path->indexinfo->indpred)
			{
				if (baserelid != root->parse->resultRelation &&
					get_rowmark(root->parse, baserelid) == NULL)
					if (predicate_implied_by(clausel,
											 best_path->indexinfo->indpred))
						continue;
			}
		}
		qpqual = lappend(qpqual, rinfo);
	}

	/* Sort clauses into best execution order */
	qpqual = order_qual_clauses(root, qpqual);

	/* Reduce RestrictInfo list to bare expressions; ignore pseudoconstants */
	qpqual = extract_actual_clauses(qpqual, false);

	/* Finally ready to build the plan node */
	scan_plan = make_indexscan(tlist,
							   qpqual,
							   baserelid,
							   indexoid,
							   fixed_indexquals,
							   stripped_indexquals,
							   best_path->indexscandir);

	copy_path_costsize(root, &scan_plan->scan.plan, &best_path->path);

	return scan_plan;
}

/*
 * create_bitmap_scan_plan
 *	  Returns a bitmap scan plan for the base relation scanned by 'best_path'
 *	  with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static BitmapHeapScan *
create_bitmap_scan_plan(PlannerInfo *root,
						BitmapHeapPath *best_path,
						List *tlist,
						List *scan_clauses)
{
	Index		baserelid = best_path->path.parent->relid;
	Plan	   *bitmapqualplan;
	List	   *bitmapqualorig;
	List	   *indexquals;
	List	   *qpqual;
	ListCell   *l;
	BitmapHeapScan *scan_plan;

	/* it should be a base rel... */
	Assert(baserelid > 0);
	Assert(best_path->path.parent->rtekind == RTE_RELATION);

	/* Process the bitmapqual tree into a Plan tree and qual lists */
	bitmapqualplan = create_bitmap_subplan(root, best_path->bitmapqual,
										   &bitmapqualorig, &indexquals);

	/* Reduce RestrictInfo list to bare expressions; ignore pseudoconstants */
	scan_clauses = extract_actual_clauses(scan_clauses, false);

	/*
	 * If this is a innerjoin scan, the indexclauses will contain join clauses
	 * that are not present in scan_clauses (since the passed-in value is just
	 * the rel's baserestrictinfo list).  We must add these clauses to
	 * scan_clauses to ensure they get checked.  In most cases we will remove
	 * the join clauses again below, but if a join clause contains a special
	 * operator, we need to make sure it gets into the scan_clauses.
	 */
	if (best_path->isjoininner)
	{
		scan_clauses = list_concat_unique(scan_clauses, bitmapqualorig);
	}

	/*
	 * The qpqual list must contain all restrictions not automatically handled
	 * by the index.  All the predicates in the indexquals will be checked
	 * (either by the index itself, or by nodeBitmapHeapscan.c), but if there
	 * are any "special" operators involved then they must be added to qpqual.
	 * The upshot is that qpqual must contain scan_clauses minus whatever
	 * appears in indexquals.
	 *
	 * In normal cases simple equal() checks will be enough to spot duplicate
	 * clauses, so we try that first.  In some situations (particularly with
	 * OR'd index conditions) we may have scan_clauses that are not equal to,
	 * but are logically implied by, the index quals; so we also try a
	 * predicate_implied_by() check to see if we can discard quals that way.
	 * (predicate_implied_by assumes its first input contains only immutable
	 * functions, so we have to check that.)
	 *
	 * Unlike create_indexscan_plan(), we need take no special thought here
	 * for partial index predicates; this is because the predicate conditions
	 * are already listed in bitmapqualorig and indexquals.  Bitmap scans have
	 * to do it that way because predicate conditions need to be rechecked if
	 * the scan becomes lossy.
	 */
	qpqual = NIL;
	foreach(l, scan_clauses)
	{
		Node	   *clause = (Node *) lfirst(l);

		if (list_member(indexquals, clause))
			continue;
		if (!contain_mutable_functions(clause))
		{
			List	   *clausel = list_make1(clause);

			if (predicate_implied_by(clausel, indexquals))
				continue;
		}
		qpqual = lappend(qpqual, clause);
	}

	/* Sort clauses into best execution order */
	qpqual = order_qual_clauses(root, qpqual);

	/*
	 * When dealing with special operators, we will at this point
	 * have duplicate clauses in qpqual and bitmapqualorig.  We may as well
	 * drop 'em from bitmapqualorig, since there's no point in making the
	 * tests twice.
	 */
	bitmapqualorig = list_difference_ptr(bitmapqualorig, qpqual);

	/*
	 * Copy the finished bitmapqualorig to make sure we have an independent
	 * copy --- needed in case there are subplans in the index quals
	 */
	bitmapqualorig = copyObject(bitmapqualorig);

	/* Finally ready to build the plan node */
	scan_plan = make_bitmap_heapscan(tlist,
									 qpqual,
									 bitmapqualplan,
									 bitmapqualorig,
									 baserelid);

	copy_path_costsize(root, &scan_plan->scan.plan, &best_path->path);

	return scan_plan;
}

/*
 * create_bitmap_appendonly_scan_plan
 *
 * NOTE: Copy of create_bitmap_scan_plan routine.
 */
static BitmapAppendOnlyScan *
create_bitmap_appendonly_scan_plan(PlannerInfo *root,
								   BitmapAppendOnlyPath *best_path,
								   List *tlist,
								   List *scan_clauses)
{
	Index		baserelid = best_path->path.parent->relid;
	Plan	   *bitmapqualplan;
	List	   *bitmapqualorig = NULL;
	List	   *indexquals = NULL;
	List	   *qpqual;
	ListCell   *l;
	BitmapAppendOnlyScan *scan_plan;

	/* it should be a base rel... */
	Assert(baserelid > 0);
	Assert(best_path->path.parent->rtekind == RTE_RELATION);

	/* Process the bitmapqual tree into a Plan tree and qual lists */
	bitmapqualplan = create_bitmap_subplan(root, best_path->bitmapqual,
										   &bitmapqualorig, &indexquals);

	/* Reduce RestrictInfo list to bare expressions; ignore pseudoconstants */
	scan_clauses = extract_actual_clauses(scan_clauses, false);

	/*
	 * If this is a innerjoin scan, the indexclauses will contain join clauses
	 * that are not present in scan_clauses (since the passed-in value is just
	 * the rel's baserestrictinfo list).  We must add these clauses to
	 * scan_clauses to ensure they get checked.  In most cases we will remove
	 * the join clauses again below, but if a join clause contains a special
	 * operator, we need to make sure it gets into the scan_clauses.
	 */
	if (best_path->isjoininner)
	{
		scan_clauses = list_concat_unique(scan_clauses, bitmapqualorig);
	}

	/*
	 * The qpqual list must contain all restrictions not automatically handled
	 * by the index.  All the predicates in the indexquals will be checked
	 * (either by the index itself, or by nodeBitmapHeapscan.c), but if there
	 * are any "special" or lossy operators involved then they must be added
	 * to qpqual.  The upshot is that qpqual must contain scan_clauses minus
	 * whatever appears in indexquals.
	 *
	 * In normal cases simple equal() checks will be enough to spot duplicate
	 * clauses, so we try that first.  In some situations (particularly with
	 * OR'd index conditions) we may have scan_clauses that are not equal to,
	 * but are logically implied by, the index quals; so we also try a
	 * predicate_implied_by() check to see if we can discard quals that way.
	 * (predicate_implied_by assumes its first input contains only immutable
	 * functions, so we have to check that.)
	 *
	 * Unlike create_indexscan_plan(), we need take no special thought here
	 * for partial index predicates; this is because the predicate conditions
	 * are already listed in bitmapqualorig and indexquals.  Bitmap scans have
	 * to do it that way because predicate conditions need to be rechecked if
	 * the scan becomes lossy.
	 */
	qpqual = NIL;
	foreach(l, scan_clauses)
	{
		Node	   *clause = (Node *) lfirst(l);

		if (list_member(indexquals, clause))
			continue;
		if (!contain_mutable_functions(clause))
		{
			List	   *clausel = list_make1(clause);

			if (predicate_implied_by(clausel, indexquals))
				continue;
		}
		qpqual = lappend(qpqual, clause);
	}

	/* Sort clauses into best execution order */
	qpqual = order_qual_clauses(root, qpqual);

	/*
	 * When dealing with special or lossy operators, we will at this point
	 * have duplicate clauses in qpqual and bitmapqualorig.  We may as well
	 * drop 'em from bitmapqualorig, since there's no point in making the
	 * tests twice.
	 */
	bitmapqualorig = list_difference_ptr(bitmapqualorig, qpqual);

	/* Finally ready to build the plan node */
	scan_plan = make_bitmap_appendonlyscan(tlist,
										   qpqual,
										   bitmapqualplan,
										   bitmapqualorig,
										   baserelid,
										   best_path->isAORow);

	copy_path_costsize(root, &scan_plan->scan.plan, &best_path->path);

	return scan_plan;
}

/*
 * Given a bitmapqual tree, generate the Plan tree that implements it
 *
 * As byproducts, we also return in *qual and *indexqual the qual lists
 * (in implicit-AND form, without RestrictInfos) describing the original index
 * conditions and the generated indexqual conditions.  (These are the same in
 * simple cases, but when special index operators are involved, the former
 * list includes the special conditions while the latter includes the actual
 * indexable conditions derived from them.)  Both lists include partial-index
 * predicates, because we have to recheck predicates as well as index
 * conditions if the bitmap scan becomes lossy.
 *
 * Note: if you find yourself changing this, you probably need to change
 * make_restrictinfo_from_bitmapqual too.
 */
static Plan *
create_bitmap_subplan(PlannerInfo *root, Path *bitmapqual,
					  List **qual, List **indexqual)
{
	Plan	   *plan;

	if (IsA(bitmapqual, BitmapAndPath))
	{
		BitmapAndPath *apath = (BitmapAndPath *) bitmapqual;
		List	   *subplans = NIL;
		List	   *subquals = NIL;
		List	   *subindexquals = NIL;
		ListCell   *l;

		/*
		 * There may well be redundant quals among the subplans, since a
		 * top-level WHERE qual might have gotten used to form several
		 * different index quals.  We don't try exceedingly hard to eliminate
		 * redundancies, but we do eliminate obvious duplicates by using
		 * list_concat_unique.
		 */
		foreach(l, apath->bitmapquals)
		{
			Plan	   *subplan;
			List	   *subqual;
			List	   *subindexqual;

			subplan = create_bitmap_subplan(root, (Path *) lfirst(l),
											&subqual, &subindexqual);
			subplans = lappend(subplans, subplan);
			subquals = list_concat_unique(subquals, subqual);
			subindexquals = list_concat_unique(subindexquals, subindexqual);
		}
		plan = (Plan *) make_bitmap_and(subplans);
		plan->startup_cost = apath->path.startup_cost;
		plan->total_cost = apath->path.total_cost;
		plan->plan_rows =
			clamp_row_est(apath->bitmapselectivity * apath->path.parent->tuples);
		plan->plan_width = 0;	/* meaningless */
		*qual = subquals;
		*indexqual = subindexquals;
	}
	else if (IsA(bitmapqual, BitmapOrPath))
	{
		BitmapOrPath *opath = (BitmapOrPath *) bitmapqual;
		List	   *subplans = NIL;
		List	   *subquals = NIL;
		List	   *subindexquals = NIL;
		bool		const_true_subqual = false;
		bool		const_true_subindexqual = false;
		ListCell   *l;

		/*
		 * Here, we only detect qual-free subplans.  A qual-free subplan would
		 * cause us to generate "... OR true ..."  which we may as well reduce
		 * to just "true".	We do not try to eliminate redundant subclauses
		 * because (a) it's not as likely as in the AND case, and (b) we might
		 * well be working with hundreds or even thousands of OR conditions,
		 * perhaps from a long IN list.  The performance of list_append_unique
		 * would be unacceptable.
		 */
		foreach(l, opath->bitmapquals)
		{
			Plan	   *subplan;
			List	   *subqual;
			List	   *subindexqual;

			subplan = create_bitmap_subplan(root, (Path *) lfirst(l),
											&subqual, &subindexqual);
			subplans = lappend(subplans, subplan);
			if (subqual == NIL)
				const_true_subqual = true;
			else if (!const_true_subqual)
				subquals = lappend(subquals,
								   make_ands_explicit(subqual));
			if (subindexqual == NIL)
				const_true_subindexqual = true;
			else if (!const_true_subindexqual)
				subindexquals = lappend(subindexquals,
										make_ands_explicit(subindexqual));
		}

		/*
		 * In the presence of ScalarArrayOpExpr quals, we might have built
		 * BitmapOrPaths with just one subpath; don't add an OR step.
		 */
		if (list_length(subplans) == 1)
		{
			plan = (Plan *) linitial(subplans);
		}
		else
		{
			plan = (Plan *) make_bitmap_or(subplans);
			plan->startup_cost = opath->path.startup_cost;
			plan->total_cost = opath->path.total_cost;
			plan->plan_rows =
				clamp_row_est(opath->bitmapselectivity * opath->path.parent->tuples);
			plan->plan_width = 0;		/* meaningless */
		}

		/*
		 * If there were constant-TRUE subquals, the OR reduces to constant
		 * TRUE.  Also, avoid generating one-element ORs, which could happen
		 * due to redundancy elimination or ScalarArrayOpExpr quals.
		 */
		if (const_true_subqual)
			*qual = NIL;
		else if (list_length(subquals) <= 1)
			*qual = subquals;
		else
			*qual = list_make1(make_orclause(subquals));
		if (const_true_subindexqual)
			*indexqual = NIL;
		else if (list_length(subindexquals) <= 1)
			*indexqual = subindexquals;
		else
			*indexqual = list_make1(make_orclause(subindexquals));
	}
	else if (IsA(bitmapqual, IndexPath))
	{
		IndexPath  *ipath = (IndexPath *) bitmapqual;
		IndexScan  *iscan;
		ListCell   *l;

		/* Use the regular indexscan plan build machinery... */
		iscan = create_indexscan_plan(root, ipath, NIL, NIL);
		/* then convert to a bitmap indexscan */
		plan = (Plan *) make_bitmap_indexscan(iscan->scan.scanrelid,
											  iscan->indexid,
											  iscan->indexqual,
											  iscan->indexqualorig);
		plan->startup_cost = 0.0;
		plan->total_cost = ipath->indextotalcost;
		plan->plan_rows =
			clamp_row_est(ipath->indexselectivity * ipath->path.parent->tuples);
		plan->plan_width = 0;	/* meaningless */
		*qual = get_actual_clauses(ipath->indexclauses);
		*indexqual = get_actual_clauses(ipath->indexquals);
		foreach(l, ipath->indexinfo->indpred)
		{
			Expr	   *pred = (Expr *) lfirst(l);

			/*
			 * We know that the index predicate must have been implied by the
			 * query condition as a whole, but it may or may not be implied by
			 * the conditions that got pushed into the bitmapqual.	Avoid
			 * generating redundant conditions.
			 */
			if (!predicate_implied_by(list_make1(pred), ipath->indexclauses))
			{
				*qual = lappend(*qual, pred);
				*indexqual = lappend(*indexqual, pred);
			}
		}
	}
	else
	{
		elog(ERROR, "unrecognized node type: %d", nodeTag(bitmapqual));
		plan = NULL;			/* keep compiler quiet */
	}

	return plan;
}

/*
 * create_tidscan_plan
 *	 Returns a tidscan plan for the base relation scanned by 'best_path'
 *	 with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static TidScan *
create_tidscan_plan(PlannerInfo *root, TidPath *best_path,
					List *tlist, List *scan_clauses)
{
	TidScan    *scan_plan;
	Index		scan_relid = best_path->path.parent->relid;
	List	   *ortidquals;

	/* it should be a base rel... */
	Assert(scan_relid > 0);
	Assert(best_path->path.parent->rtekind == RTE_RELATION);

	/* Sort clauses into best execution order */
	scan_clauses = order_qual_clauses(root, scan_clauses);

	/* Reduce RestrictInfo list to bare expressions; ignore pseudoconstants */
	scan_clauses = extract_actual_clauses(scan_clauses, false);

	/*
	 * Remove any clauses that are TID quals.  This is a bit tricky since the
	 * tidquals list has implicit OR semantics.
	 *
	 * In the case of CURRENT OF, however, we do want the CurrentOfExpr to 
	 * reside in both the tidlist and the qual, as CurrentOfExpr is effectively
	 * a ctid, gp_segment_id, and tableoid qual. Constant folding will
	 * finish up this qual rewriting to ensure what we dispatch is a sane interpretation
	 * of CURRENT OF behavior.
	 */
	if (!(list_length(scan_clauses) == 1 && IsA(linitial(scan_clauses), CurrentOfExpr)))
	{
		ortidquals = best_path->tidquals;
		if (list_length(ortidquals) > 1)
			ortidquals = list_make1(make_orclause(ortidquals));
		scan_clauses = list_difference(scan_clauses, ortidquals);
	}

	scan_plan = make_tidscan(tlist,
							 scan_clauses,
							 scan_relid,
							 best_path->tidquals);

	copy_path_costsize(root, &scan_plan->scan.plan, &best_path->path);

	return scan_plan;
}

/*
 * create_subqueryscan_plan
 *	 Returns a subqueryscan plan for the base relation scanned by 'best_path'
 *	 with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static SubqueryScan *
create_subqueryscan_plan(PlannerInfo *root, Path *best_path,
						 List *tlist, List *scan_clauses)
{
	SubqueryScan *scan_plan;
	Index		scan_relid = best_path->parent->relid;

	/* it should be a subquery base rel... */
	Assert(scan_relid > 0);
	Assert(best_path->parent->rtekind == RTE_SUBQUERY);

	/* Sort clauses into best execution order */
	scan_clauses = order_qual_clauses(root, scan_clauses);

	/* Reduce RestrictInfo list to bare expressions; ignore pseudoconstants */
	scan_clauses = extract_actual_clauses(scan_clauses, false);

	scan_plan = make_subqueryscan(root, tlist,
								  scan_clauses,
								  scan_relid,
								  best_path->parent->subplan,
								  best_path->parent->subrtable);

	copy_path_costsize(root, &scan_plan->scan.plan, best_path);

	return scan_plan;
}

/*
 * create_functionscan_plan
 *	 Returns a functionscan plan for the base relation scanned by 'best_path'
 *	 with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static FunctionScan *
create_functionscan_plan(PlannerInfo *root, Path *best_path,
						 List *tlist, List *scan_clauses)
{
	FunctionScan *scan_plan;
	Index		scan_relid = best_path->parent->relid;
	RangeTblEntry *rte;

	/* it should be a function base rel... */
	Assert(scan_relid > 0);
	rte = planner_rt_fetch(scan_relid, root);
	Assert(rte->rtekind == RTE_FUNCTION);

	/* Sort clauses into best execution order */
	scan_clauses = order_qual_clauses(root, scan_clauses);

	/* Reduce RestrictInfo list to bare expressions; ignore pseudoconstants */
	scan_clauses = extract_actual_clauses(scan_clauses, false);

	scan_plan = make_functionscan(tlist, scan_clauses, scan_relid,
								  rte->funcexpr,
								  rte->eref->colnames,
								  rte->funccoltypes,
								  rte->funccoltypmods);

	copy_path_costsize(root, &scan_plan->scan.plan, best_path);

	return scan_plan;
}

/*
 * create_tablefunction_plan
 *	 Returns a TableFunction plan for the base relation scanned by 'best_path'
 *	 with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static TableFunctionScan *
create_tablefunction_plan(PlannerInfo *root,
						  Path *best_path,
						  List *tlist,
						  List *scan_clauses)
{
	TableFunctionScan *tablefunc;
	Plan	   *subplan = best_path->parent->subplan;
	List	   *subrtable = best_path->parent->subrtable;
	Index		scan_relid = best_path->parent->relid;

	/* it should be a function base rel... */
	Assert(scan_relid > 0);
	Assert(best_path->parent->rtekind == RTE_TABLEFUNCTION);

	/* Reduce RestrictInfo list to bare expressions; ignore pseudoconstants */
	scan_clauses = extract_actual_clauses(scan_clauses, false);

	/* Create the TableFunctionScan plan */
	tablefunc = make_tablefunction(tlist, scan_clauses, subplan, subrtable,
								   scan_relid);

	/* Cost is determined largely by the cost of the underlying subplan */
	copy_plan_costsize(&tablefunc->scan.plan, subplan);

	copy_path_costsize(root, &tablefunc->scan.plan, best_path);

	return tablefunc;
}

/*
 * create_valuesscan_plan
 *	 Returns a valuesscan plan for the base relation scanned by 'best_path'
 *	 with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static ValuesScan *
create_valuesscan_plan(PlannerInfo *root, Path *best_path,
					   List *tlist, List *scan_clauses)
{
	ValuesScan *scan_plan;
	Index		scan_relid = best_path->parent->relid;
	RangeTblEntry *rte;

	/* it should be a values base rel... */
	Assert(scan_relid > 0);
	rte = planner_rt_fetch(scan_relid, root);
	Assert(rte->rtekind == RTE_VALUES);

	/* Sort clauses into best execution order */
	scan_clauses = order_qual_clauses(root, scan_clauses);

	/* Reduce RestrictInfo list to bare expressions; ignore pseudoconstants */
	scan_clauses = extract_actual_clauses(scan_clauses, false);

	scan_plan = make_valuesscan(tlist, scan_clauses, scan_relid,
								rte->values_lists);

	copy_path_costsize(root, &scan_plan->scan.plan, best_path);

	return scan_plan;
}

/*
 * create_ctescan_plan
 *	 Returns a ctescan plan for the base relation scanned by 'best_path'
 *	 with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static SubqueryScan *
create_ctescan_plan(PlannerInfo *root, Path *best_path,
					List *tlist, List *scan_clauses)
{
	Index		scan_relid = best_path->parent->relid;
	SubqueryScan *scan_plan;

	Assert(best_path->parent->rtekind == RTE_CTE);

	Assert(scan_relid > 0);

	/* Sort clauses into best execution order */
	scan_clauses = order_qual_clauses(root, scan_clauses);

	/* Reduce RestrictInfo list to bare expressions; ignore pseudoconstants */
	scan_clauses = extract_actual_clauses(scan_clauses, false);

	scan_plan = make_subqueryscan(root, tlist,
								  scan_clauses,
								  scan_relid,
								  best_path->parent->subplan,
								  best_path->parent->subrtable);

	copy_path_costsize(root, &scan_plan->scan.plan, best_path);

	return scan_plan;
}

/*
 * create_worktablescan_plan
 *	 Returns a worktablescan plan for the base relation scanned by 'best_path'
 *	 with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static WorkTableScan *
create_worktablescan_plan(PlannerInfo *root, Path *best_path,
						  List *tlist, List *scan_clauses)
{
	WorkTableScan *scan_plan;
	Index		scan_relid = best_path->parent->relid;
	RangeTblEntry *rte;
	Index		levelsup;
	PlannerInfo *cteroot;

	Assert(scan_relid > 0);
	rte = planner_rt_fetch(scan_relid, root);
	Assert(rte->rtekind == RTE_CTE);
	Assert(rte->self_reference);

	/*
	 * We need to find the worktable param ID, which is in the plan level
	 * that's processing the recursive UNION, which is one level *below*
	 * where the CTE comes from.
	 */
	levelsup = rte->ctelevelsup;
	if (levelsup == 0)			/* shouldn't happen */
			elog(ERROR, "bad levelsup for CTE \"%s\"", rte->ctename);
	levelsup--;
	cteroot = root;
	while (levelsup-- > 0)
	{
		cteroot = cteroot->parent_root;
		if (!cteroot)			/* shouldn't happen */
			elog(ERROR, "bad levelsup for CTE \"%s\"", rte->ctename);
	}
	if (cteroot->wt_param_id < 0)	/* shouldn't happen */
		elog(ERROR, "could not find param ID for CTE \"%s\"", rte->ctename);

	/* Sort clauses into best execution order */
	scan_clauses = order_qual_clauses(root, scan_clauses);

	/* Reduce RestrictInfo list to bare expressions; ignore pseudoconstants */
	scan_clauses = extract_actual_clauses(scan_clauses, false);

	scan_plan = make_worktablescan(tlist, scan_clauses, scan_relid,
								   cteroot->wt_param_id);

	copy_path_costsize(root, &scan_plan->scan.plan, best_path);

	return scan_plan;
}

static Expr *
remove_isnotfalse_expr(Expr *expr)
{
	if (IsA(expr, BooleanTest))
	{
		BooleanTest *bt = (BooleanTest *) expr;

		if (bt->booltesttype == IS_NOT_FALSE)
		{
			return bt->arg;
		}
	}
	return expr;
}

/*
 * remove_isnotfalse
 *	  Given a list of joinclauses, extract the bare clauses, removing any IS_NOT_FALSE
 *	  additions. The original data structure is not touched; a modified list is returned
 */
static List *
remove_isnotfalse(List *clauses)
{
	List	   *t_list = NIL;
	ListCell   *l;

	foreach(l, clauses)
	{
		Node	   *node = (Node *) lfirst(l);

		if (IsA(node, Expr) ||IsA(node, BooleanTest))
		{
			Expr	   *expr = (Expr *) node;

			expr = remove_isnotfalse_expr(expr);
			t_list = lappend(t_list, expr);
		}
		else if (IsA(node, RestrictInfo))
		{
			RestrictInfo *restrictinfo = (RestrictInfo *) node;
			Expr	   *rclause = restrictinfo->clause;

			rclause = remove_isnotfalse_expr(rclause);
			t_list = lappend(t_list, rclause);
		}
		else
		{
			t_list = lappend(t_list, node);
		}
	}
	return t_list;
}

/*****************************************************************************
 *
 *	JOIN METHODS
 *
 *****************************************************************************/

static NestLoop *
create_nestloop_plan(PlannerInfo *root,
					 NestPath *best_path,
					 Plan *outer_plan,
					 Plan *inner_plan)
{
	List	   *tlist = build_relation_tlist(best_path->path.parent);
	List	   *joinrestrictclauses = best_path->joinrestrictinfo;
	List	   *joinclauses;
	List	   *otherclauses;
	NestLoop   *join_plan;

	bool		prefetch = false;

	/*
	 * MPP-1459: subqueries are resolved after our deadlock checks in
	 * pathnode.c; so we have to check here to make sure that we catch all
	 * motion deadlocks.
	 *
	 * MPP-1487: if there is already a materialize node here, we don't want to
	 * insert another one. :-)
	 *
	 * NOTE: materialize_finished_plan() does *almost* what we want -- except
	 * we aren't finished.
	 */
	if (!IsA(best_path->innerjoinpath, MaterialPath) &&
		(best_path->innerjoinpath->motionHazard ||
		 !best_path->innerjoinpath->rescannable))
	{
		Material   *mat;
		Path		matpath;	/* dummy for cost fixup */

		mat = make_material(inner_plan);

		/* Set cost data */
		cost_material(&matpath,
					  root,
					  inner_plan->total_cost,
					  inner_plan->plan_rows,
					  inner_plan->plan_width);
		mat->plan.startup_cost = matpath.startup_cost;
		mat->plan.total_cost = matpath.total_cost;
		mat->plan.plan_rows = inner_plan->plan_rows;
		mat->plan.plan_width = inner_plan->plan_width;

		if (best_path->outerjoinpath->motionHazard)
		{
			mat->cdb_strict = true;
			prefetch = true;
		}

		inner_plan = (Plan *) mat;
	}

	/*
	 * MPP-1657: if there is already a materialize here, we may need to update
	 * its strictness.
	 */
	else if (IsA(best_path->innerjoinpath, MaterialPath) &&
			 best_path->innerjoinpath->motionHazard &&
			 best_path->outerjoinpath->motionHazard)
	{
		Material   *mat = (Material *) inner_plan;

		prefetch = true;
		mat->cdb_strict = true;
	}

	if (IsA(best_path->innerjoinpath, IndexPath))
	{
		/*
		 * An index is being used to reduce the number of tuples scanned in
		 * the inner relation.	If there are join clauses being used with the
		 * index, we may remove those join clauses from the list of clauses
		 * that have to be checked as qpquals at the join node.
		 *
		 * We can also remove any join clauses that are redundant with those
		 * being used in the index scan; this check is needed because
		 * find_eclass_clauses_for_index_join() may emit different clauses
		 * than generate_join_implied_equalities() did.
		 *
		 * We can skip this if the index path is an ordinary indexpath and not
		 * a special innerjoin path, since it then wouldn't be using any join
		 * clauses.
		 */
		IndexPath  *innerpath = (IndexPath *) best_path->innerjoinpath;

		if (innerpath->isjoininner)
			joinrestrictclauses =
				select_nonredundant_join_clauses(root,
												 joinrestrictclauses,
												 innerpath->indexclauses);
	}
	else if (IsA(best_path->innerjoinpath, BitmapHeapPath) ||
			 IsA(best_path->innerjoinpath, BitmapAppendOnlyPath))
	{
		/*
		 * Same deal for bitmapped index scans.
		 *
		 * Note: both here and above, we ignore any implicit index
		 * restrictions associated with the use of partial indexes.  This is
		 * OK because we're only trying to prove we can dispense with some
		 * join quals; failing to prove that doesn't result in an incorrect
		 * plan.  It is the right way to proceed because adding more quals to
		 * the stuff we got from the original query would just make it harder
		 * to detect duplication.  (Also, to change this we'd have to be wary
		 * of UPDATE/DELETE/SELECT FOR UPDATE target relations; see notes
		 * above about EvalPlanQual.)
		 */
		bool		isjoininner = false;
		Path	   *bitmapqual = NULL;

		if (IsA(best_path->innerjoinpath, BitmapHeapPath))
		{
			BitmapHeapPath *innerpath = (BitmapHeapPath *) best_path->innerjoinpath;

			isjoininner = innerpath->isjoininner;
			bitmapqual = innerpath->bitmapqual;
		}
		else
		{
			BitmapAppendOnlyPath *innerpath = (BitmapAppendOnlyPath *) best_path->innerjoinpath;

			Assert(IsA(best_path->innerjoinpath, BitmapAppendOnlyPath));
			isjoininner = innerpath->isjoininner;
			bitmapqual = innerpath->bitmapqual;
		}

		if (isjoininner)
		{
			List	   *bitmapclauses;

			bitmapclauses =
				make_restrictinfo_from_bitmapqual(bitmapqual,
												  true,
												  false);
			joinrestrictclauses =
				select_nonredundant_join_clauses(root,
												 joinrestrictclauses,
												 bitmapclauses);
		}
	}

	/* Sort join qual clauses into best execution order */
	joinrestrictclauses = order_qual_clauses(root, joinrestrictclauses);

	/* Get the join qual clauses (in plain expression form) */
	/* Any pseudoconstant clauses are ignored here */
	if (IS_OUTER_JOIN(best_path->jointype))
	{
		extract_actual_join_clauses(joinrestrictclauses,
									&joinclauses, &otherclauses);
	}
	else
	{
		/* We can treat all clauses alike for an inner join */
		joinclauses = extract_actual_clauses(joinrestrictclauses, false);
		otherclauses = NIL;
	}

	if (best_path->jointype == JOIN_LASJ_NOTIN)
	{
		joinclauses = remove_isnotfalse(joinclauses);
	}

	join_plan = make_nestloop(tlist,
							  joinclauses,
							  otherclauses,
							  outer_plan,
							  inner_plan,
							  best_path->jointype);

	copy_path_costsize(root, &join_plan->join.plan, &best_path->path);

	if (IsA(best_path->innerjoinpath, MaterialPath))
	{
		MaterialPath *mp = (MaterialPath *) best_path->innerjoinpath;

		if (mp->cdb_strict)
			prefetch = true;
	}

	if (prefetch)
		join_plan->join.prefetch_inner = true;

	return join_plan;
}

static MergeJoin *
create_mergejoin_plan(PlannerInfo *root,
					  MergePath *best_path,
					  Plan *outer_plan,
					  Plan *inner_plan)
{
	List	   *tlist = build_relation_tlist(best_path->jpath.path.parent);
	List	   *joinclauses;
	List	   *otherclauses;
	List	   *mergeclauses;
	Sort	   *sort;
	bool		prefetch = false;
	List	   *outerpathkeys;
	List	   *innerpathkeys;
	int			nClauses;
	Oid		   *mergefamilies;
	int		   *mergestrategies;
	bool	   *mergenullsfirst;
	MergeJoin  *join_plan;
	int			i;
	ListCell   *lc;
	ListCell   *lop;
	ListCell   *lip;

	/* Sort join qual clauses into best execution order */
	/* NB: do NOT reorder the mergeclauses */
	joinclauses = order_qual_clauses(root, best_path->jpath.joinrestrictinfo);

	/* Get the join qual clauses (in plain expression form) */
	/* Any pseudoconstant clauses are ignored here */
	if (IS_OUTER_JOIN(best_path->jpath.jointype))
	{
		extract_actual_join_clauses(joinclauses,
									&joinclauses, &otherclauses);
	}
	else
	{
		/* We can treat all clauses alike for an inner join */
		joinclauses = extract_actual_clauses(joinclauses, false);
		otherclauses = NIL;
	}

	/*
	 * Remove the mergeclauses from the list of join qual clauses, leaving the
	 * list of quals that must be checked as qpquals.
	 */
	mergeclauses = get_actual_clauses(best_path->path_mergeclauses);
	joinclauses = list_difference(joinclauses, mergeclauses);

	/*
	 * Rearrange mergeclauses, if needed, so that the outer variable is always
	 * on the left; mark the mergeclause restrictinfos with correct
	 * outer_is_left status.
	 */
	mergeclauses = get_switched_clauses(best_path->path_mergeclauses,
							 best_path->jpath.outerjoinpath->parent->relids);

	/*
	 * Create explicit sort nodes for the outer and inner join paths if
	 * necessary.  The sort cost was already accounted for in the path. Make
	 * sure there are no excess columns in the inputs if sorting.
	 */
	if (best_path->outersortkeys)
	{
		disuse_physical_tlist(outer_plan, best_path->jpath.outerjoinpath);
		sort =
			make_sort_from_pathkeys(root,
									outer_plan,
									best_path->outersortkeys,
									-1.0,
									true);
		if (sort)
			outer_plan = (Plan *) sort;
		outerpathkeys = best_path->outersortkeys;
	}
	else
		outerpathkeys = best_path->jpath.outerjoinpath->pathkeys;

	if (best_path->innersortkeys)
	{
		disuse_physical_tlist(inner_plan, best_path->jpath.innerjoinpath);
		sort =
			make_sort_from_pathkeys(root,
									inner_plan,
									best_path->innersortkeys,
									-1.0,
									true);
		if (sort)
			inner_plan = (Plan *) sort;
		innerpathkeys = best_path->innersortkeys;
	}
	else
		innerpathkeys = best_path->jpath.innerjoinpath->pathkeys;

	/*
	 * MPP-3300: very similar to the nested-loop join motion deadlock cases. But we may have already
	 * put some slackening operators below (e.g. a sort).
	 *
	 * We need some kind of strict slackening operator (something which consumes all of its
	 * input before producing a row of output) for our inner. And we need to prefetch that side
	 * first.
	 *
	 * See motion_sanity_walker() for details on how a deadlock may occur.
	 */
	if (best_path->jpath.outerjoinpath->motionHazard && best_path->jpath.innerjoinpath->motionHazard)
	{
		prefetch = true;
		if (!IsA(inner_plan, Sort))
		{
			if (IsA(inner_plan, Material))
			{
				((Material *) inner_plan)->cdb_strict = true;
			}
			else
			{
				Material   *mat;

				/* need to add slack. */
				mat = make_material(inner_plan);
				mat->cdb_strict = true;
				inner_plan = (Plan *) mat;
			}
		}
	}

	/*
	 * If inner plan is a sort that is expected to spill to disk, add a
	 * materialize node to shield it from the need to handle mark/restore.
	 * This will allow it to perform the last merge pass on-the-fly, while in
	 * most cases not requiring the materialize to spill to disk.
	 *
	 * XXX really, Sort oughta do this for itself, probably, to avoid the
	 * overhead of a separate plan node.
	 */
	if (IsA(inner_plan, Sort) &&
		sort_exceeds_work_mem((Sort *) inner_plan))
	{
		Plan	   *matplan = (Plan *) make_material(inner_plan);

		/*
		 * We assume the materialize will not spill to disk, and therefore
		 * charge just cpu_tuple_cost per tuple.  (Keep this estimate in sync
		 * with similar ones in cost_mergejoin and create_mergejoin_path.)
		 */
		copy_plan_costsize(matplan, inner_plan);
		matplan->total_cost += cpu_tuple_cost * matplan->plan_rows;

		inner_plan = matplan;
	}

	/*
	 * Compute the opfamily/strategy/nullsfirst arrays needed by the executor.
	 * The information is in the pathkeys for the two inputs, but we need to
	 * be careful about the possibility of mergeclauses sharing a pathkey
	 * (compare find_mergeclauses_for_pathkeys()).
	 */
	nClauses = list_length(mergeclauses);
	Assert(nClauses == list_length(best_path->path_mergeclauses));
	mergefamilies = (Oid *) palloc(nClauses * sizeof(Oid));
	mergestrategies = (int *) palloc(nClauses * sizeof(int));
	mergenullsfirst = (bool *) palloc(nClauses * sizeof(bool));

	lop = list_head(outerpathkeys);
	lip = list_head(innerpathkeys);
	i = 0;
	foreach(lc, best_path->path_mergeclauses)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(lc);
		EquivalenceClass *oeclass;
		EquivalenceClass *ieclass;
		PathKey    *opathkey;
		PathKey    *ipathkey;
		EquivalenceClass *opeclass;
		EquivalenceClass *ipeclass;
		ListCell   *l2;

		/* fetch outer/inner eclass from mergeclause */
		Assert(IsA(rinfo, RestrictInfo));
		if (rinfo->outer_is_left)
		{
			oeclass = rinfo->left_ec;
			ieclass = rinfo->right_ec;
		}
		else
		{
			oeclass = rinfo->right_ec;
			ieclass = rinfo->left_ec;
		}
		Assert(oeclass != NULL);
		Assert(ieclass != NULL);

		/*
		 * For debugging purposes, we check that the eclasses match the
		 * paths' pathkeys.  In typical cases the merge clauses are one-to-one
		 * with the pathkeys, but when dealing with partially redundant query
		 * conditions, we might have clauses that re-reference earlier path
		 * keys.  The case that we need to reject is where a pathkey is
		 * entirely skipped over.
		 *
		 * lop and lip reference the first as-yet-unused pathkey elements;
		 * it's okay to match them, or any element before them.  If they're
		 * NULL then we have found all pathkey elements to be used.
		 */
		if (lop)
		{
			opathkey = (PathKey *) lfirst(lop);
			opeclass = opathkey->pk_eclass;
			if (oeclass == opeclass)
			{
				/* fast path for typical case */
				lop = lnext(lop);
			}
			else
			{
				/* redundant clauses ... must match something before lop */
				foreach(l2, outerpathkeys)
				{
					if (l2 == lop)
						break;
					opathkey = (PathKey *) lfirst(l2);
					opeclass = opathkey->pk_eclass;
					if (oeclass == opeclass)
						break;
				}
				if (oeclass != opeclass)
					elog(ERROR, "outer pathkeys do not match mergeclauses");
			}
		}
		else
		{
			/* redundant clauses ... must match some already-used pathkey */
			opathkey = NULL;
			opeclass = NULL;
			foreach(l2, outerpathkeys)
			{
				opathkey = (PathKey *) lfirst(l2);
				opeclass = opathkey->pk_eclass;
				if (oeclass == opeclass)
					break;
			}
			if (l2 == NULL)
				elog(ERROR, "outer pathkeys do not match mergeclauses");
		}

		if (lip)
		{
			ipathkey = (PathKey *) lfirst(lip);
			ipeclass = ipathkey->pk_eclass;
			if (ieclass == ipeclass)
			{
				/* fast path for typical case */
				lip = lnext(lip);
			}
			else
			{
				/* redundant clauses ... must match something before lip */
				foreach(l2, innerpathkeys)
				{
					if (l2 == lip)
						break;
					ipathkey = (PathKey *) lfirst(l2);
					ipeclass = ipathkey->pk_eclass;
					if (ieclass == ipeclass)
						break;
				}
				if (ieclass != ipeclass)
					elog(ERROR, "inner pathkeys do not match mergeclauses");
			}
		}
		else
		{
			/* redundant clauses ... must match some already-used pathkey */
			ipathkey = NULL;
			ipeclass = NULL;
			foreach(l2, innerpathkeys)
			{
				ipathkey = (PathKey *) lfirst(l2);
				ipeclass = ipathkey->pk_eclass;
				if (ieclass == ipeclass)
					break;
			}
			if (l2 == NULL)
				elog(ERROR, "inner pathkeys do not match mergeclauses");
		}

		/* pathkeys should match each other too (more debugging) */
		if (opathkey->pk_opfamily != ipathkey->pk_opfamily ||
			opathkey->pk_strategy != ipathkey->pk_strategy ||
			opathkey->pk_nulls_first != ipathkey->pk_nulls_first)
			elog(ERROR, "left and right pathkeys do not match in mergejoin");

		/* OK, save info for executor */
		mergefamilies[i] = opathkey->pk_opfamily;
		mergestrategies[i] = opathkey->pk_strategy;
		mergenullsfirst[i] = opathkey->pk_nulls_first;
		i++;
	}

	/*
	 * Note: it is not an error if we have additional pathkey elements
	 * (i.e., lop or lip isn't NULL here).  The input paths might be
	 * better-sorted than we need for the current mergejoin.
	 */

	/*
	 * Now we can build the mergejoin node.
	 */
	join_plan = make_mergejoin(tlist,
							   joinclauses,
							   otherclauses,
							   mergeclauses,
							   mergefamilies,
							   mergestrategies,
							   mergenullsfirst,
							   outer_plan,
							   inner_plan,
							   best_path->jpath.jointype);

	join_plan->join.prefetch_inner = prefetch;

	copy_path_costsize(root, &join_plan->join.plan, &best_path->jpath.path);

	return join_plan;
}

static HashJoin *
create_hashjoin_plan(PlannerInfo *root,
					 HashPath *best_path,
					 Plan *outer_plan,
					 Plan *inner_plan)
{
	List	   *tlist = build_relation_tlist(best_path->jpath.path.parent);
	List	   *joinclauses;
	List	   *otherclauses;
	List	   *hashclauses;
	HashJoin   *join_plan;
	Hash	   *hash_plan;

	/* Sort join qual clauses into best execution order */
	joinclauses = order_qual_clauses(root, best_path->jpath.joinrestrictinfo);
	/* There's no point in sorting the hash clauses ... */

	/* Get the join qual clauses (in plain expression form) */
	/* Any pseudoconstant clauses are ignored here */
	if (IS_OUTER_JOIN(best_path->jpath.jointype))
	{
		extract_actual_join_clauses(joinclauses,
									&joinclauses, &otherclauses);
	}
	else
	{
		/* We can treat all clauses alike for an inner join */
		joinclauses = extract_actual_clauses(joinclauses, false);
		otherclauses = NIL;
	}

	/*
	 * Remove the hashclauses from the list of join qual clauses, leaving the
	 * list of quals that must be checked as qpquals.
	 */
	hashclauses = get_actual_clauses(best_path->path_hashclauses);
	joinclauses = list_difference(joinclauses, hashclauses);

	/*
	 * Rearrange hashclauses, if needed, so that the outer variable is always
	 * on the left.
	 */
	hashclauses = get_switched_clauses(best_path->path_hashclauses,
							 best_path->jpath.outerjoinpath->parent->relids);

	/*
	 * We don't want any excess columns in the hashed tuples, or in the outer
	 * either!
	 */
	disuse_physical_tlist(inner_plan, best_path->jpath.innerjoinpath);
	if (outer_plan)
		disuse_physical_tlist(outer_plan, best_path->jpath.outerjoinpath);

	/*
	 * Build the hash node and hash join node.
	 */
	hash_plan = make_hash(inner_plan);
	join_plan = make_hashjoin(tlist,
							  joinclauses,
							  otherclauses,
							  hashclauses,
							  NIL, /* hashqualclauses */
							  outer_plan,
							  (Plan *) hash_plan,
							  best_path->jpath.jointype);

	/*
	 * MPP-4635.  best_path->jpath.outerjoinpath may be NULL.
	 * From the comment, it is adaptive nestloop join may cause this.
	 */
	/*
	 * MPP-4165: we need to descend left-first if *either* of the
	 * subplans have any motion.
	 */
	/*
	 * MPP-3300: unify motion-deadlock prevention for all join types.
	 * This allows us to undo the MPP-989 changes in nodeHashjoin.c
	 * (allowing us to check the outer for rows before building the
	 * hash-table).
	 */
	if (best_path->jpath.outerjoinpath == NULL ||
		best_path->jpath.outerjoinpath->motionHazard ||
		best_path->jpath.innerjoinpath->motionHazard)
	{
		join_plan->join.prefetch_inner = true;
	}

	copy_path_costsize(root, &join_plan->join.plan, &best_path->jpath.path);

	return join_plan;
}


/*****************************************************************************
 *
 *	SUPPORTING ROUTINES
 *
 *****************************************************************************/

/*
 * fix_indexqual_references
 *	  Adjust indexqual clauses to the form the executor's indexqual
 *	  machinery needs.
 *
 * We have three tasks here:
 *	* Remove RestrictInfo nodes from the input clauses.
 *	* Index keys must be represented by Var nodes with varattno set to the
 *	  index's attribute number, not the attribute number in the original rel.
 *	* If the index key is on the right, commute the clause to put it on the
 *	  left.
 *
 * The result is a modified copy of the indexquals list --- the
 * original is not changed.  Note also that the copy shares no substructure
 * with the original; this is needed in case there is a subplan in it (we need
 * two separate copies of the subplan tree, or things will go awry).
 */
static List *
fix_indexqual_references(List *indexquals, IndexPath *index_path)
{
	IndexOptInfo *index = index_path->indexinfo;
	List	   *fixed_indexquals;
	ListCell   *l;

	fixed_indexquals = NIL;

	/*
	 * For each qual clause, commute if needed to put the indexkey operand on
	 * the left, and then fix its varattno.  (We do not need to change the
	 * other side of the clause.)
	 */
	foreach(l, indexquals)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(l);
		Expr	   *clause;

		Assert(IsA(rinfo, RestrictInfo));

		/*
		 * Make a copy that will become the fixed clause.
		 *
		 * We used to try to do a shallow copy here, but that fails if there
		 * is a subplan in the arguments of the opclause.  So just do a full
		 * copy.
		 */
		clause = (Expr *) copyObject((Node *) rinfo->clause);

		if (IsA(clause, OpExpr))
		{
			OpExpr	   *op = (OpExpr *) clause;

			if (list_length(op->args) != 2)
				elog(ERROR, "indexqual clause is not binary opclause");

			/*
			 * Check to see if the indexkey is on the right; if so, commute
			 * the clause. The indexkey should be the side that refers to
			 * (only) the base relation.
			 */
			if (!bms_equal(rinfo->left_relids, index->rel->relids))
				CommuteOpExpr(op);

			/*
			 * Now, determine which index attribute this is and change the
			 * indexkey operand as needed.
			 */
			linitial(op->args) = fix_indexqual_operand(linitial(op->args),
													   index);
		}
		else if (IsA(clause, RowCompareExpr))
		{
			RowCompareExpr *rc = (RowCompareExpr *) clause;
			ListCell   *lc;

			/*
			 * Check to see if the indexkey is on the right; if so, commute
			 * the clause. The indexkey should be the side that refers to
			 * (only) the base relation.
			 */
			if (!bms_overlap(pull_varnos(linitial(rc->largs)),
							 index->rel->relids))
				CommuteRowCompareExpr(rc);

			/*
			 * For each column in the row comparison, determine which index
			 * attribute this is and change the indexkey operand as needed.
			 */
			foreach(lc, rc->largs)
			{
				lfirst(lc) = fix_indexqual_operand(lfirst(lc),
												   index);
			}
		}
		else if (IsA(clause, ScalarArrayOpExpr))
		{
			ScalarArrayOpExpr *saop = (ScalarArrayOpExpr *) clause;

			/* Never need to commute... */

			/*
			 * Determine which index attribute this is and change the
			 * indexkey operand as needed.
			 */
			linitial(saop->args) = fix_indexqual_operand(linitial(saop->args),
														 index);
		}
		else if (IsA(clause, NullTest))
		{
			NullTest   *nt = (NullTest *) clause;

			Assert(nt->nulltesttype == IS_NULL);
			nt->arg = (Expr *) fix_indexqual_operand((Node *) nt->arg,
													 index);
		}
		else
			elog(ERROR, "unsupported indexqual type: %d",
				 (int) nodeTag(clause));

		fixed_indexquals = lappend(fixed_indexquals, clause);
	}

	return fixed_indexquals;
}

static Node *
fix_indexqual_operand(Node *node, IndexOptInfo *index)
{
	/*
	 * We represent index keys by Var nodes having the varno of the base table
	 * but varattno equal to the index's attribute number (index column
	 * position).  This is a bit hokey ... would be cleaner to use a
	 * special-purpose node type that could not be mistaken for a regular Var.
	 * But it will do for now.
	 */
	Var		   *result;
	int			pos;
	ListCell   *indexpr_item;

	/*
	 * Remove any binary-compatible relabeling of the indexkey
	 */
	if (IsA(node, RelabelType))
		node = (Node *) ((RelabelType *) node)->arg;

	if (IsA(node, Var) &&
		((Var *) node)->varno == index->rel->relid)
	{
		/* Try to match against simple index columns */
		int			varatt = ((Var *) node)->varattno;

		if (varatt != 0)
		{
			for (pos = 0; pos < index->ncolumns; pos++)
			{
				if (index->indexkeys[pos] == varatt)
				{
					result = (Var *) copyObject(node);
					result->varattno = pos + 1;
					return (Node *) result;
				}
			}
		}
	}

	/* Try to match against index expressions */
	indexpr_item = list_head(index->indexprs);
	for (pos = 0; pos < index->ncolumns; pos++)
	{
		if (index->indexkeys[pos] == 0)
		{
			Node	   *indexkey;

			if (indexpr_item == NULL)
				elog(ERROR, "too few entries in indexprs list");
			indexkey = (Node *) lfirst(indexpr_item);
			if (indexkey && IsA(indexkey, RelabelType))
				indexkey = (Node *) ((RelabelType *) indexkey)->arg;
			if (equal(node, indexkey))
			{
				/* Found a match */
				result = makeVar(index->rel->relid, pos + 1,
								 exprType(lfirst(indexpr_item)), -1,
								 0);
				return (Node *) result;
			}
			indexpr_item = lnext(indexpr_item);
		}
	}

	/* Ooops... */
	elog(ERROR, "node is not an index attribute");
	return NULL;				/* keep compiler quiet */
}

/*
 * get_switched_clauses
 *	  Given a list of merge or hash joinclauses (as RestrictInfo nodes),
 *	  extract the bare clauses, and rearrange the elements within the
 *	  clauses, if needed, so the outer join variable is on the left and
 *	  the inner is on the right.  The original clause data structure is not
 *	  touched; a modified list is returned.  We do, however, set the transient
 *	  outer_is_left field in each RestrictInfo to show which side was which.
 */
static List *
get_switched_clauses(List *clauses, Relids outerrelids)
{
	List	   *t_list = NIL;
	ListCell   *l;

	foreach(l, clauses)
	{
		RestrictInfo *restrictinfo = (RestrictInfo *) lfirst(l);

		Expr	   *rclause = restrictinfo->clause;
		OpExpr	   *clause;

		/**
		 * If this is a IS NOT FALSE boolean test, we can peek underneath.
		 */
		if (IsA(rclause, BooleanTest))
		{
			BooleanTest *bt = (BooleanTest *) rclause;

			if (bt->booltesttype == IS_NOT_FALSE)
			{
				rclause = bt->arg;
			}
		}

		Assert(is_opclause(rclause));
		clause = (OpExpr *) rclause;
		if (bms_is_subset(restrictinfo->right_relids, outerrelids))
		{
			/*
			 * Duplicate just enough of the structure to allow commuting the
			 * clause without changing the original list.  Could use
			 * copyObject, but a complete deep copy is overkill.
			 */
			OpExpr	   *temp = makeNode(OpExpr);

			temp->opno = clause->opno;
			temp->opfuncid = InvalidOid;
			temp->opresulttype = clause->opresulttype;
			temp->opretset = clause->opretset;
			temp->args = list_copy(clause->args);
			temp->location = clause->location;
			/* Commute it --- note this modifies the temp node in-place. */
			CommuteOpExpr(temp);
			t_list = lappend(t_list, temp);
			restrictinfo->outer_is_left = false;
		}
		else
		{
			Assert(bms_is_subset(restrictinfo->left_relids, outerrelids));
			t_list = lappend(t_list, clause);
			restrictinfo->outer_is_left = true;
		}
	}
	return t_list;
}

/*
 * order_qual_clauses
 *		Given a list of qual clauses that will all be evaluated at the same
 *		plan node, sort the list into the order we want to check the quals
 *		in at runtime.
 *
 * Ideally the order should be driven by a combination of execution cost and
 * selectivity, but it's not immediately clear how to account for both,
 * and given the uncertainty of the estimates the reliability of the decisions
 * would be doubtful anyway.  So we just order by estimated per-tuple cost,
 * being careful not to change the order when (as is often the case) the
 * estimates are identical.
 *
 * Although this will work on either bare clauses or RestrictInfos, it's
 * much faster to apply it to RestrictInfos, since it can re-use cost
 * information that is cached in RestrictInfos.
 *
 * Note: some callers pass lists that contain entries that will later be
 * removed; this is the easiest way to let this routine see RestrictInfos
 * instead of bare clauses.  It's OK because we only sort by cost, but
 * a cost/selectivity combination would likely do the wrong thing.
 */
static List *
order_qual_clauses(PlannerInfo *root, List *clauses)
{
	typedef struct
	{
		Node	   *clause;
		Cost		cost;
	} QualItem;
	int			nitems = list_length(clauses);
	QualItem   *items;
	ListCell   *lc;
	int			i;
	List	   *result;

	/* No need to work hard for 0 or 1 clause */
	if (nitems <= 1)
		return clauses;

	/*
	 * Collect the items and costs into an array.  This is to avoid repeated
	 * cost_qual_eval work if the inputs aren't RestrictInfos.
	 */
	items = (QualItem *) palloc(nitems * sizeof(QualItem));
	i = 0;
	foreach(lc, clauses)
	{
		Node	   *clause = (Node *) lfirst(lc);
		QualCost	qcost;

		cost_qual_eval_node(&qcost, clause, root);
		items[i].clause = clause;
		items[i].cost = qcost.per_tuple;
		i++;
	}

	/*
	 * Sort.  We don't use qsort() because it's not guaranteed stable for
	 * equal keys.	The expected number of entries is small enough that a
	 * simple insertion sort should be good enough.
	 */
	for (i = 1; i < nitems; i++)
	{
		QualItem	newitem = items[i];
		int			j;

		/* insert newitem into the already-sorted subarray */
		for (j = i; j > 0; j--)
		{
			if (newitem.cost >= items[j - 1].cost)
				break;
			items[j] = items[j - 1];
		}
		items[j] = newitem;
	}

	/* Convert back to a list */
	result = NIL;
	for (i = 0; i < nitems; i++)
		result = lappend(result, items[i].clause);

	return result;
}

/*
 * Copy cost and size info from a Path node to the Plan node created from it.
 * The executor won't use this info, but it's needed by EXPLAIN.
 */
static void
copy_path_costsize(PlannerInfo *root, Plan *dest, Path *src)
{
	if (src)
	{
		dest->startup_cost = src->startup_cost;
		dest->total_cost = src->total_cost;
		dest->plan_rows = cdbpath_rows(root, src);
		dest->plan_width = src->parent->width;
	}
	else
	{
		dest->startup_cost = 0;
		dest->total_cost = 0;
		dest->plan_rows = 0;
		dest->plan_width = 0;
	}
}

/*
 * Copy cost and size info from a lower plan node to an inserted node.
 * This is not critical, since the decisions have already been made,
 * but it helps produce more reasonable-looking EXPLAIN output.
 * (Some callers alter the info after copying it.)
 */
static void
copy_plan_costsize(Plan *dest, Plan *src)
{
	if (src)
	{
		dest->startup_cost = src->startup_cost;
		dest->total_cost = src->total_cost;
		dest->plan_rows = src->plan_rows;
		dest->plan_width = src->plan_width;
	}
	else
	{
		dest->startup_cost = 0;
		dest->total_cost = 0;
		dest->plan_rows = 0;
		dest->plan_width = 0;
	}
}


/*****************************************************************************
 *
 *	PLAN NODE BUILDING ROUTINES
 *
 * Some of these are exported because they are called to build plan nodes
 * in contexts where we're not deriving the plan node from a path node.
 *
 *****************************************************************************/

static SeqScan *
make_seqscan(List *qptlist,
			 List *qpqual,
			 Index scanrelid)
{
	SeqScan    *node = makeNode(SeqScan);
	Plan	   *plan = &node->plan;

	/* cost should be inserted by caller */
	plan->targetlist = qptlist;
	plan->qual = qpqual;
	plan->lefttree = NULL;
	plan->righttree = NULL;
	node->scanrelid = scanrelid;

	return node;
}

static AppendOnlyScan *
make_appendonlyscan(List *qptlist,
					List *qpqual,
					Index scanrelid)
{
	AppendOnlyScan *node = makeNode(AppendOnlyScan);
	Plan	   *plan = &node->scan.plan;

	/* cost should be inserted by caller */
	plan->targetlist = qptlist;
	plan->qual = qpqual;
	plan->lefttree = NULL;
	plan->righttree = NULL;

	node->scan.scanrelid = scanrelid;

	return node;
}

static AOCSScan *
make_aocsscan(List *qptlist,
			  List *qpqual,
			  Index scanrelid)
{
	AOCSScan   *node = makeNode(AOCSScan);
	Plan	   *plan = &node->scan.plan;

	/* cost should be inserted by caller */
	plan->targetlist = qptlist;
	plan->qual = qpqual;
	plan->lefttree = NULL;
	plan->righttree = NULL;

	node->scan.scanrelid = scanrelid;

	return node;
}

static ExternalScan *
make_externalscan(List *qptlist,
				  List *qpqual,
				  Index scanrelid,
				  List *urilist,
				  List *fmtopts,
				  char fmttype,
				  bool ismasteronly,
				  int rejectlimit,
				  bool rejectlimitinrows,
				  Oid fmterrtableOid,
				  int encoding)
{
	ExternalScan *node = makeNode(ExternalScan);
	Plan	   *plan = &node->scan.plan;
	static uint32 scancounter = 0;

	/* cost should be inserted by caller */
	plan->targetlist = qptlist;
	plan->qual = qpqual;
	plan->lefttree = NULL;
	plan->righttree = NULL;
	node->scan.scanrelid = scanrelid;

	/* external specifictions */
	node->uriList = urilist;
	node->fmtOpts = fmtopts;
	node->fmtType = fmttype;
	node->isMasterOnly = ismasteronly;
	node->rejLimit = rejectlimit;
	node->rejLimitInRows = rejectlimitinrows;
	node->fmterrtbl = fmterrtableOid;
	node->encoding = encoding;
	node->scancounter = scancounter++;

	return node;
}

static IndexScan *
make_indexscan(List *qptlist,
			   List *qpqual,
			   Index scanrelid,
			   Oid indexid,
			   List *indexqual,
			   List *indexqualorig,
			   ScanDirection indexscandir)
{
	IndexScan  *node = makeNode(IndexScan);
	Plan	   *plan = &node->scan.plan;

	/* cost should be inserted by caller */
	plan->targetlist = qptlist;
	plan->qual = qpqual;
	plan->lefttree = NULL;
	plan->righttree = NULL;
	node->scan.scanrelid = scanrelid;
	node->indexid = indexid;
	node->indexqual = indexqual;
	node->indexqualorig = indexqualorig;
	node->indexorderdir = indexscandir;

	return node;
}

static BitmapIndexScan *
make_bitmap_indexscan(Index scanrelid,
					  Oid indexid,
					  List *indexqual,
					  List *indexqualorig)
{
	BitmapIndexScan *node = makeNode(BitmapIndexScan);
	Plan	   *plan = &node->scan.plan;

	/* cost should be inserted by caller */
	plan->targetlist = NIL;		/* not used */
	plan->qual = NIL;			/* not used */
	plan->lefttree = NULL;
	plan->righttree = NULL;
	node->scan.scanrelid = scanrelid;
	node->indexid = indexid;
	node->indexqual = indexqual;
	node->indexqualorig = indexqualorig;

	return node;
}

static BitmapHeapScan *
make_bitmap_heapscan(List *qptlist,
					 List *qpqual,
					 Plan *lefttree,
					 List *bitmapqualorig,
					 Index scanrelid)
{
	BitmapHeapScan *node = makeNode(BitmapHeapScan);
	Plan	   *plan = &node->scan.plan;

	/* cost should be inserted by caller */
	plan->targetlist = qptlist;
	plan->qual = qpqual;
	plan->lefttree = lefttree;
	plan->righttree = NULL;
	node->scan.scanrelid = scanrelid;
	node->bitmapqualorig = bitmapqualorig;

	return node;
}

static BitmapAppendOnlyScan *
make_bitmap_appendonlyscan(List *qptlist,
						   List *qpqual,
						   Plan *lefttree,
						   List *bitmapqualorig,
						   Index scanrelid,
						   bool isAORow)
{
	BitmapAppendOnlyScan *node = makeNode(BitmapAppendOnlyScan);
	Plan	   *plan = &node->scan.plan;

	/* cost should be inserted by caller */
	plan->targetlist = qptlist;
	plan->qual = qpqual;
	plan->lefttree = lefttree;
	plan->righttree = NULL;
	node->scan.scanrelid = scanrelid;

	node->bitmapqualorig = bitmapqualorig;
	node->isAORow = isAORow;

	return node;
}

static TidScan *
make_tidscan(List *qptlist,
			 List *qpqual,
			 Index scanrelid,
			 List *tidquals)
{
	TidScan    *node = makeNode(TidScan);
	Plan	   *plan = &node->scan.plan;

	/* cost should be inserted by caller */
	plan->targetlist = qptlist;
	plan->qual = qpqual;
	plan->lefttree = NULL;
	plan->righttree = NULL;
	node->scan.scanrelid = scanrelid;
	node->tidquals = tidquals;

	return node;
}

SubqueryScan *
make_subqueryscan(PlannerInfo *root,
				  List *qptlist,
				  List *qpqual,
				  Index scanrelid,
				  Plan *subplan,
				  List *subrtable)
{
	SubqueryScan *node = makeNode(SubqueryScan);
	Plan	   *plan = &node->scan.plan;

	/*
	 * Cost is figured here for the convenience of prepunion.c.  Note this is
	 * only correct for the case where qpqual is empty; otherwise caller
	 * should overwrite cost with a better estimate.
	 */
	copy_plan_costsize(plan, subplan);
	plan->total_cost += cpu_tuple_cost * subplan->plan_rows;

	plan->targetlist = qptlist;
	plan->qual = qpqual;
	plan->lefttree = NULL;
	plan->righttree = NULL;
	plan->extParam = bms_copy(subplan->extParam);
	plan->allParam = bms_copy(subplan->allParam);

	/*
	 * Note that, in most scan nodes, scanrelid refers to an entry in the rtable of the 
	 * containing plan; in a subqueryscan node, the containing plan is the higher
	 * level plan!
	 */
	node->scan.scanrelid = scanrelid;

	node->subplan = subplan;
	node->subrtable = subrtable;

	return node;
}

static FunctionScan *
make_functionscan(List *qptlist,
				  List *qpqual,
				  Index scanrelid,
				  Node *funcexpr,
				  List *funccolnames,
				  List *funccoltypes,
				  List *funccoltypmods)
{
	FunctionScan *node = makeNode(FunctionScan);
	Plan	   *plan = &node->scan.plan;

	/* cost should be inserted by caller */
	plan->targetlist = qptlist;
	plan->qual = qpqual;
	plan->lefttree = NULL;
	plan->righttree = NULL;
	node->scan.scanrelid = scanrelid;
	node->funcexpr = funcexpr;
	node->funccolnames = funccolnames;
	node->funccoltypes = funccoltypes;
	node->funccoltypmods = funccoltypmods;

	return node;
}

static ValuesScan *
make_valuesscan(List *qptlist,
				List *qpqual,
				Index scanrelid,
				List *values_lists)
{
	ValuesScan *node = makeNode(ValuesScan);
	Plan	   *plan = &node->scan.plan;

	/* cost should be inserted by caller */
	plan->targetlist = qptlist;
	plan->qual = qpqual;
	plan->lefttree = NULL;
	plan->righttree = NULL;
	node->scan.scanrelid = scanrelid;
	node->values_lists = values_lists;

	return node;
}

static pg_attribute_unused() CteScan *
make_ctescan(List *qptlist,
			 List *qpqual,
			 Index scanrelid,
			 int ctePlanId,
			 int cteParam)
{
	CteScan *node = makeNode(CteScan);
	Plan	   *plan = &node->scan.plan;

	/* cost should be inserted by caller */
	plan->targetlist = qptlist;
	plan->qual = qpqual;
	plan->lefttree = NULL;
	plan->righttree = NULL;
	node->scan.scanrelid = scanrelid;
	node->ctePlanId = ctePlanId;
	node->cteParam = cteParam;

	return node;
}

static WorkTableScan *
make_worktablescan(List *qptlist,
				   List *qpqual,
				   Index scanrelid,
				   int wtParam)
{
	WorkTableScan *node = makeNode(WorkTableScan);
	Plan	   *plan = &node->scan.plan;

	/* cost should be inserted by caller */
	plan->targetlist = qptlist;
	plan->qual = qpqual;
	plan->lefttree = NULL;
	plan->righttree = NULL;
	node->scan.scanrelid = scanrelid;
	node->wtParam = wtParam;

	return node;
}

Append *
make_append(List *appendplans, bool isTarget, List *tlist)
{
	Append	   *node = makeNode(Append);
	Plan	   *plan = &node->plan;
	double		total_size;
	ListCell   *subnode;

	/*
	 * Compute cost as sum of subplan costs.  We charge nothing extra for the
	 * Append itself, which perhaps is too optimistic, but since it doesn't do
	 * any selection or projection, it is a pretty cheap node.
	 */
	plan->startup_cost = 0;
	plan->total_cost = 0;
	plan->plan_rows = 0;
	total_size = 0;
	foreach(subnode, appendplans)
	{
		Plan	   *subplan = (Plan *) lfirst(subnode);

		if (subnode == list_head(appendplans))	/* first node? */
			plan->startup_cost = subplan->startup_cost;
		plan->total_cost += subplan->total_cost;
		plan->plan_rows += subplan->plan_rows;
		total_size += subplan->plan_width * subplan->plan_rows;
	}
	/* GPDB_84_MERGE_FIXME: ensure this math is okay compared to before the
	 * merge */
	if (plan->plan_rows > 0)
		plan->plan_width = rint(total_size / plan->plan_rows);
	else
		plan->plan_width = 0;

	plan->targetlist = tlist;
	plan->qual = NIL;
	plan->lefttree = NULL;
	plan->righttree = NULL;
	node->appendplans = appendplans;
	node->isTarget = isTarget;
	node->isZapped = false;

	return node;
}

RecursiveUnion *
make_recursive_union(List *tlist,
					 Plan *lefttree,
					 Plan *righttree,
					 int wtParam,
					 List *distinctList,
					 long numGroups)
{
	RecursiveUnion *node = makeNode(RecursiveUnion);
	Plan	   *plan = &node->plan;
	int			numCols = list_length(distinctList);
	
	cost_recursive_union(plan, lefttree, righttree);

	plan->targetlist = tlist;
	plan->qual = NIL;
	plan->lefttree = lefttree;
	plan->righttree = righttree;
	node->wtParam = wtParam;

	/*
	 * convert SortGroupClause list into arrays of attr indexes and equality
	 * operators, as wanted by executor
	 */
	node->numCols = numCols;
	if (numCols > 0)
	{
		int			keyno = 0;
		AttrNumber *dupColIdx;
		Oid		   *dupOperators;
		ListCell   *slitem;

		dupColIdx = (AttrNumber *) palloc(sizeof(AttrNumber) * numCols);
		dupOperators = (Oid *) palloc(sizeof(Oid) * numCols);

		foreach(slitem, distinctList)
		{
			SortGroupClause *sortcl = (SortGroupClause *) lfirst(slitem);
			TargetEntry *tle = get_sortgroupclause_tle(sortcl,
													   plan->targetlist);

			dupColIdx[keyno] = tle->resno;
			dupOperators[keyno] = sortcl->eqop;
			Assert(OidIsValid(dupOperators[keyno]));
			keyno++;
		}
		node->dupColIdx = dupColIdx;
		node->dupOperators = dupOperators;
	}
	node->numGroups = numGroups;

	return node;
}

static BitmapAnd *
make_bitmap_and(List *bitmapplans)
{
	BitmapAnd  *node = makeNode(BitmapAnd);
	Plan	   *plan = &node->plan;

	/* cost should be inserted by caller */
	plan->targetlist = NIL;
	plan->qual = NIL;
	plan->lefttree = NULL;
	plan->righttree = NULL;
	node->bitmapplans = bitmapplans;

	return node;
}

static BitmapOr *
make_bitmap_or(List *bitmapplans)
{
	BitmapOr   *node = makeNode(BitmapOr);
	Plan	   *plan = &node->plan;

	/* cost should be inserted by caller */
	plan->targetlist = NIL;
	plan->qual = NIL;
	plan->lefttree = NULL;
	plan->righttree = NULL;
	node->bitmapplans = bitmapplans;

	return node;
}

NestLoop *
make_nestloop(List *tlist,
			  List *joinclauses,
			  List *otherclauses,
			  Plan *lefttree,
			  Plan *righttree,
			  JoinType jointype)
{
	NestLoop   *node = makeNode(NestLoop);
	Plan	   *plan = &node->join.plan;

	/* cost should be inserted by caller */
	plan->targetlist = tlist;
	plan->qual = otherclauses;
	plan->lefttree = lefttree;
	plan->righttree = righttree;
	node->join.jointype = jointype;
	node->join.joinqual = joinclauses;

	return node;
}

HashJoin *
make_hashjoin(List *tlist,
			  List *joinclauses,
			  List *otherclauses,
			  List *hashclauses,
			  List *hashqualclauses,
			  Plan *lefttree,
			  Plan *righttree,
			  JoinType jointype)
{
	HashJoin   *node = makeNode(HashJoin);
	Plan	   *plan = &node->join.plan;

	/* cost should be inserted by caller */
	plan->targetlist = tlist;
	plan->qual = otherclauses;
	plan->lefttree = lefttree;
	plan->righttree = righttree;
	node->hashclauses = hashclauses;
	node->hashqualclauses = hashqualclauses;
	node->join.jointype = jointype;
	node->join.joinqual = joinclauses;

	return node;
}

Hash *
make_hash(Plan *lefttree)
{
	Hash	   *node = makeNode(Hash);
	Plan	   *plan = &node->plan;

	copy_plan_costsize(plan, lefttree);

	/*
	 * For plausibility, make startup & total costs equal total cost of input
	 * plan; this only affects EXPLAIN display not decisions.
	 */
	plan->startup_cost = plan->total_cost;
	plan->targetlist = lefttree->targetlist;
	plan->qual = NIL;
	plan->lefttree = lefttree;
	plan->righttree = NULL;

	node->rescannable = false;	/* CDB (unused for now) */

	return node;
}

MergeJoin *
make_mergejoin(List *tlist,
			   List *joinclauses,
			   List *otherclauses,
			   List *mergeclauses,
			   Oid *mergefamilies,
			   int *mergestrategies,
			   bool *mergenullsfirst,
			   Plan *lefttree,
			   Plan *righttree,
			   JoinType jointype)
{
	MergeJoin  *node = makeNode(MergeJoin);
	Plan	   *plan = &node->join.plan;

	/* cost should be inserted by caller */
	plan->targetlist = tlist;
	plan->qual = otherclauses;
	plan->lefttree = lefttree;
	plan->righttree = righttree;
	node->mergeclauses = mergeclauses;
	node->mergeFamilies = mergefamilies;
	node->mergeStrategies = mergestrategies;
	node->mergeNullsFirst = mergenullsfirst;
	node->join.jointype = jointype;
	node->join.joinqual = joinclauses;

	return node;
}

/*
 * make_sort --- basic routine to build a Sort plan node
 *
 * Caller must have built the sortColIdx, sortOperators, and nullsFirst
 * arrays already.	limit_tuples is as for cost_sort (in particular, pass
 * -1 if no limit)
 */
Sort *
make_sort(PlannerInfo *root, Plan *lefttree, int numCols,
		  AttrNumber *sortColIdx, Oid *sortOperators, bool *nullsFirst,
		  double limit_tuples)
{
	Sort	   *node = makeNode(Sort);
	Plan	   *plan = &node->plan;

	copy_plan_costsize(plan, lefttree); /* only care about copying size */
	plan = add_sort_cost(root, plan, numCols, sortColIdx, sortOperators,
						 limit_tuples);

	plan->targetlist = cdbpullup_targetlist(lefttree,
				 cdbpullup_exprHasSubplanRef((Expr *) lefttree->targetlist));
	plan->qual = NIL;
	plan->lefttree = lefttree;
	plan->righttree = NULL;
	node->numCols = numCols;
	node->sortColIdx = sortColIdx;
	node->sortOperators = sortOperators;
	node->nullsFirst = nullsFirst;

	Assert(sortColIdx[0] != 0);

	node->noduplicates = false; /* CDB */

	node->share_type = SHARE_NOTSHARED;
	node->share_id = SHARE_ID_NOT_SHARED;
	node->driver_slice = -1;
	node->nsharer = 0;
	node->nsharer_xslice = 0;

	return node;
}

/*
 * add_sort_cost --- basic routine to accumulate Sort cost into a
 * plan node representing the input cost.
 *
 * Unused arguments (e.g., sortColIdx and sortOperators arrays) are 
 * included to allow for future improvements to sort costing.  Note 
 * that root may be NULL (e.g. when called outside make_sort).
 */
Plan *
add_sort_cost(PlannerInfo *root, Plan *input, int numCols,
			  AttrNumber *sortColIdx, Oid *sortOperators, double limit_tuples)
{
	Path		sort_path;		/* dummy for result of cost_sort */

	UnusedArg(numCols);
	UnusedArg(sortColIdx);
	UnusedArg(sortOperators);

	cost_sort(&sort_path, root, NIL,
			  input->total_cost,
			  input->plan_rows,
			  input->plan_width,
			  limit_tuples);
	input->startup_cost = sort_path.startup_cost;
	input->total_cost = sort_path.total_cost;

	return input;
}

/*
 * add_sort_column --- utility subroutine for building sort info arrays
 *
 * We need this routine because the same column might be selected more than
 * once as a sort key column; if so, the extra mentions are redundant.
 *
 * Caller is assumed to have allocated the arrays large enough for the
 * max possible number of columns.	Return value is the new column count.
 */
static int
add_sort_column(AttrNumber colIdx, Oid sortOp, bool nulls_first,
				int numCols, AttrNumber *sortColIdx,
				Oid *sortOperators, bool *nullsFirst)
{
	int			i;

	Assert(OidIsValid(sortOp));

	Assert(OidIsValid(sortOp));

	for (i = 0; i < numCols; i++)
	{
		/*
		 * Note: we check sortOp because it's conceivable that "ORDER BY foo
		 * USING <, foo USING <<<" is not redundant, if <<< distinguishes
		 * values that < considers equal.  We need not check nulls_first
		 * however because a lower-order column with the same sortop but
		 * opposite nulls direction is redundant.
		 */
		if (sortColIdx[i] == colIdx &&
			sortOperators[numCols] == sortOp)
		{
			/* Already sorting by this col, so extra sort key is useless */
			return numCols;
		}
	}

	/* Add the column */
	sortColIdx[numCols] = colIdx;
	sortOperators[numCols] = sortOp;
	nullsFirst[numCols] = nulls_first;
	return numCols + 1;
}

/*
 * make_sort_from_pathkeys
 *	  Create sort plan to sort according to given pathkeys
 *
 *	  'lefttree' is the node which yields input tuples
 *	  'pathkeys' is the list of pathkeys by which the result is to be sorted
 *	  'limit_tuples' is the bound on the number of output tuples;
 *				-1 if no bound
 *	  'add_keys_to_targetlist' is true if it is ok to append to the subplan's
 *				targetlist or insert a Result node atop the subplan to
 *				evaluate sort key exprs that are not already present in the
 *				subplan's tlist.
 *
 * We must convert the pathkey information into arrays of sort key column
 * numbers and sort operator OIDs.
 *
 * If the pathkeys include expressions that aren't simple Vars, we will
 * usually need to add resjunk items to the input plan's targetlist to
 * compute these expressions (since the Sort node itself won't do it).
 * If the input plan type isn't one that can do projections, this means
 * adding a Result node just to do the projection.
 *
 * Returns a new Sort node if successful.
 *
 * Returns NULL if the sort key is degenerate (0 length after truncating
 * due to add_keys_to_targetlist==false and/or omitting key cols that are
 * equal to a constant expr.)
 */
Sort *
make_sort_from_pathkeys(PlannerInfo *root, Plan *lefttree, List *pathkeys,
						double limit_tuples, bool add_keys_to_targetlist)
{
	List	   *tlist = lefttree->targetlist;
	ListCell   *i;
	int			numsortkeys;
	AttrNumber *sortColIdx;
	Oid		   *sortOperators;
	bool	   *nullsFirst;

	/*
	 * We will need at most list_length(pathkeys) sort columns; possibly less
	 */
	numsortkeys = list_length(pathkeys);
	sortColIdx = (AttrNumber *) palloc(numsortkeys * sizeof(AttrNumber));
	sortOperators = (Oid *) palloc(numsortkeys * sizeof(Oid));
	nullsFirst = (bool *) palloc(numsortkeys * sizeof(bool));

	numsortkeys = 0;

	foreach(i, pathkeys)
	{
		PathKey    *pathkey = (PathKey *) lfirst(i);
		EquivalenceClass *ec = pathkey->pk_eclass;
		TargetEntry *tle = NULL;
		Oid			pk_datatype = InvalidOid;
		Oid			sortop;
		ListCell   *j;

		if (ec->ec_has_volatile)
		{
			/*
			 * If the pathkey's EquivalenceClass is volatile, then it must
			 * have come from an ORDER BY clause, and we have to match it to
			 * that same targetlist entry.
			 */
			if (ec->ec_sortref == 0)	/* can't happen */
				elog(ERROR, "volatile EquivalenceClass has no sortref");
			tle = get_sortgroupref_tle(ec->ec_sortref, tlist);
			Assert(tle);
			Assert(list_length(ec->ec_members) == 1);
			pk_datatype = ((EquivalenceMember *) linitial(ec->ec_members))->em_datatype;
		}
		else
		{
			/*
			 * Otherwise, we can sort by any non-constant expression listed in
			 * the pathkey's EquivalenceClass.  For now, we take the first one
			 * that corresponds to an available item in the tlist.	If there
			 * isn't any, use the first one that is an expression in the
			 * input's vars.  (The non-const restriction only matters if the
			 * EC is below_outer_join; but if it isn't, it won't contain
			 * consts anyway, else we'd have discarded the pathkey as
			 * redundant.)
			 *
			 * XXX if we have a choice, is there any way of figuring out which
			 * might be cheapest to execute?  (For example, int4lt is likely
			 * much cheaper to execute than numericlt, but both might appear
			 * in the same equivalence class...)  Not clear that we ever will
			 * have an interesting choice in practice, so it may not matter.
			 */
			foreach(j, ec->ec_members)
			{
				EquivalenceMember *em = (EquivalenceMember *) lfirst(j);

				if (em->em_is_const || em->em_is_child)
					continue;

				tle = tlist_member((Node *) em->em_expr, tlist);
				if (tle)
				{
					pk_datatype = em->em_datatype;
					break;		/* found expr already in tlist */
				}

				/*
				 * We can also use it if the pathkey expression is a relabel
				 * of the tlist entry, or vice versa.  This is needed for
				 * binary-compatible cases (cf. make_pathkey_from_sortinfo).
				 * We prefer an exact match, though, so we do the basic search
				 * first.
				 */
				tle = tlist_member_ignore_relabel((Node *) em->em_expr, tlist);
				if (tle)
				{
					pk_datatype = em->em_datatype;
					break;		/* found expr already in tlist */
				}
			}

			if (!tle)
			{
				/* No matching tlist item; look for a computable expression */
				Expr	   *sortexpr = NULL;

				if (!add_keys_to_targetlist)
					break;

				foreach(j, ec->ec_members)
				{
					EquivalenceMember *em = (EquivalenceMember *) lfirst(j);
					List	   *exprvars;
					ListCell   *k;

					if (em->em_is_const || em->em_is_child)
						continue;
					sortexpr = em->em_expr;
					exprvars = pull_var_clause((Node *) sortexpr,
											   PVC_RECURSE_AGGREGATES,
											   PVC_INCLUDE_PLACEHOLDERS);
					foreach(k, exprvars)
					{
						if (!tlist_member_ignore_relabel(lfirst(k), tlist))
							break;
					}
					list_free(exprvars);
					if (!k)
					{
						pk_datatype = em->em_datatype;
						break;	/* found usable expression */
					}
				}
				if (!j)
					elog(ERROR, "could not find pathkey item to sort");

				/*
				 * Do we need to insert a Result node?
				 */
				if (!is_projection_capable_plan(lefttree))
				{
					/* copy needed so we don't modify input's tlist below */
					tlist = copyObject(tlist);
					lefttree = (Plan *) make_result(root, tlist, NULL,
													lefttree);
					if (lefttree->lefttree->flow)
						lefttree->flow = pull_up_Flow(lefttree, lefttree->lefttree);
				}

				/*
				 * Add resjunk entry to input's tlist
				 */
				tle = makeTargetEntry(sortexpr,
									  list_length(tlist) + 1,
									  NULL,
									  true);
				tlist = lappend(tlist, tle);
				lefttree->targetlist = tlist;	/* just in case NIL before */
			}
		}

		/*
		 * Look up the correct sort operator from the PathKey's slightly
		 * abstracted representation.
		 */
		sortop = get_opfamily_member(pathkey->pk_opfamily,
									 pk_datatype,
									 pk_datatype,
									 pathkey->pk_strategy);
		if (!OidIsValid(sortop))	/* should not happen */
			elog(ERROR, "could not find member %d(%u,%u) of opfamily %u",
				 pathkey->pk_strategy, pk_datatype, pk_datatype,
				 pathkey->pk_opfamily);

		/*
		 * The column might already be selected as a sort key, if the pathkeys
		 * contain duplicate entries.  (This can happen in scenarios where
		 * multiple mergejoinable clauses mention the same var, for example.)
		 * So enter it only once in the sort arrays.
		 */
		numsortkeys = add_sort_column(tle->resno,
									  sortop,
									  pathkey->pk_nulls_first,
									  numsortkeys,
									  sortColIdx, sortOperators, nullsFirst);
	}

	Assert(numsortkeys > 0 || !add_keys_to_targetlist);
	if (numsortkeys == 0)
		return NULL;

	return make_sort(root, lefttree, numsortkeys,
					 sortColIdx, sortOperators, nullsFirst, limit_tuples);
}

/*
 * make_sort_from_sortclauses
 *	  Create sort plan to sort according to given sortclauses
 *
 *	  'sortcls' is a list of SortGroupClauses
 *	  'lefttree' is the node which yields input tuples
 */
Sort *
make_sort_from_sortclauses(PlannerInfo *root, List *sortcls, Plan *lefttree)
{
	List	   *sub_tlist = lefttree->targetlist;
	ListCell   *l;
	int			numsortkeys;
	AttrNumber *sortColIdx;
	Oid		   *sortOperators;
	bool	   *nullsFirst;

	/*
	 * We will need at most list_length(sortcls) sort columns; possibly less
	 */
	numsortkeys = list_length(sortcls);
	sortColIdx = (AttrNumber *) palloc(numsortkeys * sizeof(AttrNumber));
	sortOperators = (Oid *) palloc(numsortkeys * sizeof(Oid));
	nullsFirst = (bool *) palloc(numsortkeys * sizeof(bool));

	numsortkeys = 0;

	foreach(l, sortcls)
	{
		SortGroupClause *sortcl = (SortGroupClause *) lfirst(l);
		TargetEntry *tle = get_sortgroupclause_tle(sortcl, sub_tlist);

		/*
		 * Check for the possibility of duplicate order-by clauses --- the
		 * parser should have removed 'em, but no point in sorting
		 * redundantly.
		 */
		numsortkeys = add_sort_column(tle->resno, sortcl->sortop,
									  sortcl->nulls_first,
									  numsortkeys,
									  sortColIdx, sortOperators, nullsFirst);
	}

	Assert(numsortkeys > 0);

	return make_sort(root, lefttree, numsortkeys,
					 sortColIdx, sortOperators, nullsFirst, -1.0);
}

/*
 * make_sort_from_groupcols
 *	  Create sort plan to sort based on grouping columns
 *
 * 'groupcls' is the list of SortGroupClauses
 * 'grpColIdx' gives the column numbers to use
 * 'appendGrouping' represents whether to append a Grouping
 *	  as the last sort key, used for grouping extension.
 *
 * This might look like it could be merged with make_sort_from_sortclauses,
 * but presently we *must* use the grpColIdx[] array to locate sort columns,
 * because the child plan's tlist is not marked with ressortgroupref info
 * appropriate to the grouping node.  So, only the sort ordering info
 * is used from the SortGroupClause entries.
 */
Sort *
make_sort_from_groupcols(PlannerInfo *root,
						 List *groupcls,
						 AttrNumber *grpColIdx,
						 bool appendGrouping,
						 Plan *lefttree)
{
	List	   *sub_tlist = lefttree->targetlist;
	int			grpno = 0;
	ListCell   *l;
	int			numsortkeys;
	AttrNumber *sortColIdx;
	Oid		   *sortOperators;
	bool	   *nullsFirst;
	List	   *flat_groupcls;

	/*
	 * We will need at most list_length(groupcls) sort columns; possibly less
	 */
	numsortkeys = num_distcols_in_grouplist(groupcls);

	if (appendGrouping)
		numsortkeys++;

	sortColIdx = (AttrNumber *) palloc(numsortkeys * sizeof(AttrNumber));
	sortOperators = (Oid *) palloc(numsortkeys * sizeof(Oid));
	nullsFirst = (bool *) palloc(numsortkeys * sizeof(bool));

	numsortkeys = 0;

	flat_groupcls = flatten_grouping_list(groupcls);

	foreach(l, flat_groupcls)
	{
		SortGroupClause *grpcl = (SortGroupClause *) lfirst(l);
		TargetEntry *tle = get_tle_by_resno(sub_tlist, grpColIdx[grpno]);

		grpno++;

		/*
		 * Check for the possibility of duplicate group-by clauses --- the
		 * parser should have removed 'em, but no point in sorting
		 * redundantly.
		 */
		numsortkeys = add_sort_column(tle->resno, grpcl->sortop,
									  grpcl->nulls_first,
									  numsortkeys,
									  sortColIdx, sortOperators, nullsFirst);
	}

	if (appendGrouping)
	{
		Oid			lt_opr;

		/* Grouping will be the last entry in grpColIdx */
		TargetEntry *tle = get_tle_by_resno(sub_tlist, grpColIdx[grpno]);

		if (tle->resname == NULL)
			tle->resname = "grouping";

		get_sort_group_operators(exprType((Node *) tle->expr),
								 true, false, false,
								 &lt_opr, NULL, NULL);

		numsortkeys = add_sort_column(tle->resno, lt_opr, false,
						 numsortkeys, sortColIdx, sortOperators, nullsFirst);
	}


	Assert(numsortkeys > 0);

	return make_sort(root, lefttree, numsortkeys,
					 sortColIdx, sortOperators, nullsFirst, -1.0);
}

/*
 * Reconstruct a new list of GroupClause based on the given grpCols.
 *
 * The original grouping clauses may contain grouping extensions. This function
 * extract the raw grouping attributes and construct a list of GroupClauses
 * that contains only ordinary grouping.
 */
List *
reconstruct_group_clause(List *orig_groupClause, List *tlist, AttrNumber *grpColIdx, int numcols)
{
	List	   *flat_groupcls;
	List	   *new_groupClause = NIL;
	int			grpno;

	flat_groupcls = flatten_grouping_list(orig_groupClause);
	for (grpno = 0; grpno < numcols; grpno++)
	{
		ListCell   *lc = NULL;
		TargetEntry *te;
		SortGroupClause *gc = NULL;

		te = get_tle_by_resno(tlist, grpColIdx[grpno]);

		foreach(lc, flat_groupcls)
		{
			gc = (SortGroupClause *) lfirst(lc);

			if (gc->tleSortGroupRef == te->ressortgroupref)
				break;
		}
		if (lc != NULL)
			new_groupClause = lappend(new_groupClause, gc);
	}

	return new_groupClause;
}

/* --------------------------------------------------------------------
 * make_motion -- creates a Motion node.
 * Caller must have built the pHashDefn, pFixedDefn,
 * and pSortDefn structs already.
 * useExecutorVarFormat is true if make_motion is called after setrefs
 * This call only make a motion node, without filling in flow info
 * After calling this function, caller need to call add_slice_to_motion
 * --------------------------------------------------------------------
 */
Motion *
make_motion(PlannerInfo *root, Plan *lefttree, List *sortPathKeys, bool useExecutorVarFormat)
{
    Motion *node = makeNode(Motion);
    Plan   *plan = &node->plan;

	Assert(lefttree);
	Assert(!IsA(lefttree, Motion));

	plan->startup_cost = lefttree->startup_cost;
	plan->total_cost = lefttree->total_cost;
	plan->plan_rows = lefttree->plan_rows;
	plan->plan_width = lefttree->plan_width;

	plan->targetlist = cdbpullup_targetlist(lefttree, useExecutorVarFormat);
	plan->qual = NIL;
	plan->lefttree = lefttree;
	plan->righttree = NULL;
	plan->dispatch = DISPATCH_PARALLEL;

	if (sortPathKeys)
	{
		Sort *sort;

		/* FIXME: add_keys_to_targetlist, or not? */
		sort = make_sort_from_pathkeys(root, lefttree, sortPathKeys, -1, true);

		node->numSortCols = sort->numCols;
		node->sortColIdx = sort->sortColIdx;
		node->sortOperators = sort->sortOperators;
		node->nullsFirst = sort->nullsFirst;

#ifdef USE_ASSERT_CHECKING
		/*
		 * If the child node was a Sort, then surely the order the caller gave us
		 * must match that of the underlying sort.
		 */
		if (IsA(lefttree, Sort))
		{
			Sort	   *childsort = (Sort *) lefttree;
			Assert(childsort->numCols >= node->numSortCols);
			Assert(memcmp(childsort->sortColIdx, node->sortColIdx, node->numSortCols * sizeof(AttrNumber)) == 0);
			Assert(memcmp(childsort->sortOperators, node->sortOperators, node->numSortCols * sizeof(Oid)) == 0);
			Assert(memcmp(childsort->nullsFirst, node->nullsFirst, node->numSortCols * sizeof(bool)) == 0);
		}
#endif
	}
	node->sendSorted = (node->numSortCols > 0);

	plan->extParam = bms_copy(lefttree->extParam);
	plan->allParam = bms_copy(lefttree->allParam);

	plan->flow = NULL;

	node->outputSegIdx = NULL;
	node->numOutputSegs = 0;

	return node;
}

Material *
make_material(Plan *lefttree)
{
	Material   *node = makeNode(Material);
	Plan	   *plan = &node->plan;

	/* cost should be inserted by caller */
	plan->targetlist = lefttree->targetlist;
	plan->qual = NIL;
	plan->lefttree = lefttree;
	plan->righttree = NULL;

	node->cdb_strict = false;
	node->share_type = SHARE_NOTSHARED;
	node->share_id = SHARE_ID_NOT_SHARED;
	node->driver_slice = -1;
	node->nsharer = 0;
	node->nsharer_xslice = 0;

	return node;
}

/*
 * materialize_finished_plan: stick a Material node atop a completed plan
 *
 * There are a couple of places where we want to attach a Material node
 * after completion of subquery_planner().	This currently requires hackery.
 * Since subquery_planner has already run SS_finalize_plan on the subplan
 * tree, we have to kluge up parameter lists for the Material node.
 * Possibly this could be fixed by postponing SS_finalize_plan processing
 * until setrefs.c is run?
 */
Plan *
materialize_finished_plan(PlannerInfo *root, Plan *subplan)
{
	Plan	   *matplan;
	Path		matpath;		/* dummy for result of cost_material */

	matplan = (Plan *) make_material(subplan);

	/* Set cost data */
	cost_material(&matpath,
				  root,
				  subplan->total_cost,
				  subplan->plan_rows,
				  subplan->plan_width);
	matplan->startup_cost = matpath.startup_cost;
	matplan->total_cost = matpath.total_cost;
	matplan->plan_rows = subplan->plan_rows;
	matplan->plan_width = subplan->plan_width;

	/* MPP -- propagate dispatch method and flow */
	matplan->dispatch = subplan->dispatch;
	matplan->flow = copyObject(subplan->flow);

	/* parameter kluge --- see comments above */
	matplan->extParam = bms_copy(subplan->extParam);
	matplan->allParam = bms_copy(subplan->allParam);

	return matplan;
}

Agg *
make_agg(PlannerInfo *root, List *tlist, List *qual,
		 AggStrategy aggstrategy,
		 bool streaming,
		 int numGroupCols, AttrNumber *grpColIdx, Oid *grpOperators,
		 long numGroups, int num_nullcols,
		 uint64 input_grouping, uint64 grouping,
		 int rollupGSTimes,
		 int numAggs, int transSpace,
		 Plan *lefttree)
{
	Agg		   *node = makeNode(Agg);
	Plan	   *plan = &node->plan;

	node->aggstrategy = aggstrategy;
	node->numCols = numGroupCols;
	node->grpColIdx = grpColIdx;
	node->grpOperators = grpOperators;
	node->numGroups = numGroups;
	node->transSpace = transSpace;
	node->numNullCols = num_nullcols;
	node->inputGrouping = input_grouping;
	node->grouping = grouping;
	node->inputHasGrouping = false;
	node->rollupGSTimes = rollupGSTimes;
	node->lastAgg = false;
	node->streaming = streaming;

	copy_plan_costsize(plan, lefttree); /* only care about copying size */

	add_agg_cost(root, plan, tlist, qual, aggstrategy, streaming,
				 numGroupCols, grpColIdx,
				 numGroups, num_nullcols,
				 numAggs, transSpace);

	plan->qual = qual;
	plan->targetlist = tlist;
	plan->lefttree = lefttree;
	plan->righttree = NULL;

	plan->extParam = bms_copy(lefttree->extParam);
	plan->allParam = bms_copy(lefttree->allParam);

	return node;
}

/* add_agg_cost -- basic routine to accumulate Agg cost into a
 * plan node representing the input cost.
 *
 * Unused arguments (e.g., streaming, grpColIdx, num_nullcols)
 * are included to allow for future improvements to aggregate
 * costing.  Note that root may be NULL (e.g., when called from
 * outside make_agg).
 */
Plan *
add_agg_cost(PlannerInfo *root, Plan *plan,
			 List *tlist, List *qual,
			 AggStrategy aggstrategy,
			 bool streaming,
			 int numGroupCols, AttrNumber *grpColIdx,
			 long numGroups, int num_nullcols,
			 int numAggs, int transSpace)
{
	Path		agg_path;		/* dummy for result of cost_agg */
	QualCost	qual_cost;
	HashAggTableSizes hash_info;
	double entrywidth;

	UnusedArg(grpColIdx);
	UnusedArg(num_nullcols);

    /* Solution for MPP-11942
     * Before this fix, we calculated the width from the sub_tlist which
     * only contains a subset of the tlist. For example, for the query
     * select a, min(b), max(b), min(c), max(c) from s group by a;
     * the sub_tlist has the columns {a,b,c} while the tlist
     * is {a, min(b), max(b), min(c), max(c)}. Therefore, the plan_width
     * for the above query is calculated to be 12 instead of 20.
     *
     * In this fix, we calculate the actual row width from the tlist.
     */

    plan->plan_width = get_row_width(tlist);

	if (aggstrategy == AGG_HASHED)
	{
		/* The following estimate is very rough but good enough for planning. */
		entrywidth = agg_hash_entrywidth(numAggs,
								   sizeof(HeapTupleData) + sizeof(HeapTupleHeaderData) + plan->plan_width,
								   transSpace);
		if (!calcHashAggTableSizes(global_work_mem(root),
								   numGroups,
								   entrywidth,
								   true,
								   &hash_info))
		{
			elog(ERROR, "Planner committed to impossible hash aggregate.");
		}

		cost_agg(&agg_path, root,
				 aggstrategy, numAggs,
				 numGroupCols, numGroups,
				 plan->startup_cost,
				 plan->total_cost,
				 plan->plan_rows, hash_info.workmem_per_entry,
				 hash_info.nbatches, hash_info.hashentry_width, streaming);
	}
	else
		cost_agg(&agg_path, root,
				 aggstrategy, numAggs,
				 numGroupCols, numGroups,
				 plan->startup_cost,
				 plan->total_cost,
				 plan->plan_rows, 0.0, 0.0,
				 0.0, false);


	plan->startup_cost = agg_path.startup_cost;
	plan->total_cost = agg_path.total_cost;

	/*
	 * We will produce a single output tuple if not grouping, and a tuple per
	 * group otherwise.
	 */
	if (aggstrategy == AGG_PLAIN)
		plan->plan_rows = 1;
	else
		plan->plan_rows = numGroups;

	/*
	 * We also need to account for the cost of evaluation of the qual (ie, the
	 * HAVING clause) and the tlist.  Note that cost_qual_eval doesn't charge
	 * anything for Aggref nodes; this is okay since they are really
	 * comparable to Vars.
	 *
	 * See notes in grouping_planner about why only make_agg, make_windowagg
	 * and make_group worry about tlist eval cost.
	 */
	if (qual)
	{
		cost_qual_eval(&qual_cost, qual, root);
		plan->startup_cost += qual_cost.startup;
		plan->total_cost += qual_cost.startup;
		plan->total_cost += qual_cost.per_tuple * plan->plan_rows;
	}
	cost_qual_eval(&qual_cost, tlist, root);
	plan->startup_cost += qual_cost.startup;
	plan->total_cost += qual_cost.startup;
	plan->total_cost += qual_cost.per_tuple * plan->plan_rows;

	return plan;
}

WindowAgg *
make_windowagg(PlannerInfo *root, List *tlist,
			   List *windowFuncs, Index winref,
			   int partNumCols, AttrNumber *partColIdx, Oid *partOperators,
			   int ordNumCols, AttrNumber *ordColIdx, Oid *ordOperators,
			   AttrNumber firstOrderCol, Oid firstOrderCmpOperator, bool firstOrderNullsFirst,
			   int frameOptions, Node *startOffset, Node *endOffset,
			   Plan *lefttree)
{
	WindowAgg  *node = makeNode(WindowAgg);
	Plan	   *plan = &node->plan;
	Path		windowagg_path; /* dummy for result of cost_windowagg */
	QualCost	qual_cost;

	node->winref = winref;
	node->partNumCols = partNumCols;
	node->partColIdx = partColIdx;
	node->partOperators = partOperators;
	node->ordNumCols = ordNumCols;
	node->ordColIdx = ordColIdx;
	node->ordOperators = ordOperators;
	node->firstOrderCol = firstOrderCol;
	node->firstOrderCmpOperator= firstOrderCmpOperator;
	node->firstOrderNullsFirst= firstOrderNullsFirst;
	node->frameOptions = frameOptions;
	node->startOffset = startOffset;
	node->endOffset = endOffset;

	copy_plan_costsize(plan, lefttree); /* only care about copying size */
	cost_windowagg(&windowagg_path, root,
				   list_length(windowFuncs), partNumCols, ordNumCols,
				   lefttree->startup_cost,
				   lefttree->total_cost,
				   lefttree->plan_rows);
	plan->startup_cost = windowagg_path.startup_cost;
	plan->total_cost = windowagg_path.total_cost;

	/*
	 * We also need to account for the cost of evaluation of the tlist.
	 *
	 * See notes in grouping_planner about why only make_agg, make_windowagg
	 * and make_group worry about tlist eval cost.
	 */
	cost_qual_eval(&qual_cost, tlist, root);
	plan->startup_cost += qual_cost.startup;
	plan->total_cost += qual_cost.startup;
	plan->total_cost += qual_cost.per_tuple * plan->plan_rows;

	plan->targetlist = tlist;
	plan->lefttree = lefttree;
	plan->righttree = NULL;
	/* WindowAgg nodes never have a qual clause */
	plan->qual = NIL;

	if (lefttree->flow)
		plan->flow = pull_up_Flow(plan, lefttree);

	return node;
}

static TableFunctionScan *
make_tablefunction(List *tlist,
				   List *scan_quals,
				   Plan *subplan,
				   List *subrtable,
				   Index scanrelid)
{
	TableFunctionScan *node = makeNode(TableFunctionScan);
	Plan	   *plan = &node->scan.plan;

	copy_plan_costsize(plan, subplan);  /* only care about copying size */

	/* FIXME: fix costing */
	plan->startup_cost  = subplan->startup_cost;
	plan->total_cost    = subplan->total_cost;
	plan->total_cost   += 2 * plan->plan_rows;

	plan->qual			= scan_quals;
	plan->targetlist	= tlist;
	plan->righttree		= NULL;

	/* Fill in information for the subplan */
	plan->lefttree		 = subplan;
	node->subrtable		 = subrtable;
	node->scan.scanrelid = scanrelid;

	return node;
}



/*
 * distinctList is a list of SortGroupClauses, identifying the targetlist items
 * that should be considered by the Unique filter.	The input path must
 * already be sorted accordingly.
 */
Unique *
make_unique(Plan *lefttree, List *distinctList)
{
	Unique	   *node = makeNode(Unique);
	Plan	   *plan = &node->plan;
	int			numCols = list_length(distinctList);
	int			keyno = 0;
	AttrNumber *uniqColIdx;
	Oid		   *uniqOperators;
	ListCell   *slitem;

	copy_plan_costsize(plan, lefttree);

	/*
	 * Charge one cpu_operator_cost per comparison per input tuple. We assume
	 * all columns get compared at most of the tuples.	(XXX probably this is
	 * an overestimate.)
	 */
	plan->total_cost += cpu_operator_cost * plan->plan_rows * numCols;

	/*
	 * plan->plan_rows is left as a copy of the input subplan's plan_rows; ie,
	 * we assume the filter removes nothing.  The caller must alter this if he
	 * has a better idea.
	 */

	plan->targetlist = cdbpullup_targetlist(lefttree,
				 cdbpullup_exprHasSubplanRef((Expr *) lefttree->targetlist));
	plan->qual = NIL;
	plan->lefttree = lefttree;
	plan->righttree = NULL;

	/*
	 * convert SortGroupClause list into arrays of attr indexes and equality
	 * operators, as wanted by executor
	 */
	Assert(numCols > 0);
	uniqColIdx = (AttrNumber *) palloc(sizeof(AttrNumber) * numCols);
	uniqOperators = (Oid *) palloc(sizeof(Oid) * numCols);

	foreach(slitem, distinctList)
	{
		SortGroupClause *sortcl = (SortGroupClause *) lfirst(slitem);
		TargetEntry *tle = get_sortgroupclause_tle(sortcl, plan->targetlist);

		uniqColIdx[keyno] = tle->resno;
		uniqOperators[keyno] = sortcl->eqop;
		Assert(OidIsValid(uniqOperators[keyno]));
		keyno++;
	}

	node->numCols = numCols;
	node->uniqColIdx = uniqColIdx;
	node->uniqOperators = uniqOperators;

	/* CDB */	/* pass DISTINCT to sort */
	if (IsA(lefttree, Sort) && gp_enable_sort_distinct)
	{
		Sort	   *pSort = (Sort *) lefttree;

		pSort->noduplicates = true;
	}

	return node;
}

/*
 * distinctList is a list of SortGroupClauses, identifying the targetlist
 * items that should be considered by the SetOp filter.  The input path must
 * already be sorted accordingly.
 */
SetOp *
make_setop(SetOpCmd cmd, SetOpStrategy strategy, Plan *lefttree,
		   List *distinctList, AttrNumber flagColIdx, int firstFlag,
		   long numGroups, double outputRows)
{
	SetOp	   *node = makeNode(SetOp);
	Plan	   *plan = &node->plan;
	int			numCols = list_length(distinctList);
	int			keyno = 0;
	AttrNumber *dupColIdx;
	Oid		   *dupOperators;
	ListCell   *slitem;

	copy_plan_costsize(plan, lefttree);
	plan->plan_rows = outputRows;

	/*
	 * Charge one cpu_operator_cost per comparison per input tuple. We assume
	 * all columns get compared at most of the tuples.
	 */
	plan->total_cost += cpu_operator_cost * lefttree->plan_rows * numCols;

	plan->targetlist = cdbpullup_targetlist(lefttree,
				 cdbpullup_exprHasSubplanRef((Expr *) lefttree->targetlist));
	plan->qual = NIL;
	plan->lefttree = lefttree;
	plan->righttree = NULL;

	/*
	 * convert SortGroupClause list into arrays of attr indexes and equality
	 * operators, as wanted by executor
	 */
	Assert(numCols > 0);
	dupColIdx = (AttrNumber *) palloc(sizeof(AttrNumber) * numCols);
	dupOperators = (Oid *) palloc(sizeof(Oid) * numCols);

	foreach(slitem, distinctList)
	{
		SortGroupClause *sortcl = (SortGroupClause *) lfirst(slitem);
		TargetEntry *tle = get_sortgroupclause_tle(sortcl, plan->targetlist);

		dupColIdx[keyno] = tle->resno;
		dupOperators[keyno] = sortcl->eqop;
		Assert(OidIsValid(dupOperators[keyno]));
		keyno++;
	}

	node->cmd = cmd;
	node->strategy = strategy;
	node->numCols = numCols;
	node->dupColIdx = dupColIdx;
	node->dupOperators = dupOperators;
	node->flagColIdx = flagColIdx;
	node->firstFlag = firstFlag;
	node->numGroups = numGroups;

	return node;
}

/*
 * Note: offset_est and count_est are passed in to save having to repeat
 * work already done to estimate the values of the limitOffset and limitCount
 * expressions.  Their values are as returned by preprocess_limit (0 means
 * "not relevant", -1 means "couldn't estimate").  Keep the code below in sync
 * with that function!
 */
Limit *
make_limit(Plan *lefttree, Node *limitOffset, Node *limitCount,
		   int64 offset_est, int64 count_est)
{
	Limit	   *node = makeNode(Limit);
	Plan	   *plan = &node->plan;

	copy_plan_costsize(plan, lefttree);

	/*
	 * Adjust the output rows count and costs according to the offset/limit.
	 * This is only a cosmetic issue if we are at top level, but if we are
	 * building a subquery then it's important to report correct info to the
	 * outer planner.
	 *
	 * When the offset or count couldn't be estimated, use 10% of the
	 * estimated number of rows emitted from the subplan.
	 */
	if (offset_est != 0)
	{
		double		offset_rows;

		if (offset_est > 0)
			offset_rows = (double) offset_est;
		else
			offset_rows = clamp_row_est(lefttree->plan_rows * 0.10);
		if (offset_rows > plan->plan_rows)
			offset_rows = plan->plan_rows;
		if (plan->plan_rows > 0)
			plan->startup_cost +=
				(plan->total_cost - plan->startup_cost)
				* offset_rows / plan->plan_rows;
		plan->plan_rows -= offset_rows;
		if (plan->plan_rows < 1)
			plan->plan_rows = 1;
	}

	if (count_est != 0)
	{
		double		count_rows;

		if (count_est > 0)
			count_rows = (double) count_est;
		else
			count_rows = clamp_row_est(lefttree->plan_rows * 0.10);
		if (count_rows > plan->plan_rows)
			count_rows = plan->plan_rows;
		if (plan->plan_rows > 0)
			plan->total_cost = plan->startup_cost +
				(plan->total_cost - plan->startup_cost)
				* count_rows / plan->plan_rows;
		plan->plan_rows = count_rows;
		if (plan->plan_rows < 1)
			plan->plan_rows = 1;
	}

	plan->targetlist = cdbpullup_targetlist(lefttree,
				 cdbpullup_exprHasSubplanRef((Expr *) lefttree->targetlist));
	plan->qual = NIL;
	plan->lefttree = lefttree;
	plan->righttree = NULL;

	node->limitOffset = limitOffset;
	node->limitCount = limitCount;

	return node;
}

/*
 * make_result
 *	  Build a Result plan node
 *
 * If we have a subplan, assume that any evaluation costs for the gating qual
 * were already factored into the subplan's startup cost, and just copy the
 * subplan cost.  If there's no subplan, we should include the qual eval
 * cost.  In either case, tlist eval cost is not to be included here.
 */
Result *
make_result(PlannerInfo *root,
			List *tlist,
			Node *resconstantqual,
			Plan *subplan)
{
	Result	   *node = makeNode(Result);
	Plan	   *plan = &node->plan;

	if (subplan)
		copy_plan_costsize(plan, subplan);
	else
	{
		plan->startup_cost = 0;
		plan->total_cost = cpu_tuple_cost;
		plan->plan_rows = 1;	/* wrong if we have a set-valued function? */
		plan->plan_width = 0;	/* XXX is it worth being smarter? */
		if (resconstantqual)
		{
			QualCost	qual_cost;

			cost_qual_eval(&qual_cost, (List *) resconstantqual, root);
			/* resconstantqual is evaluated once at startup */
			plan->startup_cost += qual_cost.startup + qual_cost.per_tuple;
			plan->total_cost += qual_cost.startup + qual_cost.per_tuple;
		}
	}

	plan->targetlist = tlist;
	plan->qual = NIL;
	plan->lefttree = subplan;
	plan->righttree = NULL;
	node->resconstantqual = resconstantqual;

	node->hashFilter = false;
	node->hashList = NIL;

	return node;
}

/*
 * make_repeat
 *	  Build a Repeat plan node
 */
Repeat *
make_repeat(List *tlist,
			List *qual,
			Expr *repeatCountExpr,
			uint64 grouping,
			Plan *subplan)
{
	Repeat	   *node = makeNode(Repeat);
	Plan	   *plan = &node->plan;

	Assert(subplan != NULL);
	copy_plan_costsize(plan, subplan);

	plan->targetlist = tlist;
	plan->qual = qual;
	plan->lefttree = subplan;
	plan->righttree = NULL;

	node->repeatCountExpr = repeatCountExpr;
	node->grouping = grouping;

	return node;
}

/*
 * is_projection_capable_plan
 *		Check whether a given Plan node is able to do projection.
 */
bool
is_projection_capable_plan(Plan *plan)
{
	/* Most plan types can project, so just list the ones that can't */
	switch (nodeTag(plan))
	{
		case T_Hash:
		case T_Material:
		case T_Sort:
		case T_Unique:
		case T_SetOp:
		case T_Limit:
		case T_Append:
		case T_RecursiveUnion:
		case T_Motion:
		case T_ShareInputScan:
			return false;
		default:
			break;
	}
	return true;
}


/*
 * plan_pushdown_tlist
 *
 * If the given Plan node does projection, the same node is returned after
 * replacing its targetlist with the given targetlist.
 *
 * Otherwise, returns a Result node with the given targetlist, inserted atop
 * the given plan.
 */
Plan *
plan_pushdown_tlist(PlannerInfo *root, Plan *plan, List *tlist)
{
	if (is_projection_capable_plan(plan))
	{
		/* Fix up annotation of plan's distribution and ordering properties. */
		if (plan->flow)
			plan->flow = pull_up_Flow((Plan *) make_result(root, tlist, NULL, plan),
									  plan);

		/* Install the new targetlist. */
		plan->targetlist = tlist;
	}
	else
	{
		Plan	   *subplan = plan;

		/* Insert a Result node to evaluate the targetlist. */
		plan = (Plan *) make_result(root, tlist, NULL, subplan);

		/* Propagate the subplan's distribution. */
		if (subplan->flow)
			plan->flow = pull_up_Flow(plan, subplan);
	}
	return plan;
}	/* plan_pushdown_tlist */

/*
 * Return true if there is the same tleSortGroupRef in an entry in glist
 * as the tleSortGroupRef in gc.
 */
static bool
groupcol_in_list(SortGroupClause *gc, List *glist)
{
	bool		found = false;
	ListCell   *lc;

	foreach(lc, glist)
	{
		SortGroupClause *entry = (SortGroupClause *) lfirst(lc);

		Assert(IsA(entry, SortGroupClause));
		if (gc->tleSortGroupRef == entry->tleSortGroupRef)
		{
			found = true;
			break;
		}
	}
	return found;
}


/*
 * Construct a list of GroupClauses from the transformed GROUP BY clause.
 * This list of GroupClauses has unique tleSortGroupRefs.
 */
static List *
flatten_grouping_list(List *groupcls)
{
	List	   *result = NIL;
	ListCell   *gc;

	foreach(gc, groupcls)
	{
		Node	   *node = (Node *) lfirst(gc);

		if (node == NULL)
			continue;

		Assert(IsA(node, GroupingClause) ||
			   IsA(node, SortGroupClause) ||
			   IsA(node, List));

		if (IsA(node, GroupingClause))
		{
			List	   *groupsets = ((GroupingClause *) node)->groupsets;
			List	   *flatten_groupsets =
			flatten_grouping_list(groupsets);
			ListCell   *lc;

			foreach(lc, flatten_groupsets)
			{
				SortGroupClause *flatten_gc = (SortGroupClause *) lfirst(lc);

				Assert(IsA(flatten_gc, SortGroupClause));

				if (!groupcol_in_list(flatten_gc, result))
					result = lappend(result, flatten_gc);
			}

		}
		else if (IsA(node, List))
		{
			List	   *flatten_groupsets =
			flatten_grouping_list((List *) node);
			ListCell   *lc;

			foreach(lc, flatten_groupsets)
			{
				SortGroupClause *flatten_gc = (SortGroupClause *) lfirst(lc);

				Assert(IsA(flatten_gc, SortGroupClause));

				if (!groupcol_in_list(flatten_gc, result))
					result = lappend(result, flatten_gc);
			}
		}
		else
		{
			Assert(IsA(node, SortGroupClause));

			if (!groupcol_in_list((SortGroupClause *) node, result))
				result = lappend(result, node);
		}
	}

	return result;
}
