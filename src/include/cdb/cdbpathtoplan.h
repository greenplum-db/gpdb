/*-------------------------------------------------------------------------
 *
 * cdbpathtoplan.h
 *
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbpathtoplan.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBPATHTOPLAN_H
#define CDBPATHTOPLAN_H


Flow *
cdbpathtoplan_create_flow(PlannerInfo  *root,
                          CdbPathLocus  locus,
                          Relids        relids,
                          List         *pathkeys,
                          Plan         *plan);

Motion *
cdbpathtoplan_create_motion_plan(PlannerInfo   *root,
                                 CdbMotionPath *path,
                                 Plan          *subplan);

#endif   /* CDBPATHTOPLAN_H */
