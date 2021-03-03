/*-------------------------------------------------------------------------
 *
 * memquota.c
 *	  Routines related to memory quota for queries.
 *
 * Portions Copyright (c) 2010, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/resource_manager/memquota.c
 * 
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "cdb/memquota.h"
#include "cdb/cdbllize.h"
#include "storage/lwlock.h"
#include "utils/relcache.h"
#include "executor/execdesc.h"
#include "utils/resource_manager.h"
#include "access/heapam.h"
#include "miscadmin.h"
#include "cdb/cdbvars.h"
#include "optimizer/clauses.h"
#include "parser/parsetree.h"
#include "tcop/pquery.h"

ResManagerMemoryPolicy		gp_resmanager_memory_policy_default = RESMANAGER_MEMORY_POLICY_NONE;
bool						gp_log_resmanager_memory_default = false;
int							gp_resmanager_memory_policy_auto_fixed_mem_default = 100;
bool						gp_resmanager_print_operator_memory_limits_default = false;

ResManagerMemoryPolicy		*gp_resmanager_memory_policy = &gp_resmanager_memory_policy_default;
bool						*gp_log_resmanager_memory = &gp_log_resmanager_memory_default;
int							*gp_resmanager_memory_policy_auto_fixed_mem = &gp_resmanager_memory_policy_auto_fixed_mem_default;
bool						*gp_resmanager_print_operator_memory_limits = &gp_resmanager_print_operator_memory_limits_default;

/**
 * Policy Auto. This contains information that will be used by Policy AUTO
 */
typedef struct PolicyAutoContext
{
	plan_tree_base_prefix base; /* Required prefix for plan_tree_walker/mutator */
	uint64 numNonMemIntensiveOperators; /* number of non-blocking operators */
	uint64 numMemIntensiveOperators; /* number of blocking operators */
	uint64 queryMemKB;
	PlannedStmt *plannedStmt; /* pointer to the planned statement */
} PolicyAutoContext;

/**
 * Forward declarations.
 */
static void autoIncOpMemForResGroup(uint64 *opMemKB, int numOps);
static bool PolicyAutoPrelimWalker(Node *node, PolicyAutoContext *context);
static bool	PolicyAutoAssignWalker(Node *node, PolicyAutoContext *context);
static bool IsAggMemoryIntensive(Agg *agg);
static bool IsMemoryIntensiveOperator(Node *node, PlannedStmt *stmt);

struct OperatorGroupNode;

/*
 * OperatorGroupNode
 *    Store the information regarding an operator group.
 */
typedef struct OperatorGroupNode
{
	/* The id for this group */
	uint32 groupId;

	/*
	 * The number of non-memory-intensive operators and memory-intensive
	 * operators in the group.
	 */
	uint64 numNonMemIntenseOps;
	uint64 numMemIntenseOps;

	/*
	 * The maximal number of non-memory-intensive and memory-intensive
	 * operators in all child groups of this group which might be active
	 * concurrently.
	 */
	uint64 maxNumConcNonMemIntenseOps;
	uint64 maxNumConcMemIntenseOps;

	/* The list of child groups */
	List *childGroups;

	/* The parent group */
	struct OperatorGroupNode *parentGroup;

	/* The memory limit for this group and its child groups */
	uint64 groupMemKB;
} OperatorGroupNode;

/*
 * PolicyEagerFreeContext
 *   Store the intemediate states during the tree walking for the optimize
 * memory distribution policy.
 */
typedef struct PolicyEagerFreeContext
{
	plan_tree_base_prefix base;
	OperatorGroupNode *groupTree; /* the root of the group tree */
	OperatorGroupNode *groupNode; /* the current group node in the group tree */
	uint32 nextGroupId; /* the group id for a new group node */
	uint64 queryMemKB; /* the query memory limit */
	PlannedStmt *plannedStmt; /* pointer to the planned statement */
} PolicyEagerFreeContext;

/*
 * Does the expression contain any ordered or DISTINCT aggregates?
 */
static bool
contain_ordered_aggs_walker(Node *node, void *context)
{
	if (node == NULL)
		return false;

	if (IsA(node, Aggref))
	{
		Aggref	   *aggref = (Aggref *) node;

		if (aggref->aggorder || aggref->aggdistinct)
			return true;
	}
	return expression_tree_walker(node, contain_ordered_aggs_walker, context);
}

/*
 * Automatically increase operator memory buffer in resource group mode.
 *
 * In resource group if the operator memory buffer is too small for the
 * operators we still allow the query to execute by temporarily increasing the
 * buffer size, each operator will be assigned 100KB memory no matter it is
 * memory intensive or not.  The query can execute as long as there is enough
 * resource group shared memory, the performance might not be best as 100KB is
 * rather small for memory intensive operators.  If there is no enought shared
 * memory it will run into OOM error on operators.
 *
 * @param opMemKB the original operator memory buffer size, will be in-place
 *        updated if not large enough
 * @param numOps the number of operators, both memory intensive and
 *        non-intensive
 */
static void
autoIncOpMemForResGroup(uint64 *opMemKB, int numOps)
{
	uint64		perOpMemKB;		/* per-operator buffer size */
	uint64		minOpMemKB;		/* minimal buffer size for all the operators */

	/* Only adjust operator memory buffer for resource group */
	if (!IsResGroupEnabled())
		return;

	/*
	 * The buffer reserved for a memory intensive operator is the same as
	 * non-intensive ones, by default it is 100KB
	 */
	perOpMemKB = *gp_resmanager_memory_policy_auto_fixed_mem;
	minOpMemKB = perOpMemKB * numOps;

	/* No need to change operator memory buffer if already large enough */
	if (*opMemKB >= minOpMemKB)
		return;

	ereport(DEBUG2,
			(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
			 errmsg("No enough operator memory for current query."),
			 errdetail("Current query contains %d operators, "
					   "the minimal operator memory requirement is " INT64_FORMAT " KB, "
					   "however there is only " INT64_FORMAT " KB reserved.  "
					   "Temporarily increased the operator memory to execute the query.",
					   numOps, minOpMemKB, *opMemKB),
			 errhint("Consider increase memory_spill_ratio for better performance.")));

	/* Adjust the buffer */
	*opMemKB = minOpMemKB;
}

/**
 * Is an agg operator memory intensive? The following cases mean it is:
 * 1. If agg strategy is hashed
 * 2. If targetlist or qual contains a DQA
 * 3. If there is an ordered aggregated.
 */
static bool
IsAggMemoryIntensive(Agg *agg)
{
	/* Case 1 */
	if (agg->aggstrategy == AGG_HASHED)
		return true;

	/* Cases 2 & 3 */
	if (contain_ordered_aggs_walker((Node *) agg->plan.targetlist, NULL))
		return true;
	if (contain_ordered_aggs_walker((Node *) agg->plan.qual, NULL))
		return true;

	return false;
}

/*
 * IsAggBlockingOperator
 *    Return true if an Agg node is a blocking operator.
 *
 * Agg is a blocking operator when it is
 * 1. Scalar Agg
 * 2. Second stage HashAgg when streaming is on.
 * 3. First and Second stage HashAgg when streaming is off.
 */
static bool
IsAggBlockingOperator(Agg *agg)
{
	if (agg->aggstrategy == AGG_PLAIN)
	{
		return true;
	}

	return (agg->aggstrategy == AGG_HASHED && !agg->streaming);
}

/*
 * isMaterialBlockingOperator
 *     Return true if a Material node is a blocking operator.
 *
 * Material node is a blocking operator when cdb_strict is on.
 */
static bool
IsMaterialBlockingOperator(Material *material)
{
	return material->cdb_strict;
}

/*
 * IsBlockingOperator
 *     Return true when the given plan node is a blocking operator.
 */
static bool
IsBlockingOperator(Node *node)
{
	switch(nodeTag(node))
	{
		case T_BitmapIndexScan:
		case T_Hash:
		case T_Sort:
			return true;

		case T_Material:
			return IsMaterialBlockingOperator((Material *)node);

		case T_Agg:
			return IsAggBlockingOperator((Agg *)node);

		default:
			return false;
	}
}

/**
 * Is a result node memory intensive? It is if it contains function calls.
 */
bool
IsResultMemoryIntensive(Result *res)
{

	List *funcNodes = extract_nodes(NULL /* glob */,
			(Node *) ((Plan *) res)->targetlist, T_FuncExpr);

	int nFuncExpr = list_length(funcNodes);
	/* Shallow free of the funcNodes list */
	list_free(funcNodes);
	funcNodes = NIL;
	if (nFuncExpr == 0)
	{
		/* No function expressions, not memory intensive */
		return false;
	}
	else
	{
		return true;
	}
}

/**
 * Is an operator memory intensive?
 */
static bool
IsMemoryIntensiveOperator(Node *node, PlannedStmt *stmt)
{
	Assert(is_plan_node(node));
	switch(nodeTag(node))
	{
		case T_Material:
		case T_Sort:
		case T_ShareInputScan:
		case T_Hash:
		case T_BitmapIndexScan:
		case T_WindowAgg:
		case T_TableFunctionScan:
		case T_FunctionScan:
			return true;
		case T_Agg:
			{
				Agg *agg = (Agg *) node;
				return IsAggMemoryIntensive(agg);
			}
		case T_Result:
			{
				Result *res = (Result *) node;
				return IsResultMemoryIntensive(res);
			}
		default:
			return false;
	}
}

/*
 * IsRootOperatorInGroup
 *    Return true if the given node is the root operator in an operator group.
 *
 * A node can be a root operator in a group if it satisfies the following three
 * conditions:
 * (1) a Plan node.
 * (2) a Blocking operator.
 * (3) not rescan required (no external parameters).
 */
static bool
IsRootOperatorInGroup(Node *node)
{
	return (is_plan_node(node) && IsBlockingOperator(node) && ((Plan *)(node))->extParam == NULL);
}

/**
 * This walker counts the number of memory intensive and non-memory intensive operators
 * in a plan.
 */

static bool PolicyAutoPrelimWalker(Node *node, PolicyAutoContext *context)
{
	if (node == NULL)
	{
		return false;
	}

	Assert(context);
	if (is_plan_node(node))
	{
		if (IsMemoryIntensiveOperator(node, context->plannedStmt))
		{
			context->numMemIntensiveOperators++;
		}
		else
		{
			context->numNonMemIntensiveOperators++;
		}
	}
	return plan_tree_walker(node, PolicyAutoPrelimWalker, context, true);
}

/**
 * This walker assigns specific amount of memory to each operator in a plan.
 * It allocates a fixed size to each non-memory intensive operator and distributes
 * the rest among memory intensive operators.
 */
static bool PolicyAutoAssignWalker(Node *node, PolicyAutoContext *context)
{
	const uint64 nonMemIntenseOpMemKB = (uint64) (*gp_resmanager_memory_policy_auto_fixed_mem);

	if (node == NULL)
	{
		return false;
	}

	Assert(context);

	if (is_plan_node(node))
	{
		Plan *planNode = (Plan *) node;

		/**
		 * If the operator is not a memory intensive operator, give it fixed amount of memory.
		 */
		if (!IsMemoryIntensiveOperator(node, context->plannedStmt))
		{
			planNode->operatorMemKB = nonMemIntenseOpMemKB;
		}
		else
		{
			planNode->operatorMemKB = (uint64) ( (double) context->queryMemKB
					- (double) context->numNonMemIntensiveOperators * nonMemIntenseOpMemKB)
					/ context->numMemIntensiveOperators;
		}

		Assert(planNode->operatorMemKB > 0);

		if (LogResManagerMemory())
		{
			elog(GP_RESMANAGER_MEMORY_LOG_LEVEL, "assigning plan node memory = %dKB", (int )planNode->operatorMemKB);
		}
	}
	return plan_tree_walker(node, PolicyAutoAssignWalker, context, true);
}

/**
 * Main entry point for memory quota policy AUTO. It counts how many operators
 * there are in a plan. It walks the plan again and allocates a fixed amount to every non-memory intensive operators.
 * It distributes the rest of the memory available to other operators.
 */
void PolicyAutoAssignOperatorMemoryKB(PlannedStmt *stmt, uint64 memAvailableBytes)
{
	PolicyAutoContext ctx;
	exec_init_plan_tree_base(&ctx.base, stmt);
	ctx.queryMemKB = (uint64) (memAvailableBytes / 1024);
	ctx.numMemIntensiveOperators = 0;
	ctx.numNonMemIntensiveOperators = 0;
	ctx.plannedStmt = stmt;

#ifdef USE_ASSERT_CHECKING
	bool result =
#endif
			 PolicyAutoPrelimWalker((Node *) stmt->planTree, &ctx);

	Assert(!result);
	Assert(ctx.numMemIntensiveOperators + ctx.numNonMemIntensiveOperators > 0);

	/*
	 * Make sure there is enough operator memory in resource group mode.
	 */
	autoIncOpMemForResGroup(&ctx.queryMemKB,
							ctx.numNonMemIntensiveOperators +
							ctx.numMemIntensiveOperators);

	if (ctx.queryMemKB <= ctx.numNonMemIntensiveOperators * (*gp_resmanager_memory_policy_auto_fixed_mem))
	{
		elog(ERROR, ERRMSG_GP_INSUFFICIENT_STATEMENT_MEMORY);
	}

#ifdef USE_ASSERT_CHECKING
	result =
#endif
			 PolicyAutoAssignWalker((Node *) stmt->planTree, &ctx);

	Assert(!result);
}

/*
 * What should be query mem such that memory intensive operators get a certain
 * minimum amount of memory.  Return value is in bytes.
 */
uint64
PolicyAutoStatementMemForNoSpill(PlannedStmt *stmt, uint64 minOperatorMem)
{
	Assert(stmt);
	Assert(minOperatorMem > 0);

	const uint64 nonMemIntenseOpMem = ((uint64) (*gp_resmanager_memory_policy_auto_fixed_mem) * 1024);

	PolicyAutoContext ctx;
	exec_init_plan_tree_base(&ctx.base, stmt);
	ctx.queryMemKB = (uint64) (stmt->query_mem / 1024);
	ctx.numMemIntensiveOperators = 0;
	ctx.numNonMemIntensiveOperators = 0;
	ctx.plannedStmt = stmt;

#ifdef USE_ASSERT_CHECKING
	bool result =
#endif
		PolicyAutoPrelimWalker((Node *) stmt->planTree, &ctx);

	Assert(!result);
	Assert(ctx.numMemIntensiveOperators + ctx.numNonMemIntensiveOperators > 0);

	/*
	 * Right now, the inverse is straightforward.
	 * TODO: Siva - employ binary search to find the right value.
	 */
	uint64 requiredStatementMem = ctx.numNonMemIntensiveOperators * nonMemIntenseOpMem
									+ ctx.numMemIntensiveOperators * minOperatorMem;

	return requiredStatementMem;
}

/*
 * CreateOperatorGroup
 *    create a new operator group with a specified id.
 */
static OperatorGroupNode *
CreateOperatorGroupNode(uint32 groupId, OperatorGroupNode *parentGroup)
{
	OperatorGroupNode *node = palloc0(sizeof(OperatorGroupNode));
	node->groupId = groupId;
	node->parentGroup = parentGroup;

	return node;
}

/*
 * IncrementOperatorCount
 *    Increment the count of operators in the current group based
 * on the type of the operator.
 */
static void
IncrementOperatorCount(Node *node, OperatorGroupNode *groupNode, PlannedStmt *stmt)
{
	Assert(node != NULL);
	Assert(groupNode != NULL);

	if (IsMemoryIntensiveOperator(node, stmt))
	{
		groupNode->numMemIntenseOps++;
	}
	else
	{
		groupNode->numNonMemIntenseOps++;
	}
}

/*
 * GetParentOperatorNode
 *    Return the parent operator group for a given group.
 */
static OperatorGroupNode *
GetParentOperatorGroup(OperatorGroupNode *groupNode)
{
	Assert(groupNode != NULL);
	return groupNode->parentGroup;
}


/*
 * CreateOperatorGroupForOperator
 *    create an operator group for a given operator node if the given operator node
 * is a potential root of an operator group.
 */
static OperatorGroupNode *
CreateOperatorGroupForOperator(Node *node,
							   PolicyEagerFreeContext *context)
{
	Assert(is_plan_node(node));

	OperatorGroupNode *groupNode = context->groupNode;

	/*
	 * If the group tree has not been built, we create the first operator
	 * group here.
	 */
	if (context->groupTree == NULL)
	{
		groupNode = CreateOperatorGroupNode(context->nextGroupId, NULL);
		Assert(groupNode != NULL);

		context->groupTree = groupNode;
		context->groupTree->groupMemKB = context->queryMemKB;
		context->nextGroupId++;
	}

	/*
	 * If this node is a potential root of an operator group, this means that
	 * the current group ends, and a new group starts. we create a new operator
	 * group.
	 */
	else if (IsRootOperatorInGroup(node))
	{
		Assert(groupNode != NULL);

		OperatorGroupNode *parentGroupNode = groupNode;
		groupNode = CreateOperatorGroupNode(context->nextGroupId, groupNode);
		if (parentGroupNode != NULL)
		{
			parentGroupNode->childGroups = lappend(parentGroupNode->childGroups, groupNode);
		}
		context->nextGroupId++;
	}

	return groupNode;
}

/*
 * FindOperatorGroupForOperator
 *   find the operator group for a given operator node that is the root operator
 * of the returning group.
 */
static OperatorGroupNode *
FindOperatorGroupForOperator(Node *node,
							 PolicyEagerFreeContext *context)
{
	Assert(is_plan_node(node));
	Assert(context->groupTree != NULL);

	OperatorGroupNode *groupNode = context->groupNode;

	/*
	 * If this is the beginning of the walk (or the current group is NULL),
	 * this operator node belongs to the first operator group.
	 */
	if (groupNode == NULL)
	{
		groupNode = context->groupTree;
		context->nextGroupId++;
	}

	/*
	 * If this operator is a potential root of an operator group, this means
	 * the current group ends, and a new group start. We find the group that
	 * has this node as its root.
	 */
	else if (IsRootOperatorInGroup(node))
	{
		Assert(context->groupNode != NULL);

		ListCell *lc = NULL;
		OperatorGroupNode *childGroup = NULL;
		foreach(lc, context->groupNode->childGroups)
		{
			childGroup = (OperatorGroupNode *)lfirst(lc);
			if (childGroup->groupId == context->nextGroupId)
			{
				break;
			}
		}

		Assert(childGroup != NULL);
		groupNode = childGroup;
		context->nextGroupId++;
	}

	return groupNode;
}

/*
 * ComputeAvgMemKBForMemIntenseOp
 *    Compute the average memory limit for each memory-intensive operators
 * in a given group.
 *
 * If there is no memory-intensive operators in this group, return 0.
 */
static uint64
ComputeAvgMemKBForMemIntenseOp(OperatorGroupNode *groupNode)
{
	if (groupNode->numMemIntenseOps == 0)
	{
		return 0;
	}

	const uint64 nonMemIntenseOpMemKB = (uint64)(*gp_resmanager_memory_policy_auto_fixed_mem);

	return (((double)groupNode->groupMemKB -
			 (double)groupNode->numNonMemIntenseOps * nonMemIntenseOpMemKB) /
			groupNode->numMemIntenseOps);
}

/*
 * ComputeMemLimitForChildGroups
 *    compute the query memory limit for all child groups of a given
 * parent group if it has not been computed before.
 */
static void
ComputeMemLimitForChildGroups(OperatorGroupNode *parentGroupNode)
{
	Assert(parentGroupNode != NULL);

	uint64 totalNumMemIntenseOps = 0;
	uint64 totalNumNonMemIntenseOps = 0;

	ListCell *lc;
	foreach(lc, parentGroupNode->childGroups)
	{
		OperatorGroupNode *childGroup = (OperatorGroupNode *)lfirst(lc);

		/*
		 * If the memory limit has been computed, then we are done.
		 */
		if (childGroup->groupMemKB != 0)
		{
			return;
		}

		totalNumMemIntenseOps +=
			Max(childGroup->maxNumConcMemIntenseOps, childGroup->numMemIntenseOps);
		totalNumNonMemIntenseOps +=
			Max(childGroup->maxNumConcNonMemIntenseOps, childGroup->numNonMemIntenseOps);
	}

	const uint64 nonMemIntenseOpMemKB = (uint64)(*gp_resmanager_memory_policy_auto_fixed_mem);

	foreach(lc, parentGroupNode->childGroups)
	{
		OperatorGroupNode *childGroup = (OperatorGroupNode *)lfirst(lc);

		Assert(childGroup->groupMemKB == 0);

		if (parentGroupNode->groupMemKB < totalNumNonMemIntenseOps * nonMemIntenseOpMemKB)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
					 errmsg("insufficient memory reserved for statement")));
		}

		double memIntenseOpMemKB = 0;
		if (totalNumMemIntenseOps > 0)
		{
			memIntenseOpMemKB =
				((double)(parentGroupNode->groupMemKB -
						  totalNumNonMemIntenseOps * nonMemIntenseOpMemKB)) /
				((double)totalNumMemIntenseOps);
		}

		Assert(parentGroupNode->groupMemKB > 0);

		/*
		 * MPP-23130
		 * In memory policy eager free, scaleFactor is used to balance memory allocated to
		 * each child group based on the number of memory-intensive and non-memory-intensive
		 * operators they have. The calculation of scaleFactor is as follows:
		 * scaleFactor = (memIntensiveOpMem *
		 *               Max(childGroup->maxNumConcMemIntenseOps, childGroup->numMemIntenseOps)
		 *              + nonMemIntenseOpMemKB *
		 *               Max(childGroup->maxNumConcNonMemIntenseOps, childGroup->numNonMemIntenseOps))
		 *               / parentGroupNode->groupMemKB
		 * Child group's memory: childGroup->groupMemKB = scaleFactor * parentGroupNode->groupMemKB,
		 * which is the denominator of the scaleFactor formula.
		 */
		childGroup->groupMemKB = (uint64) (memIntenseOpMemKB *
				  	  	  	  	  	  	  Max(childGroup->maxNumConcMemIntenseOps, childGroup->numMemIntenseOps) +
				  	  	  	  	  	  	  nonMemIntenseOpMemKB *
				  	  	  	  	  	  	  Max(childGroup->maxNumConcNonMemIntenseOps, childGroup->numNonMemIntenseOps));
	}
}

/*
 * PolicyEagerFreePrelimWalker
 *    Walk the plan tree to build a group tree by dividing the plan tree
 * into several groups, each of which has a block operator as its border
 * node (except for the leaves of the leave groups). At the same time,
 * we collect some stats information about operators in each group.
 */
static bool
PolicyEagerFreePrelimWalker(Node *node, PolicyEagerFreeContext *context)
{
	if (node == NULL)
	{
		return false;
	}

	Assert(context);

	OperatorGroupNode *parentGroupNode = NULL;
	bool isTopPlanNode = false;

	if (is_plan_node(node))
	{
		if (context->groupTree == NULL)
		{
			isTopPlanNode = true;
		}

		context->groupNode = CreateOperatorGroupForOperator(node, context);
		Assert(context->groupNode != NULL);

		IncrementOperatorCount(node, context->groupNode, context->plannedStmt);

		/*
		 * We also increment the parent group's counter if this node
		 * is the root node in a new group.
		 */
		parentGroupNode = GetParentOperatorGroup(context->groupNode);
		if (IsRootOperatorInGroup(node) && parentGroupNode != NULL)
		{
			IncrementOperatorCount(node, parentGroupNode, context->plannedStmt);
		}
	}

	bool result = plan_tree_walker(node, PolicyEagerFreePrelimWalker, context, true);
	Assert(!result);

	/*
	 * If this node is the top node in a group, at this point, we should have all info about
	 * its child groups. We then calculate the maximum number of potential concurrently
	 * active memory-intensive operators and non-memory-intensive operators in all
	 * child groups.
	 */
	if (isTopPlanNode || IsRootOperatorInGroup(node))
	{
		Assert(context->groupNode != NULL);

		uint64 maxNumConcNonMemIntenseOps = 0;
		uint64 maxNumConcMemIntenseOps = 0;

		ListCell *lc;
		foreach(lc, context->groupNode->childGroups)
		{
			OperatorGroupNode *childGroup = (OperatorGroupNode *)lfirst(lc);
			maxNumConcNonMemIntenseOps +=
				Max(childGroup->maxNumConcNonMemIntenseOps, childGroup->numNonMemIntenseOps);
			maxNumConcMemIntenseOps +=
				Max(childGroup->maxNumConcMemIntenseOps, childGroup->numMemIntenseOps);
		}

		Assert(context->groupNode->maxNumConcNonMemIntenseOps == 0 &&
			   context->groupNode->maxNumConcMemIntenseOps == 0);
		context->groupNode->maxNumConcNonMemIntenseOps = maxNumConcNonMemIntenseOps;
		context->groupNode->maxNumConcMemIntenseOps = maxNumConcMemIntenseOps;

		/* Reset the groupNode to point to its parentGroupNode */
		context->groupNode = GetParentOperatorGroup(context->groupNode);
	}

	return result;
}

/*
 * PolicyEagerFreeAssignWalker
 *    Walk the plan tree and assign the memory to each plan node.
 */
static bool
PolicyEagerFreeAssignWalker(Node *node, PolicyEagerFreeContext *context)
{
	if (node == NULL)
	{
		return false;
	}

	Assert(context);

	const uint64 nonMemIntenseOpMemKB = (uint64)(*gp_resmanager_memory_policy_auto_fixed_mem);

	if (is_plan_node(node))
	{
		Plan *planNode = (Plan *)node;

		context->groupNode = FindOperatorGroupForOperator(node, context);
		Assert(context->groupNode != NULL);

		/*
		 * If this is the root node in a group, compute the new query limit for
		 * all child groups of the parent group.
		 */
		if (IsRootOperatorInGroup(node) &&
			GetParentOperatorGroup(context->groupNode) != NULL)
		{
			ComputeMemLimitForChildGroups(GetParentOperatorGroup(context->groupNode));
		}

		if (!IsMemoryIntensiveOperator(node, context->plannedStmt))
		{
			planNode->operatorMemKB = nonMemIntenseOpMemKB;
		}
		else
		{
			/*
			 * Evenly distribute the remaining memory among all memory-intensive
			 * operators.
			 */
			uint64 memKB = ComputeAvgMemKBForMemIntenseOp(context->groupNode);

			planNode->operatorMemKB = memKB;

			OperatorGroupNode *parentGroupNode = GetParentOperatorGroup(context->groupNode);

			/*
			 * If this is the root node in the group, we also calculate the memory
			 * for this node as it appears in the parent group. The final memory limit
			 * for this node is the minimal value of the two.
			 */
			if (IsRootOperatorInGroup(node) &&
				parentGroupNode != NULL)
			{
				uint64 memKBInParentGroup = ComputeAvgMemKBForMemIntenseOp(parentGroupNode);

				if (memKBInParentGroup < planNode->operatorMemKB)
				{
					planNode->operatorMemKB = memKBInParentGroup;
				}
			}
		}
	}

	bool result = plan_tree_walker(node, PolicyEagerFreeAssignWalker, context, true);
	Assert(!result);

	/*
	 * If this node is the root in a group, we reset some values in the context.
	 */
	if (IsRootOperatorInGroup(node))
	{
		Assert(context->groupNode != NULL);
		context->groupNode = GetParentOperatorGroup(context->groupNode);
	}

	return result;
}

/*
 * PolicyEagerFreeAssignOperatorMemoryKB
 *    Main entry point for memory quota OPTIMIZE. This function distributes the memory
 * among all operators in a more optimized way than the AUTO policy.
 *
 * This function considers not all memory-intensive operators will be active concurrently,
 * and distributes the memory accordingly.
 */
void
PolicyEagerFreeAssignOperatorMemoryKB(PlannedStmt *stmt, uint64 memAvailableBytes)
{
	PolicyEagerFreeContext ctx;
	exec_init_plan_tree_base(&ctx.base, stmt);
	ctx.groupTree = NULL;
	ctx.groupNode = NULL;
	ctx.nextGroupId = 0;
	ctx.queryMemKB = memAvailableBytes / 1024;
	ctx.plannedStmt = stmt;

#ifdef USE_ASSERT_CHECKING
	bool result =
#endif
		PolicyEagerFreePrelimWalker((Node *) stmt->planTree, &ctx);

	Assert(!result);

	/*
	 * Reset groupNode and nextGroupId so that we can start from the
	 * beginning of the group tree.
	 */
	ctx.groupNode = NULL;
	ctx.nextGroupId = 0;

	/*
	 * Make sure there is enough operator memory in resource group mode.
	 */
	autoIncOpMemForResGroup(&ctx.groupTree->groupMemKB,
							Max(ctx.groupTree->numNonMemIntenseOps,
								ctx.groupTree->maxNumConcNonMemIntenseOps) +
							Max(ctx.groupTree->numMemIntenseOps,
								ctx.groupTree->maxNumConcMemIntenseOps));

	/*
	 * Check if memory exceeds the limit in the root group
	 */
	const uint64 nonMemIntenseOpMemKB = (uint64)(*gp_resmanager_memory_policy_auto_fixed_mem);
	if (ctx.groupTree->groupMemKB < ctx.groupTree->numNonMemIntenseOps * nonMemIntenseOpMemKB)
	{
		ereport(ERROR,
			(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
			errmsg("insufficient memory reserved for statement")));
	}

#ifdef USE_ASSERT_CHECKING
	result =
#endif
		PolicyEagerFreeAssignWalker((Node *) stmt->planTree, &ctx);

	Assert(!result);
}

/*
 * Calculate the amount of memory reserved for the query
 */
int64
ResourceManagerGetQueryMemoryLimit(PlannedStmt* stmt)
{
	if (Gp_role != GP_ROLE_DISPATCH)
		return 0;

	/* no limits in single user mode. */
	if (!IsUnderPostmaster)
		return 0;

	Assert(gp_session_id > -1);
	Assert(ActivePortal != NULL);

	if (IsResQueueEnabled())
		return ResourceQueueGetQueryMemoryLimit(stmt, ActivePortal->queueId);
	if (IsResGroupActivated())
		return ResourceGroupGetQueryMemoryLimit();

	return 0;
}
