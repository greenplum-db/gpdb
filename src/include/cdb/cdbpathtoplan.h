/*-------------------------------------------------------------------------
 *
 * cdbpathtoplan.h
 *
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbpathtoplan.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBPATHTOPLAN_H
#define CDBPATHTOPLAN_H

extern Flow *cdbpathtoplan_create_flow(PlannerInfo *root, CdbPathLocus locus);

#endif   /* CDBPATHTOPLAN_H */
