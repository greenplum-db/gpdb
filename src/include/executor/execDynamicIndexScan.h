/*--------------------------------------------------------------------------
 *
 * execDynamicIndexScan.h
 *	 Definitions and API functions for execDynamicIndexScan.c
 *
 * Copyright (c) 2014-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/execDynamicIndexScan.h
 *
 *--------------------------------------------------------------------------
 */
#ifndef EXECDYNAMICINDEXSCAN_H
#define EXECDYNAMICINDEXSCAN_H

#include "nodes/execnodes.h"
#include "utils/hsearch.h"
#include "utils/palloc.h"
#include "executor/tuptable.h"
#include "commands/tablecmds.h"

extern bool
IndexScan_MapLogicalIndexInfo(LogicalIndexInfo *logicalIndexInfo, AttrNumber *attMap, Index varno);

extern void
IndexScan_BeginIndexScan(IndexScanState *indexScanState, MemoryContext partitionContext, bool initQual, bool initTargetList, bool supportsArrayKeys);

extern bool
IndexScan_BeginIndexPartition(IndexScanState *indexScanState, MemoryContext partitionContext, bool initQual,
		bool initTargetList, bool supportsArrayKeys, bool isMultiScan);

extern void
IndexScan_EndIndexPartition(IndexScanState *indexScanState);

extern void
IndexScan_RescanIndex(IndexScanState *indexScanState, ExprContext *exprCtxt, bool initQual, bool initTargetList, bool supportsArrayKeys);

extern void
IndexScan_EndIndexScan(IndexScanState *indexScanState);

extern AttrNumber*
IndexScan_GetColumnMapping(Oid oldOid, Oid newOid);

#endif   /* EXECDYNAMICINDEXSCAN_H */
