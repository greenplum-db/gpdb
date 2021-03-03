/*-------------------------------------------------------------------------
 *
 * cdbpath.h
 *
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbpath.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBPATH_H
#define CDBPATH_H

#include "nodes/pathnodes.h"

void
cdbpath_cost_motion(PlannerInfo *root, CdbMotionPath *motionpath);

extern Path *cdbpath_create_motion_path(PlannerInfo     *root,
                           Path            *subpath,
                           List            *pathkeys,
                           bool             require_existing_order,
                           CdbPathLocus     locus);

extern Path *cdbpath_create_explicit_motion_path(PlannerInfo *root,
									Path *subpath,
									CdbPathLocus locus);

extern Path *cdbpath_create_broadcast_motion_path(PlannerInfo *root,
									 Path *subpath,
									 int numsegments);

extern Path *cdbpath_create_redistribute_motion_path_for_exprs(PlannerInfo *root,
												  Path *subpath,
												  int numsegments,
												  List *hashExprs,
												  List *hashFamilies);

extern Path *create_motion_path_for_ctas(PlannerInfo *root, GpPolicy *policy, Path *subpath);
extern Path *create_motion_path_for_insert(PlannerInfo *root, GpPolicy *targetPolicy, Path *subpath);
extern Path *create_motion_path_for_upddel(PlannerInfo *root, Index rti, GpPolicy *targetPolicy, Path *subpath);
extern Path *create_split_update_path(PlannerInfo *root, Index rti, GpPolicy *targetPolicy, Path *subpath);

extern CdbPathLocus
cdbpath_motion_for_join(PlannerInfo    *root,
                        JoinType        jointype,           /* JOIN_INNER/FULL/LEFT/RIGHT/IN */
                        Path          **p_outer_path,       /* INOUT */
                        Path          **p_inner_path,       /* INOUT */
						int			   *p_rowidexpr_id,
                        List           *redistribution_clauses,   /* equijoin RestrictInfo list */
                        List           *restrict_clauses, /* all RestrictInfos */
                        List           *outer_pathkeys,
                        List           *inner_pathkeys,
                        bool            outer_require_existing_order,
                        bool            inner_require_existing_order);

extern bool cdbpath_contains_wts(Path *path);
extern Path * turn_volatile_seggen_to_singleqe(PlannerInfo *root, Path *path, Node *node);

#endif   /* CDBPATH_H */
