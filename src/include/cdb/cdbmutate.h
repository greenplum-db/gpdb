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
								 bool useExecutorVarFormat,
								 int numsegments,
								 CdbLocusType targetLocus);
extern Motion *make_sorted_union_motion(PlannerInfo *root,
										Plan *lefttree,
										int numSortCols,
										AttrNumber *sortColIdx,
										Oid *sortOperators,
										Oid *collations,
										bool *nullsFirst,
										bool useExecutorVarFormat,
										int numsegments,
										CdbLocusType targetLocus);
extern Motion *make_hashed_motion(Plan *lefttree,
								  List *hashExpr,
								  List *hashOpfamilies,
								  bool useExecutorVarFormat,
								  int numsegments);

extern Motion *make_broadcast_motion(Plan *lefttree,
									 bool useExecutorVarFormat,
									 int numsegments);

extern Motion *make_explicit_motion(Plan *lefttree, AttrNumber segidColIdx, bool useExecutorVarFormat);

void 
cdbmutate_warn_ctid_without_segid(struct PlannerInfo *root, struct RelOptInfo *rel);

extern Plan *apply_shareinput_dag_to_tree(PlannerInfo *root, Plan *plan);
extern void collect_shareinput_producers(PlannerInfo *root, Plan *plan);
extern Plan *replace_shareinput_targetlists(PlannerInfo *root, Plan *plan);
extern Plan *apply_shareinput_xslice(Plan *plan, PlannerInfo *root);
extern void assign_plannode_id(PlannedStmt *stmt);

extern List *getExprListFromTargetList(List *tlist, int numCols, AttrNumber *colIdx,
									   bool useExecutorVarFormat);
extern void remove_unused_initplans(Plan *plan, PlannerInfo *root);
extern void remove_unused_subplans(PlannerInfo *root, SubPlanWalkerContext *context);

extern int32 cdbhash_const_list(List *plConsts, int iSegments, Oid *hashfuncs);
extern Node *makeSegmentFilterExpr(int segid);

extern Node *exec_make_plan_constant(struct PlannedStmt *stmt, EState *estate,
						bool is_SRI, List **cursorPositions);
extern void remove_subquery_in_RTEs(Node *node);
extern void fixup_subplans(Plan *plan, PlannerInfo *root, SubPlanWalkerContext *context);

extern void request_explicit_motion(Plan *plan, Index resultRelationIdx, List *rtable);
extern void sri_optimize_for_result(PlannerInfo *root, Plan *plan, RangeTblEntry *rte,
									GpPolicy **targetPolicy, List **hashExprs_p, List **hashOpclasses_p);
extern SplitUpdate *make_splitupdate(PlannerInfo *root, ModifyTable *mt, Plan *subplan,
									 RangeTblEntry *rte);
extern bool contains_outer_params(Node *node, void *context);

#endif   /* CDBMUTATE_H */
