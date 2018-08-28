/*-------------------------------------------------------------------------
 *
 * cdbpath.h
 *
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbpath.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBPATH_H
#define CDBPATH_H

#include "nodes/relation.h"

void
cdbpath_cost_motion(PlannerInfo *root, CdbMotionPath *motionpath);

Path *
cdbpath_create_motion_path(PlannerInfo     *root,
                           Path            *subpath,
                           List            *pathkeys,
                           bool             require_existing_order,
                           CdbPathLocus     locus);

CdbPathLocus
cdbpath_motion_for_join(PlannerInfo    *root,
                        JoinType        jointype,           /* JOIN_INNER/FULL/LEFT/RIGHT/IN */
                        Path          **p_outer_path,       /* INOUT */
                        Path          **p_inner_path,       /* INOUT */
                        List           *redistribution_clauses,   /* equijoin RestrictInfo list */
                        List           *outer_pathkeys,
                        List           *inner_pathkeys,
                        bool            outer_require_existing_order,
                        bool            inner_require_existing_order);

void 
cdbpath_dedup_fixup(PlannerInfo *root, Path *path);

bool
cdbpath_contains_wts(Path *path);

#endif   /* CDBPATH_H */
