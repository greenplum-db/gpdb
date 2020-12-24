/*-------------------------------------------------------------------------
 *
 * cdbllize.h
 *	  definitions for parallelizing a PostgreSQL sequential plan tree.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbllize.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef CDBLLIZE_H
#define CDBLLIZE_H

#include "nodes/nodes.h"
#include "nodes/pathnodes.h"
#include "nodes/plannodes.h"

extern CdbPathLocus cdbllize_get_final_locus(PlannerInfo *root, PathTarget *target);

extern Path *cdbllize_adjust_top_path(PlannerInfo *root, Path *best_path, PlanSlice *topslice);
extern Path *cdbllize_adjust_init_plan_path(PlannerInfo *root, Path *best_path);
extern Plan *cdbllize_decorate_subplans_with_motions(PlannerInfo *root, Plan *plan);
extern void cdbllize_build_slice_table(PlannerInfo *root, Plan *top_plan, PlanSlice *top_slice);

extern void motion_sanity_check(PlannerInfo *root, Plan *plan);

extern bool is_plan_node(Node *node);

extern Flow *makeFlow(FlowType flotype, int numsegments);

#endif   /* CDBLLIZE_H */
