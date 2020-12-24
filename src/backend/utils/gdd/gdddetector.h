/*-------------------------------------------------------------------------
 *
 * gdddetector.h
 *	  Global DeadLock Detector - Detector Algorithm
 *
 *
 * Copyright (c) 2018-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */

#ifndef GDD_DETECTOR_H
#define GDD_DETECTOR_H

#include "nodes/pg_list.h"
#include "lib/stringinfo.h"

typedef struct GddCtx		GddCtx;
typedef struct GddPair		GddPair;
typedef struct GddMap		GddMap;
typedef struct GddMapIter	GddMapIter;
typedef struct GddListIter	GddListIter;
typedef struct GddEdge		GddEdge;
typedef struct GddVert		GddVert;
typedef struct GddGraph		GddGraph;
typedef struct GddStat		GddStat;

extern GddCtx *GddCtxNew(void);
extern GddEdge *GddCtxAddEdge(GddCtx *ctx, int segid, int from, int to, bool solid);
extern void GddCtxReduce(GddCtx *ctx);
extern List *GddCtxBreakDeadLock(GddCtx *ctx);
extern bool GddCtxEmpty(GddCtx *ctx);

#endif   /* GDD_DETECTOR_H */
