/*
 * walkers.c
 *
 *  Created on: Feb 8, 2011
 *      Author: siva
 */

#include "postgres.h"

#include "miscadmin.h"
#include "optimizer/walkers.h"
#include "optimizer/var.h"

/*
 * Standard expression-tree walking support
 *
 * We used to have near-duplicate code in many different routines that
 * understood how to recurse through an expression node tree.  That was
 * a pain to maintain, and we frequently had bugs due to some particular
 * routine neglecting to support a particular node type.  In most cases,
 * these routines only actually care about certain node types, and don't
 * care about other types except insofar as they have to recurse through
 * non-primitive node types.  Therefore, we now provide generic tree-walking
 * logic to consolidate the redundant "boilerplate" code.  There are
 * two versions: expression_tree_walker() and expression_tree_mutator().
 */

/*--------------------
 * expression_tree_walker() is designed to support routines that traverse
 * a tree in a read-only fashion (although it will also work for routines
 * that modify nodes in-place but never add/delete/replace nodes).
 * A walker routine should look like this:
 *
 * bool my_walker (Node *node, my_struct *context)
 * {
 *		if (node == NULL)
 *			return false;
 *		// check for nodes that special work is required for, eg:
 *		if (IsA(node, Var))
 *		{
 *			... do special actions for Var nodes
 *		}
 *		else if (IsA(node, ...))
 *		{
 *			... do special actions for other node types
 *		}
 *		// for any node type not specially processed, do:
 *		return expression_tree_walker(node, my_walker, (void *) context);
 * }
 *
 * The "context" argument points to a struct that holds whatever context
 * information the walker routine needs --- it can be used to return data
 * gathered by the walker, too.  This argument is not touched by
 * expression_tree_walker, but it is passed down to recursive sub-invocations
 * of my_walker.  The tree walk is started from a setup routine that
 * fills in the appropriate context struct, calls my_walker with the top-level
 * node of the tree, and then examines the results.
 *
 * The walker routine should return "false" to continue the tree walk, or
 * "true" to abort the walk and immediately return "true" to the top-level
 * caller.	This can be used to short-circuit the traversal if the walker
 * has found what it came for.	"false" is returned to the top-level caller
 * iff no invocation of the walker returned "true".
 *
 * The node types handled by expression_tree_walker include all those
 * normally found in target lists and qualifier clauses during the planning
 * stage.  In particular, it handles List nodes since a cnf-ified qual clause
 * will have List structure at the top level, and it handles TargetEntry nodes
 * so that a scan of a target list can be handled without additional code.
 * Also, RangeTblRef, FromExpr, JoinExpr, and SetOperationStmt nodes are
 * handled, so that query jointrees and setOperation trees can be processed
 * without additional code.
 *
 * expression_tree_walker will handle SubLink nodes by recursing normally
 * into the "testexpr" subtree (which is an expression belonging to the outer
 * plan).  It will also call the walker on the sub-Query node; however, when
 * expression_tree_walker itself is called on a Query node, it does nothing
 * and returns "false".  The net effect is that unless the walker does
 * something special at a Query node, sub-selects will not be visited during
 * an expression tree walk. This is exactly the behavior wanted in many cases
 * --- and for those walkers that do want to recurse into sub-selects, special
 * behavior is typically needed anyway at the entry to a sub-select (such as
 * incrementing a depth counter). A walker that wants to examine sub-selects
 * should include code along the lines of:
 *
 *		if (IsA(node, Query))
 *		{
 *			adjust context for subquery;
 *			result = query_tree_walker((Query *) node, my_walker, context,
 *									   0); // adjust flags as needed
 *			restore context if needed;
 *			return result;
 *		}
 *
 * query_tree_walker is a convenience routine (see below) that calls the
 * walker on all the expression subtrees of the given Query node.
 *
 * expression_tree_walker will handle SubPlan nodes by recursing normally
 * into the "testexpr" and the "args" list (which are expressions belonging to
 * the outer plan).  It will not touch the completed subplan, however.	Since
 * there is no link to the original Query, it is not possible to recurse into
 * subselects of an already-planned expression tree.  This is OK for current
 * uses, but may need to be revisited in future.
 *--------------------
 */

bool
expression_tree_walker(Node *node,
					   bool (*walker) (),
					   void *context)
{
	ListCell   *temp = NULL;

	/*
	 * The walker has already visited the current node, and so we need only
	 * recurse into any sub-nodes it has.
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
		case T_Var:
		case T_A_Const:
		case T_Const:
		case T_Param:
		case T_CoerceToDomainValue:
		case T_CaseTestExpr:
		case T_CurrentOfExpr:
		case T_SetToDefault:
		case T_RangeTblRef:
		case T_DMLActionExpr:
		case T_PartOidExpr:
		case T_PartSelectedExpr:
		case T_PartDefaultExpr:
		case T_PartBoundExpr:
		case T_PartBoundInclusionExpr:
		case T_PartBoundOpenExpr:
		case T_PartListRuleExpr:
		case T_PartListNullTestExpr:
			/* primitive node types with no expression subnodes */
			break;
		case T_Aggref:
			{
				Aggref	   *expr = (Aggref *) node;

				/* recurse directly on List */
				if (expression_tree_walker((Node *) expr->args,
										   walker, context))
					return true;
				if (expression_tree_walker((Node *) expr->aggorder,
										   walker, context))
					return true;
			}
			break;
		case T_AggOrder:
			{
				AggOrder	   *expr = (AggOrder *) node;

				/* recurse directly on List */
				if (expression_tree_walker((Node *) expr->sortTargets,
										   walker, context))
					return true;
				if (expression_tree_walker((Node *) expr->sortClause,
										   walker, context))
					return true;
			}
			break;
		case T_WindowRef:
			{
				WindowRef	   *expr = (WindowRef *) node;

				/* recurse directly on explicit arg List */
				if (expression_tree_walker((Node *) expr->args,
										   walker, context))
					return true;
				/* don't recurse on implicit args under winspec */
			}
			break;
		case T_ArrayRef:
			{
				ArrayRef   *aref = (ArrayRef *) node;

				/* recurse directly for upper/lower array index lists */
				if (expression_tree_walker((Node *) aref->refupperindexpr,
										   walker, context))
					return true;
				if (expression_tree_walker((Node *) aref->reflowerindexpr,
										   walker, context))
					return true;
				/* walker must see the refexpr and refassgnexpr, however */
				if (walker(aref->refexpr, context))
					return true;
				if (walker(aref->refassgnexpr, context))
					return true;
			}
			break;
		case T_FuncExpr:
			{
				FuncExpr   *expr = (FuncExpr *) node;

				if (expression_tree_walker((Node *) expr->args,
										   walker, context))
					return true;
			}
			break;
		case T_OpExpr:
			{
				OpExpr	   *expr = (OpExpr *) node;

				if (expression_tree_walker((Node *) expr->args,
										   walker, context))
					return true;
			}
			break;
		case T_DistinctExpr:
			{
				DistinctExpr *expr = (DistinctExpr *) node;

				if (expression_tree_walker((Node *) expr->args,
										   walker, context))
					return true;
			}
			break;
		case T_ScalarArrayOpExpr:
			{
				ScalarArrayOpExpr *expr = (ScalarArrayOpExpr *) node;

				if (expression_tree_walker((Node *) expr->args,
										   walker, context))
					return true;
			}
			break;
		case T_BoolExpr:
			{
				BoolExpr   *expr = (BoolExpr *) node;

				if (expression_tree_walker((Node *) expr->args,
										   walker, context))
					return true;
			}
			break;
		case T_SubLink:
			{
				SubLink    *sublink = (SubLink *) node;

				if (walker(sublink->testexpr, context))
					return true;

				/*
				 * Also invoke the walker on the sublink's Query node, so it
				 * can recurse into the sub-query if it wants to.
				 */
				return walker(sublink->subselect, context);
			}
			break;
		case T_SubPlan:
			{
				SubPlan    *subplan = (SubPlan *) node;

				/* recurse into the testexpr, but not into the Plan */
				if (walker(subplan->testexpr, context))
					return true;
				/* also examine args list */
				if (expression_tree_walker((Node *) subplan->args,
										   walker, context))
					return true;
			}
			break;
		case T_FieldSelect:
			return walker(((FieldSelect *) node)->arg, context);
		case T_FieldStore:
			{
				FieldStore *fstore = (FieldStore *) node;

				if (walker(fstore->arg, context))
					return true;
				if (walker(fstore->newvals, context))
					return true;
			}
			break;
		case T_RelabelType:
			return walker(((RelabelType *) node)->arg, context);
		case T_CoerceViaIO:
			return walker(((CoerceViaIO *) node)->arg, context);
		case T_ArrayCoerceExpr:
			return walker(((ArrayCoerceExpr *) node)->arg, context);
		case T_ConvertRowtypeExpr:
			return walker(((ConvertRowtypeExpr *) node)->arg, context);
		case T_CaseExpr:
			{
				CaseExpr   *caseexpr = (CaseExpr *) node;

				if (walker(caseexpr->arg, context))
					return true;
				/* we assume walker doesn't care about CaseWhens, either */
				foreach(temp, caseexpr->args)
				{
					CaseWhen   *when = (CaseWhen *) lfirst(temp);

					Assert(IsA(when, CaseWhen));
					if (walker(when->expr, context))
						return true;
					if (walker(when->result, context))
						return true;
				}
				if (walker(caseexpr->defresult, context))
					return true;
			}
			break;
		case T_ArrayExpr:
			return walker(((ArrayExpr *) node)->elements, context);
		case T_RowExpr:
			return walker(((RowExpr *) node)->args, context);
		case T_RowCompareExpr:
			{
				RowCompareExpr *rcexpr = (RowCompareExpr *) node;

				if (walker(rcexpr->largs, context))
					return true;
				if (walker(rcexpr->rargs, context))
					return true;
			}
			break;
		case T_CoalesceExpr:
			return walker(((CoalesceExpr *) node)->args, context);
		case T_MinMaxExpr:
			return walker(((MinMaxExpr *) node)->args, context);
		case T_XmlExpr:
			{
				XmlExpr    *xexpr = (XmlExpr *) node;

				if (walker(xexpr->named_args, context))
					return true;
				/* we assume walker doesn't care about arg_names */
				if (walker(xexpr->args, context))
					return true;
			}
			break;
		case T_NullIfExpr:
			return walker(((NullIfExpr *) node)->args, context);
		case T_NullTest:
			return walker(((NullTest *) node)->arg, context);
		case T_BooleanTest:
			return walker(((BooleanTest *) node)->arg, context);
		case T_CoerceToDomain:
			return walker(((CoerceToDomain *) node)->arg, context);
		case T_TargetEntry:
			return walker(((TargetEntry *) node)->expr, context);
		case T_Query:
			/* Do nothing with a sub-Query, per discussion above */
			break;
		case T_CommonTableExpr:
			{
				CommonTableExpr *cte = (CommonTableExpr *) node;

				/*
				 * Invoke the walker on the CTE's Query node, so it can
				 * recurse into the sub-query if it wants to.
				 */
				return walker(cte->ctequery, context);
			}
			break;
		case T_List:
			foreach(temp, (List *) node)
			{
				if (walker((Node *) lfirst(temp), context))
					return true;
			}
			break;
		case T_IntList:
			/* do nothing */
			break;
		case T_FromExpr:
			{
				FromExpr   *from = (FromExpr *) node;

				if (walker(from->fromlist, context))
					return true;
				if (walker(from->quals, context))
					return true;
			}
			break;
		case T_JoinExpr:
			{
				JoinExpr   *join = (JoinExpr *) node;

				if (walker(join->larg, context))
					return true;
				if (walker(join->rarg, context))
					return true;
				if (walker(join->subqfromlist, context))    /*CDB*/
					return true;                            /*CDB*/
				if (walker(join->quals, context))
					return true;

				/*
				 * alias clause, using list are deemed uninteresting.
				 */
			}
			break;
		case T_SetOperationStmt:
			{
				SetOperationStmt *setop = (SetOperationStmt *) node;

				if (walker(setop->larg, context))
					return true;
				if (walker(setop->rarg, context))
					return true;
			}
			break;
		case T_AppendRelInfo:
			{
				AppendRelInfo *appinfo = (AppendRelInfo *) node;

				if (expression_tree_walker((Node *) appinfo->translated_vars,
										   walker, context))
					return true;
			}
			break;
		case T_GroupingClause:
			{
				GroupingClause *g = (GroupingClause *) node;
				if (expression_tree_walker((Node *)g->groupsets, walker,
					context))
					return true;
			}
			break;
		case T_GroupingFunc:
			break;
		case T_Grouping:
		case T_GroupId:
		case T_GroupClause:
		case T_SortClause: /* occurs in WindowSpec lists */
			{
				/* do nothing */
			}
			break;
		case T_TypeCast:
			{
				TypeCast *tc = (TypeCast *)node;

				if (expression_tree_walker((Node*) tc->arg, walker, context))
					return true;
			}
			break;
		case T_TableValueExpr:
			{
				TableValueExpr *expr = (TableValueExpr *) node;

				return walker(expr->subquery, context);
			}
			break;
		case T_WindowSpec:
			{
				WindowSpec *wspec = (WindowSpec *)node;

				if (expression_tree_walker((Node *) wspec->partition, walker,
										   context))
					return true;
				if (expression_tree_walker((Node *) wspec->order, walker,
										   context))
					return true;
				if (expression_tree_walker((Node *) wspec->frame,
										   walker, context))
					return true;
				return false;
			}
			break;
		case T_WindowFrame:
			{
				WindowFrame *frame = (WindowFrame *)node;

				if (expression_tree_walker((Node *) frame->trail,
										   walker, context))
					return true;
				if (expression_tree_walker((Node *) frame->lead,
										   walker, context))
					return true;
			}
			break;
		case T_WindowKey:
			{
				WindowKey *wk = (WindowKey *) node;

				if (walker((Node *) wk->frame, context))
					return true;
			}
			break;
		case T_WindowFrameEdge:
			{
				WindowFrameEdge *edge = (WindowFrameEdge *)node;

				if (walker((Node *) edge->val, context))
					return true;
			}
			break;
		case T_PercentileExpr:
			{
				PercentileExpr *perc = (PercentileExpr *) node;

				if (walker((Node *) perc->args, context))
					return true;
				if (walker((Node *) perc->sortClause, context))
					return true;
				if (walker((Node *) perc->sortTargets, context))
					return true;
				if (walker((Node *) perc->pcExpr, context))
					return true;
				if (walker((Node *) perc->tcExpr, context))
					return true;
			}
			break;

		default:
            ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
                            errmsg_internal("unrecognized node type: %d",
				                            nodeTag(node)) ));
			break;
	}
	return false;
} /* end expression_tree_walker */

/*
 * query_tree_walker --- initiate a walk of a Query's expressions
 *
 * This routine exists just to reduce the number of places that need to know
 * where all the expression subtrees of a Query are.  Note it can be used
 * for starting a walk at top level of a Query regardless of whether the
 * walker intends to descend into subqueries.  It is also useful for
 * descending into subqueries within a walker.
 *
 * Some callers want to suppress visitation of certain items in the sub-Query,
 * typically because they need to process them specially, or don't actually
 * want to recurse into subqueries.  This is supported by the flags argument,
 * which is the bitwise OR of flag values to suppress visitation of
 * indicated items.  (More flag bits may be added as needed.)
 */
bool
query_tree_walker(Query *query,
				  bool (*walker) (),
				  void *context,
				  int flags)
{
	Assert(query != NULL && IsA(query, Query));

	if (walker((Node *) query->targetList, context))
		return true;
	if (walker((Node *) query->returningList, context))
		return true;
	if (walker((Node *) query->jointree, context))
		return true;
	if (walker(query->setOperations, context))
		return true;
	if (walker(query->havingQual, context))
		return true;
	if (walker(query->groupClause, context))
		return true;
	if (walker(query->windowClause, context))
		return true;
	if (walker(query->limitOffset, context))
		return true;
	if (walker(query->limitCount, context))
		return true;
	if (!(flags & QTW_IGNORE_CTE_SUBQUERIES))
	{
		if (walker((Node *) query->cteList, context))
			return true;
	}
	if (range_table_walker(query->rtable, walker, context, flags))
		return true;
	if (query->utilityStmt)
	{
		/*
		 * Certain utility commands contain general-purpose Querys embedded in
		 * them --- if this is one, invoke the walker on the sub-Query.
		 */
		if (IsA(query->utilityStmt, CopyStmt))
		{
			if (walker(((CopyStmt *) query->utilityStmt)->query, context))
				return true;
		}
		if (IsA(query->utilityStmt, DeclareCursorStmt))
		{
			if (walker(((DeclareCursorStmt *) query->utilityStmt)->query, context))
				return true;
		}
		if (IsA(query->utilityStmt, ExplainStmt))
		{
			if (walker(((ExplainStmt *) query->utilityStmt)->query, context))
				return true;
		}
		if (IsA(query->utilityStmt, PrepareStmt))
		{
			if (walker(((PrepareStmt *) query->utilityStmt)->query, context))
				return true;
		}
		if (IsA(query->utilityStmt, ViewStmt))
		{
			if (walker(((ViewStmt *) query->utilityStmt)->query, context))
				return true;
		}
	}
	return false;
}

/*
 * range_table_walker is just the part of query_tree_walker that scans
 * a query's rangetable.  This is split out since it can be useful on
 * its own.
 */
bool
range_table_walker(List *rtable,
				   bool (*walker) (),
				   void *context,
				   int flags)
{
	ListCell   *rt;

	foreach(rt, rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(rt);

		/* For historical reasons, visiting RTEs is not the default */
		if (flags & QTW_EXAMINE_RTES)
			if (walker(rte, context))
				return true;

		switch (rte->rtekind)
		{
			case RTE_RELATION:
			case RTE_SPECIAL:
            case RTE_VOID:
			case RTE_CTE:
				/* nothing to do */
				break;
			case RTE_SUBQUERY:
				if (!(flags & QTW_IGNORE_RT_SUBQUERIES))
					if (walker(rte->subquery, context))
						return true;
				break;
			case RTE_JOIN:
				if (!(flags & QTW_IGNORE_JOINALIASES))
					if (walker(rte->joinaliasvars, context))
						return true;
				break;
			case RTE_FUNCTION:
				if (walker(rte->funcexpr, context))
					return true;
				break;
			case RTE_TABLEFUNCTION:
				if (walker(rte->subquery, context))
					return true;
				if (walker(rte->funcexpr, context))
					return true;
				break;
			case RTE_VALUES:
				if (walker(rte->values_lists, context))
					return true;
				break;
		}
	}
	return false;
}

/*
 * query_or_expression_tree_walker --- hybrid form
 *
 * This routine will invoke query_tree_walker if called on a Query node,
 * else will invoke the walker directly.  This is a useful way of starting
 * the recursion when the walker's normal change of state is not appropriate
 * for the outermost Query node.
 */
bool
query_or_expression_tree_walker(Node *node,
								bool (*walker) (),
								void *context,
								int flags)
{
	if (node && IsA(node, Query))
		return query_tree_walker((Query *) node,
								 walker,
								 context,
								 flags);
	else
		return walker(node, context);
}


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
				 void *context)
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

		case T_PartitionSelector:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			if (walker((Node *) ((PartitionSelector *) node)->levelEqExpressions, context))
				return true;
			if (walker((Node *) ((PartitionSelector *) node)->levelExpressions, context))
				return true;
			if (walker(((PartitionSelector *) node)->residualPredicate, context))
				return true;
			if (walker(((PartitionSelector *) node)->propagationExpression, context))
				return true;
			break;

		case T_Repeat:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			break;

		case T_Append:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			if (walker((Node *) ((Append *) node)->appendplans, context))
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
		case T_ExternalScan:
		case T_AppendOnlyScan:
		case T_AOCSScan:
		case T_TableScan:
		case T_DynamicTableScan:
		case T_BitmapHeapScan:
		case T_BitmapAppendOnlyScan:
		case T_BitmapTableScan:
		case T_TableFunctionScan:
		case T_ValuesScan:
			if (walk_scan_node_fields((Scan *) node, walker, context))
				return true;
			break;

		case T_FunctionScan:
			if (walker((Node *) ((FunctionScan *) node)->funcexpr, context))
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

		case T_BitmapIndexScan:
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

		case T_Window:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			if (walker(((Window *) node)->windowKeys, context))
				return true;
			break;

		case T_Unique:
			if (walk_plan_node_fields((Plan *) node, walker, context))
				return true;
			/* Other fields are simple items and lists of simple items. */
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

			if (walker((Node *) ((Motion *)node)->hashExpr, context))
				return true;

			if (walker((Node *) ((Motion *)node)->hashDataTypes, context))
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

				/* recur into the subplan's plan, a kind of Plan node */
				if (walker((Node *) subplan_plan, context))
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

		case T_DML:
		case T_SplitUpdate:
		case T_RowTrigger:
		case T_AssertOp:

			if (walk_plan_node_fields((Plan *) node, walker, context))
			{
				return true;
			}
			
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
		case T_AggOrder:
		case T_ArrayRef:
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
		case T_PartOidExpr:
		case T_PartSelectedExpr:
		case T_PartDefaultExpr:
		case T_PartBoundExpr:
		case T_PartBoundInclusionExpr:
		case T_PartBoundOpenExpr:
		case T_PartListRuleExpr:
		case T_PartListNullTestExpr:
		case T_WindowFrame:
		case T_WindowFrameEdge:
		case T_WindowKey:

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
	Insist(false);
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
		 * SubPlan has both of expressions and subquery.
		 * In case the caller wants non-subquery version,
		 * still we need to walk through its expressions.
		 */
		if (!context->descendIntoSubqueries)
		{
			if (extract_nodes_walker((Node *) subplan->testexpr,
									 context))
				return true;
			if (expression_tree_walker((Node *) subplan->args,
									   extract_nodes_walker, context))
				return true;

			/* Do not descend into subplans */
			return false;
		}
		/*
		 * Although the flag indicates the caller wants to
		 * descend into subqueries, SubPlan seems special;
		 * Some partitioning code assumes this should return
		 * immediately without descending.  See MPP-17168.
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
								  (void *) context);
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

/*
 * raw_expression_tree_walker --- walk raw parse trees
 *
 * This has exactly the same API as expression_tree_walker, but instead of
 * walking post-analysis parse trees, it knows how to walk the node types
 * found in raw grammar output.  (There is not currently any need for a
 * combined walker, so we keep them separate in the name of efficiency.)
 * Unlike expression_tree_walker, there is no special rule about query
 * boundaries: we descend to everything that's possibly interesting.
 *
 * Currently, the node type coverage extends to SelectStmt and everything
 * that could appear under it, but not other statement types.
 */
bool
raw_expression_tree_walker(Node *node, bool (*walker) (), void *context)
{
	ListCell   *temp;

	/*
	 * The walker has already visited the current node, and so we need only
	 * recurse into any sub-nodes it has.
	 */
	if (node == NULL)
		return false;

	/* Guard against stack overflow due to overly complex expressions */
	check_stack_depth();

	switch (nodeTag(node))
	{
		case T_SetToDefault:
		case T_CurrentOfExpr:
		case T_Integer:
		case T_Float:
		case T_String:
		case T_BitString:
		case T_Null:
		case T_ParamRef:
		case T_A_Const:
		case T_A_Star:
			/* primitive node types with no subnodes */
			break;
		case T_Alias:
			/* we assume the colnames list isn't interesting */
			break;
		case T_RangeVar:
			return walker(((RangeVar *) node)->alias, context);
		case T_SubLink:
			{
				SubLink    *sublink = (SubLink *) node;

				if (walker(sublink->testexpr, context))
					return true;
				/* we assume the operName is not interesting */
				if (walker(sublink->subselect, context))
					return true;
			}
			break;
		case T_CaseExpr:
			{
				CaseExpr   *caseexpr = (CaseExpr *) node;

				if (walker(caseexpr->arg, context))
					return true;
				/* we assume walker doesn't care about CaseWhens, either */
				foreach(temp, caseexpr->args)
				{
					CaseWhen   *when = (CaseWhen *) lfirst(temp);

					Assert(IsA(when, CaseWhen));
					if (walker(when->expr, context))
						return true;
					if (walker(when->result, context))
						return true;
				}
				if (walker(caseexpr->defresult, context))
					return true;
			}
			break;
		case T_RowExpr:
			return walker(((RowExpr *) node)->args, context);
		case T_CoalesceExpr:
			return walker(((CoalesceExpr *) node)->args, context);
		case T_MinMaxExpr:
			return walker(((MinMaxExpr *) node)->args, context);
		case T_XmlExpr:
			{
				XmlExpr    *xexpr = (XmlExpr *) node;

				if (walker(xexpr->named_args, context))
					return true;
				/* we assume walker doesn't care about arg_names */
				if (walker(xexpr->args, context))
					return true;
			}
			break;
		case T_NullTest:
			return walker(((NullTest *) node)->arg, context);
		case T_BooleanTest:
			return walker(((BooleanTest *) node)->arg, context);
		case T_JoinExpr:
			{
				JoinExpr   *join = (JoinExpr *) node;

				if (walker(join->larg, context))
					return true;
				if (walker(join->rarg, context))
					return true;
				if (walker(join->quals, context))
					return true;
				if (walker(join->alias, context))
					return true;
				/* using list is deemed uninteresting */
			}
			break;
		case T_IntoClause:
			{
				IntoClause *into = (IntoClause *) node;

				if (walker(into->rel, context))
					return true;
				/* colNames, options are deemed uninteresting */
			}
			break;
		case T_List:
			foreach(temp, (List *) node)
			{
				if (walker((Node *) lfirst(temp), context))
					return true;
			}
			break;
		case T_SelectStmt:
			{
				SelectStmt *stmt = (SelectStmt *) node;

				if (walker(stmt->distinctClause, context))
					return true;
				if (walker(stmt->intoClause, context))
					return true;
				if (walker(stmt->targetList, context))
					return true;
				if (walker(stmt->fromClause, context))
					return true;
				if (walker(stmt->whereClause, context))
					return true;
				if (walker(stmt->groupClause, context))
					return true;
				if (walker(stmt->havingClause, context))
					return true;
				if (walker(stmt->withClause, context))
					return true;
				if (walker(stmt->valuesLists, context))
					return true;
				if (walker(stmt->sortClause, context))
					return true;
				if (walker(stmt->limitOffset, context))
					return true;
				if (walker(stmt->limitCount, context))
					return true;
				if (walker(stmt->lockingClause, context))
					return true;
				if (walker(stmt->larg, context))
					return true;
				if (walker(stmt->rarg, context))
					return true;
			}
			break;
		case T_A_Expr:
			{
				A_Expr *expr = (A_Expr *) node;

				if (walker(expr->lexpr, context))
					return true;
				if (walker(expr->rexpr, context))
					return true;
				/* operator name is deemed uninteresting */
			}
			break;
		case T_ColumnRef:
			/* we assume the fields contain nothing interesting */
			break;
		case T_FuncCall:
			{
				FuncCall *fcall = (FuncCall *) node;

				if (walker(fcall->args, context))
					return true;
				/* function name is deemed uninteresting */
			}
			break;
		case T_A_Indices:
			{
				A_Indices *indices = (A_Indices *) node;

				if (walker(indices->lidx, context))
					return true;
				if (walker(indices->uidx, context))
					return true;
			}
			break;
		case T_A_Indirection:
			{
				A_Indirection *indir = (A_Indirection *) node;

				if (walker(indir->arg, context))
					return true;
				if (walker(indir->indirection, context))
					return true;
			}
			break;
		case T_A_ArrayExpr:
			return walker(((A_ArrayExpr *) node)->elements, context);
		case T_ResTarget:
			{
				ResTarget *rt = (ResTarget *) node;

				if (walker(rt->indirection, context))
					return true;
				if (walker(rt->val, context))
					return true;
			}
			break;
		case T_TypeCast:
			{
				TypeCast *tc = (TypeCast *) node;

				if (walker(tc->arg, context))
					return true;
				if (walker(tc->typeName, context))
					return true;
			}
			break;
		case T_SortBy:
			return walker(((SortBy *) node)->node, context);
		case T_RangeSubselect:
			{
				RangeSubselect *rs = (RangeSubselect *) node;

				if (walker(rs->subquery, context))
					return true;
				if (walker(rs->alias, context))
					return true;
			}
			break;
		case T_RangeFunction:
			{
				RangeFunction *rf = (RangeFunction *) node;

				if (walker(rf->funccallnode, context))
					return true;
				if (walker(rf->alias, context))
					return true;
			}
			break;
		case T_TypeName:
			{
				TypeName *tn = (TypeName *) node;

				if (walker(tn->typmods, context))
					return true;
				if (walker(tn->arrayBounds, context))
					return true;
				/* type name itself is deemed uninteresting */
			}
			break;
		case T_ColumnDef:
			{
				ColumnDef *coldef = (ColumnDef *) node;

				if (walker(coldef->typeName, context))
					return true;
				if (walker(coldef->raw_default, context))
					return true;
				/* for now, constraints are ignored */
			}
			break;
		case T_LockingClause:
			return walker(((LockingClause *) node)->lockedRels, context);
		case T_XmlSerialize:
			{
				XmlSerialize *xs = (XmlSerialize *) node;

				if (walker(xs->expr, context))
					return true;
				if (walker(xs->typeName, context))
					return true;
			}
			break;
		case T_WithClause:
			return walker(((WithClause *) node)->ctes, context);
		case T_CommonTableExpr:
			return walker(((CommonTableExpr *) node)->ctequery, context);
		default:
			elog(ERROR, "unrecognized node type: %d",
				 (int) nodeTag(node));
			break;
	}
	return false;
}

