/*-------------------------------------------------------------------------
 *
 * execUtils.h
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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

extern SliceTable *InitSliceTable(struct EState *estate, PlannedStmt *plannedstmt);
extern ExecSlice *getCurrentSlice(struct EState *estate, int sliceIndex);
extern bool sliceRunsOnQD(ExecSlice *slice);
extern bool sliceRunsOnQE(ExecSlice *slice);

extern void AssignGangs(struct CdbDispatcherState *ds, QueryDesc *queryDesc);

extern Motion *findSenderMotion(PlannedStmt *plannedstmt, int sliceIndex);
extern Bitmapset *getLocallyExecutableSubplans(PlannedStmt *plannedstmt, Plan *root);
extern void InstallDispatchedExecParams(QueryDispatchDesc *ddesc, EState *estate);

#ifdef USE_ASSERT_CHECKING
struct PlannedStmt;
extern void AssertSliceTableIsValid(SliceTable *st);
#endif

#endif
