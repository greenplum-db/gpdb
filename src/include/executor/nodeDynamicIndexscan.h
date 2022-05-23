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

extern DynamicIndexScanState *ExecInitDynamicIndexScan(DynamicIndexScan *node, EState *estate, int eflags);
extern TupleTableSlot *ExecDynamicIndexScan(PlanState *node);
extern void ExecEndDynamicIndexScan(DynamicIndexScanState *node);
extern void ExecReScanDynamicIndex(DynamicIndexScanState *node);

extern AttrNumber *IndexScan_GetColumnMapping(Oid oldOid, Oid newOid);

#endif
