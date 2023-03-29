/*-------------------------------------------------------------------------
 *
 * nodeDynamicForeignscan.h
 *
 * Copyright (C) 2023 VMware Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/nodeDynamicForeignscan.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODEDYNAMICFOREIGNSCAN_H
#define NODEDYNAMICFOREIGNSCAN_H

#include "nodes/execnodes.h"

extern DynamicForeignScanState *ExecInitDynamicForeignScan(DynamicForeignScan *node, EState *estate, int eflags);
extern TupleTableSlot *ExecDynamicForeignScan(PlanState *pstate);
extern void ExecEndDynamicForeignScan(DynamicForeignScanState *node);
extern void ExecReScanDynamicForeignScan(DynamicForeignScanState *node);

#endif
