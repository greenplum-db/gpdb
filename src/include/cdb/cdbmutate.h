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
#include "nodes/relation.h"
#include "optimizer/walkers.h"

extern Motion *make_union_motion(Plan *lefttree);
extern Motion *make_sorted_union_motion(PlannerInfo *root, Plan *lefttree, int numSortCols, AttrNumber *sortColIdx, Oid *sortOperators,
										Oid *collations, bool *nullsFirst);
extern Motion *make_hashed_motion(Plan *lefttree,
								  List *hashExpr,
								  List *hashOpfamilies,
								  int numHashSegments);

extern Motion *make_broadcast_motion(Plan *lefttree);

extern Plan *make_explicit_motion(PlannerInfo *root,
								  Plan *lefttree,
								  AttrNumber segidColIdx);

void 
cdbmutate_warn_ctid_without_segid(struct PlannerInfo *root, struct RelOptInfo *rel);

extern Plan *apply_shareinput_dag_to_tree(PlannerInfo *root, Plan *plan);
extern void collect_shareinput_producers(PlannerInfo *root, Plan *plan);
extern Plan *replace_shareinput_targetlists(PlannerInfo *root, Plan *plan);
extern Plan *apply_shareinput_xslice(Plan *plan, PlannerInfo *root);

extern List *getExprListFromTargetList(List *tlist, int numCols, AttrNumber *colIdx);
extern void remove_unused_initplans(Plan *plan, PlannerInfo *root);

extern int32 cdbhash_const_list(List *plConsts, int iSegments, Oid *hashfuncs);
extern Node *makeSegmentFilterExpr(int segid);

extern Node *exec_make_plan_constant(struct PlannedStmt *stmt, EState *estate,
						bool is_SRI, List **cursorPositions);
extern void remove_subquery_in_RTEs(Node *node);

extern Plan *cdbpathtoplan_create_sri_plan(RangeTblEntry *rte, PlannerInfo *subroot, Path *subpath, int createplan_flags);

extern bool contains_outer_params(Node *node, void *context);

#endif   /* CDBMUTATE_H */
