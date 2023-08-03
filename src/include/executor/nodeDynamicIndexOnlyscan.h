/*-------------------------------------------------------------------------
 *
 * nodeDynamicIndexOnlyScan.h
 *
 * Copyright (C) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/nodeDynamicIndexOnlyscan.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODEDYNAMICINDEXONLYSCAN_H
#define NODEDYNAMICINDEXONLYSCAN_H

#include "nodes/execnodes.h"

extern DynamicIndexScanState *ExecInitDynamicIndexOnlyScan(DynamicIndexOnlyScan *node, EState *estate, int eflags);
extern TupleTableSlot *ExecDynamicIndexOnlyScan(PlanState *node);
extern void ExecEndDynamicIndexOnlyScan(DynamicIndexScanState *node);
extern void ExecReScanDynamicIndexOnly(DynamicIndexScanState *node);

#endif

