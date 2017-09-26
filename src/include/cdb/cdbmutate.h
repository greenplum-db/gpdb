/*-------------------------------------------------------------------------
 *
 * cdbmutate.h
 *	  definitions for cdbmutate.c utilities
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbmutate.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBMUTATE_H
#define CDBMUTATE_H

#include "nodes/execnodes.h"
#include "nodes/plannodes.h"
#include "nodes/params.h"
#include "nodes/relation.h"
#include "optimizer/walkers.h"

extern Plan *apply_motion(struct PlannerInfo *root, Plan *plan, Query *query);

extern Motion *make_union_motion(Plan *lefttree,
		                                int destSegIndex, bool useExecutorVarFormat);
extern Motion *make_sorted_union_motion(PlannerInfo *root, Plan *lefttree, int destSegIndex,
										List *sortPathKeys,
						 bool useExecutorVarFormat);
extern Motion *make_hashed_motion(Plan *lefttree,
				    List *hashExpr, bool useExecutorVarFormat);

extern Motion *make_broadcast_motion(Plan *lefttree, bool useExecutorVarFormat);

extern Motion *make_explicit_motion(Plan *lefttree, AttrNumber segidColIdx, bool useExecutorVarFormat);

void 
cdbmutate_warn_ctid_without_segid(struct PlannerInfo *root, struct RelOptInfo *rel);

extern Plan *zap_trivial_result(PlannerInfo *root, Plan *plan); 
extern Plan *apply_shareinput_dag_to_tree(PlannerGlobal *glob, Plan *plan, List *rtable);
extern void collect_shareinput_producers(PlannerGlobal *glob, Plan *plan, List *rtable);
extern Plan *replace_shareinput_targetlists(PlannerGlobal *glob, Plan *plan, List *rtable);
extern Plan *apply_shareinput_xslice(Plan *plan, PlannerGlobal *glob);
extern void assign_plannode_id(PlannedStmt *stmt);

extern List *getExprListFromTargetList(List *tlist, int numCols, AttrNumber *colIdx,
									   bool useExecutorVarFormat);
extern void remove_unused_initplans(Plan *plan, PlannerInfo *root);
extern void remove_unused_subplans(PlannerInfo *root, SubPlanWalkerContext *context);

extern int32 cdbhash_const(Const *pconst, int iSegments);
extern int32 cdbhash_const_list(List *plConsts, int iSegments);

extern Node *exec_make_plan_constant(struct PlannedStmt *stmt, EState *estate,
						bool is_SRI, List **cursorPositions);
extern void remove_subquery_in_RTEs(Node *node);
extern void fixup_subplans(Plan *plan, PlannerInfo *root, SubPlanWalkerContext *context);
#endif   /* CDBMUTATE_H */
