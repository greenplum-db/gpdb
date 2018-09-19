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
                          Plan         *plan);

#endif   /* CDBPATHTOPLAN_H */
