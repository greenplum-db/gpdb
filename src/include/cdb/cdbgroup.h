/*-------------------------------------------------------------------------
 *
 * cdbgroup.h
 *	  prototypes for cdbgroup.c.
 *
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbgroup.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBGROUP_H
#define CDBGROUP_H

#include "nodes/params.h"
#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"
#include "nodes/relation.h"

#include "optimizer/clauses.h" /* AggClauseCounts */
#include "optimizer/planmain.h"   /* GroupContext */

extern Plan *
cdb_grouping_planner(PlannerInfo* root,
					 AggClauseCounts *agg_counts,
					 GroupContext *group_context);

extern bool cdbpathlocus_collocates(PlannerInfo *root, CdbPathLocus locus, List *pathkeys, bool exact_match);
extern CdbPathLocus cdbpathlocus_from_flow(Flow *flow);
extern void generate_three_tlists(List *tlist,
								  bool twostage,
								  List *sub_tlist,
								  Node *havingQual,
								  int numGroupCols,
								  AttrNumber *groupColIdx,
								  Oid *groupOperators,
								  List **p_tlist1,
								  List **p_tlist2,
								  List **p_tlist3,
								  List **p_final_qual);
extern Plan *add_second_stage_agg(PlannerInfo *root,
								  bool is_agg,
								  List *prelim_tlist,
								  List *final_tlist,
								  List *final_qual,
								  AggStrategy aggstrategy,
								  int numGroupCols,
								  AttrNumber *prelimGroupColIdx,
								  Oid *prelimGroupOperators,
								  int num_nullcols,
								  uint64 input_grouping,
								  uint64 grouping,
								  int rollup_gs_times,
								  double numGroups,
								  int numAggs,
								  int transSpace,
								  const char *alias,
								  List **p_current_pathkeys,
								  Plan *result_plan,
								  bool use_root,
								  bool adjust_scatter);
extern List *generate_subquery_tlist(Index varno, List *input_tlist,
									 bool keep_resjunk, int **p_resno_map);
extern List *reconstruct_pathkeys(PlannerInfo *root, List *pathkeys, int *resno_map,
								  List *orig_tlist, List *new_tlist);
extern List *augment_subplan_tlist(List *tlist, List *exprs, int *pnum, AttrNumber **pcols, bool return_resno);

extern Plan *within_agg_planner(PlannerInfo *root, AggClauseCounts *agg_counts,
								GroupContext *group_context);

#endif   /* CDBGROUP_H */
