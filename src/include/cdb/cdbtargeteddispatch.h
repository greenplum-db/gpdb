/*-------------------------------------------------------------------------
 *
 * The targeted dispatch code will use query information to assign content ids
 *    to the root node of a plan, when that information is calculatable.  
 *
 * For example, in the query select * from t1 where t1.distribution_key = 100
 *
 * Then it is known that t1.distribution_key must equal 100, and so the content
 *    id can be calculated from that.
 *
 * See MPP-6939 for more information.
 *
 * Portions Copyright (c) 2009, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbtargeteddispatch.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBTARGETEDDISPATCH_H
#define CDBTARGETEDDISPATCH_H

#include "nodes/pathnodes.h"
#include "nodes/plannodes.h"

extern void FinalizeDirectDispatchDataForSlice(PlanSlice *slice);
extern void DirectDispatchUpdateContentIdsFromPlan(PlannerInfo *root, Plan *plan);
extern void DirectDispatchUpdateContentIdsForInsert(PlannerInfo *root, Plan *plan,
													GpPolicy *targetPolicy, Oid *hashfuncs);

extern void MergeDirectDispatchCalculationInfo(DirectDispatchInfo *to, DirectDispatchInfo *from);

#endif   /* CDBTARGETEDDISPATCH_H */
