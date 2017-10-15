/*-------------------------------------------------------------------------
 *
 * nodeDynamicIndexScan.h
 *
 * Portions Copyright (c) 2013 - present, EMC/Greenplum
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/nodeDynamicIndexscan.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODEDYNAMICINDEXSCAN_H
#define NODEDYNAMICINDEXSCAN_H

#include "nodes/execnodes.h"

extern int ExecCountSlotsDynamicIndexScan(DynamicIndexScan *node);
extern DynamicIndexScanState *ExecInitDynamicIndexScan(DynamicIndexScan *node, EState *estate, int eflags);
extern TupleTableSlot *ExecDynamicIndexScan(DynamicIndexScanState *node);
extern void ExecEndDynamicIndexScan(DynamicIndexScanState *node);
extern void ExecDynamicIndexReScan(DynamicIndexScanState *node, ExprContext *exprCtxt);

extern bool IndexScan_MapLogicalIndexInfo(LogicalIndexInfo *logicalIndexInfo, AttrNumber *attMap, Index varno);
extern AttrNumber *IndexScan_GetColumnMapping(Oid oldOid, Oid newOid);

static inline gpmon_packet_t * GpmonPktFromDynamicIndexScanState(DynamicIndexScanState *node)
{
	return &node->indexScanState->ss.ps.gpmon_pkt;
}
#endif

