/*-------------------------------------------------------------------------
 *
 * execUtils.h
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/execUtils.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef _EXECUTILS_H_
#define _EXECUTILS_H_

#include "executor/execdesc.h"

struct EState;
struct QueryDesc;
struct CdbDispatcherState;

typedef struct ScanOidEntry
{
	Oid         rel_id;
	void       *ss;
} ScanOidEntry;

extern void InitSliceTable(struct EState *estate, int nMotions, int nSubplans);
extern Slice *getCurrentSlice(struct EState *estate, int sliceIndex);
extern bool sliceRunsOnQD(Slice *slice);
extern bool sliceRunsOnQE(Slice *slice);
extern int sliceCalculateNumSendingProcesses(Slice *slice);

extern void AssignGangs(struct CdbDispatcherState *ds, QueryDesc *queryDesc);

extern Motion *findSenderMotion(PlannedStmt *plannedstmt, int sliceIndex);
extern Bitmapset *getLocallyExecutableSubplans(PlannedStmt *plannedstmt, Plan *root);
extern void ExtractParamsFromInitPlans(PlannedStmt *plannedstmt, Plan *root, EState *estate);
extern void AssignParentMotionToPlanNodes(PlannedStmt *plannedstmt);

#ifdef USE_ASSERT_CHECKING
struct PlannedStmt;
extern void AssertSliceTableIsValid(SliceTable *st, struct PlannedStmt *pstmt);
#endif

extern HTAB *create_ss_cache_for_dynamic_scan(char *name, EState *estate);
extern void  release_ss_cache_for_dynamic_scan(HTAB *reltable, List *relids);

#endif
