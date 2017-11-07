/*-------------------------------------------------------------------------
 *
 * allpaths.c
 *	  Routines to find possible search paths for processing a query
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/optimizer/path/allpaths.c,v 1.171 2008/06/27 03:56:55 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <math.h>

#include "nodes/nodeFuncs.h"
#ifdef OPTIMIZER_DEBUG
#include "nodes/print.h"
#endif
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/plancat.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/prep.h"
#include "optimizer/var.h"
#include "optimizer/planshare.h"
#include "parser/parse_clause.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"
#include "utils/guc.h"

#include "cdb/cdbmutate.h"		/* cdbmutate_warn_ctid_without_segid */
#include "cdb/cdbpath.h"		/* cdbpath_rows() */

// TODO: these planner gucs need to be refactored into PlannerConfig.
bool		gp_enable_sort_limit = FALSE;
bool		gp_enable_sort_distinct = FALSE;

/* Hook for plugins to replace standard_join_search() */
join_search_hook_type join_search_hook = NULL;


static void set_base_rel_pathlists(PlannerInfo *root);
static void set_rel_pathlist(PlannerInfo *root, RelOptInfo *rel,
				 Index rti, RangeTblEntry *rte);
static void set_plain_rel_pathlist(PlannerInfo *root, RelOptInfo *rel,
					   RangeTblEntry *rte);
static void set_append_rel_pathlist(PlannerInfo *root, RelOptInfo *rel,
						Index rti, RangeTblEntry *rte);
static bool has_multiple_baserels(PlannerInfo *root);
static void set_dummy_rel_pathlist(PlannerInfo *root, RelOptInfo *rel);
static void set_subquery_pathlist(PlannerInfo *root, RelOptInfo *rel,
					  Index rti, RangeTblEntry *rte);
static void set_function_pathlist(PlannerInfo *root, RelOptInfo *rel,
					  RangeTblEntry *rte);
static void set_tablefunction_pathlist(PlannerInfo *root, RelOptInfo *rel,
						   RangeTblEntry *rte);
static void set_values_pathlist(PlannerInfo *root, RelOptInfo *rel,
					RangeTblEntry *rte);
static void set_cte_pathlist(PlannerInfo *root, RelOptInfo *rel,
							 RangeTblEntry *rte);
static void set_worktable_pathlist(PlannerInfo *root, RelOptInfo *rel,
								   RangeTblEntry *rte);
static RelOptInfo *make_rel_from_joinlist(PlannerInfo *root, List *joinlist);
static Query *push_down_restrict(PlannerInfo *root, RelOptInfo *rel,
				   RangeTblEntry *rte, Index rti, Query *subquery);
static bool subquery_is_pushdown_safe(Query *subquery, Query *topquery,
						  bool *differentTypes);
static bool recurse_pushdown_safe(Node *setOp, Query *topquery,
					  bool *differentTypes);
static void compare_tlist_datatypes(List *tlist, List *colTypes,
						bool *differentTypes);
static bool qual_is_pushdown_safe(Query *subquery, Index rti, Node *qual,
					  bool *differentTypes);
static void subquery_push_qual(Query *subquery,
				   RangeTblEntry *rte, Index rti, Node *qual);
static void recurse_push_qual(Node *setOp, Query *topquery,
				  RangeTblEntry *rte, Index rti, Node *qual);
#ifdef _MSC_VER
__declspec(noreturn)
#endif
static void cdb_no_path_for_query(void) __attribute__((__noreturn__));


/*
 * make_one_rel
 *	  Finds all possible access paths for executing a query, returning a
 *	  single rel that represents the join of all base rels in the query.
 */
RelOptInfo *
make_one_rel(PlannerInfo *root, List *joinlist)
{
	RelOptInfo *rel;

	/*
	 * Generate access paths for the base rels.
	 */
	set_base_rel_pathlists(root);

	/*
	 * CDB: If join, warn of any tables that need ANALYZE.
	 */
	if (has_multiple_baserels(root))
	{
		Index		rti;
		RelOptInfo *brel;
		RangeTblEntry *brte;

		for (rti = 1; rti < root->simple_rel_array_size; rti++)
		{
			brel = root->simple_rel_array[rti];
			if (brel &&
				brel->cdb_default_stats_used)
			{
				brte = rt_fetch(rti, root->parse->rtable);
				cdb_default_stats_warning_for_table(brte->relid);
			}
		}
	}

	/*
	 * Generate access paths for the entire join tree.
	 */
	rel = make_rel_from_joinlist(root, joinlist);

	/* CDB: No path might be found if user set enable_xxx = off */
	if (!rel ||
		!rel->cheapest_total_path)
		cdb_no_path_for_query();	/* raise error - no return */

	/*
	 * The result should join all and only the query's base rels.
	 */
#ifdef USE_ASSERT_CHECKING
	{
		int			num_base_rels = 0;
		Index		rti;

		for (rti = 1; rti < root->simple_rel_array_size; rti++)
		{
			RelOptInfo *brel = root->simple_rel_array[rti];

			if (brel == NULL)
				continue;

			Assert(brel->relid == rti); /* sanity check on array */

			/* ignore RTEs that are "other rels" */
			if (brel->reloptkind != RELOPT_BASEREL)
				continue;

			Assert(bms_is_member(rti, rel->relids));
			num_base_rels++;
		}

		Assert(bms_num_members(rel->relids) == num_base_rels);
	}
#endif

	return rel;
}

/*
 * set_base_rel_pathlists
 *	  Finds all paths available for scanning each base-relation entry.
 *	  Sequential scan and any available indices are considered.
 *	  Each useful path is attached to its relation's 'pathlist' field.
 */
static void
set_base_rel_pathlists(PlannerInfo *root)
{
	Index		rti;

	for (rti = 1; rti < root->simple_rel_array_size; rti++)
	{
		RelOptInfo *rel = root->simple_rel_array[rti];

		/* there may be empty slots corresponding to non-baserel RTEs */
		if (rel == NULL)
			continue;

		Assert(rel->relid == rti);		/* sanity check on array */

		/* ignore RTEs that are "other rels" */
		if (rel->reloptkind != RELOPT_BASEREL)
			continue;

		/* CDB: Warn if ctid column is referenced but gp_segment_id is not. */
		cdbmutate_warn_ctid_without_segid(root, rel);

		set_rel_pathlist(root, rel, rti, root->simple_rte_array[rti]);
	}
}

/*
 * set_rel_pathlist
 *	  Build access paths for a base relation
 */
static void
set_rel_pathlist(PlannerInfo *root, RelOptInfo *rel,
				 Index rti, RangeTblEntry *rte)
{
	if (rte->inh)
	{
		/* It's an "append relation", process accordingly */
		set_append_rel_pathlist(root, rel, rti, rte);
	}
	else if (rel->rtekind == RTE_SUBQUERY)
	{
		/* Subquery --- generate a separate plan for it */
		set_subquery_pathlist(root, rel, rti, rte);
	}
	else if (rel->rtekind == RTE_FUNCTION)
	{
		/* RangeFunction --- generate a suitable path for it */
		set_function_pathlist(root, rel, rte);
	}
	else if (rel->rtekind == RTE_TABLEFUNCTION)
	{
		/* RangeFunction --- generate a separate plan for it */
		set_tablefunction_pathlist(root, rel, rte);
	}
	else if (rel->rtekind == RTE_VALUES)
	{
		/* Values list --- generate a suitable path for it */
		set_values_pathlist(root, rel, rte);
	}
	else if (rel->rtekind == RTE_CTE)
	{
		/* CTE reference --- generate a suitable path for it */
		if (rte->self_reference)
			set_worktable_pathlist(root, rel, rte);
		else
			set_cte_pathlist(root, rel, rte);
	}
	else
	{
		/* Plain relation */
		Assert(rel->rtekind == RTE_RELATION);
		set_plain_rel_pathlist(root, rel, rte);
	}

#ifdef OPTIMIZER_DEBUG
	debug_print_rel(root, rel);
#endif
}

/*
 * set_plain_rel_pathlist
 *	  Build access paths for a plain relation (no subquery, no inheritance)
 */
static void
set_plain_rel_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
	List	   *pathlist = NIL;
	List	   *indexpathlist = NIL;
	List	   *bitmappathlist = NIL;
	List	   *tidpathlist = NIL;
	Path	   *seqpath = NULL;
	ListCell   *cell;

	/*
	 * If we can prove we don't need to scan the rel via constraint exclusion,
	 * set up a single dummy path for it.  We only need to check for regular
	 * baserels; if it's an otherrel, CE was already checked in
	 * set_append_rel_pathlist().
	 */
	if (rel->reloptkind == RELOPT_BASEREL &&
		relation_excluded_by_constraints(root, rel, rte))
	{
		set_dummy_rel_pathlist(root, rel);
		return;
	}

	/* Mark rel with estimated output rows, width, etc */
	set_baserel_size_estimates(root, rel);

	/* Test any partial indexes of rel for applicability */
	check_partial_indexes(root, rel);

	/*
	 * Check to see if we can extract any restriction conditions from join
	 * quals that are OR-of-AND structures.  If so, add them to the rel's
	 * restriction list, and recompute the size estimates.
	 */
	if (create_or_index_quals(root, rel))
		set_baserel_size_estimates(root, rel);

	/* CDB: Attach subquery duplicate suppression info. */
	if (root->join_info_list)
		rel->dedup_info = cdb_make_rel_dedup_info(root, rel);

	/*
	 * Generate paths and add them to the rel's pathlist.
	 *
	 * Note: add_path() will discard any paths that are dominated by another
	 * available path, keeping only those paths that are superior along at
	 * least one dimension of cost or sortedness.
	 *
	 * CDB: So that create_index_paths() and create_tidscan_paths() can set
	 * the rel->onerow flag before it is tested by add_path(), we gather the
	 * paths in a temporary list, then add them.
	 */

	/* early exit for external and append only relations */
	switch (rel->relstorage)
	{
		case RELSTORAGE_EXTERNAL:

			/*
			 * If the relation is external, create an external path for it and
			 * select it (only external path is considered for an external
			 * base rel).
			 */
			add_path(root, rel, (Path *) create_external_path(root, rel));
			set_cheapest(root, rel);
			return;

		case RELSTORAGE_AOROWS:
			seqpath = (Path *) create_appendonly_path(root, rel);
			break;

		case RELSTORAGE_AOCOLS:
			seqpath = (Path *) create_aocs_path(root, rel);
			break;

		case RELSTORAGE_HEAP:
			seqpath = create_seqscan_path(root, rel);
			break;

		default:

			/*
			 * should not be feasible, usually indicates a failure to
			 * correctly apply rewrite rules.
			 */
			elog(ERROR, "plan contains range table with relstorage='%c'", rel->relstorage);
			return;
	}

	/* Consider sequential scan */
	if (root->config->enable_seqscan)
		pathlist = lappend(pathlist, seqpath);

	/* Consider index and bitmap scans */
	create_index_paths(root, rel,
					   &indexpathlist, &bitmappathlist);

	/*
	 * Random access to Append-Only is slow because AO doesn't use the buffer
	 * pool and we want to avoid decompressing blocks multiple times.  So,
	 * only consider bitmap paths because they are processed in TID order.
	 * The appendonlyam.c module will optimize fetches in TID order by keeping
	 * the last decompressed block between fetch calls.
	 */
	if (rel->relstorage == RELSTORAGE_AOROWS ||
		rel->relstorage == RELSTORAGE_AOCOLS)
		indexpathlist = NIL;

	if (indexpathlist && root->config->enable_indexscan)
		pathlist = list_concat(pathlist, indexpathlist);
	if (bitmappathlist && root->config->enable_bitmapscan)
		pathlist = list_concat(pathlist, bitmappathlist);

	/* Consider TID scans */
	create_tidscan_paths(root, rel, &tidpathlist);

	/*
	 * AO and CO tables do not currently support TidScans. Disable TidScan
	 * path for such tables
	 */
	if (rel->relstorage == RELSTORAGE_AOROWS ||
		rel->relstorage == RELSTORAGE_AOCOLS)
		tidpathlist = NIL;

	if (tidpathlist && root->config->enable_tidscan)
		pathlist = list_concat(pathlist, tidpathlist);

	/* If no enabled path was found, consider disabled paths. */
	if (!pathlist)
	{
		pathlist = lappend(pathlist, seqpath);
		if (root->config->gp_enable_fallback_plan)
		{
			pathlist = list_concat(pathlist, indexpathlist);
			pathlist = list_concat(pathlist, bitmappathlist);
			pathlist = list_concat(pathlist, tidpathlist);
		}
	}

	/* Add them, now that we know whether the quals specify a unique key. */
	foreach(cell, pathlist)
		add_path(root, rel, (Path *) lfirst(cell));

	/* Now find the cheapest of the paths for this rel */
	set_cheapest(root, rel);
}

/*
 * set_append_rel_pathlist
 *	  Build access paths for an "append relation"
 *
 * The passed-in rel and RTE represent the entire append relation.	The
 * relation's contents are computed by appending together the output of
 * the individual member relations.  Note that in the inheritance case,
 * the first member relation is actually the same table as is mentioned in
 * the parent RTE ... but it has a different RTE and RelOptInfo.  This is
 * a good thing because their outputs are not the same size.
 */
static void
set_append_rel_pathlist(PlannerInfo *root, RelOptInfo *rel,
						Index rti, RangeTblEntry *rte)
{
	int			parentRTindex = rti;
	List	   *subpaths = NIL;
	double		parent_rows;
	double		parent_size;
	double	   *parent_attrsizes;
	int			nattrs;
	ListCell   *l;

	/* weighted average of widths */
	double		width_avg = 0;

	/*
	 * XXX for now, can't handle inherited expansion of FOR UPDATE/SHARE; can
	 * we do better?  (This will take some redesign because the executor
	 * currently supposes that every rowMark relation is involved in every row
	 * returned by the query.)
	 */
	if (get_rowmark(root->parse, parentRTindex))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("SELECT FOR UPDATE/SHARE is not supported for inheritance queries")));

	/* Mark rel with estimated output rows, width, etc */
	set_baserel_size_estimates(root, rel);

	/*
	 * Initialize to compute size estimates for whole append relation.
	 *
	 * We handle width estimates by weighting the widths of different
	 * child rels proportionally to their number of rows.  This is sensible
	 * because the use of width estimates is mainly to compute the total
	 * relation "footprint" if we have to sort or hash it.  To do this,
	 * we sum the total equivalent size (in "double" arithmetic) and then
	 * divide by the total rowcount estimate.  This is done separately for
	 * the total rel width and each attribute.
	 *
	 * Note: if you consider changing this logic, beware that child rels could
	 * have zero rows and/or width, if they were excluded by constraints.
	 */
	parent_rows = 0;
	parent_size = 0;
	nattrs = rel->max_attr - rel->min_attr + 1;
	parent_attrsizes = (double *) palloc0(nattrs * sizeof(double));

	/*
	 * Generate access paths for each member relation, and pick the cheapest
	 * path for each one.
	 */
	foreach(l, root->append_rel_list)
	{
		AppendRelInfo *appinfo = (AppendRelInfo *) lfirst(l);
		int			childRTindex;
		RangeTblEntry *childRTE;
		RelOptInfo *childrel;
		Path	   *childpath;
		ListCell   *parentvars;
		ListCell   *childvars;

		/* append_rel_list contains all append rels; ignore others */
		if (appinfo->parent_relid != parentRTindex)
			continue;

		childRTindex = appinfo->child_relid;
		childRTE = root->simple_rte_array[childRTindex];

		/*
		 * The child rel's RelOptInfo was already created during
		 * add_base_rels_to_query.
		 */
		childrel = find_base_rel(root, childRTindex);
		Assert(childrel->reloptkind == RELOPT_OTHER_MEMBER_REL);

		/*
		 * We have to copy the parent's targetlist and quals to the child,
		 * with appropriate substitution of variables.	However, only the
		 * baserestrictinfo quals are needed before we can check for
		 * constraint exclusion; so do that first and then check to see if we
		 * can disregard this child.
		 */
		childrel->baserestrictinfo = (List *)
			adjust_appendrel_attrs(root, (Node *) rel->baserestrictinfo,
								   appinfo);

		if (relation_excluded_by_constraints(root, childrel, childRTE))
		{
			/*
			 * This child need not be scanned, so we can omit it from the
			 * appendrel.  Mark it with a dummy cheapest-path though, in case
			 * best_appendrel_indexscan() looks at it later.
			 */
			set_dummy_rel_pathlist(root, childrel);
			continue;
		}

		/* CE failed, so finish copying targetlist and join quals */
		childrel->joininfo = (List *)
			adjust_appendrel_attrs(root, (Node *) rel->joininfo,
								   appinfo);
		childrel->reltargetlist = (List *)
			adjust_appendrel_attrs(root, (Node *) rel->reltargetlist,
								   appinfo);

		/*
		 * We have to make child entries in the EquivalenceClass data
		 * structures as well.
		 */
		if (rel->has_eclass_joins)
		{
			add_child_rel_equivalences(root, appinfo, rel, childrel);
			childrel->has_eclass_joins = true;
		}

		/*
		 * Note: we could compute appropriate attr_needed data for the
		 * child's variables, by transforming the parent's attr_needed
		 * through the translated_vars mapping.  However, currently there's
		 * no need because attr_needed is only examined for base relations
		 * not otherrels.  So we just leave the child's attr_needed empty.
		 */

		/*
		 * Compute the child's access paths, and add the cheapest one to the
		 * Append path we are constructing for the parent.
		 *
		 * It's possible that the child is itself an appendrel, in which case
		 * we can "cut out the middleman" and just add its child paths to our
		 * own list.  (We don't try to do this earlier because we need to
		 * apply both levels of transformation to the quals.)
		 */
		set_rel_pathlist(root, childrel, childRTindex, childRTE);

		childpath = childrel->cheapest_total_path;
		if (IsA(childpath, AppendPath))
			subpaths = list_concat(subpaths,
							list_copy(((AppendPath *) childpath)->subpaths));
		else
			subpaths = lappend(subpaths, childpath);

		/*
		 * Accumulate size information from each child.
		 */

		if (childrel->rows > 0)
		{
			parent_rows += cdbpath_rows(root, childrel->cheapest_total_path);
			width_avg += cdbpath_rows(root, childrel->cheapest_total_path) * childrel->width;

			/*
			 * Accumulate per-column estimates too.  Whole-row Vars and
			 * PlaceHolderVars can be ignored here.
			 */
			forboth(parentvars, rel->reltargetlist,
					childvars, childrel->reltargetlist)
			{
				Var		   *parentvar = (Var *) lfirst(parentvars);
				Var		   *childvar = (Var *) lfirst(childvars);

				if (IsA(parentvar, Var) &&
					IsA(childvar, Var))
				{
					int			pndx = parentvar->varattno - rel->min_attr;
					int			cndx = childvar->varattno - childrel->min_attr;

					parent_attrsizes[pndx] += childrel->attr_widths[cndx] * childrel->rows;
				}
			}
		}
	}

	/*
	 * GPDB_84_MERGE_FIXME: review the estimation math here; 8.4 changed the
	 * logic around. In particular, we capped the minimum parent_rows at 1,
	 * whereas upstream will divide by any positive number. We also set
	 * rel->width to width_avg if the parent_rows are zero, whereas upstream
	 * sets it to zero.
	 */
	/*
	 * Save the finished size estimates.
	 */
	rel->rows = parent_rows;
	if (parent_rows > 0)
	{
		int		i;

		rel->width = rint(width_avg / parent_rows);
		for (i = 0; i < nattrs; i++)
			rel->attr_widths[i] = rint(parent_attrsizes[i] / parent_rows);
	}
	else
		rel->width = 0;			/* attr_widths should be zero already */

	/* CDB: Just one child (or none)?  Set flag if result is at most 1 row. */
	if (!subpaths)
		rel->onerow = true;
	else if (list_length(subpaths) == 1)
		rel->onerow = ((Path *) linitial(subpaths))->parent->onerow;

	/*
	 * GPDB_84_MERGE_FIXME: ensure that this rel->tuples count works for us.
	 */
	/*
	 * Set "raw tuples" count equal to "rows" for the appendrel; needed
	 * because some places assume rel->tuples is valid for any baserel.
	 */
	rel->tuples = parent_rows;

	pfree(parent_attrsizes);

	/*
	 * Finally, build Append path and install it as the only access path for
	 * the parent rel.	(Note: this is correct even if we have zero or one
	 * live subpath due to constraint exclusion.)
	 */
	add_path(root, rel, (Path *) create_append_path(root, rel, subpaths));

	/* Select cheapest path (pretty easy in this case...) */
	set_cheapest(root, rel);
}

/*
 * set_dummy_rel_pathlist
 *	  Build a dummy path for a relation that's been excluded by constraints
 *
 * Rather than inventing a special "dummy" path type, we represent this as an
 * AppendPath with no members (see also IS_DUMMY_PATH macro).
 */
static void
set_dummy_rel_pathlist(PlannerInfo *root, RelOptInfo *rel)
{
	/* Set dummy size estimates --- we leave attr_widths[] as zeroes */
	rel->rows = 0;
	rel->width = 0;

	add_path(root, rel, (Path *) create_append_path(root, rel, NIL));

	/* Select cheapest path (pretty easy in this case...) */
	set_cheapest(root, rel);
}

/* quick-and-dirty test to see if any joining is needed */
static bool
has_multiple_baserels(PlannerInfo *root)
{
	int			num_base_rels = 0;
	Index		rti;

	for (rti = 1; rti < root->simple_rel_array_size; rti++)
	{
		RelOptInfo *brel = root->simple_rel_array[rti];

		if (brel == NULL)
			continue;

		/* ignore RTEs that are "other rels" */
		if (brel->reloptkind == RELOPT_BASEREL)
			if (++num_base_rels > 1)
				return true;
	}
	return false;
}

/*
 * set_subquery_pathlist
 *		Build the (single) access path for a subquery RTE
 */
static void
set_subquery_pathlist(PlannerInfo *root, RelOptInfo *rel,
					  Index rti, RangeTblEntry *rte)
{
	Query	   *subquery = rte->subquery;
	double		tuple_fraction;
	PlannerInfo *subroot;
	List	   *pathkeys;
	bool		forceDistRand;
	Path	   *subquery_path;
	PlannerConfig *config;

	/*
	 * Must copy the Query so that planning doesn't mess up the RTE contents
	 * (really really need to fix the planner to not scribble on its input,
	 * someday).
	 */
	subquery = copyObject(subquery);

	forceDistRand = rte->forceDistRandom;

	/* CDB: Could be a preplanned subquery from window_planner. */
	if (rte->subquery_plan == NULL)
	{
		/*
		 * push down quals if possible. Note subquery might be
		 * different pointer from original one.
		 */
		subquery = push_down_restrict(root, rel, rte, rti, subquery);

		/*
		 * CDB: Does the subquery return at most one row?
		 */
		rel->onerow = false;

		/* Set-returning function in tlist could give any number of rows. */
		if (expression_returns_set((Node *)subquery->targetList))
		{}

		/* Always one row if aggregate function without GROUP BY. */
		else if (!subquery->groupClause &&
				 (subquery->hasAggs || subquery->havingQual))
			rel->onerow = true;

		/* LIMIT 1 or less? */
		else if (subquery->limitCount &&
				 IsA(subquery->limitCount, Const) &&
				 !((Const *) subquery->limitCount)->constisnull)
		{
			Const	   *cnst = (Const *) subquery->limitCount;

			if (cnst->consttype == INT8OID &&
				DatumGetInt64(cnst->constvalue) <= 1)
				rel->onerow = true;
		}

		/*
		 * We can safely pass the outer tuple_fraction down to the subquery if the
		 * outer level has no joining, aggregation, or sorting to do. Otherwise
		 * we'd better tell the subquery to plan for full retrieval. (XXX This
		 * could probably be made more intelligent ...)
		 */
		if (subquery->hasAggs ||
			subquery->groupClause ||
			subquery->havingQual ||
			subquery->distinctClause ||
			subquery->sortClause ||
			has_multiple_baserels(root))
			tuple_fraction = 0.0;	/* default case */
		else
			tuple_fraction = root->tuple_fraction;

		/* Generate the plan for the subquery */
		config = CopyPlannerConfig(root->config);
		config->honor_order_by = false;		/* partial order is enough */

		rel->subplan = subquery_planner(root->glob, subquery,
									root,
									false, tuple_fraction,
									&subroot,
									config);
		rel->subrtable = subroot->parse->rtable;
	}
	else
	{
		/* This is a preplanned sub-query RTE. */
		rel->subplan = rte->subquery_plan;
		rel->subrtable = rte->subquery_rtable;
		subroot = root;
		/* XXX rel->onerow = ??? */
	}

	/* Copy number of output rows from subplan */
	if (rel->onerow)
		rel->tuples = 1;
	else
		rel->tuples = rel->subplan->plan_rows;

	/* CDB: Attach subquery duplicate suppression info. */
	if (root->join_info_list)
		rel->dedup_info = cdb_make_rel_dedup_info(root, rel);

	/* Mark rel with estimated output rows, width, etc */
	set_baserel_size_estimates(root, rel);

	/* Convert subquery pathkeys to outer representation */
	pathkeys = convert_subquery_pathkeys(root, rel, subroot->query_pathkeys);

	/* Generate appropriate path */
	subquery_path = create_subqueryscan_path(root, rel, pathkeys);

	if (forceDistRand)
		CdbPathLocus_MakeStrewn(&subquery_path->locus);

	add_path(root, rel, subquery_path);

	/* Select cheapest path (pretty easy in this case...) */
	set_cheapest(root, rel);
}

/*
 * set_function_pathlist
 *		Build the (single) access path for a function RTE
 */
static void
set_function_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
	/* CDB: Could the function return more than one row? */
	rel->onerow = !expression_returns_set(rte->funcexpr);

	/* CDB: Attach subquery duplicate suppression info. */
	if (root->join_info_list)
		rel->dedup_info = cdb_make_rel_dedup_info(root, rel);

	/* Mark rel with estimated output rows, width, etc */
	set_function_size_estimates(root, rel);

	/* Generate appropriate path */
	add_path(root, rel, create_functionscan_path(root, rel, rte));

	/* Select cheapest path (pretty easy in this case...) */
	set_cheapest(root, rel);
}

/*
 * set_tablefunction_pathlist
 *		Build the (single) access path for a table function RTE
 */
static void
set_tablefunction_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
	PlannerConfig *config;
	PlannerInfo *subroot = NULL;
	FuncExpr   *fexpr = (FuncExpr *) rte->funcexpr;
	ListCell   *arg;

	/* Cannot be a preplanned subquery from window_planner. */
	Assert(!rte->subquery_plan);
	Assert(fexpr && IsA(fexpr, FuncExpr));

	config = CopyPlannerConfig(root->config);
	config->honor_order_by = false;		/* partial order is enough */

	/* Plan input subquery */
	rel->subplan = subquery_planner(root->glob, rte->subquery, root,
									false,
									0.0, //tuple_fraction
									& subroot,
									config);
	rel->subrtable = subroot->parse->rtable;

	/*
	 * With the subquery planned we now need to clear the subquery from the
	 * TableValueExpr nodes, otherwise preprocess_expression will trip over
	 * it.
	 */
	foreach(arg, fexpr->args)
	{
		if (IsA(arg, TableValueExpr))
		{
			TableValueExpr *tve = (TableValueExpr *) arg;

			tve->subquery = NULL;
		}
	}

	/* Could the function return more than one row? */
	rel->onerow = !expression_returns_set(rte->funcexpr);

	/* Attach subquery duplicate suppression info. */
	if (root->join_info_list)
		rel->dedup_info = cdb_make_rel_dedup_info(root, rel);

	/* Mark rel with estimated output rows, width, etc */
	set_table_function_size_estimates(root, rel);

	/* Generate appropriate path */
	add_path(root, rel, create_tablefunction_path(root, rel, rte));

	/* Select cheapest path (pretty easy in this case...) */
	set_cheapest(root, rel);
}

/*
 * set_values_pathlist
 *		Build the (single) access path for a VALUES RTE
 */
static void
set_values_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
	/* Mark rel with estimated output rows, width, etc */
	set_values_size_estimates(root, rel);

	/* CDB: Just one row? */
	rel->onerow = (rel->tuples <= 1 &&
				   !expression_returns_set((Node *) rte->values_lists));

	/* CDB: Attach subquery duplicate suppression info. */
	if (root->join_info_list)
		rel->dedup_info = cdb_make_rel_dedup_info(root, rel);

	/* Generate appropriate path */
	add_path(root, rel, create_valuesscan_path(root, rel, rte));

	/* Select cheapest path (pretty easy in this case...) */
	set_cheapest(root, rel);
}

/*
 * set_cte_pathlist
 *		Build the (single) access path for a CTE RTE.
 */
static void
set_cte_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
	PlannerInfo *cteroot;
	Index		levelsup;
	int			ndx;
	ListCell   *lc;
	int			planinfo_id;
	CommonTableExpr *cte = NULL;
	double		tuple_fraction = 0.0;
	CtePlanInfo *cteplaninfo;
	Plan	   *subplan = NULL;
	List	   *subrtable = NULL;
	List	   *pathkeys = NULL;
	PlannerInfo *subroot = NULL;

	/*
	 * Find the referenced CTE based on the given range table entry
	 */
	levelsup = rte->ctelevelsup;
	cteroot = root;
	while (levelsup-- > 0)
	{
		cteroot = cteroot->parent_root;
		if (!cteroot)			/* shouldn't happen */
			elog(ERROR, "bad levelsup for CTE \"%s\"", rte->ctename);
	}

	ndx = 0;
	foreach(lc, cteroot->parse->cteList)
	{
		cte = (CommonTableExpr *) lfirst(lc);

		if (strcmp(cte->ctename, rte->ctename) == 0)
			break;
		ndx++;
	}
	if (lc == NULL)				/* shouldn't happen */
		elog(ERROR, "could not find CTE \"%s\"", rte->ctename);

	Assert(IsA(cte->ctequery, Query));

	/*
	 * In PostgreSQL, we use the index to look up the plan ID in the
	 * cteroot->cte_plan_ids list. In GPDB, CTE plans work differently, and
	 * we look up the CtePlanInfo struct in the list_cteplaninfo instead.
	 */
	planinfo_id = ndx;

	/*
	 * Determine whether we need to generate a new subplan for this CTE.
	 *
	 * There are the following cases:
	 *   (1) If this subquery can be pulled up as an InitPlan, we will
	 *       generate a new subplan. In InitPlan case, the subplan can
	 *       not be shared with the main query or other InitPlans. We
	 *       do not store this subplan in cteplaninfo.
	 *   (2) If we never generate a subplan for this CTE, then we generate
	 *       one. If the reference count for this CTE is greater than 1
	 *       (excluding ones used in InitPlans), we create multiple subplans,
	 *       each of which has a SharedNode on top. We store these subplans
	 *       in cteplaninfo so that they can be used later.
	 */
	Assert(list_length(cteroot->list_cteplaninfo) > planinfo_id);
	cteplaninfo = list_nth(cteroot->list_cteplaninfo, planinfo_id);

	/*
	 * If there is exactly one reference to this CTE in the query, or plan
	 * sharing is disabled, create a new subplan for this CTE. It will
	 * become simple subquery scan.
	 *
	 * NOTE: The check for "exactly one reference" is a bit fuzzy. The
	 * references are counted in parse analysis phase, and it's possible
	 * that we duplicate a reference during query planning. So the check
	 * for number of references must be treated merely as a hint. If it
	 * turns out that there are in fact multiple references to the same
	 * CTE, even though we thought that there is only one, we might choose
	 * a sub-optimal plan because we missed the opportunity to share the
	 * subplan. That's acceptable for now.
	 *
	 * subquery tree will be modified if any qual is pushed down.
	 * There's risk that it'd be confusing if the tree is used
	 * later. At the moment InitPlan case uses the tree, but it
	 * is called earlier than this pass always, so we don't avoid it.
	 *
	 * Also, we might want to think extracting "common"
	 * qual expressions between multiple references, but
	 * so far we don't support it.
	 */
	if (!root->config->gp_cte_sharing || cte->cterefcount == 1)
	{
		PlannerConfig *config = CopyPlannerConfig(root->config);

		/*
		 * Copy query node since subquery_planner may trash it, and we need it
		 * intact in case we need to create another plan for the CTE
		 */
		Query	   *subquery = (Query *) copyObject(cte->ctequery);

		/*
		 * Having multiple SharedScans can lead to deadlocks. For now,
		 * disallow sharing of ctes at lower levels.
		 */
		config->gp_cte_sharing = false;

		config->honor_order_by = false;

		if (!cte->cterecursive)
		{
			/*
			 * Adjust the subquery so that 'root', i.e. this subquery, is the
			 * parent of the CTE subquery, even though the CTE might've been
			 * higher up syntactically. This is because some of the quals that
			 * we push down might refer to relations between the current level
			 * and the CTE's syntactical level. Such relations are not visible
			 * at the CTE's syntactical level, and SS_finalize_plan() would
			 * throw an error on them.
			 */
			IncrementVarSublevelsUp((Node *) subquery, rte->ctelevelsup, 1);

			/*
			 * Push down quals, like we do in set_subquery_pathlist()
			 */
			subquery = push_down_restrict(root, rel, rte, rel->relid, subquery);
		}

		subplan = subquery_planner(cteroot->glob, subquery, root, cte->cterecursive,
								   tuple_fraction, &subroot, config);

		subrtable = subroot->parse->rtable;
		pathkeys = subroot->query_pathkeys;

		/*
		 * Do not store the subplan in cteplaninfo, since we will not share
		 * this plan.
		 */
	}
	else
	{
		/*
		 * If we haven't created a subplan for this CTE yet, do it now. This
		 * subplan will not be used by InitPlans, so that they can be shared
		 * if this CTE is referenced multiple times (excluding in InitPlans).
		 */
		if (cteplaninfo->shared_plan == NULL)
		{
			PlannerConfig *config = CopyPlannerConfig(root->config);

			/*
			 * Copy query node since subquery_planner may trash it and we need
			 * it intact in case we need to create another plan for the CTE
			 */
			Query	   *subquery = (Query *) copyObject(cte->ctequery);

			/*
			 * Having multiple SharedScans can lead to deadlocks. For now,
			 * disallow sharing of ctes at lower levels.
			 */
			config->gp_cte_sharing = false;

			config->honor_order_by = false;

			subplan = subquery_planner(cteroot->glob, subquery, cteroot, cte->cterecursive,
									   tuple_fraction, &subroot, config);

			cteplaninfo->shared_plan = prepare_plan_for_sharing(cteroot, subplan);
			cteplaninfo->subrtable = subroot->parse->rtable;
			cteplaninfo->pathkeys = subroot->query_pathkeys;
		}

		/*
		 * Create another ShareInputScan to reference the already-created
		 * subplan.
		 */
		subplan = share_prepared_plan(cteroot, cteplaninfo->shared_plan);
		subrtable = cteplaninfo->subrtable;
		pathkeys = cteplaninfo->pathkeys;
	}

	rel->subplan = subplan;
	rel->subrtable = subrtable;

	/* Mark rel with estimated output rows, width, etc */
	set_cte_size_estimates(root, rel, rel->subplan);

	/* Convert subquery pathkeys to outer representation */
	pathkeys = convert_subquery_pathkeys(root, rel, pathkeys);

	/* Generate appropriate path */
	add_path(root, rel, create_ctescan_path(root, rel, pathkeys));

	/* Select cheapest path (pretty easy in this case...) */
	set_cheapest(root, rel);
}

/*
 * set_worktable_pathlist
 *		Build the (single) access path for a self-reference CTE RTE
 */
static void
set_worktable_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
	Plan	   *cteplan;
	PlannerInfo *cteroot;
	Index		levelsup;
	CdbLocusType ctelocus;

	/*
	 * We need to find the non-recursive term's plan, which is in the plan
	 * level that's processing the recursive UNION, which is one level
	 * *below* where the CTE comes from.
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
	cteplan = cteroot->non_recursive_plan;
	if (!cteplan)				/* shouldn't happen */
		elog(ERROR, "could not find plan for CTE \"%s\"", rte->ctename);

	/* Mark rel with estimated output rows, width, etc */
	set_cte_size_estimates(root, rel, cteplan);

	ctelocus = cteplan->flow->locustype;
	/* Generate appropriate path */
	add_path(root, rel, create_worktablescan_path(root, rel, ctelocus));

	/* Select cheapest path (pretty easy in this case...) */
	set_cheapest(root, rel);
}

/*
 * make_rel_from_joinlist
 *	  Build access paths using a "joinlist" to guide the join path search.
 *
 * See comments for deconstruct_jointree() for definition of the joinlist
 * data structure.
 */
static RelOptInfo *
make_rel_from_joinlist(PlannerInfo *root, List *joinlist)
{
	int			levels_needed;
	List	   *initial_rels;
	ListCell   *jl;

	/*
	 * Count the number of child joinlist nodes.  This is the depth of the
	 * dynamic-programming algorithm we must employ to consider all ways of
	 * joining the child nodes.
	 */
	levels_needed = list_length(joinlist);

	if (levels_needed <= 0)
		return NULL;			/* nothing to do? */

	/*
	 * Construct a list of rels corresponding to the child joinlist nodes.
	 * This may contain both base rels and rels constructed according to
	 * sub-joinlists.
	 */
	initial_rels = NIL;
	foreach(jl, joinlist)
	{
		Node	   *jlnode = (Node *) lfirst(jl);
		RelOptInfo *thisrel;

		if (IsA(jlnode, RangeTblRef))
		{
			int			varno = ((RangeTblRef *) jlnode)->rtindex;

			thisrel = find_base_rel(root, varno);
		}
		else if (IsA(jlnode, List))
		{
			/* Recurse to handle subproblem */
			thisrel = make_rel_from_joinlist(root, (List *) jlnode);
		}
		else
		{
			elog(ERROR, "unrecognized joinlist node type: %d",
				 (int) nodeTag(jlnode));
			thisrel = NULL;		/* keep compiler quiet */
		}

		/* CDB: Fail if no path could be built due to set enable_xxx = off. */
		if (!thisrel ||
			!thisrel->cheapest_total_path)
			return NULL;

		initial_rels = lappend(initial_rels, thisrel);
	}

	if (levels_needed == 1)
	{
		/*
		 * Single joinlist node, so we're done.
		 */
		return (RelOptInfo *) linitial(initial_rels);
	}
	else
	{
		/*
		 * Consider the different orders in which we could join the rels,
		 * using a plugin, GEQO, or the regular join search code.
		 *
		 * We put the initial_rels list into a PlannerInfo field because
		 * has_legal_joinclause() needs to look at it (ugly :-().
		 */
		root->initial_rels = initial_rels;

		if (join_search_hook)
			return (*join_search_hook) (root, levels_needed, initial_rels);
		else
		{
			RelOptInfo *rel;

			rel = standard_join_search(root, levels_needed, initial_rels, false);
			if (rel == NULL && root->config->gp_enable_fallback_plan)
			{
				root->join_rel_hash = NULL;
				root->join_rel_list = NULL;
				rel = standard_join_search(root, levels_needed, initial_rels, true);
			}
			return rel;
		}
	}
}

/*
 * standard_join_search
 *	  Find possible joinpaths for a query by successively finding ways
 *	  to join component relations into join relations.
 *
 * 'levels_needed' is the number of iterations needed, ie, the number of
 *		independent jointree items in the query.  This is > 1.
 *
 * 'initial_rels' is a list of RelOptInfo nodes for each independent
 *		jointree item.	These are the components to be joined together.
 *		Note that levels_needed == list_length(initial_rels).
 *
 * Returns the final level of join relations, i.e., the relation that is
 * the result of joining all the original relations together.
 * At least one implementation path must be provided for this relation and
 * all required sub-relations.
 *
 * To support loadable plugins that modify planner behavior by changing the
 * join searching algorithm, we provide a hook variable that lets a plugin
 * replace or supplement this function.  Any such hook must return the same
 * final join relation as the standard code would, but it might have a
 * different set of implementation paths attached, and only the sub-joinrels
 * needed for these paths need have been instantiated.
 *
 * Note to plugin authors: the functions invoked during standard_join_search()
 * modify root->join_rel_list and root->join_rel_hash.	If you want to do more
 * than one join-order search, you'll probably need to save and restore the
 * original states of those data structures.  See geqo_eval() for an example.
 */
RelOptInfo *
standard_join_search(PlannerInfo *root, int levels_needed, List *initial_rels, bool fallback)
{
	List	  **joinitems;
	int			lev;
	RelOptInfo *rel;

	root->config->mpp_trying_fallback_plan = fallback;

	/*
	 * We employ a simple "dynamic programming" algorithm: we first find all
	 * ways to build joins of two jointree items, then all ways to build joins
	 * of three items (from two-item joins and single items), then four-item
	 * joins, and so on until we have considered all ways to join all the
	 * items into one rel.
	 *
	 * joinitems[j] is a list of all the j-item rels.  Initially we set
	 * joinitems[1] to represent all the single-jointree-item relations.
	 */
	joinitems = (List **) palloc0((levels_needed + 1) * sizeof(List *));

	joinitems[1] = initial_rels;

	for (lev = 2; lev <= levels_needed; lev++)
	{
		ListCell   *w = NULL;
		ListCell   *x;
		ListCell   *y;

		/*
		 * Determine all possible pairs of relations to be joined at this
		 * level, and build paths for making each one from every available
		 * pair of lower-level relations.
		 */
		joinitems[lev] = join_search_one_level(root, lev, joinitems);

		/*
		 * Do cleanup work on each just-processed rel.
		 */
		for (x = list_head(joinitems[lev]); x; x = y)	/* cannot use foreach */
		{
			y = lnext(x);
			rel = (RelOptInfo *) lfirst(x);

			/* Find and save the cheapest paths for this rel */
			set_cheapest(root, rel);

			/* CDB: Prune this rel if it has no path. */
			if (!rel->cheapest_total_path)
				joinitems[lev] = list_delete_cell(joinitems[lev], x, w);

			/* Keep this rel. */
			else
				w = x;

#ifdef OPTIMIZER_DEBUG
			debug_print_rel(root, rel);
#endif
		}
		/* If no paths found because enable_xxx=false, enable all & retry. */
		if (!joinitems[lev] &&
			root->config->gp_enable_fallback_plan &&
			!root->config->mpp_trying_fallback_plan)
		{
			root->config->mpp_trying_fallback_plan = true;
			lev--;
		}

	}

	/*
	 * We should have a single rel at the final level.
	 */
	if (joinitems[levels_needed] == NIL)
		return NULL;
	Assert(list_length(joinitems[levels_needed]) == 1);

	rel = (RelOptInfo *) linitial(joinitems[levels_needed]);

	return rel;
}

/*****************************************************************************
 *			PUSHING QUALS DOWN INTO SUBQUERIES
 *****************************************************************************/

/*
 * push_down_restrict
 *   push down restrictinfo to subquery if any.
 *
 * If there are any restriction clauses that have been attached to the
 * subquery relation, consider pushing them down to become WHERE or HAVING
 * quals of the subquery itself.  This transformation is useful because it
 * may allow us to generate a better plan for the subquery than evaluating
 * all the subquery output rows and then filtering them.
 *
 * There are several cases where we cannot push down clauses. Restrictions
 * involving the subquery are checked by subquery_is_pushdown_safe().
 * Restrictions on individual clauses are checked by
 * qual_is_pushdown_safe().  Also, we don't want to push down
 * pseudoconstant clauses; better to have the gating node above the
 * subquery.
 *
 * Non-pushed-down clauses will get evaluated as qpquals of the
 * SubqueryScan node.
 *
 * XXX Are there any cases where we want to make a policy decision not to
 * push down a pushable qual, because it'd result in a worse plan?
 */
static Query *
push_down_restrict(PlannerInfo *root, RelOptInfo *rel,
				   RangeTblEntry *rte, Index rti, Query *subquery)
{
	bool	   *differentTypes;

	/* Nothing to do here if it doesn't have qual at all */
	if (rel->baserestrictinfo == NIL)
		return subquery;

	/* We need a workspace for keeping track of set-op type coercions */
	differentTypes = (bool *)
		palloc0((list_length(subquery->targetList) + 1) * sizeof(bool));

	if (subquery_is_pushdown_safe(subquery, subquery, differentTypes))
	{
		/* Ok to consider pushing down individual quals */
		List	   *upperrestrictlist = NIL;
		ListCell   *l;

		foreach(l, rel->baserestrictinfo)
		{
			RestrictInfo *rinfo = (RestrictInfo *) lfirst(l);
			Node	   *clause = (Node *) rinfo->clause;

			if (!rinfo->pseudoconstant &&
				qual_is_pushdown_safe(subquery, rti, clause, differentTypes))
			{
				/* Push it down */
				subquery_push_qual(subquery, rte, rti, clause);
			}
			else
			{
				/* Keep it in the upper query */
				upperrestrictlist = lappend(upperrestrictlist, rinfo);
			}
		}
		rel->baserestrictinfo = upperrestrictlist;
	}

	pfree(differentTypes);

	return subquery;
}

/*
 * subquery_is_pushdown_safe - is a subquery safe for pushing down quals?
 *
 * subquery is the particular component query being checked.  topquery
 * is the top component of a set-operations tree (the same Query if no
 * set-op is involved).
 *
 * Conditions checked here:
 *
 * 1. If the subquery has a LIMIT clause, we must not push down any quals,
 * since that could change the set of rows returned.
 *
 * 2. If the subquery contains EXCEPT or EXCEPT ALL set ops we cannot push
 * quals into it, because that would change the results.
 *
 * 3. For subqueries using UNION/UNION ALL/INTERSECT/INTERSECT ALL, we can
 * push quals into each component query, but the quals can only reference
 * subquery columns that suffer no type coercions in the set operation.
 * Otherwise there are possible semantic gotchas.  So, we check the
 * component queries to see if any of them have different output types;
 * differentTypes[k] is set true if column k has different type in any
 * component.
 *
 * 4. If the subquery target list has expressions containing calls to
 * window functions, we must not push down any quals since this could
 * change the meaning of the query.  At runtime, window functions refer
 * to the executor state of their Window node.  If a pushed-down qual
 * removed a tuple, the state seen by later tuples (hence the values
 * of window functions) could be affected.
 *
 * 5. Do not push down quals if the subquery is a grouping extension
 * query, since this may change the meaning of the query.
 */
static bool
subquery_is_pushdown_safe(Query *subquery, Query *topquery,
						  bool *differentTypes)
{
	SetOperationStmt *topop;

	/* Check point 1 */
	if (subquery->limitOffset != NULL || subquery->limitCount != NULL)
		return false;

	/* Targetlist must not contain SRF */
	if (expression_returns_set((Node *) subquery->targetList))
		return false;

	/* See point 5. */
	if (subquery->groupClause != NULL &&
		contain_extended_grouping(subquery->groupClause))
		return false;

	/* Are we at top level, or looking at a setop component? */
	if (subquery == topquery)
	{
		/* Top level, so check any component queries */
		if (subquery->setOperations != NULL)
			if (!recurse_pushdown_safe(subquery->setOperations, topquery,
									   differentTypes))
				return false;
	}
	else
	{
		/* Setop component must not have more components (too weird) */
		if (subquery->setOperations != NULL)
			return false;
		/* Check whether setop component output types match top level */
		topop = (SetOperationStmt *) topquery->setOperations;
		Assert(topop && IsA(topop, SetOperationStmt));
		compare_tlist_datatypes(subquery->targetList,
								topop->colTypes,
								differentTypes);
	}
	return true;
}

/*
 * Helper routine to recurse through setOperations tree
 */
static bool
recurse_pushdown_safe(Node *setOp, Query *topquery,
					  bool *differentTypes)
{
	if (IsA(setOp, RangeTblRef))
	{
		RangeTblRef *rtr = (RangeTblRef *) setOp;
		RangeTblEntry *rte = rt_fetch(rtr->rtindex, topquery->rtable);
		Query	   *subquery = rte->subquery;

		Assert(subquery != NULL);
		return subquery_is_pushdown_safe(subquery, topquery, differentTypes);
	}
	else if (IsA(setOp, SetOperationStmt))
	{
		SetOperationStmt *op = (SetOperationStmt *) setOp;

		/* EXCEPT is no good */
		if (op->op == SETOP_EXCEPT)
			return false;
		/* Else recurse */
		if (!recurse_pushdown_safe(op->larg, topquery, differentTypes))
			return false;
		if (!recurse_pushdown_safe(op->rarg, topquery, differentTypes))
			return false;
	}
	else
	{
		elog(ERROR, "unrecognized node type: %d",
			 (int) nodeTag(setOp));
	}
	return true;
}

/*
 * Compare tlist's datatypes against the list of set-operation result types.
 * For any items that are different, mark the appropriate element of
 * differentTypes[] to show that this column will have type conversions.
 *
 * We don't have to care about typmods here: the only allowed difference
 * between set-op input and output typmods is input is a specific typmod
 * and output is -1, and that does not require a coercion.
 */
static void
compare_tlist_datatypes(List *tlist, List *colTypes,
						bool *differentTypes)
{
	ListCell   *l;
	ListCell   *colType = list_head(colTypes);

	foreach(l, tlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(l);

		if (tle->resjunk)
			continue;			/* ignore resjunk columns */
		if (colType == NULL)
			elog(ERROR, "wrong number of tlist entries");
		if (exprType((Node *) tle->expr) != lfirst_oid(colType))
			differentTypes[tle->resno] = true;
		colType = lnext(colType);
	}
	if (colType != NULL)
		elog(ERROR, "wrong number of tlist entries");
}



/*
 * qual_contains_winref
 *
 * does qual include a window ref node?
 *
 */
static bool
qual_contains_winref(Query *topquery,
					 Index rti, /* index of RTE of subquery where qual needs
								 * to be checked */
					 Node *qual)
{
	/*
	 * extract subquery where qual needs to be checked
	 */
	RangeTblEntry *rte = rt_fetch(rti, topquery->rtable);
	Query	   *subquery = rte->subquery;
	bool		result = false;

	if (NULL != subquery && NIL != subquery->windowClause)
	{
		/*
		 * qual needs to be resolved first to map qual columns to the
		 * underlying set of produced columns, e.g., if we work on a setop
		 * child
		 */
		Node	   *qualNew = ResolveNew(qual, rti, 0, rte,
										 subquery->targetList,
										 CMD_SELECT, 0, NULL);

		result = contain_window_function(qualNew);
		pfree(qualNew);
	}

	return result;
}


/*
 * qual_is_pushdown_safe_set_operation
 *
 * is a particular qual safe to push down set operation?
 *
 */
static bool
qual_is_pushdown_safe_set_operation(Query *subquery, Node *qual)
{
	SetOperationStmt *setop = (SetOperationStmt *) subquery->setOperations;

	/*
	 * MPP-21075
	 * for queries of the form:
	 *   SELECT * from (SELECT max(i) over () as w from X Union Select 1 as w) as foo where w > 0
	 * the qual (w > 0) is not push_down_safe since it uses a window ref
	 *
	 * we check if this is the case for either left or right setop inputs
	 *
	 */
	Index		rtiLeft = ((RangeTblRef *) setop->larg)->rtindex;
	Index		rtiRight = ((RangeTblRef *) setop->rarg)->rtindex;

	if (qual_contains_winref(subquery, rtiLeft, qual) ||
		qual_contains_winref(subquery, rtiRight, qual))
	{
		return false;
	}

	return true;
}


/*
 * qual_is_pushdown_safe - is a particular qual safe to push down?
 *
 * qual is a restriction clause applying to the given subquery (whose RTE
 * has index rti in the parent query).
 *
 * Conditions checked here:
 *
 * 1. The qual must not contain any subselects (mainly because I'm not sure
 * it will work correctly: sublinks will already have been transformed into
 * subplans in the qual, but not in the subquery).
 *
 * 2X. If we try to push qual below set operation, then qual must be pushable
 * below set operation children
 *
 * 2. The qual must not refer to the whole-row output of the subquery
 * (since there is no easy way to name that within the subquery itself).
 *
 * 3. The qual must not refer to any subquery output columns that were
 * found to have inconsistent types across a set operation tree by
 * subquery_is_pushdown_safe().
 *
 * 4. If the subquery uses DISTINCT ON, we must not push down any quals that
 * refer to non-DISTINCT output columns, because that could change the set
 * of rows returned.  This condition is vacuous for DISTINCT, because then
 * there are no non-DISTINCT output columns, but unfortunately it's fairly
 * expensive to tell the difference between DISTINCT and DISTINCT ON in the
 * parsetree representation.  It's cheaper to just make sure all the Vars
 * in the qual refer to DISTINCT columns.
 *
 * 5. We must not push down any quals that refer to subselect outputs that
 * return sets, else we'd introduce functions-returning-sets into the
 * subquery's WHERE/HAVING quals.
 *
 * 6. We must not push down any quals that refer to subselect outputs that
 * contain volatile functions, for fear of introducing strange results due
 * to multiple evaluation of a volatile function.
 */
static bool
qual_is_pushdown_safe(Query *subquery, Index rti, Node *qual,
					  bool *differentTypes)
{
	bool		safe = true;
	List	   *vars;
	ListCell   *vl;
	Bitmapset  *tested = NULL;

	/* Refuse subselects (point 1) */
	if (contain_subplans(qual))
		return false;

	/*
	 * (point 2X)
	 * if we try to push quals below set operation, make
	 * sure that qual is pushable to below set operation children
	 */
	if (NULL != subquery->setOperations &&
		!qual_is_pushdown_safe_set_operation(subquery, qual))
	{
		return false;
	}

	/*
	 * Examine all Vars used in clause; since it's a restriction clause, all
	 * such Vars must refer to subselect output columns.
	 */
	vars = pull_var_clause(qual, true);
	foreach(vl, vars)
	{
		Var		   *var = (Var *) lfirst(vl);
		TargetEntry *tle;

		/*
		 * XXX Punt if we find any PlaceHolderVars in the restriction clause.
		 * It's not clear whether a PHV could safely be pushed down, and even
		 * less clear whether such a situation could arise in any cases of
		 * practical interest anyway.  So for the moment, just refuse to push
		 * down.
		 */
		if (!IsA(var, Var))
		{
			safe = false;
			break;
		}

		Assert(var->varno == rti);

		/* Check point 2 */
		if (var->varattno == 0)
		{
			safe = false;
			break;
		}

		/*
		 * We use a bitmapset to avoid testing the same attno more than once.
		 * (NB: this only works because subquery outputs can't have negative
		 * attnos.)
		 */
		if (bms_is_member(var->varattno, tested))
			continue;
		tested = bms_add_member(tested, var->varattno);

		/* Check point 3 */
		if (differentTypes[var->varattno])
		{
			safe = false;
			break;
		}

		/* Must find the tlist element referenced by the Var */
		tle = get_tle_by_resno(subquery->targetList, var->varattno);
		Assert(tle != NULL);
		Assert(!tle->resjunk);

		/* If subquery uses DISTINCT or DISTINCT ON, check point 4 */
		if (subquery->distinctClause != NIL &&
			!targetIsInSortGroupList(tle, InvalidOid, subquery->distinctClause))
		{
			/* non-DISTINCT column, so fail */
			safe = false;
			break;
		}

		/* Refuse functions returning sets (point 5) */
		if (expression_returns_set((Node *) tle->expr))
		{
			safe = false;
			break;
		}

		/* Refuse volatile functions (point 6) */
		if (contain_volatile_functions((Node *) tle->expr))
		{
			safe = false;
			break;
		}

		/* Refuse subplans */
		if (contain_subplans((Node *) tle->expr))
		{
			safe = false;
			break;
		}

		/* MPP-19244:
		 * if subquery has WINDOW clause, it is safe to push-down quals that
		 * use columns included in in the Partition-By clauses of every OVER
		 * clause in the subquery
		 * */
		if (subquery->windowClause != NIL)
		{
			ListCell   *lc;

			foreach(lc, subquery->windowClause)
			{
				WindowClause *wc = (WindowClause *) lfirst(lc);

				if (!targetIsInSortGroupList(tle, InvalidOid, wc->partitionClause))
				{
					/*
					 * qual's columns are not included in Partition-By clause,
					 * so fail
					 */
					safe = false;
					break;
				}
			}
		}

	}

	list_free(vars);
	bms_free(tested);

	return safe;
}

/*
 * subquery_push_qual - push down a qual that we have determined is safe
 */
static void
subquery_push_qual(Query *subquery, RangeTblEntry *rte, Index rti, Node *qual)
{
	if (subquery->setOperations != NULL)
	{
		/* Recurse to push it separately to each component query */
		recurse_push_qual(subquery->setOperations, subquery,
						  rte, rti, qual);
	}
	else
	{
		/*
		 * We need to replace Vars in the qual (which must refer to outputs of
		 * the subquery) with copies of the subquery's targetlist expressions.
		 * Note that at this point, any uplevel Vars in the qual should have
		 * been replaced with Params, so they need no work.
		 *
		 * This step also ensures that when we are pushing into a setop tree,
		 * each component query gets its own copy of the qual.
		 */
		qual = ResolveNew(qual, rti, 0, rte,
						  subquery->targetList,
						  CMD_SELECT, 0,
						  &subquery->hasSubLinks);

		/*
		 * Now attach the qual to the proper place: normally WHERE, but if the
		 * subquery uses grouping or aggregation, put it in HAVING (since the
		 * qual really refers to the group-result rows).
		 */
		if (subquery->hasAggs || subquery->groupClause || subquery->havingQual)
			subquery->havingQual = make_and_qual(subquery->havingQual, qual);
		else
			subquery->jointree->quals =
				make_and_qual(subquery->jointree->quals, qual);

		/*
		 * We need not change the subquery's hasAggs or hasSublinks flags,
		 * since we can't be pushing down any aggregates that weren't there
		 * before, and we don't push down subselects at all.
		 */
	}
}

/*
 * Helper routine to recurse through setOperations tree
 */
static void
recurse_push_qual(Node *setOp, Query *topquery,
				  RangeTblEntry *rte, Index rti, Node *qual)
{
	if (IsA(setOp, RangeTblRef))
	{
		RangeTblRef *rtr = (RangeTblRef *) setOp;
		RangeTblEntry *subrte = rt_fetch(rtr->rtindex, topquery->rtable);
		Query	   *subquery = subrte->subquery;

		Assert(subquery != NULL);
		subquery_push_qual(subquery, rte, rti, qual);
	}
	else if (IsA(setOp, SetOperationStmt))
	{
		SetOperationStmt *op = (SetOperationStmt *) setOp;

		recurse_push_qual(op->larg, topquery, rte, rti, qual);
		recurse_push_qual(op->rarg, topquery, rte, rti, qual);
	}
	else
	{
		elog(ERROR, "unrecognized node type: %d",
			 (int) nodeTag(setOp));
	}
}

/*
 * cdb_no_path_for_query
 *
 * Raise error when the set of allowable paths for a query is empty.
 * Does not return.
 */
void
cdb_no_path_for_query(void)
{
	char	   *settings;

	settings = gp_guc_list_show(PGC_S_DEFAULT, gp_guc_list_for_no_plan);

	if (*settings)
	{
		ereport(ERROR,
				(errcode(ERRCODE_GP_FEATURE_NOT_CONFIGURED),
				 errmsg("Query requires a feature that has been disabled by a configuration setting."),
				 errdetail("Could not devise a query plan for the given query."),
				 errhint("Current settings:  %s", settings)));
	}
	else
		elog(ERROR, "Could not devise a query plan for the given query.");
}	/* cdb_no_path_for_query */



/*****************************************************************************
 *			DEBUG SUPPORT
 *****************************************************************************/

#ifdef OPTIMIZER_DEBUG

static void
print_relids(Relids relids)
{
	Relids		tmprelids;
	int			x;
	bool		first = true;

	tmprelids = bms_copy(relids);
	while ((x = bms_first_member(tmprelids)) >= 0)
	{
		if (!first)
			printf(" ");
		printf("%d", x);
		first = false;
	}
	bms_free(tmprelids);
}

static void
print_restrictclauses(PlannerInfo *root, List *clauses)
{
	ListCell   *l;

	foreach(l, clauses)
	{
		RestrictInfo *c = lfirst(l);

		print_expr((Node *) c->clause, root->parse->rtable);
		if (lnext(l))
			printf(", ");
	}
}

static void
print_path(PlannerInfo *root, Path *path, int indent)
{
	const char *ptype;
	bool		join = false;
	Path	   *subpath = NULL;
	int			i;

	switch (nodeTag(path))
	{
		case T_Path:
			ptype = "SeqScan";
			break;
		case T_IndexPath:
			ptype = "IdxScan";
			break;
		case T_BitmapHeapPath:
			ptype = "BitmapHeapScan";
			break;
		case T_BitmapAppendOnlyPath:
			if (((BitmapAppendOnlyPath *) path)->isAORow)
				ptype = "BitmapAppendOnlyScan Row-oriented";
			else
				ptype = "BitmapAppendOnlyScan Column-oriented";
			break;
		case T_BitmapAndPath:
			ptype = "BitmapAndPath";
			break;
		case T_BitmapOrPath:
			ptype = "BitmapOrPath";
			break;
		case T_TidPath:
			ptype = "TidScan";
			break;
		case T_AppendPath:
			ptype = "Append";
			break;
		case T_ResultPath:
			ptype = "Result";
			subpath = ((ResultPath *) path)->subpath;
			break;
		case T_MaterialPath:
			ptype = "Material";
			subpath = ((MaterialPath *) path)->subpath;
			break;
		case T_UniquePath:
			ptype = "Unique";
			subpath = ((UniquePath *) path)->subpath;
			break;
		case T_NestPath:
			ptype = "NestLoop";
			join = true;
			break;
		case T_MergePath:
			ptype = "MergeJoin";
			join = true;
			break;
		case T_HashPath:
			ptype = "HashJoin";
			join = true;
			break;
		default:
			ptype = "???Path";
			break;
	}

	for (i = 0; i < indent; i++)
		printf("\t");
	printf("%s", ptype);

	if (path->parent)
	{
		printf("(");
		print_relids(path->parent->relids);
		printf(") rows=%.0f", path->parent->rows);
	}
	printf(" cost=%.2f..%.2f\n", path->startup_cost, path->total_cost);

	if (path->pathkeys)
	{
		for (i = 0; i < indent; i++)
			printf("\t");
		printf("  pathkeys: ");
		print_pathkeys(path->pathkeys, root->parse->rtable);
	}

	if (join)
	{
		JoinPath   *jp = (JoinPath *) path;

		for (i = 0; i < indent; i++)
			printf("\t");
		printf("  clauses: ");
		print_restrictclauses(root, jp->joinrestrictinfo);
		printf("\n");

		if (IsA(path, MergePath))
		{
			MergePath  *mp = (MergePath *) path;

			if (mp->outersortkeys || mp->innersortkeys)
			{
				for (i = 0; i < indent; i++)
					printf("\t");
				printf("  sortouter=%d sortinner=%d\n",
					   ((mp->outersortkeys) ? 1 : 0),
					   ((mp->innersortkeys) ? 1 : 0));
			}
		}

		print_path(root, jp->outerjoinpath, indent + 1);
		print_path(root, jp->innerjoinpath, indent + 1);
	}

	if (subpath)
		print_path(root, subpath, indent + 1);
}

void
debug_print_rel(PlannerInfo *root, RelOptInfo *rel)
{
	ListCell   *l;

	printf("RELOPTINFO (");
	print_relids(rel->relids);
	printf("): rows=%.0f width=%d\n", rel->rows, rel->width);

	if (rel->baserestrictinfo)
	{
		printf("\tbaserestrictinfo: ");
		print_restrictclauses(root, rel->baserestrictinfo);
		printf("\n");
	}

	if (rel->joininfo)
	{
		printf("\tjoininfo: ");
		print_restrictclauses(root, rel->joininfo);
		printf("\n");
	}

	printf("\tpath list:\n");
	foreach(l, rel->pathlist)
		print_path(root, lfirst(l), 1);
	printf("\n\tcheapest startup path:\n");
	print_path(root, rel->cheapest_startup_path, 1);
	printf("\n\tcheapest total path:\n");
	print_path(root, rel->cheapest_total_path, 1);
	printf("\n");
	fflush(stdout);
}

#endif   /* OPTIMIZER_DEBUG */
