/*-------------------------------------------------------------------------
 *
 * cdbgroupingpaths.h
 *	  prototypes for cdbgroupingpaths.c.
 *
 *
 * Portions Copyright (c) 2019-Present Pivotal Software, Inc.
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbgroupingpaths.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBGROUPINGPATHS_H
#define CDBGROUPINGPATHS_H

#include "nodes/pathnodes.h"

extern void cdb_create_twostage_grouping_paths(PlannerInfo *root,
											   RelOptInfo *input_rel,
											   RelOptInfo *output_rel,
											   PathTarget *target,
											   PathTarget *partial_grouping_target,
											   List *havingQual,
											   bool can_sort,
											   bool can_hash,
											   double dNumGroupsTotal,
											   const AggClauseCosts *agg_costs,
											   const AggClauseCosts *agg_partial_costs,
											   const AggClauseCosts *agg_final_costs,
											   List *rollups);

extern Path *cdb_prepare_path_for_sorted_agg(PlannerInfo *root,
											 bool is_sorted,
											 RelOptInfo *rel,
											 Path *subpath,
											 PathTarget *target,
											 List *group_pathkeys,
											 double limit_tuples,
											 List *groupClause,
											 List *rollups);
extern Path *cdb_prepare_path_for_hashed_agg(PlannerInfo *root,
											 Path *subpath,
											 PathTarget *target,
											 List *groupClause,
											 List *rollups);

#endif   /* CDBGROUPINGPATHS_H */
