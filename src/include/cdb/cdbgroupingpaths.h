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

#include "nodes/relation.h"

extern void cdb_create_twostage_grouping_paths(PlannerInfo *root,
											   RelOptInfo *input_rel,
											   RelOptInfo *output_rel,
											   PathTarget *target,
											   PathTarget *partial_grouping_target,
											   bool can_sort,
											   bool consider_hash,
											   double dNumGroups,
											   const AggClauseCosts *agg_costs,
											   const AggClauseCosts *agg_partial_costs,
											   const AggClauseCosts *agg_final_costs,
											   List *rollup_lists,
											   List *rollup_groupclauses);

extern CdbPathLocus cdb_choose_grouping_locus(PlannerInfo *root, Path *path,
											  PathTarget *target,
											  List *groupClause,
											  List *rollup_lists,
											  List *rollup_groupclauses,
											  bool *need_redistribute_p);

#endif   /* CDBGROUPINGPATHS_H */
