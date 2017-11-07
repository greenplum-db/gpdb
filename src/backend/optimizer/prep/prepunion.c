/*-------------------------------------------------------------------------
 *
 * prepunion.c
 *	  Routines to plan set-operation queries.  The filename is a leftover
 *	  from a time when only UNIONs were implemented.
 *
 * There are two code paths in the planner for set-operation queries.
 * If a subquery consists entirely of simple UNION ALL operations, it
 * is converted into an "append relation".	Otherwise, it is handled
 * by the general code in this module (plan_set_operations and its
 * subroutines).  There is some support code here for the append-relation
 * case, but most of the heavy lifting for that is done elsewhere,
 * notably in prepjointree.c and allpaths.c.
 *
 * There is also some code here to support planning of queries that use
 * inheritance (SELECT FROM foo*).	Inheritance trees are converted into
 * append relations, and thenceforth share code with the UNION ALL case.
 *
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/optimizer/prep/prepunion.c,v 1.152 2008/08/07 19:35:02 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"


#include "access/heapam.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
<<<<<<< HEAD
#include "cdb/cdbpartition.h"
#include "commands/tablecmds.h"
#include "nodes/makefuncs.h"
#include "optimizer/paths.h"
#include "nodes/nodeFuncs.h"
=======
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
>>>>>>> eca1388629facd9e65d2c7ce405e079ba2bc60c4
#include "optimizer/plancat.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/prep.h"
#include "optimizer/tlist.h"
#include "parser/parse_clause.h"
#include "parser/parse_coerce.h"
#include "parser/parsetree.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/selfuncs.h"

#include "cdb/cdbllize.h"                   /* pull_up_Flow() */
#include "cdb/cdbvars.h"
#include "cdb/cdbsetop.h"

static Plan *recurse_set_operations(Node *setOp, PlannerInfo *root,
					   double tuple_fraction,
					   List *colTypes, bool junkOK,
					   int flag, List *refnames_tlist,
<<<<<<< HEAD
					   List **sortClauses);
static Plan *generate_recursion_plan(SetOperationStmt *setOp,
						PlannerInfo *root, double tuple_fraction,
						List *refnames_tlist,
						List **sortClauses);
=======
					   List **sortClauses, double *pNumGroups);
>>>>>>> eca1388629facd9e65d2c7ce405e079ba2bc60c4
static Plan *generate_union_plan(SetOperationStmt *op, PlannerInfo *root,
					double tuple_fraction,
					List *refnames_tlist,
					List **sortClauses, double *pNumGroups);
static Plan *generate_nonunion_plan(SetOperationStmt *op, PlannerInfo *root,
					   double tuple_fraction,
					   List *refnames_tlist,
					   List **sortClauses, double *pNumGroups);
static List *recurse_union_children(Node *setOp, PlannerInfo *root,
					   double tuple_fraction,
					   SetOperationStmt *top_union,
					   List *refnames_tlist);
static Plan *make_union_unique(SetOperationStmt *op, Plan *plan,
				  PlannerInfo *root, double tuple_fraction,
				  List **sortClauses);
static bool choose_hashed_setop(PlannerInfo *root, List *groupClauses,
					Plan *input_plan,
					double dNumGroups, double dNumOutputRows,
					double tuple_fraction,
					const char *construct);
static List *generate_setop_tlist(List *colTypes, int flag,
					 Index varno,
					 bool hack_constants,
					 List *input_tlist,
					 List *refnames_tlist);
static List *generate_append_tlist(List *colTypes, bool flag,
					  List *input_plans,
					  List *refnames_tlist);
static List *generate_setop_grouplist(SetOperationStmt *op, List *targetlist);
static void expand_inherited_rtentry(PlannerInfo *root, RangeTblEntry *rte,
						 Index rti);
static void make_inh_translation_lists(Relation oldrelation,
						   Relation newrelation,
						   Index newvarno,
						   List **col_mappings,
						   List **translated_vars);
static Relids adjust_relid_set(Relids relids, Index oldrelid, Index newrelid);
static List *adjust_inherited_tlist(List *tlist,
					   AppendRelInfo *apprelinfo);


/*
 * plan_set_operations
 *
 *	  Plans the queries for a tree of set operations (UNION/INTERSECT/EXCEPT)
 *
 * This routine only deals with the setOperations tree of the given query.
 * Any top-level ORDER BY requested in root->parse->sortClause will be added
 * when we return to grouping_planner.
 *
 * tuple_fraction is the fraction of tuples we expect will be retrieved.
 * tuple_fraction is interpreted as for grouping_planner(); in particular,
 * zero means "all the tuples will be fetched".  Any LIMIT present at the
 * top level has already been factored into tuple_fraction.
 *
 * *sortClauses is an output argument: it is set to a list of SortGroupClauses
 * representing the result ordering of the topmost set operation.  (This will
 * be NIL if the output isn't ordered.)
 */
Plan *
plan_set_operations(PlannerInfo *root, double tuple_fraction,
					List **sortClauses)
{
	Query	   *parse = root->parse;
	SetOperationStmt *topop = (SetOperationStmt *) parse->setOperations;
	Node	   *node;
	Query	   *leftmostQuery;

	Assert(topop && IsA(topop, SetOperationStmt));

	/* check for unsupported stuff */
	Assert(parse->jointree->fromlist == NIL);
	Assert(parse->jointree->quals == NULL);
	Assert(parse->groupClause == NIL);
	Assert(parse->havingQual == NULL);
	Assert(parse->distinctClause == NIL);

	/*
	 * Find the leftmost component Query.  We need to use its column names for
	 * all generated tlists (else SELECT INTO won't work right).
	 */
	node = topop->larg;
	while (node && IsA(node, SetOperationStmt))
		node = ((SetOperationStmt *) node)->larg;
	Assert(node && IsA(node, RangeTblRef));
	leftmostQuery = rt_fetch(((RangeTblRef *) node)->rtindex,
							 parse->rtable)->subquery;
	Assert(leftmostQuery != NULL);

	/*
	 * If the topmost node is a recursive union, it needs special processing.
	 */
	if (root->hasRecursion)
		return generate_recursion_plan(topop, root, tuple_fraction,
									   leftmostQuery->targetList,
									   sortClauses);

	/*
	 * Recurse on setOperations tree to generate plans for set ops. The final
	 * output plan should have just the column types shown as the output from
	 * the top-level node, plus possibly resjunk working columns (we can rely
	 * on upper-level nodes to deal with that).
	 */
	return recurse_set_operations((Node *) topop, root, tuple_fraction,
								  topop->colTypes, true, -1,
								  leftmostQuery->targetList,
								  sortClauses, NULL);
}

/*
 * recurse_set_operations
 *	  Recursively handle one step in a tree of set operations
 *
 * tuple_fraction: fraction of tuples we expect to retrieve from node
 * colTypes: list of type OIDs of expected output columns
 * junkOK: if true, child resjunk columns may be left in the result
 * flag: if >= 0, add a resjunk output column indicating value of flag
 * refnames_tlist: targetlist to take column names from
 *
 * Returns a plan for the subtree, as well as these output parameters:
 * *sortClauses: receives list of SortGroupClauses for result plan, if any
 * *pNumGroups: if not NULL, we estimate the number of distinct groups
 *		in the result, and store it there
 *
 * We don't have to care about typmods here: the only allowed difference
 * between set-op input and output typmods is input is a specific typmod
 * and output is -1, and that does not require a coercion.
 */
static Plan *
recurse_set_operations(Node *setOp, PlannerInfo *root,
					   double tuple_fraction,
					   List *colTypes, bool junkOK,
					   int flag, List *refnames_tlist,
					   List **sortClauses, double *pNumGroups)
{
	if (IsA(setOp, RangeTblRef))
	{
		RangeTblRef *rtr = (RangeTblRef *) setOp;
		RangeTblEntry *rte = rt_fetch(rtr->rtindex, root->parse->rtable);
		Query	   *subquery = rte->subquery;
		PlannerInfo *subroot = NULL;
		Plan	   *subplan,
				   *plan;

		Assert(subquery != NULL);

		/*
		 * Generate plan for primitive subquery
		 */
		PlannerConfig *config = CopyPlannerConfig(root->config);
		config->honor_order_by = false;
		subplan = subquery_planner(root->glob, subquery,
								   root,
								   false,
								   tuple_fraction,
								   &subroot,
								   config);

		/*
		 * Estimate number of groups if caller wants it.  If the subquery
		 * used grouping or aggregation, its output is probably mostly
		 * unique anyway; otherwise do statistical estimation.
		 */
		if (pNumGroups)
		{
			if (subquery->groupClause || subquery->distinctClause ||
				subroot->hasHavingQual || subquery->hasAggs)
				*pNumGroups = subplan->plan_rows;
			else
				*pNumGroups = estimate_num_groups(subroot,
												  get_tlist_exprs(subquery->targetList, false),
												  subplan->plan_rows);
		}

		/*
		 * Add a SubqueryScan with the caller-requested targetlist
		 */
		plan = (Plan *)
			make_subqueryscan(root, generate_setop_tlist(colTypes, flag,
												   1,
												   true,
												   subplan->targetlist,
												   refnames_tlist),
							  NIL,
							  rtr->rtindex,
							  subplan,
							  subroot->parse->rtable);
		mark_passthru_locus(plan, FALSE, FALSE); /* CDB: no hash/sort keys */

		/*
		 * We don't bother to determine the subquery's output ordering since
		 * it won't be reflected in the set-op result anyhow.
		 */
		*sortClauses = NIL;

		return plan;
	}
	else if (IsA(setOp, SetOperationStmt))
	{
		SetOperationStmt *op = (SetOperationStmt *) setOp;
		Plan	   *plan;

		/* UNIONs are much different from INTERSECT/EXCEPT */
		if (op->op == SETOP_UNION)
			plan = generate_union_plan(op, root, tuple_fraction,
									   refnames_tlist,
									   sortClauses, pNumGroups);
		else
			plan = generate_nonunion_plan(op, root, tuple_fraction,
										  refnames_tlist,
										  sortClauses, pNumGroups);

		/*
		 * If necessary, add a Result node to project the caller-requested
		 * output columns.
		 *
		 * XXX you don't really want to know about this: setrefs.c will apply
		 * fix_upper_expr() to the Result node's tlist. This would fail if the
		 * Vars generated by generate_setop_tlist() were not exactly equal()
		 * to the corresponding tlist entries of the subplan. However, since
		 * the subplan was generated by generate_union_plan() or
		 * generate_nonunion_plan(), and hence its tlist was generated by
		 * generate_append_tlist(), this will work.  We just tell
		 * generate_setop_tlist() to use varno OUTER (this was changed for
         * better EXPLAIN output in CDB/MPP; varno 0 is used in PostgreSQL).
		 */
		if (flag >= 0 ||
			!tlist_same_datatypes(plan->targetlist, colTypes, junkOK))
		{
			plan = (Plan *)
				make_result(root,
							generate_setop_tlist(colTypes, flag,
												 OUTER,
												 false,
												 plan->targetlist,
												 refnames_tlist),
							NULL,
							plan);
            plan->flow = pull_up_Flow(plan, plan->lefttree);
		}
		return plan;
	}
	else
	{
		elog(ERROR, "unrecognized node type: %d",
			 (int) nodeTag(setOp));
		return NULL;			/* keep compiler quiet */
	}
}

/*
 * Generate plan for a recursive UNION node
 */
static Plan *
generate_recursion_plan(SetOperationStmt *setOp, PlannerInfo *root,
						double tuple_fraction,
						List *refnames_tlist,
						List **sortClauses)
{
	Plan	   *plan;
	Plan	   *lplan;
	Plan	   *rplan;
	List	   *tlist;

	/* Parser should have rejected other cases */
	if (setOp->op != SETOP_UNION || !setOp->all)
		elog(ERROR, "only UNION ALL queries can be recursive");
	/* Worktable ID should be assigned */
	Assert(root->wt_param_id >= 0);

	/*
	 * Unlike a regular UNION node, process the left and right inputs
	 * separately without any intention of combining them into one Append.
	 */
	lplan = recurse_set_operations(setOp->larg, root, tuple_fraction,
								   setOp->colTypes, false, -1,
								   refnames_tlist, sortClauses);

	/* The right plan will want to look at the left one ... */
	root->non_recursive_plan = lplan;
	rplan = recurse_set_operations(setOp->rarg, root, tuple_fraction,
								   setOp->colTypes, false, -1,
								   refnames_tlist, sortClauses);
	root->non_recursive_plan = NULL;

	/*
	 * Generate tlist for RecursiveUnion plan node --- same as in Append cases
	 */
	tlist = generate_append_tlist(setOp->colTypes, false,
								  list_make2(lplan, rplan),
								  refnames_tlist);

	/*
	 * And make the plan node.
	 */
	plan = (Plan *) make_recursive_union(tlist, lplan, rplan,
										 root->wt_param_id);

	*sortClauses = NIL;			/* result of UNION ALL is always unsorted */

	return plan;
}

/*
 * Generate plan for a UNION or UNION ALL node
 */
static Plan *
generate_union_plan(SetOperationStmt *op, PlannerInfo *root,
					double tuple_fraction,
					List *refnames_tlist,
					List **sortClauses, double *pNumGroups)
{
	List	   *planlist;
	List	   *tlist;
	Plan	   *plan;
	GpSetOpType optype = PSETOP_NONE; /* CDB */

	/*
	 * If plain UNION, tell children to fetch all tuples.
	 *
	 * Note: in UNION ALL, we pass the top-level tuple_fraction unmodified to
	 * each arm of the UNION ALL.  One could make a case for reducing the
	 * tuple fraction for later arms (discounting by the expected size of the
	 * earlier arms' results) but it seems not worth the trouble. The normal
	 * case where tuple_fraction isn't already zero is a LIMIT at top level,
	 * and passing it down as-is is usually enough to get the desired result
	 * of preferring fast-start plans.
	 */
	if (!op->all)
		tuple_fraction = 0.0;

	/*
	 * If any of my children are identical UNION nodes (same op, all-flag, and
	 * colTypes) then they can be merged into this node so that we generate
	 * only one Append and unique-ification for the lot.  Recurse to find such
	 * nodes and compute their children's plans.
	 */
	planlist = list_concat(recurse_union_children(op->larg, root,
												  tuple_fraction,
												  op, refnames_tlist),
						   recurse_union_children(op->rarg, root,
												  tuple_fraction,
												  op, refnames_tlist));
	
	/* CDB: Decide on approach, condition argument plans to suit. */
	if ( Gp_role == GP_ROLE_DISPATCH )
	{
		optype = choose_setop_type(planlist);
		adjust_setop_arguments(root, planlist, optype);
	}
	else if (Gp_role == GP_ROLE_UTILITY ||
			 Gp_role == GP_ROLE_EXECUTE) /* MPP-2928 */
	{
		optype = PSETOP_SEQUENTIAL_QD;
	}

	/*
	 * Generate tlist for Append plan node.
	 *
	 * The tlist for an Append plan isn't important as far as the Append is
	 * concerned, but we must make it look real anyway for the benefit of the
	 * next plan level up.
	 */
	tlist = generate_append_tlist(op->colTypes, false,
								  planlist, refnames_tlist);

	/*
	 * Append the child results together.
	 */
	plan = (Plan *) make_append(planlist, false, tlist);
	mark_append_locus(plan, optype); /* CDB: Mark the plan result locus. */

	/*
	 * For UNION ALL, we just need the Append plan.  For UNION, need to add
	 * node(s) to remove duplicates.
	 */
<<<<<<< HEAD
	if (!op->all)
	{
		List	   *sortList;

		sortList = generate_setop_sortlist(tlist);
		if (sortList)
		{
			if ( optype == PSETOP_PARALLEL_PARTITIONED )
			{
				/* CDB: Hash motion to collocate non-distinct tuples. */
				plan = (Plan *) make_motion_hash_all_targets(root, plan);
			}
			plan = (Plan *) make_sort_from_sortclauses(root, sortList, plan);
			mark_sort_locus(plan); /* CDB */
			plan = (Plan *) make_unique(plan, sortList);
            plan->flow = pull_up_Flow(plan, plan->lefttree);
		}
		*sortClauses = sortList;
	}
=======
	if (op->all)
		*sortClauses = NIL;		/* result of UNION ALL is always unsorted */
>>>>>>> eca1388629facd9e65d2c7ce405e079ba2bc60c4
	else
		plan = make_union_unique(op, plan, root, tuple_fraction, sortClauses);

	/*
	 * Estimate number of groups if caller wants it.  For now we just
	 * assume the output is unique --- this is certainly true for the
	 * UNION case, and we want worst-case estimates anyway.
	 */
	if (pNumGroups)
		*pNumGroups = plan->plan_rows;

	return plan;
}

/*
 * Generate plan for an INTERSECT, INTERSECT ALL, EXCEPT, or EXCEPT ALL node
 */
static Plan *
generate_nonunion_plan(SetOperationStmt *op, PlannerInfo *root,
					   double tuple_fraction,
					   List *refnames_tlist,
					   List **sortClauses, double *pNumGroups)
{
	Plan	   *lplan,
			   *rplan,
			   *plan;
	List	   *tlist,
			   *groupList,
			   *planlist,
			   *child_sortclauses;
	double		dLeftGroups,
				dRightGroups,
				dNumGroups,
				dNumOutputRows;
	long		numGroups;
	bool		use_hash;
	SetOpCmd	cmd;
<<<<<<< HEAD
	GpSetOpType optype = PSETOP_NONE; /* CDB */
=======
	int			firstFlag;
>>>>>>> eca1388629facd9e65d2c7ce405e079ba2bc60c4

	/* Recurse on children, ensuring their outputs are marked */
	lplan = recurse_set_operations(op->larg, root,
								   0.0 /* all tuples needed */ ,
								   op->colTypes, false, 0,
								   refnames_tlist,
								   &child_sortclauses, &dLeftGroups);
	rplan = recurse_set_operations(op->rarg, root,
								   0.0 /* all tuples needed */ ,
								   op->colTypes, false, 1,
								   refnames_tlist,
								   &child_sortclauses, &dRightGroups);

	/*
	 * For EXCEPT, we must put the left input first.  For INTERSECT, either
	 * order should give the same results, and we prefer to put the smaller
	 * input first in order to minimize the size of the hash table in the
	 * hashing case.  "Smaller" means the one with the fewer groups.
	 */
	if (op->op == SETOP_EXCEPT || dLeftGroups <= dRightGroups)
	{
		planlist = list_make2(lplan, rplan);
		firstFlag = 0;
	}
	else
	{
		planlist = list_make2(rplan, lplan);
		firstFlag = 1;
	}

	/* CDB: Decide on approach, condition argument plans to suit. */
	if ( Gp_role == GP_ROLE_DISPATCH )
	{
		optype = choose_setop_type(planlist);
		adjust_setop_arguments(root, planlist, optype);
	}
	else if ( Gp_role == GP_ROLE_UTILITY 
			|| Gp_role == GP_ROLE_EXECUTE ) /* MPP-2928 */
	{
		optype = PSETOP_SEQUENTIAL_QD;
	}
	
	/*
	 * Generate tlist for Append plan node.
	 *
	 * The tlist for an Append plan isn't important as far as the Append is
	 * concerned, but we must make it look real anyway for the benefit of the
	 * next plan level up.	In fact, it has to be real enough that the flag
	 * column is shown as a variable not a constant, else setrefs.c will get
	 * confused.
	 */
	tlist = generate_append_tlist(op->colTypes, true,
								  planlist, refnames_tlist);

	/*
	 * Append the child results together.
	 */
	plan = (Plan *) make_append(planlist, false, tlist);
	mark_append_locus(plan, optype); /* CDB: Mark the plan result locus. */

	/* Identify the grouping semantics */
	groupList = generate_setop_grouplist(op, tlist);

	/* punt if nothing to group on (can this happen?) */
	if (groupList == NIL)
	{
		*sortClauses = NIL;
		return plan;
	}
	
	if ( optype == PSETOP_PARALLEL_PARTITIONED )
	{
		/* CDB: Collocate non-distinct tuples prior to sort. */
		plan = (Plan *) make_motion_hash_all_targets(root, plan);
	}

<<<<<<< HEAD
	plan = (Plan *) make_sort_from_sortclauses(root, sortList, plan);
	mark_sort_locus(plan); /* CDB */
	
=======
	/*
	 * Estimate number of distinct groups that we'll need hashtable entries
	 * for; this is the size of the left-hand input for EXCEPT, or the smaller
	 * input for INTERSECT.  Also estimate the number of eventual output rows.
	 * In non-ALL cases, we estimate each group produces one output row;
	 * in ALL cases use the relevant relation size.  These are worst-case
	 * estimates, of course, but we need to be conservative.
	 */
	if (op->op == SETOP_EXCEPT)
	{
		dNumGroups = dLeftGroups;
		dNumOutputRows = op->all ? lplan->plan_rows : dNumGroups;
	}
	else
	{
		dNumGroups = Min(dLeftGroups, dRightGroups);
		dNumOutputRows = op->all ? Min(lplan->plan_rows, rplan->plan_rows) : dNumGroups;
	}

	/* Also convert to long int --- but 'ware overflow! */
	numGroups = (long) Min(dNumGroups, (double) LONG_MAX);

	/*
	 * Decide whether to hash or sort, and add a sort node if needed.
	 */
	use_hash = choose_hashed_setop(root, groupList, plan,
								   dNumGroups, dNumOutputRows, tuple_fraction,
								   (op->op == SETOP_INTERSECT) ? "INTERSECT" : "EXCEPT");

	if (!use_hash)
		plan = (Plan *) make_sort_from_sortclauses(root, groupList, plan);

	/*
	 * Finally, add a SetOp plan node to generate the correct output.
	 */
>>>>>>> eca1388629facd9e65d2c7ce405e079ba2bc60c4
	switch (op->op)
	{
		case SETOP_INTERSECT:
			cmd = op->all ? SETOPCMD_INTERSECT_ALL : SETOPCMD_INTERSECT;
			break;
		case SETOP_EXCEPT:
			cmd = op->all ? SETOPCMD_EXCEPT_ALL : SETOPCMD_EXCEPT;
			break;
		default:
			elog(ERROR, "unrecognized set op: %d", (int) op->op);
			cmd = SETOPCMD_INTERSECT;	/* keep compiler quiet */
			break;
	}
<<<<<<< HEAD
	plan = (Plan *) make_setop(cmd, plan, sortList, list_length(op->colTypes) + 1);
    plan->flow = pull_up_Flow(plan, plan->lefttree);
=======
	plan = (Plan *) make_setop(cmd, use_hash ? SETOP_HASHED : SETOP_SORTED,
							   plan, groupList,
							   list_length(op->colTypes) + 1,
							   use_hash ? firstFlag : -1,
							   numGroups, dNumOutputRows);

	/* Result is sorted only if we're not hashing */
	*sortClauses = use_hash ? NIL : groupList;
>>>>>>> eca1388629facd9e65d2c7ce405e079ba2bc60c4

	if (pNumGroups)
		*pNumGroups = dNumGroups;

	return plan;
}

/*
 * Pull up children of a UNION node that are identically-propertied UNIONs.
 *
 * NOTE: we can also pull a UNION ALL up into a UNION, since the distinct
 * output rows will be lost anyway.
 */
static List *
recurse_union_children(Node *setOp, PlannerInfo *root,
					   double tuple_fraction,
					   SetOperationStmt *top_union,
					   List *refnames_tlist)
{
	List	   *child_sortclauses;

	if (IsA(setOp, SetOperationStmt))
	{
		SetOperationStmt *op = (SetOperationStmt *) setOp;

		if (op->op == top_union->op &&
			(op->all == top_union->all || op->all) &&
			equal(op->colTypes, top_union->colTypes))
		{
			/* Same UNION, so fold children into parent's subplan list */
			return list_concat(recurse_union_children(op->larg, root,
													  tuple_fraction,
													  top_union,
													  refnames_tlist),
							   recurse_union_children(op->rarg, root,
													  tuple_fraction,
													  top_union,
													  refnames_tlist));
		}
	}

	/*
	 * Not same, so plan this child separately.
	 *
	 * Note we disallow any resjunk columns in child results.  This is
	 * necessary since the Append node that implements the union won't do any
	 * projection, and upper levels will get confused if some of our output
	 * tuples have junk and some don't.  This case only arises when we have an
	 * EXCEPT or INTERSECT as child, else there won't be resjunk anyway.
	 */
	return list_make1(recurse_set_operations(setOp, root,
											 tuple_fraction,
											 top_union->colTypes, false,
											 -1, refnames_tlist,
											 &child_sortclauses, NULL));
}

/*
 * Add nodes to the given plan tree to unique-ify the result of a UNION.
 */
static Plan *
make_union_unique(SetOperationStmt *op, Plan *plan,
				  PlannerInfo *root, double tuple_fraction,
				  List **sortClauses)
{
	List	   *groupList;
	double		dNumGroups;
	long		numGroups;

	/* Identify the grouping semantics */
	groupList = generate_setop_grouplist(op, plan->targetlist);

	/* punt if nothing to group on (can this happen?) */
	if (groupList == NIL)
	{
		*sortClauses = NIL;
		return plan;
	}

	/*
	 * XXX for the moment, take the number of distinct groups as equal to
	 * the total input size, ie, the worst case.  This is too conservative,
	 * but we don't want to risk having the hashtable overrun memory; also,
	 * it's not clear how to get a decent estimate of the true size.  One
	 * should note as well the propensity of novices to write UNION rather
	 * than UNION ALL even when they don't expect any duplicates...
	 */
	dNumGroups = plan->plan_rows;

	/* Also convert to long int --- but 'ware overflow! */
	numGroups = (long) Min(dNumGroups, (double) LONG_MAX);

	/* Decide whether to hash or sort */
	if (choose_hashed_setop(root, groupList, plan,
							dNumGroups, dNumGroups, tuple_fraction,
							"UNION"))
	{
		/* Hashed aggregate plan --- no sort needed */
		plan = (Plan *) make_agg(root,
								 plan->targetlist,
								 NIL,
								 AGG_HASHED,
								 list_length(groupList),
								 extract_grouping_cols(groupList,
													   plan->targetlist),
								 extract_grouping_ops(groupList),
								 numGroups,
								 0,
								 plan);
		/* Hashed aggregation produces randomly-ordered results */
		*sortClauses = NIL;
	}
	else
	{
		/* Sort and Unique */
		plan = (Plan *) make_sort_from_sortclauses(root, groupList, plan);
		plan = (Plan *) make_unique(plan, groupList);
		plan->plan_rows = dNumGroups;
		/* We know the sort order of the result */
		*sortClauses = groupList;
	}

	return plan;
}

/*
 * choose_hashed_setop - should we use hashing for a set operation?
 */
static bool
choose_hashed_setop(PlannerInfo *root, List *groupClauses,
					Plan *input_plan,
					double dNumGroups, double dNumOutputRows,
					double tuple_fraction,
					const char *construct)
{
	int			numGroupCols = list_length(groupClauses);
	bool		can_sort;
	bool		can_hash;
	Size		hashentrysize;
	Path		hashed_p;
	Path		sorted_p;

	/* Check whether the operators support sorting or hashing */
	can_sort = grouping_is_sortable(groupClauses);
	can_hash = grouping_is_hashable(groupClauses);
	if (can_hash && can_sort)
	{
		/* we have a meaningful choice to make, continue ... */
	}
	else if (can_hash)
		return true;
	else if (can_sort)
		return false;
	else
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 /* translator: %s is UNION, INTERSECT, or EXCEPT */
				 errmsg("could not implement %s", construct),
				 errdetail("Some of the datatypes only support hashing, while others only support sorting.")));

	/* Prefer sorting when enable_hashagg is off */
	if (!enable_hashagg)
		return false;

	/*
	 * Don't do it if it doesn't look like the hashtable will fit into
	 * work_mem.
	 */
	hashentrysize = MAXALIGN(input_plan->plan_width) + MAXALIGN(sizeof(MinimalTupleData));

	if (hashentrysize * dNumGroups > work_mem * 1024L)
		return false;

	/*
	 * See if the estimated cost is no more than doing it the other way.
	 *
	 * We need to consider input_plan + hashagg versus input_plan + sort +
	 * group.  Note that the actual result plan might involve a SetOp or
	 * Unique node, not Agg or Group, but the cost estimates for Agg and Group
	 * should be close enough for our purposes here.
	 *
	 * These path variables are dummies that just hold cost fields; we don't
	 * make actual Paths for these steps.
	 */
	cost_agg(&hashed_p, root, AGG_HASHED, 0,
			 numGroupCols, dNumGroups,
			 input_plan->startup_cost, input_plan->total_cost,
			 input_plan->plan_rows);

	/*
	 * Now for the sorted case.  Note that the input is *always* unsorted,
	 * since it was made by appending unrelated sub-relations together.
	 */
	sorted_p.startup_cost = input_plan->startup_cost;
	sorted_p.total_cost = input_plan->total_cost;
	/* XXX cost_sort doesn't actually look at pathkeys, so just pass NIL */
	cost_sort(&sorted_p, root, NIL, sorted_p.total_cost,
			  input_plan->plan_rows, input_plan->plan_width, -1.0);
	cost_group(&sorted_p, root, numGroupCols, dNumGroups,
			   sorted_p.startup_cost, sorted_p.total_cost,
			   input_plan->plan_rows);

	/*
	 * Now make the decision using the top-level tuple fraction.  First we
	 * have to convert an absolute count (LIMIT) into fractional form.
	 */
	if (tuple_fraction >= 1.0)
		tuple_fraction /= dNumOutputRows;

	if (compare_fractional_path_costs(&hashed_p, &sorted_p,
									  tuple_fraction) < 0)
	{
		/* Hashed is cheaper, so use it */
		return true;
	}
	return false;
}

/*
 * Generate targetlist for a set-operation plan node
 *
 * colTypes: column datatypes for non-junk columns
 * flag: -1 if no flag column needed, 0 or 1 to create a const flag column
 * varno: varno to use in generated Vars
 * hack_constants: true to copy up constants (see comments in code)
 * input_tlist: targetlist of this node's input node
 * refnames_tlist: targetlist to take column names from
 */
static List *
generate_setop_tlist(List *colTypes, int flag,
					 Index varno,
					 bool hack_constants,
					 List *input_tlist,
					 List *refnames_tlist)
{
	List	   *tlist = NIL;
	int			resno = 1;
	ListCell   *i,
			   *j,
			   *k;
	TargetEntry *tle;
	Node	   *expr;

	j = list_head(input_tlist);
	k = list_head(refnames_tlist);
	foreach(i, colTypes)
	{
		Oid			colType = lfirst_oid(i);
		TargetEntry *inputtle = (TargetEntry *) lfirst(j);
		TargetEntry *reftle = (TargetEntry *) lfirst(k);

		Assert(inputtle->resno == resno);
		Assert(reftle->resno == resno);
		Assert(!inputtle->resjunk);
		Assert(!reftle->resjunk);

		/*
		 * Generate columns referencing input columns and having appropriate
		 * data types and column names.  Insert datatype coercions where
		 * necessary.
		 *
		 * HACK: constants in the input's targetlist are copied up as-is
		 * rather than being referenced as subquery outputs.  This is mainly
		 * to ensure that when we try to coerce them to the output column's
		 * datatype, the right things happen for UNKNOWN constants.  But do
		 * this only at the first level of subquery-scan plans; we don't want
		 * phony constants appearing in the output tlists of upper-level
		 * nodes!
		 */
		if (hack_constants && inputtle->expr && IsA(inputtle->expr, Const))
			expr = (Node *) inputtle->expr;
		else
			expr = (Node *) makeVar(varno,
									inputtle->resno,
									exprType((Node *) inputtle->expr),
									exprTypmod((Node *) inputtle->expr),
									0);
		if (exprType(expr) != colType)
		{
			expr = coerce_to_common_type(NULL,	/* no UNKNOWNs here */
										 expr,
										 colType,
										 "UNION/INTERSECT/EXCEPT");
		}
		tle = makeTargetEntry((Expr *) expr,
							  (AttrNumber) resno++,
							  pstrdup(reftle->resname),
							  false);
		tlist = lappend(tlist, tle);

		j = lnext(j);
		k = lnext(k);
	}

	if (flag >= 0)
	{
		/* Add a resjunk flag column */
		/* flag value is the given constant */
		expr = (Node *) makeConst(INT4OID,
								  -1,
								  sizeof(int4),
								  Int32GetDatum(flag),
								  false,
								  true);
		tle = makeTargetEntry((Expr *) expr,
							  (AttrNumber) resno++,
							  pstrdup("flag"),
							  true);
		tlist = lappend(tlist, tle);
	}

	return tlist;
}

/*
 * Generate targetlist for a set-operation Append node
 *
 * colTypes: column datatypes for non-junk columns
 * flag: true to create a flag column copied up from subplans
 * input_plans: list of sub-plans of the Append
 * refnames_tlist: targetlist to take column names from
 *
 * The entries in the Append's targetlist should always be simple Vars;
 * we just have to make sure they have the right datatypes and typmods.
 * The Vars are always generated with varno OUTER (CDB/MPP change for
 * EXPLAIN; varno 0 was used in PostgreSQL).
 */
static List *
generate_append_tlist(List *colTypes, bool flag,
					  List *input_plans,
					  List *refnames_tlist)
{
	List	   *tlist = NIL;
	int			resno = 1;
	ListCell   *curColType;
	ListCell   *ref_tl_item;
	int			colindex;
	TargetEntry *tle;
	Node	   *expr;
	ListCell   *planl;
	int32	   *colTypmods;

	/*
	 * First extract typmods to use.
	 *
	 * If the inputs all agree on type and typmod of a particular column, use
	 * that typmod; else use -1.
	 */
	colTypmods = (int32 *) palloc(list_length(colTypes) * sizeof(int32));

	foreach(planl, input_plans)
	{
		Plan	   *subplan = (Plan *) lfirst(planl);
		ListCell   *subtlist;

		curColType = list_head(colTypes);
		colindex = 0;
		foreach(subtlist, subplan->targetlist)
		{
			TargetEntry *subtle = (TargetEntry *) lfirst(subtlist);

			if (subtle->resjunk)
				continue;
			Assert(curColType != NULL);
			if (exprType((Node *) subtle->expr) == lfirst_oid(curColType))
			{
				/* If first subplan, copy the typmod; else compare */
				int32		subtypmod = exprTypmod((Node *) subtle->expr);

				if (planl == list_head(input_plans))
					colTypmods[colindex] = subtypmod;
				else if (subtypmod != colTypmods[colindex])
					colTypmods[colindex] = -1;
			}
			else
			{
				/* types disagree, so force typmod to -1 */
				colTypmods[colindex] = -1;
			}
			curColType = lnext(curColType);
			colindex++;
		}
		Assert(curColType == NULL);
	}

	/*
	 * Now we can build the tlist for the Append.
	 */
	colindex = 0;
	forboth(curColType, colTypes, ref_tl_item, refnames_tlist)
	{
		Oid			colType = lfirst_oid(curColType);
		int32		colTypmod = colTypmods[colindex++];
		TargetEntry *reftle = (TargetEntry *) lfirst(ref_tl_item);

		Assert(reftle->resno == resno);
		Assert(!reftle->resjunk);
		expr = (Node *) makeVar(OUTER,
								resno,
								colType,
								colTypmod,
								0);
		tle = makeTargetEntry((Expr *) expr,
							  (AttrNumber) resno++,
							  pstrdup(reftle->resname),
							  false);
		tlist = lappend(tlist, tle);
	}

	if (flag)
	{
		/* Add a resjunk flag column */
		/* flag value is shown as copied up from subplan */
		expr = (Node *) makeVar(OUTER,
								resno,
								INT4OID,
								-1,
								0);
		tle = makeTargetEntry((Expr *) expr,
							  (AttrNumber) resno++,
							  pstrdup("flag"),
							  true);
		tlist = lappend(tlist, tle);
	}

	pfree(colTypmods);

	return tlist;
}

/*
 * generate_setop_grouplist
 *		Build a SortGroupClause list defining the sort/grouping properties
 *		of the setop's output columns.
 *
 * Parse analysis already determined the properties and built a suitable
 * list, except that the entries do not have sortgrouprefs set because
 * the parser output representation doesn't include a tlist for each
 * setop.  So what we need to do here is copy that list and install
 * proper sortgrouprefs into it and into the targetlist.
 */
static List *
generate_setop_grouplist(SetOperationStmt *op, List *targetlist)
{
	List	   *grouplist = (List *) copyObject(op->groupClauses);
	ListCell   *lg;
	ListCell   *lt;
	Index		refno = 1;

	lg = list_head(grouplist);
	foreach(lt, targetlist)
	{
<<<<<<< HEAD
		TargetEntry *tle = (TargetEntry *) lfirst(l);
		SortBy sortby;

		/* GPDB_84_MERGE_FIXME: ensure that this block is deleted in a future
		 * 8.4 merge iteration. */
		sortby.type = T_SortBy;
		sortby.sortby_dir = SORTBY_DEFAULT;
		sortby.sortby_nulls = SORTBY_NULLS_DEFAULT;
		sortby.useOp = NIL;
		sortby.location = -1;
		sortby.node = (Node *) tle->expr;

		if (!tle->resjunk)
			sortlist = addTargetToSortList(NULL, tle,
										   sortlist, targetlist,
										   &sortby, false);
=======
		TargetEntry *tle = (TargetEntry *) lfirst(lt);
		SortGroupClause *sgc;

		/* tlist shouldn't have any sortgrouprefs yet */
		Assert(tle->ressortgroupref == 0);

		if (tle->resjunk)
			continue;			/* ignore resjunk columns */

		/* non-resjunk columns should have grouping clauses */
		Assert(lg != NULL);
		sgc = (SortGroupClause *) lfirst(lg);
		lg = lnext(lg);
		Assert(sgc->tleSortGroupRef == 0);

		/* we could use assignSortGroupRef here, but seems a bit silly */
		sgc->tleSortGroupRef = tle->ressortgroupref = refno++;
>>>>>>> eca1388629facd9e65d2c7ce405e079ba2bc60c4
	}
	Assert(lg == NULL);
	return grouplist;
}


/*
 * find_all_inheritors -
 *		Returns a list of relation OIDs including the given rel plus
 *		all relations that inherit from it, directly or indirectly.
 */
List *
find_all_inheritors(Oid parentrel)
{
	List	   *rels_list;
	ListCell   *l;

	/*
	 * We build a list starting with the given rel and adding all direct and
	 * indirect children.  We can use a single list as both the record of
	 * already-found rels and the agenda of rels yet to be scanned for more
	 * children.  This is a bit tricky but works because the foreach() macro
	 * doesn't fetch the next list element until the bottom of the loop.
	 */
	rels_list = list_make1_oid(parentrel);

	foreach(l, rels_list)
	{
		Oid			currentrel = lfirst_oid(l);
		List	   *currentchildren;

		/* Get the direct children of this rel */
		currentchildren = find_inheritance_children(currentrel);

		/*
		 * Add to the queue only those children not already seen. This avoids
		 * making duplicate entries in case of multiple inheritance paths from
		 * the same parent.  (It'll also keep us from getting into an infinite
		 * loop, though theoretically there can't be any cycles in the
		 * inheritance graph anyway.)
		 */
		rels_list = list_concat_unique_oid(rels_list, currentchildren);
	}

	return rels_list;
}

/*
 * expand_inherited_tables
 *		Expand each rangetable entry that represents an inheritance set
 *		into an "append relation".	At the conclusion of this process,
 *		the "inh" flag is set in all and only those RTEs that are append
 *		relation parents.
 */
void
expand_inherited_tables(PlannerInfo *root)
{
	Index		nrtes;
	Index		rti;
	ListCell   *rl;

	/*
	 * expand_inherited_rtentry may add RTEs to parse->rtable; there is no
	 * need to scan them since they can't have inh=true.  So just scan as far
	 * as the original end of the rtable list.
	 */
	nrtes = list_length(root->parse->rtable);
	rl = list_head(root->parse->rtable);
	for (rti = 1; rti <= nrtes; rti++)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(rl);

		expand_inherited_rtentry(root, rte, rti);
		rl = lnext(rl);
	}
}

/*
 * expand_inherited_rtentry
 *		Check whether a rangetable entry represents an inheritance set.
 *		If so, add entries for all the child tables to the query's
 *		rangetable, and build AppendRelInfo nodes for all the child tables
 *		and add them to root->append_rel_list.	If not, clear the entry's
 *		"inh" flag to prevent later code from looking for AppendRelInfos.
 *
 * Note that the original RTE is considered to represent the whole
 * inheritance set.  The first of the generated RTEs is an RTE for the same
 * table, but with inh = false, to represent the parent table in its role
 * as a simple member of the inheritance set.
 *
 * A childless table is never considered to be an inheritance set; therefore
 * a parent RTE must always have at least two associated AppendRelInfos.
 */
static void
expand_inherited_rtentry(PlannerInfo *root, RangeTblEntry *rte, Index rti)
{
	Query	   *parse = root->parse;
	Oid			parentOID;
	Relation	oldrelation;
	LOCKMODE	lockmode;
	List	   *inhOIDs;
	List	   *appinfos;
	ListCell   *l;
	bool		parent_is_partitioned;
	Relids		child_relids = NULL;

	/* Does RT entry allow inheritance? */
	if (!rte->inh)
		return;
	/* Ignore any already-expanded UNION ALL nodes */
	if (rte->rtekind != RTE_RELATION)
	{
		Assert(rte->rtekind == RTE_SUBQUERY);
		return;
	}
	/* Fast path for common case of childless table */
	parentOID = rte->relid;
	if (!has_subclass_fast(parentOID))
	{
		/* Clear flag before returning */
		rte->inh = false;
		return;
	}

	/* Scan for all members of inheritance set */
	inhOIDs = find_all_inheritors(parentOID);

	/*
	 * Check that there's at least one descendant, else treat as no-child
	 * case.  This could happen despite above has_subclass() check, if table
	 * once had a child but no longer does.
	 */
	if (list_length(inhOIDs) < 2)
	{
		/* Clear flag before returning */
		rte->inh = false;
		return;
	}

	/*
	 * Must open the parent relation to examine its tupdesc.  We need not lock
	 * it since the rewriter already obtained at least AccessShareLock on each
	 * relation used in the query.
	 */
	oldrelation = heap_open(parentOID, NoLock);

	/*
	 * However, for each child relation we add to the query, we must obtain an
	 * appropriate lock, because this will be the first use of those relations
	 * in the parse/rewrite/plan pipeline.
	 *
	 * If the parent relation is the query's result relation, then we need
	 * RowExclusiveLock.  Otherwise, check to see if the relation is accessed
	 * FOR UPDATE/SHARE or not.  We can't just grab AccessShareLock because
	 * then the executor would be trying to upgrade the lock, leading to
	 * possible deadlocks.	(This code should match the parser and rewriter.)
	 */
	if (rti == parse->resultRelation)
		lockmode = RowExclusiveLock;
	else if (get_rowmark(parse, rti))
		lockmode = RowShareLock;
	else
		lockmode = AccessShareLock;

	parent_is_partitioned = rel_is_partitioned(parentOID);

	/* Scan the inheritance set and expand it */
	appinfos = NIL;
	foreach(l, inhOIDs)
	{
		Oid			childOID = lfirst_oid(l);
		Relation	newrelation;
		RangeTblEntry *childrte;
		Index		childRTindex;
		AppendRelInfo *appinfo;

		/*
		 * It is possible that the parent table has children that are temp
		 * tables of other backends.  We cannot safely access such tables
		 * (because of buffering issues), and the best thing to do seems to be
		 * to silently ignore them.
		 */
		if (childOID != parentOID &&
			isOtherTempNamespace(get_rel_namespace(childOID)))
			continue;

		/*
		 * show root and leaf partitions
		 */
		if (parent_is_partitioned && !rel_is_leaf_partition(childOID))
		{
			continue;
		}

		/* Open rel, acquire the appropriate lock type */
		if (childOID != parentOID)
			newrelation = heap_open(childOID, lockmode);
		else
			newrelation = oldrelation;

		/*
		 * Build an RTE for the child, and attach to query's rangetable list.
		 * We copy most fields of the parent's RTE, but replace relation OID,
		 * and set inh = false.
		 */
		childrte = copyObject(rte);
		childrte->relid = childOID;
		childrte->inh = false;
		parse->rtable = lappend(parse->rtable, childrte);
		childRTindex = list_length(parse->rtable);

		child_relids = bms_add_member(child_relids, childRTindex);

		/*
		 * Build an AppendRelInfo for this parent and child.
		 */
		appinfo = makeNode(AppendRelInfo);
		appinfo->parent_relid = rti;
		appinfo->child_relid = childRTindex;
		appinfo->parent_reltype = oldrelation->rd_rel->reltype;
		appinfo->child_reltype = newrelation->rd_rel->reltype;
		make_inh_translation_lists(oldrelation, newrelation, childRTindex,
								   &appinfo->col_mappings,
								   &appinfo->translated_vars);
		appinfo->parent_reloid = parentOID;
		appinfos = lappend(appinfos, appinfo);

		/* Close child relations, but keep locks */
		if (childOID != parentOID)
			heap_close(newrelation, rel_needs_long_lock(childOID) ? NoLock: lockmode);
	}

	heap_close(oldrelation, NoLock);

	if (parent_is_partitioned)
	{
		DynamicScanInfo *dsinfo;

		dsinfo = palloc(sizeof(DynamicScanInfo));
		dsinfo->parentOid = parentOID;
		dsinfo->rtindex = rti;
		dsinfo->hasSelector = false;

		dsinfo->children = child_relids;

		dsinfo->partKeyAttnos = rel_partition_key_attrs(parentOID);

		root->dynamicScans = lappend(root->dynamicScans, dsinfo);
		dsinfo->dynamicScanId = list_length(root->dynamicScans);
	}

	/*
	 * If all the children were temp tables, pretend it's a non-inheritance
	 * situation.  The duplicate RTE we added for the parent table is
	 * harmless, so we don't bother to get rid of it.
	 */
	if (list_length(appinfos) < 1)
	{
		/* Clear flag before returning */
		rte->inh = false;
		return;
	}

	/* Otherwise, OK to add to root->append_rel_list */
	root->append_rel_list = list_concat(root->append_rel_list, appinfos);

	/*
	 * The executor will check the parent table's access permissions when it
	 * examines the parent's added RTE entry.  There's no need to check twice,
	 * so turn off access check bits in the original RTE.
	 */
	rte->requiredPerms = 0;
}

/*
 * make_inh_translation_lists
 *	  Build the lists of translations from parent Vars to child Vars for
 *	  an inheritance child.  We need both a column number mapping list
 *	  and a list of Vars representing the child columns.
 *
 * For paranoia's sake, we match type as well as attribute name.
 */
static void
make_inh_translation_lists(Relation oldrelation, Relation newrelation,
						   Index newvarno,
						   List **col_mappings, List **translated_vars)
{
	List	   *numbers = NIL;
	List	   *vars = NIL;
	TupleDesc	old_tupdesc = RelationGetDescr(oldrelation);
	TupleDesc	new_tupdesc = RelationGetDescr(newrelation);
	int			oldnatts = old_tupdesc->natts;
	int			newnatts = new_tupdesc->natts;
	int			old_attno;

	for (old_attno = 0; old_attno < oldnatts; old_attno++)
	{
		Form_pg_attribute att;
		char	   *attname;
		Oid			atttypid;
		int32		atttypmod;
		int			new_attno;

		att = old_tupdesc->attrs[old_attno];
		if (att->attisdropped)
		{
			/* Just put 0/NULL into this list entry */
			numbers = lappend_int(numbers, 0);
			vars = lappend(vars, NULL);
			continue;
		}
		attname = NameStr(att->attname);
		atttypid = att->atttypid;
		atttypmod = att->atttypmod;

		/*
		 * When we are generating the "translation list" for the parent table
		 * of an inheritance set, no need to search for matches.
		 */
		if (oldrelation == newrelation)
		{
			numbers = lappend_int(numbers, old_attno + 1);
			vars = lappend(vars, makeVar(newvarno,
										 (AttrNumber) (old_attno + 1),
										 atttypid,
										 atttypmod,
										 0));
			continue;
		}

		/*
		 * Otherwise we have to search for the matching column by name.
		 * There's no guarantee it'll have the same column position, because
		 * of cases like ALTER TABLE ADD COLUMN and multiple inheritance.
		 * However, in simple cases it will be the same column number, so try
		 * that before we go groveling through all the columns.
		 *
		 * Note: the test for (att = ...) != NULL cannot fail, it's just a
		 * notational device to include the assignment into the if-clause.
		 */
		if (old_attno < newnatts &&
			(att = new_tupdesc->attrs[old_attno]) != NULL &&
			!att->attisdropped && att->attinhcount != 0 &&
			strcmp(attname, NameStr(att->attname)) == 0)
			new_attno = old_attno;
		else
		{
			for (new_attno = 0; new_attno < newnatts; new_attno++)
			{
				att = new_tupdesc->attrs[new_attno];
				if (!att->attisdropped && att->attinhcount != 0 &&
					strcmp(attname, NameStr(att->attname)) == 0)
					break;
			}
			if (new_attno >= newnatts)
				elog(ERROR, "could not find inherited attribute \"%s\" of relation \"%s\"",
					 attname, RelationGetRelationName(newrelation));
		}

		/* Found it, check type */
		if (atttypid != att->atttypid || atttypmod != att->atttypmod)
			elog(ERROR, "attribute \"%s\" of relation \"%s\" does not match parent's type",
				 attname, RelationGetRelationName(newrelation));

		numbers = lappend_int(numbers, new_attno + 1);
		vars = lappend(vars, makeVar(newvarno,
									 (AttrNumber) (new_attno + 1),
									 atttypid,
									 atttypmod,
									 0));
	}

	*col_mappings = numbers;
	*translated_vars = vars;
}

/**
 * Struct to enable adjusting for partitioned tables.
 */
typedef struct AppendRelInfoContext
{
	plan_tree_base_prefix base;
	AppendRelInfo *appinfo;
} AppendRelInfoContext;

static Node *adjust_appendrel_attrs_mutator(Node *node, AppendRelInfoContext *ctx);

/*
 * adjust_appendrel_attrs
 *	  Copy the specified query or expression and translate Vars referring
 *	  to the parent rel of the specified AppendRelInfo to refer to the
 *	  child rel instead.  We also update rtindexes appearing outside Vars,
 *	  such as resultRelation and jointree relids.
 *
 * Note: this is only applied after conversion of sublinks to subplans,
 * so we don't need to cope with recursion into sub-queries.
 *
 * Note: this is not hugely different from what ResolveNew() does; maybe
 * we should try to fold the two routines together.
 */
Node *
adjust_appendrel_attrs(PlannerInfo *root, Node *node, AppendRelInfo *appinfo)
{
	Node	   *result;
	AppendRelInfoContext ctx;
	ctx.base.node = (Node *) root;
	ctx.appinfo = appinfo;

	/*
	 * Must be prepared to start with a Query or a bare expression tree.
	 */
	if (node && IsA(node, Query))
	{
		Query	   *newnode;

		newnode = query_tree_mutator((Query *) node,
									 adjust_appendrel_attrs_mutator,
									 (void *) &ctx,
									 QTW_IGNORE_RT_SUBQUERIES);
		if (newnode->resultRelation == appinfo->parent_relid)
		{
			newnode->resultRelation = appinfo->child_relid;
			/* Fix tlist resnos too, if it's inherited UPDATE */
			if (newnode->commandType == CMD_UPDATE)
				newnode->targetList =
					adjust_inherited_tlist(newnode->targetList,
										   appinfo);
		}
		result = (Node *) newnode;
	}
	else
		result = adjust_appendrel_attrs_mutator(node, &ctx);

	return result;
}

/**
 * Mutator's function is to modify nodes so that they may be applicable
 * for a child partition.
 */
static Node *
adjust_appendrel_attrs_mutator(Node *node, AppendRelInfoContext *ctx)
{
	Assert(ctx);
	AppendRelInfo *appinfo = ctx->appinfo;
	Assert(appinfo);

	if (node == NULL)
		return NULL;
	if (IsA(node, Var))
	{
		Var		   *var = (Var *) copyObject(node);

		if (var->varlevelsup == 0 &&
			var->varno == appinfo->parent_relid)
		{
			var->varno = appinfo->child_relid;
			var->varnoold = appinfo->child_relid;
			if (var->varattno > 0)
			{
				Node	   *newnode;

				if (var->varattno > list_length(appinfo->translated_vars))
					elog(ERROR, "attribute %d of relation \"%s\" does not exist",
						 var->varattno, get_rel_name(appinfo->parent_reloid));
				newnode = copyObject(list_nth(appinfo->translated_vars,
											  var->varattno - 1));
				if (newnode == NULL)
					elog(ERROR, "attribute %d of relation \"%s\" does not exist",
						 var->varattno, get_rel_name(appinfo->parent_reloid));
				return newnode;
			}
			else if (var->varattno == 0)
			{
				/*
				 * Whole-row Var: if we are dealing with named rowtypes, we
				 * can use a whole-row Var for the child table plus a coercion
				 * step to convert the tuple layout to the parent's rowtype.
				 * Otherwise we have to generate a RowExpr.
				 */
				if (OidIsValid(appinfo->child_reltype))
				{
					Assert(var->vartype == appinfo->parent_reltype);
					if (appinfo->parent_reltype != appinfo->child_reltype)
					{
						ConvertRowtypeExpr *r = makeNode(ConvertRowtypeExpr);

						r->arg = (Expr *) var;
						r->resulttype = appinfo->parent_reltype;
						r->convertformat = COERCE_IMPLICIT_CAST;
						r->location = -1;
						/* Make sure the Var node has the right type ID, too */
						var->vartype = appinfo->child_reltype;
						return (Node *) r;
					}
				}
				else
				{
					/*
					 * Build a RowExpr containing the translated variables.
					 */
					RowExpr    *rowexpr;
					List	   *fields;

					fields = (List *) copyObject(appinfo->translated_vars);
					rowexpr = makeNode(RowExpr);
					rowexpr->args = fields;
					rowexpr->row_typeid = var->vartype;
					rowexpr->row_format = COERCE_IMPLICIT_CAST;
					rowexpr->colnames = NIL;
					rowexpr->location = -1;
					
					return (Node *) rowexpr;
				}
			}
			/* system attributes don't need any other translation */
		}
		return (Node *) var;
	}
	if (IsA(node, CurrentOfExpr))
	{
		CurrentOfExpr *cexpr = (CurrentOfExpr *) copyObject(node);

		if (cexpr->cvarno == appinfo->parent_relid)
			cexpr->cvarno = appinfo->child_relid;
		return (Node *) cexpr;
	}
	if (IsA(node, RangeTblRef))
	{
		RangeTblRef *rtr = (RangeTblRef *) copyObject(node);

		if (rtr->rtindex == appinfo->parent_relid)
			rtr->rtindex = appinfo->child_relid;
		return (Node *) rtr;
	}
	if (IsA(node, JoinExpr))
	{
		/* Copy the JoinExpr node with correct mutation of subnodes */
		JoinExpr   *j;

		j = (JoinExpr *) expression_tree_mutator(node,
											  adjust_appendrel_attrs_mutator,
												 (void *) ctx);
		/* now fix JoinExpr's rtindex (probably never happens) */
		if (j->rtindex == appinfo->parent_relid)
			j->rtindex = appinfo->child_relid;
		return (Node *) j;
	}
	if (IsA(node, PlaceHolderVar))
	{
		/* Copy the PlaceHolderVar node with correct mutation of subnodes */
		PlaceHolderVar *phv;
		
		phv = (PlaceHolderVar *) expression_tree_mutator(node,
														 adjust_appendrel_attrs_mutator,
														 (void *) ctx);
		/* now fix PlaceHolderVar's relid sets */
		if (phv->phlevelsup == 0)
			phv->phrels = adjust_relid_set(phv->phrels,
										   appinfo->parent_relid,
										   appinfo->child_relid);
		return (Node *) phv;
	}

	/* Shouldn't need to handle planner auxiliary nodes here */
	Assert(!IsA(node, SpecialJoinInfo));
	Assert(!IsA(node, AppendRelInfo));
	Assert(!IsA(node, PlaceHolderInfo));

	/*
	 * We have to process RestrictInfo nodes specially.
	 */
	if (IsA(node, RestrictInfo))
	{
		RestrictInfo *oldinfo = (RestrictInfo *) node;
		RestrictInfo *newinfo = makeNode(RestrictInfo);

		/* Copy all flat-copiable fields */
		memcpy(newinfo, oldinfo, sizeof(RestrictInfo));

		/* Recursively fix the clause itself */
		newinfo->clause = (Expr *)
				adjust_appendrel_attrs_mutator((Node *) oldinfo->clause, ctx);

		/* and the modified version, if an OR clause */
		newinfo->orclause = (Expr *)
				adjust_appendrel_attrs_mutator((Node *) oldinfo->orclause, ctx);

		/* adjust relid sets too */
		newinfo->clause_relids = adjust_relid_set(oldinfo->clause_relids,
												  appinfo->parent_relid,
												  appinfo->child_relid);
		newinfo->required_relids = adjust_relid_set(oldinfo->required_relids,
													appinfo->parent_relid,
													appinfo->child_relid);
		newinfo->nullable_relids = adjust_relid_set(oldinfo->nullable_relids,
													appinfo->parent_relid,
													appinfo->child_relid);
		newinfo->left_relids = adjust_relid_set(oldinfo->left_relids,
												appinfo->parent_relid,
												appinfo->child_relid);
		newinfo->right_relids = adjust_relid_set(oldinfo->right_relids,
												 appinfo->parent_relid,
												 appinfo->child_relid);

		/*
		 * Reset cached derivative fields, since these might need to have
		 * different values when considering the child relation.
		 */
		newinfo->eval_cost.startup = -1;
		newinfo->this_selec = -1;
		newinfo->left_ec = NULL;
		newinfo->right_ec = NULL;
		newinfo->left_em = NULL;
		newinfo->right_em = NULL;
		newinfo->scansel_cache = NIL;
		newinfo->left_bucketsize = -1;
		newinfo->right_bucketsize = -1;

		return (Node *) newinfo;
	}

	/*
	 * NOTE: we do not need to recurse into sublinks, because they should
	 * already have been converted to subplans before we see them.
	 */
	Assert(!IsA(node, SubLink));
	Assert(!IsA(node, Query));

	node = expression_tree_mutator(node, adjust_appendrel_attrs_mutator,
								   (void *) ctx);

	/*
	 * In GPDB, if you have two SubPlans referring to the same initplan, we
	 * require two separate copies of the subplan, one for each SubPlan
	 * reference. That's because even if a plan is otherwise the same, we
	 * may want to later apply different flow to different SubPlans
	 * referring it. Any subplan that is left unused, because we created
	 * the new copy here, will be removed by remove_unused_subplans().
	 */
	if (IsA(node, SubPlan))
	{
		SubPlan *sp = (SubPlan *) node;

		if (!sp->is_initplan)
		{
			PlannerInfo *root = (PlannerInfo *) ctx->base.node;
			Plan *newsubplan = (Plan *) copyObject(planner_subplan_get_plan(root, sp));
			List *newrtable = (List *) copyObject(planner_subplan_get_rtable(root, sp));

			/*
			 * Add the subplan and its rtable to the global lists.
			 */
			root->glob->subplans = lappend(root->glob->subplans, newsubplan);
			root->glob->subrtables = lappend(root->glob->subrtables, newrtable);

			/*
			 * expression_tree_mutator made a copy of the SubPlan already, so
			 * we can modify it directly.
			 */
			sp->plan_id = list_length(root->glob->subplans);
		}
	}

	return node;
}

/*
 * Substitute newrelid for oldrelid in a Relid set
 */
static Relids
adjust_relid_set(Relids relids, Index oldrelid, Index newrelid)
{
	if (bms_is_member(oldrelid, relids))
	{
		/* Ensure we have a modifiable copy */
		relids = bms_copy(relids);
		/* Remove old, add new */
		relids = bms_del_member(relids, oldrelid);
		relids = bms_add_member(relids, newrelid);
	}
	return relids;
}

/*
 * Adjust the targetlist entries of an inherited UPDATE operation
 *
 * The expressions have already been fixed, but we have to make sure that
 * the target resnos match the child table (they may not, in the case of
 * a column that was added after-the-fact by ALTER TABLE).	In some cases
 * this can force us to re-order the tlist to preserve resno ordering.
 * (We do all this work in special cases so that preptlist.c is fast for
 * the typical case.)
 *
 * The given tlist has already been through expression_tree_mutator;
 * therefore the TargetEntry nodes are fresh copies that it's okay to
 * scribble on.
 *
 * Note that this is not needed for INSERT because INSERT isn't inheritable.
 */
static List *
adjust_inherited_tlist(List *tlist, AppendRelInfo *context)
{
	bool		changed_it = false;
	ListCell   *tl;
	List	   *new_tlist;
	bool		more;
	int			attrno;

	/* This should only happen for an inheritance case, not UNION ALL */
	Assert(OidIsValid(context->parent_reloid));

	/* Scan tlist and update resnos to match attnums of child rel */
	foreach(tl, tlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(tl);
		int			newattno;

		if (tle->resjunk)
			continue;			/* ignore junk items */

		/* Look up the translation of this column */
		if (tle->resno <= 0 ||
			tle->resno > list_length(context->col_mappings))
			elog(ERROR, "attribute %d of relation \"%s\" does not exist",
				 tle->resno, get_rel_name(context->parent_reloid));
		newattno = list_nth_int(context->col_mappings, tle->resno - 1);
		if (newattno <= 0)
			elog(ERROR, "attribute %d of relation \"%s\" does not exist",
				 tle->resno, get_rel_name(context->parent_reloid));

		if (tle->resno != newattno)
		{
			tle->resno = newattno;
			changed_it = true;
		}
	}

	/*
	 * If we changed anything, re-sort the tlist by resno, and make sure
	 * resjunk entries have resnos above the last real resno.  The sort
	 * algorithm is a bit stupid, but for such a seldom-taken path, small is
	 * probably better than fast.
	 */
	if (!changed_it)
		return tlist;

	new_tlist = NIL;
	more = true;
	for (attrno = 1; more; attrno++)
	{
		more = false;
		foreach(tl, tlist)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(tl);

			if (tle->resjunk)
				continue;		/* ignore junk items */

			if (tle->resno == attrno)
				new_tlist = lappend(new_tlist, tle);
			else if (tle->resno > attrno)
				more = true;
		}
	}

	foreach(tl, tlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(tl);

		if (!tle->resjunk)
			continue;			/* here, ignore non-junk items */

		tle->resno = attrno;
		new_tlist = lappend(new_tlist, tle);
		attrno++;
	}

	return new_tlist;
}
