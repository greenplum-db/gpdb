/*
 * walkers.h
 *
 *  Created on: Feb 8, 2011
 *      Author: siva
 */

#ifndef WALKERS_H_
#define WALKERS_H_

#include "nodes/nodes.h"
#include "nodes/parsenodes.h"
#include "nodes/nodeFuncs.h"
#include "nodes/relation.h"

/* The plan associated with a SubPlan is found in a list.  During planning this is in
 * the global structure found through the root PlannerInfo.  After planning this is in
 * the PlannedStmt.
 *
 * Structure plan_tree_base_prefix carries the appropriate pointer for GPDB's general plan
 * tree walker/mutator framework.  All users of the framework must prefix their context
 * structure with a plan_tree_base_prefix and initialize it appropriately.
 */
typedef struct plan_tree_base_prefix
{
	Node *node; /* PlannerInfo* or PlannedStmt* */
} plan_tree_base_prefix;

/*
 * Structure to hold the SUBPLAN plan_id used in the plan
 */
typedef struct SubPlanWalkerContext
{
	plan_tree_base_prefix base; /* Required prefix for plan_tree_walker/mutator */
	Bitmapset	   *bms_subplans; /* Bitmapset for used subplans */
} SubPlanWalkerContext;

extern void planner_init_plan_tree_base(plan_tree_base_prefix *base, PlannerInfo *root);
extern void exec_init_plan_tree_base(plan_tree_base_prefix *base, PlannedStmt *stmt);
extern Plan *plan_tree_base_subplan_get_plan(plan_tree_base_prefix *base, SubPlan *subplan);
extern void plan_tree_base_subplan_put_plan(plan_tree_base_prefix *base, SubPlan *subplan, Plan *plan);

extern bool walk_plan_node_fields(Plan *plan, bool (*walker) (), void *context);

extern bool plan_tree_walker(Node *node, bool (*walker) (), void *context);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Useful functions that aggregate information from expressions or plans.
 */
extern List *extract_nodes(PlannerGlobal *glob, Node *node, int nodeTag);
extern List *extract_nodes_plan(Plan *pl, int nodeTag, bool descendIntoSubqueries);
extern List *extract_nodes_expression(Node *node, int nodeTag, bool descendIntoSubqueries);
extern int find_nodes(Node *node, List *nodeTags);

#ifdef __cplusplus
}
#endif

#endif /* WALKERS_H_ */
