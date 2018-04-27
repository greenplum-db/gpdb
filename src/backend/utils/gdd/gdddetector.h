/*-------------------------------------------------------------------------
 *
 * gdddetector.h
 *	  Global DeadLock Detector - Detector Algorithm
 *
 *
 * Copyright (c) 2018-Present Pivotal Software, Inc.
 *
 *
 *-------------------------------------------------------------------------
 */

#ifndef GDD_DETECTOR_H
#define GDD_DETECTOR_H

#include "nodes/pg_list.h"
#include "lib/stringinfo.h"

typedef struct GddCtx		GddCtx;

extern GddCtx *GddCtxNew(void);
extern void GddCtxAddEdge(GddCtx *ctx, int segid, int from, int to, bool solid);
extern void GddCtxReduce(GddCtx *ctx);
extern List *GddCtxBreakDeadLock(GddCtx *ctx);
extern bool GddCtxEmpty(GddCtx *ctx);
extern void GddCtxDump(GddCtx *ctx, StringInfo str);

#endif   /* GDD_DETECTOR_H */
