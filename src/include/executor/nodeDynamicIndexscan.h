/*-------------------------------------------------------------------------
 *
 * nodeDynamicIndexScan.h
 *
 * Portions Copyright (c) 2013 - present, EMC/Greenplum
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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

/* GPDB_12_MERGE_FIXME */
#if 0
extern DynamicIndexScanState *ExecInitDynamicIndexScan(DynamicIndexScan *node, EState *estate, int eflags);
extern TupleTableSlot *ExecDynamicIndexScan(DynamicIndexScanState *node);
extern void ExecEndDynamicIndexScan(DynamicIndexScanState *node);
extern void ExecReScanDynamicIndex(DynamicIndexScanState *node);

extern bool IndexScan_MapLogicalIndexInfo(LogicalIndexInfo *logicalIndexInfo, AttrNumber *attMap, Index varno);
extern AttrNumber *IndexScan_GetColumnMapping(Oid oldOid, Oid newOid);
#endif

#endif
