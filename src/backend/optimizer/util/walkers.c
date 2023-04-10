/*
 * walkers.c
 *
 *  Created on: Feb 8, 2011
 *      Author: siva
 */

#include "postgres.h"

#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/walkers.h"

/**
 * Plan node walker related methods.
 */

/* Initialize a plan_tree_base_prefix during planning. */
void planner_init_plan_tree_base(plan_tree_base_prefix *base, PlannerInfo *root)
{
	base->node = (Node*)root;
}

/* Initialize a plan_tree_base_prefix after planning. */
void exec_init_plan_tree_base(plan_tree_base_prefix *base, PlannedStmt *stmt)
{
	base->node = (Node*)stmt;
}

static bool walk_scan_node_fields(Scan *scan, bool (*walker) (), void *context);
static bool walk_join_node_fields(Join *join, bool (*walker) (), void *context);


/* ----------------------------------------------------------------------- *
 * Plan Tree Walker Framework
 * ----------------------------------------------------------------------- *
 */

/* Function walk_plan_node_fields is a subroutine used by plan_tree_walker()
 * to walk the fields of Plan nodes.  Plan is actually an abstract superclass
 * of all plan nodes and this function encapsulates the common structure.
 *
 * Most specific walkers won't need to call this function, but complicated
 * ones may find it a useful utility.
 *
 * Caution: walk_scan_node_fields and walk_join_node_fields call this
 * function.  Use only the most specific function.
 */
bool
walk_plan_node_fields(Plan *plan,
					  bool (*walker) (),
					  void *context)
{
	/* target list to be computed at this node */
	if (walker((Node *) (plan->targetlist), context))
		return true;

	/* implicitly ANDed qual conditions */
	if (walker((Node *) (plan->qual), context))
		return true;

	/* input plan tree(s) */
	if (walker((Node *) (plan->lefttree), context))
		return true;

	/* target list to be computed at this node */
	if (walker((Node *) (plan->righttree), context))
		return true;

	/* Init Plan nodes (uncorrelated expr subselects */
	if (walker((Node *) (plan->initPlan), context))
		return true;

	/* Greenplum Database Flow node */
	if (walker((Node *) (plan->flow), context))
		return true;

	return false;
}


/* Function walk_scan_node_fields is a subroutine used by plan_tree_walker()
 * to walk the fields of Scan nodes.  Scan is actually an abstract superclass
 * of all scan nodes and a subclass of Plan.  This function encapsulates the
 * common structure.
 *
 * Most specific walkers won't need to call this function, but complicated
 * ones may find it a useful utility.
 *
 * Caution: This function calls walk_plan_node_fields so callers shouldn't,
 * else they will walk common plan fields twice.
 */
bool
walk_scan_node_fields(Scan *scan,
					  bool (*walker) (),
					  void *context)
{
	/* A Scan node is a kind of Plan node. */
	if (walk_plan_node_fields((Plan *) scan, walker, context))
		return true;

	/* The only additional field is an Index so no extra walking. */
	return false;
}


/* Function walk_join_node_fields is a subroutine used by plan_tree_walker()
 * to walk the fields of Join nodes.  Join is actually an abstract superclass
 * of all join nodes and a subclass of Plan.  This function encapsulates the
 * common structure.
 *
 * Most specific walkers won't need to call this function, but complicated
 * ones may find it a useful utility.
 *
 * Caution: This function calls walk_plan_node_fields so callers shouldn't,
 * else they will walk common plan fields twice.
 */
bool
walk_join_node_fields(Join *join,
					  bool (*walker) (),
					  void *context)
{
	/* A Join node is a kind of Plan node. */
	if (walk_plan_node_fields((Plan *) join, walker, context))
		return true;

	return walker((Node *) (join->joinqual), context);
}


/* Function plan_tree_walker is a general walker for Plan trees.
 *
 * It is based on (and uses) the expression_tree_walker() framework from
 * src/backend/optimizer/util/clauses.c.  See the comments there for more
 * insight into how this function works.
 *
 * The basic idea is that this function (and its helpers) walk plan-specific
 * nodes and delegate other nodes to expression_tree_walker().	The caller
 * may supply a specialized walker
 */
bool
plan_tree_walker(Node *node,
				 bool (*walker) (),
				 void *context,
				 bool recurse_into_subplans)
{
	/*
	 * The walker has already visited the current node, and so we need
	 * only recurse into any sub-nodes it has.
	 *
	 * We assume that the walker is not interested in List nodes per se, so
	 * when we expect a List we just recurse directly to self without
	 * bothering to call the walker.
	 */
	if (node == NULL)
		return false;

	/* Guard against stack overflow due to overly complex expressions */
	check_stack_depth();

	switch (nodeTag(node))
	{
			/*
			 * Plan nodes aren't handled by expression_tree_walker, so we need
			 * to do them here.
			 */
		case T_Plan:
			/* Abstract: really should see only subclasses. */
			return walk_plan_node_fields((Plan *) node, walker, context);

		case T_Result:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			if (walker((Node *) ((Result *) node)->resconstantqual, context))
				return true;
			break;

		case T_ProjectSet:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			break;

		case T_PartitionSelector:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			break;

		case T_Append:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			if (walker((Node *) ((Append *) node)->appendplans, context))
				return true;
			break;

		case T_MergeAppend:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			if (walker((Node *) ((MergeAppend *) node)->mergeplans, context))
				return true;
			break;

		case T_RecursiveUnion:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			break;

		case T_Sequence:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			if (walker((Node *) ((Sequence *) node)->subplans, context))
				return true;
			break;

		case T_BitmapAnd:
            if (walk_plan_node_fields((Plan *) node, walker, context))
                return true;
            if (walker((Node *) ((BitmapAnd *) node)->bitmapplans, context))
                return true;
            break;
        case T_BitmapOr:
            if (walk_plan_node_fields((Plan *) node, walker, context))
                return true;
            if (walker((Node *) ((BitmapOr *) node)->bitmapplans, context))
                return true;
            break;

		case T_Scan:
			/* Abstract: really should see only subclasses. */
			return walk_scan_node_fields((Scan *) node, walker, context);

		case T_SeqScan:
		case T_SampleScan:
		case T_DynamicSeqScan:
		case T_BitmapHeapScan:
		case T_DynamicBitmapHeapScan:
		case T_WorkTableScan:
		case T_NamedTuplestoreScan:
			if (walk_scan_node_fields((Scan *) node, walker, context))
				return true;
			break;

		case T_ForeignScan:
		case T_DynamicForeignScan:
			if (walk_scan_node_fields((Scan *) node, walker, context))
				return true;
			if (walker(((ForeignScan *) node)->fdw_exprs, context))
				return true;
			break;

		case T_CustomScan:
			if (walk_scan_node_fields((Scan *) node, walker, context))
				return true;
			if (walker(((CustomScan *) node)->custom_plans, context))
				return true;
			if (walker(((CustomScan *) node)->custom_exprs, context))
				return true;
			if (walker(((CustomScan *) node)->custom_private, context))
				return true;
			if (walker(((CustomScan *) node)->custom_scan_tlist, context))
				return true;
			break;

		case T_ValuesScan:
			if (walker((Node *) ((ValuesScan *) node)->values_lists, context))
				return true;
			if (walk_scan_node_fields((Scan *) node, walker, context))
				return true;
			break;

		case T_TableFuncScan:
			if (walker((Node *) ((TableFuncScan *) node)->tablefunc, context))
				return true;
			if (walk_scan_node_fields((Scan *) node, walker, context))
				return true;
			break;
			
		case T_TableFunctionScan:
			if (walker((Node *) ((TableFunctionScan *) node)->function, context))
				return true;
			if (walk_scan_node_fields((Scan *) node, walker, context))
				return true;
			break;

		case T_FunctionScan:
			if (walker((Node *) ((FunctionScan *) node)->functions, context))
				return true;
			if (walker((Node *) ((FunctionScan *) node)->param, context))
				return true;
			if (walk_scan_node_fields((Scan *) node, walker, context))
				return true;
			break;

		case T_IndexScan:
		case T_DynamicIndexScan:
			if (walk_scan_node_fields((Scan *) node, walker, context))
				return true;
			if (walker((Node *) ((IndexScan *) node)->indexqual, context))
				return true;
			/* Other fields are lists of basic items, nothing to walk. */
			break;

		case T_IndexOnlyScan:
			if (walk_scan_node_fields((Scan *) node, walker, context))
				return true;
			if (walker((Node *) ((IndexOnlyScan *) node)->indexqual, context))
				return true;
			break;

		case T_BitmapIndexScan:
		case T_DynamicBitmapIndexScan:
			if (walk_scan_node_fields((Scan *) node, walker, context))
				return true;
			if (walker((Node *) ((BitmapIndexScan *) node)->indexqual, context))
				return true;
			/* Other fields are lists of basic items, nothing to walk. */
			break;

		case T_TidScan:
			if (walk_scan_node_fields((Scan *) node, walker, context))
				return true;
			if (walker((Node *) ((TidScan *) node)->tidquals, context))
				return true;
			break;

		case T_SubqueryScan:
			if (walk_scan_node_fields((Scan *) node, walker, context))
				return true;
			if (walker((Node *) ((SubqueryScan *) node)->subplan, context))
				return true;
			break;

		case T_Join:
			/* Abstract: really should see only subclasses. */
			return walk_join_node_fields((Join *) node, walker, context);

		case T_NestLoop:
			if (walk_join_node_fields((Join *) node, walker, context))
				return true;
			break;

		case T_MergeJoin:
			if (walk_join_node_fields((Join *) node, walker, context))
				return true;
			if (walker((Node *) ((MergeJoin *) node)->mergeclauses, context))
				return true;
			break;

		case T_HashJoin:
			if (walk_join_node_fields((Join *) node, walker, context))
				return true;
			if (walker((Node *) ((HashJoin *) node)->hashclauses, context))
				return true;
			if (walker((Node *) ((HashJoin *) node)->hashqualclauses, context))
				return true;
			break;

		case T_Material:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			break;

		case T_Sort:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			/* Other fields are simple counts and lists of indexes and oids. */
			break;

		case T_Agg:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			/* Other fields are simple items and lists of simple items. */
			break;

		case T_TupleSplit:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			if (expression_tree_walker((Node *)((TupleSplit *)node)->dqa_expr_lst, walker, context))
				return true;
			/* Other fields are simple items and lists of simple items. */
			break;

		case T_DQAExpr:
			if (walker(((DQAExpr *)node)->agg_filter, context))
				return true;
			/* Other fields are simple items and lists of simple items. */
			break;

		case T_WindowAgg:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			if (walker(((WindowAgg *) node)->startOffset, context))
				return true;
			if (walker(((WindowAgg *) node)->endOffset, context))
				return true;
			break;

		case T_Unique:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			/* Other fields are simple items and lists of simple items. */
			break;

		case T_Gather:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			/* Other fields are simple items. */
			break;

		case T_Hash:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			/* Other info is in parent HashJoin node. */
			break;

		case T_SetOp:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			/* Other fields are simple items and lists of simple items. */
			break;

		case T_Limit:

			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;

			/* Greenplum Database Limit Count */
			if (walker((Node *) (((Limit*) node)->limitCount), context))
					return true;

			/* Greenplum Database Limit Offset */
			if (walker((Node *) (((Limit*) node)->limitOffset), context))
					return true;

			/* Other fields are simple items and lists of simple items. */
			break;

		case T_Motion:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;

			if (walker((Node *) ((Motion *)node)->hashExprs, context))
				return true;

			break;

		case T_ShareInputScan:
		case T_CteScan:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			break;

		case T_Flow:
			/* Nothing to walk in Flow nodes at present. */
			break;

		case T_SubPlan:

			/*
			 * SubPlan is handled by expression_tree_walker() , but that
			 * walker doesn't recur into the plan itself.  Here, we do so
			 * by using information in the prefix structure defined in
			 * cdbplan.h.
			 *
			 * NB Callers usually don't have to take special account of
			 *    the range table, but should be sure to understand what
			 *    stage of processing they occur in (e.g., early planning,
			 *    late planning, dispatch, or execution) since this affects
			 *    what value are available.
			 */
			{
				SubPlan    *subplan = (SubPlan *) node;
				Plan	   *subplan_plan = plan_tree_base_subplan_get_plan(context, subplan);

				Assert(subplan_plan);

				/* recurse into the exprs list */
				if (expression_tree_walker((Node *) subplan->testexpr,
										   walker, context))
					return true;

				/* recurse into the subplan's plan, a kind of Plan node */
				if (recurse_into_subplans && walker((Node *) subplan_plan, context))
					return true;

				/* also examine args list */
				if (expression_tree_walker((Node *) subplan->args,
										   walker, context))
					return true;
			}
			break;

		case T_IntList:
		case T_OidList:

			/*
			 * Note that expression_tree_walker handles T_List but not these.
			 * No contained stuff, so just succeed.
			 */
			break;

		case T_ModifyTable:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			if (walker((Node *) ((ModifyTable *) node)->plans, context))
				return true;
			if (walker((Node *) ((ModifyTable *) node)->withCheckOptionLists, context))
				return true;
			if (walker((Node *) ((ModifyTable *) node)->onConflictSet, context))
				return true;
			if (walker((Node *) ((ModifyTable *) node)->onConflictWhere, context))
				return true;
			if (walker((Node *) ((ModifyTable *) node)->returningLists, context))
				return true;

			break;

		case T_LockRows:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			break;

		case T_SplitUpdate:
		case T_AssertOp:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			break;

			/*
			 * Query nodes are handled by the Postgres query_tree_walker. We
			 * just use it without setting any ignore flags.  The caller must
			 * handle query nodes specially to get other behavior, e.g. by
			 * calling query_tree_walker directly.
			 */
		case T_Query:
			return query_tree_walker((Query *) node, walker, context, 0);

			/*
			 * The following cases are handled by expression_tree_walker.  In
			 * addition, we let expression_tree_walker handle unrecognized
			 * nodes.
			 *
			 * TODO: Identify node types that should never appear in plan trees
			 * and disallow them here by issuing an error or asserting false.
			 */
		case T_Var:
		case T_Const:
		case T_Param:
		case T_CoerceToDomainValue:
		case T_CaseTestExpr:
		case T_SetToDefault:
		case T_CurrentOfExpr:
		case T_RangeTblRef:
		case T_Aggref:
		case T_SubscriptingRef:
		case T_FuncExpr:
		case T_OpExpr:
		case T_DistinctExpr:
		case T_ScalarArrayOpExpr:
		case T_BoolExpr:
		case T_SubLink:
		case T_FieldSelect:
		case T_FieldStore:
		case T_RelabelType:
		case T_CaseExpr:
		case T_ArrayExpr:
		case T_RowExpr:
		case T_CoalesceExpr:
		case T_NullIfExpr:
		case T_NullTest:
		case T_BooleanTest:
		case T_CoerceToDomain:
		case T_TargetEntry:
		case T_List:
		case T_FromExpr:
		case T_JoinExpr:
		case T_SetOperationStmt:
		case T_SpecialJoinInfo:
		case T_TableValueExpr:

		default:
			return expression_tree_walker(node, walker, context);
	}
	return false;
}

/* Return the plan associated with a SubPlan node in a walker.  (This is used by
 * framework, not by users of the framework.)
 */
Plan *plan_tree_base_subplan_get_plan(plan_tree_base_prefix *base, SubPlan *subplan)
{
	if (!base)
		return NULL;
	else if (IsA(base->node, PlannedStmt))
		return exec_subplan_get_plan((PlannedStmt*)base->node, subplan);
	else if (IsA(base->node, PlannerInfo))
		return planner_subplan_get_plan((PlannerInfo*)base->node, subplan);
	else if (IsA(base->node, PlannerGlobal))
	{
		PlannerInfo rootdata;
		rootdata.glob = (PlannerGlobal*)base->node;
		return planner_subplan_get_plan(&rootdata, subplan);
	}
	Assert(false);
	return NULL;
}

/* Rewrite the plan associated with a SubPlan node in a mutator.  (This is used by
 * framework, not by users of the framework.)
 */
void plan_tree_base_subplan_put_plan(plan_tree_base_prefix *base, SubPlan *subplan, Plan *plan)
{
	Assert(base);
	if (IsA(base->node, PlannedStmt))
	{
		exec_subplan_put_plan((PlannedStmt*)base->node, subplan, plan);
		return;
	}
	else if (IsA(base->node, PlannerInfo))
	{
		planner_subplan_put_plan((PlannerInfo*)base->node, subplan, plan);
		return;
	}
	Assert(false && "Must provide relevant base info.");
}

/**
 * These are helpers to retrieve nodes from plans.
 */
typedef struct extract_context
{
	plan_tree_base_prefix base; /* Required prefix for plan_tree_walker/mutator */
 	bool descendIntoSubqueries;
	NodeTag  nodeTag;
	List *nodes;
} extract_context;

static bool extract_nodes_walker(Node *node, extract_context *context);
static bool extract_nodes_expression_walker(Node *node, extract_context *context);

/**
 * Extract nodes with specific tag.
 */
List *extract_nodes(PlannerGlobal *glob, Node *node, int nodeTag)
{
	extract_context context;
	context.base.node = (Node *) glob;
	context.nodes = NULL;
	context.nodeTag = nodeTag;
	context.descendIntoSubqueries = false;
	extract_nodes_walker(node, &context);
	return context.nodes;
}

/**
 * Extract nodes with specific tag.
 * Same as above, but starts off a Plan node rather than a PlannedStmt
 *
 */
List *extract_nodes_plan(Plan *pl, int nodeTag, bool descendIntoSubqueries)
{
	extract_context context;
	Assert(pl);
	context.base.node = NULL;
	context.nodes = NULL;
	context.nodeTag = nodeTag;
	context.descendIntoSubqueries = descendIntoSubqueries;
	extract_nodes_walker((Node *)pl, &context);
	return context.nodes;
}

static bool
extract_nodes_walker(Node *node, extract_context *context)
{
	if (node == NULL)
		return false;
	if (nodeTag(node) == context->nodeTag)
	{
		context->nodes = lappend(context->nodes, node);
	}
	if (nodeTag(node) == T_SubPlan)
	{
		SubPlan	   *subplan = (SubPlan *) node;

		/*
		 * SubPlan has both of expressions and subquery.  In case the caller wants
		 * non-subquery version, still we need to walk through its expressions.
		 * NB: Since we're not going to descend into SUBPLANs anyway (see below),
		 * look at the SUBPLAN node here, even if descendIntoSubqueries is false
		 * lest we miss some nodes there.
		 */
		if (extract_nodes_walker((Node *) subplan->testexpr,
								 context))
			return true;
		if (expression_tree_walker((Node *) subplan->args,
								   extract_nodes_walker, context))
			return true;

		/*
		 * Do not descend into subplans.
		 * Even if descendIntoSubqueries indicates the caller wants to descend into
		 * subqueries, SubPlan seems special; Some partitioning code assumes this
		 * should return immediately without descending.  See MPP-17168.
		 */
		return false;
	}
	if (nodeTag(node) == T_SubqueryScan
		&& !context->descendIntoSubqueries)
	{
		/* Do not descend into subquery scans. */
		return false;
	}
	if (nodeTag(node) == T_TableFunctionScan
		&& !context->descendIntoSubqueries)
	{
		/* Do not descend into table function subqueries */
		return false;
	}

	return plan_tree_walker(node, extract_nodes_walker,
							(void *) context,
							true);
}

/**
 * Extract nodes with specific tag.
 * Same as above, but starts off a scalar expression node rather than a PlannedStmt
 *
 */
List *extract_nodes_expression(Node *node, int nodeTag, bool descendIntoSubqueries)
{
	extract_context context;
	Assert(node);
	context.base.node = NULL;
	context.nodes = NULL;
	context.nodeTag = nodeTag;
	context.descendIntoSubqueries = descendIntoSubqueries;
	extract_nodes_expression_walker(node, &context);
	
	return context.nodes;
}

static bool
extract_nodes_expression_walker(Node *node, extract_context *context)
{
	if (NULL == node)
	{
		return false;
	}
	
	if (nodeTag(node) == context->nodeTag)
	{
		context->nodes = lappend(context->nodes, node);
	}
	
	if (nodeTag(node) == T_Query && context->descendIntoSubqueries)
	{
		Query *query = (Query *) node;
		if (expression_tree_walker((Node *)query->targetList, extract_nodes_expression_walker, (void *) context))
		{
			return true;
		}

		if (query->jointree != NULL &&
		   expression_tree_walker(query->jointree->quals, extract_nodes_expression_walker, (void *) context))
		{
			return true;
		}

		return expression_tree_walker(query->havingQual, extract_nodes_expression_walker, (void *) context);
	}

	return expression_tree_walker(node, extract_nodes_expression_walker, (void *) context);
}

/**
 * These are helpers to find node in queries
 */
typedef struct find_nodes_context
{
	List *nodeTags;
	int foundNode;
} find_nodes_context;

static bool find_nodes_walker(Node *node, find_nodes_context *context);

/**
 * Looks for nodes that belong to the given list.
 * Returns the index of the first such node that it encounters, or -1 if none
 */
int find_nodes(Node *node, List *nodeTags)
{
	find_nodes_context context;
	Assert(NULL != node);
	context.nodeTags = nodeTags;
	context.foundNode = -1;
	find_nodes_walker(node, &context);

	return context.foundNode;
}

static bool
find_nodes_walker(Node *node, find_nodes_context *context)
{
	if (NULL == node)
	{
		return false;
	}

	if (IsA(node, Query))
	{
		/* Recurse into subselects */
		return query_tree_walker((Query *) node, find_nodes_walker, (void *) context, 0 /* flags */);
	}

	ListCell *lc;
	int i = 0;
	foreach(lc, context->nodeTags)
	{
		NodeTag nodeTag = (NodeTag) lfirst_int(lc);
		if (nodeTag(node) == nodeTag)
		{
			context->foundNode = i;
			return true;
		}

		i++;
	}

	return expression_tree_walker(node, find_nodes_walker, (void *) context);
}

/**
 * GPDB_91_MERGE_FIXME: collation
 * Look for nodes with non-default collation; return 1 if any exist, -1
 * otherwise.
 */
typedef struct check_collation_context
{
	int foundNonDefaultCollation;
} check_collation_context;

static bool check_collation_walker(Node *node, check_collation_context *context);

int check_collation(Node *node)
{
	check_collation_context context;
	Assert(NULL != node);
	context.foundNonDefaultCollation = -1;
	check_collation_walker(node, &context);

	return context.foundNonDefaultCollation;
}


static void
check_collation_in_list(List *colllist, check_collation_context *context)
{
	ListCell *lc;
	foreach (lc, colllist)
	{
		Oid coll = lfirst_oid(lc);
		if (InvalidOid != coll && DEFAULT_COLLATION_OID != coll)
		{
			context->foundNonDefaultCollation = 1;
			break;
		}
	}
}

static bool
check_collation_walker(Node *node, check_collation_context *context)
{
	Oid collation, inputCollation, type;

	if (NULL == node)
	{
		return false;
	}

	if (IsA(node, Query))
	{
		/* Recurse into subselects */
		return query_tree_walker((Query *) node, check_collation_walker, (void *) context, 0 /* flags */);
	}

	switch (nodeTag(node))
	{
		case T_Var:
		case T_Const:
		case T_OpExpr:
			type = exprType((node));
			collation = exprCollation(node);
			if (type == NAMEOID || type == NAMEARRAYOID)
			{
				if (collation != C_COLLATION_OID)
					context->foundNonDefaultCollation = 1;
			}
			else if (InvalidOid != collation && DEFAULT_COLLATION_OID != collation)
			{
				context->foundNonDefaultCollation = 1;
			}
			break;
		case T_ScalarArrayOpExpr:
		case T_DistinctExpr:
		case T_BoolExpr:
		case T_BooleanTest:
		case T_CaseExpr:
		case T_CaseTestExpr:
		case T_CoalesceExpr:
		case T_MinMaxExpr:
		case T_FuncExpr:
		case T_Aggref:
		case T_WindowFunc:
		case T_NullTest:
		case T_NullIfExpr:
		case T_RelabelType:
		case T_CoerceToDomain:
		case T_CoerceViaIO:
		case T_ArrayCoerceExpr:
		case T_SubLink:
		case T_ArrayExpr:
		case T_SubscriptingRef:
		case T_RowExpr:
		case T_RowCompareExpr:
		case T_FieldSelect:
		case T_FieldStore:
		case T_CoerceToDomainValue:
		case T_CurrentOfExpr:
		case T_NamedArgExpr:
		case T_ConvertRowtypeExpr:
		case T_CollateExpr:
		case T_TableValueExpr:
		case T_XmlExpr:
		case T_SetToDefault:
		case T_PlaceHolderVar:
		case T_Param:
		case T_SubPlan:
		case T_AlternativeSubPlan:
		case T_GroupingFunc:
		case T_DMLActionExpr:
			collation = exprCollation(node);
			inputCollation = exprInputCollation(node);
			if ((InvalidOid != collation && DEFAULT_COLLATION_OID != collation) ||
				(InvalidOid != inputCollation && DEFAULT_COLLATION_OID != inputCollation))
			{
				context->foundNonDefaultCollation = 1;
			}
			break;
		case T_CollateClause:
			/* unsupported */
			context->foundNonDefaultCollation = 1;
			break;
		case T_RangeTblEntry:
			check_collation_in_list(((RangeTblEntry *) node)->colcollations, context);
			break;
		case T_RangeTblFunction:
			check_collation_in_list(((RangeTblFunction *) node)->funccolcollations, context);
			break;
		case T_CommonTableExpr:
			check_collation_in_list(((CommonTableExpr *) node)->ctecolcollations, context);
			break;
		case T_SetOperationStmt:
			check_collation_in_list(((SetOperationStmt *) node)->colCollations, context);
			break;
		default:
			/* make compiler happy */
			break;
	}

	if (context->foundNonDefaultCollation == 1)
	{
		/* end recursion */
		return true;
	}
	else
	{
		return expression_tree_walker(node, check_collation_walker, (void *) context);
	}
}

